/* gcal-week-grid.c
 *
 * Copyright (C) 2016 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *                    Vamsi Krishna Gollapudi <pandu.sonu@yahoo.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "GcalWeekGrid"

#include "gcal-week-grid.h"
#include "gcal-week-view.h"
#include "gcal-utils.h"
#include "gcal-view.h"
#include "gcal-event-widget.h"
#include "gcal-range-tree.h"

#include <glib/gi18n.h>
#include <string.h>
#include <math.h>

static const double dashed [] =
{
  5.0,
  6.0
};

typedef struct
{
  GtkWidget          *widget;
  guint16             start;
  guint16             end;
} ChildData;

struct _GcalWeekGrid
{
  GtkContainer        parent;

  GtkWidget          *hours_sidebar;

  GdkWindow          *event_window;

  gint                first_weekday;
  gboolean            use_24h_format : 1;

  icaltimetype       *active_date;
  icaltimetype       *current_date;

  GcalRangeTree      *events;

  gboolean            children_changed;
  gint                redraw_timeout_id;

  /*
   * These fields are "cells" rather than minutes. Each cell
   * correspond to 30 minutes.
   */
  gint                selection_start;
  gint                selection_end;
  gint                dnd_cell;

  GcalManager        *manager;
};

G_DEFINE_TYPE (GcalWeekGrid, gcal_week_grid, GTK_TYPE_CONTAINER);

enum
{
  PROP_0,
  PROP_ACTIVE_DATE,
  LAST_PROP
};

enum
{
  EVENT_ACTIVATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };
static GParamSpec* properties[LAST_PROP] = { NULL, };

/* ChildData methods */
static ChildData*
child_data_new (GtkWidget *widget,
                guint16    start,
                guint16    end)
{
  ChildData *data;

  data = g_new (ChildData, 1);
  data->widget = widget;
  data->start = start;
  data->end = end;

  return data;
}

/* Event activation methods */
static void
on_event_widget_activated (GcalEventWidget *widget,
                           GcalWeekGrid    *self)
{
  g_signal_emit (self, signals[EVENT_ACTIVATED], 0, widget);
}


static inline void
setup_event_widget (GcalWeekGrid *self,
                    GtkWidget    *widget)
{
  g_signal_connect (widget, "activate", G_CALLBACK (on_event_widget_activated), self);
}

static inline void
destroy_event_widget (GcalWeekGrid *self,
                      GtkWidget    *widget)
{
  g_signal_handlers_disconnect_by_func (widget, on_event_widget_activated, self);
  gtk_widget_destroy (widget);
}

/* Auxiliary methods */
static void
get_event_range (GcalWeekGrid *self,
                 GcalEvent    *event,
                 guint16      *start,
                 guint16      *end)
{
  GDateTime *week_start;
  GTimeSpan diff;
  gboolean week_start_dst;

  if (!self->active_date)
    return;

  week_start = get_start_of_week (self->active_date);
  week_start_dst = g_date_time_is_daylight_savings (week_start);

  if (start)
    {
      GDateTime *event_start;
      gboolean event_start_dst;

      event_start = g_date_time_to_local (gcal_event_get_date_start (event));
      event_start_dst = g_date_time_is_daylight_savings (event_start);

      diff = g_date_time_difference (event_start, week_start);

      *start = CLAMP (diff / G_TIME_SPAN_MINUTE, 0, MAX_MINUTES);
      *start += 60 * (event_start_dst - week_start_dst);

      g_clear_pointer (&event_start, g_date_time_unref);
    }

  if (end)
    {

      GDateTime *event_end;
      gboolean event_end_dst;

      event_end = g_date_time_to_local (gcal_event_get_date_end (event));
      event_end_dst = g_date_time_is_daylight_savings (event_end);

      diff = g_date_time_difference (event_end, week_start);

      *end = CLAMP (diff / G_TIME_SPAN_MINUTE, 0, MAX_MINUTES);
      *end += 60 * (event_end_dst - week_start_dst);

      /*
       * XXX: it may happen that the event has the same start and end
       * dates. For this case, just enforce that the event is at least
       * 1 minute long.
       */
      if (start && *start == *end)
        *end = *end + 1;

      g_clear_pointer (&event_end, g_date_time_unref);
    }

  g_clear_pointer (&week_start, g_date_time_unref);
}

