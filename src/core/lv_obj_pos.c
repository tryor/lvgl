/**
 * @file lv_obj_pos.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_obj.h"
#include "lv_disp.h"
#include "lv_refr.h"

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS &lv_obj_class

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void calc_auto_size(lv_obj_t * obj, lv_coord_t * w_out, lv_coord_t * h_out);
static void layout_update_core(lv_obj_t * obj);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_layout_update_cb_t * layouts;
static uint32_t layout_cnt;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_obj_set_pos(lv_obj_t * obj, lv_coord_t x, lv_coord_t y)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_obj_set_x(obj, x);
    lv_obj_set_y(obj, y);
}

void lv_obj_set_x(lv_obj_t * obj, lv_coord_t x)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if(lv_obj_is_layout_positioned(obj)) {
        return;
    }

    lv_res_t res_x;
    lv_style_value_t v_x;

    res_x = lv_obj_get_local_style_prop(obj, LV_PART_MAIN, LV_STATE_DEFAULT, LV_STYLE_X, &v_x);

    if((res_x == LV_RES_OK && v_x.num != x) || res_x == LV_RES_INV) {
        lv_obj_set_style_x(obj, LV_PART_MAIN, LV_STATE_DEFAULT, x);
    }

    lv_obj_refr_pos(obj);
}

void lv_obj_set_y(lv_obj_t * obj, lv_coord_t y)
{
    if(lv_obj_is_layout_positioned(obj)) {
        return;
    }
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_res_t res_y;
    lv_style_value_t v_y;

    res_y = lv_obj_get_local_style_prop(obj, LV_PART_MAIN, LV_STATE_DEFAULT, LV_STYLE_Y, &v_y);

    if((res_y == LV_RES_OK && v_y.num != y) || res_y == LV_RES_INV) {
        lv_obj_set_style_y(obj, LV_PART_MAIN, LV_STATE_DEFAULT, y);
    }

    lv_obj_refr_pos(obj);
}

void lv_obj_refr_size(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    /*If the width or height is set by a layout do not modify them*/
    if(obj->w_layout && obj->h_layout) return;

    lv_obj_t * parent = lv_obj_get_parent(obj);
    if(parent == NULL) return;

    lv_coord_t w;
    lv_coord_t h;
    if(obj->w_layout) w = lv_obj_get_width(obj);
    else w = lv_obj_get_style_width(obj, LV_PART_MAIN);
    if(obj->h_layout) h = lv_obj_get_height(obj);
    else h = lv_obj_get_style_height(obj, LV_PART_MAIN);

    /*Calculate the required auto sizes*/
    bool w_content = w == LV_SIZE_CONTENT ? true : false;
    bool h_content = h == LV_SIZE_CONTENT ? true : false;

    /*Be sure the object is not scrolled when it has auto size*/
    if(w_content) lv_obj_scroll_to_x(obj, 0, LV_ANIM_OFF);
    if(h_content) lv_obj_scroll_to_y(obj, 0, LV_ANIM_OFF);

    if(w_content && h_content) calc_auto_size(obj, &w, &h);
    else if(w_content) calc_auto_size(obj, &w, NULL);
    else if(h_content) calc_auto_size(obj, NULL, &h);

    /*Calculate the required auto sizes*/
    bool pct_w = LV_COORD_IS_PCT(w) ? true : false;
    bool pct_h = LV_COORD_IS_PCT(h) ? true : false;

    lv_coord_t parent_w = lv_obj_get_width_fit(parent);
    lv_coord_t parent_h = lv_obj_get_height_fit(parent);
    if(pct_w) w = (LV_COORD_GET_PCT(w) * parent_w) / 100;
    if(pct_h) h = (LV_COORD_GET_PCT(h) * parent_h) / 100;

    /*Do nothing if the size is not changed*/
    /*It is very important else recursive resizing can occur without size change*/
    if(lv_obj_get_width(obj) == w && lv_obj_get_height(obj) == h) return;

    /*Invalidate the original area*/
    lv_obj_invalidate(obj);

    /*Save the original coordinates*/
    lv_area_t ori;
    lv_obj_get_coords(obj, &ori);

    /*Check if the object inside the parent or not*/
    lv_area_t parent_fit_area;
    lv_obj_get_coords_fit(parent, &parent_fit_area);

    /*If the object is already out of the parent and its position is changes
     *surely the scrollbars also changes so invalidate them*/
    bool on1 = _lv_area_is_in(&ori, &parent_fit_area, 0);
    if(!on1) lv_obj_scrollbar_invalidate(parent);

    /*Set the length and height
     *Be sure the content is not scrolled in an invalid position on the new size*/
    obj->coords.y2 = obj->coords.y1 + h - 1;
    if(lv_obj_get_base_dir(obj) == LV_BIDI_DIR_RTL) {
        obj->coords.x1 = obj->coords.x2 - w + 1;
    }
    else {
        obj->coords.x2 = obj->coords.x1 + w - 1;
    }

    /*Call the ancestor's event handler to the object with its new coordinates*/
    lv_event_send(obj, LV_EVENT_SIZE_CHANGED, &ori);

    /*Call the ancestor's event handler to the parent too*/
    lv_event_send(parent, LV_EVENT_CHILD_CHANGED, obj);

    /*Invalidate the new area*/
    lv_obj_invalidate(obj);

    /*If the object was out of the parent invalidate the new scrollbar area too.
     *If it wasn't out of the parent but out now, also invalidate the srollbars*/
    bool on2 = _lv_area_is_in(&obj->coords, &parent_fit_area, 0);
    if(on1 || (!on1 && on2)) lv_obj_scrollbar_invalidate(parent);
}

