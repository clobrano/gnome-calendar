/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-subscriber-view.c
 *
 * Copyright (C) 2014 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "GcalSubscriberView"

#include "gcal-subscriber-view.h"
#include "gcal-subscriber-view-private.h"

#include "gcal-event.h"
#include "gcal-event-widget.h"
#include "gcal-view.h"

enum
{
  EVENT_ACTIVATED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

static void          gcal_subscriber_view_clear_state            (GcalSubscriberView *self);

static guint         gcal_subscriber_view_get_child_cell         (GcalSubscriberView *self,
                                                                  GcalEventWidget    *child);

static void          gcal_data_model_subscriber_interface_init   (ECalDataModelSubscriberInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcalSubscriberView, gcal_subscriber_view, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GcalSubscriberView)
                         G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER,
                                                gcal_data_model_subscriber_interface_init));

static void
event_activated (GcalEventWidget *widget,
                 gpointer         user_data)
{
  /* FIXME: implement clear_state vfunc in descendants */
  gcal_subscriber_view_clear_state (GCAL_SUBSCRIBER_VIEW (user_data));
  g_signal_emit (GCAL_SUBSCRIBER_VIEW (user_data), signals[EVENT_ACTIVATED], 0, widget);
}

static void
event_visibility_changed (GtkWidget *widget,
                          gpointer   user_data)
{
  GcalSubscriberViewPrivate *priv = GCAL_SUBSCRIBER_VIEW (user_data)->priv;
  priv->children_changed = TRUE;
}

/* ECalDataModelSubscriber interface API */
static void
gcal_subscriber_view_component_added (ECalDataModelSubscriber *subscriber,
                                      ECalClient              *client,
                                      ECalComponent           *comp)
{
  GtkWidget *event_widget;
  GcalEvent *event;
  GError *error;

  error = NULL;
  event = gcal_event_new (e_client_get_source (E_CLIENT (client)), comp, &error);

  if (error)
    {
      g_message ("Error creating event: %s", error->message);
      g_clear_error (&error);
      return;
    }

  event_widget = gcal_event_widget_new (event);
  gcal_event_widget_set_read_only (GCAL_EVENT_WIDGET (event_widget), e_client_is_readonly (E_CLIENT (client)));

  gtk_widget_show (event_widget);
  gtk_container_add (GTK_CONTAINER (subscriber), event_widget);

  g_clear_object (&event);
}

static void
gcal_subscriber_view_component_modified (ECalDataModelSubscriber *subscriber,
                                         ECalClient              *client,
                                         ECalComponent           *comp)
{
  GcalSubscriberViewPrivate *priv;
  GList *l;
  GtkWidget *new_widget;
  GcalEvent *event;
  GError *error;

  error = NULL;
  priv = gcal_subscriber_view_get_instance_private (GCAL_SUBSCRIBER_VIEW (subscriber));
  event = gcal_event_new (e_client_get_source (E_CLIENT (client)), comp, &error);

  if (error)
    {
      g_message ("Error creating event: %s", error->message);
      g_clear_error (&error);
      return;
    }

  new_widget = gcal_event_widget_new (event);

  l = g_hash_table_lookup (priv->children, gcal_event_get_uid (event));

  if (l != NULL)
    {
      gtk_widget_destroy (l->data);

      gtk_widget_show (new_widget);
      gtk_container_add (GTK_CONTAINER (subscriber), new_widget);
    }
  else
    {
      g_warning ("%s: Widget with uuid: %s not found in view: %s",
                 G_STRFUNC, gcal_event_get_uid (event),
                 gtk_widget_get_name (GTK_WIDGET (subscriber)));
      gtk_widget_destroy (new_widget);
    }

  g_clear_object (&event);
}

static void
gcal_subscriber_view_component_removed (ECalDataModelSubscriber *subscriber,
                                        ECalClient              *client,
                                        const gchar             *uid,
                                        const gchar             *rid)
{
  GcalSubscriberViewPrivate *priv;
  const gchar *sid;
  gchar *uuid;
  GList *l;

  priv = gcal_subscriber_view_get_instance_private (GCAL_SUBSCRIBER_VIEW (subscriber));

  sid = e_source_get_uid (e_client_get_source (E_CLIENT (client)));
  if (rid != NULL)
      uuid = g_strdup_printf ("%s:%s:%s", sid, uid, rid);
  else
    uuid = g_strdup_printf ("%s:%s", sid, uid);

  l = g_hash_table_lookup (priv->children, uuid);
  if (l != NULL)
    {
      gtk_widget_destroy (l->data);
    }
  else
    {
      g_warning ("%s: Widget with uuid: %s not found in view: %s",
                 G_STRFUNC, uuid, gtk_widget_get_name (GTK_WIDGET (subscriber)));
    }

  g_free (uuid);
}

static void
gcal_subscriber_view_freeze (ECalDataModelSubscriber *subscriber)
{
}

static void
gcal_subscriber_view_thaw (ECalDataModelSubscriber *subscriber)
{
}


