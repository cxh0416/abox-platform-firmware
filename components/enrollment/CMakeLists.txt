add_library(abox_enrollment STATIC src/abox_enrollment.c
    src/abox_enrollment_identity.c)
add_library(abox::enrollment ALIAS abox_enrollment)
target_include_directories(abox_enrollment PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(abox_enrollment PUBLIC abox::cjson)
add_library(abox_enrollment_ec800_http STATIC src/abox_enrollment_ec800_http.c)
add_library(abox::enrollment_ec800_http ALIAS abox_enrollment_ec800_http)
target_include_directories(abox_enrollment_ec800_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(abox_enrollment_ec800_http PUBLIC abox::ec800)
