/*
 * printa — captura de tela simples em GTK3/X11, no estilo do Flameshot.
 *
 * Fluxo: congela a tela inteira, mostra por cima uma camada escurecida e
 * deixa o usuário escolher o que capturar:
 *   - passar o mouse destaca a janela sob o cursor; um clique a seleciona;
 *   - arrastar seleciona uma região livre;
 *   - Enter salva em ~/Imagens (e copia), Ctrl+C só copia,
 *     Ctrl+S abre "Salvar como", Esc ou botão direito cancela.
 *
 * Só funciona em X11: no Wayland o compositor não deixa ler a tela assim.
 */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <math.h>

#define CLICK_SLOP 4 /* movimento máximo (px) para um arrasto contar como clique */

typedef struct {
    GdkPixbuf *shot;   /* tela inteira, em pixels do dispositivo */
    int scale;         /* fator de escala HiDPI */
    int width, height; /* tamanho lógico da tela */
    GArray *windows;   /* GdkRectangle das janelas visíveis, a do topo por último */

    GtkWidget *win;
    GtkWidget *save_btn; /* aparece junto à seleção */
    guint grab_timer;

    gboolean dragging;
    double x0, y0, x1, y1;
    gboolean have_sel;
    GdkRectangle sel;
    gboolean have_hover;
    GdkRectangle hover;

    int status; /* código de saída */
} App;

/* ---------- coleta da tela e das janelas ---------- */

/* Janelas GTK com decoração própria (CSD) incluem uma sombra invisível na
 * própria geometria; _GTK_FRAME_EXTENTS diz quanto descontar. */
static void csd_shadow(Display *dpy, Window xid, long ext[4])
{
    ext[0] = ext[1] = ext[2] = ext[3] = 0;
    Atom prop = XInternAtom(dpy, "_GTK_FRAME_EXTENTS", True);
    if (prop == None)
        return;

    Atom type;
    int format;
    unsigned long n, after;
    unsigned char *data = NULL;
    if (XGetWindowProperty(dpy, xid, prop, 0, 4, False, XA_CARDINAL, &type, &format,
                           &n, &after, &data) == Success &&
        type == XA_CARDINAL && format == 32 && n == 4) {
        long *v = (long *)data;
        for (int i = 0; i < 4; i++)
            ext[i] = v[i];
    }
    if (data)
        XFree(data);
}

static void collect_windows(App *app, GdkScreen *screen)
{
    GdkDisplay *display = gdk_screen_get_display(screen);
    Display *dpy = GDK_DISPLAY_XDISPLAY(display);
    GdkRectangle screen_rect = {0, 0, app->width, app->height};

    /* Ordem de empilhamento do gerenciador de janelas, de baixo para cima. */
    GList *stack = gdk_screen_get_window_stack(screen);
    for (GList *l = stack; l; l = l->next) {
        GdkWindow *w = l->data;
        Window xid = gdk_x11_window_get_xid(w);
        XWindowAttributes attrs;
        GdkRectangle r, clipped;
        long ext[4];

        /* A janela pode sumir a qualquer momento: ignore erros do X. */
        gdk_x11_display_error_trap_push(display);
        Status ok = XGetWindowAttributes(dpy, xid, &attrs);
        gboolean viewable = ok && attrs.map_state == IsViewable;
        if (viewable) {
            gdk_window_get_frame_extents(w, &r);
            csd_shadow(dpy, xid, ext);
        }
        if (gdk_x11_display_error_trap_pop(display) == 0 && viewable) {
            r.x += ext[0] / app->scale;
            r.width -= (ext[0] + ext[1]) / app->scale;
            r.y += ext[2] / app->scale;
            r.height -= (ext[2] + ext[3]) / app->scale;
            if (gdk_rectangle_intersect(&r, &screen_rect, &clipped))
                g_array_append_val(app->windows, clipped);
        }
        g_object_unref(w);
    }
    g_list_free(stack);
}

static gboolean capture_screen(App *app)
{
    GdkWindow *root = gdk_get_default_root_window();
    app->scale = gdk_window_get_scale_factor(root);
    app->width = gdk_window_get_width(root);
    app->height = gdk_window_get_height(root);
    app->shot = gdk_pixbuf_get_from_window(root, 0, 0, app->width, app->height);
    if (!app->shot)
        return FALSE;
    collect_windows(app, gdk_window_get_screen(root));
    return TRUE;
}

