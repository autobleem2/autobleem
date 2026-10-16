# ci/prefix_map.cmake - included after every project() by ci/build.sh (CMAKE_PROJECT_INCLUDE).
# The source and build directories differ per clone and per container mount, and their absolute paths end up
# in __FILE__, the line markers and the debug info - so sccache saw "different" sources and missed. Map both
# to "." (the build directory last: the later map wins for the paths below it). add_compile_options is used
# rather than CMAKE_<LANG>_FLAGS because the toolchain files overwrite those.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	set(_ab_maps "${CMAKE_SOURCE_DIR}")
	if(AB_EXTENSION_DIRS)
		foreach(_d IN LISTS AB_EXTENSION_DIRS)
			list(APPEND _ab_maps "${_d}")
		endforeach()
	endif()
	list(APPEND _ab_maps "${CMAKE_BINARY_DIR}")
	foreach(_d IN LISTS _ab_maps)
		add_compile_options(
			"$<$<COMPILE_LANGUAGE:C,CXX>:-ffile-prefix-map=${_d}=.>"
			"$<$<COMPILE_LANGUAGE:C,CXX>:-fdebug-prefix-map=${_d}=.>")
	endforeach()
endif()
