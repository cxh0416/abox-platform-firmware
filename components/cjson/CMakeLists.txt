add_library(abox_cjson STATIC cJSON.c)
add_library(abox::cjson ALIAS abox_cjson)
target_include_directories(abox_cjson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
