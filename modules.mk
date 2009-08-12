mod_rounded_corners.la: mod_rounded_corners.slo
	$(SH_LINK) -rpath $(libexecdir) -module -avoid-version  mod_rounded_corners.lo
DISTCLEAN_TARGETS = modules.mk
shared =  mod_rounded_corners.la
