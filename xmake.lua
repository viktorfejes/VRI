add_rules("mode.debug", "mode.release")

set_languages("c99")
set_toolchains("clang")
set_warnings("all", "error", "extra", "pedantic")

if is_mode("debug") then
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.undefined", true)
end

target("chroma-scopes")
    set_kind("binary")
    add_includedirs("include", "external")
    add_files("src/**.c", "examples/triangle/*.c", "external/**.c")
    add_syslinks("d3d11", "d3dcompiler", "dxgi", "uuid", "dxguid", "shcore", "winmm", "gdi32")

    add_defines("WINVER=0x0A00", "_WIN32_WINNT=0x0A00")
    add_defines("VRI_ENABLE_D3D11_SUPPORT")
    if is_mode("debug") then
        add_defines("_DEBUG")
    end

    set_rundir(os.projectdir())
