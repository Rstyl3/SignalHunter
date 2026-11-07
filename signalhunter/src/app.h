#pragma once
// Application entry point: wires the LED, the button and scanner tasks and
// the LVGL screens together. Call once from setup(), after LCD_Init() and
// Lvgl_Init().
void Signalhunter_Init(void);
