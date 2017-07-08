cmake_minimum_required (VERSION 2.8)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

project (apng2webp_dependecies)
set (apng2webp_dependecies_VERSION_MAJOR 1)
set (apng2webp_dependecies_VERSION_MINOR 0)

set (7Z_ROOT_DIR apng2webp_apngopt/7z)
set (ZOPFLI_ROOT_DIR apng2webp_apngopt/zopfli)

file (GLOB 7Z_LIBRARIES_DIRS
    ${7Z_ROOT_DIR}/*.cc
)
file (GLOB ZOPFLI_LIBRARIES_DIRS
    ${ZOPFLI_ROOT_DIR}/*.c
)
file (GLOB APNG2WEBP_APNGOPT_LIBRARIES_DIRS
    apng2webp_apngopt/apngopt.cpp
    ${7Z_LIBRARIES_DIRS}
    ${ZOPFLI_LIBRARIES_DIRS}
)
file (GLOB APNGDISRAW_LIBRARIES_DIRS
    apngdisraw/apngdis.cpp
)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIRS})
find_package(Jsoncpp REQUIRED)
include_directories(${Jsoncpp_INCLUDE_DIRS})
include_directories(${7Z_ROOT_DIR})
include_directories(${ZOPFLI_ROOT_DIR})

add_executable(apng2webp_apngopt ${APNG2WEBP_APNGOPT_LIBRARIES_DIRS})
add_executable(apngdisraw ${APNGDISRAW_LIBRARIES_DIRS})

target_link_libraries(apng2webp_apngopt ${ZLIB_LIBRARIES})
target_link_libraries(apng2webp_apngopt ${PNG_LIBRARIES})
target_link_libraries(apngdisraw ${Jsoncpp_LIBRARIES})
target_link_libraries(apngdisraw ${PNG_LIBRARIES})

install (TARGETS apng2webp_apngopt apngdisraw DESTINATION bin)