static void
gcal_data_model_subscriber_interface_init (ECalDataModelSubscriberInterface *iface)
{
  iface->component_added = gcal_subscriber_view_component_added;
  iface->component_modified = gcal_subscriber_view_component_modified;
  iface->component_removed = gcal_subscriber_view_component_removed;
  iface->freeze = gcal_subscriber_view_freeze;
  iface->thaw = gcal_subscriber_view_thaw;
}

static void
gcal_subscriber_view_finalize (GObject *object)
{
  GcalSubscriberViewPrivate *priv;

  priv = gcal_subscriber_view_get_instance_private (GCAL_SUBSCRIBER_VIEW (object));

  g_hash_table_destroy (priv->children);
  g_hash_table_destroy (priv->single_cell_children);
  g_hash_table_destroy (priv->overflow_cells);
  g_hash_table_destroy (priv->hidden_as_overflow);

  if (priv->multi_cell_children != NULL)
    g_list_free (priv->multi_cell_children);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_subscriber_view_parent_class)->finalize (object);
}

static void
gcal_subscriber_view_add (GtkContainer *container,
                          GtkWidget    *widget)
{
  GcalSubscriberViewPrivate *priv;
  GcalEvent *event;
  const gchar *uuid;
  GList *l = NULL;

  g_return_if_fail (GCAL_IS_EVENT_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);
  priv = gcal_subscriber_view_get_instance_private (GCAL_SUBSCRIBER_VIEW (container));
  event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (widget));
  uuid = gcal_event_get_uid (event);

  /* inserting in all children hash */
  if (g_hash_table_lookup (priv->children, uuid) != NULL)
    {
      g_warning ("Event with uuid: %s already added", uuid);
      gtk_widget_destroy (widget);
      return;
    }

  priv->children_changed = TRUE;
  l = g_list_append (l, widget);
  g_hash_table_insert (priv->children, g_strdup (uuid), l);

  if (gcal_event_is_multiday (event))
    {
      priv->multi_cell_children = g_list_insert_sorted (priv->multi_cell_children, widget,
                                                       (GCompareFunc) gcal_event_widget_sort_events);
    }
  else
    {
      guint cell_idx;

      cell_idx = gcal_subscriber_view_get_child_cell (GCAL_SUBSCRIBER_VIEW (container), GCAL_EVENT_WIDGET (widget));
      l = g_hash_table_lookup (priv->single_cell_children, GINT_TO_POINTER (cell_idx));
      l = g_list_insert_sorted (l, widget, (GCompareFunc) gcal_event_widget_compare_by_start_date);

      if (g_list_length (l) != 1)
        {
          g_hash_table_steal (priv->single_cell_children, GINT_TO_POINTER (cell_idx));
        }
      g_hash_table_insert (priv->single_cell_children, GINT_TO_POINTER (cell_idx), l);
    }

  _gcal_subscriber_view_setup_child (GCAL_SUBSCRIBER_VIEW (container), widget);
}

