function(add_shader TARGET SHADER)

	# Find glslc shader compiler.
	find_program(GLSLC glslc)

	# All shaders for a sample are found here.
	set(current-shader-path ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER})

	# Write SPIR-V files and output in the binary directory.
    set(current-output-path ${CMAKE_CURRENT_BINARY_DIR}/${SHADER}.spv)

    # message(STATUS "shader: ${current-shader-path}")
    # message(STATUS "shader: ${current-output-path}")

	# Add a custom command to compile GLSL to SPIR-V.
	get_filename_component(current-output-dir ${current-output-path} DIRECTORY)
	file(MAKE_DIRECTORY ${current-output-dir})
	add_custom_command(
		OUTPUT ${current-output-path}
		COMMAND ${GLSLC} -o ${current-output-path} ${current-shader-path}
		DEPENDS ${current-shader-path}
		IMPLICIT_DEPENDS CXX ${current-shader-path}
		VERBATIM)

	# Make sure our build depends on this output.
	set_source_files_properties(${current-output-path} PROPERTIES GENERATED TRUE)
	target_sources(${TARGET} PRIVATE ${current-output-path})
endfunction(add_shader)

function(add_all_shader TARGET)
    # Find all shaders.
    file(GLOB vertex-shaders ${CMAKE_CURRENT_SOURCE_DIR}/*.vert)
    file(GLOB fragment-shaders ${CMAKE_CURRENT_SOURCE_DIR}/*.frag)
    file(GLOB compute-shaders ${CMAKE_CURRENT_SOURCE_DIR}/*.comp)

    # Add them to the build.
    foreach(vertex-shader ${vertex-shaders})
        get_filename_component(p ${vertex-shader} NAME)
        add_shader(${TARGET} ${p})
    endforeach(vertex-shader)

    foreach(fragment-shader ${fragment-shaders})
        get_filename_component(p ${fragment-shader} NAME)
        add_shader(${TARGET} ${p})
    endforeach(fragment-shader)

    foreach(compute-shader ${compute-shaders})
        get_filename_component(p ${compute-shader} NAME)
        add_shader(${TARGET} ${p})
    endforeach(compute-shader)
endfunction(add_all_shader)