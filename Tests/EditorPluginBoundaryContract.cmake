cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED LUNA_SOURCE_DIR OR LUNA_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "LUNA_SOURCE_DIR is required")
endif()

set(LUNA_EDITOR_PLUGIN_ROOT "${LUNA_SOURCE_DIR}/Plugins/Editor")
set(LUNA_EDITOR_SDK_TEMPLATE_ROOT "${LUNA_SOURCE_DIR}/SDK/Editor/Templates")

set(LUNA_BOUNDARY_FILES)
foreach(LUNA_BOUNDARY_ROOT IN ITEMS "${LUNA_EDITOR_PLUGIN_ROOT}" "${LUNA_EDITOR_SDK_TEMPLATE_ROOT}")
    if(EXISTS "${LUNA_BOUNDARY_ROOT}")
        file(
            GLOB_RECURSE LUNA_BOUNDARY_ROOT_FILES
            LIST_DIRECTORIES false
            "${LUNA_BOUNDARY_ROOT}/*.c"
            "${LUNA_BOUNDARY_ROOT}/*.cpp"
            "${LUNA_BOUNDARY_ROOT}/*.h"
            "${LUNA_BOUNDARY_ROOT}/*.hpp"
        )
        list(APPEND LUNA_BOUNDARY_FILES ${LUNA_BOUNDARY_ROOT_FILES})
    endif()
endforeach()

set(LUNA_BOUNDARY_VIOLATIONS)

function(luna_record_boundary_violation file_path line_number reason line_text)
    file(RELATIVE_PATH LUNA_RELATIVE_PATH "${LUNA_SOURCE_DIR}" "${file_path}")
    string(STRIP "${line_text}" LUNA_STRIPPED_LINE)
    list(APPEND LUNA_BOUNDARY_VIOLATIONS "${LUNA_RELATIVE_PATH}:${line_number}: ${reason}: ${LUNA_STRIPPED_LINE}")
    set(LUNA_BOUNDARY_VIOLATIONS "${LUNA_BOUNDARY_VIOLATIONS}" PARENT_SCOPE)
endfunction()

foreach(LUNA_BOUNDARY_FILE IN LISTS LUNA_BOUNDARY_FILES)
    file(STRINGS "${LUNA_BOUNDARY_FILE}" LUNA_BOUNDARY_LINES)
    set(LUNA_BOUNDARY_LINE_NUMBER 0)
    foreach(LUNA_BOUNDARY_LINE IN LISTS LUNA_BOUNDARY_LINES)
        math(EXPR LUNA_BOUNDARY_LINE_NUMBER "${LUNA_BOUNDARY_LINE_NUMBER} + 1")

        if(LUNA_BOUNDARY_LINE MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"]imgui\\.h[>\"]")
            luna_record_boundary_violation(
                "${LUNA_BOUNDARY_FILE}"
                "${LUNA_BOUNDARY_LINE_NUMBER}"
                "editor plugins must use EditorUi instead of raw imgui.h"
                "${LUNA_BOUNDARY_LINE}"
            )
        endif()

        if(LUNA_BOUNDARY_LINE MATCHES "ImGui::")
            luna_record_boundary_violation(
                "${LUNA_BOUNDARY_FILE}"
                "${LUNA_BOUNDARY_LINE_NUMBER}"
                "editor plugins must use EditorUi instead of raw ImGui calls"
                "${LUNA_BOUNDARY_LINE}"
            )
        endif()

        if(LUNA_BOUNDARY_LINE MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"][^\"]*Shell/")
            luna_record_boundary_violation(
                "${LUNA_BOUNDARY_FILE}"
                "${LUNA_BOUNDARY_LINE_NUMBER}"
                "editor plugins must not include editor shell internals"
                "${LUNA_BOUNDARY_LINE}"
            )
        endif()

        if(LUNA_BOUNDARY_LINE MATCHES "EditorBuiltinPluginRegistry")
            luna_record_boundary_violation(
                "${LUNA_BOUNDARY_FILE}"
                "${LUNA_BOUNDARY_LINE_NUMBER}"
                "editor plugins must use Luna/Editor/EditorBuiltinPluginRegistration.h instead of shell registry internals"
                "${LUNA_BOUNDARY_LINE}"
            )
        endif()

        if(LUNA_BOUNDARY_LINE MATCHES "LunaEditorLayer|EditorContext\\.h|Editor/Source|ImGuizmo")
            luna_record_boundary_violation(
                "${LUNA_BOUNDARY_FILE}"
                "${LUNA_BOUNDARY_LINE_NUMBER}"
                "editor plugins must not depend on editor-private implementation details"
                "${LUNA_BOUNDARY_LINE}"
            )
        endif()
    endforeach()
endforeach()

if(LUNA_BOUNDARY_VIOLATIONS)
    list(JOIN LUNA_BOUNDARY_VIOLATIONS "\n" LUNA_BOUNDARY_VIOLATION_TEXT)
    message(FATAL_ERROR "Editor plugin boundary violations found:\n${LUNA_BOUNDARY_VIOLATION_TEXT}")
endif()

message(STATUS "Editor plugin boundary contract passed")
