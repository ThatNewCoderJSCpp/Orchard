#include <orchard/scripting.hpp>
#include <orchard/graph.hpp>
#include <orchard/script/orchard_api.h>

#include "script_detail.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace orchard {

namespace script_detail {

std::uint64_t fnv_start() {
    return 1469598103934665603ULL;
}

std::uint64_t fnv(std::uint64_t h, const std::string& s) {
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    h ^= 0xff;
    h *= 1099511628211ULL;
    return h;
}

std::string hex(std::uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(v));
    return buf;
}

std::string read_text(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::string();
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool write_text(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

namespace {

std::string quote(const std::string& arg) {
#if defined(_WIN32)
    std::string out = "\"";
    for (char c : arg) {
        if (c == '"') out += '\\';
        out += c;
    }
    return out + "\"";
#else
    std::string out = "'";
    for (char c : arg) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
#endif
}

} // namespace

int run(const std::vector<std::string>& args, const fs::path& log_path, bool verbose, const ScriptRuntime& runtime) {
    std::string command;
    for (const std::string& a : args) {
        if (!command.empty()) command += ' ';
        command += quote(a);
    }
    if (verbose) runtime.log("$ " + command, false);
    command += " > " + quote(log_path.string()) + " 2>&1";
#if defined(_WIN32)
    command = "\"" + command + "\"";
#endif
    const int status = std::system(command.c_str());
#if defined(_WIN32)
    return status;
#else
    if (status == -1) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}

std::string module_extension() {
#if defined(_WIN32)
    return ".dll";
#else
    return ".so";
#endif
}

std::string replace_all(std::string text, const std::string& from, const std::string& to) {
    if (from.empty()) return text;
    for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) text.replace(at, from.size(), to);
    return text;
}

int param_count(const std::string& params) {
    bool any = false;
    int commas = 0;
    int depth = 0;
    for (char c : params) {
        if (c == '(' || c == '<' || c == '[') ++depth;
        else if (c == ')' || c == '>' || c == ']') --depth;
        else if (c == ',' && depth == 0) ++commas;
        else if (!std::isspace(static_cast<unsigned char>(c))) any = true;
    }
    if (!any) return 0;
    if (commas == 0) {
        std::string t;
        for (char c : params) {
            if (!std::isspace(static_cast<unsigned char>(c))) t += c;
        }
        if (t == "void") return 0;
    }
    return commas + 1;
}

std::string safe_stem(const std::string& script) {
    std::string stem = fs::path(script).stem().string();
    for (char& c : stem) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') c = '_';
    }
    return stem.empty() ? std::string("script") : stem;
}

std::string find_on_path(const std::string& program) {
    if (program.empty()) return std::string();
    std::error_code ec;
    if (program.find('/') != std::string::npos || program.find('\\') != std::string::npos) return fs::is_regular_file(program, ec) ? program : std::string();
    const char* path = std::getenv("PATH");
    if (!path) return std::string();
#if defined(_WIN32)
    const char sep = ';';
    const std::vector<std::string> suffixes = {".exe", ".cmd", ".bat", ""};
#else
    const char sep = ':';
    const std::vector<std::string> suffixes = {""};
#endif
    std::string all = path;
    std::size_t start = 0;
    while (start <= all.size()) {
        std::size_t end = all.find(sep, start);
        if (end == std::string::npos) end = all.size();
        const std::string dir = all.substr(start, end - start);
        if (!dir.empty()) {
            for (const std::string& suffix : suffixes) {
                const fs::path candidate = fs::path(dir) / (program + suffix);
                if (fs::is_regular_file(candidate, ec)) return candidate.string();
            }
        }
        start = end + 1;
    }
    return std::string();
}

std::string real_path(const std::string& path) {
    std::error_code ec;
    const fs::path p = fs::canonical(path, ec);
    return ec ? path : p.string();
}

void prune_cache(const fs::path& cache, const std::string& stem, const std::string& keep_hash, const std::vector<std::string>& in_use) {
    std::error_code ec;
    const std::string prefix = stem.substr(0, stem.size() - keep_hash.size());
    for (const auto& entry : fs::directory_iterator(cache, ec)) {
        const std::string name = entry.path().filename().string();
        if (name.rfind(prefix, 0) != 0 || name.find(keep_hash) != std::string::npos || entry.path().extension() == ".o") continue;
        if (name.size() < prefix.size() + 16) continue;
        bool used = false;
        for (const std::string& u : in_use) used = used || u.rfind(entry.path().string(), 0) == 0;
        if (!used) fs::remove_all(entry.path(), ec);
    }
}

void* open_library(const std::string& path, std::string* error) {
#if defined(_WIN32)
    HMODULE h = LoadLibraryW(fs::path(path).wstring().c_str());
    if (!h && error) *error = "Windows error " + std::to_string(GetLastError());
    return reinterpret_cast<void*>(h);
#else
    void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h && error) {
        const char* e = dlerror();
        *error = e ? e : "dlopen failed";
    }
    return h;
#endif
}

void* library_symbol(void* handle, const char* name) {
    if (!handle) return nullptr;
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(handle), name));
#else
    return dlsym(handle, name);
#endif
}

