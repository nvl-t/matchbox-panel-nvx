AM_CPPFLAGS=-DPKGDATADIR=\"$(pkgdatadir)\" \
            -DGETTEXT_PACKAGE=\"mb-panel\" \
	    -DDEFAULT_APPLET_PATH=\"$(pkglibdir)\" \
            $(MB_PANEL_CFLAGS) \
	    -I$(top_srcdir) -I$(top_builddir)
AM_CFLAGS = $(WARN_CFLAGS)

mb_panelincdir = $(pkgincludedir)

mb_panelinc_HEADERS = mb-panel.h mb-panel-scaling-image.h mb-panel-scaling-image2.h

bin_PROGRAMS = mb-panel

mb_panel_SOURCES = mb-panel.c mb-panel-scaling-image.c mb-panel-scaling-image2.c

mb_panel_LDADD = $(MB_PANEL_LIBS)

-include $(top_srcdir)/git.mk
