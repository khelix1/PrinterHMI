#include "ui_bed_mesh_gestures.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    ui_bed_mesh_view_transform_t *view;
    ui_bed_mesh_gesture_render_cb_t render;
    int32_t canvas_width;
    int32_t canvas_height;

    float pinch_base_zoom;
    uint32_t pinch_last_render_tick;
    uint32_t pan_last_render_tick;
    uint32_t drag_last_render_tick;
    lv_point_t two_touch_center;
    lv_point_t two_touch_start;
    lv_point_t last;
    uint8_t touch_count;
    bool pan_active;
    bool block_one_finger;
    bool drag;
    bool pinch_active;
    lv_indev_t *gesture_indev;
} ui_bed_mesh_gesture_state_t;

static ui_bed_mesh_gesture_state_t s_gesture;


static void request_render(void)
{
    if (s_gesture.render) {
        s_gesture.render();
    }
}


void ui_bed_mesh_gestures_init(
    ui_bed_mesh_view_transform_t *transform,
    ui_bed_mesh_gesture_render_cb_t render_cb,
    int32_t canvas_width,
    int32_t canvas_height)
{
    ui_bed_mesh_gestures_close();
    memset(&s_gesture, 0, sizeof(s_gesture));

    s_gesture.view = transform;
    s_gesture.render = render_cb;
    s_gesture.canvas_width = canvas_width;
    s_gesture.canvas_height = canvas_height;

    if (transform) {
        transform->yaw = -0.72f;
        transform->pitch = 0.88f;
        transform->zoom = 1.0f;
        transform->pan_x = 0.0f;
        transform->pan_y = 0.0f;
    }

#if LV_USE_GESTURE_RECOGNITION
    s_gesture.gesture_indev = lv_indev_active();

    if (s_gesture.gesture_indev) {
        lv_indev_set_pinch_up_threshold(
            s_gesture.gesture_indev,
            1.02f);
        lv_indev_set_pinch_down_threshold(
            s_gesture.gesture_indev,
            0.98f);
        lv_indev_set_rotation_rad_threshold(
            s_gesture.gesture_indev,
            3.20f);
    }
#endif
}


void ui_bed_mesh_gestures_close(void)
{
#if LV_USE_GESTURE_RECOGNITION
    if (s_gesture.gesture_indev) {
        lv_indev_set_pinch_up_threshold(
            s_gesture.gesture_indev,
            1.50f);
        lv_indev_set_pinch_down_threshold(
            s_gesture.gesture_indev,
            0.75f);
        lv_indev_set_rotation_rad_threshold(
            s_gesture.gesture_indev,
            0.20f);
    }
#endif

    memset(&s_gesture, 0, sizeof(s_gesture));
}


void ui_bed_mesh_gestures_multitouch_update(
    uint8_t count,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1)
{
    uint8_t previous_count =
        s_gesture.touch_count;
    s_gesture.touch_count = count;

    if (count >= 2) {
        s_gesture.two_touch_center.x =
            (x0 + x1) / 2;
        s_gesture.two_touch_center.y =
            (y0 + y1) / 2;
        s_gesture.block_one_finger = true;

        if (previous_count < 2) {
            s_gesture.two_touch_start =
                s_gesture.two_touch_center;
            s_gesture.last =
                s_gesture.two_touch_center;
            s_gesture.pan_active = false;
            s_gesture.pan_last_render_tick = 0;
            s_gesture.drag = false;
        }
    } else if (count == 0) {
        /*
         * After a two-finger gesture, require all fingers to lift before
         * one-finger orbit can begin again.
         */
        s_gesture.block_one_finger = false;
    }
}


