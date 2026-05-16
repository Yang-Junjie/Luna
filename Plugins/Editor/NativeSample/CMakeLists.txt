luna_add_editor_native_plugin(
    TARGET LunaEditorNativeSample
    OUTPUT_NAME LunaEditorNativeSample
    SOURCES
        Source/NativeSampleEditorPlugin.cpp
)

if(TARGET LunaEditor)
    add_dependencies(LunaEditor LunaEditorNativeSample)
endif()