void lv_obj_set_size(lv_obj_t * obj, lv_coord_t w, lv_coord_t h)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_obj_set_width(obj, w);
    lv_obj_set_height(obj, h);
}

void lv_obj_set_width(lv_obj_t * obj, lv_coord_t w)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_res_t res_w;
    lv_style_value_t v_w;

    res_w = lv_obj_get_local_style_prop(obj, LV_PART_MAIN, LV_STATE_DEFAULT, LV_STYLE_WIDTH, &v_w);

    if((res_w == LV_RES_OK && v_w.num != w) || res_w == LV_RES_INV) {
        lv_obj_set_style_width(obj, LV_PART_MAIN, LV_STATE_DEFAULT, w);
    }

    lv_obj_refr_size(obj);
}

void lv_obj_set_height(lv_obj_t * obj, lv_coord_t h)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_res_t res_h;
    lv_style_value_t v_h;

    res_h = lv_obj_get_local_style_prop(obj, LV_PART_MAIN, LV_STATE_DEFAULT, LV_STYLE_HEIGHT, &v_h);

    if((res_h == LV_RES_OK && v_h.num != h) || res_h == LV_RES_INV) {
        lv_obj_set_style_height(obj, LV_PART_MAIN, LV_STATE_DEFAULT, h);
    }

    lv_obj_refr_size(obj);
}

void lv_obj_set_content_width(lv_obj_t * obj, lv_coord_t w)
{
    lv_coord_t pleft = lv_obj_get_style_pad_left(obj, LV_PART_MAIN);
    lv_coord_t pright = lv_obj_get_style_pad_right(obj, LV_PART_MAIN);

    lv_obj_set_width(obj, w + pleft + pright);
}

void lv_obj_set_content_height(lv_obj_t * obj, lv_coord_t h)
{
    lv_coord_t ptop = lv_obj_get_style_pad_top(obj, LV_PART_MAIN);
    lv_coord_t pbottom = lv_obj_get_style_pad_bottom(obj, LV_PART_MAIN);

    lv_obj_set_height(obj, h + ptop + pbottom);
}

void lv_obj_set_layout(lv_obj_t * obj, uint32_t layout)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_obj_set_style_layout(obj, LV_PART_MAIN, LV_STATE_DEFAULT, layout);

    lv_obj_mark_layout_as_dirty(obj);
}

