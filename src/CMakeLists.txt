add_definitions(-DEMIPLIB_COMPILING)

set(HEADERS
core/mipcompat.h
core/mipmessage.h
core/mipmediamessage.h
core/miperrorbase.h
core/mipfeedback.h
core/mipversion.h
core/miprawvideomessage.h
core/mipdebug.h
core/mipencodedaudiomessage.h
core/mipsystemmessage.h
core/mipencodedvideomessage.h
core/miptime.h
core/mipcomponent.h
core/mipcomponentchain.h
core/mipaudiomessage.h
core/miprtpmessage.h
core/mipvideomessage.h
core/mipcomponentalias.h
core/miprawaudiomessage.h
core/miptypes_win.h
${PROJECT_BINARY_DIR}/src/core/mipconfig.h
${PROJECT_BINARY_DIR}/src/core/miptypes.h
components/input/mipjackinput.h
components/input/mipdirectshowcapture.h
components/input/mipsndfileinput.h
components/input/mipaudiorecorderinput.h
components/input/mipv4l2input.h
components/input/mipfrequencygenerator.h
components/input/mipwavinput.h
components/input/mipaudiofileinput.h
components/input/mipwinmminput.h
components/input/mipyuv420fileinput.h
components/output/mipsdlaudiooutput.h
components/output/mipjackoutput.h
components/output/mipesdoutput.h
components/output/mipopenaloutput.h
components/output/mipalsaoutput.h
components/output/mipmessagedumper.h
components/output/mipqtoutput.h
components/output/mipsndfileoutput.h
components/output/mipvideoframestorage.h
components/output/mipaudiotrackoutput.h
components/output/mipwavoutput.h
components/output/mipwinmmoutput.h 
components/timer/mipinterchaintimer.h
components/timer/mipaveragetimer.h
components/timer/mippusheventtimer.h
components/mixer/mipaudiomixer.h
components/mixer/mipmediabuffer.h
components/mixer/mipvideomixer.h
components/io/mipossinputoutput.h
components/io/mippainputoutput.h
components/transmission/miprtppacketdecoder.h
components/transmission/miprtpl16encoder.h
components/transmission/miprtpcomponent.h
components/transmission/miprtpdecoder.h
components/transmission/miprtpulawencoder.h
components/transmission/miprtpalawdecoder.h
components/transmission/miprtpgsmdecoder.h
components/transmission/miprtplpcencoder.h
components/transmission/miprtph263encoder.h
components/transmission/miprtpspeexencoder.h
components/transmission/miprtpl16decoder.h
components/transmission/miprtpulawdecoder.h
components/transmission/miprtplpcdecoder.h
components/transmission/miprtpvideoencoder.h
components/transmission/miprtph263decoder.h
components/transmission/miprtpspeexdecoder.h
components/transmission/miprtpencoder.h
components/transmission/miprtpdummydecoder.h
components/transmission/miprtpalawencoder.h
components/transmission/miprtpgsmencoder.h
components/transmission/miprtpvideodecoder.h
components/transmission/miprtpsilkdecoder.h
components/transmission/miprtpsilkencoder.h
components/transmission/miprtpopusencoder.h
components/transmission/miprtpopusdecoder.h
components/transform/mipaudio3dbase.h
components/transform/miphrirlisten.h
components/transform/mipaudiosplitter.h
components/transform/mipspeexechocanceller.h
components/transform/mipaudiodistancefade.h
components/transform/miphrirbase.h
components/transform/mipaudiofilter.h
components/transform/mipavcodecframeconverter.h
components/transform/mipsamplingrateconverter.h
components/transform/mipsampleencoder.h
components/transform/mipyuv420framecutter.h
components/codec/mipspeexencoder.h
components/codec/mipalawencoder.h
components/codec/mipgsmencoder.h
components/codec/mipavcodecdecoder.h
components/codec/mipulawdecoder.h
components/codec/mipavcodecencoder.h
components/codec/miplpcdecoder.h
components/codec/mipulawencoder.h
components/codec/mipalawdecoder.h
components/codec/mipspeexdecoder.h
components/codec/mipgsmdecoder.h
components/codec/miplpcencoder.h
components/codec/miptinyjpegdecoder.h
components/codec/mipsilkdecoder.h
components/codec/mipsilkencoder.h
components/codec/mipopusencoder.h
components/codec/mipopusdecoder.h
components/util/mipoutputmessagequeue.h
sessions/mipaudiosession.h
sessions/mipvideosession.h
util/miprtpsynchronizer.h
util/mipstreambuffer.h
util/mipsignalwaiter.h
util/mipwavwriter.h
util/miprtppacketgrouper.h
util/mipdirectorybrowser.h
util/mipresample.h
util/mipwavreader.h
util/mipspeexutil.h
)

