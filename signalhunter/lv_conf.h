/*
 * LVGL configuration for SignalHunter (inherited from blewatch, reduced).
 *
 * Intentionally minimal: LVGL 9 applies its own defaults for every option
 * that is not set here (see lv_conf_internal.h in the library).
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/* The ST7789 flush path is RGB565 (16-bit). */
#define LV_COLOR_DEPTH 16

/* Built-in Montserrat fonts used by the UI. */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1

/* No LVGL logging (saves flash/RAM). */
#define LV_USE_LOG 0

#endif /*LV_CONF_H*/
