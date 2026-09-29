add_library(abox_board_io STATIC src/abox_board_io.c)
add_library(abox::board_io ALIAS abox_board_io)
target_include_directories(abox_board_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
