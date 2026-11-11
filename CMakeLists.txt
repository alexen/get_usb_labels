cmake_minimum_required(VERSION 3.12)

project(get_usb_labels LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -O0 -fno-inline")

find_package(Boost REQUIRED
  COMPONENTS
    filesystem
    system
)

add_executable(get_usb_labels main.cpp)

target_link_libraries(get_usb_labels
  PRIVATE
    Boost::filesystem
    Boost::system
)
