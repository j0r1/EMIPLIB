noinst_PROGRAMS = simplechain soundfileplayer feedbackexample avsession soundrecorder
simplechain_SOURCES = simplechain.cpp
soundfileplayer_SOURCES = soundfileplayer.cpp
feedbackexample_SOURCES = feedbackexample.cpp
avsession_SOURCES = avsession.cpp
soundrecorder_SOURCES = soundrecorder.cpp
INCLUDES = -I ../src/core -I ../src/components/input -I ../src/components/output -I ../src/components/timer \
           -I ../src/components/transform -I ../src/components/io -I ../src/util -I ../src/components/mixer \
	   -I ../src/components/transmission -I ../src/sessions -I ../src/components/codec @MIPINCLUDES@
LDADD = ../src/.libs/libemip.a @MIPLINKLIBS@
