lib_LTLIBRARIES = libemip.la
libemip_la_SOURCES = \
core/mipcomponent.cpp \
core/mipcomponentchain.cpp \
core/mipversion.cpp \
components/timer/mipaveragetimer.cpp \
components/timer/mippusheventtimer.cpp

if MIP_USEQT
libemip_la_SOURCES += components/output/mipqtoutput.moc
endif

libemip_la_SOURCES += \
components/output/mipmessagedumper.cpp \
components/output/mipsndfileoutput.cpp \
components/output/mipalsaoutput.cpp \
components/output/mipesdoutput.cpp \
components/output/mipqtoutput.cpp \
components/output/mipvideoframestorage.cpp \
components/input/mipsndfileinput.cpp \
components/input/mipaudiofileinput.cpp \
components/input/mipv4linput.cpp \
components/input/mipfrequencygenerator.cpp \
components/input/mipwavinput.cpp \
components/io/mipossinputoutput.cpp \
components/transmission/miprtpcomponent.cpp \
components/transmission/miprtpaudioencoder.cpp \
components/transmission/miprtpdecoder.cpp \
components/transmission/miprtpaudiodecoder.cpp \
components/transmission/miprtpvideoencoder.cpp \
components/transmission/miprtpvideodecoder.cpp \
components/transform/mipsamplingrateconverter.cpp \
components/transform/mipsampleencoder.cpp \
components/transform/mipaudio3dbase.cpp \
components/transform/miphrirlisten.cpp \
components/transform/mipaudiosplitter.cpp \
components/mixer/mipaudiomixer.cpp \
components/mixer/mipvideomixer.cpp \
components/mixer/mipmediabuffer.cpp \
components/codec/mipspeexencoder.cpp \
components/codec/mipspeexdecoder.cpp \
components/codec/mipavcodecencoder.cpp \
components/codec/mipavcodecdecoder.cpp \
util/mipsignalwaiter.cpp \
util/mipdirectorybrowser.cpp \
util/mipwavreader.cpp \
util/miprtpsynchronizer.cpp \
sessions/mipaudiosession.cpp \
sessions/mipvideosession.cpp

libemipinclude_HEADERS = \
core/mipcompat.h \
core/miperrorbase.h \
core/mipcomponent.h \
core/mipcomponentchain.h \
core/mipconfig.h \
core/mipdebug.h \
core/mipmessage.h \
core/mipsystemmessage.h \
core/miptime.h \
core/miptypes.h \
core/mipmediamessage.h \
core/mipaudiomessage.h \
core/miprawaudiomessage.h \
core/miprawvideomessage.h \
core/mipvideomessage.h \
core/mipencodedaudiomessage.h \
core/mipencodedvideomessage.h \
core/miprtpmessage.h \
core/mipconfig_unix.h \
core/miptypes_unix.h \
core/mipfeedback.h \
core/mipversion.h \
components/timer/mipaveragetimer.h \
components/timer/mippusheventtimer.h \
components/output/mipmessagedumper.h \
components/output/mipsndfileoutput.h \
components/output/mipalsaoutput.h \
components/output/mipesdoutput.h \
components/output/mipqtoutput.h \
components/output/mipwinmmoutput.h \
components/output/mipvideoframestorage.h \
components/input/mipsndfileinput.h  \
components/input/mipaudiofileinput.h  \
components/input/mipv4linput.h \
components/input/mipwinmminput.h \
components/input/mipfrequencygenerator.h \
components/input/mipwavinput.h \
components/io/mipossinputoutput.h \
components/transmission/miprtpcomponent.h \
components/transmission/miprtpaudioencoder.h \
components/transmission/miprtpdecoder.h \
components/transmission/miprtpaudiodecoder.h \
components/transmission/miprtpvideoencoder.h \
components/transmission/miprtpvideodecoder.h \
components/transform/mipsamplingrateconverter.h \
components/transform/mipsampleencoder.h \
components/transform/mipaudio3dbase.h \
components/transform/miphrirbase.h \
components/transform/miphrirlisten.h \
components/transform/mipaudiosplitter.h \
components/mixer/mipaudiomixer.h \
components/mixer/mipvideomixer.h \
components/mixer/mipmediabuffer.h \
components/codec/mipspeexencoder.h \
components/codec/mipspeexdecoder.h \
components/codec/mipavcodecencoder.h \
components/codec/mipavcodecdecoder.h \
util/mipsignalwaiter.h \
util/mipdirectorybrowser.h \
util/mipwavreader.h \
util/miprtpsynchronizer.h \
sessions/mipaudiosession.h \
sessions/mipvideosession.h

INCLUDES = -I core/ -I util/ -I components/codec/ -I components/input/ -I components/io/ -I components/mixer/ \
	   -I components/output/ -I components/timer/ -I components/transform/ -I components/transmission/ \
	   @MIPINCLUDES@
libemip_la_LDFLAGS = -release 0.10.0
libemipincludedir = ${includedir}/emiplib

if MIP_USEQT
components/output/mipqtoutput.cpp: components/output/mipqtoutput.moc
	
components/output/mipqtoutput.moc:
	/usr/lib/qt/bin/moc -o components/output/mipqtoutput.moc components/output/mipqtoutput.cpp
endif

EXTRA_DIST = \
core/miptypes_win.h \
core/mipconfig_win.h \
components/input/mipwinmminput.cpp \
components/input/mipwinmminput.h \
components/input/mipdirectshowcapture.cpp \
components/input/mipdirectshowcapture.h \
components/output/mipwinmmoutput.cpp \
components/output/mipwinmmoutput.h