bool lv_obj_is_layout_positioned(const lv_obj_t * obj)
{
    if(lv_obj_has_flag_any(obj, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING)) return false;

    lv_obj_t * parent = lv_obj_get_parent(obj);
    if(parent == NULL) return false;

    uint32_t layout = lv_obj_get_style_layout(parent, LV_PART_MAIN);
    if(layout) return true;
    else return false;
}

void lv_obj_mark_layout_as_dirty(lv_obj_t * obj)
{
    obj->layout_inv = 1;

    /*Mark the screen as dirty too to mark that there is an something to do on this screen*/
    lv_obj_t * scr = lv_obj_get_screen(obj);
    scr->layout_inv = 1;

    /*Make the display refreshing*/
    lv_disp_t * disp = lv_obj_get_disp(scr);
    lv_timer_pause(disp->refr_timer, false);
}

void lv_obj_update_layout(lv_obj_t * obj)
{
    static bool mutex = false;
    if(mutex) {
        LV_LOG_TRACE("Already running, returning")
        return;
    }
    mutex = true;

    lv_obj_t * scr = lv_obj_get_screen(obj);

    /*Repeat until there where layout invalidations*/
    while(scr->layout_inv) {
        LV_LOG_INFO("Start layout update")
        scr->layout_inv = 0;
        layout_update_core(scr);
        LV_LOG_TRACE("Layout update finished")
    }

    mutex = false;
}

uint32_t lv_layout_register(lv_layout_update_cb_t cb)
{
    layout_cnt++;
    layouts = lv_mem_realloc(layouts, layout_cnt * sizeof(lv_layout_update_cb_t));
    LV_ASSERT_MALLOC(layouts);

    layouts[layout_cnt - 1] = cb;
    return layout_cnt;  /*No -1 to skip 0th index*/
}

void lv_obj_align(lv_obj_t * obj, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs)
{
    lv_obj_set_style_align(obj, LV_PART_MAIN, LV_STATE_DEFAULT, align);
    lv_obj_set_pos(obj, x_ofs, y_ofs);
}

