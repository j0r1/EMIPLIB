SUBDIRS = src examples doc pkgconfig 
EXTRA_DIST = Doxyfile emiplib.sln emiplib.vcproj emiplib_wce.sln emiplib_wce.vcproj \
	     LICENSE.GPL LICENSE.LGPL ChangeLog copyheaderfiles.bat \
	     emiplib_wce5.vcproj emiplib_wce5.sln

dist-hook:
	rm -f $(distdir)/src/components/output/mipqtoutput.moc