static inline gint
int16_compare (gconstpointer a,
               gconstpointer b)
{
  return GPOINTER_TO_INT (a) - GPOINTER_TO_INT (b);
}

static inline guint
get_event_index (GcalRangeTree *tree,
                 guint16        start,
                 guint16        end)
{
  g_autoptr (GPtrArray) array;
  gint idx, i;

  i = idx = 0;
  array = gcal_range_tree_get_data_at_range (tree, start, end);

  if (!array)
    return 0;

  g_ptr_array_sort (array, int16_compare);

  for (i = 0; array && i < array->len; i++)
    {
      if (idx == GPOINTER_TO_INT (g_ptr_array_index (array, i)))
        idx++;
      else
        break;
    }

  return idx;
}

static guint
count_overlaps_at_range (GcalRangeTree *self,
                         guint16        start,
                         guint16        end)
{
  guint64 i, counter;

  g_return_val_if_fail (self, 0);
  g_return_val_if_fail (end >= start, 0);

  counter = 0;

  for (i = start; i < end; i++)
    {
      guint n_events;

      n_events = gcal_range_tree_count_entries_at_range (self, i, i + 1);

      counter = MAX (counter, n_events);
    }

  return counter;
}

static void
gcal_week_grid_finalize (GObject *object)
{
  GcalWeekGrid *self = GCAL_WEEK_GRID (object);

  g_clear_pointer (&self->events, gcal_range_tree_unref);
  g_clear_pointer (&self->active_date, g_free);
  g_clear_pointer (&self->current_date, g_free);

  if (self->redraw_timeout_id > 0)
    {
      g_source_remove (self->redraw_timeout_id);
      self->redraw_timeout_id = 0;
    }

  G_OBJECT_CLASS (gcal_week_grid_parent_class)->finalize (object);
}

static void
gcal_week_grid_add (GtkContainer *container,
                    GtkWidget    *widget)
{
  if (!gtk_widget_get_parent (widget))
    gtk_widget_set_parent (widget, GTK_WIDGET (container));
}

static void
gcal_week_grid_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
}

static void
gcal_week_grid_forall (GtkContainer *container,
                       gboolean      include_internals,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
  GcalWeekGrid *self;
  GPtrArray *widgets_data;
  guint i;

  self = GCAL_WEEK_GRID (container);
  widgets_data = gcal_range_tree_get_data_at_range (self->events, 0, MAX_MINUTES);

  for (i = 0; widgets_data && i < widgets_data->len; i++)
    {
      ChildData *data;

      data = g_ptr_array_index (widgets_data, i);

      callback (data->widget, callback_data);
    }

  g_clear_pointer (&widgets_data, g_ptr_array_unref);
}

