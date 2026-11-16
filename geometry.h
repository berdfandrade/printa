/* Geometria da seleção: funções puras, sem depender de display. */
#ifndef PRINTA_GEOMETRY_H
#define PRINTA_GEOMETRY_H

#include <gdk/gdk.h>

/* Movimento máximo (px) para um arrasto ainda contar como clique. */
#define CLICK_SLOP 4

/* Retângulo coberto por um arrasto de (x0, y0) a (x1, y1), em qualquer
 * direção, arredondado para fora e recortado à tela. Pode sair vazio. */
GdkRectangle rect_from_drag(double x0, double y0, double x1, double y1, int screen_w, int screen_h);

/* Um arrasto tão curto que deve ser tratado como clique. */
gboolean is_click(double x0, double y0, double x1, double y1);

/* A janela mais ao topo que contém o ponto. `windows` é um GArray de
 * GdkRectangle em ordem de empilhamento, a do topo por último. */
gboolean window_at(const GArray *windows, double x, double y, GdkRectangle *out);

/* Desconta a sombra invisível de janelas CSD. `ext` é o _GTK_FRAME_EXTENTS
 * (esquerda, direita, cima, baixo) em pixels do dispositivo. */
GdkRectangle without_csd_shadow(GdkRectangle frame, const long ext[4], int scale);

/* Onde pôr o botão Salvar: abaixo do canto inferior direito da seleção, ou
 * dentro dela quando não há espaço embaixo, sempre dentro da tela. */
GdkRectangle save_button_position(const GdkRectangle *sel, int btn_w, int btn_h, int screen_w, int screen_h);

#endif
