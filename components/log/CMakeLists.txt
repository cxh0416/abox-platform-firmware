add_library(abox_log_ring STATIC src/abox_log_ring.c)
add_library(abox::log_ring ALIAS abox_log_ring)
target_include_directories(abox_log_ring PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
