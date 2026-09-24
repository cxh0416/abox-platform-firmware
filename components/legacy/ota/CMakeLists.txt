add_library(abox_ota STATIC src/ota_flash.c src/ota_ec_at.c)
add_library(abox::ota ALIAS abox_ota)
target_include_directories(abox_ota PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(abox_ota PUBLIC abox::core)
