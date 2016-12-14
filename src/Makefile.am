SUBDIRS=thirdparty

lib_LTLIBRARIES = libemip.la
libemip_la_SOURCES = \
core/mipcomponent.cpp \
core/mipcomponentchain.cpp \
core/mipversion.cpp \
core/mipdebug.cpp \
components/timer/mipaveragetimer.cpp \
components/timer/mippusheventtimer.cpp

if MIP_USEQT
libemip_la_SOURCES += components/output/mipqtoutput.moc
endif

libemip_la_SOURCES += \
components/output/mipmessagedumper.cpp \
components/output/mipsndfileoutput.cpp \
components/output/mipwavoutput.cpp \
components/output/mipalsaoutput.cpp \
components/output/mipesdoutput.cpp \
components/output/mipqtoutput.cpp \
components/output/mipvideoframestorage.cpp \
components/output/mipjackoutput.cpp \
components/output/mipsdlaudiooutput.cpp \
components/output/mipopenaloutput.cpp \
components/input/mipsndfileinput.cpp \
components/input/mipaudiofileinput.cpp \
components/input/mipv4linput.cpp \
components/input/mipfrequencygenerator.cpp \
components/input/mipwavinput.cpp \
components/input/mipjackinput.cpp \
components/io/mipossinputoutput.cpp \
components/transmission/miprtpcomponent.cpp \
components/transmission/miprtpspeexencoder.cpp \
components/transmission/miprtpdecoder.cpp \
components/transmission/miprtpspeexdecoder.cpp \
components/transmission/miprtpulawencoder.cpp \
components/transmission/miprtpulawdecoder.cpp \
components/transmission/miprtpalawencoder.cpp \
components/transmission/miprtpalawdecoder.cpp \
components/transmission/miprtpvideoencoder.cpp \
components/transmission/miprtpvideodecoder.cpp \
components/transmission/miprtplpcencoder.cpp \
components/transmission/miprtplpcdecoder.cpp \
components/transmission/miprtpgsmencoder.cpp \
components/transmission/miprtpgsmdecoder.cpp \
components/transform/mipsamplingrateconverter.cpp \
components/transform/mipsampleencoder.cpp \
components/transform/mipaudio3dbase.cpp \
components/transform/miphrirlisten.cpp \
components/transform/mipaudiosplitter.cpp \
components/transform/mipaudiodistancefade.cpp \
components/transform/mipaudiofilter.cpp \
components/mixer/mipaudiomixer.cpp \
components/mixer/mipvideomixer.cpp \
components/mixer/mipmediabuffer.cpp \
components/codec/mipspeexencoder.cpp \
components/codec/mipspeexdecoder.cpp \
components/codec/mipavcodecencoder.cpp \
components/codec/mipavcodecdecoder.cpp \
components/codec/mipulawencoder.cpp \
components/codec/mipulawdecoder.cpp \
components/codec/mipalawencoder.cpp \
components/codec/mipalawdecoder.cpp \
components/codec/mipgsmencoder.cpp \
components/codec/mipgsmdecoder.cpp \
components/codec/miplpcencoder.cpp \
components/codec/miplpcdecoder.cpp \
util/mipsignalwaiter.cpp \
util/mipdirectorybrowser.cpp \
util/mipwavreader.cpp \
util/mipwavwriter.cpp \
util/miprtpsynchronizer.cpp \
util/mipspeexutil.cpp \
sessions/mipaudiosession.cpp \
sessions/mipvideosession.cpp \
thirdparty/lpc/lpccodec.cpp \
thirdparty/lpc/lpccodec.h \
thirdparty/gsm/src/gsm_add.cpp \
thirdparty/gsm/src/gsm_code.cpp \
thirdparty/gsm/src/gsm_create.cpp \
thirdparty/gsm/src/gsm_debug.cpp \
thirdparty/gsm/src/gsm_decode.cpp \
thirdparty/gsm/src/gsm_decode2.cpp \
thirdparty/gsm/src/gsm_destroy.cpp \
thirdparty/gsm/src/gsm_encode.cpp \
thirdparty/gsm/src/gsm_explode.cpp \
thirdparty/gsm/src/gsm_implode.cpp \
thirdparty/gsm/src/gsm_long_term.cpp \
thirdparty/gsm/src/gsm_lpc.cpp \
thirdparty/gsm/src/gsm_option.cpp \
thirdparty/gsm/src/gsm_preprocess.cpp \
thirdparty/gsm/src/gsm_print.cpp \
thirdparty/gsm/src/gsm_rpe.cpp \
thirdparty/gsm/src/gsm_short_term.cpp \
thirdparty/gsm/src/gsm_table.cpp \
thirdparty/gsm/inc/config.h \
thirdparty/gsm/inc/gsm.h \
thirdparty/gsm/inc/private.h \
thirdparty/gsm/inc/proto.h \
thirdparty/gsm/inc/unproto.h

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
components/output/mipwavoutput.h \
components/output/mipalsaoutput.h \
components/output/mipesdoutput.h \
components/output/mipqtoutput.h \
components/output/mipwinmmoutput.h \
components/output/mipvideoframestorage.h \
components/output/mipjackoutput.h \
components/output/mipsdlaudiooutput.h \
components/output/mipopenaloutput.h \
components/input/mipsndfileinput.h  \
components/input/mipaudiofileinput.h  \
components/input/mipv4linput.h \
components/input/mipwinmminput.h \
components/input/mipfrequencygenerator.h \
components/input/mipwavinput.h \
components/input/mipjackinput.h \
components/io/mipossinputoutput.h \
components/transmission/miprtpcomponent.h \
components/transmission/miprtpencoder.h \
components/transmission/miprtpspeexencoder.h \
components/transmission/miprtpdecoder.h \
components/transmission/miprtppacketdecoder.h \
components/transmission/miprtpspeexdecoder.h \
components/transmission/miprtpulawencoder.h \
components/transmission/miprtpulawdecoder.h \
components/transmission/miprtpalawencoder.h \
components/transmission/miprtpalawdecoder.h \
components/transmission/miprtpvideoencoder.h \
components/transmission/miprtpvideodecoder.h \
components/transmission/miprtpdummydecoder.h \
components/transmission/miprtplpcencoder.h \
components/transmission/miprtplpcdecoder.h \
components/transmission/miprtpgsmencoder.h \
components/transmission/miprtpgsmdecoder.h \
components/transform/mipsamplingrateconverter.h \
components/transform/mipsampleencoder.h \
components/transform/mipaudio3dbase.h \
components/transform/miphrirbase.h \
components/transform/miphrirlisten.h \
components/transform/mipaudiosplitter.h \
components/transform/mipaudiodistancefade.h \
components/transform/mipaudiofilter.h \
components/mixer/mipaudiomixer.h \
components/mixer/mipvideomixer.h \
components/mixer/mipmediabuffer.h \
components/codec/mipspeexencoder.h \
components/codec/mipspeexdecoder.h \
components/codec/mipavcodecencoder.h \
components/codec/mipavcodecdecoder.h \
components/codec/mipulawencoder.h \
components/codec/mipulawdecoder.h \
components/codec/mipalawencoder.h \
components/codec/mipalawdecoder.h \
components/codec/mipgsmencoder.h \
components/codec/mipgsmdecoder.h \
components/codec/miplpcencoder.h \
components/codec/miplpcdecoder.h \
util/mipsignalwaiter.h \
util/mipdirectorybrowser.h \
util/mipwavreader.h \
util/mipwavwriter.h \
util/miprtpsynchronizer.h \
util/mipresample.h \
util/mipspeexutil.h \
sessions/mipaudiosession.h \
sessions/mipvideosession.h

INCLUDES = -I core/ -I util/ -I components/codec/ -I components/input/ -I components/io/ -I components/mixer/ \
	   -I components/output/ -I components/timer/ -I components/transform/ -I components/transmission/ \
	   -I thirdparty/lpc -I thirdparty/gsm/inc \
	   @MIPINCLUDES@

libemip_la_LDFLAGS = -release @VERSION@
libemipincludedir = ${includedir}/emiplib

if MIP_USEQT
components/output/mipqtoutput.cpp: components/output/mipqtoutput.moc
	
components/output/mipqtoutput.moc:
	@MOC@ -o components/output/mipqtoutput.moc components/output/mipqtoutput.cpp
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

