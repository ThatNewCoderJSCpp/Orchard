#include "script_detail.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(_WIN32)
#define ORCHARD_HOSTFXR_CALL __cdecl
#else
#define ORCHARD_HOSTFXR_CALL
#endif

namespace orchard {
namespace script_detail {

namespace {

#if defined(_WIN32)
using char_t = wchar_t;
using host_string = std::wstring;
host_string host_str(const std::string& s) { return fs::path(s).wstring(); }
const char* const k_hostfxr_name = "hostfxr.dll";
#else
using char_t = char;
using host_string = std::string;
host_string host_str(const std::string& s) { return s; }
#if defined(__APPLE__)
const char* const k_hostfxr_name = "libhostfxr.dylib";
#else
const char* const k_hostfxr_name = "libhostfxr.so";
#endif
#endif

using init_for_config_fn = int(ORCHARD_HOSTFXR_CALL*)(const char_t* runtime_config_path, const void* parameters, void** host_context);
using get_delegate_fn = int(ORCHARD_HOSTFXR_CALL*)(void* host_context, int type, void** delegate);
using close_fn = int(ORCHARD_HOSTFXR_CALL*)(void* host_context);
using load_assembly_fn = int(ORCHARD_HOSTFXR_CALL*)(const char_t* assembly_path, const char_t* type_name, const char_t* method_name, const char_t* delegate_type_name, void* reserved, void** delegate);

constexpr int k_load_assembly_and_get_function_pointer = 5;
const char_t* const k_unmanaged_callers_only = reinterpret_cast<const char_t*>(static_cast<std::intptr_t>(-1));

using init_fn = int (*)(const orchard_host_api*);
using hooks_fn = int (*)();
using call_fn = int (*)(int, orchard_entity, double);
using error_fn = const char* (*)();

std::vector<int> version_parts(const std::string& v) {
    std::vector<int> out;
    int cur = 0;
    bool any = false;
    for (char c : v) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            cur = cur * 10 + (c - '0');
            any = true;
        } else if (c == '.') {
            out.push_back(cur);
            cur = 0;
        } else {
            break;
        }
    }
    if (any) out.push_back(cur);
    return out;
}

std::string dotnet_root(const ScriptToolchain& tools) {
    if (const char* env = std::getenv("DOTNET_ROOT")) {
        if (*env) return env;
    }
    const std::string exe = find_on_path(tools.dotnet);
    if (exe.empty()) return std::string();
    return fs::path(real_path(exe)).parent_path().string();
}

std::string find_hostfxr(const ScriptToolchain& tools) {
    const std::string root = dotnet_root(tools);
    if (root.empty()) return std::string();
    std::error_code ec;
    const fs::path fxr = fs::path(root) / "host" / "fxr";
    std::string best;
    std::vector<int> best_v;
    for (const auto& entry : fs::directory_iterator(fxr, ec)) {
        const fs::path lib = entry.path() / k_hostfxr_name;
        if (!fs::is_regular_file(lib, ec)) continue;
        const std::vector<int> v = version_parts(entry.path().filename().string());
        if (best.empty() || v > best_v) {
            best = lib.string();
            best_v = v;
        }
    }
    return best;
}

struct Runtime {
    void* library = nullptr;
    load_assembly_fn load = nullptr;
    std::string error;
};

Runtime& runtime_state() {
    static Runtime r;
    return r;
}

load_assembly_fn start_runtime(const ScriptToolchain& tools, const std::string& runtime_config, std::string* error) {
    Runtime& r = runtime_state();
    if (r.load) return r.load;
    if (!r.library) {
        const std::string lib = find_hostfxr(tools);
        if (lib.empty()) {
            if (error) *error = "can't find .NET's hostfxr; install the .NET SDK or set DOTNET_ROOT";
            return nullptr;
        }
        std::string why;
        r.library = open_library(lib, &why);
        if (!r.library) {
            if (error) *error = "can't load " + lib + ": " + why;
            return nullptr;
        }
    }
    auto init = reinterpret_cast<init_for_config_fn>(library_symbol(r.library, "hostfxr_initialize_for_runtime_config"));
    auto get = reinterpret_cast<get_delegate_fn>(library_symbol(r.library, "hostfxr_get_runtime_delegate"));
    auto close = reinterpret_cast<close_fn>(library_symbol(r.library, "hostfxr_close"));
    if (!init || !get || !close) {
        if (error) *error = "this hostfxr is too old; install .NET 6 or newer";
        return nullptr;
    }
    void* context = nullptr;
    const host_string config = host_str(runtime_config);
    const int status = init(config.c_str(), nullptr, &context);
    if (status < 0 || !context) {
        if (context) close(context);
        if (error) *error = "couldn't start the .NET runtime (hostfxr error " + std::to_string(status) + ")";
        return nullptr;
    }
    void* fn = nullptr;
    const int got = get(context, k_load_assembly_and_get_function_pointer, &fn);
    close(context);
    if (got < 0 || !fn) {
        if (error) *error = "couldn't get .NET's assembly loader (hostfxr error " + std::to_string(got) + ")";
        return nullptr;
    }
    r.load = reinterpret_cast<load_assembly_fn>(fn);
    return r.load;
}

void set_env(const char* name, const char* value) {
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 0);
#endif
}

