if(TARGET LunaEditor)
    target_link_libraries(LunaEditor PRIVATE LunaEditorSDK)

    target_sources(
        LunaEditor
        PRIVATE
            Source/RenderDebugPlugin.h
            Source/RenderDebugPlugin.cpp
    )
endif()
