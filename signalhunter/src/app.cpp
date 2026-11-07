#include "app.h"

#include "hw/button_task.h"
#include "hw/led.h"
#include "scan/scanner_task.h"
#include "ui/ui_tick.h"

void Signalhunter_Init(void) {
    sh::ledInit();
    sh::uiInit();           // builds both screens and starts the UI timer
    sh::scannerStart();     // Wi-Fi sweeps + continuous BLE scan (FreeRTOS task)
    sh::buttonTaskStart();  // BOOT button gestures (FreeRTOS task)
}
