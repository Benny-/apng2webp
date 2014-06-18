cmake_minimum_required (VERSION 2.8)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

project (apng2webp_dependecies)
set (apng2webp_dependecies_VERSION_MAJOR 1)
set (apng2webp_dependecies_VERSION_MINOR 0)

add_executable(apng2webp_apngopt apng2webp_apngopt/apngopt.c)
add_executable(apngdisraw apngdisraw/apngdis.cpp)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries( apng2webp_apngopt ${ZLIB_LIBRARIES} )

find_package(Jsoncpp REQUIRED)
include_directories(${Jsoncpp_INCLUDE_DIRS})
target_link_libraries(apngdisraw ${Jsoncpp_LIBRARIES})

find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIRS})
target_link_libraries(apngdisraw ${PNG_LIBRARIES})

install (TARGETS apng2webp_apngopt apngdisraw DESTINATION bin)

