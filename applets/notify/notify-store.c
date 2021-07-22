/* 
 * (C) 2008 OpenedHand Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Ross Burton <ross@openedhand.com>
 *
 * Licensed under the GPL v2 or greater.
 */

#include <config.h>

#include "notify-store.h"
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <gtk/gtk.h>
#include "marshal.h"

G_DEFINE_TYPE (MbNotifyStore, mb_notify_store, G_TYPE_OBJECT);

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MB_TYPE_NOTIFY_STORE, MbNotifyStorePrivate))

enum {
  NOTIFICATION_ADDED,
  NOTIFICATION_CLOSED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

#define DEFAULT_TIMEOUT 3000

typedef struct {
  guint next_id;
  GList *notifications;
} MbNotifyStorePrivate;

typedef struct {
  MbNotifyStore *notify;
  guint id;
} TimeoutData;

static guint
get_next_id (MbNotifyStore *notify)
{
  MbNotifyStorePrivate *priv = GET_PRIVATE (notify);
  /* We're not threaded, so this is perfectly safe */
  return ++priv->next_id;
}

static gboolean
find_notification (MbNotifyStore *notify, guint id, Notification **found)
{
  /* TODO: should this return a GList*? */
  MbNotifyStorePrivate *priv = GET_PRIVATE (notify);
  GList *l;

  for (l = priv->notifications; l ; l = l->next) {
    Notification *notification = l->data;
    if (notification->id == id) {
      *found = notification;
      return TRUE;
    }
  }
  
  return FALSE;
}

static void
free_notification (Notification *n)
{
  g_free (n->summary);
  g_free (n->body);
  g_free (n->icon_name);
  g_source_remove (n->timeout_id);
  g_slice_free (Notification, n);
}

static gboolean
notification_timeout (TimeoutData *data)
{
  mb_notify_store_close (data->notify, data->id, ClosedExpired);
  g_slice_free (TimeoutData, data);
  return FALSE;
}


/*
 * Notification Manager implementation.
 */

/* TODO: make this async so we can store the sender, and then target the
   NotificationClosed signal? */
static gboolean
notification_manager_notify (MbNotifyStore *notify,
                             const gchar *app_name, const guint id,
                             const gchar *icon, const gchar *summary,
                             const gchar *body,
                             const gchar **actions, GHashTable *hints,
                             gint timeout, guint *new_id, GError *error)
{
  MbNotifyStorePrivate *priv = GET_PRIVATE (notify);
  Notification *notification;

  /* TODO: Sanity check the required arguments */
  
  if (find_notification (notify, id, &notification)) {
    /* Found an existing notification, clear it */
    g_free (notification->summary);
    g_free (notification->body);
    g_free (notification->icon_name);
  } else {
    /* This is a new notification, create a new structure and allocate an ID */
    notification = g_slice_new0 (Notification);
    notification->id = get_next_id (notify);
    /* TODO: use _insert_sorted with some magic sorting algorithm */
    priv->notifications = g_list_append (priv->notifications, notification);
  }

  notification->summary = g_strdup (summary);
  notification->body = g_strdup (body);
  notification->icon_name = g_strdup (icon);

  /* A timeout of -1 means implementation defined */
  if (timeout == -1)
    timeout = DEFAULT_TIMEOUT;
  
  if (timeout) {
    /* This struct is a bit annoying, maybe add a store pointer to the
       Notification struct? */
    TimeoutData *data;
    data = g_slice_new (TimeoutData);
    data->notify = notify;
    data->id = notification->id;
    notification->timeout_id = g_timeout_add
      (timeout, (GSourceFunc)notification_timeout, data);
  }
  
  g_signal_emit (notify, signals[NOTIFICATION_ADDED], 0, notification);
  
  *new_id = notification->id;
  return TRUE;
}

static gboolean
notification_manager_close_notification (MbNotifyStore *notify, guint id, GError **error)
{
  if (mb_notify_store_close (notify, id, ClosedProgramatically)) {
    return TRUE;
  } else {
    g_set_error (error, g_quark_from_static_string("NotifyStore"), 0, "Unknown notification ID %d", id);
    return FALSE;
  }
}

static gboolean
notification_manager_get_capabilities (MbNotifyStore *notify, gchar ***caps, GError *error)
{
  *caps = g_new0 (gchar *, 4);
  
  (*caps)[0] = g_strdup ("body");
  (*caps)[1] = g_strdup ("body-markup");
  (*caps)[2] = g_strdup ("icon-static");
  (*caps)[3] = NULL;
  
  return TRUE;
}

static gboolean
notification_manager_get_server_information (MbNotifyStore *notify,
                                             gchar **name,
                                             gchar **vendor,
                                             gchar **version,
                                             gchar **spec_version,
                                             GError *error)
{
  *name = g_strdup ("Matchbox Panel Notification Manager");
  *vendor = g_strdup ("OpenedHand");
  *version = g_strdup (VERSION);
  *spec_version = g_strdup ("1.0");
  return TRUE;
}


/*
 * GObject methods
 */

static void
mb_notify_store_finalize (GObject *object)
{
  MbNotifyStorePrivate *priv = GET_PRIVATE (object);

  g_list_foreach (priv->notifications, (GFunc)free_notification, NULL);
  g_list_free (priv->notifications);
  
  G_OBJECT_CLASS (mb_notify_store_parent_class)->finalize (object);
}

#include "notification-manager-glue.h"
static void
mb_notify_store_class_init (MbNotifyStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MbNotifyStorePrivate));

  object_class->finalize = mb_notify_store_finalize;

  signals[NOTIFICATION_ADDED] =
    g_signal_new ("notification-added",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MbNotifyStoreClass, notification_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  signals[NOTIFICATION_CLOSED] =
    g_signal_new ("notification-closed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MbNotifyStoreClass, notification_closed),
                  NULL, NULL,
                  mb_marshal_VOID__UINT_UINT,
                  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
  
  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass),
                                   &dbus_glib_notification_manager_object_info);
}