static void
gcal_week_grid_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GcalWeekGrid *self = GCAL_WEEK_GRID (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_DATE:
      g_value_set_boxed (value, self->active_date);
      return;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_week_grid_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GcalWeekGrid *self = GCAL_WEEK_GRID (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_DATE:
      g_clear_pointer (&self->active_date, g_free);
      self->active_date = g_value_dup_boxed (value);

      gtk_widget_queue_resize (GTK_WIDGET (self));
      gtk_widget_queue_draw (GTK_WIDGET (self));
      return;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gcal_week_grid_realize (GtkWidget *widget)
{
  GcalWeekGrid *self;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  self = GCAL_WEEK_GRID (widget);
  parent_window = gtk_widget_get_parent_window (widget);

  gtk_widget_set_realized (widget, TRUE);
  gtk_widget_set_window (widget, parent_window);
  g_object_ref (parent_window);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_BUTTON1_MOTION_MASK |
                            GDK_POINTER_MOTION_HINT_MASK |
                            GDK_POINTER_MOTION_MASK |
                            GDK_ENTER_NOTIFY_MASK |
                            GDK_LEAVE_NOTIFY_MASK |
                            GDK_SCROLL_MASK |
                            GDK_SMOOTH_SCROLL_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  self->event_window = gdk_window_new (parent_window,
                                       &attributes,
                                       attributes_mask);
  gtk_widget_register_window (widget, self->event_window);
}

static void
gcal_week_grid_unrealize (GtkWidget *widget)
{
  GcalWeekGrid *self;

  self = GCAL_WEEK_GRID (widget);

  if (self->event_window)
    {
      gtk_widget_unregister_window (widget, self->event_window);
      gdk_window_destroy (self->event_window);
      self->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gcal_week_grid_parent_class)->unrealize (widget);
}

static void
gcal_week_grid_map (GtkWidget *widget)
{
  GcalWeekGrid *self;

  self = GCAL_WEEK_GRID (widget);

  if (self->event_window)
    gdk_window_show (self->event_window);

  GTK_WIDGET_CLASS (gcal_week_grid_parent_class)->map (widget);
}

static void
gcal_week_grid_unmap (GtkWidget *widget)
{
  GcalWeekGrid *self;

  self = GCAL_WEEK_GRID (widget);

  if (self->event_window)
    gdk_window_hide (self->event_window);

  GTK_WIDGET_CLASS (gcal_week_grid_parent_class)->unmap (widget);
}

static inline gint
get_today_column (GcalWeekGrid *self)
{
  g_autoptr(GDateTime) today, week_start;
  gint days_diff;

  today = g_date_time_new_now_local ();
  week_start = get_start_of_week (self->active_date);
  days_diff = g_date_time_difference (today, week_start) / G_TIME_SPAN_DAY;

  /* Today is out of range */
  if (g_date_time_compare (today, week_start) < 0 || days_diff > 7)
    return -1;

  return days_diff;
}

static gboolean
on_redraw_timeout_cb (gpointer data)
{
  gtk_widget_queue_draw (data);

  return G_SOURCE_CONTINUE;
}

static gboolean
gcal_week_grid_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GcalWeekGrid *self;
  GtkBorder padding;
  GdkRGBA color;

  gboolean ltr;
  gdouble minutes_height;
  gdouble x, column_width;
  gint i, width, height, today_column;

  self = GCAL_WEEK_GRID (widget);
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "lines");
  gtk_style_context_get_color (context, state, &color);
  gtk_style_context_get_padding (context, state, &padding);

  gdk_cairo_set_source_rgba (cr, &color);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  column_width = width / 7.0;
  minutes_height = (gdouble) height / MINUTES_PER_DAY;

  cairo_set_line_width (cr, 0.65);

  /* First, draw the selection */
  if (self->selection_start != -1 && self->selection_end != -1)
    {
      gint selection_height;
      gint column;
      gint start;
      gint end;

      start = self->selection_start;
      end = self->selection_end;

      /* Swap cells if needed */
      if (start > end)
        {
          start = start + end;
          end = start - end;
          start = start - end;
        }

      column = start * 30 / MINUTES_PER_DAY;
      selection_height = (end - start + 1) * 30 * minutes_height;

      x = column * column_width;

      gtk_style_context_save (context);
      gtk_style_context_set_state (context, state | GTK_STATE_FLAG_SELECTED);

      gtk_render_background (context,
                             cr,
                             ALIGNED (x),
                             round ((start * 30 % MINUTES_PER_DAY) * minutes_height),
                             column_width,
                             selection_height);

      gtk_style_context_restore (context);
    }

  /* Drag and Drop highlight */
  if (self->dnd_cell != -1)
    {
      gdouble cell_height;
      gint column, row;

      cell_height = minutes_height * 30;
      column = self->dnd_cell / (MINUTES_PER_DAY / 30);
      row = self->dnd_cell - column * 48;

      gtk_render_background (context,
                             cr,
                             column * column_width,
                             row * cell_height,
                             column_width,
                             cell_height);
    }

  /* Today column */
  today_column = get_today_column (GCAL_WEEK_GRID (widget));

  if (today_column != -1)
    {
      g_autoptr (GDateTime) now;
      GtkBorder margin;
      gdouble strip_width;
      guint minutes_from_midnight;
      gint min_stip_height;

      now = g_date_time_new_now_local ();
      minutes_from_midnight = g_date_time_get_hour (now) * 60 + g_date_time_get_minute (now);

      gtk_style_context_save (context);
      gtk_style_context_add_class (context, "now-strip");

      gtk_style_context_get (context, state, "min-height", &min_stip_height, NULL);
      gtk_style_context_get_margin (context, state, &margin);

      strip_width = column_width - margin.left - margin.right;

      if (ltr)
        x = today_column * column_width + margin.left;
      else
        x = width - (today_column * column_width + margin.right) - strip_width;

      gtk_render_background (context,
                             cr,
                             x,
                             round (minutes_from_midnight * ((gdouble) height / MINUTES_PER_DAY) + margin.top),
                             strip_width,
                             MAX (1, min_stip_height - margin.top - margin.bottom));

      gtk_style_context_restore (context);
    }

  /* Vertical lines */
  for (i = 0; i < 7; i++)
    {
      if (ltr)
        x = column_width * i;
      else
        x = width - column_width * i;

      cairo_move_to (cr, ALIGNED (x), 0);
      cairo_rel_line_to (cr, 0, height);
    }

  /* Horizontal lines */
  for (i = 1; i < 24; i++)
    {
      cairo_move_to (cr, 0, ALIGNED ((height / 24.0) * i));
      cairo_rel_line_to (cr, width, 0);
    }

  cairo_stroke (cr);

  /* Dashed lines between the vertical lines */
  cairo_set_dash (cr, dashed, 2, 0);

  for (i = 0; i < 24; i++)
    {
      cairo_move_to (cr, 0, ALIGNED ((height / 24.0) * i + (height / 48.0)));
      cairo_rel_line_to (cr, width, 0);
    }

  cairo_stroke (cr);

  gtk_style_context_restore (context);

  GTK_WIDGET_CLASS (gcal_week_grid_parent_class)->draw (widget, cr);

  /* Fire the redraw timeout if needed */
  if (self->redraw_timeout_id == 0)
    {
      self->redraw_timeout_id = g_timeout_add_seconds (5,
                                                       on_redraw_timeout_cb,
                                                       self);
    }

  return FALSE;
}

