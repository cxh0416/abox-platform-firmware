add_library(abox_core STATIC src/abox_platform_config.c src/abox_platform_port.c)
add_library(abox::core ALIAS abox_core)
target_include_directories(abox_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(abox_core PUBLIC
    ABOX_PLATFORM_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    ABOX_PLATFORM_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    ABOX_PLATFORM_VERSION_PATCH=${PROJECT_VERSION_PATCH})
add_library(abox_boot INTERFACE)
add_library(abox::boot ALIAS abox_boot)
target_link_libraries(abox_boot INTERFACE abox::core)
foreach(scheduler baremetal freertos)
    add_library(abox_port_${scheduler} INTERFACE)
    add_library(abox::port_${scheduler} ALIAS abox_port_${scheduler})
    target_link_libraries(abox_port_${scheduler} INTERFACE abox::core)
    string(TOUPPER ${scheduler} scheduler_upper)
    target_compile_definitions(abox_port_${scheduler} INTERFACE ABOX_PLATFORM_SCHEDULER_${scheduler_upper}=1)
endforeach()
