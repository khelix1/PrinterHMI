#include "demo_camera.h"
void demo_camera_draw(uint16_t *frame, bool side)
{
    if (!frame) return;
    for (int y = 0; y < DEMO_CAMERA_HEIGHT; ++y) {
        for (int x = 0; x < DEMO_CAMERA_WIDTH; ++x) {
            uint16_t color = ((x / 40 + y / 40) % 2) ? 0x2104 : 0x2945;
            if (y > 210 && y < 290 && x > 55 && x < 425) color = 0x52aa;
            if (((x > 80 && x < 98) || (x > 382 && x < 400)) && y > 30 && y < 270) color = 0x7bef;
            if (y > 40 && y < 55 && x > 80 && x < 400) color = 0x7bef;
            if (x >= 180 && x < 240 && y > 110 + (240 - x) / 3 && y < 235 + (x - 240) / 3) color = side ? 0x4fdf : 0xac3f;
            if (x >= 240 && x <= 300 && y > 110 + (x - 240) / 3 && y < 235 - (x - 240) / 3) color = side ? 0x2494 : 0x5ad8;
            if (x < 55 && y < 35) color = 0xf800;
            if (x > 420 && y > 285) color = 0x07e0;
            frame[y * DEMO_CAMERA_WIDTH + x] = color;
        }
    }
}
