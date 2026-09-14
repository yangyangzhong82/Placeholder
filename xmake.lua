add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

if is_config("target_type", "server") then
    add_requires("levilamina 26.40.2", {configs = {target_type = "server"}})
else
    add_requires("levilamina 26.40.2", {configs = {target_type = "client"}})
end
add_requires("exprtk")
add_requires("utfcpp")
add_requires("levibuildscript")
add_requires("fast_float")
add_requires("icu4c")
add_requires("sol2")
add_requires("legacyremotecall 0.21.1")
add_requires("magic_enum 0.9.7")
if not has_config("vs_runtime") then
    set_runtimes("MD")
end

option("target_type")
    set_default("server")
    set_showmenu(true)
    set_values("server", "client")
option_end()

target("Placeholder") -- Change this to your mod name.
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    set_toolchains("clang-cl")
    add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
    add_defines("NOMINMAX", "UNICODE","Placeholder_EXPORTS")
    add_packages("levilamina","exprtk","utfcpp","fast_float", "icu4c","sol2","legacyremotecall","magic_enum")
    set_exceptions("none") -- To avoid conflicts with /EHa.
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    remove_files("src/PA/Entry/test.cpp") 
    add_includedirs("src")

        after_build(function (target)
        local bindir = path.join(os.projectdir(), "bin")
        local includedir = path.join(bindir, "include") -- 修改目标包含目录
        local libdir = path.join(bindir, "lib")
        os.mkdir(includedir)
        os.mkdir(libdir)

        -- 复制 .lib 文件
        os.cp(path.join(target:targetdir(), target:name() .. ".lib"), libdir)

        -- 复制头文件并保留目录结构
        local src_root = path.join(os.projectdir(), "src")
        for _, header_file in ipairs(target:headerfiles()) do
            local src_file_path = path.join(os.projectdir(), header_file)
            local relative_path = path.relative(src_file_path, src_root)
            local dest_file_path = path.join(includedir, relative_path)
            os.mkdir(path.directory(dest_file_path)) -- 确保目标目录存在
            os.cp(src_file_path, dest_file_path)
        end
        end)
    if is_config("target_type", "server") then
        add_defines("LL_PLAT_S")
    else
        add_defines("LL_PLAT_C")
    end