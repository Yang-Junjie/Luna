add_subdirectory(Scripting/Lua)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Editor/CMakeLists.txt")
    add_subdirectory(Editor)
endif()
