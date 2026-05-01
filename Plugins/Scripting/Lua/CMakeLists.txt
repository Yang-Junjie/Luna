add_library(
    LunaScriptLuaPlugin
    SHARED
    Source/LuaPluginRuntime.h
    Source/LuaPluginRuntime.cpp
    Source/LuaPropertySchema.h
    Source/LuaPropertySchema.cpp
    Source/Bindings/LuaCoreBindings.h
    Source/Bindings/LuaCoreBindings.cpp
    Source/Bindings/LuaInputBindings.h
    Source/Bindings/LuaInputBindings.cpp
    Source/Bindings/LuaSceneBindings.h
    Source/Bindings/LuaSceneBindings.cpp
    Source/LuaScriptPlugin.cpp
)

target_include_directories(
    LunaScriptLuaPlugin
    PRIVATE
        ${PROJECT_SOURCE_DIR}/Luna
        ${PROJECT_SOURCE_DIR}/third_party/lua
        ${PROJECT_SOURCE_DIR}/third_party/sol2/include
        ${PROJECT_SOURCE_DIR}/third_party/glm
)

target_link_libraries(
    LunaScriptLuaPlugin
    PRIVATE
        lua
)

target_compile_definitions(
    LunaScriptLuaPlugin
    PRIVATE
        SOL_NO_LUA_HPP=1
)

set_target_properties(
    LunaScriptLuaPlugin
    PROPERTIES
        OUTPUT_NAME "LunaScriptLua"
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64"
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64"
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64"
)
