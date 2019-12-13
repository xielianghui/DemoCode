cmake_minimum_required(VERSION 3.0)
project(convertservice)

list(APPEND add_cppflags -Wall -Wextra -std=c++17 -g)
list(APPEND add_libraries pthread websockets event jsoncpp)

include_directories(/usr/local/include ./levservice)
file(GLOB project_sources ./*.h ./*.cpp ./levservice/ev_loop.cpp ./lwsclient/lws_client.cpp)
add_executable(convertservice ${project_sources})

target_compile_options(convertservice PRIVATE ${add_cppflags})
target_link_libraries(convertservice  ${add_libraries})

if(UNIX)
    install (TARGETS convertservice RUNTIME DESTINATION /usr/bin)
endif()