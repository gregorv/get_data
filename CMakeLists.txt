cmake_minimum_required(VERSION 2.8)

project(get_data)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(ENABLE_PROFILING OFF CACHE BOOL "Enable gprof compile flags")

find_package(ROOT)
find_package(LibUSB REQUIRED)
find_package(ZLIB REQUIRED)

set(GET_DATA_SRC main.cpp detectorcontrol.cpp)
if(${ROOT_FOUND})
    set(GET_DATA_SRC ${GET_DATA_SRC} rootoutput.cpp)
endif(${ROOT_FOUND})

if(${ENABLE_PROFILING})
   set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -p")
endif(${ENABLE_PROFILING})


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

execute_process(COMMAND git describe
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GITDESC_VERSION_STRING
                OUTPUT_STRIP_TRAILING_WHITESPACE
                )

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configuration.h.in ${CMAKE_CURRENT_BINARY_DIR}/configuration.h)

include_directories(${LIBUSB_INCLUDE_DIR}
                    ${ROOT_INCLUDE_DIR}
                    ${ZLIB_INCLUDE_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR})
link_directories(${ROOT_LIBRARY_PATH} ${LIBUSB_LIBRARY_PATH})
add_executable(get_data ${GET_DATA_SRC})
set_target_properties(get_data PROPERTIES COMPILE_FLAGS "-DHAVE_USB -DHAVE_LIBUSB10 -g")
target_link_libraries(get_data drs ${ZLIB_LIBRARIES} ${ROOT_LIBRARIES} ${LIBUSB_LIBRARIES})

install(TARGETS get_data RUNTIME DESTINATION bin)