static void
gcal_week_grid_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GcalWeekGrid *self = GCAL_WEEK_GRID (widget);
  GcalRangeTree *overlaps;
  gboolean ltr;
  gdouble minutes_height;
  gdouble column_width;
  guint i;
  guint x;

  /* No need to relayout stuff if nothing changed */
  if (allocation->height == gtk_widget_get_allocated_height (widget) &&
      allocation->width == gtk_widget_get_allocated_width (widget) &&
      !self->children_changed)
    {
      return;
    }

  /* Allocate the widget */
  gtk_widget_set_allocation (widget, allocation);

  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (self->event_window,
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }


  /* Preliminary calculations */
  minutes_height = (gdouble) allocation->height / MINUTES_PER_DAY;
  column_width = (gdouble) allocation->width / 7.0;

  /* Temporary range tree to hold positioned events' indexes */
  overlaps = gcal_range_tree_new ();

  /*
   * Iterate through weekdays; we don't have to worry about events that
   * jump between days because they're already handled by GcalWeekHeader.
   */
  for (i = 0; i < 7; i++)
    {
      GPtrArray *widgets_data;
      guint16 day_start;
      guint16 day_end;
      guint j;

      day_start = i * MINUTES_PER_DAY;
      day_end = day_start + MINUTES_PER_DAY;
      widgets_data = gcal_range_tree_get_data_at_range (self->events, day_start, day_end);

      for (j = 0; widgets_data && j < widgets_data->len; j++)
        {
          GtkStyleContext *context;
          GtkAllocation child_allocation;
          GtkWidget *event_widget;
          ChildData *data;
          GtkBorder margin;
          guint64 events_at_range;
          gint natural_height;
          gint widget_index;
          gint offset;
          gint height;
          gint width;

          data =  g_ptr_array_index (widgets_data, j);
          event_widget = data->widget;
          context = gtk_widget_get_style_context (event_widget);

          /* The total number of events available in this range */
          events_at_range = count_overlaps_at_range (self->events, data->start, data->end);

          /* The real horizontal position of this event */
          widget_index = get_event_index (overlaps, data->start, data->end);

          /* Gtk complains about that */
          gtk_widget_get_preferred_height (event_widget, NULL, &natural_height);

          /* Consider the margins of the child */
          gtk_style_context_get_margin (context,
                                        gtk_style_context_get_state (context),
                                        &margin);

          width = column_width / events_at_range - margin.left - margin.right;
          height = MAX ((data->end - data->start) * minutes_height - margin.top - margin.bottom, natural_height);
          offset = (width + margin.left + margin.right) * widget_index;

          if (ltr)
            x = column_width * i + offset + allocation->x + margin.left + 1;
          else
            x = allocation->width - width - (column_width * i + offset + allocation->x + margin.left + 1);

          /* Setup the child position and size */
          child_allocation.x = x;
          child_allocation.y = (data->start % MINUTES_PER_DAY) * minutes_height + margin.top;
          child_allocation.width = width;
          child_allocation.height = height;

          gtk_widget_size_allocate (event_widget, &child_allocation);

          /*
           * Add the current event to the temporary overlaps tree so we have a way to
           * know how many events are already positioned in the current column.
           */
          gcal_range_tree_add_range (overlaps,
                                     data->start,
                                     data->end,
                                     GINT_TO_POINTER (widget_index));
        }

      g_clear_pointer (&widgets_data, g_ptr_array_unref);
    }

  self->children_changed = FALSE;

  g_clear_pointer (&overlaps, gcal_range_tree_unref);
}

