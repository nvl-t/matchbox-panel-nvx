/* 
 * (C) 2006 OpenedHand Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * Licensed under the GPL v2 or greater.
 */

#ifndef __MB_PANEL_H__
#define __MB_PANEL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

G_MODULE_IMPORT GtkWidget *
mb_panel_applet_create (const char    *id,
                        GtkOrientation orientation);

G_END_DECLS

#endif /* __MB_PANEL_H__ */
