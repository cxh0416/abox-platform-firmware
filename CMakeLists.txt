cmake_minimum_required(VERSION 3.22)

project(ABoxPlatform VERSION 0.2.3 LANGUAGES C)

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
    components/ota/ota_ec_at.c
)
add_library(abox::ota ALIAS abox_ota)
target_include_directories(abox_ota PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(abox_ota PUBLIC abox::core)

add_library(abox_ec800 STATIC
    components/ec800/abox_ec800_at.c
    components/ec800/abox_ec800_at_legacy.c
    components/ec800/abox_ec800_rx.c
)
add_library(abox::ec800 ALIAS abox_ec800)
target_include_directories(abox_ec800 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(abox_https_ufs_downloader STATIC
    components/ota/abox_https_ufs_downloader.c
)
add_library(abox::https_ufs_downloader ALIAS abox_https_ufs_downloader)
target_include_directories(abox_https_ufs_downloader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(abox_ec_power STATIC
    components/ec_power/ec_power.c
)
add_library(abox::ec_power ALIAS abox_ec_power)
target_include_directories(abox_ec_power PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(abox_ec_power PUBLIC abox::core)

add_library(abox_boot INTERFACE)
add_library(abox::boot ALIAS abox_boot)
target_link_libraries(abox_boot INTERFACE abox::core)

add_library(abox_boot_v2_common STATIC
    components/boot_v2/abox_boot_v2_state.c
    components/boot_v2/abox_boot_v2_descriptor.c
    components/boot_v2/abox_boot_v2_image.c
    components/boot_v2/abox_boot_v2_fault.c
)
add_library(abox::boot_v2_common ALIAS abox_boot_v2_common)
target_include_directories(abox_boot_v2_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(abox_boot_v2_common PUBLIC abox::core)

add_library(abox_boot_v2_app STATIC components/boot_v2/abox_boot_v2_app.c)
add_library(abox::boot_v2_app ALIAS abox_boot_v2_app)
target_include_directories(abox_boot_v2_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(abox_boot_v2_app PUBLIC abox::boot_v2_common abox::https_ufs_downloader)

add_library(abox_boot_v2_boot STATIC components/boot_v2/abox_boot_v2_boot.c)
add_library(abox::boot_v2_boot ALIAS abox_boot_v2_boot)
target_include_directories(abox_boot_v2_boot PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(abox_boot_v2_boot PUBLIC abox::boot_v2_common abox::core)

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

    add_executable(abox_ota_flash_test tests/ota_flash_test.c)
    target_link_libraries(abox_ota_flash_test PRIVATE abox::ota)
    add_test(NAME abox_ota_flash_test COMMAND abox_ota_flash_test)

    add_executable(abox_ota_ec_at_test tests/ota_ec_at_test.c)
    target_link_libraries(abox_ota_ec_at_test PRIVATE abox::ota)
    add_test(NAME abox_ota_ec_at_test COMMAND abox_ota_ec_at_test)

    add_executable(abox_ec800_test tests/abox_ec800_test.c)
    target_link_libraries(abox_ec800_test PRIVATE abox::ec800)
    add_test(NAME abox_ec800_test COMMAND abox_ec800_test)

    add_executable(abox_https_ufs_downloader_test tests/abox_https_ufs_downloader_test.c)
    target_link_libraries(abox_https_ufs_downloader_test PRIVATE abox::https_ufs_downloader)
    add_test(NAME abox_https_ufs_downloader_test COMMAND abox_https_ufs_downloader_test)

    add_executable(abox_ec_power_test tests/ec_power_test.c)
    target_link_libraries(abox_ec_power_test PRIVATE abox::ec_power)
    add_test(NAME abox_ec_power_test COMMAND abox_ec_power_test)

    add_executable(abox_boot_v2_test tests/boot_v2_test.c)
    target_link_libraries(abox_boot_v2_test PRIVATE abox::boot_v2_boot)
    add_test(NAME abox_boot_v2_test COMMAND abox_boot_v2_test)

    add_executable(abox_boot_v2_app_test tests/boot_v2_app_test.c)
    target_link_libraries(abox_boot_v2_app_test PRIVATE abox::boot_v2_app)
    add_test(NAME abox_boot_v2_app_test COMMAND abox_boot_v2_app_test)
endif()
