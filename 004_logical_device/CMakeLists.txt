set(PROGRAM_NAME 004)

set(TEST_SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set(TEST_BIN_PATH "${CMAKE_CURRENT_BINARY_DIR}")
configure_file (
  "${PROJECT_SOURCE_DIR}/config.h.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/config.h"
  )

# Add program
aux_source_directory(./ SRC)
add_executable(${PROGRAM_NAME} ${SRC})
target_link_libraries(${PROGRAM_NAME} ${ALL_LIBS})

add_all_shader(${PROGRAM_NAME})
