#pragma once
// UI state machine: runs on the LVGL timer, reads detection snapshots,
// applies button gestures, drives the LED and whichever screen is active.
namespace sh {

void uiInit();   // builds the screens and starts the periodic tick

}  // namespace sh
