#pragma once
// Fixed-size table of tracked devices plus the helpers that work on a sorted
// snapshot of it. Pure and unsynchronised: the firmware wraps calls in a
// critical section and passes the current time in.
#include <cstddef>
#include <cstdint>

#include "config.h"
#include "detection.h"

namespace sh {

struct Sighting {
    const uint8_t* mac;
    int            rssi;
    const char*    name;     // may be nullptr / empty
    Radio          radio;
    uint8_t        channel;  // 0 for BLE
};

class DetectionTable {
public:
    DetectionTable();

    // Record a sighting. Inserts (evicting if needed) or updates. Returns the
    // slot index, or -1 if the table is full of locked/unevictable entries.
    int note(const Sighting& s, uint32_t now);

    void setLock(const uint8_t mac[6]);
    void clearLock();
    bool hasLock() const { return haveLock_; }
    const uint8_t* lockMac() const { return lockMac_; }
    bool isLocked(const Detection& d) const { return haveLock_ && macEqual(d.mac, lockMac_); }

    // Copy fresh entries into `out` (at most maxOut) in table order. The
    // locked target uses the longer kLockedStaleMs window. Cheap enough to
    // run inside a critical section; sort afterwards with sortSnapshot().
    int copyFresh(Detection* out, int maxOut, uint32_t now) const;

    // copyFresh() followed by sortSnapshot(): higher priority first, hardware
    // MACs before privacy MACs, then stronger smoothed RSSI.
    int snapshot(Detection* out, int maxOut, uint32_t now) const;

    // Copy entries that have not been announced yet (at most maxOut) and mark
    // them announced. Lets the caller log new devices outside any lock.
    int takeUnannounced(Detection* out, int maxOut);

    int used() const;
    const Detection& at(int i) const { return slots_[i]; }

private:
    int find(const uint8_t mac[6]) const;
    int evictCandidate(uint32_t now) const;

    Detection slots_[kMaxDetections];
    bool      haveLock_ = false;
    uint8_t   lockMac_[6] = {0, 0, 0, 0, 0, 0};
};

// --- helpers over a snapshot --------------------------------------------------

int  findInSnapshot(const Detection* snap, int n, const uint8_t mac[6]);
bool sortsBefore(const Detection& a, const Detection& b);
void sortSnapshot(Detection* snap, int n);

// Auto mode target choice: the best entry by sort order, except that a
// previously chosen target (prevMac, may be nullptr) is kept unless the
// challenger has a higher priority class or is at least marginDb stronger.
// Returns an index into snap, or -1 if the snapshot is empty.
int pickAuto(const Detection* snap, int n, const uint8_t* prevMac, int marginDb);

// Copy a C string into a fixed buffer, replacing control characters so the
// result is safe to display.
void copyDisplayName(char* dst, size_t dstLen, const char* src);

}  // namespace sh