void lv_obj_align_to(lv_obj_t * obj, const lv_obj_t * base, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_obj_update_layout(obj);
    if(base == NULL) base = lv_obj_get_parent(obj);

    LV_ASSERT_OBJ(base, MY_CLASS);

    lv_coord_t x = 0;
    lv_coord_t y = 0;
    lv_obj_t * parent = lv_obj_get_parent(obj);
    lv_coord_t pleft = lv_obj_get_style_pad_left(parent, LV_PART_MAIN);
    lv_coord_t ptop = lv_obj_get_style_pad_top(parent, LV_PART_MAIN);
    switch(align) {
    case LV_ALIGN_CENTER:
        x = lv_obj_get_width_fit(base) / 2 - lv_obj_get_width(obj) / 2;
        y = lv_obj_get_height_fit(base) / 2 - lv_obj_get_height(obj) / 2;
        break;
    case LV_ALIGN_TOP_LEFT:
        x = 0;
        y = 0;
        break;
    case LV_ALIGN_TOP_MID:
        x = lv_obj_get_width_fit(base) / 2 - lv_obj_get_width(obj) / 2;
        y = 0;
        break;

    case LV_ALIGN_TOP_RIGHT:
        x = lv_obj_get_width_fit(base) - lv_obj_get_width(obj);
        y = 0;
        break;

    case LV_ALIGN_BOTTOM_LEFT:
        x = 0;
        y = lv_obj_get_height_fit(base) - lv_obj_get_height(obj);
        break;
    case LV_ALIGN_BOTTOM_MID:
        x = lv_obj_get_width_fit(base) / 2 - lv_obj_get_width(obj) / 2;
        y = lv_obj_get_height_fit(base) - lv_obj_get_height(obj);
        break;

    case LV_ALIGN_BOTTOM_RIGHT:
        x = lv_obj_get_width_fit(base) - lv_obj_get_width(obj);
        y = lv_obj_get_height_fit(base) - lv_obj_get_height(obj);
        break;

    case LV_ALIGN_LEFT_MID:
        x = 0;
        y = lv_obj_get_height_fit(base) / 2 - lv_obj_get_height(obj) / 2;
        break;

    case LV_ALIGN_RIGHT_MID:
        x = lv_obj_get_width_fit(base) - lv_obj_get_width(obj);
        y = lv_obj_get_height_fit(base) / 2 - lv_obj_get_height(obj) / 2;
        break;

    case LV_ALIGN_OUT_TOP_LEFT:
        x = -pleft;
        y = -lv_obj_get_height(obj) - ptop;
        break;

    case LV_ALIGN_OUT_TOP_MID:
        x = lv_obj_get_width(base) / 2 - lv_obj_get_width(obj) / 2 - pleft;
        y = -lv_obj_get_height(obj) - ptop;
        break;

    case LV_ALIGN_OUT_TOP_RIGHT:
        x = lv_obj_get_width(base) - lv_obj_get_width(obj) - pleft;
        y = -lv_obj_get_height(obj) - ptop;
        break;

    case LV_ALIGN_OUT_BOTTOM_LEFT:
        x = - pleft;
        y = lv_obj_get_height(base) - ptop;
        break;

    case LV_ALIGN_OUT_BOTTOM_MID:
        x = lv_obj_get_width(base) / 2 - lv_obj_get_width(obj) / 2 - pleft;
        y = lv_obj_get_height(base) - ptop;
        break;

    case LV_ALIGN_OUT_BOTTOM_RIGHT:
        x = lv_obj_get_width(base) - lv_obj_get_width(obj) - pleft;
        y = lv_obj_get_height(base) - ptop;
        break;

    case LV_ALIGN_OUT_LEFT_TOP:
        x = -lv_obj_get_width(obj) - pleft;
        y = - ptop;
        break;

    case LV_ALIGN_OUT_LEFT_MID:
        x = -lv_obj_get_width(obj) - pleft;
        y = lv_obj_get_height(base) / 2 - lv_obj_get_height(obj) / 2 - ptop;
        break;

    case LV_ALIGN_OUT_LEFT_BOTTOM:
        x = -lv_obj_get_width(obj) - pleft;
        y = lv_obj_get_height(base) - lv_obj_get_height(obj) - ptop;
        break;

    case LV_ALIGN_OUT_RIGHT_TOP:
        x = lv_obj_get_width(base) - pleft;
        y = - ptop;
        break;

    case LV_ALIGN_OUT_RIGHT_MID:
        x = lv_obj_get_width(base) - pleft;
        y = lv_obj_get_height(base) / 2 - lv_obj_get_height(obj) / 2 - ptop;
        break;

    case LV_ALIGN_OUT_RIGHT_BOTTOM:
        x = lv_obj_get_width(base) - pleft;
        y = lv_obj_get_height(base) - lv_obj_get_height(obj) - ptop;
        break;
    }

    x += x_ofs + base->coords.x1 - parent->coords.x1 + lv_obj_get_scroll_left(parent);
    y += y_ofs + base->coords.y1 - parent->coords.y1 + lv_obj_get_scroll_top(parent);
    lv_obj_set_style_align(obj, LV_PART_MAIN, LV_STATE_DEFAULT, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(obj, x, y);

}

void lv_obj_get_coords(const lv_obj_t * obj, lv_area_t * coords)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_area_copy(coords, &obj->coords);
}

lv_coord_t lv_obj_get_x(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_coord_t rel_x;
    lv_obj_t * parent = lv_obj_get_parent(obj);
    if(parent) {
        rel_x  = obj->coords.x1 - parent->coords.x1;
        rel_x += lv_obj_get_scroll_x(parent);
        rel_x -= lv_obj_get_style_pad_left(parent, LV_PART_MAIN);
    }
    else {
        rel_x = obj->coords.x1;
    }
    return rel_x;
}

lv_coord_t lv_obj_get_x2(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    return lv_obj_get_x(obj) + lv_obj_get_width(obj);
}

