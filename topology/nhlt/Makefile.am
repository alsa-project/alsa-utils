alsatplg_module_nhlt_LTLIBRARIES = libalsatplg_module_nhlt.la

alsatplg_module_nhltdir = @ALSA_TOPOLOGY_PLUGIN_DIR@

AM_CFLAGS = -Wall -fvisibility=hidden -I$(top_srcdir)/include -I$(top_srcdir)/topology
AM_LDFLAGS = -module -avoid-version -export-dynamic -no-undefined $(LDFLAGS_NOUNDEFINED)

libalsatplg_module_nhlt_la_SOURCES = nhlt-processor.c \
				intel/intel-nhlt.c \
				intel/dmic-nhlt.c \
				intel/dmic/dmic-debug.c intel/dmic/dmic-process.c \
				intel/ssp-nhlt.c \
				intel/ssp/ssp-debug.c intel/ssp/ssp-process.c

noinst_HEADERS = nhlt.h \
		intel/intel-nhlt.h \
		intel/dmic-nhlt.h \
		intel/dmic/dmic-intel.h \
		intel/dmic/dmic-process.h \
		intel/dmic/dmic-debug.h \
		intel/dmic/dmic-internal.h \
		intel/dmic/pdm-decim-fir.h \
		intel/ssp-nhlt.h \
		intel/ssp/ssp-debug.h \
		intel/ssp/ssp-intel.h \
		intel/ssp/ssp-process.h \
		intel/ssp/ssp-internal.h

libalsatplg_module_nhlt_la_LIBADD = @ALSA_LIBS@