/* ---------- geometria da seleção ---------- */

static gboolean window_at(App *app, double x, double y, GdkRectangle *out)
{
    for (int i = (int)app->windows->len - 1; i >= 0; i--) {
        GdkRectangle *r = &g_array_index(app->windows, GdkRectangle, i);
        if (x >= r->x && x < r->x + r->width && y >= r->y && y < r->y + r->height) {
            *out = *r;
            return TRUE;
        }
    }
    return FALSE;
}

static GdkRectangle drag_rect(App *app)
{
    GdkRectangle r, screen_rect = {0, 0, app->width, app->height}, clipped = {0};
    r.x = (int)floor(MIN(app->x0, app->x1));
    r.y = (int)floor(MIN(app->y0, app->y1));
    r.width = (int)ceil(MAX(app->x0, app->x1)) - r.x;
    r.height = (int)ceil(MAX(app->y0, app->y1)) - r.y;
    gdk_rectangle_intersect(&r, &screen_rect, &clipped);
    return clipped;
}

/* Clique simples: seleciona a janela sob o cursor (ou nada). */
static void select_at(App *app, double x, double y)
{
    app->have_sel = window_at(app, x, y, &app->sel);
}

/* O que está destacado agora; *tentative = ainda não é uma seleção. */
static gboolean shown_rect(App *app, GdkRectangle *r, gboolean *tentative)
{
    *tentative = FALSE;
    if (app->dragging) {
        *r = drag_rect(app);
        return TRUE;
    }
    if (app->have_sel) {
        *r = app->sel;
        return TRUE;
    }
    if (app->have_hover) {
        *r = app->hover;
        *tentative = TRUE;
        return TRUE;
    }
    return FALSE;
}

/* ---------- resultado: salvar / copiar ---------- */

static GdkPixbuf *crop(App *app)
{
    GdkRectangle r = app->have_sel ? app->sel : (GdkRectangle){0, 0, app->width, app->height};
    int s = app->scale;
    GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(app->shot, r.x * s, r.y * s, r.width * s, r.height * s);
    GdkPixbuf *copy = gdk_pixbuf_copy(sub);
    g_object_unref(sub);
    return copy;
}

static char *default_dir(void)
{
    const char *pics = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
    return g_strdup(pics ? pics : g_get_home_dir());
}

static char *default_name(void)
{
    GDateTime *now = g_date_time_new_now_local();
    char *name = g_date_time_format(now, "Captura_%Y-%m-%d_%H-%M-%S.png");
    g_date_time_unref(now);
    return name;
}

static gboolean save_pixbuf(GdkPixbuf *pix, const char *path)
{
    const char *type = "png";
    if (g_str_has_suffix(path, ".jpg") || g_str_has_suffix(path, ".jpeg") ||
        g_str_has_suffix(path, ".JPG") || g_str_has_suffix(path, ".JPEG"))
        type = "jpeg";

    GError *err = NULL;
    if (!gdk_pixbuf_save(pix, path, type, &err, NULL)) {
        g_printerr("printa: não foi possível salvar %s: %s\n", path, err->message);
        g_error_free(err);
        return FALSE;
    }
    g_print("%s\n", path);
    return TRUE;
}

/* Entrega a imagem ao gerenciador de área de transferência, para que ela
 * continue disponível depois que o printa terminar. */
static void copy_pixbuf(GdkPixbuf *pix)
{
    GtkClipboard *cb = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_image(cb, pix);
    gtk_clipboard_store(cb);
}

static void finish(App *app, int status)
{
    app->status = status;
    gtk_main_quit();
}

static void accept_save_and_copy(App *app)
{
    GdkPixbuf *pix = crop(app);
    char *dir = default_dir(), *name = default_name();
    char *path = g_build_filename(dir, name, NULL);
    g_mkdir_with_parents(dir, 0755);
    gboolean ok = save_pixbuf(pix, path);
    copy_pixbuf(pix);
    g_free(path);
    g_free(name);
    g_free(dir);
    g_object_unref(pix);
    finish(app, ok ? 0 : 1);
}

static void accept_copy(App *app)
{
    GdkPixbuf *pix = crop(app);
    copy_pixbuf(pix);
    g_object_unref(pix);
    finish(app, 0);
}

