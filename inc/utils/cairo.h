/*
 * utils/cairo.h - Cairo utilities
 *
 * Copyright (C) 2019 Alexandros Theodotou
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * \file
 *
 * Cairo utilities.
 */

#ifndef __UTILS_CAIRO_H__
#define __UTILS_CAIRO_H__

#include <gtk/gtk.h>

/**
 * @addtogroup utils
 *
 * @{
 */

/**
 * Default font for drawing pango text.
 */
#define Z_CAIRO_FONT "Sans Bold 9"

/**
 * Padding to leave from the top/left edges when
 * drawing text.
 */
#define Z_CAIRO_TEXT_PADDING 2

void
z_cairo_draw_selection (cairo_t * cr,
                        double    start_x,
                        double    start_y,
                        double    offset_x,
                        double    offset_y);

void
z_cairo_draw_horizontal_line (cairo_t * cr,
                              double    y,
                              double    from_x,
                              double    to_x,
                              double    alpha);

void
z_cairo_draw_vertical_line (cairo_t * cr,
                            double    x,
                            double    from_y,
                            double    to_y);

/**
 * @param aspect Aspect ratio.
 * @param corner_radius Corner curvature radius.
 */
static inline void
z_cairo_rounded_rectangle (
  cairo_t * cr,
  double    x,
  double    y,
  double    width,
  double    height,
  double    aspect,
  double    corner_radius)
{
  double radius = corner_radius / aspect;
  double degrees = G_PI / 180.0;

  cairo_new_sub_path (cr);
  cairo_arc (cr, x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
  cairo_arc (cr, x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
  cairo_arc (cr, x + radius, y + height - radius, radius, 90 * degrees, 180 * degrees);
  cairo_arc (cr, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
  cairo_close_path (cr);
}

/**
 * Gets the width of the given text in pixels
 * for the given widget when z_cairo_draw_text()
 * is used.
 *
 * @param widget The widget to derive a PangoLayout
 *   from.
 * @param text The text to draw.
 * @param width The width to fill in.
 * @param height The height to fill in.
 */
void
z_cairo_get_text_extents_for_widget (
  GtkWidget *  widget,
  const char * text,
  int *        width,
  int *        height);

static inline void
z_cairo_draw_text (
  cairo_t *    cr,
  const char * text)
{

  PangoLayout *layout;
  PangoFontDescription *desc;

  cairo_translate (
    cr, Z_CAIRO_TEXT_PADDING, Z_CAIRO_TEXT_PADDING);

  /* Create a PangoLayout, set the font and text */
  layout = pango_cairo_create_layout (cr);

  pango_layout_set_markup (layout, text, -1);
  desc =
    pango_font_description_from_string (
      Z_CAIRO_FONT);
  pango_layout_set_font_description (layout, desc);
  pango_font_description_free (desc);

  /* Inform Pango to re-layout the text with the new transformation */
  /*pango_cairo_update_layout (cr, layout);*/

  /*pango_layout_get_size (layout, &width, &height);*/
  /*cairo_move_to (cr, - ((double)width / PANGO_SCALA) / 2, - RADIUS);*/
  pango_cairo_show_layout (cr, layout);

  /* free the layout object */
  g_object_unref (layout);
}

/**
 * Draws a diamond shape.
 */
static inline void
z_cairo_diamond (
  cairo_t * cr,
  double    x,
  double    y,
  double    width,
  double    height)
{
  cairo_move_to (cr, x, height / 2);
  cairo_line_to (cr, width / 2, y);
  cairo_line_to (cr, width, height / 2);
  cairo_line_to (cr, width / 2, height);
  cairo_line_to (cr, x, height / 2);
  cairo_close_path (cr);
}

#endif
