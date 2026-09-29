add_library(abox_cloud_service STATIC src/abox_cloud_service.c)
add_library(abox::cloud_service ALIAS abox_cloud_service)
target_include_directories(abox_cloud_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