static void accept_save_as(App *app)
{
    /* Esconde a camada (e solta o grab) para o diálogo poder receber eventos. */
    gdk_seat_ungrab(gdk_display_get_default_seat(gdk_display_get_default()));
    gtk_widget_hide(app->win);

    GtkWidget *dlg = gtk_file_chooser_dialog_new("Salvar captura", NULL, GTK_FILE_CHOOSER_ACTION_SAVE,
                                                 "_Cancelar", GTK_RESPONSE_CANCEL,
                                                 "_Salvar", GTK_RESPONSE_ACCEPT, NULL);
    GtkFileChooser *fc = GTK_FILE_CHOOSER(dlg);
    char *dir = default_dir(), *name = default_name();
    gtk_file_chooser_set_do_overwrite_confirmation(fc, TRUE);
    gtk_file_chooser_set_current_folder(fc, dir);
    gtk_file_chooser_set_current_name(fc, name);
    g_free(dir);
    g_free(name);

    gboolean saved = FALSE, ok = FALSE;
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(fc);
        GdkPixbuf *pix = crop(app);
        ok = save_pixbuf(pix, path);
        saved = TRUE;
        g_object_unref(pix);
        g_free(path);
    }
    gtk_widget_destroy(dlg);

    if (saved)
        finish(app, ok ? 0 : 1);
    else
        gtk_widget_show_all(app->win); /* cancelou o diálogo: volta à seleção */
}

/* ---------- desenho ---------- */

/* Caixa de texto arredondada; (x, y) é o canto superior esquerdo,
 * ou o centro superior se center == TRUE. */
static void draw_box(cairo_t *cr, const char *text, double x, double y, gboolean center)
{
    const double pad = 6;
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font = pango_font_description_from_string("Sans Bold 10");
    pango_layout_set_font_description(layout, font);
    pango_font_description_free(font);
    pango_layout_set_text(layout, text, -1);

    int tw, th;
    pango_layout_get_pixel_size(layout, &tw, &th);
    double w = tw + 2 * pad, h = th + 2 * pad;
    if (center)
        x -= w / 2;

    double rad = 5;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - rad, y + rad, rad, -G_PI / 2, 0);
    cairo_arc(cr, x + w - rad, y + h - rad, rad, 0, G_PI / 2);
    cairo_arc(cr, x + rad, y + h - rad, rad, G_PI / 2, G_PI);
    cairo_arc(cr, x + rad, y + rad, rad, G_PI, 3 * G_PI / 2);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, 0.08, 0.08, 0.1, 0.88);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_move_to(cr, x + pad, y + pad);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}

static void hint_anchor(App *app, double *cx, double *top)
{
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *mon = gdk_display_get_primary_monitor(display);
    if (!mon)
        mon = gdk_display_get_monitor(display, 0);
    GdkRectangle g = {0, 0, app->width, app->height};
    if (mon)
        gdk_monitor_get_geometry(mon, &g);
    *cx = g.x + g.width / 2.0;
    *top = g.y + 24;
}

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, App *app)
{
    (void)widget;

    /* A tela congelada. */
    cairo_save(cr);
    cairo_scale(cr, 1.0 / app->scale, 1.0 / app->scale);
    gdk_cairo_set_source_pixbuf(cr, app->shot, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);

    /* Escurece tudo, menos o retângulo destacado. */
    GdkRectangle r;
    gboolean tentative;
    gboolean have = shown_rect(app, &r, &tentative) && r.width > 0 && r.height > 0;
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_rectangle(cr, 0, 0, app->width, app->height);
    if (have)
        cairo_rectangle(cr, r.x, r.y, r.width, r.height);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
    cairo_fill(cr);

    if (have) {
        cairo_set_source_rgb(cr, 0.55, 0.33, 1.0);
        cairo_set_line_width(cr, 2);
        if (tentative) {
            double dash[] = {6, 4};
            cairo_set_dash(cr, dash, 2, 0);
        }
        cairo_rectangle(cr, r.x + 1, r.y + 1, r.width - 2, r.height - 2);
        cairo_stroke(cr);
        cairo_set_dash(cr, NULL, 0, 0);

        char *size = g_strdup_printf("%d × %d", r.width * app->scale, r.height * app->scale);
        double ly = r.y >= 32 ? r.y - 30 : r.y + 6;
        draw_box(cr, size, r.x + (r.y >= 32 ? 0 : 6), ly, FALSE);
        g_free(size);
    }

    double cx, top;
    hint_anchor(app, &cx, &top);
    draw_box(cr,
             app->have_sel
                 ? "Enter: salvar e copiar   ·   Ctrl+C: copiar   ·   Ctrl+S: salvar como   ·   Esc: sair"
                 : "Clique numa janela ou arraste uma região   ·   Enter: tela inteira   ·   Esc: sair",
             cx, top, TRUE);
    return TRUE;
}

