add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

-- add_requires("levilamina x.x.x") for a specific version
-- add_requires("levilamina develop") to use develop version
-- please note that you should add bdslibrary yourself if using dev version
if is_config("target_type", "server") then
    add_requires("levilamina", {configs = {target_type = "server"}})
else
    add_requires("levilamina", {configs = {target_type = "client"}})
end

add_requires("levibuildscript")

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
    add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
    add_defines("NOMINMAX", "UNICODE","Placeholder_EXPORTS")
    add_packages("levilamina")
    set_exceptions("none") -- To avoid conflicts with /EHa.
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_includedirs("src")
    -- if is_config("target_type", "server") then
    --     add_includedirs("src-server")
    --     add_files("src-server/**.cpp")
    -- else
    --     add_includedirs("src-client")
    --     add_files("src-client/**.cpp")
    -- end
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