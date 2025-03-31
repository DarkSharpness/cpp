add_rules("mode.debug", "mode.release")

local warnings = {
    "all",      -- turn on all warnings
    "extra",    -- turn on extra warnings
    "error",    -- treat warnings as errors
    "pedantic", -- be pedantic
}

local other_cxflags = {
    "-Wswitch-default",     -- warn if no default case in a switch statement
}

set_languages("c++23")

target("error-handler")
    set_kind("static")
    set_warnings(warnings)
    add_cxflags(other_cxflags)
    add_includedirs("csrc/include")
    add_files("csrc/cpp/utils/*.cpp")
    add_ldflags("-lstdc++_libbacktrace")

target("custom-test")
    set_kind("binary")
    add_deps("error-handler")
    set_warnings(warnings)
    add_cxflags(other_cxflags)
    add_includedirs("csrc/include")
    add_files("csrc/cpp/*.cpp")
    if is_mode("debug") then
        add_defines("_DARK_DEBUG")
    end