/* ---------- eventos ---------- */

/* O botão Salvar só faz sentido com uma seleção pronta. */
static void update_save_button(App *app)
{
    gtk_widget_set_visible(app->save_btn, app->have_sel && !app->dragging);
    gtk_widget_queue_resize(app->save_btn); /* reposiciona junto à nova seleção */
}

static void on_save_clicked(GtkButton *btn, App *app)
{
    (void)btn;
    accept_save_and_copy(app);
}

static gboolean on_button_press(GtkWidget *w, GdkEventButton *ev, App *app)
{
    if (ev->button == 3) {
        finish(app, 1);
        return TRUE;
    }
    if (ev->button != 1)
        return FALSE;

    if (ev->type == GDK_2BUTTON_PRESS) {
        /* Duplo clique: seleciona o que está sob o cursor e salva. */
        app->dragging = FALSE;
        select_at(app, ev->x, ev->y);
        if (app->have_sel)
            accept_save_and_copy(app);
        return TRUE;
    }
    if (ev->type != GDK_BUTTON_PRESS)
        return FALSE;

    app->dragging = TRUE;
    app->have_sel = FALSE;
    app->x0 = app->x1 = ev->x;
    app->y0 = app->y1 = ev->y;
    update_save_button(app);
    gtk_widget_queue_draw(w);
    return TRUE;
}

static gboolean on_button_release(GtkWidget *w, GdkEventButton *ev, App *app)
{
    if (ev->button != 1 || !app->dragging)
        return FALSE;
    app->dragging = FALSE;
    app->x1 = ev->x;
    app->y1 = ev->y;

    if (fabs(app->x1 - app->x0) < CLICK_SLOP && fabs(app->y1 - app->y0) < CLICK_SLOP) {
        select_at(app, ev->x, ev->y);
    } else {
        app->sel = drag_rect(app);
        app->have_sel = app->sel.width > 0 && app->sel.height > 0;
    }
    update_save_button(app);
    gtk_widget_queue_draw(w);
    return TRUE;
}

static gboolean on_motion(GtkWidget *w, GdkEventMotion *ev, App *app)
{
    if (app->dragging) {
        app->x1 = ev->x;
        app->y1 = ev->y;
    } else {
        app->have_hover = window_at(app, ev->x, ev->y, &app->hover);
    }
    gtk_widget_queue_draw(w);
    return TRUE;
}

static gboolean on_key_press(GtkWidget *w, GdkEventKey *ev, App *app)
{
    (void)w;
    guint key = gdk_keyval_to_lower(ev->keyval);
    gboolean ctrl = (ev->state & GDK_CONTROL_MASK) != 0;

    if (key == GDK_KEY_Escape)
        finish(app, 1);
    else if (key == GDK_KEY_Return || key == GDK_KEY_KP_Enter)
        accept_save_and_copy(app);
    else if (ctrl && key == GDK_KEY_c)
        accept_copy(app);
    else if (ctrl && key == GDK_KEY_s)
        accept_save_as(app);
    else
        return FALSE;
    return TRUE;
}

/* Janelas popup não recebem foco: o grab garante teclado e mouse.
 * Outro programa pode estar segurando o grab por um instante; tenta de novo. */
static gboolean try_grab(gpointer data)
{
    App *app = data;
    GdkWindow *gw = gtk_widget_get_window(app->win);
    GdkDisplay *display = gdk_window_get_display(gw);
    GdkCursor *cursor = gdk_cursor_new_from_name(display, "crosshair");
    GdkGrabStatus st = gdk_seat_grab(gdk_display_get_default_seat(display), gw,
                                     GDK_SEAT_CAPABILITY_ALL, TRUE, cursor, NULL, NULL, NULL);
    g_clear_object(&cursor);
    if (st == GDK_GRAB_SUCCESS) {
        app->grab_timer = 0;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static gboolean on_map(GtkWidget *w, GdkEvent *ev, App *app)
{
    (void)ev;
    if (try_grab(app) == G_SOURCE_CONTINUE && !app->grab_timer)
        app->grab_timer = g_timeout_add(50, try_grab, app);

    /* Destaca de imediato a janela que já está sob o cursor. */
    int x, y;
    GdkDevice *pointer = gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(w)));
    gdk_device_get_position(pointer, NULL, &x, &y);
    app->have_hover = window_at(app, x, y, &app->hover);
    update_save_button(app);
    gtk_widget_queue_draw(w);
    return FALSE;
}

