add_library(stb STATIC stb_image.cpp)
add_library(stb::stb ALIAS stb)

target_include_directories(stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
