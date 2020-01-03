cmake_minimum_required(VERSION 3.0)
project(feedproxy)

set(FEED_PROXY_DIRS ${PROJECT_SOURCE_DIR}/)

add_subdirectory(proto)
add_subdirectory(convertservice)