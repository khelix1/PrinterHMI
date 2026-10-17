#include "ui_page_state.h"
#include "ui_text.h"
#include "ui_value_update.h"

#include "ui_theme.h"

struct ui_page_state {
    lv_obj_t *root;
    lv_obj_t *icon;
    lv_obj_t *title;
    lv_obj_t *detail;
    lv_obj_t *spinner;
    ui_page_state_t **owner_slot;
    uint32_t epoch;
    unsigned active_calls;
};

static void page_state_release(ui_page_state_t *state)
{
    if (--state->active_calls == 0 && !state->root) lv_free(state);
}

static void page_state_field_deleted(lv_event_t *event)
{
    ui_page_state_t *state=lv_event_get_user_data(event);
    lv_obj_t *object=lv_event_get_target_obj(event);
    lv_obj_t **fields[]={&state->icon,&state->title,&state->detail,&state->spinner};
    for(unsigned i=0;i<4;++i)if(*fields[i]==object){*fields[i]=NULL;++state->epoch;}
}

static void page_state_delete_cb(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=lv_event_get_current_target_obj(event))return;
    ui_page_state_t *state =
        (ui_page_state_t *)lv_event_get_user_data(event);
    if (!state || lv_event_get_target_obj(event)!=state->root) return;
    lv_obj_t *fields[]={state->icon,state->title,state->detail,state->spinner};
    for(unsigned i=0;i<4;++i)if(fields[i])
        lv_obj_remove_event_cb_with_user_data(fields[i],page_state_field_deleted,state);
    if(state->owner_slot && *state->owner_slot==state)*state->owner_slot=NULL;
    ++state->epoch;
    state->root=state->icon=state->title=state->detail=state->spinner=NULL;
    if(!state->active_calls)lv_free(state);
}

ui_page_state_t *ui_page_state_create(
    lv_obj_t *parent, int x, int y, int width, int height)
{
    return ui_page_state_create_owned(parent,x,y,width,height,NULL);
}

