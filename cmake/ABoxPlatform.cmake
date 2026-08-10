function(abox_platform_attach target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "abox_platform_attach: target '${target}' does not exist")
    endif()

    target_link_libraries(${target} PRIVATE abox::core)

    if(ABOX_PLATFORM_SCHEDULER STREQUAL "FREERTOS")
        target_link_libraries(${target} PRIVATE abox::port_freertos)
    else()
        target_link_libraries(${target} PRIVATE abox::port_baremetal)
    endif()

    if(ABOX_PRODUCT_CONFIG_DIR)
        target_include_directories(${target} PRIVATE "${ABOX_PRODUCT_CONFIG_DIR}")
    endif()
endfunction()

function(abox_platform_attach_boot target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "abox_platform_attach_boot: target '${target}' does not exist")
    endif()

    target_link_libraries(${target} PRIVATE abox::boot)

    if(ABOX_PRODUCT_CONFIG_DIR)
        target_include_directories(${target} PRIVATE "${ABOX_PRODUCT_CONFIG_DIR}")
    endif()
endfunction()
