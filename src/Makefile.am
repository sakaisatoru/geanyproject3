bin_PROGRAMS = geanyproject

AM_CPPFLAGS = -DICONDIR=\"$(datadir)/pixmaps\" -DLOCALEDIR=\"$(localedir)\" -DGETTEXT_PACKAGE=\""$(GETTEXT_PACKAGE)"\"

geanyproject_SOURCES = \
	main.c
	
#~ 	i18n.h
#~  	gtksourceiter.h gtksourceiter.c
#~ 	undo.h undo.c
#~ 	emacs.h emacs.c
#~ 	gnomeprint.h gnomeprint.c
#~ 	indent.h indent.c
#~ 	linenum.h linenum.c

geanyproject_CFLAGS  = -pthread $(GTK_CFLAGS) 
geanyproject_LDADD   =  $(INTLLIBS) $(GTK_LIBS) 