ui_page_state_t *ui_page_state_create_owned(
    lv_obj_t *parent, int x, int y, int width, int height, ui_page_state_t **owner_slot)
{
    if (!parent) return NULL;

    ui_page_state_t *state = lv_calloc(1,sizeof(*state));
    if (!state) return NULL;

    state->root = lv_obj_create(parent);
    state->icon = NULL;
    state->title = NULL;
    state->detail = NULL;
    state->spinner = NULL;

    if (!state->root) {
        lv_free(state);
        return NULL;
    }

    lv_obj_set_size(state->root, width, height);
    lv_obj_set_pos(state->root, x, y);
    lv_obj_clear_flag(state->root, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_root_style(state->root);
    lv_obj_add_event_cb(state->root,
                        page_state_delete_cb,
                        LV_EVENT_DELETE,
                        state);

    lv_obj_t *body = lv_obj_create(state->root);
    ui_apply_surface_role(body, UI_SURFACE_TRANSPARENT);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(body, lv_pct(90), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(body, UI_GAP_ROW, 0);
    lv_obj_center(body);

    state->icon = lv_label_create(body);
    ui_apply_text_heading(state->icon);
    /* Theme text fonts may omit LVGL symbols (notably Studio Inter). */
    lv_obj_set_style_text_font(state->icon, &lv_font_montserrat_32, 0);
    state->spinner = lv_spinner_create(body);
    lv_obj_set_size(state->spinner, 34, 34);
    state->title = lv_label_create(body);
    lv_obj_set_width(state->title, lv_pct(100));
    lv_obj_set_style_text_align(state->title, LV_TEXT_ALIGN_CENTER, 0);
    ui_apply_text_title(state->title);
    ui_apply_label_bright(state->title);
    state->detail = lv_label_create(body);
    lv_obj_set_width(state->detail, lv_pct(100));
    lv_label_set_long_mode(state->detail, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(state->detail, LV_TEXT_ALIGN_CENTER, 0);
    ui_apply_text_body(state->detail);
    ui_apply_label_dim(state->detail);
    lv_obj_add_flag(state->root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *fields[]={state->icon,state->title,state->detail,state->spinner};
    for(unsigned i=0;i<4;++i)
        lv_obj_add_event_cb(fields[i],page_state_field_deleted,LV_EVENT_DELETE,state);
    state->owner_slot=owner_slot;
    if(owner_slot)*owner_slot=state;
    return state;
}

void ui_page_state_show(ui_page_state_t *state,
                            ui_page_state_kind_t kind,
                            const char *title,
                            const char *detail)
{
    if (!state || !state->root) return;
    ++state->active_calls;
    uint32_t epoch=++state->epoch;
    /* Widget events may retire this panel or replace an outer Show with Hide
     * or another Show. Hold metadata until the active call has unwound. */
#define STATE_UPDATE(field, operation) do { \
    if(!state->root || state->epoch!=epoch)goto finished; \
    if(state->field){operation;} \
} while(0)

    const char *icon = LV_SYMBOL_WARNING;
    lv_color_t color = UI_TEXT_DIM;

    switch (kind) {
    case UI_PAGE_STATE_LOADING:
        icon = "";
        color = UI_ACCENT_CYAN;
        STATE_UPDATE(spinner,lv_obj_clear_flag(state->spinner, LV_OBJ_FLAG_HIDDEN));
        STATE_UPDATE(icon,lv_obj_add_flag(state->icon, LV_OBJ_FLAG_HIDDEN));
        break;
    case UI_PAGE_STATE_EMPTY:
        icon = LV_SYMBOL_DIRECTORY;
        color = UI_ACCENT_CYAN;
        STATE_UPDATE(spinner,lv_obj_add_flag(state->spinner, LV_OBJ_FLAG_HIDDEN));
        STATE_UPDATE(icon,lv_obj_clear_flag(state->icon, LV_OBJ_FLAG_HIDDEN));
        break;
    case UI_PAGE_STATE_OFFLINE:
        icon = LV_SYMBOL_WIFI;
        color = UI_DANGER_BRIGHT;
        STATE_UPDATE(spinner,lv_obj_add_flag(state->spinner, LV_OBJ_FLAG_HIDDEN));
        STATE_UPDATE(icon,lv_obj_clear_flag(state->icon, LV_OBJ_FLAG_HIDDEN));
        break;
    case UI_PAGE_STATE_ERROR:
    default:
        color = UI_DANGER_BRIGHT;
        STATE_UPDATE(spinner,lv_obj_add_flag(state->spinner, LV_OBJ_FLAG_HIDDEN));
        STATE_UPDATE(icon,lv_obj_clear_flag(state->icon, LV_OBJ_FLAG_HIDDEN));
        break;
    }

    STATE_UPDATE(icon,ui_value_set_text(state->icon, icon));
    STATE_UPDATE(icon,lv_obj_set_style_text_font(state->icon,&lv_font_montserrat_32,0));
    STATE_UPDATE(icon,ui_value_set_color(state->icon, color,0));
    STATE_UPDATE(title,ui_value_set_text(state->title, title ? title : ui_text("")));
    STATE_UPDATE(detail,ui_value_set_text(state->detail, detail ? detail : ui_text("")));
    STATE_UPDATE(root,lv_obj_clear_flag(state->root, LV_OBJ_FLAG_HIDDEN));
    STATE_UPDATE(root,lv_obj_move_foreground(state->root));
finished:
    page_state_release(state);
#undef STATE_UPDATE
}

void ui_page_state_hide(ui_page_state_t *state)
{
    if (state && state->root) {
        ++state->active_calls;
        ++state->epoch;
        lv_obj_add_flag(state->root, LV_OBJ_FLAG_HIDDEN);
        page_state_release(state);
    }
}