static void
gcal_week_grid_get_preferred_height (GtkWidget *widget,
                                     gint      *minimum_height,
                                     gint      *natural_height)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;

  PangoLayout *layout;
  PangoFontDescription *font_desc;

  gint hours_12_height, hours_24_height, cell_height, height;

  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "hours");

  gtk_style_context_get (context, state,
                         "font", &font_desc,
                         NULL);
  gtk_style_context_get_padding (context, state, &padding);

  layout = pango_layout_new (gtk_widget_get_pango_context (widget));
  pango_layout_set_font_description (layout, font_desc);

  pango_layout_set_text (layout, _("00 AM"), -1);
  pango_layout_get_pixel_size (layout, NULL, &hours_12_height);

  pango_layout_set_text (layout, _("00:00"), -1);
  pango_layout_get_pixel_size (layout, NULL, &hours_24_height);

  cell_height = MAX (hours_12_height, hours_24_height) + padding.top + padding.bottom;
  height = cell_height * 48;

  gtk_style_context_restore (context);

  pango_font_description_free (font_desc);
  g_object_unref (layout);

  /* Report the height */
  if (minimum_height)
    *minimum_height = height;

  if (natural_height)
    *natural_height = height;
}

static gboolean
gcal_week_grid_button_press (GtkWidget      *widget,
                             GdkEventButton *event_button)
{
  GcalWeekGrid *self;
  GtkAllocation alloc;
  gdouble minute_height;
  gint column_width;
  gint column;
  gint minute;

  self = GCAL_WEEK_GRID (widget);

  gtk_widget_get_allocation (widget, &alloc);
  minute_height = (gdouble) alloc.height / MINUTES_PER_DAY;
  column_width = floor (alloc.width / 7);
  column = (gint) event_button->x / column_width;
  minute = event_button->y / minute_height;
  minute = minute - (minute % 30);

  self->selection_start = (column * MINUTES_PER_DAY + minute) / 30;
  self->selection_end = self->selection_start;

  gtk_widget_queue_draw (widget);

  return GDK_EVENT_STOP;
}

static gboolean
gcal_week_grid_motion_notify_event (GtkWidget      *widget,
                                    GdkEventMotion *event)
{
  GcalWeekGrid *self;
  GtkAllocation alloc;
  gdouble minute_height;
  gint column;
  gint minute;

  if (!(event->state & GDK_BUTTON_PRESS_MASK))
    return GDK_EVENT_PROPAGATE;

  self = GCAL_WEEK_GRID (widget);

  gtk_widget_get_allocation (widget, &alloc);
  minute_height = (gdouble) alloc.height / MINUTES_PER_DAY;
  column = self->selection_start * 30 / MINUTES_PER_DAY;
  minute = event->y / minute_height;
  minute = minute - (minute % 30);

  self->selection_end = (column * MINUTES_PER_DAY + minute) / 30;

  gtk_widget_queue_draw (widget);

  return GDK_EVENT_STOP;
}

