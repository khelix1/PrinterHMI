#include "ui_font_fallback.h"

/* Enabled by STEP221 in sdkconfig.defaults. */
LV_FONT_DECLARE(lv_font_montserrat_14);

typedef struct {
    const lv_font_t *base;
    lv_font_t copy;
} ui_font_fallback_slot_t;

static ui_font_fallback_slot_t s_slots[8];

const lv_font_t *ui_font_with_fallback(const lv_font_t *base)
{
    if (!base || base == &lv_font_montserrat_14) {
        return base;
    }

    for (size_t index = 0; index < sizeof(s_slots) / sizeof(s_slots[0]); ++index) {
        if (s_slots[index].base == base) {
            return &s_slots[index].copy;
        }
    }

    for (size_t index = 0; index < sizeof(s_slots) / sizeof(s_slots[0]); ++index) {
        if (!s_slots[index].base) {
            s_slots[index].base = base;
            s_slots[index].copy = *base;
            s_slots[index].copy.fallback = &lv_font_montserrat_14;
            return &s_slots[index].copy;
        }
    }

    /* The UI has fewer than eight shared fonts; keep a safe fallback if that
     * ever changes rather than returning a descriptor with no fallback. */
    return base;
}
