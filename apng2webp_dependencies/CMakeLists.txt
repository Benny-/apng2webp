cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

project(apng2webp_dependecies)
set(apng2webp_dependecies_VERSION_MAJOR 1)
set(apng2webp_dependecies_VERSION_MINOR 0)

add_compile_options(-std=c++11)

add_executable(apng2webp_apngopt apng2webp_apngopt/apngopt.cpp)
add_executable(apngdisraw apngdisraw/apngdis.cpp)

if(STATIC_LINKING)
# Windows MinGW use actually statically linked binary
    if (MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "{$CMAKE_EXE_LINKER_FLAGS} -static")
        set_target_properties(apng2webp_apngopt PROPERTIES LINK_SEARCH_START_STATIC ON)
        set_target_properties(apng2webp_apngopt PROPERTIES LINK_SEARCH_END_STATIC ON)
        set_target_properties(apngdisraw PROPERTIES LINK_SEARCH_START_STATIC ON)
        set_target_properties(apngdisraw PROPERTIES LINK_SEARCH_END_STATIC ON)
    endif(MINGW)
    set(BUILD_SHARED_LIBRARIES OFF)
endif(STATIC_LINKING)

if(STATIC_LINKING)
# OS X provide system zlib .dylib but not static .a
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        find_package(ZLIB REQUIRED)
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    else()
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
        find_package(ZLIB REQUIRED)
    endif()
else(STATIC_LINKING)
    find_package(ZLIB REQUIRED)
endif(STATIC_LINKING)
find_package(PNG REQUIRED)
find_package(Jsoncpp REQUIRED)

include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${PNG_INCLUDE_DIRS})
include_directories(${Jsoncpp_INCLUDE_DIRS})

target_link_libraries(apng2webp_apngopt ${ZLIB_LIBRARIES})
target_link_libraries(apng2webp_apngopt ${PNG_LIBRARIES})
target_link_libraries(apngdisraw ${PNG_LIBRARIES})
target_link_libraries(apngdisraw ${Jsoncpp_LIBRARIES})

install(TARGETS apng2webp_apngopt apngdisraw DESTINATION bin)