static gboolean
gcal_week_grid_button_release (GtkWidget      *widget,
                               GdkEventButton *event)
{
  GcalWeekGrid *self;
  GtkAllocation alloc;
  GDateTime *week_start;
  GDateTime *start, *end;
  GtkWidget *weekview;
  gboolean ltr;
  gdouble minute_height;
  gdouble x, y;
  gint column;
  gint minute;
  gint start_cell;
  gint end_cell;
  gint out_x;
  gint out_y;

  self = GCAL_WEEK_GRID (widget);
  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

  gtk_widget_get_allocation (widget, &alloc);
  minute_height = (gdouble) alloc.height / MINUTES_PER_DAY;
  column = self->selection_start * 30 / MINUTES_PER_DAY;
  minute = event->y / minute_height;
  minute = minute - (minute % 30);

  self->selection_end = (column * MINUTES_PER_DAY + minute) / 30;

  start_cell = self->selection_start;
  end_cell = self->selection_end;

  if (start_cell > end_cell)
    {
      start_cell = start_cell + end_cell;
      end_cell = start_cell - end_cell;
      start_cell = start_cell - end_cell;
    }

  gtk_widget_queue_draw (widget);

  /* Fake the week view's event so we can control the X and Y values */
  weekview = gtk_widget_get_ancestor (widget, GCAL_TYPE_WEEK_VIEW);
  week_start = get_start_of_week (self->active_date);

  if (ltr)
    {
      start = g_date_time_add_minutes (week_start, start_cell * 30);
      end = g_date_time_add_minutes (week_start, (end_cell + 1) * 30);
    }
  else
    {
      start = g_date_time_add_minutes (week_start, MAX_MINUTES - column * MINUTES_PER_DAY + minute);
      end = g_date_time_add_minutes (week_start, MAX_MINUTES - column * MINUTES_PER_DAY + (end_cell + 1) * 30);
    }

  x = round ((column + 0.5) * (alloc.width / 7.0));
  y = (minute + 15) * minute_height;

  gtk_widget_translate_coordinates (widget,
                                    weekview,
                                    x,
                                    y,
                                    &out_x,
                                    &out_y);

  g_signal_emit_by_name (weekview,
                         "create-event",
                         start,
                         end,
                         (gdouble) out_x,
                         (gdouble) out_y);

  gcal_clear_datetime (&week_start);
  gcal_clear_datetime (&start);
  gcal_clear_datetime (&end);

  return GDK_EVENT_STOP;
}

static gint
get_dnd_cell (GtkWidget *widget,
              gint       x,
              gint       y)
{
  GtkAllocation alloc;
  gdouble column_width, cell_height;
  gint column, row;

  gtk_widget_get_allocation (widget, &alloc);

  column_width = alloc.width / 7.0;
  cell_height = alloc.height / 48.0;
  column = floor (x / column_width);
  row = y / cell_height;

  return column * 48 + row;
}

static gboolean
gcal_week_grid_drag_motion (GtkWidget      *widget,
                            GdkDragContext *context,
                            gint            x,
                            gint            y,
                            guint           time)
{
  GcalWeekGrid *self;

  self = GCAL_WEEK_GRID (widget);
  self->dnd_cell = get_dnd_cell (widget, x, y);

  /* Setup the drag highlight */
  if (self->dnd_cell != -1)
    gtk_drag_highlight (widget);
  else
    gtk_drag_unhighlight (widget);

  /*
   * Sets the status of the drag - if it fails, sets the action to 0 and
   * aborts the drag with FALSE.
   */
  gdk_drag_status (context,
                   self->dnd_cell == -1 ? 0 : GDK_ACTION_COPY,
                   time);

  gtk_widget_queue_draw (widget);

  return self->dnd_cell != -1;
}

