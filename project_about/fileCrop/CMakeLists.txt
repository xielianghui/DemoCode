cmake_minimum_required(VERSION 3.0)
project(filecrop)

list(APPEND add_cppflags -fvisibility=hidden)
list(APPEND add_cppflags -Wall -Wextra)
list(APPEND add_cppflags -std=c++11)
list(APPEND add_cppflags -O0 -g)
list(APPEND add_cppflags -Wno-unused-but-set-variable)
list(APPEND add_cppflags -Wno-unused-parameter)
list(APPEND add_includes "/usr/include/")

link_directories(/usr/lib)
list(APPEND add_libraries dl uv pthread)

set(filecrop_sources
    ./base_task.h
    ./base_task.cpp
    ./col_task.h
    ./col_task.cpp
    ./code_task.h
    ./code_task.cpp
    ./date_task.h
    ./date_task.cpp
    ./task_dispatcher.h
    ./task_dispatcher.cpp
    ./main.cpp)

add_executable(filecrop ${filecrop_sources})
target_compile_options(filecrop PRIVATE ${add_cppflags})
target_include_directories(filecrop PRIVATE ${add_includes})
target_link_libraries(filecrop ${add_libraries})


install (TARGETS filecrop RUNTIME DESTINATION /usr/bin)

