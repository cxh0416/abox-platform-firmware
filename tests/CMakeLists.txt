find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_executable(abox_board_io_test abox_board_io_test.c)
target_link_libraries(abox_board_io_test PRIVATE abox::board_io)
add_test(NAME abox_board_io_test COMMAND abox_board_io_test)
add_executable(abox_board_io_stm32_test abox_board_io_stm32_test.c
    ../targets/stm32f105_ec800_v1/board_io_stm32f105.c)
target_include_directories(abox_board_io_stm32_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stm32_stub
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/stm32f105_ec800_v1/include)
target_link_libraries(abox_board_io_stm32_test PRIVATE abox::board_io)
add_test(NAME abox_board_io_stm32_test COMMAND abox_board_io_stm32_test)
add_test(NAME abox_component_contract_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/cmake_component_contract_test.py)
foreach(case IN ITEMS
    enrollment:enrollment
    enrollment_ec800_http:enrollment_ec800_http
    ec800_ufs:ec800_ufs
    ec800_tls:ec800_tls
    mqtt_tls:mqtt_tls
    mqtt_trial:mqtt_trial
    ec800_command_port:ec800_command_port
    platform_config:core
    ota_flash:ota
    ota_ec_at:ota
    ec800:ec800
    mqtt_v4_contract:mqtt_v4
    mqtt_runtime:mqtt_runtime
    mqtt_ec800_rx:mqtt_ec800_rx
    mqtt_ec800:mqtt_ec800
    https_ufs_downloader:https_ufs_downloader
    ec_power:ec_power
    boot_v2:boot_v2_boot
    boot_v2_app:boot_v2_app)
    string(REPLACE ":" ";" pair "${case}")
    list(GET pair 0 test_name)
    list(GET pair 1 module)
    if(test_name STREQUAL "ota_flash" OR test_name STREQUAL "ota_ec_at"
            OR test_name STREQUAL "mqtt_v4_contract" OR test_name STREQUAL "ec_power"
            OR test_name STREQUAL "boot_v2" OR test_name STREQUAL "boot_v2_app")
        set(source "${test_name}_test.c")
    else()
        set(source "abox_${test_name}_test.c")
    endif()
    add_executable(abox_${test_name}_test ${source})
    target_link_libraries(abox_${test_name}_test PRIVATE abox::${module})
    add_test(NAME abox_${test_name}_test COMMAND abox_${test_name}_test)
endforeach()
add_executable(abox_ec800_mqtt_rx_route_test abox_ec800_mqtt_rx_route_test.c)
target_link_libraries(abox_ec800_mqtt_rx_route_test PRIVATE abox::ec800 abox::mqtt_ec800_rx)
add_test(NAME abox_ec800_mqtt_rx_route_test COMMAND abox_ec800_mqtt_rx_route_test)
