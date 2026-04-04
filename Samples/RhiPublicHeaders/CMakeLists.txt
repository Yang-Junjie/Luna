add_executable(
    LunaRhiPublicHeaders
    main.cpp
)

target_include_directories(
    LunaRhiPublicHeaders
    PRIVATE
        ${PROJECT_SOURCE_DIR}/Luna
)

luna_configure_runtime_output(LunaRhiPublicHeaders)
