cmake_minimum_required(VERSION 3.16)
project(LunaEditorNativePluginTemplate LANGUAGES CXX)

find_package(LunaEditorSDK CONFIG REQUIRED)

luna_add_editor_native_plugin(
    TARGET LunaEditorNativePluginTemplate
    OUTPUT_NAME LunaEditorNativePluginTemplate
    SOURCES
        Source/NativeTemplatePlugin.cpp
)
