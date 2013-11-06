cmake_minimum_required(VERSION 2.8)

project(get_data)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)
add_executable(get_data main.cpp)
set_target_properties(get_data PROPERTIES COMPILE_FLAGS "-DHAVE_USB -DHAVE_LIBUSB10 -g")
target_link_libraries(get_data drs usb-1.0 z)

install(TARGETS get_data RUNTIME DESTINATION bin)
