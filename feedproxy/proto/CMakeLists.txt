cmake_minimum_required(VERSION 3.0)
project(convertproto)

## generate cpp 
execute_process(COMMAND sh ${PROJECT_SOURCE_DIR}/build_cpp.sh ${PROJECT_SOURCE_DIR})

list(APPEND add_cppflags -O0 -g)
list(APPEND add_cppflags --std=c++17)
list(APPEND add_cppflags -Wall -Wextra)
list(APPEND add_cppflags -Wno-unused-but-set-variable)
list(APPEND add_cppflags -Wno-unused-parameter)
list(APPEND add_libraries pthread protobuf)

file(GLOB project_sources ${PROJECT_SOURCE_DIR}/*.proto
                          ${PROJECT_SOURCE_DIR}/gen_cpp/*.pb.h
                          ${PROJECT_SOURCE_DIR}/gen_cpp/*.pb.cc)

add_library(convertproto SHARED ${project_sources})


target_compile_options(convertproto PRIVATE ${add_cppflags})
target_include_directories(convertproto PRIVATE ${add_includes})
target_link_libraries(convertproto ${add_libraries})

if(UNIX)
  install (TARGETS convertproto LIBRARY DESTINATION /usr/lib/)
endif()