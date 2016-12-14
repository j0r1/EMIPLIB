apply_include_paths("${EMIPLIB_INTERNAL_INCLUDES}")
apply_include_paths("${EMIPLIB_EXTERNAL_INCLUDES}")

foreach(IDX avsession feedbackexample multiplesoundfileplayer simplechain soundfileplayer soundrecorder pushtotalk
	    pushtotalk2)
	add_executable(${IDX} ${IDX}.cpp)
	target_link_libraries(${IDX} emiplib-static ${EMIPLIB_LINK_LIBS})
endforeach(IDX)