static void
connect_to_dbus (MbNotifyStore *self)
{
  DBusGConnection *connection;
  DBusGProxy *bus_proxy;
  GError *error = NULL;
  guint32 request_name_ret;

  connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (!connection) {
    g_warning ("Cannot connect to DBus: %s", error->message);
    g_error_free (error);
    return;
  }

  bus_proxy = dbus_g_proxy_new_for_name (connection,
                                         DBUS_SERVICE_DBUS,
                                         DBUS_PATH_DBUS,
                                         DBUS_INTERFACE_DBUS);
  
  if (!org_freedesktop_DBus_request_name (bus_proxy, "org.freedesktop.Notifications",
                                          DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                          &request_name_ret, &error)) {
    g_warning ("Cannot request name: %s", error->message);
    g_error_free (error);
    return;
  }
  
  if (request_name_ret == DBUS_REQUEST_NAME_REPLY_EXISTS) {
    g_printerr ("Notification manager already running, not taking over\n");
    return;
  }
  
  dbus_g_connection_register_g_object (connection,
                                       "/org/freedesktop/Notifications",
                                       G_OBJECT (self));
}

static void
mb_notify_store_init (MbNotifyStore *self)
{
  connect_to_dbus (self);
}

MbNotifyStore *
mb_notify_store_new (void)
{
  return g_object_new (MB_TYPE_NOTIFY_STORE, NULL);
}

gboolean
mb_notify_store_close (MbNotifyStore *notify, guint id, MbNotifyStoreCloseReason reason)
{
  MbNotifyStorePrivate *priv = GET_PRIVATE (notify);
  Notification *notification;

  if (find_notification (notify, id, &notification)) {
    priv->notifications = g_list_remove (priv->notifications, notification);
    free_notification (notification);
    g_signal_emit (notify, signals[NOTIFICATION_CLOSED], 0, id, reason);
    return TRUE;
  } else {
    return FALSE;
  }
}
