cmake_minimum_required(VERSION 3.0)
project(examples)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

message(STATUS "Operation system is ${CMAKE_SYSTEM}")

# -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
# set(ALL_LIBS glfw3 vulkan GL Xrandr Xi X11 pthread dl)
set(ALL_LIBS glfw3 vulkan dl pthread X11 Xxf86vm Xrandr Xi)

include_directories(thirdparty/stb_image)
include_directories(thirdparty/glm)
# include_directories(src/)

include(cmake/shader.cmake)

add_subdirectory(triangle)

