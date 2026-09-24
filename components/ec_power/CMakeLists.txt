add_library(abox_ec_power STATIC src/ec_power.c)
add_library(abox::ec_power ALIAS abox_ec_power)
target_include_directories(abox_ec_power PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(abox_ec_power PUBLIC abox::core)