lv_coord_t lv_obj_get_y(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_coord_t rel_y;
    lv_obj_t * parent = lv_obj_get_parent(obj);
    if(parent) {
        rel_y = obj->coords.y1 - parent->coords.y1;
        rel_y += lv_obj_get_scroll_y(parent);
        rel_y -= lv_obj_get_style_pad_top(parent, LV_PART_MAIN);
    }
    else {
        rel_y = obj->coords.y1;
    }
    return rel_y;
}

lv_coord_t lv_obj_get_y2(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    return lv_obj_get_y(obj) + lv_obj_get_height(obj);
}

lv_coord_t lv_obj_get_width(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    return lv_area_get_width(&obj->coords);
}

lv_coord_t lv_obj_get_height(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    return lv_area_get_height(&obj->coords);
}

lv_coord_t lv_obj_get_width_fit(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_coord_t left = lv_obj_get_style_pad_left(obj, LV_PART_MAIN);
    lv_coord_t right = lv_obj_get_style_pad_right(obj, LV_PART_MAIN);

    return lv_obj_get_width(obj) - left - right;
}

lv_coord_t lv_obj_get_height_fit(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_coord_t top = lv_obj_get_style_pad_top((lv_obj_t *)obj, LV_PART_MAIN);
    lv_coord_t bottom =  lv_obj_get_style_pad_bottom((lv_obj_t *)obj, LV_PART_MAIN);

    return lv_obj_get_height(obj) - top - bottom;
}

void lv_obj_get_coords_fit(const lv_obj_t * obj, lv_area_t * area)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_obj_get_coords(obj, area);
    area->x1 += lv_obj_get_style_pad_left(obj, LV_PART_MAIN);
    area->x2 -= lv_obj_get_style_pad_right(obj, LV_PART_MAIN);
    area->y1 += lv_obj_get_style_pad_top(obj, LV_PART_MAIN);
    area->y2 -= lv_obj_get_style_pad_bottom(obj, LV_PART_MAIN);

}

lv_coord_t lv_obj_get_height_visible(const lv_obj_t * obj)
{
    lv_obj_update_layout(obj);

    lv_coord_t h = LV_COORD_MAX;
    lv_obj_t * parent = lv_obj_get_parent(obj);
    while(parent) {
        h = LV_MIN(lv_obj_get_height_fit(parent), h);
        parent = lv_obj_get_parent(parent);
    }

    return h == LV_COORD_MAX ? LV_DPI_DEF : h;
}

lv_coord_t lv_obj_get_width_visible(const lv_obj_t * obj)
{
    lv_obj_update_layout(obj);

    lv_coord_t w = LV_COORD_MAX;
    lv_obj_t * parent = lv_obj_get_parent(obj);
    while(parent) {
        w = LV_MIN(lv_obj_get_width_fit(parent), w);
        parent = lv_obj_get_parent(parent);
    }

    return w == LV_COORD_MAX ? LV_DPI_DEF : w;
}

lv_coord_t lv_obj_get_self_width(struct _lv_obj_t * obj)
{
    lv_point_t p = {0, LV_COORD_MIN};
    lv_event_send((lv_obj_t * )obj, LV_EVENT_GET_SELF_SIZE, &p);
    return p.x;
}

lv_coord_t lv_obj_get_self_height(struct _lv_obj_t * obj)
{
    lv_point_t p = {LV_COORD_MIN, 0};
    lv_event_send((lv_obj_t * )obj, LV_EVENT_GET_SELF_SIZE, &p);
    return p.y;
}

bool lv_obj_handle_self_size_chg(struct _lv_obj_t * obj)
{
    lv_coord_t w_set = lv_obj_get_style_width(obj, LV_PART_MAIN);
    lv_coord_t h_set = lv_obj_get_style_height(obj, LV_PART_MAIN);
    if(w_set != LV_SIZE_CONTENT && h_set == LV_SIZE_CONTENT) return false;

    lv_obj_refr_size(obj);
    return true;
}

