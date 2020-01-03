cmake_minimum_required(VERSION 3.0)
project(lwstest)

list(APPEND add_cppflags -Wall -Wextra -std=c++17 -g)
list(APPEND add_libraries pthread websockets event)

file(GLOB project_sources ./*.h ./*.cpp)
add_executable(lwstest ${project_sources})

target_compile_options(lwstest PRIVATE ${add_cppflags})
target_link_libraries(lwstest  ${add_libraries})

if(UNIX)
    install (TARGETS lwstest RUNTIME DESTINATION /usr/bin)
endif()