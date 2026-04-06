add_executable(
    RhiSwapchainLab
    main.cpp
    SwapchainLabState.h
    SwapchainLabPipeline.h
    SwapchainLabPipeline.cpp
)

target_include_directories(
    RhiSwapchainLab
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    RhiSwapchainLab
    PRIVATE
        LunaCore
)

luna_configure_runtime_output(RhiSwapchainLab)
