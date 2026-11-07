#pragma once
// Mode B, List: frozen-order rows of everything in range, a highlight the
// button moves, and a detail footer for the highlighted device.
#include <cstdint>

#include "../core/detection.h"
#include "../core/list_order.h"

namespace sh {

void listBuild();
void listShow();

// Repaint from a sorted snapshot using the frozen `order`. `highlight` is
// an index into `order` (clamped here).
void listRender(const Detection* snap, int n, const ListOrder& order,
                int highlight, uint32_t nowMs);

}  // namespace sh