set(SOURCES
core/mipcomponent.cpp
core/mipcomponentchain.cpp
core/mipversion.cpp
core/mipdebug.cpp
components/input/mipjackinput.cpp
components/input/mipdirectshowcapture.cpp
components/input/mipsndfileinput.cpp
components/input/mipaudiorecorderinput.cpp
components/input/mipv4l2input.cpp
components/input/mipfrequencygenerator.cpp
components/input/mipwavinput.cpp
components/input/mipaudiofileinput.cpp
components/input/mipwinmminput.cpp 
components/input/mipyuv420fileinput.cpp
components/output/mipjackoutput.cpp
components/output/mipesdoutput.cpp
components/output/mipopenaloutput.cpp
components/output/mipalsaoutput.cpp
components/output/mipmessagedumper.cpp
components/output/mipsndfileoutput.cpp
components/output/mipqtoutput.cpp
components/output/mipvideoframestorage.cpp
components/output/mipaudiotrackoutput.cpp
components/output/mipwavoutput.cpp
components/output/mipsdlaudiooutput.cpp
components/output/mipwinmmoutput.cpp
components/timer/mipaveragetimer.cpp
components/timer/mippusheventtimer.cpp
components/timer/mipinterchaintimer.cpp
components/mixer/mipmediabuffer.cpp
components/mixer/mipvideomixer.cpp
components/mixer/mipaudiomixer.cpp
components/io/mippainputoutput.cpp
components/io/mipossinputoutput.cpp
components/transmission/miprtplpcdecoder.cpp
components/transmission/miprtpvideoencoder.cpp
components/transmission/miprtph263decoder.cpp
components/transmission/miprtpspeexdecoder.cpp
components/transmission/miprtpalawencoder.cpp
components/transmission/miprtpgsmencoder.cpp
components/transmission/miprtpvideodecoder.cpp
components/transmission/miprtpcomponent.cpp
components/transmission/miprtpl16encoder.cpp
components/transmission/miprtpdecoder.cpp
components/transmission/miprtpulawencoder.cpp
components/transmission/miprtpalawdecoder.cpp
components/transmission/miprtpgsmdecoder.cpp
components/transmission/miprtplpcencoder.cpp
components/transmission/miprtph263encoder.cpp
components/transmission/miprtpspeexencoder.cpp
components/transmission/miprtpl16decoder.cpp
components/transmission/miprtpulawdecoder.cpp
components/transmission/miprtpsilkdecoder.cpp
components/transmission/miprtpsilkencoder.cpp
components/transmission/miprtpopusencoder.cpp
components/transmission/miprtpopusdecoder.cpp
components/transform/mipaudiosplitter.cpp
components/transform/mipspeexechocanceller.cpp
components/transform/mipaudiodistancefade.cpp
components/transform/mipaudiofilter.cpp
components/transform/mipavcodecframeconverter.cpp
components/transform/mipsamplingrateconverter.cpp
components/transform/mipsampleencoder.cpp
components/transform/miphrirlisten.cpp
components/transform/mipaudio3dbase.cpp
components/transform/mipyuv420framecutter.cpp
components/codec/mipavcodecdecoder.cpp
components/codec/mipulawdecoder.cpp
components/codec/mipavcodecencoder.cpp
components/codec/miplpcdecoder.cpp
components/codec/mipulawencoder.cpp
components/codec/mipspeexdecoder.cpp
components/codec/mipalawdecoder.cpp
components/codec/mipgsmdecoder.cpp
components/codec/miplpcencoder.cpp
components/codec/miptinyjpegdecoder.cpp
components/codec/mipalawencoder.cpp
components/codec/mipspeexencoder.cpp
components/codec/mipgsmencoder.cpp
components/codec/mipsilkdecoder.cpp
components/codec/mipsilkencoder.cpp
components/codec/mipopusencoder.cpp
components/codec/mipopusdecoder.cpp
components/util/mipoutputmessagequeue.cpp
sessions/mipvideosession.cpp
sessions/mipaudiosession.cpp
util/mipsignalwaiter.cpp
util/mipwavwriter.cpp
util/miprtppacketgrouper.cpp
util/mipdirectorybrowser.cpp
util/mipwavreader.cpp
util/mipspeexutil.cpp
util/miprtpsynchronizer.cpp
util/mipstreambuffer.cpp 
thirdparty/gsm/src/gsm_add.cpp
thirdparty/gsm/src/gsm_destroy.cpp
thirdparty/gsm/src/gsm_implode.cpp
thirdparty/gsm/src/gsm_print.cpp
thirdparty/gsm/src/gsm_encode.cpp
thirdparty/gsm/src/gsm_explode.cpp
thirdparty/gsm/src/gsm_create.cpp
thirdparty/gsm/src/gsm_decode2.cpp
thirdparty/gsm/src/gsm_option.cpp
thirdparty/gsm/src/gsm_code.cpp
thirdparty/gsm/src/gsm_lpc.cpp
thirdparty/gsm/src/gsm_decode.cpp
thirdparty/gsm/src/gsm_preprocess.cpp
thirdparty/gsm/src/gsm_debug.cpp
thirdparty/gsm/src/gsm_long_term.cpp
thirdparty/gsm/src/gsm_rpe.cpp
thirdparty/gsm/src/gsm_short_term.cpp
thirdparty/gsm/src/gsm_table.cpp
thirdparty/lpc/lpccodec.cpp
thirdparty/tinyjpeg/tinyjpeg.c
thirdparty/tinyjpeg/jidctflt.c )

