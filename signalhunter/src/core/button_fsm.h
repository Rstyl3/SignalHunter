#pragma once
// Gesture recogniser for the single BOOT button. Feed it the debounced
// pressed/released level at a steady rate (every ~20 ms) with the current
// time; it returns at most one event per call. Pure.
#include <cstdint>

namespace sh {

enum class ButtonEvent : uint8_t {
    None = 0,
    Short,      // press + release shorter than kShortPressMaxMs, no second press
    Double,     // two short presses within kDoubleClickMaxGapMs
    Long,       // held between kShortPressMaxMs and kLongPressMaxMs, fires on release
    ExtraLong,  // held kExtraLongPressMs, fires while still held
};

const char* buttonEventName(ButtonEvent e);

class ButtonFsm {
public:
    ButtonEvent feed(bool pressed, uint32_t now);

private:
    bool     wasPressed_        = false;
    uint32_t pressStartMs_      = 0;
    bool     extraLongFired_    = false;
    uint32_t pendingShortAt_    = 0;      // release time of a short press awaiting a double; 0 = none
    bool     thisPressIsDouble_ = false;
};

}  // namespace sh
