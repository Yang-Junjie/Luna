if(TARGET LunaEditor)
    target_link_libraries(LunaEditor PRIVATE LunaEditorSDK)

    target_sources(
        LunaEditor
        PRIVATE
            Source/RenderProfilerPlugin.h
            Source/RenderProfilerPlugin.cpp
    )
endif()
