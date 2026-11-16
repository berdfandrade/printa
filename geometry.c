#include "geometry.h"

#include <math.h>

#define BUTTON_GAP 8 /* distância entre a seleção e o botão Salvar */

GdkRectangle rect_from_drag(double x0, double y0, double x1, double y1, int screen_w, int screen_h)
{
    GdkRectangle r, screen = {0, 0, screen_w, screen_h}, clipped = {0, 0, 0, 0};
    r.x = (int)floor(MIN(x0, x1));
    r.y = (int)floor(MIN(y0, y1));
    r.width = (int)ceil(MAX(x0, x1)) - r.x;
    r.height = (int)ceil(MAX(y0, y1)) - r.y;
    if (!gdk_rectangle_intersect(&r, &screen, &clipped))
        return (GdkRectangle){0, 0, 0, 0};
    return clipped;
}

gboolean is_click(double x0, double y0, double x1, double y1)
{
    return fabs(x1 - x0) < CLICK_SLOP && fabs(y1 - y0) < CLICK_SLOP;
}

gboolean window_at(const GArray *windows, double x, double y, GdkRectangle *out)
{
    for (guint i = windows->len; i-- > 0;) {
        const GdkRectangle *r = &g_array_index(windows, GdkRectangle, i);
        if (x >= r->x && x < r->x + r->width && y >= r->y && y < r->y + r->height) {
            *out = *r;
            return TRUE;
        }
    }
    return FALSE;
}

GdkRectangle without_csd_shadow(GdkRectangle frame, const long ext[4], int scale)
{
    frame.x += ext[0] / scale;
    frame.width -= (ext[0] + ext[1]) / scale;
    frame.y += ext[2] / scale;
    frame.height -= (ext[2] + ext[3]) / scale;
    return frame;
}

GdkRectangle save_button_position(const GdkRectangle *sel, int btn_w, int btn_h, int screen_w, int screen_h)
{
    GdkRectangle pos = {0, 0, btn_w, btn_h};
    pos.x = CLAMP(sel->x + sel->width - btn_w, 0, MAX(screen_w - btn_w, 0));
    pos.y = sel->y + sel->height + BUTTON_GAP;
    if (pos.y + btn_h > screen_h)
        pos.y = MAX(sel->y + sel->height - btn_h - BUTTON_GAP, 0);
    return pos;
}