void lv_obj_refr_pos(lv_obj_t * obj)
{
    lv_obj_t * parent = lv_obj_get_parent(obj);
    lv_coord_t x = lv_obj_get_style_x(obj, LV_PART_MAIN);
    lv_coord_t y = lv_obj_get_style_y(obj, LV_PART_MAIN);
    if(parent == NULL) {
        lv_obj_move_to(obj, x, y);
        return;
    }

    lv_align_t align = lv_obj_get_style_align(obj, LV_PART_MAIN);
    if(align == LV_ALIGN_TOP_LEFT) {
        lv_obj_move_to(obj, x, y);
    }
    else {
        lv_coord_t pw = lv_obj_get_width_fit(parent);
        lv_coord_t ph = lv_obj_get_height_fit(parent);
        lv_coord_t w = lv_obj_get_width(obj);
        lv_coord_t h = lv_obj_get_height(obj);

        switch(align) {
        case LV_ALIGN_TOP_MID:
            x += pw / 2 - w / 2;
            break;
        case LV_ALIGN_TOP_RIGHT:
            x += pw - w;
            break;
        case LV_ALIGN_LEFT_MID:
            y += ph / 2 - h / 2;
            break;
        case LV_ALIGN_BOTTOM_LEFT:
            y += ph - h;
            break;
        case LV_ALIGN_BOTTOM_MID:
            x += pw / 2 - w / 2;
            y += ph - h;
            break;
        case LV_ALIGN_BOTTOM_RIGHT:
            x += pw - w;
            y += ph - h;
            break;
        case LV_ALIGN_RIGHT_MID:
            x += pw - w;
            y += ph / 2 - h / 2;
            break;
        case LV_ALIGN_CENTER:
            x += pw / 2 - w / 2;
            y += ph / 2 - h / 2;
            break;
        default:
            break;
        }
        lv_obj_move_to(obj, x, y);
    }
}

void lv_obj_move_to(lv_obj_t * obj, lv_coord_t x, lv_coord_t y)
{
    if(lv_obj_is_layout_positioned(obj)) return;

    /*Convert x and y to absolute coordinates*/
    lv_obj_t * parent = obj->parent;

    if(parent) {
        lv_coord_t pad_left = lv_obj_get_style_pad_left(parent, LV_PART_MAIN);
        lv_coord_t pad_top = lv_obj_get_style_pad_top(parent, LV_PART_MAIN);

        x += pad_left + parent->coords.x1 - lv_obj_get_scroll_x(parent);
        y += pad_top + parent->coords.y1 - lv_obj_get_scroll_y(parent);
    }

    /*Calculate and set the movement*/
    lv_point_t diff;
    diff.x = x - obj->coords.x1;
    diff.y = y - obj->coords.y1;

    /*Do nothing if the position is not changed*/
    /*It is very important else recursive positioning can
     *occur without position change*/
    if(diff.x == 0 && diff.y == 0) return;

    /*Invalidate the original area*/
    lv_obj_invalidate(obj);

    /*Save the original coordinates*/
    lv_area_t ori;
    lv_obj_get_coords(obj, &ori);

    /*Check if the object inside the parent or not*/
    lv_area_t parent_fit_area;
    bool on1 = false;
    if(parent) {
        lv_obj_get_coords_fit(parent, &parent_fit_area);

        /*If the object is already out of the parent and its position is changes
         *surely the scrollbars also changes so invalidate them*/
        on1 = _lv_area_is_in(&ori, &parent_fit_area, 0);
        if(!on1) lv_obj_scrollbar_invalidate(parent);
    }

    obj->coords.x1 += diff.x;
    obj->coords.y1 += diff.y;
    obj->coords.x2 += diff.x;
    obj->coords.y2 += diff.y;

    lv_obj_move_children_by(obj, diff.x, diff.y, false);

    /*Call the ancestor's event handler to the parent too*/
    if(parent) lv_event_send(parent, LV_EVENT_CHILD_CHANGED, obj);

    /*Invalidate the new area*/
    lv_obj_invalidate(obj);

    /*If the object was out of the parent invalidate the new scrollbar area too.
     *If it wasn't out of the parent but out now, also invalidate the srollbars*/
    if(parent) {
        bool on2 = _lv_area_is_in(&obj->coords, &parent_fit_area, 0);
        if(on1 || (!on1 && on2)) lv_obj_scrollbar_invalidate(parent);
    }
}

