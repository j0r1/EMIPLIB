
find_path(ESOUND_INCLUDE_DIR esd.h)

set(ESOUND_INCLUDE_DIRS ${ESOUND_INCLUDE_DIR})

find_library(ESOUND_LIBRARY esd)
if (ESOUND_LIBRARY)
	set(ESOUND_LIBRARIES ${ESOUND_LIBRARY})
endif (ESOUND_LIBRARY)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(ESound DEFAULT_MSG ESOUND_INCLUDE_DIRS ESOUND_LIBRARIES)