/* Põe o botão Salvar logo abaixo do canto inferior direito da seleção;
 * sem espaço embaixo, ele fica por dentro, no mesmo canto. */
static gboolean on_child_position(GtkOverlay *overlay, GtkWidget *child, GdkRectangle *pos, App *app)
{
    (void)overlay;
    const int gap = 8;
    GtkRequisition req;
    gtk_widget_get_preferred_size(child, NULL, &req);
    GdkRectangle *s = &app->sel;

    pos->width = req.width;
    pos->height = req.height;
    pos->x = CLAMP(s->x + s->width - req.width, 0, MAX(app->width - req.width, 0));
    pos->y = s->y + s->height + gap;
    if (pos->y + req.height > app->height)
        pos->y = MAX(s->y + s->height - req.height - gap, 0);
    return TRUE;
}

static void show_overlay(App *app)
{
    app->win = gtk_window_new(GTK_WINDOW_POPUP); /* sem decoração, cobre todos os monitores */
    gtk_window_move(GTK_WINDOW(app->win), 0, 0);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(app->win), overlay);
    g_signal_connect(overlay, "get-child-position", G_CALLBACK(on_child_position), app);

    GtkWidget *area = gtk_drawing_area_new();
    gtk_widget_set_size_request(area, app->width, app->height);
    gtk_widget_add_events(area, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    gtk_container_add(GTK_CONTAINER(overlay), area);

    app->save_btn = gtk_button_new_with_label("Salvar");
    gtk_button_set_image(GTK_BUTTON(app->save_btn),
                         gtk_image_new_from_icon_name("document-save-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image(GTK_BUTTON(app->save_btn), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->save_btn), "suggested-action");
    gtk_widget_set_tooltip_text(app->save_btn, "Salvar em Imagens e copiar (Enter)");
    gtk_widget_set_no_show_all(app->save_btn, TRUE); /* visibilidade controlada pela seleção */
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), app->save_btn);
    g_signal_connect(app->save_btn, "clicked", G_CALLBACK(on_save_clicked), app);

    g_signal_connect(area, "draw", G_CALLBACK(on_draw), app);
    g_signal_connect(area, "button-press-event", G_CALLBACK(on_button_press), app);
    g_signal_connect(area, "button-release-event", G_CALLBACK(on_button_release), app);
    g_signal_connect(area, "motion-notify-event", G_CALLBACK(on_motion), app);
    g_signal_connect(app->win, "key-press-event", G_CALLBACK(on_key_press), app);
    g_signal_connect(app->win, "map-event", G_CALLBACK(on_map), app);

    gtk_widget_show_all(app->win);
}

static gboolean start(gpointer data)
{
    App *app = data;
    if (!capture_screen(app)) {
        g_printerr("printa: falha ao capturar a tela\n");
        finish(app, 1);
    } else {
        show_overlay(app);
    }
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv)
{
    int delay = 0;
    GOptionEntry entries[] = {
        {"delay", 'd', 0, G_OPTION_ARG_INT, &delay, "Espera N segundos antes de capturar", "N"},
        {NULL},
    };

    /* Capturar a tela exige X11; no Wayland a imagem viria vazia. */
    gdk_set_allowed_backends("x11");
    if (g_strcmp0(g_getenv("XDG_SESSION_TYPE"), "wayland") == 0)
        g_printerr("printa: aviso: sessão Wayland — a captura provavelmente sairá em branco\n");

    GError *err = NULL;
    if (!gtk_init_with_args(&argc, &argv, "- captura de tela", entries, NULL, &err)) {
        g_printerr("printa: %s\n", err ? err->message : "não foi possível abrir o display X11");
        g_clear_error(&err);
        return 1;
    }

    App app = {.windows = g_array_new(FALSE, FALSE, sizeof(GdkRectangle)), .status = 1};
    g_timeout_add_seconds(MAX(delay, 0), start, &app);
    gtk_main();

    if (app.grab_timer)
        g_source_remove(app.grab_timer);
    g_clear_object(&app.shot);
    g_array_free(app.windows, TRUE);
    return app.status;
}