static gboolean
gcal_week_grid_drag_drop (GtkWidget      *widget,
                          GdkDragContext *context,
                          gint            x,
                          gint            y,
                          guint           time)
{
  GcalWeekGrid *self;
  g_autoptr (GDateTime) week_start;
  g_autoptr (GDateTime) dnd_date;
  GtkWidget *event_widget;
  GcalEvent *event;
  GTimeSpan timespan = 0;
  gboolean ltr;
  gint drop_cell;

  self = GCAL_WEEK_GRID (widget);
  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;
  drop_cell = get_dnd_cell (widget, x, y);
  event_widget = gtk_drag_get_source_widget (context);

  week_start = dnd_date = NULL;

  if (!GCAL_IS_EVENT_WIDGET (event_widget))
    return FALSE;

  /* RTL languages swap the drop cell column */
  if (!ltr)
    {
      gint column, row;

      column = drop_cell / (MINUTES_PER_DAY / 30);
      row = drop_cell - column * 48;

      drop_cell = (6 - column) * 48 + row;
    }

  event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (event_widget));
  week_start = get_start_of_week (self->active_date);
  dnd_date = g_date_time_add_minutes (week_start, drop_cell * 30);

  /*
   * Calculate the diff between the dropped cell and the event's start date,
   * so we can update the end date accordingly.
   */
  if (gcal_event_get_date_end (event))
    timespan = g_date_time_difference (gcal_event_get_date_end (event), gcal_event_get_date_start (event));

  /*
   * Set the event's start and end dates. Since the event may have a
   * NULL end date, so we have to check it here
   */
  gcal_event_set_all_day (event, FALSE);
  gcal_event_set_date_start (event, dnd_date);

  if (gcal_event_get_date_end (event))
    {
      g_autoptr (GDateTime) new_end = g_date_time_add (dnd_date, timespan);

      gcal_event_set_date_end (event, new_end);
    }

  /* Commit the changes */
  gcal_manager_update_event (self->manager, event);

  /* Cancel the DnD */
  self->dnd_cell = -1;
  gtk_drag_unhighlight (widget);

  gtk_drag_finish (context, TRUE, FALSE, time);

  gtk_widget_queue_draw (widget);

  return TRUE;
}

static void
gcal_week_grid_drag_leave (GtkWidget      *widget,
                           GdkDragContext *context,
                           guint           time)
{
  GcalWeekGrid *self = GCAL_WEEK_GRID (widget);

  /* Cancel the drag */
  self->dnd_cell = -1;
  gtk_drag_unhighlight (widget);

  gtk_widget_queue_draw (widget);
}

static void
gcal_week_grid_class_init (GcalWeekGridClass *klass)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  container_class->add = gcal_week_grid_add;
  container_class->remove = gcal_week_grid_remove;
  container_class->forall = gcal_week_grid_forall;

  object_class->finalize = gcal_week_grid_finalize;
  object_class->get_property = gcal_week_grid_get_property;
  object_class->set_property = gcal_week_grid_set_property;

  widget_class->draw = gcal_week_grid_draw;
  widget_class->size_allocate = gcal_week_grid_size_allocate;
  widget_class->realize = gcal_week_grid_realize;
  widget_class->unrealize = gcal_week_grid_unrealize;
  widget_class->map = gcal_week_grid_map;
  widget_class->unmap = gcal_week_grid_unmap;
  widget_class->get_preferred_height = gcal_week_grid_get_preferred_height;
  widget_class->button_press_event = gcal_week_grid_button_press;
  widget_class->motion_notify_event = gcal_week_grid_motion_notify_event;
  widget_class->button_release_event = gcal_week_grid_button_release;
  widget_class->drag_motion = gcal_week_grid_drag_motion;
  widget_class->drag_leave = gcal_week_grid_drag_leave;
  widget_class->drag_drop = gcal_week_grid_drag_drop;

  properties[PROP_ACTIVE_DATE] = g_param_spec_boxed ("active-date",
                                                     "Date",
                                                     "The active selected date",
                                                     ICAL_TIME_TYPE,
                                                     G_PARAM_CONSTRUCT | G_PARAM_READWRITE);


  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated",
                                           GCAL_TYPE_WEEK_GRID,
                                           G_SIGNAL_RUN_FIRST,
                                           0,  NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           1,
                                           GCAL_TYPE_EVENT_WIDGET);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  gtk_widget_class_set_css_name (widget_class, "weekgrid");
}

