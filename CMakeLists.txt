cmake_minimum_required(VERSION 2.6)
cmake_policy(SET CMP0026 OLD)

project(emiplib)
set(VERSION 1.1.1)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")

set (_DEFAULT_LIBRARY_INSTALL_DIR lib)
if (EXISTS "${CMAKE_INSTALL_PREFIX}/lib32/" AND CMAKE_SIZEOF_VOID_P EQUAL 4)
	set (_DEFAULT_LIBRARY_INSTALL_DIR lib32)
elseif (EXISTS "${CMAKE_INSTALL_PREFIX}/lib64/" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
	set (_DEFAULT_LIBRARY_INSTALL_DIR lib64)
endif ()

set(LIBRARY_INSTALL_DIR "${_DEFAULT_LIBRARY_INSTALL_DIR}" CACHE PATH "Library installation directory")
if(NOT IS_ABSOLUTE "${LIBRARY_INSTALL_DIR}")
	set(LIBRARY_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${LIBRARY_INSTALL_DIR}")
endif()

include(CheckCXXCompilerFlag)
include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckCXXSourceCompiles)
include(TestBigEndian)
include("${PROJECT_SOURCE_DIR}/cmake/Macros.cmake")

if (NOT UNIX)
	set(EMIPLIB_COMPILE_STATIC ON CACHE BOOL "Flag indicating if a static library should be built, or a dynamic one")
endif (NOT UNIX)

if (UNIX)
	if (APPLE)
		set(TMP ON)
	else (APPLE)
		set(TMP OFF)
	endif (APPLE)
	option(EMIPLIB_LINK "Flag indicating if the shared library should be automatically linked against the other libraries used" ${TMP})
else (UNIX)
	if (EMIPLIB_COMPILE_STATIC)
		set(EMIPLIB_LINK FALSE)
	else (EMIPLIB_COMPILE_STATIC)
		set(EMIPLIB_LINK TRUE)
	endif (EMIPLIB_COMPILE_STATIC)
endif (UNIX)

set(EMIPLIB_INTERNAL_INCLUDES "")
set(EMIPLIB_EXTERNAL_INCLUDES "")
set(EMIPLIB_LINK_LIBS "")

add_additional_stuff(EMIPLIB_EXTERNAL_INCLUDES EMIPLIB_LINK_LIBS)

find_package(JThread REQUIRED)
find_package(JRTPLIB REQUIRED)

list(APPEND EMIPLIB_EXTERNAL_INCLUDES "${JTHREAD_INCLUDE_DIRS}")
list(APPEND EMIPLIB_EXTERNAL_INCLUDES "${JRTPLIB_INCLUDE_DIRS}")
list(APPEND EMIPLIB_LINK_LIBS "${JTHREAD_LIBRARIES}")
list(APPEND EMIPLIB_LINK_LIBS "${JRTPLIB_LIBRARIES}")

emiplib_support_option("Support for GPL components. This changes the license to GPL instead of LGPL."
	               EMIPLIB_GPL MIPCONFIG_GPL OFF "// No GPL components will be compiled")
emiplib_support_option("Support for the internal GSM codec" EMIPLIB_SUPPORT_GSM MIPCONFIG_SUPPORT_GSM ON "// No GSM support")
emiplib_support_option("Support for the internal LPC codec" EMIPLIB_SUPPORT_LPC MIPCONFIG_SUPPORT_LPC ON "// No LPC support")
emiplib_support_option("Support for Intel IPP library" EMIPLIB_SUPPORT_INTELIPP MIPCONFIG_SUPPORT_INTELIPP OFF "// No support for Intel IPP library")
emiplib_support_option("Support for SILK codec" EMIPLIB_SUPPORT_SILK MIPCONFIG_SUPPORT_SILK OFF "// No support for SILK codec")
emiplib_support_option("Support for Android AudioTrack output component" EMIPLIB_SUPPORT_AUDIOTRACK MIPCONFIG_SUPPORT_AUDIOTRACK OFF "// No support for AudioTrack output component")
emiplib_support_option("Support for Android AudioRecorder input component" EMIPLIB_SUPPORT_AUDIORECORDER MIPCONFIG_SUPPORT_AUDIORECORDER OFF "// No support for AudioRecorder input component")

emiplib_test_feature_option(osstest MIPCONFIG_SUPPORT_OSS "// No support for OSS" EMIPLIB_SUPPORT_OSS "Support for Open Sound System input/output")
emiplib_test_feature_option(v4ltest MIPCONFIG_SUPPORT_VIDEO4LINUX "// No support for Video4Linux" EMIPLIB_SUPPORT_VIDEO4LINUX "Support for Video4Linux input")

emiplib_test_feature_option(winmmtest MIPCONFIG_SUPPORT_WINMM "// No support for Win32 multimedia input/output" EMIPLIB_SUPPORT_WINMM "Support for Win32 multimedia input/output")
if (EMIPLIB_SUPPORT_WINMM)
	list(APPEND EMIPLIB_LINK_LIBS "winmm")
endif (EMIPLIB_SUPPORT_WINMM)

emiplib_support_option("Support for DirectShow" EMIPLIB_SUPPORT_DIRECTSHOW MIPCONFIG_SUPPORT_DIRECTSHOW OFF "// No support for DirectShow video input codec")

find_package(Speex)
if (SPEEX_FOUND)
	set(TMP ON)
else (SPEEX_FOUND)
	set(TMP OFF)
endif (SPEEX_FOUND)
emiplib_support_option("Support for Speex audio codec" EMIPLIB_SUPPORT_SPEEX MIPCONFIG_SUPPORT_SPEEX ${TMP} "// No Speex support")
if (EMIPLIB_SUPPORT_SPEEX AND SPEEX_FOUND)
	list(APPEND EMIPLIB_LINK_LIBS ${SPEEX_LIBRARIES})
	list(APPEND EMIPLIB_EXTERNAL_INCLUDES ${SPEEX_INCLUDE_DIRS})
endif (EMIPLIB_SUPPORT_SPEEX AND SPEEX_FOUND)

find_package(ALSA)
if (ALSA_FOUND)
	set(TMP ON)
else (ALSA_FOUND)
	set(TMP OFF)
endif (ALSA_FOUND)
emiplib_support_option("Support for ALSA" EMIPLIB_SUPPORT_ALSA MIPCONFIG_SUPPORT_ALSA ${TMP} "// No ALSA support")
if (EMIPLIB_SUPPORT_ALSA AND ALSA_FOUND)
	list(APPEND EMIPLIB_LINK_LIBS ${ALSA_LIBRARIES})
	list(APPEND EMIPLIB_EXTERNAL_INCLUDES ${ALSA_INCLUDE_DIRS})
endif(EMIPLIB_SUPPORT_ALSA AND ALSA_FOUND)

find_package(PortAudio)
if (PORTAUDIO_FOUND)
	set(TMP ON)
else (PORTAUDIO_FOUND)
	set(TMP OFF)
endif (PORTAUDIO_FOUND)
emiplib_support_option("Support for PortAudio input/output" EMIPLIB_SUPPORT_PORTAUDIO MIPCONFIG_SUPPORT_PORTAUDIO ${TMP} "// No PortAudio support")
if (EMIPLIB_SUPPORT_PORTAUDIO AND PORTAUDIO_FOUND)
	list(APPEND EMIPLIB_LINK_LIBS ${PORTAUDIO_LIBRARIES})
	list(APPEND EMIPLIB_EXTERNAL_INCLUDES ${PORTAUDIO_INCLUDE_DIRS})
endif (EMIPLIB_SUPPORT_PORTAUDIO AND PORTAUDIO_FOUND)

find_package(OpenAL)
if (OPENAL_FOUND)
	set(TMP ON)
else (OPENAL_FOUND)
	set(TMP OFF)
endif (OPENAL_FOUND)
emiplib_support_option("Support for OpenAL output" EMIPLIB_SUPPORT_OPENAL MIPCONFIG_SUPPORT_OPENAL ${TMP} "// No OpenAL output support")
if (EMIPLIB_SUPPORT_OPENAL AND OPENAL_FOUND)
	list(APPEND EMIPLIB_LINK_LIBS ${OPENAL_LIBRARY})
	list(APPEND EMIPLIB_EXTERNAL_INCLUDES ${OPENAL_INCLUDE_DIR})
endif (EMIPLIB_SUPPORT_OPENAL AND OPENAL_FOUND)

find_package(LibSndFile)
if (LIBSNDFILE_FOUND)
	set(TMP ON)
else (LIBSNDFILE_FOUND)
	set(TMP OFF)
endif (LIBSNDFILE_FOUND)
emiplib_support_option("Support for libsndfile audio file input/output" EMIPLIB_SUPPORT_SNDFILE MIPCONFIG_SUPPORT_SNDFILE ${TMP} "// No libsndfile support")
if (EMIPLIB_SUPPORT_SNDFILE AND LIBSNDFILE_FOUND)
	list(APPEND EMIPLIB_LINK_LIBS ${LIBSNDFILE_LIBRARIES})
	list(APPEND EMIPLIB_EXTERNAL_INCLUDES ${LIBSNDFILE_INCLUDE_DIRS})
endif (EMIPLIB_SUPPORT_SNDFILE AND LIBSNDFILE_FOUND)

find_package(AudioFile)
if (AUDIOFILE_FOUND)
	set(TMP ON)
else (AUDIOFILE_FOUND)
	set(TMP OFF)
endif (AUDIOFILE_FOUND)
emiplib_support_option("Support for libaudiofile audio file input" EMIPLIB_SUPPORT_AUDIOFILE MIPCONFIG_SUPPORT_AUDIOFILE ${TMP} "// No libaudiofile support")
if (EMIPLIB_SUPPORT_AUDIOFILE AND AUDIOFILE_FOUND)
	list(APPEND EMIPLIB_LINK_LIBS ${AUDIOFILE_LIBRARIES})
	list(APPEND EMIPLIB_EXTERNAL_INCLUDES ${AUDIOFILE_INCLUDE_DIRS})
endif (EMIPLIB_SUPPORT_AUDIOFILE AND AUDIOFILE_FOUND)

find_package(SDL)
if (SDL_FOUND)
	set(TMP ON)
else (SDL_FOUND)
	set(TMP OFF)
endif (SDL_FOUND)
emiplib_support_option("Support for SDL audio output" EMIPLIB_SUPPORT_SDLAUDIO MIPCONFIG_SUPPORT_SDLAUDIO ${TMP} "// No support for SDL audio output")
if (EMIPLIB_SUPPORT_SDLAUDIO AND SDL_FOUND)
	list(APPEND EMIPLIB_LINK_LIBS ${SDL_LIBRARY})
	list(APPEND EMIPLIB_INTERNAL_INCLUDES ${SDL_INCLUDE_DIR}) # Only needed in a cpp file
endif (EMIPLIB_SUPPORT_SDLAUDIO AND SDL_FOUND)

find_package(JACK)
if (JACK_FOUND)
	set(TMP ON)
else (JACK_FOUND)
	set(TMP OFF)
endif (JACK_FOUND)
emiplib_support_option("Support for JACK audio input/output" EMIPLIB_SUPPORT_JACK MIPCONFIG_SUPPORT_JACK ${TMP} "// No support for JACK audio input/output")
if (EMIPLIB_SUPPORT_JACK AND JACK_FOUND)
	list(APPEND EMIPLIB_LINK_LIBS ${JACK_LIBRARIES})
	list(APPEND EMIPLIB_EXTERNAL_INCLUDES ${JACK_INCLUDE_DIRS})
endif (EMIPLIB_SUPPORT_JACK AND JACK_FOUND)

find_package(ESound)
if (ESOUND_FOUND)
	set(TMP ON)
else (ESOUND_FOUND)
	set(TMP OFF)
endif (ESOUND_FOUND)
emiplib_support_option("Support for the Enlightened Sound Daemon based output" EMIPLIB_SUPPORT_ESD MIPCONFIG_SUPPORT_ESD ${TMP} "// No support for ESD based output")
if (EMIPLIB_SUPPORT_ESD AND ESOUND_FOUND)
	list(APPEND EMIPLIB_LINK_LIBS ${ESOUND_LIBRARIES})
	list(APPEND EMIPLIB_INTERNAL_INCLUDES ${ESOUND_INCLUDE_DIRS}) # Only needed in a cpp file
endif (EMIPLIB_SUPPORT_ESD AND ESOUND_FOUND)

find_package(LibAVCodec)
if (LIBAVCODEC_FOUND)
	set(TMP ON)
else (LIBAVCODEC_FOUND)
	set(TMP OFF)
endif (LIBAVCODEC_FOUND)
emiplib_support_option("Support for libavcodec for video compression/decompression" EMIPLIB_SUPPORT_AVCODEC MIPCONFIG_SUPPORT_AVCODEC ${TMP} "// No libavcodec support")
if (EMIPLIB_SUPPORT_AVCODEC AND LIBAVCODEC_FOUND)
	list(APPEND EMIPLIB_LINK_LIBS ${LIBAVCODEC_LIBRARIES})
	list(APPEND EMIPLIB_EXTERNAL_INCLUDES ${LIBAVCODEC_INCLUDE_DIRS})
endif (EMIPLIB_SUPPORT_AVCODEC AND LIBAVCODEC_FOUND)
if (LIBAVCODEC_FOUND AND LIBAVCODEC_OLD)
	set(TMP ON)
else ()
	set(TMP OFF)
endif ()
emiplib_support_option("Older libavcodec API" EMIPLIB_SUPPORT_AVCODEC_OLD MIPCONFIG_SUPPORT_AVCODEC_OLD ${TMP} "// Using a recent libavcodec version")

find_package(Qt3)
if (QT_FOUND)
	set(TMP ON)
else (QT_FOUND)
	set(TMP OFF)
endif (QT_FOUND)
emiplib_support_option("Support for Qt3 output component" EMIPLIB_SUPPORT_QT3 MIPCONFIG_SUPPORT_QT ${TMP} "// No support for Qt3 output component")
if (EMIPLIB_SUPPORT_QT3 AND QT_FOUND)
	list(APPEND EMIPLIB_LINK_LIBS ${QT_LIBRARIES})
	list(APPEND EMIPLIB_EXTERNAL_INCLUDES ${QT_INCLUDE_DIR})
endif (EMIPLIB_SUPPORT_QT3 AND QT_FOUND)

if (CMAKE_CROSSCOMPILING)
	option (EMIPLIB_USE_BIGENDIAN "Target platform is big endian" ON)
	if (EMIPLIB_USE_BIGENDIAN)
		set(MIPCONFIG_BIGENDIAN "#define MIPCONFIG_BIGENDIAN")
	else (EMIPLIB_USE_BIGENDIAN)
		set(MIPCONFIG_BIGENDIAN "// Little endian system")
	endif (EMIPLIB_USE_BIGENDIAN)
else (CMAKE_CROSSCOMPILING)
	test_big_endian(EMIPLIB_BIGENDIAN)
	if (EMIPLIB_BIGENDIAN)
		set(MIPCONFIG_BIGENDIAN "#define MIPCONFIG_BIGENDIAN")
	else (EMIPLIB_BIGENDIAN)
		set(MIPCONFIG_BIGENDIAN "// Little endian system")
	endif (EMIPLIB_BIGENDIAN)
endif (CMAKE_CROSSCOMPILING)

set(EMIPLIB_INTERNAL_INCLUDES ${EMIPLIB_INTERNAL_INCLUDES}
	"${PROJECT_SOURCE_DIR}/src/core"
	"${PROJECT_BINARY_DIR}/"
	"${PROJECT_BINARY_DIR}/src/core"
	"${PROJECT_SOURCE_DIR}/src/components/input"
	"${PROJECT_SOURCE_DIR}/src/components/output"
	"${PROJECT_SOURCE_DIR}/src/components/output"
	"${PROJECT_SOURCE_DIR}/src/components/timer"
	"${PROJECT_SOURCE_DIR}/src/components/mixer"
	"${PROJECT_SOURCE_DIR}/src/components/io"
	"${PROJECT_SOURCE_DIR}/src/components/transmission"
	"${PROJECT_SOURCE_DIR}/src/components/transform"
	"${PROJECT_SOURCE_DIR}/src/components/codec"
	"${PROJECT_SOURCE_DIR}/src/sessions"
	"${PROJECT_SOURCE_DIR}/src/util"
	"${PROJECT_SOURCE_DIR}/src/thirdparty/gsm/inc"
	"${PROJECT_SOURCE_DIR}/src/thirdparty/lpc"
	"${PROJECT_SOURCE_DIR}/src/thirdparty/tinyjpeg" )

check_cxx_source_compiles("#include <stdint.h>\nint main(void) { uint32_t x = 0; uint32_t y = x; return 0; }" EMIPLIB_STDINT)
if (EMIPLIB_STDINT)
	set(EMIPLIB_INTTYPE_HEADERS "#include <stdint.h>\n")
else (EMIPLIB_STDINT)
	check_cxx_source_compiles("#include <inttypes.h>\nint main(void) { uint32_t x = 0; uint32_t y = x; return 0; }" EMIPLIB_INTTYPES)
	if (EMIPLIB_INTTYPES)
		set(EMIPLIB_INTTYPE_HEADERS "#include <inttypes.h>\n")
	else (EMIPLIB_INTTYPES)
		if (NOT UNIX AND WIN32)
			set(EMIPLIB_INTTYPE_HEADERS "#include \"miptypes_win.h\"")
		else (NOT UNIX AND WIN32)
			set(EMIPLIB_INTTYPE_HEADERS "#error Could not find header files that define types like 'uint32_t'. Please edit the file ${PROJECT_BINARY_DIR}/src/core/miptypes.h and make sure that the following types are defined: int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t")
			message("\n\nError: Could not find header files that define types like 'uint32_t'.\nPlease edit the file ${PROJECT_BINARY_DIR}/src/core/miptypes.h\nand make sure that the following types are defined: \nint8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t\n\n")
		endif (NOT UNIX AND WIN32)
	endif (EMIPLIB_INTTYPES)
endif (EMIPLIB_STDINT)

if (UNIX OR EMIPLIB_COMPILE_STATIC)
	set(EMIPLIB_IMPORT "")
	set(EMIPLIB_EXPORT "")
else (UNIX OR EMIPLIB_COMPILE_STATIC)
	set(EMIPLIB_IMPORT "__declspec(dllimport)")
	set(EMIPLIB_EXPORT "__declspec(dllexport)")
endif (UNIX OR EMIPLIB_COMPILE_STATIC)

configure_file("${PROJECT_SOURCE_DIR}/src/core/mipconfig.h.in" "${PROJECT_BINARY_DIR}/src/core/mipconfig.h")
configure_file("${PROJECT_SOURCE_DIR}/src/core/miptypes.h.in" "${PROJECT_BINARY_DIR}/src/core/miptypes.h")

add_subdirectory(src)
add_subdirectory(examples)

if (UNIX)
	get_target_property(EMIPLIB_LOCAL_LIBRARY_NAME emiplib-shared LOCATION)
	get_filename_component(EMIPLIB_LIBNAME "${EMIPLIB_LOCAL_LIBRARY_NAME}" NAME)
	set(EMIPLIB_LIBS "${LIBRARY_INSTALL_DIR}/${EMIPLIB_LIBNAME}")
else (UNIX)
	if (EMIPLIB_COMPILE_STATIC)
		get_target_property(EMIPLIB_LOCAL_LIBRARY_NAME emiplib-static RELEASE_LOCATION)
		get_filename_component(EMIPLIB_LIBNAME_RELEASE "${EMIPLIB_LOCAL_LIBRARY_NAME}" NAME_WE)
		get_target_property(EMIPLIB_LOCAL_LIBRARY_NAME emiplib-static DEBUG_LOCATION)
		get_filename_component(EMIPLIB_LIBNAME_DEBUG "${EMIPLIB_LOCAL_LIBRARY_NAME}" NAME_WE)
	else (EMIPLIB_COMPILE_STATIC)
		get_target_property(EMIPLIB_LOCAL_LIBRARY_NAME emiplib-shared RELEASE_LOCATION)
		get_filename_component(EMIPLIB_LIBNAME_RELEASE "${EMIPLIB_LOCAL_LIBRARY_NAME}" NAME_WE)
		get_target_property(EMIPLIB_LOCAL_LIBRARY_NAME emiplib-shared DEBUG_LOCATION)
		get_filename_component(EMIPLIB_LIBNAME_DEBUG "${EMIPLIB_LOCAL_LIBRARY_NAME}" NAME_WE)
	endif (EMIPLIB_COMPILE_STATIC)
	set(EMIPLIB_LIBS optimized "${LIBRARY_INSTALL_DIR}/${EMIPLIB_LIBNAME_RELEASE}.lib" 
		debug "${LIBRARY_INSTALL_DIR}/${EMIPLIB_LIBNAME_DEBUG}.lib")
endif (UNIX)

set(EMIPLIB_INCDIRS ${EMIPLIB_EXTERNAL_INCLUDES} ${CMAKE_INSTALL_PREFIX}/include)
set(EMIPLIB_LIBS ${EMIPLIB_LIBS} ${EMIPLIB_LINK_LIBS})
remove_empty(EMIPLIB_INCDIRS)
list(REMOVE_DUPLICATES EMIPLIB_INCDIRS)
remove_empty(EMIPLIB_LIBS) # Note that we can't remove duplicates because of multiple 'optimized' or 'debug' options in Win32 version

foreach(ARG ${EMIPLIB_LIBS})
	set(EMIPLIB_LIBS_CMAKECONFIG "${EMIPLIB_LIBS_CMAKECONFIG} \"${ARG}\"")
endforeach()
foreach(ARG ${EMIPLIB_INCDIRS})
	set(EMIPLIB_INCDIRS_CMAKECONFIG "${EMIPLIB_INCDIRS_CMAKECONFIG} \"${ARG}\"")
endforeach()

configure_file("${PROJECT_SOURCE_DIR}/cmake/EMIPLIBConfig.cmake.in" "${PROJECT_BINARY_DIR}/cmake/EMIPLIBConfig.cmake")
install(FILES "${PROJECT_BINARY_DIR}/cmake/EMIPLIBConfig.cmake" DESTINATION ${LIBRARY_INSTALL_DIR}/cmake/EMIPLIB)

if (UNIX)
	foreach(ARG ${EMIPLIB_LIBS})
		set(EMIPLIB_LIBS_PKGCONFIG "${EMIPLIB_LIBS_PKGCONFIG} ${ARG}")
	endforeach()
	foreach(ARG ${EMIPLIB_INCDIRS})
		set(EMIPLIB_INCDIRS_PKGCONFIG "${EMIPLIB_INCDIRS_PKGCONFIG} -I${ARG}")
	endforeach()

	configure_file(${PROJECT_SOURCE_DIR}/pkgconfig/emiplib.pc.in ${PROJECT_BINARY_DIR}/pkgconfig/emiplib.pc)
	install(FILES ${PROJECT_BINARY_DIR}/pkgconfig/emiplib.pc DESTINATION ${LIBRARY_INSTALL_DIR}/pkgconfig)
endif (UNIX)

if (EMIPLIB_GPL)
	message("License: GPL\n\n"
	"You have specified that you want to use components licensed under the terms\n"
	"of the GPL. By specifying this option, the GPL license also applies to this\n"
	"library. The complete text of the GPL license can be found in the file\n"
	"LICENSE.GPL included in the source code archive.\n")
else (EMIPLIB_GPL)
	message("License: LGPL\n\n"
	"The library is licensed under the terms of the LGPL, the full text can be\n"
	"found in the file LICENSE.LGPL. However, depending on the libraries your\n"
	"application links with, it may be possible that another license has to be\n"
	"taken into account as well. For example, if the libavcodec library you link\n"
	"with was compiled as a GPL library, then your application has to respect the\n"
	"terms of the GPL, even though it may only make use of libavcodec indirectly\n"
	"(by using the EMIPLIB libavcodec component). Similarly, if you use the Qt\n"
	"library with a GPL license, linking with the Qt library will cause your\n"
	"application to be GPL as well.\n")
endif (EMIPLIB_GPL)
	
