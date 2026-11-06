#include "list_order.h"

#include <cstring>

namespace sh {

bool listOrderRefresh(ListOrder& lo, const Detection* snap, int n, uint32_t now, bool force) {
    if (!force && lo.valid && (now - lo.lastRefreshMs) < kListRefreshPeriodMs) return false;
    if (n > kMaxDetections) n = kMaxDetections;
    if (n < 0) n = 0;
    for (int i = 0; i < n; i++) std::memcpy(lo.macs[i], snap[i].mac, 6);
    lo.count = n;
    lo.lastRefreshMs = now;
    lo.valid = true;
    return true;
}

int clampHighlight(int highlight, int total) {
    if (total <= 0) return 0;
    if (highlight >= total) return total - 1;
    if (highlight < 0) return 0;
    return highlight;
}

int listScroll(int highlight, int total, int visibleRows) {
    int scroll = 0;
    if (highlight >= visibleRows) scroll = highlight - visibleRows + 1;
    if (scroll > total - visibleRows) scroll = total - visibleRows;
    if (scroll < 0) scroll = 0;
    return scroll;
}

}  // namespace sh
