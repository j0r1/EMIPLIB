apply_include_paths("${EMIPLIB_INTERNAL_INCLUDES}")
apply_include_paths("${EMIPLIB_EXTERNAL_INCLUDES}")

macro(linkit TARGET)
	if (NOT MSVC OR EMIPLIB_COMPILE_STATIC)
		target_link_libraries(${TARGET} emiplib-static ${EMIPLIB_LINK_LIBS} ${ARGN})
	else ()
		target_link_libraries(${TARGET} emiplib-shared ${EMIPLIB_LINK_LIBS} ${ARGN})
	endif ()
endmacro()

foreach(IDX pulseouttest portaudioouttest replayaudio qtouttest audiocodectest delayedchainstarttest streamopus streamopusrecv
            streamopusrecv2 alsaouttest alsaintest)
	add_executable(${IDX} ${IDX}.cpp)
	linkit(${IDX})
endforeach(IDX)

if (EMIPLIB_SUPPORT_QT5)
	find_package(Qt5Multimedia)
	if (Qt5Multimedia_FOUND)
		include_directories(${Qt5Multimedia_INCLUDE_DIRS})

		add_executable(qtaudiooutput qtaudiooutput.cpp)
		linkit(qtaudiooutput ${Qt5Multimedia_LIBRARIES})
	endif()
endif()


