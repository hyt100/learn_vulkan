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

include_directories(3rd-party/stb_image)
include_directories(3rd-party/glm)
# include_directories(src/)

add_subdirectory(test_triangle)
