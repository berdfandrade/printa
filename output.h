/* Do recorte ao arquivo: funções puras, sem depender de display. */
#ifndef PRINTA_OUTPUT_H
#define PRINTA_OUTPUT_H

#include <gdk/gdk.h>

/* Cópia independente do trecho `r` (em coordenadas lógicas) da captura
 * `shot` (em pixels do dispositivo, `scale` vezes maior). O trecho é
 * recortado aos limites da captura; NULL se não sobrar nada. */
GdkPixbuf *crop_capture(GdkPixbuf *shot, const GdkRectangle *r, int scale);

/* Formato do gdk-pixbuf para a extensão do caminho: "jpeg" para .jpg/.jpeg
 * (sem diferenciar maiúsculas), "png" para todo o resto. */
const char *image_format_for(const char *path);

/* Nome padrão do arquivo, p.ex. "Captura_2026-11-15_20-34-47.png". */
char *capture_filename(GDateTime *when);

#endif
