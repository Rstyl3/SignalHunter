#include "camera_match.h"

#include <cstddef>
#include <cstring>

#include "camera_oui_table.h"

namespace sh {

namespace {

int searchTable(const OuiEntry* t, size_t n, uint64_t key) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (t[mid].key < key)      lo = mid + 1;
        else if (t[mid].key > key) hi = mid;
        else                       return t[mid].vendor;
    }
    return -1;
}

enum class PatMode : uint8_t {
    Substr,  // anywhere in the name
    Prefix,  // only at the start
    Word,    // bounded by non-alphanumerics or the string ends
};

struct NamePattern {
    const char* pat;
    PatMode     mode;
};

// Patterns are matched against the upper-cased name, first match wins.
constexpr NamePattern kNamePatterns[] = {
    {"IPCAM", PatMode::Substr},   {"IP-CAM", PatMode::Substr},  {"IPCAMERA", PatMode::Substr},
    {"IPC", PatMode::Prefix},     {"CAM-", PatMode::Substr},    {"-CAM", PatMode::Substr},
    {"CAMERA", PatMode::Substr},  {"SPYCAM", PatMode::Substr},  {"MINICAM", PatMode::Substr},
    {"NANNYCAM", PatMode::Substr},{"PETCAM", PatMode::Substr},  {"BABYCAM", PatMode::Substr},
    {"BABYMON", PatMode::Substr}, {"BABY MON", PatMode::Substr},{"DOORBELL", PatMode::Substr},
    {"HIKVISION", PatMode::Substr}, {"DS-", PatMode::Prefix},   {"EZVIZ", PatMode::Substr},
    {"DAHUA", PatMode::Substr},   {"DH-", PatMode::Prefix},     {"IMOU", PatMode::Substr},
    {"LOREX", PatMode::Substr},   {"AMCREST", PatMode::Substr}, {"UNIVIEW", PatMode::Substr},
    {"UNV", PatMode::Word},       {"WYZE", PatMode::Substr},    {"HUALAI", PatMode::Substr},
    {"REOLINK", PatMode::Substr}, {"FOSCAM", PatMode::Substr},  {"V380", PatMode::Substr},
    {"VSTARCAM", PatMode::Substr},{"WANSVIEW", PatMode::Substr},{"TAPO", PatMode::Substr},
    {"EUFY", PatMode::Substr},    {"ARLO", PatMode::Word},      {"RING", PatMode::Word},
    {"BLINK", PatMode::Word},     {"NEST", PatMode::Word},      {"GOPRO", PatMode::Substr},
    {"AXIS-", PatMode::Prefix},   {"MOBOTIX", PatMode::Substr}, {"VIVOTEK", PatMode::Substr},
    {"XMEYE", PatMode::Substr},   {"MIPC", PatMode::Substr},    {"SMARTLIFE-", PatMode::Substr},
    {"TUYA", PatMode::Substr},    {"LSC", PatMode::Prefix},     {"ACTION-", PatMode::Prefix},
    {"YI-", PatMode::Prefix},     {"YICAM", PatMode::Substr},   {"KAMI", PatMode::Word},
    {"ANNKE", PatMode::Substr},   {"SWANN", PatMode::Substr},   {"ZMODO", PatMode::Substr},
    {"SRICAM", PatMode::Substr},  {"NOOIE", PatMode::Substr},   {"YOOSEE", PatMode::Substr},
    {"NETATMO", PatMode::Substr}, {"CANARY", PatMode::Word},    {"WEBCAM", PatMode::Substr},
};
constexpr size_t kNamePatternCount = sizeof(kNamePatterns) / sizeof(kNamePatterns[0]);

bool isAlnum(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

bool matches(const char* up, const NamePattern& p) {
    const size_t pl = std::strlen(p.pat);
    switch (p.mode) {
        case PatMode::Substr:
            return std::strstr(up, p.pat) != nullptr;
        case PatMode::Prefix:
            return std::strncmp(up, p.pat, pl) == 0;
        case PatMode::Word: {
            const char* s = up;
            while ((s = std::strstr(s, p.pat)) != nullptr) {
                const bool startOk = (s == up) || !isAlnum(s[-1]);
                const bool endOk   = !isAlnum(s[pl]);
                if (startOk && endOk) return true;
                s += 1;
            }
            return false;
        }
    }
    return false;
}

}  // namespace

VendorMatch vendorLookup(const uint8_t mac[6]) {
    uint64_t m48 = 0;
    for (int i = 0; i < 6; i++) m48 = (m48 << 8) | mac[i];
    int v = searchTable(kOui36, kOui36Count, m48 >> 12);
    if (v < 0) v = searchTable(kOui28, kOui28Count, m48 >> 20);
    if (v < 0) v = searchTable(kOui24, kOui24Count, m48 >> 24);
    if (v < 0) return {nullptr, VendorTier::None};
    return {kOuiVendorNames[v], kOuiVendorTiers[v]};
}

const char* cameraNameMatch(const char* name) {
    if (!name || !name[0]) return nullptr;
    char up[49];
    size_t n = 0;
    for (; name[n] && n < sizeof(up) - 1; n++) {
        char c = name[n];
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        up[n] = c;
    }
    up[n] = '\0';
    for (size_t i = 0; i < kNamePatternCount; i++) {
        if (matches(up, kNamePatterns[i])) return kNamePatterns[i].pat;
    }
    return nullptr;
}

}  // namespace sh
