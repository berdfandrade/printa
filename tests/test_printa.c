/* Testes de unidade do printa (GLib GTest). Não precisam de display. */
#include <gdk/gdk.h>

#include "geometry.h"
#include "output.h"

#define assert_rect(r, X, Y, W, H)                                                                   \
    do {                                                                                             \
        GdkRectangle r_ = (r);                                                                       \
        g_assert_cmpint(r_.x, ==, (X));                                                              \
        g_assert_cmpint(r_.y, ==, (Y));                                                              \
        g_assert_cmpint(r_.width, ==, (W));                                                          \
        g_assert_cmpint(r_.height, ==, (H));                                                         \
    } while (0)

/* ---------- rect_from_drag ---------- */

static void test_drag_down_right(void)
{
    assert_rect(rect_from_drag(100, 120, 300, 270, 800, 600), 100, 120, 200, 150);
}

static void test_drag_any_direction(void)
{
    /* De baixo-direita para cima-esquerda dá o mesmo retângulo. */
    assert_rect(rect_from_drag(300, 270, 100, 120, 800, 600), 100, 120, 200, 150);
    assert_rect(rect_from_drag(300, 120, 100, 270, 800, 600), 100, 120, 200, 150);
}

static void test_drag_rounds_outward(void)
{
    /* Coordenadas fracionárias cobrem todo pixel tocado. */
    assert_rect(rect_from_drag(10.6, 20.2, 30.1, 40.9, 800, 600), 10, 20, 21, 21);
}

static void test_drag_clipped_to_screen(void)
{
    assert_rect(rect_from_drag(-50, -20, 100, 80, 800, 600), 0, 0, 100, 80);
    assert_rect(rect_from_drag(700, 500, 900, 650, 800, 600), 700, 500, 100, 100);
}

static void test_drag_off_screen_is_empty(void)
{
    assert_rect(rect_from_drag(900, 700, 950, 750, 800, 600), 0, 0, 0, 0);
}

/* ---------- is_click ---------- */

static void test_is_click(void)
{
    g_assert_true(is_click(10, 10, 10, 10));
    g_assert_true(is_click(10, 10, 10 + CLICK_SLOP - 1, 10 - (CLICK_SLOP - 1)));
    g_assert_false(is_click(10, 10, 10 + CLICK_SLOP, 10));
    g_assert_false(is_click(10, 10, 10, 10 - CLICK_SLOP));
}

/* ---------- window_at ---------- */

static GArray *stack_of(const GdkRectangle *rects, guint n)
{
    GArray *a = g_array_new(FALSE, FALSE, sizeof(GdkRectangle));
    g_array_append_vals(a, rects, n);
    return a;
}

static void test_window_at_topmost_wins(void)
{
    const GdkRectangle rects[] = {
        {0, 0, 800, 600},     /* área de trabalho, embaixo de tudo */
        {100, 100, 300, 200}, /* janela A */
        {200, 150, 300, 200}, /* janela B, por cima de A */
    };
    GArray *stack = stack_of(rects, G_N_ELEMENTS(rects));
    GdkRectangle hit;

    g_assert_true(window_at(stack, 250, 200, &hit)); /* sobreposição: B */
    assert_rect(hit, 200, 150, 300, 200);
    g_assert_true(window_at(stack, 120, 120, &hit)); /* só A */
    assert_rect(hit, 100, 100, 300, 200);
    g_assert_true(window_at(stack, 10, 590, &hit)); /* só a área de trabalho */
    assert_rect(hit, 0, 0, 800, 600);
    g_array_free(stack, TRUE);
}

static void test_window_at_edges(void)
{
    const GdkRectangle rects[] = {{100, 100, 50, 50}};
    GArray *stack = stack_of(rects, 1);
    GdkRectangle hit;

    g_assert_true(window_at(stack, 100, 100, &hit));   /* borda esquerda/superior: dentro */
    g_assert_true(window_at(stack, 149.9, 149.9, &hit));
    g_assert_false(window_at(stack, 150, 120, &hit));  /* borda direita: fora */
    g_assert_false(window_at(stack, 120, 150, &hit));  /* borda inferior: fora */
    g_assert_false(window_at(stack, 99.9, 120, &hit));
    g_array_free(stack, TRUE);
}

