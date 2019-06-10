cmake_minimum_required(VERSION 3.7)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/") 

project(libFirmwareUpdate++)

file(GLOB_RECURSE FirmwareUpdate_src FOLLOW_SYMLINKS src/*.cpp src/*.c src/*.hpp src/*.h include/*.hpp)

add_library(FirmwareUpdate++ ${FirmwareUpdate_src})
target_include_directories(FirmwareUpdate++
    PUBLIC 
        $<INSTALL_INTERFACE:include>    
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(FirmwareUpdate++ PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
)
target_compile_options(FirmwareUpdate++ PRIVATE $<$<CXX_COMPILER_ID:GNU>:-O2 -Wall -Wextra>)

find_package(LibUSB REQUIRED)
target_link_libraries(FirmwareUpdate++ PRIVATE ${LibUSB_LIBRARIES})
target_include_directories(FirmwareUpdate++ PRIVATE ${LibUSB_INCLUDE_DIRS})
# LibUSB_HEADER_FILE not currently used

include(CheckFunctionExists)
check_function_exists(getpagesize HAVE_GETPAGESIZE)
if(HAVE_GETPAGESIZE)
	target_compile_definitions(FirmwareUpdate++ PRIVATE -DHAVE_GETPAGESIZE)
endif()
