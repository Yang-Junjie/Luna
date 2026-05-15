if(TARGET LunaEditor)
    target_link_libraries(
        LunaEditor
        PRIVATE
            LunaEditorSDK
    )

    target_sources(
        LunaEditor
        PRIVATE
            Source/HighFreedomSamplePlugin.h
            Source/HighFreedomSamplePlugin.cpp
    )
endif()