static void test_window_at_empty_stack(void)
{
    GArray *stack = g_array_new(FALSE, FALSE, sizeof(GdkRectangle));
    GdkRectangle hit;
    g_assert_false(window_at(stack, 10, 10, &hit));
    g_array_free(stack, TRUE);
}

/* ---------- without_csd_shadow ---------- */

static void test_csd_shadow_none(void)
{
    const long ext[4] = {0, 0, 0, 0};
    assert_rect(without_csd_shadow((GdkRectangle){10, 20, 300, 200}, ext, 1), 10, 20, 300, 200);
}

static void test_csd_shadow_removed(void)
{
    const long ext[4] = {20, 22, 14, 26}; /* esquerda, direita, cima, baixo */
    assert_rect(without_csd_shadow((GdkRectangle){100, 100, 400, 300}, ext, 1), 120, 114, 358, 260);
}

static void test_csd_shadow_hidpi(void)
{
    /* As extensões vêm em pixels do dispositivo; a geometria é lógica. */
    const long ext[4] = {40, 40, 20, 60};
    assert_rect(without_csd_shadow((GdkRectangle){100, 100, 400, 300}, ext, 2), 120, 110, 360, 260);
}

/* ---------- save_button_position ---------- */

static void test_button_below_selection(void)
{
    GdkRectangle sel = {100, 120, 200, 150};
    /* Alinhado à direita da seleção, 8 px abaixo. */
    assert_rect(save_button_position(&sel, 75, 32, 800, 600), 225, 278, 75, 32);
}

static void test_button_inside_at_bottom_edge(void)
{
    GdkRectangle sel = {500, 450, 280, 149};
    assert_rect(save_button_position(&sel, 75, 32, 800, 600), 705, 559, 75, 32);
}

static void test_button_clamped_left(void)
{
    /* Seleção mais estreita que o botão, colada à esquerda. */
    GdkRectangle sel = {0, 10, 30, 30};
    assert_rect(save_button_position(&sel, 75, 32, 800, 600), 0, 48, 75, 32);
}

static void test_button_full_screen_selection(void)
{
    GdkRectangle sel = {0, 0, 800, 600};
    assert_rect(save_button_position(&sel, 75, 32, 800, 600), 725, 560, 75, 32);
}

/* ---------- crop_capture ---------- */

/* Imagem RGB w×h em que o pixel (x, y) vale (x*10, y*10, 7). */
static GdkPixbuf *gradient(int w, int h)
{
    GdkPixbuf *p = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
    int stride = gdk_pixbuf_get_rowstride(p);
    guchar *px = gdk_pixbuf_get_pixels(p);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            guchar *c = px + y * stride + x * 3;
            c[0] = x * 10;
            c[1] = y * 10;
            c[2] = 7;
        }
    return p;
}

static void assert_pixel(GdkPixbuf *p, int x, int y, int r, int g)
{
    const guchar *c = gdk_pixbuf_read_pixels(p) + y * gdk_pixbuf_get_rowstride(p) +
                      x * gdk_pixbuf_get_n_channels(p);
    g_assert_cmpint(c[0], ==, r);
    g_assert_cmpint(c[1], ==, g);
}

static void test_crop_region(void)
{
    GdkPixbuf *shot = gradient(4, 4);
    GdkPixbuf *out = crop_capture(shot, &(GdkRectangle){1, 2, 2, 2}, 1);
    g_assert_nonnull(out);
    g_assert_cmpint(gdk_pixbuf_get_width(out), ==, 2);
    g_assert_cmpint(gdk_pixbuf_get_height(out), ==, 2);
    assert_pixel(out, 0, 0, 10, 20);
    assert_pixel(out, 1, 1, 20, 30);
    g_object_unref(out);
    g_object_unref(shot);
}

static void test_crop_hidpi(void)
{
    /* Tela lógica 4×4 com escala 2: captura de 8×8 pixels. */
    GdkPixbuf *shot = gradient(8, 8);
    GdkPixbuf *out = crop_capture(shot, &(GdkRectangle){1, 0, 2, 1}, 2);
    g_assert_cmpint(gdk_pixbuf_get_width(out), ==, 4);
    g_assert_cmpint(gdk_pixbuf_get_height(out), ==, 2);
    assert_pixel(out, 0, 0, 20, 0);
    assert_pixel(out, 3, 1, 50, 10);
    g_object_unref(out);
    g_object_unref(shot);
}

