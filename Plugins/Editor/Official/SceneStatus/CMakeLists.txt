if(TARGET LunaEditor)
    target_link_libraries(LunaEditor PRIVATE LunaEditorSDK)

    target_sources(
        LunaEditor
        PRIVATE
            Source/SceneStatusPlugin.h
            Source/SceneStatusPlugin.cpp
    )
endif()
