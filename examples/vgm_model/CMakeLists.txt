# \author Sergio Agostinho - sergio(dot)r(dot)agostinho(at)gmail(dot)com
# \data created: 2017/07/27
# \data last modified: 2017/08/17

set (EXAMPLE_NAME "vgm_model")
set (TARGET_NAME "${TARGET_NAME_PREFIX}_${EXAMPLE_NAME}")
cmake_dependent_option (BUILD_${MODULE_NAME}_${EXAMPLE_NAME}
													"Enables the compilation of the ${EXAMPLE_NAME} example." ON
													"BUILD_${MODULE_NAME};BUILD_io;BUILD_filter;Matlab_FOUND" OFF)

#target
if (BUILD_${MODULE_NAME}_${EXAMPLE_NAME})
	add_executable (${TARGET_NAME} "vgm_model.cpp")
	target_link_libraries (${TARGET_NAME}
														"${PROJECT_NAME}_io"
														"${PROJECT_NAME}_filter"
														${Matlab_LIBRARIES})
	target_include_directories (${TARGET_NAME}
																PRIVATE ${Matlab_INCLUDE_DIRS})

	# install rules
	if (INSTALL_examples)
		install (TARGETS ${TARGET_NAME} RUNTIME DESTINATION "bin")
	endif (INSTALL_examples)
endif (BUILD_${MODULE_NAME}_${EXAMPLE_NAME})
