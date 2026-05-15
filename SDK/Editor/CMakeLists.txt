include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(LUNA_EDITOR_SDK_VERSION 0.1.0)

add_library(LunaEditorSDK INTERFACE)
add_library(Luna::EditorSDK ALIAS LunaEditorSDK)

set_target_properties(
    LunaEditorSDK
    PROPERTIES
        EXPORT_NAME EditorSDK
)

target_include_directories(
    LunaEditorSDK
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/Luna>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(
    TARGETS LunaEditorSDK
    EXPORT LunaEditorSDKTargets
    COMPONENT LunaEditorSDK
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT LunaEditorSDK
)

install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/Luna/EditorApi/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/EditorApi
    COMPONENT LunaEditorSDK
    FILES_MATCHING PATTERN "*.h"
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Templates/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/LunaEditorSDK/Templates
    COMPONENT LunaEditorSDK
)

install(
    EXPORT LunaEditorSDKTargets
    FILE LunaEditorSDKTargets.cmake
    NAMESPACE Luna::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LunaEditorSDK
    COMPONENT LunaEditorSDK
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/LunaEditorSDKConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/LunaEditorSDKConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LunaEditorSDK
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/LunaEditorSDKConfigVersion.cmake
    VERSION ${LUNA_EDITOR_SDK_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/LunaEditorSDKConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/LunaEditorSDKConfigVersion.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/LunaEditorSDKHelpers.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LunaEditorSDK
    COMPONENT LunaEditorSDK
)
