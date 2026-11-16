#include "output.h"

#include <string.h>

GdkPixbuf *crop_capture(GdkPixbuf *shot, const GdkRectangle *r, int scale)
{
    GdkRectangle device = {r->x * scale, r->y * scale, r->width * scale, r->height * scale};
    GdkRectangle bounds = {0, 0, gdk_pixbuf_get_width(shot), gdk_pixbuf_get_height(shot)};
    GdkRectangle area;
    if (!gdk_rectangle_intersect(&device, &bounds, &area))
        return NULL;

    /* O subpixbuf compartilha memória com a captura; a cópia não. */
    GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(shot, area.x, area.y, area.width, area.height);
    GdkPixbuf *copy = gdk_pixbuf_copy(sub);
    g_object_unref(sub);
    return copy;
}

const char *image_format_for(const char *path)
{
    const char *base = strrchr(path, '/');
    const char *ext = strrchr(base ? base + 1 : path, '.');
    if (ext && (g_ascii_strcasecmp(ext, ".jpg") == 0 || g_ascii_strcasecmp(ext, ".jpeg") == 0))
        return "jpeg";
    return "png";
}

char *capture_filename(GDateTime *when)
{
    return g_date_time_format(when, "Captura_%Y-%m-%d_%H-%M-%S.png");
}
