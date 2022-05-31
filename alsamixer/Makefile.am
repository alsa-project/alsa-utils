AM_CFLAGS = -D_GNU_SOURCE @CURSES_CFLAGS@ -DCURSESINC="@CURSESINC@"
LDADD = @CURSESLIB@

bin_PROGRAMS = alsamixer
alsamixer_SOURCES = card_select.c card_select.h \
		bindings.c bindings.h \
		cli.c \
		colors.c colors.h \
		curskey.c curskey.h \
		configparser.c configparser.h \
		device_name.c device_name.h \
		die.c die.h \
		mainloop.c mainloop.h \
		mem.c mem.h \
		menu_widget.c menu_widget.h \
		mixer_clickable.c mixer_clickable.h \
		mixer_controls.c mixer_controls.h \
		mixer_display.c mixer_display.h \
		mixer_widget.c mixer_widget.h \
		proc_files.c proc_files.h \
		textbox.c textbox.h \
		utils.c utils.h \
		volume_mapping.c volume_mapping.h \
		widget.c widget.h
man_MANS = alsamixer.1
EXTRA_DIST = alsamixer.1 alsamixer.rc.example
alsamixer_CPPFLAGS = -I$(top_srcdir)/include

#LDFLAGS = -static
#CFLAGS += -g -Wall
