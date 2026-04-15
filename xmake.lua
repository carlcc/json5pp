add_rules("mode.debug", "mode.release")

option("json5pp_build_examples", {default = false, description = "Build examples"})
option("json5pp_build_tests", {default = false, description = "Build tests"})

target("json5pp", function () 
    set_kind("static")
    set_languages("c++20")
    set_warnings("all")
    add_files("src/json5.cpp")
    add_includedirs("include", {public = true})
    add_headerfiles("include/(json5/*.hpp)")
end)

if get_config("json5pp_build_examples") then
    for _, exfile in ipairs(os.files("examples/*.cpp")) do
        local name = "example_" .. path.basename(exfile)
        target(name, function () 
            set_kind("binary")
            set_languages("c++20")
            set_warnings("all")
            add_files(exfile)
            add_deps("json5pp")
        end)
    end
end

if get_config("json5pp_build_tests") then
    for _, testfile in ipairs(os.files("tests/test_*.cpp")) do
        local name = path.basename(testfile)
        target(name, function () 
            set_kind("binary")
            set_languages("c++20")
            set_warnings("all")
            add_files(testfile)
            add_deps("json5pp")
            set_group("tests")

            -- Pass the testdata source directory to the compat test
            if name == "test_json5_compat" then
                add_defines('TESTDATA_DIR="' .. path.absolute("tests/testdata") .. '"')
            end
        end)
    end
end