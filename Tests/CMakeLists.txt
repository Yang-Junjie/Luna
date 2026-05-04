add_executable(
    ScriptPluginContractTests
    ScriptPluginContractTests.cpp
)

target_link_libraries(
    ScriptPluginContractTests
    PRIVATE
        LunaCore
)

target_compile_features(
    ScriptPluginContractTests
    PRIVATE
        cxx_std_20
)

if(TARGET LunaScriptLuaPlugin)
    add_dependencies(ScriptPluginContractTests LunaScriptLuaPlugin)
endif()

add_test(
    NAME ScriptPluginContractTests
    COMMAND ScriptPluginContractTests
)
