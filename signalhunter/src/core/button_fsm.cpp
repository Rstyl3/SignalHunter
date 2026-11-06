#include "button_fsm.h"

#include "config.h"

namespace sh {

const char* buttonEventName(ButtonEvent e) {
    switch (e) {
        case ButtonEvent::None:      return "NONE";
        case ButtonEvent::Short:     return "SHORT";
        case ButtonEvent::Double:    return "DOUBLE";
        case ButtonEvent::Long:      return "LONG";
        case ButtonEvent::ExtraLong: return "EXTRA_LONG";
    }
    return "?";
}

ButtonEvent ButtonFsm::feed(bool pressed, uint32_t now) {
    ButtonEvent ev = ButtonEvent::None;

    if (pressed && !wasPressed_) {
        pressStartMs_ = now;
        extraLongFired_ = false;
        // A press inside the double-click window after a short release is the
        // second half of a double click; it is committed on release.
        if (pendingShortAt_ && (now - pendingShortAt_) <= kDoubleClickMaxGapMs) {
            thisPressIsDouble_ = true;
            pendingShortAt_ = 0;
        } else {
            thisPressIsDouble_ = false;
        }
    } else if (pressed && wasPressed_) {
        // Extra-long fires as soon as the threshold passes, without a release.
        if (!extraLongFired_ && (now - pressStartMs_) >= kExtraLongPressMs) {
            ev = ButtonEvent::ExtraLong;
            extraLongFired_ = true;
            pendingShortAt_ = 0;
            thisPressIsDouble_ = false;
        }
    } else if (!pressed && wasPressed_) {
        const uint32_t held = now - pressStartMs_;
        if (!extraLongFired_) {
            if (held < kShortPressMaxMs) {
                if (thisPressIsDouble_) {
                    ev = ButtonEvent::Double;
                    thisPressIsDouble_ = false;
                } else {
                    pendingShortAt_ = now ? now : 1;   // wait for a possible second click
                }
            } else if (held < kLongPressMaxMs) {
                ev = ButtonEvent::Long;
                pendingShortAt_ = 0;
                thisPressIsDouble_ = false;
            }
        }
    }

    // Promote a pending single click once the double-click window has passed.
    if (ev == ButtonEvent::None && pendingShortAt_ &&
        (now - pendingShortAt_) > kDoubleClickMaxGapMs) {
        ev = ButtonEvent::Short;
        pendingShortAt_ = 0;
    }

    wasPressed_ = pressed;
    return ev;
}

}  // namespace sh