void quiet_dotnet() {
    set_env("DOTNET_NOLOGO", "1");
    set_env("DOTNET_CLI_TELEMETRY_OPTOUT", "1");
    set_env("DOTNET_SKIP_FIRST_TIME_EXPERIENCE", "1");
    set_env("DOTNET_GENERATE_ASPNET_CERTIFICATE", "0");
}

std::string target_framework(const ScriptToolchain& tools, const ScriptRuntime& runtime, const fs::path& cache) {
    static std::string cached;
    if (!cached.empty()) return cached;
    quiet_dotnet();
    const fs::path log = cache / "dotnet-version.log";
    if (run({tools.dotnet, "--version"}, log, false, runtime) == 0) {
        const std::vector<int> v = version_parts(read_text(log));
        if (!v.empty() && v[0] >= 6) cached = "net" + std::to_string(v[0]) + ".0";
    }
    if (cached.empty()) cached = "net8.0";
    return cached;
}

std::string xml(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else out += c;
    }
    return out;
}

std::string cs_string(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

class DotnetModule final : public ScriptModule {
public:
    bool open(const ScriptToolchain& tools, const ScriptBuildResult& built, std::string* error) {
        const fs::path dll(built.module);
        const std::string assembly = dll.stem().string();
        fs::path config = dll;
        config.replace_extension(".runtimeconfig.json");
        load_assembly_fn load = start_runtime(tools, config.string(), error);
        if (!load) return false;
        const host_string path = host_str(dll.string());
        const host_string type = host_str("Orchard.OrchardHost, " + assembly);
        auto get = [&](const char* method, void** out) {
            const host_string m = host_str(method);
            return load(path.c_str(), type.c_str(), m.c_str(), k_unmanaged_callers_only, nullptr, out) == 0 && *out;
        };
        void* init = nullptr;
        void* hooks = nullptr;
        void* call = nullptr;
        void* err = nullptr;
        if (!get("Init", &init) || !get("Hooks", &hooks) || !get("Call", &call) || !get("Error", &err)) {
            if (error) *error = "couldn't find Orchard's entry points in " + dll.filename().string() + "; rebuild it";
            return false;
        }
        m_call = reinterpret_cast<call_fn>(call);
        m_error = reinterpret_cast<error_fn>(err);
        if (reinterpret_cast<init_fn>(init)(&host_table()) != 0) {
            if (error) *error = m_error();
            return false;
        }
        m_hooks = reinterpret_cast<hooks_fn>(hooks)();
        return true;
    }

    int hooks() const override { return m_hooks; }

    bool call(int hook, std::uint64_t entity, double dt, std::string& error) override {
        if (m_call(hook, entity, dt) == 0) return true;
        error = m_error();
        return false;
    }

private:
    call_fn m_call = nullptr;
    error_fn m_error = nullptr;
    int m_hooks = 0;
};

class CSharpLanguage final : public ScriptLanguage {
public:
    std::string name() const override { return "C#"; }
    std::vector<std::string> extensions() const override { return {".cs"}; }

    bool available(const ScriptToolchain& tools, std::string* why) const override {
        if (!tools.has_headers(why)) return false;
        std::error_code ec;
        if (!fs::is_regular_file(fs::path(tools.include_dir) / "orchard" / "script" / "csharp" / "Orchard.cs", ec)) {
            if (why) *why = "can't find Orchard's C# API (orchard/script/csharp/Orchard.cs) in '" + tools.include_dir + "'";
            return false;
        }
        if (find_on_path(tools.dotnet).empty()) {
            if (why) *why = "C# scripts need the .NET SDK (dotnet) on your PATH";
            return false;
        }
        return true;
    }

    ScriptBuildResult build(ScriptRuntime& runtime, const Project& project, const std::string& script) override {
        ScriptBuildResult result;
        result.script = script;
        const ScriptToolchain& tools = runtime.toolchain();
        std::string why;
        if (!available(tools, &why)) {
            result.log = why;
            return result;
        }
        const fs::path script_path = fs::absolute(project.path_of(script));
        std::error_code ec;
        if (!fs::is_regular_file(script_path, ec)) {
            result.log = script + " doesn't exist";
            return result;
        }
        const fs::path api = fs::path(tools.include_dir) / "orchard" / "script" / "csharp";
        const fs::path cache(tools.cache_dir);
        fs::create_directories(cache, ec);
        const std::string framework = target_framework(tools, runtime, cache);
        const std::string source = read_text(script_path);
        std::uint64_t h = fnv(fnv_start(), std::to_string(ORCHARD_SCRIPT_API_VERSION));
        h = fnv(h, read_text(api / "Orchard.cs"));
        h = fnv(h, read_text(api / "OrchardHost.cs"));
        h = fnv(fnv(fnv(h, source), script_path.string()), framework);
        const std::string stem = safe_stem(script) + "-cs-" + hex(h);
        const fs::path dir = cache / stem;
        const std::string assembly = "OrchardScript_" + hex(h);
        const fs::path dll = dir / "out" / (assembly + ".dll");
        result.module = dll.string();
        if (!tools.rebuild && fs::is_regular_file(dll, ec)) {
            result.ok = true;
            result.cached = true;
            return result;
        }
        fs::create_directories(dir, ec);
        std::string cls = fs::path(script).stem().string();
        std::string info = "namespace Orchard\n{\n    internal static class OrchardScriptInfo\n    {\n        public const string ClassName = " + cs_string(cls) + ";\n    }\n}\n";
        std::string proj;
        proj += "<Project Sdk=\"Microsoft.NET.Sdk\">\n";
        proj += "  <PropertyGroup>\n";
        proj += "    <TargetFramework>" + framework + "</TargetFramework>\n";
        proj += "    <AssemblyName>" + assembly + "</AssemblyName>\n";
        proj += "    <RootNamespace>Orchard</RootNamespace>\n";
        proj += "    <EnableDynamicLoading>true</EnableDynamicLoading>\n";
        proj += "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n";
        proj += "    <ImplicitUsings>enable</ImplicitUsings>\n";
        proj += "    <Nullable>disable</Nullable>\n";
        proj += "    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>\n";
        proj += "    <SatelliteResourceLanguages>en</SatelliteResourceLanguages>\n";
        proj += "    <GenerateDocumentationFile>false</GenerateDocumentationFile>\n";
        proj += "    <UseAppHost>false</UseAppHost>\n";
        proj += "    <DebugType>embedded</DebugType>\n";
        proj += "  </PropertyGroup>\n";
        proj += "  <ItemGroup>\n";
        proj += "    <Compile Include=\"" + xml((api / "Orchard.cs").string()) + "\" />\n";
        proj += "    <Compile Include=\"" + xml((api / "OrchardHost.cs").string()) + "\" />\n";
        proj += "    <Compile Include=\"OrchardScriptInfo.cs\" />\n";
        proj += "    <Compile Include=\"" + xml(script_path.string()) + "\" />\n";
        proj += "  </ItemGroup>\n";
        proj += "</Project>\n";
        const std::string nuget = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<configuration>\n  <packageSources>\n    <clear />\n  </packageSources>\n</configuration>\n";
        if (!write_text(dir / "OrchardScriptInfo.cs", info) || !write_text(dir / (assembly + ".csproj"), proj) || !write_text(dir / "nuget.config", nuget)) {
            result.log = "can't write the C# project in " + dir.string();
            return result;
        }
        quiet_dotnet();
        const fs::path step_log = cache / (stem + ".log");
        std::vector<std::string> cmd = {tools.dotnet, "build", (dir / (assembly + ".csproj")).string(), "-c", "Release", "-o", (dir / "out").string(),
                                        "--nologo", "-v", "q", "-clp:NoSummary;ErrorsOnly", "-p:TreatWarningsAsErrors=false"};
        const int status = run(cmd, step_log, tools.verbose, runtime);
        std::string log = read_text(step_log);
        log = replace_all(log, script_path.string(), script);
        log = replace_all(log, " [" + (dir / (assembly + ".csproj")).string() + "]", "");
        result.log = log;
        if (status != 0 || !fs::is_regular_file(dll, ec)) {
            if (result.log.empty()) result.log = "dotnet couldn't build " + script;
            fs::remove_all(dir / "out", ec);
            return result;
        }
        prune_cache(cache, stem, hex(h), runtime.loaded_modules());
        result.ok = true;
        return result;
    }

    std::unique_ptr<ScriptModule> load(ScriptRuntime& runtime, const ScriptBuildResult& built, std::string* error) override {
        auto m = std::make_unique<DotnetModule>();
        if (!m->open(runtime.toolchain(), built, error)) return nullptr;
        return m;
    }

    std::string starter(const std::string& class_name, Dimension d) const override { return starter(class_name, d, Axes::y_up); }

    std::string starter(const std::string& class_name, Dimension d, Axes axes) const override {
        std::string s = "using Orchard;\n\n";
        s += "public class " + class_name + " : Script\n{\n";
        s += "    public double Speed = 45;\n\n";
        s += "    public override void OnStart()\n    {\n";
        s += "        World.Log(\"Hello from \" + Entity.Name);\n";
        s += "    }\n\n";
        s += "    public override void OnTick(double dt)\n    {\n";
        s += "        Entity.Rotate(" + spin_args(d, axes, "0", "Speed * dt") + ");\n";
        s += "    }\n}\n";
        return s;
    }
};

} // namespace

std::unique_ptr<ScriptLanguage> make_csharp_language() {
    return std::make_unique<CSharpLanguage>();
}

} // namespace script_detail
} // namespace orchard
