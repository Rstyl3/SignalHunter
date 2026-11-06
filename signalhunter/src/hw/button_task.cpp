#include "button_task.h"

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "../core/config.h"

namespace sh {

namespace {

QueueHandle_t g_queue = nullptr;

void buttonTask(void* /*arg*/) {
    pinMode(kBootBtnPin, INPUT_PULLUP);
    ButtonFsm fsm;
    for (;;) {
        const ButtonEvent ev = fsm.feed(digitalRead(kBootBtnPin) == LOW, millis());
        if (ev != ButtonEvent::None) {
            const uint8_t raw = static_cast<uint8_t>(ev);
            xQueueSend(g_queue, &raw, 0);   // drop if the UI is 4 events behind
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

}  // namespace

void buttonTaskStart() {
    g_queue = xQueueCreate(4, sizeof(uint8_t));
    xTaskCreate(buttonTask, "sh_btn", 2048, nullptr, 1, nullptr);
}

bool buttonPoll(ButtonEvent& ev) {
    if (!g_queue) return false;
    uint8_t raw = 0;
    if (xQueueReceive(g_queue, &raw, 0) != pdTRUE) return false;
    ev = static_cast<ButtonEvent>(raw);
    return true;
}

}  // namespace sh
