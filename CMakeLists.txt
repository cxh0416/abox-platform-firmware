cmake_minimum_required(VERSION 3.22)

project(ABoxPlatform VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_library(abox_core STATIC
    src/abox_platform_config.c
    src/abox_platform_port.c
)
add_library(abox::core ALIAS abox_core)

target_include_directories(abox_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(abox_core
    PUBLIC
        ABOX_PLATFORM_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        ABOX_PLATFORM_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        ABOX_PLATFORM_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

add_library(abox_cjson STATIC
    components/cjson/cJSON.c
)
add_library(abox::cjson ALIAS abox_cjson)
target_include_directories(abox_cjson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/components/cjson)

add_library(abox_ota STATIC
    components/ota/ota_flash.c
)
add_library(abox::ota ALIAS abox_ota)
target_include_directories(abox_ota PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(abox_ota PUBLIC abox::core)

add_library(abox_boot INTERFACE)
add_library(abox::boot ALIAS abox_boot)
target_link_libraries(abox_boot INTERFACE abox::core)

add_library(abox_port_baremetal INTERFACE)
add_library(abox::port_baremetal ALIAS abox_port_baremetal)
target_link_libraries(abox_port_baremetal INTERFACE abox::core)
target_compile_definitions(abox_port_baremetal INTERFACE ABOX_PLATFORM_SCHEDULER_BAREMETAL=1)

add_library(abox_port_freertos INTERFACE)
add_library(abox::port_freertos ALIAS abox_port_freertos)
target_link_libraries(abox_port_freertos INTERFACE abox::core)
target_compile_definitions(abox_port_freertos INTERFACE ABOX_PLATFORM_SCHEDULER_FREERTOS=1)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ABoxPlatform.cmake)

include(CTest)
if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_executable(abox_platform_config_test tests/abox_platform_config_test.c)
    target_link_libraries(abox_platform_config_test PRIVATE abox::core)
    add_test(NAME abox_platform_config_test COMMAND abox_platform_config_test)
endif()
