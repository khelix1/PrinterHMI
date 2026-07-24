# PrinterHMI JD9165 customization

This local component preserves the JC1060P470C 1024x600 panel initialization
sequence and display timing required by PrinterHMI.

Upstream base: `espressif/esp_lcd_jd9165` 1.0.2.

Intentional changes:

- JC1060-specific JD9165 vendor initialization register sequence.
- DPI clock set to 51.2 MHz.
- Horizontal sync pulse width set to 24.
- Vertical back porch set to 21.
- Vertical front porch set to 12.

Do not replace this directory with a registry download without hardware testing.
