#pragma once
// Polls the BOOT button in its own task, runs the gesture recogniser and
// queues the resulting events for the UI.
#include "../core/button_fsm.h"

namespace sh {

void buttonTaskStart();
// Non-blocking: returns true and fills `ev` if an event is waiting.
bool buttonPoll(ButtonEvent& ev);

}  // namespace sh
