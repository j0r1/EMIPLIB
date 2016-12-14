
find_path(JACK_INCLUDE_DIR jack/jack.h)

set(JACK_INCLUDE_DIRS ${JACK_INCLUDE_DIR})

find_library(JACK_LIBRARY jack)
if (JACK_LIBRARY)
	set(JACK_LIBRARIES ${JACK_LIBRARY})
endif (JACK_LIBRARY)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(JACK DEFAULT_MSG JACK_INCLUDE_DIRS JACK_LIBRARIES)

