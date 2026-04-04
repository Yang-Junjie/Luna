add_executable(
    LegacyScene
    main.cpp
)

target_include_directories(
    LegacyScene
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    LegacyScene
    PRIVATE
        LunaCore
)

luna_configure_runtime_output(LegacyScene)