void close_library(void* handle) {
    if (!handle) return;
#if defined(_WIN32)
    FreeLibrary(reinterpret_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

namespace {

ScriptRuntime* g_runtime = nullptr;

ScriptRuntime& rt() {
    return *g_runtime;
}

Entity entity_of(orchard_entity id) {
    return Entity::from_id(id);
}

orchard_entity id_of(Entity e) {
    return e.valid() ? e.id() : 0;
}

const char* keep(std::string value) {
    static std::array<std::string, 16> ring;
    static std::size_t next = 0;
    std::string& slot = ring[next];
    next = (next + 1) % ring.size();
    slot = std::move(value);
    return slot.c_str();
}

void host_log(const char* message, int error) {
    if (g_runtime) rt().log(message ? message : "", error != 0);
}

double host_time() {
    return g_runtime ? rt().time() : 0.0;
}

int host_is_2d() {
    return g_runtime && rt().scene() && rt().scene()->dimension() == Dimension::two_d ? 1 : 0;
}

orchard_entity host_find(const char* name) {
    if (!g_runtime || !rt().scene() || !name) return 0;
    return id_of(rt().scene()->find(name));
}

orchard_entity host_spawn(const char* name, orchard_entity parent) {
    if (!g_runtime || !rt().scene()) return 0;
    return id_of(rt().scene()->create(name ? name : "Entity", entity_of(parent)));
}

int host_exists(orchard_entity e) {
    return g_runtime && rt().scene() && rt().scene()->alive(entity_of(e)) ? 1 : 0;
}

void host_destroy(orchard_entity e) {
    if (g_runtime && rt().scene()) rt().scene()->destroy(entity_of(e));
}

const char* host_name(orchard_entity e) {
    if (!g_runtime || !rt().scene()) return "";
    return keep(rt().scene()->name(entity_of(e)));
}

void host_set_name(orchard_entity e, const char* name) {
    if (g_runtime && rt().scene() && name) rt().scene()->set_name(entity_of(e), name);
}

orchard_entity host_parent(orchard_entity e) {
    if (!g_runtime || !rt().scene()) return 0;
    return id_of(rt().scene()->parent(entity_of(e)));
}

void host_get_vec(orchard_entity id, int which, double* xyz) {
    xyz[0] = xyz[1] = xyz[2] = 0.0;
    if (!g_runtime || !rt().scene()) return;
    Scene& scene = *rt().scene();
    const Entity e = entity_of(id);
    const Transform* t = scene.transform(e);
    if (!t) return;
    Vec3 v;
    switch (which) {
    case ORCHARD_VEC_POSITION: v = t->position; break;
    case ORCHARD_VEC_ROTATION: v = to_euler(t->rotation); break;
    case ORCHARD_VEC_SCALE: v = t->scale; break;
    case ORCHARD_VEC_VELOCITY: if (rt().physics()) v = rt().physics()->velocity(e); break;
    case ORCHARD_VEC_SPIN: if (rt().physics()) v = rt().physics()->spin(e); break;
    case ORCHARD_VEC_WORLD_POSITION: {
        const Vec4 p = scene.world_matrix(e) * Vec4{0.0f, 0.0f, 0.0f, 1.0f};
        v = {p.x, p.y, p.z};
        break;
    }
    default: break;
    }
    xyz[0] = v.x;
    xyz[1] = v.y;
    xyz[2] = v.z;
}

void host_set_vec(orchard_entity id, int which, double x, double y, double z) {
    if (!g_runtime || !rt().scene()) return;
    Scene& scene = *rt().scene();
    const Entity e = entity_of(id);
    Transform* t = scene.transform(e);
    if (!t) return;
    const Vec3 v{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    switch (which) {
    case ORCHARD_VEC_POSITION: t->position = v; break;
    case ORCHARD_VEC_ROTATION: t->rotation = from_euler(v); break;
    case ORCHARD_VEC_SCALE: t->scale = v; break;
    case ORCHARD_VEC_VELOCITY: if (rt().physics()) rt().physics()->set_velocity(e, v); return;
    case ORCHARD_VEC_SPIN: if (rt().physics()) rt().physics()->set_spin(e, v); return;
    default: return;
    }
    if (rt().physics()) rt().physics()->moved(e);
}

void host_push(orchard_entity id, int kind, double x, double y, double z) {
    if (!g_runtime || !rt().physics()) return;
    const Entity e = entity_of(id);
    const Vec3 v{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    if (kind == ORCHARD_PUSH_FORCE) rt().physics()->force(e, v);
    else if (kind == ORCHARD_PUSH_TORQUE) rt().physics()->torque(e, v);
    else rt().physics()->impulse(e, v);
}

int host_has_body(orchard_entity id) {
    return g_runtime && rt().physics() && rt().physics()->has_body(entity_of(id)) ? 1 : 0;
}

int host_key_down(const char* key) {
    return g_runtime && key && rt().key_down(key) ? 1 : 0;
}

int host_touching(orchard_entity a, orchard_entity b) {
    return g_runtime && rt().physics() && rt().physics()->touching(entity_of(a), entity_of(b)) ? 1 : 0;
}

orchard_entity host_raycast(double ox, double oy, double oz, double dx, double dy, double dz, double max_distance, double* hit) {
    if (!g_runtime || !rt().physics() || !std::isfinite(max_distance) || max_distance <= 0.0) return 0;
    Vec3 point, normal;
    float distance = 0.0f;
    const Entity e = rt().physics()->raycast(Vec3{static_cast<float>(ox), static_cast<float>(oy), static_cast<float>(oz)}, Vec3{static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)}, static_cast<float>(max_distance), &point, &normal, &distance);
    if (!e.valid()) return 0;
    if (hit) {
        hit[0] = point.x;
        hit[1] = point.y;
        hit[2] = point.z;
        hit[3] = normal.x;
        hit[4] = normal.y;
        hit[5] = normal.z;
        hit[6] = distance;
    }
    return id_of(e);
}

int host_overlap_sphere(double x, double y, double z, double radius, orchard_entity* out, int capacity) {
    if (!g_runtime || !rt().physics() || !std::isfinite(radius) || radius <= 0.0) return 0;
    const std::vector<Entity> found = rt().physics()->overlap_sphere(Vec3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)}, static_cast<float>(radius));
    const int n = static_cast<int>(found.size());
    for (int i = 0; out && i < n && i < capacity; ++i) out[i] = id_of(found[static_cast<std::size_t>(i)]);
    return n;
}

void host_set_layers(orchard_entity e, unsigned long long layer, unsigned long long mask) {
    if (g_runtime && rt().physics()) rt().physics()->set_layers(entity_of(e), layer, mask);
}

orchard_entity host_instantiate(const char* path, double x, double y, double z, orchard_entity parent) {
    if (!g_runtime || !rt().scene() || !path) return 0;
    Transform at;
    at.position = Vec3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    return id_of(rt().instantiate(path, at, entity_of(parent)));
}

int host_fluid_brush(int brush, double x, double y, double z, double radius, double vx, double vy, double vz, int material) {
    if (!g_runtime || !rt().physics()) return 0;
    const Vec3 p{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    const Vec3 v{static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz)};
    return static_cast<int>(std::min<std::size_t>(rt().physics()->fluid_brush(brush, p, static_cast<float>(radius), v, material), 2147483647u));
}

void host_particles(orchard_entity e, int action, int count) {
    if (g_runtime) rt().particles(entity_of(e), action, count);
}

double host_animation(orchard_entity e, int action, const char* clip, double value) {
    return g_runtime ? rt().animation(entity_of(e), action, clip ? std::string(clip) : std::string(), value) : 0.0;
}

const char* host_animation_event(orchard_entity e) {
    static std::string last;
    last = g_runtime ? rt().next_animation_event(entity_of(e)) : std::string();
    return last.c_str();
}

std::vector<std::string> split_text(const char* text) {
    std::vector<std::string> out(1);
    if (!text) return out;
    for (const char* c = text; *c; ++c) {
        if (*c == '\x1f') out.emplace_back();
        else out.back() += *c;
    }
    return out;
}

std::string part(const std::vector<std::string>& p, std::size_t i, const std::string& fallback = std::string()) {
    return i < p.size() ? p[i] : fallback;
}

std::vector<int> parse_layers(const std::string& s) {
    std::vector<int> out;
    std::string cur;
    for (char c : s + ",") {
        if (c == ',' || c == ' ' || c == 'x') {
            if (!cur.empty()) out.push_back(std::atoi(cur.c_str()));
            cur.clear();
        } else {
            cur += c;
        }
    }
    return out;
}

ai::VoxelAgent voxel_agent(const double* args, int count, int from) {
    auto arg = [args, count](int i, double fb) { return args && i < count ? args[i] : fb; };
    ai::VoxelAgent a;
    a.height = static_cast<int>(arg(from, 2.0));
    a.step_up = static_cast<int>(arg(from + 1, 1.0));
    a.jump_up = a.step_up;
    a.max_drop = static_cast<int>(arg(from + 2, 3.0));
    a.jump_across = static_cast<int>(arg(from + 3, 0.0));
    a.danger_weight = static_cast<float>(arg(from + 4, 10.0));
    return a;
}

double host_lib(int op, Entity e, Entity o, const double* args, int count, const char* text) {
    ScriptRuntime& r = rt();
    ai::AiWorld& w = r.ai();
    Scene* s = r.scene();
    auto arg = [args, count](int i, double fallback) { return args && i < count ? args[i] : fallback; };
    auto argf = [&](int i, double fallback) { return static_cast<float>(arg(i, fallback)); };
    auto v3 = [&](int i) { return Vec3{argf(i, 0.0), argf(i + 1, 0.0), argf(i + 2, 0.0)}; };
    const std::vector<std::string> t = split_text(text);
    switch (op) {
    case ORCHARD_GAME_RANDOM_SEED:
        r.random().seed(static_cast<std::uint64_t>(arg(0, 0.0)));
        w.random().seed(static_cast<std::uint64_t>(arg(0, 0.0)) ^ 0x5bd1e995ull);
        return 0.0;
    case ORCHARD_GAME_RANDOM_GAUSSIAN: return r.random().gaussian(arg(0, 0.0), arg(1, 1.0));
    case ORCHARD_GAME_SECURE_RANDOM: return secure_random().range(arg(0, 0.0), arg(1, 1.0));
    case ORCHARD_GAME_SECURE_RANDOM_INTEGER: return static_cast<double>(secure_random().integer(static_cast<long long>(std::floor(arg(0, 0.0))), static_cast<long long>(std::floor(arg(1, 100.0)))));
    case ORCHARD_GAME_SECURE_CHANCE: return secure_random().chance(arg(0, 0.5)) ? 1.0 : 0.0;
    case ORCHARD_GAME_RANDOM_CHANCE: return r.random().chance(arg(0, 0.5)) ? 1.0 : 0.0;
    case ORCHARD_GAME_RANDOM_RANGE: return r.random().range(arg(0, 0.0), arg(1, 1.0));
    case ORCHARD_GAME_RANDOM_WHOLE:
    case ORCHARD_GAME_RANDOM_INTEGER: return static_cast<double>(r.random().integer(static_cast<long long>(std::floor(arg(0, 0.0))), static_cast<long long>(std::floor(arg(1, 10.0)))));
    case ORCHARD_GAME_RANDOM_WEIGHTED: {
        std::vector<double> wts;
        for (int i = 0; i < count; ++i) wts.push_back(args[i]);
        return wts.empty() ? -1.0 : static_cast<double>(r.random().weighted(wts));
    }
    case ORCHARD_GAME_NOISE: return noise(argf(0, 0.0), argf(1, 0.0), argf(2, 0.0), static_cast<std::uint32_t>(arg(3, 0.0)));
    case ORCHARD_GAME_FRACTAL_NOISE: return fractal_noise(argf(0, 0.0), argf(1, 0.0), argf(2, 0.0), static_cast<int>(arg(3, 4.0)), static_cast<std::uint32_t>(arg(4, 0.0)));
    case ORCHARD_GAME_BAG_ADD: r.bag(part(t, 0)).add(part(t, 1), static_cast<std::size_t>(std::max(1.0, arg(0, 1.0)))); return 0.0;
    case ORCHARD_GAME_BAG_COUNT:
    case ORCHARD_GAME_BAG_REMAINING: return r.has_bag(part(t, 0)) ? static_cast<double>(r.bag(part(t, 0)).remaining()) : 0.0;
    case ORCHARD_GAME_BAG_SIZE: return r.has_bag(part(t, 0)) ? static_cast<double>(r.bag(part(t, 0)).size()) : 0.0;
    case ORCHARD_GAME_BAG_RESET: if (r.has_bag(part(t, 0))) r.bag(part(t, 0)).refill(); return 0.0;
    case ORCHARD_GAME_BAG_CLEAR: r.drop_bag(part(t, 0)); return 0.0;
    case ORCHARD_GAME_BAG_REMOVE: return r.has_bag(part(t, 0)) && r.bag(part(t, 0)).remove(part(t, 1)) ? 1.0 : 0.0;
    case ORCHARD_GAME_BAG_AVOID_REPEATS: r.bag(part(t, 0)).avoid_boundary_repeat(arg(0, 1.0) != 0.0); return 0.0;
    case ORCHARD_GAME_SOUND_PLAY: return r.play_sound(part(t, 0), Vec3{}, Entity{}, false, argf(0, 1.0), argf(1, 1.0));
    case ORCHARD_GAME_SOUND_PLAY_AT: return r.play_sound(part(t, 0), v3(0), Entity{}, true, argf(3, 1.0), argf(4, 1.0), false, argf(5, 25.0));
    case ORCHARD_GAME_SOUND_PLAY_ON: return r.play_sound(part(t, 0), s && s->alive(e) ? r.gameplay().position(e) : Vec3{}, e, true, argf(0, 1.0), argf(1, 1.0), false, argf(2, 25.0));
    case ORCHARD_GAME_SOUND_LOOP: return r.play_sound(part(t, 0), Vec3{}, Entity{}, false, argf(0, 1.0), 1.0f, true);
    case ORCHARD_GAME_SOUND_LOOP_ON: return r.play_sound(part(t, 0), s && s->alive(e) ? r.gameplay().position(e) : Vec3{}, e, true, argf(0, 1.0), 1.0f, true, argf(1, 25.0));
    case ORCHARD_GAME_SOUND_STOP: r.stop_sound(static_cast<int>(arg(0, 0.0))); return 0.0;
    case ORCHARD_GAME_SOUND_STOP_ALL: r.stop_all_sounds(); return 0.0;
    case ORCHARD_GAME_SOUND_SET_VOLUME: r.set_sound_volume(static_cast<int>(arg(0, 0.0)), argf(1, 1.0)); return 0.0;
    case ORCHARD_GAME_SOUND_SET_PITCH: r.set_sound_pitch(static_cast<int>(arg(0, 0.0)), argf(1, 1.0)); return 0.0;
    case ORCHARD_GAME_SOUND_PAUSE: r.pause_sound(static_cast<int>(arg(0, 0.0)), arg(1, 1.0) != 0.0); return 0.0;
    case ORCHARD_GAME_SOUND_PLAYING: return r.sound_playing(static_cast<int>(arg(0, 0.0))) ? 1.0 : 0.0;
    case ORCHARD_GAME_SOUND_PLAY_SOURCE: return r.play_sound_source(e);
    case ORCHARD_GAME_SOUND_STOP_SOURCE: r.stop_sound_source(e); return 0.0;
    case ORCHARD_GAME_AI_ADD: return w.add_agent(e, part(t, 0, "guard")) ? 1.0 : 0.0;
    case ORCHARD_GAME_AI_SET_BRAIN: return w.set_brain(e, part(t, 0)) ? 1.0 : 0.0;
    case ORCHARD_GAME_AI_SET_BEHAVIOR: {
        std::string why;
        const bool ok = w.set_behavior(e, part(t, 0), &why);
        if (!ok && !why.empty()) r.log("AI behavior: " + why, true);
        return ok ? 1.0 : 0.0;
    }
    case ORCHARD_GAME_AI_DEFINE_BEHAVIOR: {
        std::string why;
        const bool ok = w.define_behavior(part(t, 0), part(t, 1), &why);
        if (!ok) r.log("AI behavior " + part(t, 0) + ": " + why, true);
        return ok ? 1.0 : 0.0;
    }
    case ORCHARD_GAME_AI_ENABLE: return w.set_enabled(e, arg(0, 1.0) != 0.0) ? 1.0 : 0.0;
    case ORCHARD_GAME_AI_IS_AGENT: return w.is_agent(e) ? 1.0 : 0.0;
    case ORCHARD_GAME_AI_SET_TEAM: return w.set_team(e, part(t, 0)) ? 1.0 : 0.0;
    case ORCHARD_GAME_AI_SET_RELATION: w.set_relation(part(t, 0), part(t, 1), static_cast<int>(arg(0, -1.0))); return 0.0;
    case ORCHARD_GAME_AI_RELATION: return w.relation(part(t, 0), part(t, 1));
    case ORCHARD_GAME_AI_HOSTILE: return w.hostile(e, o) ? 1.0 : 0.0;
    case ORCHARD_GAME_AI_MOVE_TO: return w.move_to(e, v3(0), arg(3, 0.0) != 0.0) ? 1.0 : 0.0;
    case ORCHARD_GAME_AI_STOP: w.stop_moving(e); return 0.0;
    case ORCHARD_GAME_AI_ARRIVED: return w.arrived(e) ? 1.0 : 0.0;
    case ORCHARD_GAME_AI_IS_MOVING: return w.moving(e) ? 1.0 : 0.0;
    case ORCHARD_GAME_AI_SET_TARGET: return w.set_target(e, o) ? 1.0 : 0.0;
    case ORCHARD_GAME_AI_CAN_SEE: return w.can_see(e, o) ? 1.0 : 0.0;
    case ORCHARD_GAME_AI_AWARENESS: return w.awareness(e, o);
    case ORCHARD_GAME_AI_SUSPICION: return w.suspicion(e);
    case ORCHARD_GAME_AI_ALERT_LEVEL: return w.alert_level(e);
    case ORCHARD_GAME_AI_FINISH_ACTION: return w.finish_action(e, arg(0, 1.0) != 0.0) ? 1.0 : 0.0;
    case ORCHARD_GAME_AI_SET_VISIBILITY: w.set_visibility(e, argf(0, 1.0)); return 0.0;
    case ORCHARD_GAME_AI_MAKE_NOISE: w.make_noise(v3(0), argf(3, 15.0), e, part(t, 0, "noise")); return 0.0;
    case ORCHARD_GAME_AI_MAKE_NOISE_FROM: w.make_noise(s && s->alive(e) ? r.gameplay().position(e) : Vec3{}, argf(0, 15.0), e, part(t, 0, "noise")); return 0.0;
    case ORCHARD_GAME_AI_SET_TEAM_ALERT: {
        const std::string ph = part(t, 1, "alert");
        const ai::AlertPhase p = ph == "calm" ? ai::AlertPhase::calm : ph == "caution" ? ai::AlertPhase::caution : ph == "evasion" ? ai::AlertPhase::evasion : ai::AlertPhase::alert;
        w.set_team_alert(part(t, 0), p, v3(0));
        return 0.0;
    }
    case ORCHARD_GAME_AI_PATH_LENGTH: {
        ai::NavPathOptions opt;
        opt.partial = false;
        const ai::NavPath p = w.find_path(v3(0), v3(3), opt);
        return p.found ? p.length : -1.0;
    }
    case ORCHARD_GAME_AI_ADD_DANGER: return w.add_danger(v3(0), argf(3, 3.0), argf(4, 5.0), argf(5, 0.0));
    case ORCHARD_GAME_AI_CLEAR_DANGER: w.clear_danger(); return 0.0;
    case ORCHARD_GAME_AI_BAKE: return static_cast<double>(w.bake(v3(0), v3(3)));
    case ORCHARD_GAME_AI_LINE_OF_SIGHT: return w.line_of_sight(v3(0), v3(3)) ? 1.0 : 0.0;
    case ORCHARD_GAME_AI_SET_NUMBER:
        if (ai::Blackboard* b = w.blackboard(e)) b->set_number(part(t, 0), arg(0, 0.0));
        return 0.0;
    case ORCHARD_GAME_AI_GET_NUMBER: {
        ai::Blackboard* b = w.blackboard(e);
        return b ? b->number(part(t, 0), 0.0) : 0.0;
    }
    case ORCHARD_GAME_AI_SET_TEXT:
        if (ai::Blackboard* b = w.blackboard(e)) b->set(part(t, 0), part(t, 1));
        return 0.0;
    case ORCHARD_GAME_AI_DIRECTOR_INTENSITY: return w.director().intensity();
    case ORCHARD_GAME_AI_DIRECTOR_MENACE: return w.director().menace();
    case ORCHARD_GAME_AI_DIRECTOR_NOTE: w.director().note(part(t, 0), argf(0, 1.0)); return 0.0;
    case ORCHARD_GAME_AI_DIRECTOR_HABIT: return w.director().habit(part(t, 0));
    case ORCHARD_GAME_AI_SET_SPAWNS: w.set_spawns(part(t, 0), static_cast<int>(arg(0, 10.0)), argf(1, 15.0), argf(2, 40.0), argf(3, 3.0)); return 0.0;
    case ORCHARD_GAME_AI_AGENT_COUNT: return static_cast<double>(w.agent_count());
    case ORCHARD_GAME_VOXEL_DEFINE: {
        ai::Block b;
        b.solid = arg(0, 1.0) != 0.0;
        b.climbable = arg(1, 0.0) != 0.0;
        b.liquid = arg(2, 0.0) != 0.0;
        b.cost = argf(3, 1.0);
        b.danger = argf(4, 0.0);
        b.open = arg(5, 0.0) != 0.0;
        if (b.open) b.solid = true;
        return w.voxels().define(part(t, 0), b);
    }
    case ORCHARD_GAME_VOXEL_SET: {
        const int id = w.voxels().type_id(part(t, 0, "air"));
        w.voxels().set(static_cast<int>(std::floor(arg(0, 0.0))), static_cast<int>(std::floor(arg(1, 0.0))), static_cast<int>(std::floor(arg(2, 0.0))), id < 0 ? 0 : id);
        return 0.0;
    }
    case ORCHARD_GAME_VOXEL_FILL: {
        const int id = w.voxels().type_id(part(t, 0, "air"));
        auto ip = [&](int i) { return static_cast<int>(std::floor(arg(i, 0.0))); };
        w.voxels().fill(ai::BlockPos{ip(0), ip(1), ip(2)}, ai::BlockPos{ip(3), ip(4), ip(5)}, id < 0 ? 0 : id);
        return 0.0;
    }
    case ORCHARD_GAME_VOXEL_CLEAR: w.voxels().clear(); return 0.0;
    case ORCHARD_GAME_VOXEL_FLOOR: w.voxels().solid_floor = static_cast<int>(std::floor(arg(0, 0.0))); return 0.0;
    case ORCHARD_GAME_GOAP_ACTION: w.planner(part(t, 0)).add(part(t, 1), part(t, 2), part(t, 3), argf(0, 1.0)); return 0.0;
    case ORCHARD_GAME_GOAP_REMOVE: w.planner(part(t, 0)).remove(part(t, 1)); return 0.0;
    case ORCHARD_GAME_GOAP_CLEAR: w.planner(part(t, 0)).clear(); return 0.0;
    case ORCHARD_GAME_CHESS_NEW: return w.new_chess(part(t, 0));
    case ORCHARD_GAME_CHESS_FREE: w.drop_chess(static_cast<int>(arg(0, 0.0))); return 0.0;
    case ORCHARD_GAME_CHESS_LOAD: {
        ai::Chess* c = w.chess(static_cast<int>(arg(0, 0.0)));
        return c && c->load_fen(part(t, 0)) ? 1.0 : 0.0;
    }
    case ORCHARD_GAME_CHESS_MOVE: {
        ai::Chess* c = w.chess(static_cast<int>(arg(0, 0.0)));
        return c && c->play(part(t, 0)) ? 1.0 : 0.0;
    }
    case ORCHARD_GAME_CHESS_IS_LEGAL: {
        ai::Chess* c = w.chess(static_cast<int>(arg(0, 0.0)));
        return c && c->parse_move(part(t, 0)) ? 1.0 : 0.0;
    }
    case ORCHARD_GAME_CHESS_UNDO: {
        ai::Chess* c = w.chess(static_cast<int>(arg(0, 0.0)));
        return c && c->undo() ? 1.0 : 0.0;
    }
    case ORCHARD_GAME_CHESS_TURN: {
        ai::Chess* c = w.chess(static_cast<int>(arg(0, 0.0)));
        return c ? c->turn() : 0.0;
    }
    case ORCHARD_GAME_CHESS_PIECE: {
        ai::Chess* c = w.chess(static_cast<int>(arg(0, 0.0)));
        return c ? c->piece_at(part(t, 0)) : 0.0;
    }
    case ORCHARD_GAME_NN_CREATE: {
        const auto hidden = ai::parse_activation(part(t, 2, "tanh"));
        const auto output = ai::parse_activation(part(t, 3, "sigmoid"));
        const std::vector<int> layers = parse_layers(part(t, 1));
        if (layers.size() < 2 || !hidden || !output) return 0.0;
        w.make_net(part(t, 0), layers, *hidden, *output);
        return 1.0;
    }
    case ORCHARD_GAME_NN_TRAIN: {
        ai::NeuralNet* n = w.net(part(t, 0));
        if (!n) return -1.0;
        const double rate = arg(0, 0.05);
        std::vector<double> in, target;
        for (int i = 1; i < count; ++i) (static_cast<int>(in.size()) < n->inputs() ? in : target).push_back(args[i]);
        return n->train(in, target, rate);
    }
    case ORCHARD_GAME_NN_LOAD: {
        auto n = ai::NeuralNet::load(part(t, 1));
        if (!n) return 0.0;
        ai::NeuralNet& dst = w.make_net(part(t, 0), n->layers(), ai::Activation::tanh, ai::Activation::sigmoid);
        dst = *n;
        return 1.0;
    }
    case ORCHARD_GAME_Q_CHOOSE: return w.learner(part(t, 0), static_cast<int>(arg(0, 4.0))).choose(part(t, 1));
    case ORCHARD_GAME_Q_BEST: return w.learner(part(t, 0), 0).best(part(t, 1));
    case ORCHARD_GAME_Q_LEARN: w.learner(part(t, 0), 0).learn(part(t, 1), static_cast<int>(arg(0, 0.0)), arg(1, 0.0), part(t, 2), arg(2, 0.0) != 0.0); return 0.0;
    case ORCHARD_GAME_Q_SETTINGS: {
        ai::QLearner& q = w.learner(part(t, 0), 0);
        q.epsilon = arg(0, 0.1);
        q.alpha = arg(1, 0.2);
        q.gamma = arg(2, 0.9);
        return 0.0;
    }
    case ORCHARD_GAME_Q_LOAD: return w.learner(part(t, 0), 0).load(part(t, 1)) ? 1.0 : 0.0;
    case ORCHARD_GAME_MARKOV_TRAIN: w.markov(part(t, 0), static_cast<int>(arg(0, 1.0))).train_text(part(t, 1), arg(1, 0.0) != 0.0); return 0.0;
    default: return 0.0;
    }
}

const char* host_game_text(int op, orchard_entity entity, orchard_entity other, const double* args, int count, const char* text) {
    static std::string out;
    out.clear();
    if (!g_runtime) return out.c_str();
    ScriptRuntime& r = rt();
    ai::AiWorld& w = r.ai();
    const Entity e = entity_of(entity);
    static_cast<void>(other);
    auto arg = [args, count](int i, double fallback) { return args && i < count ? args[i] : fallback; };
    const std::vector<std::string> t = split_text(text);
    auto chess = [&]() { return w.chess(static_cast<int>(arg(0, 0.0))); };
    switch (op) {
    case ORCHARD_GAME_SECURE_PICK:
    case ORCHARD_GAME_RANDOM_PICK: {
        std::vector<std::string> choices;
        std::string cur;
        const std::string all = part(t, 0);
        const char sep = all.find('|') != std::string::npos ? '|' : ',';
        for (char c : all + sep) {
            if (c == sep) {
                std::size_t a = cur.find_first_not_of(' '), b = cur.find_last_not_of(' ');
                if (a != std::string::npos) choices.push_back(cur.substr(a, b - a + 1));
                cur.clear();
            } else {
                cur += c;
            }
        }
        if (!choices.empty()) out = op == ORCHARD_GAME_SECURE_PICK ? choices[secure_random().index(choices.size())] : r.random().pick(choices);
        break;
    }
    case ORCHARD_GAME_SECURE_RANDOM_TEXT: out = secure_random().text(static_cast<std::size_t>(std::clamp(arg(0, 16.0), 0.0, 4096.0)), part(t, 0, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789")); break;
    case ORCHARD_GAME_SECURE_TOKEN: out = secure_random().token(static_cast<std::size_t>(std::clamp(arg(0, 32.0), 1.0, 4096.0))); break;
    case ORCHARD_GAME_SECURE_HEX: out = secure_random().hex(static_cast<std::size_t>(std::clamp(arg(0, 16.0), 1.0, 4096.0))); break;
    case ORCHARD_GAME_SECURE_UUID: out = secure_random().uuid(); break;
    case ORCHARD_GAME_RANDOM_TEXT: out = r.random().text(static_cast<std::size_t>(std::clamp(arg(0, 8.0), 0.0, 4096.0)), part(t, 0, "abcdefghijklmnopqrstuvwxyz")); break;
    case ORCHARD_GAME_BAG_NEXT:
        if (r.has_bag(part(t, 0)) && !r.bag(part(t, 0)).empty()) out = r.bag(part(t, 0)).next();
        break;
    case ORCHARD_GAME_BAG_PEEK:
        if (r.has_bag(part(t, 0))) {
            ShuffleBag<std::string>& b = r.bag(part(t, 0));
            if (b.remaining() == 0 && !b.empty()) b.refill();
            if (const std::string* p = b.peek()) out = *p;
        }
        break;
    case ORCHARD_GAME_AI_BRAIN_BEHAVIOR: out = ai::brain_behavior(part(t, 0)); break;
    case ORCHARD_GAME_AI_BEHAVIOR_TEXT: out = w.behavior_text(e); break;
    case ORCHARD_GAME_AI_TEAM: out = w.team(e); break;
    case ORCHARD_GAME_AI_STATE: out = w.state(e); break;
    case ORCHARD_GAME_AI_ACTION: out = w.action(e); break;
    case ORCHARD_GAME_AI_EVENT: out = w.next_event(e); break;
    case ORCHARD_GAME_AI_PENDING: out = w.pending_action(e); break;
    case ORCHARD_GAME_AI_PENDING_ARG: out = w.pending_argument(e, static_cast<int>(arg(0, 0.0))); break;
    case ORCHARD_GAME_AI_TEAM_ALERT: out = ai::to_string(w.team_alert(part(t, 0))); break;
    case ORCHARD_GAME_AI_GET_TEXT:
        if (ai::Blackboard* b = w.blackboard(e)) out = b->text(part(t, 0));
        break;
    case ORCHARD_GAME_AI_DIRECTOR_PHASE: out = w.director().phase_name(); break;
    case ORCHARD_GAME_VOXEL_GET: {
        const int id = w.voxels().get(static_cast<int>(std::floor(arg(0, 0.0))), static_cast<int>(std::floor(arg(1, 0.0))), static_cast<int>(std::floor(arg(2, 0.0))));
        if (const ai::VoxelWorld::Type* ty = w.voxels().type(id)) out = ty->name;
        break;
    }
    case ORCHARD_GAME_GOAP_PLAN: {
        const ai::GoapPlan plan = w.planner(part(t, 0)).plan(ai::parse_state(part(t, 1)), ai::parse_state(part(t, 2)));
        for (const std::string& a : plan.actions) out += (out.empty() ? "" : ",") + a;
        break;
    }
    case ORCHARD_GAME_CHESS_FEN:
        if (ai::Chess* c = chess()) out = c->fen();
        break;
    case ORCHARD_GAME_CHESS_BEST:
        if (ai::Chess* c = chess()) {
            const ai::Chess::SearchInfo info = c->search_level(static_cast<int>(arg(1, 5.0)), &w.random());
            if (info.found) out = info.move.uci();
        }
        break;
    case ORCHARD_GAME_CHESS_STATUS:
        if (ai::Chess* c = chess()) out = c->status();
        break;
    case ORCHARD_GAME_CHESS_MOVES:
        if (ai::Chess* c = chess())
            for (const ai::Chess::Move& m : c->legal_moves()) out += (out.empty() ? "" : " ") + m.uci();
        break;
    case ORCHARD_GAME_CHESS_BOARD:
        if (ai::Chess* c = chess()) out = c->board_text();
        break;
    case ORCHARD_GAME_CHESS_HISTORY:
        if (ai::Chess* c = chess())
            for (const std::string& m : c->history_san()) out += (out.empty() ? "" : " ") + m;
        break;
    case ORCHARD_GAME_NN_SAVE:
        if (ai::NeuralNet* n = w.net(part(t, 0))) out = n->save();
        break;
    case ORCHARD_GAME_Q_SAVE: out = w.learner(part(t, 0), 0).save(); break;
    case ORCHARD_GAME_MARKOV_GENERATE: out = w.markov(part(t, 0), 1).generate_text(w.random(), static_cast<int>(arg(0, 20.0)), arg(1, 0.0) != 0.0); break;
    default: break;
    }
    return out.c_str();
}

int host_game_values(int op, orchard_entity entity, orchard_entity other, const double* args, int count, const char* text, double* out, int capacity) {
    if (!g_runtime || !out || capacity <= 0) return 0;
    ScriptRuntime& r = rt();
    ai::AiWorld& w = r.ai();
    const Entity e = entity_of(entity);
    static_cast<void>(other);
    auto arg = [args, count](int i, double fallback) { return args && i < count ? args[i] : fallback; };
    auto argf = [&](int i, double fallback) { return static_cast<float>(arg(i, fallback)); };
    auto v3 = [&](int i) { return Vec3{argf(i, 0.0), argf(i + 1, 0.0), argf(i + 2, 0.0)}; };
    const std::vector<std::string> t = split_text(text);
    int n = 0;
    auto put = [&](double v) {
        if (n < capacity) out[n++] = v;
    };
    auto put3 = [&](Vec3 v) {
        put(v.x);
        put(v.y);
        put(v.z);
    };
    switch (op) {
    case ORCHARD_GAME_RANDOM_IN_SPHERE: put3(r.random().in_sphere(argf(0, 1.0))); break;
    case ORCHARD_GAME_RANDOM_ON_SPHERE: put3(r.random().on_sphere(argf(0, 1.0))); break;
    case ORCHARD_GAME_RANDOM_IN_CIRCLE: {
        Vec3 v = r.random().in_circle(argf(0, 1.0));
        if (r.scene() && r.scene()->dimension() == Dimension::two_d) v = Vec3{v.x, v.z, 0.0f};
        put3(v);
        break;
    }
    case ORCHARD_GAME_RANDOM_IN_CONE: put3(r.random().direction_in_cone(v3(0), argf(3, 30.0))); break;
    case ORCHARD_GAME_AI_FIND_PATH: {
        const ai::NavPath p = w.find_path(v3(0), v3(3));
        for (const ai::NavPoint& pt : p.points) put3(pt.position);
        break;
    }
    case ORCHARD_GAME_AI_PATH:
        for (const Vec3& p : w.current_path(e)) put3(p);
        break;
    case ORCHARD_GAME_AI_RANDOM_POINT: put3(w.random_point(v3(0), argf(3, 10.0))); break;
    case ORCHARD_GAME_AI_SNAP: put3(w.nav().snap(v3(0), 3.0f)); break;
    case ORCHARD_GAME_AI_FIND_COVER: {
        const ai::CoverSpot c = w.find_cover(e, v3(0), argf(3, 12.0));
        if (c.found) {
            put3(c.position);
            put(c.full ? 1.0 : 0.0);
        }
        break;
    }
    case ORCHARD_GAME_AI_LAST_SEEN: {
        const Entity target = w.target(e);
        if (r.scene() && r.scene()->alive(target)) put3(r.gameplay().position(target));
        break;
    }
    case ORCHARD_GAME_AI_LOB: put3(ai::lob_velocity(v3(0), v3(3), argf(6, 2.0), r.physics() ? r.physics()->gravity() : 9.81f)); break;
    case ORCHARD_GAME_AI_INTERCEPT: {
        Vec3 aim;
        ai::intercept(v3(0), v3(3), v3(6), argf(9, 30.0), aim);
        put3(aim);
        break;
    }
    case ORCHARD_GAME_VOXEL_PATH: {
        auto ip = [&](int i) { return static_cast<int>(std::floor(arg(i, 0.0))); };
        const ai::VoxelPath p = w.voxels().pathfinder().find(ai::BlockPos{ip(0), ip(1), ip(2)}, ai::BlockPos{ip(3), ip(4), ip(5)}, voxel_agent(args, count, 6), false);
        for (const ai::VoxelStep& st : p.steps) put3(Vec3{static_cast<float>(st.at.x), static_cast<float>(st.at.y), static_cast<float>(st.at.z)});
        break;
    }
    case ORCHARD_GAME_VOXEL_FLEE: {
        auto ip = [&](int i) { return static_cast<int>(std::floor(arg(i, 0.0))); };
        ai::VoxelAgent agent;
        agent.height = static_cast<int>(arg(7, 2.0));
        const ai::VoxelPath p = w.voxels().pathfinder().flee(ai::BlockPos{ip(0), ip(1), ip(2)}, ai::BlockPos{ip(3), ip(4), ip(5)}, ip(6), agent);
        for (const ai::VoxelStep& st : p.steps) put3(Vec3{static_cast<float>(st.at.x), static_cast<float>(st.at.y), static_cast<float>(st.at.z)});
        break;
    }
    case ORCHARD_GAME_NN_RUN:
        if (ai::NeuralNet* net = w.net(part(t, 0))) {
            std::vector<double> in;
            for (int i = 0; i < count; ++i) in.push_back(args[i]);
            for (double v : net->run(in)) put(v);
        }
        break;
    default: break;
    }
    return n;
}

double host_game(int op, orchard_entity entity, orchard_entity other, const double* args, int count, const char* text) {
    if (!g_runtime) return 0.0;
    Gameplay& g = rt().gameplay();
    Scene* s = rt().scene();
    const Entity e = entity_of(entity), o = entity_of(other);
    auto arg = [args, count](int i, double fallback) { return args && i < count ? args[i] : fallback; };
    const std::string tag = text ? std::string(text) : std::string();
    switch (op) {
    case ORCHARD_GAME_HEALTH: return g.health(e);
    case ORCHARD_GAME_MAX_HEALTH: return g.max_health(e);
    case ORCHARD_GAME_SET_HEALTH: return g.set_health(e, static_cast<float>(arg(0, 0.0))) ? 1.0 : 0.0;
    case ORCHARD_GAME_SET_MAX_HEALTH: return g.set_max_health(e, static_cast<float>(arg(0, 100.0))) ? 1.0 : 0.0;
    case ORCHARD_GAME_HEAL: return g.heal(e, static_cast<float>(arg(0, 0.0)));
    case ORCHARD_GAME_DAMAGE: return g.damage(e, static_cast<float>(arg(0, 0.0)), o);
    case ORCHARD_GAME_KILL: return g.kill(e, o) ? 1.0 : 0.0;
    case ORCHARD_GAME_IS_DEAD: return g.dead(e) ? 1.0 : 0.0;
    case ORCHARD_GAME_REVIVE: return g.revive(e, static_cast<float>(arg(0, -1.0))) ? 1.0 : 0.0;
    case ORCHARD_GAME_SET_INVULNERABLE: return g.set_invulnerable(e, arg(0, 1.0) != 0.0) ? 1.0 : 0.0;
    case ORCHARD_GAME_DAMAGE_TAKEN: return g.damage_taken(e);
    case ORCHARD_GAME_DIED: return g.died(e) ? 1.0 : 0.0;
    case ORCHARD_GAME_IGNITE: return g.ignite(e, static_cast<float>(arg(0, 10.0))) ? 1.0 : 0.0;
    case ORCHARD_GAME_EXTINGUISH: return g.extinguish(e) ? 1.0 : 0.0;
    case ORCHARD_GAME_IS_BURNING: return g.burning(e) ? 1.0 : 0.0;
    case ORCHARD_GAME_EXPLODE:
        return g.explode(Vec3{static_cast<float>(arg(0, 0.0)), static_cast<float>(arg(1, 0.0)), static_cast<float>(arg(2, 0.0))}, static_cast<float>(arg(3, 5.0)), static_cast<float>(arg(4, 20000.0)), static_cast<float>(arg(5, 100.0)), e, arg(6, 1.0) != 0.0);
    case ORCHARD_GAME_ADD_TAG: return s && s->add_tag(e, tag) ? 1.0 : 0.0;
    case ORCHARD_GAME_REMOVE_TAG: return s && s->remove_tag(e, tag) ? 1.0 : 0.0;
    case ORCHARD_GAME_HAS_TAG: return s && s->has_tag(e, tag) ? 1.0 : 0.0;
    case ORCHARD_GAME_TAGGED_COUNT: return s ? static_cast<double>(s->tagged(tag).size()) : 0.0;
    case ORCHARD_GAME_DISTANCE: return g.distance(e, o);
    case ORCHARD_GAME_TELEPORT: return g.teleport(e, Vec3{static_cast<float>(arg(0, 0.0)), static_cast<float>(arg(1, 0.0)), static_cast<float>(arg(2, 0.0))}) ? 1.0 : 0.0;
    case ORCHARD_GAME_LOOK_AT: return g.look_at(e, Vec3{static_cast<float>(arg(0, 0.0)), static_cast<float>(arg(1, 0.0)), static_cast<float>(arg(2, 0.0))}) ? 1.0 : 0.0;
    case ORCHARD_GAME_RANDOM: return g.random(arg(0, 0.0), arg(1, 1.0));
    case ORCHARD_GAME_HAS_HEALTH: return s && s->alive(g.holder(e)) ? 1.0 : 0.0;
    default: return op >= 200 ? host_lib(op, e, o, args, count, text) : 0.0;
    }
}

orchard_entity host_game_entity(int op, orchard_entity entity, orchard_entity other, const double* args, int count, const char* text) {
    if (!g_runtime) return 0;
    Gameplay& g = rt().gameplay();
    Scene* s = rt().scene();
    const Entity e = entity_of(entity);
    static_cast<void>(other);
    auto arg = [args, count](int i, double fallback) { return args && i < count ? args[i] : fallback; };
    const std::string name = text ? std::string(text) : std::string();
    switch (op) {
    case ORCHARD_GAME_LAST_ATTACKER: return id_of(g.last_attacker(e));
    case ORCHARD_GAME_PLAY_EFFECT: {
        const Vec3 at{static_cast<float>(arg(0, 0.0)), static_cast<float>(arg(1, 0.0)), static_cast<float>(arg(2, 0.0))};
        return id_of(g.effect(name, at, static_cast<float>(arg(3, 1.0)), e));
    }
    case ORCHARD_GAME_TAGGED_AT: {
        if (!s) return 0;
        const std::vector<Entity> all = s->tagged(name);
        const long long i = static_cast<long long>(arg(0, 0.0));
        return i >= 0 && i < static_cast<long long>(all.size()) ? id_of(all[static_cast<std::size_t>(i)]) : 0;
    }
    case ORCHARD_GAME_NEAREST_TAGGED: {
        const Vec3 from = s && s->alive(e) ? g.position(e) : Vec3{static_cast<float>(arg(0, 0.0)), static_cast<float>(arg(1, 0.0)), static_cast<float>(arg(2, 0.0))};
        return id_of(g.nearest_tagged(name, from, e));
    }
    case ORCHARD_GAME_AI_TARGET: return id_of(rt().ai().target(e));
    case ORCHARD_GAME_AI_MAIN_PLAYER: return id_of(rt().ai().main_player());
    default: return 0;
    }
}

} // namespace

void set_current_runtime(ScriptRuntime* runtime) {
    g_runtime = runtime;
}

const orchard_host_api& host_table() {
    static const orchard_host_api table = [] {
        orchard_host_api t{};
        t.version = ORCHARD_SCRIPT_API_VERSION;
        t.size = sizeof(orchard_host_api);
        t.log = &host_log;
        t.time = &host_time;
        t.is_2d = &host_is_2d;
        t.find = &host_find;
        t.spawn = &host_spawn;
        t.exists = &host_exists;
        t.destroy = &host_destroy;
        t.name = &host_name;
        t.set_name = &host_set_name;
        t.parent = &host_parent;
        t.get_vec = &host_get_vec;
        t.set_vec = &host_set_vec;
        t.push = &host_push;
        t.has_body = &host_has_body;
        t.key_down = &host_key_down;
        t.touching = &host_touching;
        t.raycast = &host_raycast;
        t.overlap_sphere = &host_overlap_sphere;
        t.set_layers = &host_set_layers;
        t.instantiate = &host_instantiate;
        t.particles = &host_particles;
        t.fluid_brush = &host_fluid_brush;
        t.animation = &host_animation;
        t.animation_event = &host_animation_event;
        t.game = &host_game;
        t.game_entity = &host_game_entity;
        t.game_text = &host_game_text;
        t.game_values = &host_game_values;
        return t;
    }();
    return table;
}


namespace {

class NativeModule final : public ScriptModule {
public:
    ~NativeModule() override { close_library(m_handle); }

    bool open(const ScriptBuildResult& built, std::string* error) {
        std::string why;
        m_handle = open_library(built.module, &why);
        if (!m_handle) {
            if (error) *error = "couldn't load the built script: " + why;
            return false;
        }
        auto init = reinterpret_cast<orchard_module_init_fn>(library_symbol(m_handle, "orchard_module_init"));
        auto hooks = reinterpret_cast<orchard_module_hooks_fn>(library_symbol(m_handle, "orchard_module_hooks"));
        m_call = reinterpret_cast<orchard_module_call_fn>(library_symbol(m_handle, "orchard_module_call"));
        m_error = reinterpret_cast<orchard_module_error_fn>(library_symbol(m_handle, "orchard_module_error"));
        if (!init || !hooks || !m_call) {
            if (error) *error = "the built script is missing Orchard's entry points; rebuild it";
            return false;
        }
        if (init(&host_table()) != 0) {
            if (error) *error = m_error ? std::string(m_error()) : std::string("failed while loading");
            return false;
        }
        m_hooks = hooks();
        return true;
    }

    int hooks() const override { return m_hooks; }

    bool call(int hook, std::uint64_t entity, double dt, std::string& error) override {
        if (m_call(hook, entity, dt) == 0) return true;
        error = m_error ? m_error() : "unknown error";
        return false;
    }

private:
    void* m_handle = nullptr;
    orchard_module_call_fn m_call = nullptr;
    orchard_module_error_fn m_error = nullptr;
    int m_hooks = 0;
};

const char* const k_script_headers[] = {
    "orchard/script/orchard_module.cpp",
    "orchard/script/script_api.hpp",
    "orchard/script/orchard_api.h",
    "orchard/script/orchard.hpp",
    "orchard/script/orchard.h",
};

std::vector<std::string> module_link_flags() {
#if defined(_WIN32)
    return {"-static"};
#else
    return {};
#endif
}

std::uint64_t header_hash(const ScriptToolchain& tools) {
    std::uint64_t h = fnv(fnv_start(), std::to_string(ORCHARD_SCRIPT_API_VERSION));
    const fs::path inc(tools.include_dir);
    for (const char* f : k_script_headers) h = fnv(h, read_text(inc / f));
    for (const std::string& f : tools.cxx_flags) h = fnv(h, f);
    for (const std::string& f : module_link_flags()) h = fnv(h, f);
    return h;
}

class WalnutLanguage final : public ScriptLanguage {
public:
    std::string name() const override { return "Walnut"; }
    std::vector<std::string> extensions() const override { return {".wal", ".graph"}; }
    bool available(const ScriptToolchain& tools, std::string* why) const override { return tools.usable(why); }
    ScriptBuildResult build(ScriptRuntime& runtime, const Project& project, const std::string& script) override { return runtime.build_walnut(project, script); }
    std::unique_ptr<ScriptModule> load(ScriptRuntime&, const ScriptBuildResult& built, std::string* error) override { return load_native_module(built, error); }

    std::string starter(const std::string& cls, Dimension d) const override { return starter(cls, d, Axes::y_up); }

    std::string starter(const std::string&, Dimension d, Axes axes) const override {
        std::string s = "import module.orchard\n\n";
        s += "function on_start(orchard::Entity&: self) -> void {\n";
        s += "    orchard::log(\"Hello from \" + self.name());\n";
        s += "} end function on_start\n\n";
        s += "function on_tick(orchard::Entity&: self, double: dt) -> void {\n";
        s += "    self.rotate(" + spin_args(d, axes, "0.0", "45.0 * dt") + ");\n";
        s += "} end function on_tick\n";
        return s;
    }
};

class NativeLanguage final : public ScriptLanguage {
public:
    explicit NativeLanguage(bool c) : m_c(c) {}

    std::string name() const override { return m_c ? "C" : "C++"; }
    std::vector<std::string> extensions() const override {
        if (m_c) return {".c"};
        return {".cpp", ".cc", ".cxx"};
    }

    bool available(const ScriptToolchain& tools, std::string* why) const override {
        if (!tools.has_headers(why)) return false;
        const std::string& compiler = m_c ? tools.cc : tools.cxx;
        if (find_on_path(compiler).empty()) {
            if (why) *why = "can't find the " + std::string(m_c ? "C" : "C++") + " compiler '" + compiler + "'";
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
        const std::string source = read_text(script_path);
        const ScriptHooks hooks = ScriptRuntime::find_native_hooks(source);
        if (!hooks.any()) {
            result.log = script + " has no on_start, on_tick or on_stop function";
            return result;
        }
        const fs::path inc(tools.include_dir);
        const fs::path cache(tools.cache_dir);
        fs::create_directories(cache, ec);

        std::uint64_t base = fnv(header_hash(tools), "native");
        base = fnv(base, tools.cxx);
        const std::string glue_obj = (cache / ("glue-native-" + hex(base) + ".o")).string();
        std::uint64_t h = fnv(fnv(fnv(base, source), script_path.string()), name());
        if (m_c) h = fnv(h, tools.cc);
        std::string stem = safe_stem(script) + (m_c ? "-c-" : "-cpp-") + hex(h);
        const fs::path module = cache / (stem + module_extension());
        result.module = module.string();
        if (!tools.rebuild && fs::is_regular_file(module, ec)) {
            result.ok = true;
            result.cached = true;
            return result;
        }

        const fs::path step_log = cache / (stem + ".log");
        auto fail = [&](const std::string& what) {
            std::string log = read_text(step_log);
            log = replace_all(log, script_path.string(), script);
            result.log = log.empty() ? what : log;
            fs::remove(module, ec);
            return result;
        };
        if (!fs::is_regular_file(glue_obj, ec)) {
            std::vector<std::string> cmd = {tools.cxx, "-std=c++17", "-O2", "-fPIC", "-fvisibility=hidden", "-DORCHARD_MODULE_NATIVE", "-I" + inc.string()};
            cmd.insert(cmd.end(), tools.cxx_flags.begin(), tools.cxx_flags.end());
            cmd.insert(cmd.end(), {"-c", (inc / "orchard" / "script" / "orchard_module.cpp").string(), "-o", glue_obj});
            if (run(cmd, step_log, tools.verbose, runtime) != 0) {
                fs::remove(glue_obj, ec);
                return fail("couldn't build the script glue");
            }
        }

        const fs::path reg = cache / (stem + ".reg.cpp");
        if (!write_text(reg, ScriptRuntime::native_registration(hooks, m_c, script_path.generic_string()))) {
            result.log = "can't write " + reg.string();
            return result;
        }
        std::vector<std::string> link = {tools.cxx, "-std=c++17", "-O2", "-fPIC", "-shared", "-fvisibility=hidden", "-I" + inc.string(), "-I" + script_path.parent_path().string()};
        link.insert(link.end(), tools.cxx_flags.begin(), tools.cxx_flags.end());
        if (m_c) {
            const fs::path obj = cache / (stem + ".o");
            std::vector<std::string> cc = {tools.cc, "-O2", "-fPIC", "-fvisibility=hidden", "-I" + inc.string(), "-I" + script_path.parent_path().string(), "-c", script_path.string(), "-o", obj.string()};
            if (run(cc, step_log, tools.verbose, runtime) != 0) return fail("couldn't compile " + script);
            link.push_back(obj.string());
        }
        link.insert(link.end(), {reg.string(), glue_obj, "-o", module.string()});
        for (const std::string& f : module_link_flags()) link.push_back(f);
        if (run(link, step_log, tools.verbose, runtime) != 0 || !fs::is_regular_file(module, ec)) return fail("couldn't build " + script);
        prune_cache(cache, stem, hex(h), runtime.loaded_modules());
        result.ok = true;
        return result;
    }

    std::unique_ptr<ScriptModule> load(ScriptRuntime&, const ScriptBuildResult& built, std::string* error) override { return load_native_module(built, error); }

    std::string starter(const std::string& cls, Dimension d) const override { return starter(cls, d, Axes::y_up); }

    std::string starter(const std::string&, Dimension d, Axes axes) const override {
        const std::string axis = spin_args(d, axes, "0.0", "45.0 * dt");
        if (m_c) {
            std::string s = "#include <orchard/script/orchard.h>\n\n";
            s += "void on_start(orchard_entity self) {\n";
            s += "    orchard_log(\"Hello from C\");\n";
            s += "    orchard_log(orchard_name(self));\n";
            s += "}\n\n";
            s += "void on_tick(orchard_entity self, double dt) {\n";
            s += "    orchard_rotate(self, " + axis + ");\n";
            s += "}\n";
            return s;
        }
        std::string s = "#include <orchard/script/orchard.hpp>\n\nusing namespace orchard::script;\n\n";
        s += "void on_start(Entity self) {\n";
        s += "    log(\"Hello from \" + self.name());\n";
        s += "}\n\n";
        s += "void on_tick(Entity self, double dt) {\n";
        s += "    self.rotate({" + axis + "});\n";
        s += "}\n";
        return s;
    }

private:
    bool m_c = false;
};

} // namespace

std::string spin_args(Dimension dimension, Axes axes, const std::string& zero, const std::string& amount) {
    int axis = 2;
    if (dimension == Dimension::three_d) axis = axes == Axes::z_up || axes == Axes::z_up_left ? 2 : 1;
    std::string out;
    for (int i = 0; i < 3; ++i) {
        if (i) out += ", ";
        out += i == axis ? amount : zero;
    }
    return out;
}

std::unique_ptr<ScriptModule> load_native_module(const ScriptBuildResult& built, std::string* error) {
    auto m = std::make_unique<NativeModule>();
    if (!m->open(built, error)) return nullptr;
    return m;
}

ScriptHooks find_c_hooks(const std::string& source) {
    return ScriptRuntime::find_native_hooks(source);
}

std::unique_ptr<ScriptLanguage> make_walnut_language() {
    return std::make_unique<WalnutLanguage>();
}

std::unique_ptr<ScriptLanguage> make_cpp_language() {
    return std::make_unique<NativeLanguage>(false);
}

std::unique_ptr<ScriptLanguage> make_c_language() {
    return std::make_unique<NativeLanguage>(true);
}

#if !defined(ORCHARD_WITH_LUA)
std::unique_ptr<ScriptLanguage> make_lua_language() {
    return nullptr;
}
#endif

#if !defined(ORCHARD_WITH_JAVA)
std::unique_ptr<ScriptLanguage> make_java_language() {
    return nullptr;
}
#endif

} // namespace script_detail

using namespace script_detail;

struct ScriptRuntime::Module {
    std::string script;
    std::string path;
    std::unique_ptr<ScriptModule> impl;
    int hooks = 0;
    bool ok = false;
};

std::string ScriptLanguage::starter(const std::string& class_name, Dimension dimension, Axes) const {
    return starter(class_name, dimension);
}

bool ScriptLanguage::handles(const std::string& path) const {
    std::string ext = fs::path(path).extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (const std::string& e : extensions()) {
        if (e == ext) return true;
    }
    return false;
}

bool ScriptToolchain::has_headers(std::string* why) const {
    std::error_code ec;
    if (include_dir.empty() || !fs::is_regular_file(fs::path(include_dir) / "orchard" / "script" / "orchard_module.cpp", ec)) {
        if (why) *why = "can't find Orchard's script headers (orchard/script/orchard_module.cpp) in '" + include_dir + "'; set ORCHARD_INCLUDE_DIR";
        return false;
    }
    if (cache_dir.empty()) {
        if (why) *why = "there's no folder to keep built scripts in";
        return false;
    }
    return true;
}

bool ScriptToolchain::usable(std::string* why) const {
    if (!has_headers(why)) return false;
    std::error_code ec;
    if (!fs::is_regular_file(fs::path(include_dir) / "orchard" / "walnut" / "orchard.walh", ec)) {
        if (why) *why = "can't find module.orchard (orchard/walnut/orchard.walh) in '" + include_dir + "'";
        return false;
    }
    if (walnut.empty()) {
        if (why) *why = "no Walnut compiler was found";
        return false;
    }
    return true;
}

std::string ScriptToolchain::find_include_dir(const std::string& exe_dir) {
    std::vector<fs::path> candidates;
    if (const char* env = std::getenv("ORCHARD_INCLUDE_DIR")) candidates.push_back(env);
    if (!exe_dir.empty()) {
        candidates.push_back(fs::path(exe_dir) / "include");
        candidates.push_back(fs::path(exe_dir).parent_path() / "include");
        candidates.push_back(fs::path(exe_dir).parent_path() / "share" / "orchard" / "include");
    }
#ifdef ORCHARD_SOURCE_INCLUDE_DIR
    candidates.push_back(ORCHARD_SOURCE_INCLUDE_DIR);
#endif
    for (const fs::path& c : candidates) {
        std::error_code ec;
        if (fs::is_regular_file(c / "orchard" / "script" / "orchard_module.cpp", ec) && fs::is_regular_file(c / "orchard" / "walnut" / "orchard.walh", ec)) return c.string();
    }
    return std::string();
}

ScriptRuntime::ScriptRuntime() {
    add_language(make_walnut_language());
    add_language(make_cpp_language());
    add_language(make_c_language());
    add_language(make_lua_language());
    add_language(make_csharp_language());
    add_language(make_java_language());
}

ScriptRuntime::~ScriptRuntime() {
    stop();
    unload_all();
}

void ScriptRuntime::add_language(std::unique_ptr<ScriptLanguage> language) {
    if (!language) return;
    for (auto& existing : m_languages) {
        if (existing->name() == language->name()) {
            existing = std::move(language);
            return;
        }
    }
    m_languages.push_back(std::move(language));
}

ScriptLanguage* ScriptRuntime::language_for(const std::string& script) const {
    for (auto it = m_languages.rbegin(); it != m_languages.rend(); ++it) {
        if ((*it)->handles(script)) return it->get();
    }
    return nullptr;
}

ScriptLanguage* ScriptRuntime::language(const std::string& name) const {
    for (const auto& l : m_languages) {
        if (l->name() == name) return l.get();
    }
    return nullptr;
}

std::vector<ScriptLanguage*> ScriptRuntime::languages() const {
    std::vector<ScriptLanguage*> out;
    for (const auto& l : m_languages) out.push_back(l.get());
    return out;
}

std::string ScriptRuntime::starter(const std::string& script, Dimension dimension) const {
    return starter(script, dimension, Axes::y_up);
}

std::string ScriptRuntime::starter(const std::string& script, Dimension dimension, Axes axes) const {
    ScriptLanguage* l = language_for(script);
    if (!l) return std::string();
    std::string cls = fs::path(script).stem().string();
    for (char& c : cls) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') c = '_';
    }
    if (cls.empty() || std::isdigit(static_cast<unsigned char>(cls[0]))) cls = "Script" + cls;
    return l->starter(cls, dimension, axes);
}

bool ScriptRuntime::create_script(const Project& project, const std::string& script, Dimension dimension, std::string* error) const {
    const std::string text = starter(script, dimension, project.info().axes);
    if (text.empty()) {
        if (error) *error = "Orchard doesn't know how to script '" + fs::path(script).extension().string() + "' files";
        return false;
    }
    const fs::path path = project.path_of(script);
    std::error_code ec;
    if (fs::exists(path, ec)) {
        if (error) *error = script + " already exists";
        return false;
    }
    fs::create_directories(path.parent_path(), ec);
    if (!write_text(path, text)) {
        if (error) *error = "can't write " + script;
        return false;
    }
    return true;
}

std::vector<std::string> ScriptRuntime::loaded_modules() const {
    std::vector<std::string> out;
    for (const auto& [key, m] : m_modules) out.push_back(m->path);
    return out;
}

std::string ScriptRuntime::key_name(const std::string& key) {
    std::string k;
    for (char c : key)
        if (c != ' ' && c != '_' && c != '-') k += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (k == "up" || k == "uparrow" || k == "arrowup") return "up";
    if (k == "down" || k == "downarrow" || k == "arrowdown") return "down";
    if (k == "left" || k == "leftarrow" || k == "arrowleft") return "left";
    if (k == "right" || k == "rightarrow" || k == "arrowright") return "right";
    if (k == "leftshift" || k == "rightshift" || k == "shiftleft" || k == "shiftright") return "shift";
    if (k == "leftcontrol" || k == "rightcontrol" || k == "controlleft" || k == "controlright" || k == "control" || k == "leftctrl" || k == "rightctrl") return "ctrl";
    if (k == "leftalt" || k == "rightalt" || k == "altleft" || k == "altright") return "alt";
    if (k == " " || k == "spacebar" || k.empty()) return key == " " ? std::string("space") : k;
    if (k == "return") return "enter";
    if (k == "esc") return "escape";
    return k;
}

void ScriptRuntime::set_key(const std::string& key, bool down) {
    const std::string k = key_name(key);
    if (down) {
        m_keys.insert(key);
        m_keys.insert(k);
    } else {
        m_keys.erase(key);
        m_keys.erase(k);
    }
}

void ScriptRuntime::log(const std::string& message, bool error) const {
    if (m_log) m_log(message, error);
}

ScriptHooks ScriptRuntime::find_hooks(const std::string& source) {
    ScriptHooks hooks;
    const std::regex pattern(R"((^|\n)[ \t]*function[ \t]+(on_start|on_tick|on_stop)[ \t]*\(([^)]*)\))");
    for (std::sregex_iterator it(source.begin(), source.end(), pattern), end; it != end; ++it) {
        const std::string name = (*it)[2].str();
        const int n = param_count((*it)[3].str());
        if (name == "on_start") hooks.start_params = n;
        else if (name == "on_tick") hooks.tick_params = n;
        else hooks.stop_params = n;
    }
    return hooks;
}

ScriptHooks ScriptRuntime::find_native_hooks(const std::string& source) {
    ScriptHooks hooks;
    const std::regex pattern(R"((^|[\s;}])void[ \t\r\n]+(on_start|on_tick|on_stop)[ \t]*\(([^)]*)\)[ \t\r\n]*\{)");
    for (std::sregex_iterator it(source.begin(), source.end(), pattern), end; it != end; ++it) {
        const std::string name = (*it)[2].str();
        const int n = param_count((*it)[3].str());
        if (name == "on_start") hooks.start_params = n;
        else if (name == "on_tick") hooks.tick_params = n;
        else hooks.stop_params = n;
    }
    return hooks;
}

std::string ScriptRuntime::with_hooks(const std::string& source, const ScriptHooks& hooks) {
    std::string out = source;
    if (!out.empty() && out.back() != '\n') out += '\n';
    auto add = [&out](const char* hook, const char* fn, int params) {
        if (params < 0) return;
        out += "\norchard::";
        out += hook;
        out += "([]() -> void {\n";
        if (params == 0) {
            out += "    ";
            out += fn;
            out += "();\n";
        } else {
            out += "    orchard::Entity: orchard_self = orchard::current();\n    ";
            out += fn;
            out += params >= 2 ? "(orchard_self, orchard::dt());\n" : "(orchard_self);\n";
        }
        out += "});\n";
    };
    add("on_start", "on_start", hooks.start_params);
    add("on_tick", "on_tick", hooks.tick_params);
    add("on_stop", "on_stop", hooks.stop_params);
    return out;
}

std::string ScriptRuntime::native_registration(const ScriptHooks& hooks, bool c_linkage, const std::string& include_path) {
    std::string out;
    if (!c_linkage) out += "#include \"" + include_path + "\"\n";
    out += "#include <orchard/script/script_api.hpp>\n\n";
    auto proto = [&out](const char* fn, int params) {
        if (params < 0) return;
        out += "extern \"C\" void ";
        out += fn;
        out += params == 0 ? "(void);\n" : params == 1 ? "(orchard_entity);\n" : "(orchard_entity, double);\n";
    };
    if (c_linkage) {
        proto("on_start", hooks.start_params);
        proto("on_tick", hooks.tick_params);
        proto("on_stop", hooks.stop_params);
        out += "\n";
    }
    out += "void orchard_register_hooks() {\n";
    auto add = [&out, c_linkage](const char* fn, int params) {
        if (params < 0) return;
        out += "    orchard::script::World::";
        out += fn;
        out += "([]() {\n";
        if (params == 0) {
            out += "        ";
            out += fn;
            out += "();\n";
        } else {
            out += c_linkage ? "        orchard_entity orchard_self = orchard::script::Entity::current().id();\n"
                             : "        orchard::script::Entity orchard_self = orchard::script::Entity::current();\n";
            out += "        ";
            out += fn;
            out += params >= 2 ? "(orchard_self, orchard::script::World::dt());\n" : "(orchard_self);\n";
        }
        out += "    });\n";
    };
    add("on_start", hooks.start_params);
    add("on_tick", hooks.tick_params);
    add("on_stop", hooks.stop_params);
    out += "}\n";
    return out;
}

std::string ScriptRuntime::walnut_source(const Project& project, const std::string& script, std::string* error) const {
    const fs::path path = project.path_of(script);
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
        if (error) *error = script + " doesn't exist";
        return std::string();
    }
    const std::string text = read_text(path);
    if (path.extension() != ".graph") return text;
    std::string why;
    const std::optional<graph::Graph> g = graph::Graph::load(text, &why);
    if (!g) {
        if (error) *error = script + ": " + why;
        return std::string();
    }
    std::vector<std::string> errors;
    std::string code = g->to_walnut(&errors);
    if (!errors.empty()) {
        if (error) *error = script + ": " + errors.front();
        return std::string();
    }
    return code;
}

ScriptBuildResult ScriptRuntime::build(const Project& project, const std::string& script) {
    ScriptLanguage* lang = language_for(script);
    ScriptBuildResult result;
    if (!lang) {
        result.script = script;
        result.log = "Orchard doesn't know how to run '" + fs::path(script).extension().string() + "' scripts";
        return result;
    }
    result = lang->build(*this, project, script);
    result.script = script;
    result.language = lang->name();
    if (result.ok) m_built[script] = result;
    return result;
}

ScriptBuildResult ScriptRuntime::build_walnut(const Project& project, const std::string& script) {
    ScriptBuildResult result;
    result.script = script;
    result.language = "Walnut";
    std::string why;
    if (!m_tools.usable(&why)) {
        result.log = why;
        return result;
    }
    const std::string source = walnut_source(project, script, &why);
    if (source.empty()) {
        result.log = why.empty() ? script + " is empty" : why;
        return result;
    }
    const ScriptHooks hooks = find_hooks(source);
    if (!hooks.any()) {
        result.log = script + " has no on_start, on_tick or on_stop function";
        return result;
    }
    const fs::path inc(m_tools.include_dir);
    const fs::path cache(m_tools.cache_dir);
    std::error_code ec;
    fs::create_directories(cache, ec);

    std::uint64_t base = header_hash(m_tools);
    base = fnv(base, read_text(inc / "orchard/walnut/orchard.walh"));
    base = fnv(base, m_tools.walnut);
    base = fnv(base, m_tools.cxx);

    const std::string glue_obj = (cache / ("glue-" + hex(base) + ".o")).string();
    const fs::path script_path = project.path_of(script);
    const std::string script_dir = script_path.parent_path().string();
    const std::uint64_t h = fnv(fnv(base, source), script_dir);
    std::string stem = safe_stem(script);
    stem += (fs::path(script).extension() == ".graph" ? "-graph-" : "-") + hex(h);
    const fs::path module = cache / (stem + module_extension());
    result.module = module.string();

    if (!m_tools.rebuild && fs::is_regular_file(module, ec)) {
        result.ok = true;
        result.cached = true;
        m_built[script] = result;
        return result;
    }

    const fs::path step_log = cache / (stem + ".log");
    if (!fs::is_regular_file(glue_obj, ec)) {
        std::vector<std::string> cmd = {m_tools.cxx, "-std=c++17", "-O2", "-fPIC", "-fvisibility=hidden", "-I" + inc.string()};
        cmd.insert(cmd.end(), m_tools.cxx_flags.begin(), m_tools.cxx_flags.end());
        cmd.insert(cmd.end(), {"-c", (inc / "orchard" / "script" / "orchard_module.cpp").string(), "-o", glue_obj});
        if (run(cmd, step_log, m_tools.verbose, *this) != 0) {
            result.log = "couldn't build the script glue:\n" + read_text(step_log);
            fs::remove(glue_obj, ec);
            return result;
        }
    }

    const fs::path generated = cache / (stem + ".wal");
    if (!write_text(generated, with_hooks(source, hooks))) {
        result.log = "can't write " + generated.string();
        return result;
    }
    std::vector<std::string> w = {m_tools.walnut, "build", generated.string(), glue_obj, "-o", module.string(), "--no-cache", "--cxx", m_tools.cxx,
                                  "-M", (inc / "orchard" / "walnut").string(), "-M", script_dir, "-I", script_dir};
    for (const char* f : {"-fPIC", "-shared", "-Dmain=orchard_walnut_main"}) w.insert(w.end(), {"--cxx-flag", f});
    w.insert(w.end(), {"--cxx-flag", "-I" + inc.string()});
    for (const std::string& f : m_tools.cxx_flags) w.insert(w.end(), {"--cxx-flag", f});
    for (const std::string& f : module_link_flags()) w.insert(w.end(), {"--cxx-flag", f});
    const int status = run(w, step_log, m_tools.verbose, *this);
    std::string log = read_text(step_log);
    log = replace_all(log, generated.string(), script);
    log = replace_all(log, generated.filename().string(), script);
    result.log = log;
    if (status != 0 || !fs::is_regular_file(module, ec)) {
        fs::remove(module, ec);
        if (result.log.empty()) result.log = "walnut couldn't build " + script;
        return result;
    }
    prune_cache(cache, stem, hex(h), loaded_modules());
    result.ok = true;
    m_built[script] = result;
    return result;
}

std::vector<ScriptBuildResult> ScriptRuntime::build_all(const Project& project, const Scene& scene) {
    std::set<std::string> scripts;
    for (Entity e : scene.entities()) {
        if (!scene.script(e).empty()) scripts.insert(scene.script(e));
    }
    for (const std::string& path : project.entity_templates()) {
        const std::optional<Scene> t = Scene::load_file(project.path_of(path));
        if (!t) continue;
        for (Entity e : t->entities())
            if (!t->script(e).empty()) scripts.insert(t->script(e));
    }
    std::vector<ScriptBuildResult> out;
    for (const std::string& s : scripts) out.push_back(build(project, s));
    return out;
}

ScriptRuntime::Module* ScriptRuntime::load(const ScriptBuildResult& built) {
    const std::string key = built.module.empty() ? built.script : built.module;
    auto found = m_modules.find(key);
    if (found != m_modules.end()) return found->second->ok ? found->second.get() : nullptr;
    auto m = std::make_unique<Module>();
    m->script = built.script;
    m->path = key;
    Module* raw = m.get();
    m_modules[key] = std::move(m);
    ScriptLanguage* lang = language_for(built.script);
    if (!lang) {
        log(built.script + ": Orchard doesn't know how to run this kind of script", true);
        return nullptr;
    }
    set_current_runtime(this);
    std::string error;
    raw->impl = lang->load(*this, built, &error);
    if (!raw->impl) {
        log(built.script + ": " + (error.empty() ? std::string("failed while loading") : error), true);
        return nullptr;
    }
    raw->hooks = raw->impl->hooks();
    raw->ok = true;
    return raw;
}

void ScriptRuntime::call(Instance& instance, int hook, double dt) {
    if (instance.broken || !instance.module || !instance.module->impl || !(instance.module->hooks & hook)) return;
    if (!m_scene || !m_scene->alive(instance.entity)) return;
    std::string why;
    ++m_call_depth;
    const bool ok = instance.module->impl->call(hook, instance.entity.id(), dt, why);
    --m_call_depth;
    if (!ok) {
        instance.broken = true;
        if (why.empty()) why = "unknown error";
        log(instance.module->script + " on " + m_scene->name(instance.entity) + ": " + why + " (stopped for this entity)", true);
    }
}

void ScriptRuntime::call_at(std::size_t index, int hook, double dt) {
    if (index >= m_instances.size()) return;
    Instance copy = m_instances[index];
    call(copy, hook, dt);
    if (index < m_instances.size() && m_instances[index].entity == copy.entity) m_instances[index].broken = copy.broken;
    if (m_call_depth == 0 && !m_pending_starts.empty()) start_pending();
}

void ScriptRuntime::start_pending() {
    for (std::size_t guard = 0; !m_pending_starts.empty() && guard < 100000; ++guard) {
        const std::size_t i = m_pending_starts.front();
        m_pending_starts.erase(m_pending_starts.begin());
        if (i >= m_instances.size()) continue;
        Instance copy = m_instances[i];
        call(copy, ORCHARD_HOOK_START, 0.0);
        if (i < m_instances.size() && m_instances[i].entity == copy.entity) m_instances[i].broken = copy.broken;
    }
}

std::string ScriptRuntime::entity_template_file(const std::string& name) {
    std::string p = name;
    for (char& c : p)
        if (c == '\\') c = '/';
    while (!p.empty() && (p.front() == '/' || p.front() == '.')) {
        if (p.rfind("./", 0) == 0) p.erase(0, 2);
        else if (p.front() == '/') p.erase(0, 1);
        else break;
    }
    if (p.find('/') == std::string::npos) p = "Entities/" + p;
    if (p.size() < 7 || p.compare(p.size() - 7, 7, ".entity") != 0) p += ".entity";
    return p;
}

const Scene* ScriptRuntime::entity_template(const std::string& name) {
    if (!m_project) return nullptr;
    const std::string file = entity_template_file(name);
    auto found = m_templates.find(file);
    if (found == m_templates.end()) {
        std::string error;
        std::optional<Scene> loaded = Scene::load_file(m_project->path_of(file), &error);
        if (!loaded) log(file + ": " + (error.empty() ? std::string("can't open this entity template") : error), true);
        found = m_templates.emplace(file, std::move(loaded)).first;
    }
    return found->second ? &*found->second : nullptr;
}

Entity ScriptRuntime::instantiate(const std::string& name, const Transform& at, Entity parent) {
    if (!m_scene) return Entity{};
    const Scene* t = entity_template(name);
    if (!t) return Entity{};
    const Entity root = m_scene->instantiate(*t, entity_template_file(name), parent, at);
    if (!m_scene->alive(root)) return root;
    if (m_physics) m_physics->added(root);
    if (!m_running || !m_project) return root;
    const std::size_t first = m_instances.size();
    for (Entity e : m_scene->subtree(root)) {
        const std::string& script = m_scene->script(e);
        if (script.empty()) continue;
        auto built = m_built.find(script);
        ScriptBuildResult result;
        if (built == m_built.end() || !built->second.ok) {
            result = build(*m_project, script);
            if (!result.ok) {
                log(script + ": " + result.log, true);
                m_built.erase(script);
                continue;
            }
        } else {
            result = built->second;
        }
        Module* m = load(result);
        if (m) m_instances.push_back(Instance{e, m, false});
    }
    for (std::size_t i = first; i < m_instances.size(); ++i) m_pending_starts.push_back(i);
    if (m_call_depth == 0) start_pending();
    return root;
}

void ScriptRuntime::particles(Entity e, int action, int count) {
    if (m_particle_commands.size() < 100000) m_particle_commands.push_back(ParticleCommand{e, action, count});
}

double ScriptRuntime::animation(Entity e, int action, const std::string& clip, double value) {
    if (!m_scene || !m_scene->alive(e)) return 0.0;
    Entity target = e;
    if (!m_scene->animator(target) && !m_scene->model(target)) {
        for (Entity c : m_scene->subtree(e))
            if (m_scene->animator(c) || m_scene->model(c)) {
                target = c;
                break;
            }
    }
    const auto status = m_animation_status.find(target.id());
    if (action == ORCHARD_ANIM_TIME) return status == m_animation_status.end() ? 0.0 : status->second.time;
    if (action == ORCHARD_ANIM_LENGTH) return status == m_animation_status.end() ? 0.0 : status->second.length;
    if (action == ORCHARD_ANIM_PLAYING) {
        const Animator* a = m_scene->animator(target);
        return a && a->playing && status != m_animation_status.end() && status->second.playing ? 1.0 : 0.0;
    }
    if (!m_scene->model(target) && !m_scene->animator(target)) return 0.0;
    Animator& a = m_scene->add_animator(target);
    switch (action) {
    case ORCHARD_ANIM_PLAY:
    case ORCHARD_ANIM_PLAY_ONCE:
        if (!clip.empty()) a.clip = clip;
        a.loop = action == ORCHARD_ANIM_PLAY;
        a.blend = static_cast<float>(std::max(0.0, value));
        a.start = 0.0f;
        a.playing = true;
        ++a.restart;
        break;
    case ORCHARD_ANIM_STOP: a.playing = false; break;
    case ORCHARD_ANIM_RESUME: a.playing = true; break;
    case ORCHARD_ANIM_SPEED: a.speed = static_cast<float>(value); break;
    case ORCHARD_ANIM_SET_TIME:
        a.start = static_cast<float>(std::max(0.0, value));
        ++a.restart;
        break;
    default: break;
    }
    return 0.0;
}

std::string ScriptRuntime::next_animation_event(Entity e) {
    auto it = m_animation_events.find(e.id());
    if (it == m_animation_events.end() || it->second.empty()) {
        if (m_scene && m_scene->alive(e))
            for (Entity c : m_scene->subtree(e)) {
                auto ci = m_animation_events.find(c.id());
                if (ci != m_animation_events.end() && !ci->second.empty()) {
                    it = ci;
                    break;
                }
            }
        if (it == m_animation_events.end() || it->second.empty()) return std::string();
    }
    const std::string out = it->second.front();
    it->second.erase(it->second.begin());
    return out;
}

void ScriptRuntime::push_animation_event(Entity e, const std::string& name) {
    std::vector<std::string>& q = m_animation_events[e.id()];
    if (q.size() < 256) q.push_back(name);
}

void ScriptRuntime::set_animation_status(Entity e, double time, double length, bool playing) {
    m_animation_status[e.id()] = AnimationStatus{time, length, playing};
}

std::vector<ParticleCommand> ScriptRuntime::take_particle_commands() {
    std::vector<ParticleCommand> out;
    out.swap(m_particle_commands);
    return out;
}

bool ScriptRuntime::start(const Project& project, Scene& scene) {
    stop();
    m_scene = &scene;
    m_project = &project;
    m_templates.clear();
    m_particle_commands.clear();
    m_animation_status.clear();
    m_animation_events.clear();
    m_pending_starts.clear();
    m_time = 0.0;
    m_instances.clear();
    set_current_runtime(this);
    bool all_ok = true;
    for (Entity e : scene.entities()) {
        const std::string& script = scene.script(e);
        if (script.empty()) continue;
        auto built = m_built.find(script);
        ScriptBuildResult result;
        if (built == m_built.end() || !built->second.ok) {
            result = build(project, script);
            if (!result.ok) {
                log(script + ": " + result.log, true);
                all_ok = false;
                m_built.erase(script);
                continue;
            }
        } else {
            result = built->second;
        }
        Module* m = load(result);
        if (!m) {
            all_ok = false;
            continue;
        }
        m_instances.push_back(Instance{e, m, false});
    }
    m_running = true;
    m_gameplay.start(&scene, m_physics);
    m_sound_commands.clear();
    m_sounds_playing.clear();
    m_source_sounds.clear();
    m_bags.clear();
    m_ai.set_log([this](const std::string& msg, bool error) { log(msg, error); });
    m_ai.set_spawner([this](const std::string& name, Vec3 at) {
        Transform t;
        t.position = at;
        return instantiate(name, t);
    });
    m_ai.set_sound_player([this](const std::string& clip, Vec3 at, Entity follow, float volume) { play_sound(clip, at, follow, true, volume); });
    m_ai.start(&scene, m_physics, &m_gameplay);
    std::set<std::string> defined;
    auto define_file = [&](const std::string& b, bool needed, const std::string& who) {
        if (defined.count(b)) return;
        defined.insert(b);
        std::ifstream in(project.path_of(b));
        if (!in) {
            if (needed) log(who + ": can't find the behavior file " + b, true);
            return;
        }
        std::stringstream text;
        text << in.rdbuf();
        std::string why;
        std::string tree = text.str();
        if (graph::is_brain_file(b)) {
            const std::optional<std::string> made = graph::brain_file_behavior(tree, &why);
            if (!made) {
                log(b + ": " + why, true);
                return;
            }
            tree = *made;
        }
        if (!m_ai.define_behavior(b, tree, &why)) log(b + ": " + why, true);
    };
    auto is_file = [](const std::string& b) {
        auto ends = [&b](const std::string& x) { return b.size() > x.size() && b.compare(b.size() - x.size(), x.size(), x) == 0; };
        return ends(".bt") || ends(".behavior") || ends(".brain");
    };
    for (Entity e : scene.entities()) {
        const Agent* a = scene.agent(e);
        if (!a || a->behavior.empty() || a->behavior.find('\n') != std::string::npos || !is_file(a->behavior)) continue;
        define_file(a->behavior, true, scene.name(e));
    }
    for (const std::string& f : project.behaviors()) define_file(f, false, f);
    for (Entity e : scene.entities())
        if (const SoundSource* src = scene.sound(e))
            if (src->autoplay && !src->clip.empty()) play_sound_source(e);
    set_current_runtime(this);
    const std::size_t starting = m_instances.size();
    for (std::size_t i = 0; i < starting; ++i) call_at(i, ORCHARD_HOOK_START, 0.0);
    return all_ok;
}

void ScriptRuntime::tick(double dt) {
    if (!m_running) return;
    set_current_runtime(this);
    m_time += dt;
    for (std::size_t i = 0; i < m_instances.size(); ++i) call_at(i, ORCHARD_HOOK_TICK, dt);
    if (m_gameplay.running()) m_gameplay.tick(dt, m_time);
    if (m_ai.running()) m_ai.tick(dt, m_time);
}

void ScriptRuntime::stop() {
    if (!m_running) return;
    set_current_runtime(this);
    for (std::size_t i = 0; i < m_instances.size(); ++i) call_at(i, ORCHARD_HOOK_STOP, 0.0);
    m_instances.clear();
    m_running = false;
    m_gameplay.stop();
    m_ai.stop();
    stop_all_sounds();
    m_bags.clear();
    unload_all();
    m_scene = nullptr;
    m_project = nullptr;
    m_templates.clear();
    set_current_runtime(nullptr);
}

int ScriptRuntime::play_sound(const std::string& clip, Vec3 at, Entity follow, bool positional, float volume, float pitch, bool loop, float range, float ai_loudness) {
    if (clip.empty()) return 0;
    SoundCommand c;
    c.op = SoundOp::play;
    c.id = m_next_sound++;
    c.clip = clip;
    c.at = at;
    c.follow = follow;
    c.positional = positional;
    c.volume = std::max(0.0f, volume);
    c.pitch = std::max(0.01f, pitch);
    c.loop = loop;
    c.range = std::max(0.1f, range);
    if (m_sound_commands.size() < 4096) m_sound_commands.push_back(c);
    m_sounds_playing.insert(c.id);
    if (ai_loudness > 0.0f && m_ai.running()) {
        const Vec3 where = m_scene && m_scene->alive(follow) ? m_gameplay.position(follow) : at;
        m_ai.make_noise(where, ai_loudness, follow, "sound");
    }
    return c.id;
}

int ScriptRuntime::play_sound_source(Entity e) {
    if (!m_scene) return 0;
    const SoundSource* s = m_scene->sound(e);
    if (!s || s->clip.empty()) return 0;
    stop_sound_source(e);
    const int id = play_sound(s->clip, m_gameplay.position(e), e, s->spatial, s->volume, s->pitch, s->loop, s->range, s->ai_hears ? s->loudness : 0.0f);
    for (SoundCommand& c : m_sound_commands)
        if (c.id == id) c.min_distance = s->min_distance;
    m_source_sounds[e.id()] = id;
    return id;
}

void ScriptRuntime::stop_sound_source(Entity e) {
    const auto it = m_source_sounds.find(e.id());
    if (it == m_source_sounds.end()) return;
    stop_sound(it->second);
    m_source_sounds.erase(it);
}

void ScriptRuntime::stop_sound(int id) {
    if (!m_sounds_playing.count(id)) return;
    SoundCommand c;
    c.op = SoundOp::stop;
    c.id = id;
    m_sound_commands.push_back(c);
    m_sounds_playing.erase(id);
}

void ScriptRuntime::set_sound_volume(int id, float volume) {
    SoundCommand c;
    c.op = SoundOp::volume;
    c.id = id;
    c.volume = std::max(0.0f, volume);
    m_sound_commands.push_back(c);
}

void ScriptRuntime::set_sound_pitch(int id, float pitch) {
    SoundCommand c;
    c.op = SoundOp::pitch;
    c.id = id;
    c.pitch = std::max(0.01f, pitch);
    m_sound_commands.push_back(c);
}

void ScriptRuntime::pause_sound(int id, bool paused) {
    SoundCommand c;
    c.op = paused ? SoundOp::pause : SoundOp::resume;
    c.id = id;
    m_sound_commands.push_back(c);
}

bool ScriptRuntime::sound_playing(int id) const {
    return m_sounds_playing.count(id) > 0;
}

void ScriptRuntime::stop_all_sounds() {
    SoundCommand c;
    c.op = SoundOp::stop_all;
    m_sound_commands.push_back(c);
    m_sounds_playing.clear();
    m_source_sounds.clear();
}

std::vector<SoundCommand> ScriptRuntime::take_sound_commands() {
    std::vector<SoundCommand> out;
    out.swap(m_sound_commands);
    return out;
}

void ScriptRuntime::sound_finished(int id) {
    m_sounds_playing.erase(id);
    for (auto it = m_source_sounds.begin(); it != m_source_sounds.end();) {
        if (it->second == id) it = m_source_sounds.erase(it);
        else ++it;
    }
}

ShuffleBag<std::string>& ScriptRuntime::bag(const std::string& name) {
    auto it = m_bags.find(name);
    if (it == m_bags.end()) {
        it = m_bags.emplace(name, ShuffleBag<std::string>(m_random)).first;
    }
    return it->second;
}

void ScriptRuntime::unload_all() {
    m_modules.clear();
}

} // namespace orchard