void lv_obj_move_children_by(lv_obj_t * obj, lv_coord_t x_diff, lv_coord_t y_diff, bool ignore_floating)
{
    uint32_t i;
    for(i = 0; i < lv_obj_get_child_cnt(obj); i++) {
        lv_obj_t * child = lv_obj_get_child(obj, i);
        if(ignore_floating && lv_obj_has_flag(child, LV_OBJ_FLAG_FLOATING)) continue;
        child->coords.x1 += x_diff;
        child->coords.y1 += y_diff;
        child->coords.x2 += x_diff;
        child->coords.y2 += y_diff;

        lv_obj_move_children_by(child, x_diff, y_diff, false);
    }
}


void lv_obj_invalidate_area(const lv_obj_t * obj, const lv_area_t * area)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_area_t area_tmp;
    lv_area_copy(&area_tmp, area);
    bool visible = lv_obj_area_is_visible(obj, &area_tmp);

    if(visible) _lv_inv_area(lv_obj_get_disp(obj), &area_tmp);
}

void lv_obj_invalidate(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    /*Truncate the area to the object*/
    lv_area_t obj_coords;
    lv_coord_t ext_size = _lv_obj_get_ext_draw_size(obj);
    lv_area_copy(&obj_coords, &obj->coords);
    obj_coords.x1 -= ext_size;
    obj_coords.y1 -= ext_size;
    obj_coords.x2 += ext_size;
    obj_coords.y2 += ext_size;

    lv_obj_invalidate_area(obj, &obj_coords);

}

bool lv_obj_area_is_visible(const lv_obj_t * obj, lv_area_t * area)
{
    if(lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return false;

    /*Invalidate the object only if it belongs to the current or previous'*/
    lv_obj_t * obj_scr = lv_obj_get_screen(obj);
    lv_disp_t * disp   = lv_obj_get_disp(obj_scr);
    if(obj_scr != lv_disp_get_scr_act(disp) &&
       obj_scr != lv_disp_get_scr_prev(disp) &&
       obj_scr != lv_disp_get_layer_top(disp) &&
       obj_scr != lv_disp_get_layer_sys(disp))
    {
    	return false;
    }

	/*Truncate the area to the object*/
	lv_area_t obj_coords;
	lv_coord_t ext_size = _lv_obj_get_ext_draw_size(obj);
	lv_area_copy(&obj_coords, &obj->coords);
	obj_coords.x1 -= ext_size;
	obj_coords.y1 -= ext_size;
	obj_coords.x2 += ext_size;
	obj_coords.y2 += ext_size;

	bool is_common;

	is_common = _lv_area_intersect(area, area, &obj_coords);
	if(is_common == false) return false;  /*The area is not on the object*/

	/*Truncate recursively to the parents*/
	lv_obj_t * par = lv_obj_get_parent(obj);
	while(par != NULL) {
		is_common = _lv_area_intersect(area, area, &par->coords);
		if(is_common == false) return false;       /*If no common parts with parent break;*/
		if(lv_obj_has_flag(par, LV_OBJ_FLAG_HIDDEN)) return false; /*If the parent is hidden then the child is hidden and won't be drawn*/

		par = lv_obj_get_parent(par);
	}

    return true;
}

bool lv_obj_is_visible(const lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_area_t obj_coords;
    lv_coord_t ext_size = _lv_obj_get_ext_draw_size(obj);
    lv_area_copy(&obj_coords, &obj->coords);
    obj_coords.x1 -= ext_size;
    obj_coords.y1 -= ext_size;
    obj_coords.x2 += ext_size;
    obj_coords.y2 += ext_size;

    return lv_obj_area_is_visible(obj, &obj_coords);

}

void lv_obj_set_ext_click_area(lv_obj_t * obj, lv_coord_t size)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_obj_allocate_spec_attr(obj);
    obj->spec_attr->ext_click_pad = size;
}

