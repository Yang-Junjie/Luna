add_executable(
    RhiOffscreenCopy
    main.cpp
)

target_include_directories(
    RhiOffscreenCopy
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    RhiOffscreenCopy
    PRIVATE
        LunaCore
)

luna_configure_runtime_output(RhiOffscreenCopy)
