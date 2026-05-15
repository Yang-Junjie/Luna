if(TARGET LunaEditor)
    target_link_libraries(LunaEditor PRIVATE LunaEditorSDK)

    target_sources(
        LunaEditor
        PRIVATE
            Source/BackendCapabilitiesPlugin.h
            Source/BackendCapabilitiesPlugin.cpp
    )
endif()