if (EMIPLIB_SUPPORT_QT3)
	if (QT_FOUND)
		message(STATUS "Using Qt moc to generate mipqtoutput.moc")
		execute_process(COMMAND ${QT_MOC_EXECUTABLE} -o${PROJECT_BINARY_DIR}/mipqtoutput.moc ${PROJECT_SOURCE_DIR}/src/components/output/mipqtoutput.cpp)
	else (QT_FOUND)
		message("Qt3 Support was requested, but Qt3 could not be found. Apart from "
			"making sure that the header and libraries are listed in the additional "
			"headers and libaries section, the Qt3 'moc' utility has to be run on "
			"${PROJECT_SOURCE_DIR}/src/components/output/mipqtoutput.cpp to produce "
			"the file ${PROJECT_BINARY_DIR}/mipqtoutput.moc")
	endif (QT_FOUND)
endif (EMIPLIB_SUPPORT_QT3)

if (NOT UNIX AND WIN32)
	set(CMAKE_DEBUG_POSTFIX _d)
endif (NOT UNIX AND WIN32)

apply_include_paths("${EMIPLIB_INTERNAL_INCLUDES}")
apply_include_paths("${EMIPLIB_EXTERNAL_INCLUDES}")

if (UNIX OR EMIPLIB_COMPILE_STATIC)
	add_library(emiplib-static STATIC ${SOURCES} ${HEADERS})
	if (UNIX)
		set_target_properties(emiplib-static PROPERTIES OUTPUT_NAME emip)
	else (UNIX)
		set_target_properties(emiplib-static PROPERTIES OUTPUT_NAME emiplib)
	endif (UNIX)
	set_target_properties(emiplib-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
	set(EMIPLIB_INSTALLTARGETS emiplib-static)
endif (UNIX OR EMIPLIB_COMPILE_STATIC)

if (UNIX OR NOT EMIPLIB_COMPILE_STATIC)
	add_library(emiplib-shared SHARED ${SOURCES} ${HEADERS})
	set_target_properties(emiplib-shared PROPERTIES VERSION ${VERSION})
	if (UNIX)
		set_target_properties(emiplib-shared PROPERTIES OUTPUT_NAME emip)
	else (UNIX)
		set_target_properties(emiplib-shared PROPERTIES OUTPUT_NAME emiplib)
	endif (UNIX)
	set_target_properties(emiplib-shared PROPERTIES CLEAN_DIRECT_OUTPUT 1)
	set(EMIPLIB_INSTALLTARGETS ${EMIPLIB_INSTALLTARGETS} emiplib-shared)
	if (EMIPLIB_LINK)
		target_link_libraries(emiplib-shared ${EMIPLIB_LINK_LIBS})
	endif (EMIPLIB_LINK)
endif (UNIX OR NOT EMIPLIB_COMPILE_STATIC)

install(FILES ${HEADERS} DESTINATION include/emiplib)
install(TARGETS ${EMIPLIB_INSTALLTARGETS} DESTINATION ${LIBRARY_INSTALL_DIR})