void lv_obj_get_click_area(const lv_obj_t * obj, lv_area_t * area)
{
    lv_area_copy(area, &obj->coords);
    if(obj->spec_attr) {
        area->x1 -= obj->spec_attr->ext_click_pad;
        area->x2 += obj->spec_attr->ext_click_pad;
        area->y1 -= obj->spec_attr->ext_click_pad;
        area->y2 += obj->spec_attr->ext_click_pad;
    }
}

bool lv_obj_hit_test(lv_obj_t * obj, const lv_point_t * point)
{
    lv_area_t a;
    lv_obj_get_click_area(obj, &a);
    bool res = _lv_area_is_point_on(&a, point, 0);
    if(res == false) return false;

    if(lv_obj_has_flag(obj, LV_OBJ_FLAG_ADV_HITTEST)) {
        lv_hit_test_info_t hit_info;
        hit_info.point = point;
        hit_info.result = true;
        lv_event_send(obj, LV_EVENT_HIT_TEST, &hit_info);
        return hit_info.result;
    }

    return res;
}


/**********************
 *   STATIC FUNCTIONS
 **********************/


/**
 * Calculate the "auto size". It's `auto_size = max(children_size, self_size)`
 * @param obj pointer to an object
 * @param w_out store the width here. NULL to not calculate width
 * @param h_out store the height here. NULL to not calculate height
 */
static void calc_auto_size(lv_obj_t * obj, lv_coord_t * w_out, lv_coord_t * h_out)
{
    if(!w_out && !h_out) return;
    /*Get the bounding box of the children*/
    if(w_out) {
        lv_coord_t scroll_right = lv_obj_get_scroll_right(obj);
        lv_coord_t scroll_left = lv_obj_get_scroll_left(obj);
        *w_out = lv_obj_get_width(obj) + scroll_right + scroll_left;
    }

    if(h_out) {
        lv_coord_t scroll_bottom = lv_obj_get_scroll_bottom(obj);
        lv_coord_t scroll_top = lv_obj_get_scroll_top(obj);
        *h_out = lv_obj_get_height(obj) + scroll_bottom + scroll_top;
    }
}

static void layout_update_core(lv_obj_t * obj)
{
    uint32_t i;
    for(i = 0; i < lv_obj_get_child_cnt(obj); i++) {
        lv_obj_t * child = lv_obj_get_child(obj, i);
        layout_update_core(child);
    }

    if(obj->layout_inv == 0) return;

    if(lv_obj_get_screen(obj) != obj) obj->layout_inv = 0;

    lv_obj_refr_size(obj);
    lv_obj_refr_pos(obj);

    /*Be sure the bottom side is not remains scrolled in*/
    lv_coord_t st = lv_obj_get_scroll_top(obj);
    lv_coord_t sb = lv_obj_get_scroll_bottom(obj);
    if(sb < 0 && st > 0) {
        sb = LV_MIN(st, -sb);
        lv_obj_scroll_by(obj, 0, sb, LV_ANIM_OFF);
    }

    lv_coord_t sl = lv_obj_get_scroll_left(obj);
    lv_coord_t sr = lv_obj_get_scroll_right(obj);
    if(lv_obj_get_base_dir(obj) != LV_BIDI_DIR_RTL) {
        /*Be sure the left side is not remains scrolled in*/
        if(sr < 0 && sl > 0) {
            sr = LV_MIN(sl, -sr);
            lv_obj_scroll_by(obj, 0, sr, LV_ANIM_OFF);
        }
    } else {
        /*Be sure the right side is not remains scrolled in*/
        if(sl < 0 && sr > 0) {
            sr = LV_MIN(sr, -sl);
            lv_obj_scroll_by(obj, 0, sl, LV_ANIM_OFF);
        }
    }

    if(lv_obj_get_child_cnt(obj) > 0) {
        uint32_t layout_id = lv_obj_get_style_layout(obj, LV_PART_MAIN);
        if(layout_id > 0 && layout_id <= layout_cnt) {
            layouts[layout_id -1](obj);
        }
    }
}