static void test_crop_clipped_to_capture(void)
{
    GdkPixbuf *shot = gradient(4, 4);
    GdkPixbuf *out = crop_capture(shot, &(GdkRectangle){3, 3, 5, 5}, 1);
    g_assert_cmpint(gdk_pixbuf_get_width(out), ==, 1);
    g_assert_cmpint(gdk_pixbuf_get_height(out), ==, 1);
    assert_pixel(out, 0, 0, 30, 30);
    g_object_unref(out);

    g_assert_null(crop_capture(shot, &(GdkRectangle){10, 10, 5, 5}, 1));
    g_object_unref(shot);
}

static void test_crop_is_independent_copy(void)
{
    GdkPixbuf *shot = gradient(4, 4);
    GdkPixbuf *out = crop_capture(shot, &(GdkRectangle){0, 0, 2, 2}, 1);
    gdk_pixbuf_fill(shot, 0xffffffff); /* mexer na captura não altera o recorte */
    assert_pixel(out, 1, 1, 10, 10);
    g_object_unref(out);
    g_object_unref(shot);
}

/* ---------- image_format_for / capture_filename ---------- */

static void test_image_format(void)
{
    g_assert_cmpstr(image_format_for("/tmp/a.png"), ==, "png");
    g_assert_cmpstr(image_format_for("/tmp/a.jpg"), ==, "jpeg");
    g_assert_cmpstr(image_format_for("/tmp/a.JPEG"), ==, "jpeg");
    g_assert_cmpstr(image_format_for("/tmp/a.Jpg"), ==, "jpeg");
    g_assert_cmpstr(image_format_for("/tmp/captura"), ==, "png");
    g_assert_cmpstr(image_format_for("/tmp/fotos.jpg/captura"), ==, "png"); /* ponto só no diretório */
    g_assert_cmpstr(image_format_for("/tmp/a.jpg.png"), ==, "png");
}

static void test_capture_filename(void)
{
    GDateTime *t = g_date_time_new_local(2026, 11, 5, 9, 4, 7);
    char *name = capture_filename(t);
    g_assert_cmpstr(name, ==, "Captura_2026-11-05_09-04-07.png");
    g_free(name);
    g_date_time_unref(t);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/geometry/drag/down-right", test_drag_down_right);
    g_test_add_func("/geometry/drag/any-direction", test_drag_any_direction);
    g_test_add_func("/geometry/drag/rounds-outward", test_drag_rounds_outward);
    g_test_add_func("/geometry/drag/clipped-to-screen", test_drag_clipped_to_screen);
    g_test_add_func("/geometry/drag/off-screen-is-empty", test_drag_off_screen_is_empty);
    g_test_add_func("/geometry/is-click", test_is_click);
    g_test_add_func("/geometry/window-at/topmost-wins", test_window_at_topmost_wins);
    g_test_add_func("/geometry/window-at/edges", test_window_at_edges);
    g_test_add_func("/geometry/window-at/empty-stack", test_window_at_empty_stack);
    g_test_add_func("/geometry/csd-shadow/none", test_csd_shadow_none);
    g_test_add_func("/geometry/csd-shadow/removed", test_csd_shadow_removed);
    g_test_add_func("/geometry/csd-shadow/hidpi", test_csd_shadow_hidpi);
    g_test_add_func("/geometry/save-button/below", test_button_below_selection);
    g_test_add_func("/geometry/save-button/inside-at-bottom", test_button_inside_at_bottom_edge);
    g_test_add_func("/geometry/save-button/clamped-left", test_button_clamped_left);
    g_test_add_func("/geometry/save-button/full-screen", test_button_full_screen_selection);
    g_test_add_func("/output/crop/region", test_crop_region);
    g_test_add_func("/output/crop/hidpi", test_crop_hidpi);
    g_test_add_func("/output/crop/clipped", test_crop_clipped_to_capture);
    g_test_add_func("/output/crop/independent-copy", test_crop_is_independent_copy);
    g_test_add_func("/output/image-format", test_image_format);
    g_test_add_func("/output/capture-filename", test_capture_filename);

    return g_test_run();
}