static void
gcal_week_grid_init (GcalWeekGrid *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->selection_start = -1;
  self->selection_end = -1;
  self->dnd_cell = -1;

  self->events = gcal_range_tree_new ();

  /* Setup the week view as a drag n' drop destination */
  gtk_drag_dest_set (GTK_WIDGET (self),
                     0,
                     NULL,
                     0,
                     GDK_ACTION_MOVE);
}

/* Public API */
void
gcal_week_grid_set_manager (GcalWeekGrid *self,
                            GcalManager  *manager)
{
  g_return_if_fail (GCAL_IS_WEEK_GRID (self));

  self->manager = manager;
}

void
gcal_week_grid_set_first_weekday (GcalWeekGrid *self,
                                  gint          nr_day)
{
  g_return_if_fail (GCAL_IS_WEEK_GRID (self));

  self->first_weekday = nr_day;
}

void
gcal_week_grid_set_use_24h_format (GcalWeekGrid *self,
                                     gboolean    use_24h_format)
{
  g_return_if_fail (GCAL_IS_WEEK_GRID (self));

  self->use_24h_format = use_24h_format;
}

void
gcal_week_grid_set_current_date (GcalWeekGrid *self,
                                 icaltimetype *current_date)
{
  g_return_if_fail (GCAL_IS_WEEK_GRID (self));

  g_clear_pointer (&self->current_date, g_free);
  self->current_date = gcal_dup_icaltime (current_date);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
gcal_week_grid_add_event (GcalWeekGrid *self,
                          GcalEvent    *event)
{
  GtkWidget *widget;
  guint16 start, end;

  g_return_if_fail (GCAL_IS_WEEK_GRID (self));

  end = 0;
  start = 0;

  widget = g_object_new (GCAL_TYPE_EVENT_WIDGET,
                         "event", event,
                         "orientation", GTK_ORIENTATION_VERTICAL,
                         NULL);

  self->children_changed = TRUE;

  get_event_range (self, event, &start, &end);

  gcal_range_tree_add_range (self->events,
                             start,
                             end,
                             child_data_new (widget, start, end));

  setup_event_widget (self, widget);
  gtk_widget_show (widget);

  gtk_container_add (GTK_CONTAINER (self), widget);
}

void
gcal_week_grid_remove_event (GcalWeekGrid *self,
                             const gchar  *uid)
{
  GPtrArray *widgets;
  guint i;

  g_return_if_fail (GCAL_IS_WEEK_GRID (self));

  widgets = gcal_range_tree_get_data_at_range (self->events, 0, MAX_MINUTES);

  for (i = 0; widgets && i < widgets->len; i++)
    {
      ChildData *data;
      GcalEvent *event;
      guint16 event_start;
      guint16 event_end;

      data = g_ptr_array_index (widgets, i);
      event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (data->widget));

      if (g_strcmp0 (gcal_event_get_uid (event), uid) != 0)
        continue;

      self->children_changed = TRUE;

      get_event_range (self, event, &event_start, &event_end);

      gcal_range_tree_remove_range (self->events, data->start, data->end, data);
      destroy_event_widget (self, data->widget);
      gtk_widget_queue_allocate (GTK_WIDGET (self));
      g_free (data);
    }

  g_clear_pointer (&widgets, g_ptr_array_unref);
}

GList*
gcal_week_grid_get_children_by_uuid (GcalWeekGrid *self,
                                     const gchar  *uid)
{
  GPtrArray *widgets;
  GList *result;
  guint i;

  result = NULL;
  widgets = gcal_range_tree_get_data_at_range (self->events, 0, MAX_MINUTES);

  for (i = 0; widgets && i < widgets->len; i++)
    {
      ChildData *data;
      GcalEvent *event;

      data = g_ptr_array_index (widgets, i);
      event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (data->widget));

      if (g_strcmp0 (gcal_event_get_uid (event), uid) == 0)
        result = g_list_prepend (result, data->widget);

    }

  g_clear_pointer (&widgets, g_ptr_array_unref);

  return result;
}

void
gcal_week_grid_clear_marks (GcalWeekGrid *self)
{
  g_return_if_fail (GCAL_IS_WEEK_GRID (self));

  self->selection_start = -1;
  self->selection_end = -1;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}