void ui_bed_mesh_gestures_event_cb(
    lv_event_t *event)
{
    if (!event || !s_gesture.view) {
        return;
    }

    lv_event_code_t code =
        lv_event_get_code(event);
    lv_indev_t *indev = lv_indev_active();

    if (!indev) {
        return;
    }

    if (code == LV_EVENT_PRESSING &&
        s_gesture.touch_count >= 2) {
        if (s_gesture.pinch_active) {
            return;
        }

        if (!s_gesture.pan_active) {
            int32_t start_dx =
                s_gesture.two_touch_center.x -
                s_gesture.two_touch_start.x;
            int32_t start_dy =
                s_gesture.two_touch_center.y -
                s_gesture.two_touch_start.y;

            if (start_dx * start_dx +
                    start_dy * start_dy <
                144) {
                return;
            }

            s_gesture.pan_active = true;
            s_gesture.pan_last_render_tick = 0;
            s_gesture.last =
                s_gesture.two_touch_center;
            s_gesture.drag = false;
            return;
        }

        int32_t dx =
            s_gesture.two_touch_center.x -
            s_gesture.last.x;
        int32_t dy =
            s_gesture.two_touch_center.y -
            s_gesture.last.y;

        if (abs(dx) <= 80 && abs(dy) <= 80) {
            s_gesture.view->pan_x += dx;
            s_gesture.view->pan_y += dy;

            float pan_x_limit =
                s_gesture.canvas_width * .45f;
            float pan_y_limit =
                s_gesture.canvas_height * .45f;

            if (s_gesture.view->pan_x <
                -pan_x_limit) {
                s_gesture.view->pan_x =
                    -pan_x_limit;
            }
            if (s_gesture.view->pan_x >
                pan_x_limit) {
                s_gesture.view->pan_x =
                    pan_x_limit;
            }
            if (s_gesture.view->pan_y <
                -pan_y_limit) {
                s_gesture.view->pan_y =
                    -pan_y_limit;
            }
            if (s_gesture.view->pan_y >
                pan_y_limit) {
                s_gesture.view->pan_y =
                    pan_y_limit;
            }

            uint32_t now = lv_tick_get();
            if (s_gesture.pan_last_render_tick == 0 ||
                now -
                    s_gesture.pan_last_render_tick >=
                    33U) {
                s_gesture.pan_last_render_tick = now;
                request_render();
            }
        }

        s_gesture.last =
            s_gesture.two_touch_center;
        return;
    }

    if (s_gesture.pan_active &&
        s_gesture.touch_count < 2) {
        s_gesture.pan_active = false;
        s_gesture.pan_last_render_tick = 0;
        request_render();
    }

#if LV_USE_GESTURE_RECOGNITION
    if (code == LV_EVENT_GESTURE) {
        lv_indev_gesture_type_t type =
            lv_event_get_gesture_type(event);

        if (type == LV_INDEV_GESTURE_PINCH) {
            lv_indev_gesture_state_t state =
                lv_event_get_gesture_state(
                    event,
                    LV_INDEV_GESTURE_PINCH);

            if (state ==
                LV_INDEV_GESTURE_STATE_RECOGNIZED) {
                if (s_gesture.pan_active) {
                    return;
                }

                if (!s_gesture.pinch_active) {
                    s_gesture.pinch_base_zoom =
                        s_gesture.view->zoom;
                    s_gesture.pinch_last_render_tick =
                        0;
                    s_gesture.pinch_active = true;
                    s_gesture.drag = false;
                }

                float raw_scale =
                    lv_event_get_pinch_scale(event);

                if (raw_scale > 0.01f) {
                    float adjusted_scale =
                        1.0f +
                        ((raw_scale - 1.0f) * 0.60f);
                    float target_zoom =
                        s_gesture.pinch_base_zoom *
                        adjusted_scale;

                    if (target_zoom < 0.45f) {
                        target_zoom = 0.45f;
                    }
                    if (target_zoom > 3.5f) {
                        target_zoom = 3.5f;
                    }

                    float delta =
                        target_zoom -
                        s_gesture.view->zoom;

                    if (fabsf(delta) < 0.002f) {
                        s_gesture.view->zoom =
                            target_zoom;
                    } else {
                        s_gesture.view->zoom +=
                            delta * 0.45f;
                    }

                    uint32_t now = lv_tick_get();
                    if (s_gesture
                                .pinch_last_render_tick ==
                            0 ||
                        now -
                                s_gesture
                                    .pinch_last_render_tick >=
                            33U) {
                        s_gesture.pinch_last_render_tick =
                            now;
                        request_render();
                    }
                }
            } else if (
                state ==
                LV_INDEV_GESTURE_STATE_ENDED) {
                s_gesture.pinch_base_zoom =
                    s_gesture.view->zoom;
                s_gesture.pinch_last_render_tick = 0;
                s_gesture.pinch_active = false;
                s_gesture.drag = false;
                request_render();
            }

            return;
        }

        return;
    }
#endif

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(
            indev,
            &s_gesture.last);
        s_gesture.drag_last_render_tick = 0;
        s_gesture.drag =
            !s_gesture.pinch_active &&
            !s_gesture.block_one_finger &&
            s_gesture.touch_count < 2;
    } else if (
        code == LV_EVENT_PRESSING &&
        s_gesture.drag &&
        !s_gesture.pinch_active &&
        !s_gesture.block_one_finger &&
        s_gesture.touch_count < 2) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);

        s_gesture.view->yaw -=
            (point.x - s_gesture.last.x) *
            0.012f;
        s_gesture.view->pitch -=
            (point.y - s_gesture.last.y) *
            0.01f;

        if (s_gesture.view->pitch < 0.15f) {
            s_gesture.view->pitch = 0.15f;
        }
        if (s_gesture.view->pitch > 1.45f) {
            s_gesture.view->pitch = 1.45f;
        }

        s_gesture.last = point;

        uint32_t now = lv_tick_get();
        if (s_gesture.drag_last_render_tick == 0 ||
            now -
                    s_gesture.drag_last_render_tick >=
                33U) {
            s_gesture.drag_last_render_tick = now;
            request_render();
        }
    } else if (
        code == LV_EVENT_RELEASED ||
        code == LV_EVENT_PRESS_LOST) {
        s_gesture.drag = false;
        s_gesture.drag_last_render_tick = 0;
        request_render();
    }
}


void ui_bed_mesh_gestures_reset(void)
{
    if (!s_gesture.view) {
        return;
    }

    s_gesture.view->yaw = -0.72f;
    s_gesture.view->pitch = 0.88f;
    s_gesture.view->zoom = 1.0f;
    s_gesture.view->pan_x = 0.0f;
    s_gesture.view->pan_y = 0.0f;
    request_render();
}


void ui_bed_mesh_gestures_zoom_in(void)
{
    if (!s_gesture.view) {
        return;
    }

    s_gesture.view->zoom =
        fminf(
            3.5f,
            s_gesture.view->zoom * 1.15f);
    request_render();
}


void ui_bed_mesh_gestures_zoom_out(void)
{
    if (!s_gesture.view) {
        return;
    }

    s_gesture.view->zoom =
        fmaxf(
            0.45f,
            s_gesture.view->zoom / 1.15f);
    request_render();
}


bool ui_bed_mesh_gestures_is_active(void)
{
    return s_gesture.drag ||
        s_gesture.pinch_active ||
        s_gesture.pan_active;
}
