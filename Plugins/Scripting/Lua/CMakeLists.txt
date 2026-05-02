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

if(WIN32)
    set(LUNA_SCRIPT_LUA_PLATFORM_DIR Win64)
    set(LUNA_SCRIPT_LUA_PREFIX "")
elseif(APPLE)
    set(LUNA_SCRIPT_LUA_PLATFORM_DIR macOS)
    set(LUNA_SCRIPT_LUA_PREFIX "lib")
else()
    set(LUNA_SCRIPT_LUA_PLATFORM_DIR Linux)
    set(LUNA_SCRIPT_LUA_PREFIX "lib")
endif()

set(LUNA_SCRIPT_LUA_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/${LUNA_SCRIPT_LUA_PLATFORM_DIR}")

set_target_properties(
    LunaScriptLuaPlugin
    PROPERTIES
        OUTPUT_NAME "LunaScriptLua"
        PREFIX "${LUNA_SCRIPT_LUA_PREFIX}"
        RUNTIME_OUTPUT_DIRECTORY "${LUNA_SCRIPT_LUA_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${LUNA_SCRIPT_LUA_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${LUNA_SCRIPT_LUA_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${LUNA_SCRIPT_LUA_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${LUNA_SCRIPT_LUA_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${LUNA_SCRIPT_LUA_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${LUNA_SCRIPT_LUA_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${LUNA_SCRIPT_LUA_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${LUNA_SCRIPT_LUA_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${LUNA_SCRIPT_LUA_OUTPUT_DIR}"
)
