#ifndef ORCHARD_SCRIPT_DETAIL_HPP
#define ORCHARD_SCRIPT_DETAIL_HPP

#include <orchard/scripting.hpp>
#include <orchard/script/orchard_api.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace orchard {
namespace script_detail {

namespace fs = std::filesystem;

const orchard_host_api& host_table();
void set_current_runtime(ScriptRuntime* runtime);

std::uint64_t fnv(std::uint64_t h, const std::string& s);
std::uint64_t fnv_start();
std::string hex(std::uint64_t v);
std::string read_text(const fs::path& path);
bool write_text(const fs::path& path, const std::string& text);
int run(const std::vector<std::string>& args, const fs::path& log_path, bool verbose, const ScriptRuntime& runtime);
std::string module_extension();
std::string replace_all(std::string text, const std::string& from, const std::string& to);
int param_count(const std::string& params);
std::string safe_stem(const std::string& script);
std::string find_on_path(const std::string& program);
std::string real_path(const std::string& path);
void prune_cache(const fs::path& cache, const std::string& stem, const std::string& keep_hash, const std::vector<std::string>& in_use);

void* open_library(const std::string& path, std::string* error);
void* library_symbol(void* handle, const char* name);
void close_library(void* handle);

std::unique_ptr<ScriptModule> load_native_module(const ScriptBuildResult& built, std::string* error);

ScriptHooks find_c_hooks(const std::string& source);
std::string spin_args(Dimension dimension, Axes axes, const std::string& zero, const std::string& amount);

std::unique_ptr<ScriptLanguage> make_walnut_language();
std::unique_ptr<ScriptLanguage> make_cpp_language();
std::unique_ptr<ScriptLanguage> make_c_language();
std::unique_ptr<ScriptLanguage> make_lua_language();
std::unique_ptr<ScriptLanguage> make_csharp_language();
std::unique_ptr<ScriptLanguage> make_java_language();

} // namespace script_detail
} // namespace orchard

#endif // ORCHARD_SCRIPT_DETAIL_HPP
