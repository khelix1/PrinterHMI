# ESP LVGL Touch Screen Example

> Standalone upstream example: instructions below apply only to this example,
> not the PrinterHMI project root. Do not delete PrinterHMI's tracked sdkconfig
> or lockfile to follow an example. Use the
> [application build guide](../../../../docs/BUILDING.md) for PrinterHMI firmware.


Very simple example for demonstration of initialization and usage of the `esp_lvgl_port` component. This example contains four main parts:

## 1. LCD HW initialization - `app_lcd_init()`

Standard HW initialization of the LCD using [`esp_lcd`](https://github.com/espressif/esp-idf/tree/master/components/esp_lcd) component. Settings of this example are fully compatible with [ESP-BOX](https://github.com/espressif/esp-bsp/tree/master/esp-box) board.

## 2. Touch HW initialization - `app_touch_init()`

Standard HW initialization of the LCD touch using [`esp_lcd_touch`](https://github.com/espressif/esp-bsp/tree/master/components/lcd_touch/esp_lcd_touch) component. Settings of this example are fully compatible with [ESP-BOX](https://github.com/espressif/esp-bsp/tree/master/esp-box) board.

## 3. LVGL port initialization - `app_lvgl_init()`

Initialization of the LVGL port.

## 4. LVGL objects example usage - `app_main_display()`

Very simple demonstration code of using LVGL objects after LVGL port initialization.