static void
gcal_subscriber_view_remove (GtkContainer *container,
                             GtkWidget    *widget)
{
  GcalSubscriberViewPrivate *priv;
  GcalEvent *event;
  const gchar *uuid;

  GList *l, *aux;
  GtkWidget *master_widget;

  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (container));
  priv = gcal_subscriber_view_get_instance_private (GCAL_SUBSCRIBER_VIEW (container));
  event = gcal_event_widget_get_event (GCAL_EVENT_WIDGET (widget));
  uuid = gcal_event_get_uid (event);

  l = g_hash_table_lookup (priv->children, uuid);
  if (l != NULL)
    {
      priv->children_changed = TRUE;

      gtk_widget_unparent (widget);

      master_widget = (GtkWidget*) l->data;
      if (widget == master_widget)
        {
          if (g_list_find (priv->multi_cell_children, widget) != NULL)
            {
              priv->multi_cell_children = g_list_remove (priv->multi_cell_children, widget);

              aux = g_list_next (l);
              if (aux != NULL)
                {
                  l->next = NULL;
                  aux->prev = NULL;
                  g_list_foreach (aux, (GFunc) gtk_widget_unparent, NULL);
                  g_list_free (aux);
                }
            }
          else
            {
              GHashTableIter iter;
              gpointer key, value;

              /*
               * When an event is changed, we can't rely on it's old date
               * to remove the corresponding widget. Because of that, we have
               * to iter through all the widgets to see which one matches
               */
              g_hash_table_iter_init (&iter, priv->single_cell_children);

              while (g_hash_table_iter_next (&iter, &key, &value))
                {
                  gboolean should_break;

                  should_break = FALSE;

                  for (aux = value; aux != NULL; aux = aux->next)
                    {
                      if (aux->data == widget)
                        {
                          aux = g_list_remove (g_list_copy (value), widget);

                          /*
                           * If we removed the event and there's no event left for
                           * the day, remove the key from the table. If there are
                           * events for that day, replace the list.
                           */
                          if (aux == NULL)
                            g_hash_table_remove (priv->single_cell_children, key);
                          else
                            g_hash_table_replace (priv->single_cell_children, key, aux);

                          should_break = TRUE;

                          break;
                        }
                    }

                  if (should_break)
                    break;
                }
            }
        }

      l = g_list_remove (g_list_copy (l), widget);
      if (l == NULL)
        g_hash_table_remove (priv->children, uuid);
      else
        g_hash_table_replace (priv->children, g_strdup (uuid), l);

      g_hash_table_remove (priv->hidden_as_overflow, uuid);

      gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

static void
gcal_subscriber_view_forall (GtkContainer *container,
                             gboolean      include_internals,
                             GtkCallback   callback,
                             gpointer      callback_data)
{
  GcalSubscriberViewPrivate *priv;
  GList *l, *l2, *aux = NULL;

  priv = gcal_subscriber_view_get_instance_private (GCAL_SUBSCRIBER_VIEW (container));

  l2 = g_hash_table_get_values (priv->children);
  for (l = l2; l != NULL; l = g_list_next (l))
    aux = g_list_concat (aux, g_list_reverse (g_list_copy (l->data)));
  g_list_free (l2);

  l = aux;
  while (aux != NULL)
    {
      GtkWidget *widget = (GtkWidget*) aux->data;
      aux = aux->next;

      (*callback) (widget, callback_data);
    }
  g_list_free (l);
}

static guint
gcal_subscriber_view_get_child_cell (GcalSubscriberView  *subscriber,
                                     GcalEventWidget     *child)
{
  GcalSubscriberViewClass *klass;

  g_return_val_if_fail (GCAL_IS_SUBSCRIBER_VIEW (subscriber), 0);
  g_return_val_if_fail (GCAL_IS_EVENT_WIDGET (child), 0);

  klass = GCAL_SUBSCRIBER_VIEW_GET_CLASS (subscriber);

  if (klass->get_child_cell)
    return klass->get_child_cell (subscriber, child);

  return 0;
}

static void
gcal_subscriber_view_clear_state (GcalSubscriberView  *subscriber_view)
{
  GcalSubscriberViewClass *klass;

  g_return_if_fail (GCAL_IS_SUBSCRIBER_VIEW (subscriber_view));

  klass = GCAL_SUBSCRIBER_VIEW_GET_CLASS (subscriber_view);
  if (klass->clear_state)
    klass->clear_state (subscriber_view);
}

static void
gcal_subscriber_view_class_init (GcalSubscriberViewClass *klass)
{
  GtkContainerClass *container_class;

  klass->get_child_cell = gcal_subscriber_view_get_child_cell;

  G_OBJECT_CLASS (klass)->finalize = gcal_subscriber_view_finalize;

  container_class = GTK_CONTAINER_CLASS (klass);
  container_class->add = gcal_subscriber_view_add;
  container_class->remove = gcal_subscriber_view_remove;
  container_class->forall = gcal_subscriber_view_forall;

  signals[EVENT_ACTIVATED] = g_signal_new ("event-activated", GCAL_TYPE_SUBSCRIBER_VIEW, G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET (GcalSubscriberViewClass, event_activated),
                                           NULL, NULL, NULL,
                                           G_TYPE_NONE, 1, GCAL_TYPE_EVENT_WIDGET);
}

static void
gcal_subscriber_view_init (GcalSubscriberView *self)
{
  GcalSubscriberViewPrivate *priv;

  priv = gcal_subscriber_view_get_instance_private (self);
  self->priv = priv;

  priv->children = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_list_free);
  priv->single_cell_children = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_list_free);
  priv->overflow_cells = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_list_free);
  priv->hidden_as_overflow = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

/* Public API */
GtkWidget*
gcal_subscriber_view_get_child_by_uuid (GcalSubscriberView *subscriber_view,
                                        const gchar        *uuid)
{
  GcalSubscriberViewClass *klass;
  GcalSubscriberViewPrivate *priv;
  GList *l;

  g_return_val_if_fail (GCAL_IS_SUBSCRIBER_VIEW (subscriber_view), NULL);

  /* Allow for descendants to hook in here */
  klass = GCAL_SUBSCRIBER_VIEW_GET_CLASS (subscriber_view);
  if (klass->get_child_by_uuid)
    return klass->get_child_by_uuid (subscriber_view, uuid);

  priv = gcal_subscriber_view_get_instance_private (subscriber_view);
  l = g_hash_table_lookup (priv->children, uuid);
  if (l != NULL)
    return l->data;

  return 0;
}

void
_gcal_subscriber_view_setup_child (GcalSubscriberView *subscriber_view,
				   GtkWidget          *child_widget)
{
  if (gtk_widget_get_parent (child_widget) == NULL)
    gtk_widget_set_parent (child_widget, GTK_WIDGET (subscriber_view));
  g_signal_connect (child_widget, "activate", G_CALLBACK (event_activated), subscriber_view);
  g_signal_connect (child_widget, "hide", G_CALLBACK (event_visibility_changed), subscriber_view);
  g_signal_connect (child_widget, "show", G_CALLBACK (event_visibility_changed), subscriber_view);
}
