#include "detection_table.h"

#include <cstring>

#include "camera_match.h"

namespace sh {

void copyDisplayName(char* dst, size_t dstLen, const char* src) {
    if (dstLen == 0) return;
    size_t i = 0;
    if (src) {
        for (; src[i] && i < dstLen - 1; i++) {
            const unsigned char c = static_cast<unsigned char>(src[i]);
            dst[i] = (c < 0x20 || c == 0x7F) ? '?' : static_cast<char>(c);
        }
    }
    dst[i] = '\0';
}

DetectionTable::DetectionTable() {
    for (Detection& d : slots_) d = Detection{};
}

int DetectionTable::find(const uint8_t mac[6]) const {
    for (int i = 0; i < kMaxDetections; i++) {
        if (slots_[i].used && macEqual(slots_[i].mac, mac)) return i;
    }
    return -1;
}

// Lowest score goes first. Fresh entries, flagged entries and hardware MACs
// are worth keeping; stale privacy-MAC phones are not. Ties: oldest first.
int DetectionTable::evictCandidate(uint32_t now) const {
    int bestIdx = -1;
    int bestScore = 0;
    uint32_t bestAge = 0;
    for (int i = 0; i < kMaxDetections; i++) {
        const Detection& d = slots_[i];
        if (!d.used) return i;                 // free slot: no eviction needed
        if (isLocked(d)) continue;             // never evict the locked target
        const uint32_t age = now - d.lastSeenMs;
        const bool fresh = age <= kStaleMs;
        const int score = (fresh ? 8 : 0) + d.priority() * 2 + (macIsLaa(d.mac) ? 0 : 1);
        if (bestIdx < 0 || score < bestScore || (score == bestScore && age > bestAge)) {
            bestIdx = i;
            bestScore = score;
            bestAge = age;
        }
    }
    return bestIdx;
}

int DetectionTable::note(const Sighting& s, uint32_t now) {
    int idx = find(s.mac);
    const bool fresh = (idx < 0);
    if (fresh) {
        idx = evictCandidate(now);
        if (idx < 0) return -1;
        Detection& d = slots_[idx];
        d = Detection{};
        std::memcpy(d.mac, s.mac, 6);
        const VendorMatch vm = vendorLookup(s.mac);
        d.vendor = vm.name;
        d.tier = vm.tier;
        d.radio = s.radio;
        d.used = true;
        d.announced = false;
        emaReset(d.ema, s.rssi);
    }
    Detection& d = slots_[idx];
    d.rssi = static_cast<int8_t>(s.rssi);
    if (!fresh) {
        if (now - d.lastSeenMs > kEmaResetGapMs) emaReset(d.ema, s.rssi);
        else                                     emaUpdate(d.ema, s.rssi);
        // An access point stays an access point; its own probe/data frames may
        // look like station traffic to a frame parser.
        if (d.radio != Radio::WifiAp) d.radio = s.radio;
    }
    d.lastSeenMs = now;
    d.channel = s.channel;
    if (s.name && s.name[0]) {
        // Names can arrive late (BLE scan responses) and may change; keep the latest.
        copyDisplayName(d.name, sizeof(d.name), s.name);
        d.nameHit = cameraNameMatch(d.name);
    }
    return idx;
}

void DetectionTable::setLock(const uint8_t mac[6]) {
    std::memcpy(lockMac_, mac, 6);
    haveLock_ = true;
}

void DetectionTable::clearLock() {
    haveLock_ = false;
}

int DetectionTable::copyFresh(Detection* out, int maxOut, uint32_t now) const {
    int n = 0;
    for (int i = 0; i < kMaxDetections && n < maxOut; i++) {
        const Detection& d = slots_[i];
        if (!d.used) continue;
        const uint32_t limit = isLocked(d) ? kLockedStaleMs : kStaleMs;
        if ((now - d.lastSeenMs) > limit) continue;
        out[n++] = d;
    }
    return n;
}

int DetectionTable::snapshot(Detection* out, int maxOut, uint32_t now) const {
    const int n = copyFresh(out, maxOut, now);
    sortSnapshot(out, n);
    return n;
}

int DetectionTable::takeUnannounced(Detection* out, int maxOut) {
    int n = 0;
    for (int i = 0; i < kMaxDetections && n < maxOut; i++) {
        Detection& d = slots_[i];
        if (!d.used || d.announced) continue;
        d.announced = true;
        out[n++] = d;
    }
    return n;
}

int DetectionTable::used() const {
    int n = 0;
    for (int i = 0; i < kMaxDetections; i++) n += slots_[i].used ? 1 : 0;
    return n;
}

int findInSnapshot(const Detection* snap, int n, const uint8_t mac[6]) {
    for (int i = 0; i < n; i++) {
        if (macEqual(snap[i].mac, mac)) return i;
    }
    return -1;
}

bool sortsBefore(const Detection& a, const Detection& b) {
    const int ap = a.priority(), bp = b.priority();
    if (ap != bp) return ap > bp;                 // camera vendor > name hit > IoT > none
    const bool al = macIsLaa(a.mac), bl = macIsLaa(b.mac);
    if (al != bl) return !al;                     // hardware MAC before privacy MAC
    return a.rssiSmoothed() > b.rssiSmoothed();   // stronger first
}

void sortSnapshot(Detection* snap, int n) {
    // Insertion sort: n is small and the input is usually nearly sorted.
    for (int i = 1; i < n; i++) {
        Detection key = snap[i];
        int j = i - 1;
        while (j >= 0 && sortsBefore(key, snap[j])) {
            snap[j + 1] = snap[j];
            j--;
        }
        snap[j + 1] = key;
    }
}

int pickAuto(const Detection* snap, int n, const uint8_t* prevMac, int marginDb) {
    if (n <= 0) return -1;
    if (!prevMac) return 0;
    const int p = findInSnapshot(snap, n, prevMac);
    if (p < 0) return 0;                                            // previous target gone
    if (snap[0].priority() > snap[p].priority()) return 0;          // better class wins outright
    if (snap[0].rssiSmoothed() - snap[p].rssiSmoothed() >= marginDb) return 0;
    return p;                                                       // sticky
}

}  // namespace sh
