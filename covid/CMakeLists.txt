# CMakeList.txt : CMake project for covid, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
add_library(${PROJECT_NAME}   "socket.h" "socket.cpp" )
target_link_libraries(${PROJECT_NAME} Ws2_32.lib)

function(addDirectory dir)
	add_subdirectory(${dir})
	include_directories(${dir})
	target_link_libraries(${PROJECT_NAME} ${dir})
	target_include_directories(${PROJECT_NAME} PUBLIC ${dir})
endfunction()
