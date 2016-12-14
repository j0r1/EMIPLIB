### all of the standard pc files we need to generate
pcfiles = 							\
	emiplib.pc

pcfiles_uninstalled =						\
	emiplib-uninstalled.pc

all-local: $(pcfiles) $(pcfiles_uninstalled)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcfiles)

EXTRA_DIST =							\
	emiplib.pc.in						\
	emiplib-uninstalled.pc.in

CLEANFILES = $(pcfiles) $(pcfiles_uninstalled)
