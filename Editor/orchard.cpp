#include <orchard/orchard.hpp>
#include <orchard/scripting.hpp>
#include <xylem/xylem.hpp>
#include <chlorophyll/chlorophyll.hpp>

#include "axes_preview.hpp"
#include "gpu_view.hpp"

#include <grove/app.hpp>
#include <grove/bundle.hpp>
#include <grove/canvas.hpp>
#include <grove/grove.hpp>
#include <grove/native.hpp>
#include <grove/windowing.hpp>
#include <grove/runtime/runtime.hpp>

#if defined(__ANDROID__)
#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string env(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

std::vector<std::string> fields_of(const std::vector<std::string>& args) {
    if (args.size() != 1) return args;
    std::vector<std::string> out;
    std::string cur;
    for (char c : args[0]) {
        if (c == '\t') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

std::string arg(const std::vector<std::string>& f, std::size_t i) {
    return i < f.size() ? f[i] : std::string();
}

std::string clean(std::string s) {
    for (char& c : s) {
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    }
    return s;
}

std::string number(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3f", static_cast<double>(v));
    std::string s = buf;
    while (s.size() > 1 && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s == "-0" ? "0" : s;
}

float srgb_from_linear(float v) {
    v = std::clamp(v, 0.0f, 1.0f);
    return v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

float linear_from_srgb(float v) {
    v = std::clamp(v, 0.0f, 1.0f);
    return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

std::string hex_color(orchard::Vec3 linear) {
    const float m = std::max({linear.x, linear.y, linear.z, 1.0f});
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x",
                  static_cast<int>(std::lround(srgb_from_linear(linear.x / m) * 255.0f)),
                  static_cast<int>(std::lround(srgb_from_linear(linear.y / m) * 255.0f)),
                  static_cast<int>(std::lround(srgb_from_linear(linear.z / m) * 255.0f)));
    return buf;
}

bool parse_hex_color(const std::string& text, orchard::Vec3& out) {
    std::string h = text;
    if (!h.empty() && h[0] == '#') h.erase(0, 1);
    if (h.size() == 3) h = std::string{h[0], h[0], h[1], h[1], h[2], h[2]};
    if (h.size() != 6 && h.size() != 8) return false;
    for (std::size_t i = 0; i < 6; ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(h[i]))) return false;
    }
    const unsigned long v = std::strtoul(h.substr(0, 6).c_str(), nullptr, 16);
    out = {linear_from_srgb(static_cast<float>((v >> 16) & 0xFFu) / 255.0f), linear_from_srgb(static_cast<float>((v >> 8) & 0xFFu) / 255.0f), linear_from_srgb(static_cast<float>(v & 0xFFu) / 255.0f)};
    return true;
}

bool parse_vec3(const std::string& text, orchard::Vec3& out) {
    float v[3] = {0.0f, 0.0f, 0.0f};
    std::size_t start = 0;
    for (int i = 0; i < 3; ++i) {
        const std::size_t comma = text.find(',', start);
        const std::string part = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (part.empty()) return false;
        v[i] = static_cast<float>(std::atof(part.c_str()));
        if (comma == std::string::npos) {
            if (i != 2) return false;
            break;
        }
        start = comma + 1;
    }
    out = {v[0], v[1], v[2]};
    return true;
}

std::string vec_text(orchard::Vec3 v) {
    return number(v.x) + "," + number(v.y) + "," + number(v.z);
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

struct Section {
    const char* key;
    const char* label;
    const char* icon;
    const char* one;
};

const std::vector<Section>& outline_sections() {
    static const std::vector<Section> all = {
        {"bodies", "Rigid Bodies", "◆", "Rigid body"},
        {"containers", "Containers", "▬", "Container (doesn't move)"},
        {"triggers", "Triggers", "◇", "Trigger zone"},
        {"lights", "Lights", "✦", "Light"},
        {"fluids", "Fluids", "≈", "Fluid volume"},
        {"particles", "Particles", "✺", "Particle emitter"},
        {"cameras", "Cameras", "▣", "Camera"},
        {"scripted", "Scripted", "▸", "Scripted"},
        {"groups", "Groups", "▤", "Group"},
        {"other", "Empty", "○", "Empty"},
    };
    return all;
}

const char* section_icon(const std::string& key) {
    for (const Section& s : outline_sections()) {
        if (key == s.key) return s.icon;
    }
    return "○";
}

std::string options_of(const std::vector<std::pair<std::string, std::string>>& items) {
    std::string out;
    for (std::size_t i = 0; i < items.size(); ++i) out += (i ? "|" : "") + items[i].first + "=" + items[i].second;
    return out;
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string find_ui() {
    std::vector<fs::path> candidates;
    const std::string over = env("ORCHARD_UI_DIR");
    if (!over.empty()) candidates.push_back(over);
    const fs::path exe = fs::path(grove::executable_path()).parent_path();
    candidates.push_back(exe / "ui");
    candidates.push_back(exe.parent_path() / "share" / "orchard" / "ui");
#ifdef ORCHARD_SOURCE_UI_DIR
    candidates.push_back(ORCHARD_SOURCE_UI_DIR);
#endif
    for (const fs::path& c : candidates) {
        std::error_code ec;
        if (fs::is_regular_file(c / "orchard.grove", ec)) return (c / "orchard.grove").string();
    }
    return std::string();
}

orchard::ScriptToolchain script_toolchain(const std::string& project_folder) {
    const grove::ScriptTools grove_tools = grove::default_script_tools((fs::path(project_folder) / "project.orchard").string());
    orchard::ScriptToolchain tools;
    tools.walnut = grove_tools.walnut;
    tools.cxx = grove_tools.cxx;
    if (grove_tools.compiler_from_walnut) {
        const std::string found = grove::walnut_compiler(grove_tools.walnut);
        if (!found.empty()) tools.cxx = found;
    }
    tools.include_dir = orchard::ScriptToolchain::find_include_dir(fs::path(grove::executable_path()).parent_path().string());
    tools.cache_dir = (fs::path(project_folder) / ".orchard" / "scripts").string();
    tools.verbose = !env("ORCHARD_VERBOSE").empty();
    return tools;
}

class Session {
public:
    using Log = std::function<void(const std::string&, bool)>;

    explicit Session(Log log) : m_log(std::move(log)) {
        m_scripts.set_log(m_log);
    }

    ~Session() {
        if (m_build.valid()) m_build.wait();
        stop_now();
    }

    bool playing() const { return m_playing; }
    bool building() const { return m_build.valid(); }
    orchard::ScriptRuntime& scripts() { return m_scripts; }
    xylem::World* world() { return m_world.get(); }

    void begin(const orchard::Project& project, orchard::Scene& scene) {
        if (m_playing || m_build.valid()) return;
        m_project = &project;
        m_scene = &scene;
        m_snapshot = scene;
        m_scripts.set_toolchain(script_toolchain(project.folder()));
        bool any_script = false;
        for (orchard::Entity e : scene.entities()) any_script = any_script || !scene.script(e).empty();
        if (!any_script) {
            start_now();
            return;
        }
        m_log("Building scripts...", false);
        orchard::Scene* target = &scene;
        m_build = std::async(std::launch::async, [this, &project, target] { return m_scripts.build_all(project, *target); });
    }

    void poll() {
        if (!m_build.valid()) return;
        if (m_build.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        const std::vector<orchard::ScriptBuildResult> results = m_build.get();
        for (const orchard::ScriptBuildResult& r : results) {
            if (!r.ok) m_log(r.script + " didn't build:\n" + r.log, true);
            else if (!r.cached) m_log("Built " + r.script, false);
        }
        start_now();
    }

    double time() const { return m_time; }

    void advance(double dt) {
        if (!m_playing) return;
        dt = std::min(std::max(dt, 0.0), 0.1);
        m_time += dt;
        if (m_world) m_world->step(dt);
        m_scripts.tick(dt);
    }

    bool stop(orchard::Scene& scene) {
        if (m_build.valid()) {
            m_build.wait();
            m_build.get();
        }
        if (!m_playing) return false;
        stop_now();
        scene = m_snapshot;
        m_log("Stopped", false);
        return true;
    }

private:
    void start_now() {
        if (!m_scene || !m_project) return;
        m_world = std::make_unique<xylem::World>(m_scene->dimension(), xylem::Settings::for_project(m_project->info()));
        m_world->build(*m_scene);
        m_scripts.set_physics(m_world.get());
        m_scripts.start(*m_project, *m_scene);
        m_time = 0.0;
        m_playing = true;
        m_log("Playing: " + std::to_string(m_world->body_count()) + " bodies, " + std::to_string(m_scripts.instance_count()) + " scripts", false);
    }

    void stop_now() {
        if (!m_playing) return;
        m_scripts.stop();
        m_scripts.set_physics(nullptr);
        m_scripts.clear_keys();
        m_world.reset();
        m_playing = false;
    }

    Log m_log;
    orchard::ScriptRuntime m_scripts;
    std::unique_ptr<xylem::World> m_world;
    std::future<std::vector<orchard::ScriptBuildResult>> m_build;
    const orchard::Project* m_project = nullptr;
    orchard::Scene* m_scene = nullptr;
    orchard::Scene m_snapshot;
    bool m_playing = false;
    double m_time = 0.0;
};

class Editor {
public:
    explicit Editor(grove::Project& ui) : m_doc(ui.document), m_session([this](const std::string& m, bool bad) { add_log(m, bad); }) {
        register_natives();
        if (grove::Node* canvas = m_doc.find_by_id("viewport")) {
            grove::set_canvas_draw(m_doc, *canvas, [this](grove::GpuCanvasFrame& f) { frame(f); });
        }
        if (grove::Node* canvas = m_doc.find_by_id("axes-preview")) {
            grove::set_canvas_draw(m_doc, *canvas, [this](grove::GpuCanvasFrame& f) { m_axes_preview.draw(f, m_axes_choice); });
        }
    }

    bool open(const std::string& path, std::string& error) {
        std::optional<orchard::Project> p = orchard::Project::open(path, &error);
        if (!p) return false;
        std::optional<orchard::Scene> s = orchard::Scene::load_file(p->path_of(p->info().main_scene), &error);
        if (!s) {
            error = p->info().main_scene + ": " + error;
            s = orchard::Scene(p->info().dimension);
        }
        m_session.stop(m_scene);
        m_template_path.clear();
        m_main_scene.reset();
        m_project = std::move(p);
        m_scene = std::move(*s);
        m_selected = orchard::Entity{};
        m_dirty = false;
        m_view.set_dimension(m_project->info().dimension);
        sync_view_axes();
        m_view.reset_camera();
        load_prefs();
        m_light_built = false;
        m_light_dirty = true;
        set_animating(wants_animation());
        redraw();
        return true;
    }

private:
    void native(const std::string& name, std::function<std::string(const std::vector<std::string>&)> fn) {
        grove::register_native(m_doc, "orchard." + name, [fn = std::move(fn)](const std::vector<std::string>& args) {
            return fn(fields_of(args));
        });
    }

    void add_log(const std::string& message, bool error) {
        std::istringstream lines(message);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.empty()) continue;
            m_logs.push_back(LogLine{++m_log_seq, line, error});
        }
        while (m_logs.size() > 500) m_logs.pop_front();
        if (error) std::cerr << "orchard: " << message << "\n";
    }

    void frame(grove::GpuCanvasFrame& f) {
        m_session.poll();
        if (m_particle_burst_until != std::chrono::steady_clock::time_point{} && std::chrono::steady_clock::now() >= m_particle_burst_until) {
            m_particle_burst_until = std::chrono::steady_clock::time_point{};
            set_animating(wants_animation());
        }
        if (!m_fly_keys.empty()) fly_step(f.dt);
        if (m_session.playing()) {
            m_session.advance(f.dt);
            if (!m_scene.alive(m_selected)) m_selected = orchard::Entity{};
        }
        double seconds = 0.0;
        if (m_session.playing()) seconds = m_session.time();
        else if (pref("preview_effects", "0") == "1") seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_preview_start).count();
        const std::string trace = pref("viewport.trace", "off");
        if (trace != "off" && m_project && m_view.dimension() == orchard::Dimension::three_d) {
            chlorophyll::World& lw = light_world();
            if (m_session.playing() || seconds > 0.0) lw.animate(m_scene, seconds);
            const fizmo::lighting::LightingSettings s = chlorophyll::settings_for(m_project->info(), trace == "photon" ? orchard::LightingLevel::photon : orchard::LightingLevel::path_traced);
            fizmo::lighting::LightingSettings preview = s;
            if (preview.firefly_clamp <= 0.0f) preview.firefly_clamp = 8.0f;
            const bool settling = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_view_moved).count() < 0.2 || m_drag || !m_fly_keys.empty();
            draw_editor_view(f, seconds);
            if (!settling) m_gpu.draw_traced_over(f, lw.lighting_scene(), preview, lw.camera_for(m_view, f.viewport_width, f.viewport_height), 12u);
            return;
        }
        draw_editor_view(f, seconds);
    }

    void draw_editor_view(grove::GpuCanvasFrame& f, double seconds) {
        orchard::editor::GpuViewOptions view_options;
        view_options.culling = pref("viewport.cull", "1") == "1";
        view_options.occlusion = view_options.culling;
        view_options.shadows = pref("viewport.shadows", "1") == "1";
        view_options.move_gizmo = m_tool == "move" && !m_session.playing();
        view_options.hot_axis = m_move_entity.valid() ? m_move_axis : -1;
        view_options.scene_lights = pref("viewport.lights", "1") == "1";
        view_options.materials = pref("viewport.materials", "1") == "1";
        view_options.particles = pref("viewport.particles", "1") == "1";
        view_options.particles_running = m_session.playing() || pref("preview_effects", "0") == "1" || std::chrono::steady_clock::now() < m_particle_burst_until;
        if (m_project) {
            view_options.particle_budget = m_project->info().physics.particle_budget;
            view_options.particle_quality = m_project->info().physics.particle_quality;
        }
        m_gpu.set_options(view_options);
        if (m_session.playing() != m_particles_were_playing) {
            m_gpu.particles().reset();
            m_particles_were_playing = m_session.playing();
        }
        for (const orchard::ParticleCommand& c : m_session.scripts().take_particle_commands()) m_gpu.particles().command(c.entity, c.action, c.count);
        for (const orchard::ParticleCommand& c : m_particle_commands) m_gpu.particles().command(c.entity, c.action, c.count);
        m_particle_commands.clear();
        if (view_options.scene_lights && m_project && m_view.dimension() == orchard::Dimension::three_d) {
            chlorophyll::World& lw = light_world();
            if (seconds > 0.0) lw.animate(m_scene, seconds);
            m_gpu.set_lights(&lw.lighting_scene());
        } else {
            m_gpu.set_lights(nullptr);
        }
        xylem::World* running = m_session.playing() ? m_session.world() : nullptr;
        if (running && running->fluid_count() > 0) running->fluid_draws(m_fluid_draws);
        else xylem::preview_fluids(m_scene, m_project ? m_project->info().physics.fluid_level : orchard::FluidLevel::waves, m_fluid_draws);
        m_gpu.set_fluids(&m_fluid_draws);
        m_gpu.set_texture_folder(m_project ? m_project->folder() : std::string());
        m_gpu.draw(f, m_scene, m_selected, m_view, m_session.playing(), seconds);
    }

    void moved_view() {
        m_view_moved = std::chrono::steady_clock::now();
    }

    void fly_step(double dt) {
        if (m_session.playing()) {
            m_fly_keys.clear();
            return;
        }
        auto held = [this](std::initializer_list<const char*> names) {
            for (const char* n : names) {
                if (m_fly_keys.count(n)) return true;
            }
            return false;
        };
        const float step = static_cast<float>(std::clamp(dt, 0.0, 0.1));
        const bool fast = held({"Shift", "LeftShift", "RightShift", "ShiftLeft", "ShiftRight"});
        float up = 0.0f, level_forward = 0.0f, level_right = 0.0f;
        if (held({"E", "PageUp"})) up += 1.0f;
        if (held({"Q", "PageDown"})) up -= 1.0f;
        if (held({"W", "UpArrow", "Up", "ArrowUp"})) level_forward += 1.0f;
        if (held({"S", "DownArrow", "Down", "ArrowDown"})) level_forward -= 1.0f;
        if (held({"D", "RightArrow", "Right", "ArrowRight"})) level_right += 1.0f;
        if (held({"A", "LeftArrow", "Left", "ArrowLeft"})) level_right -= 1.0f;
        if (up == 0.0f && level_forward == 0.0f && level_right == 0.0f) return;
        float speed = 0.0f;
        if (m_view.dimension() == orchard::Dimension::three_d) speed = std::max(1.5f, m_view.orbit_camera().distance * 0.6f);
        else speed = 360.0f / std::max(m_view.pan_camera().pixels_per_unit, 1e-3f);
        if (fast) speed *= 3.0f;
        const float d = speed * step;
        if (up != 0.0f) m_view.fly(0.0f, 0.0f, up * d);
        if (level_forward != 0.0f || level_right != 0.0f) m_view.fly_level(level_forward * d, level_right * d);
        moved_view();
    }

    orchard::Vec3 world_up() const {
        if (m_view.dimension() == orchard::Dimension::two_d) return {0.0f, 1.0f, 0.0f};
        return m_view.axis_system().up();
    }

    float gizmo_length(orchard::Vec3 at) const {
        return m_view.units_per_pixel(at) * 90.0f;
    }

    int gizmo_axis_at(float x, float y) const {
        if (!m_scene.alive(m_selected) || m_view.dimension() == orchard::Dimension::two_d) return -1;
        const orchard::Vec3 at = xylem::world_position(m_scene, m_selected);
        const float len = gizmo_length(at);
        float x0, y0;
        if (!m_view.project(at, x0, y0)) return -1;
        if (std::hypot(x - x0, y - y0) < 18.0f) return -1;
        int best = -1;
        float best_d = 9.0f;
        for (int i = 0; i < 3; ++i) {
            orchard::Vec3 dir{};
            (i == 0 ? dir.x : i == 1 ? dir.y : dir.z) = 1.0f;
            float x1, y1;
            if (!m_view.project(at + dir * len, x1, y1)) continue;
            const float vx = x1 - x0, vy = y1 - y0;
            const float l2 = vx * vx + vy * vy;
            if (l2 < 4.0f) continue;
            const float t = std::clamp(((x - x0) * vx + (y - y0) * vy) / l2, 0.15f, 1.1f);
            const float d = std::hypot(x - (x0 + vx * t), y - (y0 + vy * t));
            if (d < best_d) {
                best_d = d;
                best = i;
            }
        }
        return best;
    }

    void set_world_position(orchard::Entity e, orchard::Vec3 world) {
        orchard::Transform* t = m_scene.transform(e);
        if (!t) return;
        const orchard::Entity parent = m_scene.parent(e);
        if (parent.valid() && m_scene.alive(parent)) {
            orchard::Mat4 inv;
            if (orchard::invert(m_scene.world_matrix(parent), inv)) world = orchard::mul_point(inv, world);
        }
        if (m_view.dimension() == orchard::Dimension::two_d) world.z = t->position.z;
        t->position = world;
    }

    bool begin_move(float x, float y, bool vertical) {
        if (m_session.playing() || m_session.building()) return false;
        int axis = gizmo_axis_at(x, y);
        orchard::Entity hit = axis >= 0 ? m_selected : m_view.pick(m_scene, x, y);
        if (!m_scene.alive(hit)) return false;
        if (axis < 0 && vertical) axis = 3;
        m_selected = hit;
        m_move_entity = hit;
        m_move_axis = axis;
        m_move_start = xylem::world_position(m_scene, hit);
        m_move_moved = false;
        orchard::Vec3 origin, dir;
        m_move_grab = m_move_start;
        if (axis < 0 && m_view.ray(x, y, origin, dir)) {
            const orchard::Vec3 n = world_up();
            const float denom = orchard::dot(dir, n);
            if (std::fabs(denom) > 0.08f) {
                const float t = orchard::dot(m_move_start - origin, n) / denom;
                if (t > 0.0f) m_move_grab = origin + dir * t;
            }
        }
        m_move_press_x = x;
        m_move_press_y = y;
        redraw();
        return true;
    }

    void continue_move(float x, float y) {
        if (!m_scene.alive(m_move_entity)) return;
        orchard::Vec3 target = m_move_start;
        if (m_move_axis >= 0) {
            orchard::Vec3 dir{};
            if (m_move_axis == 3) dir = world_up();
            else (m_move_axis == 0 ? dir.x : m_move_axis == 1 ? dir.y : dir.z) = 1.0f;
            const float len = gizmo_length(m_move_start);
            float x0, y0, x1, y1;
            if (!m_view.project(m_move_start, x0, y0) || !m_view.project(m_move_start + dir * len, x1, y1)) return;
            const float vx = x1 - x0, vy = y1 - y0;
            const float l2 = vx * vx + vy * vy;
            if (l2 < 1.0f) return;
            const float amount = ((x - m_move_press_x) * vx + (y - m_move_press_y) * vy) / l2 * len;
            target = m_move_start + dir * amount;
        } else {
            orchard::Vec3 origin, dir;
            if (!m_view.ray(x, y, origin, dir)) return;
            if (m_view.dimension() == orchard::Dimension::two_d) {
                orchard::Vec3 o0, d0;
                if (!m_view.ray(m_move_press_x, m_move_press_y, o0, d0)) return;
                target = m_move_start + orchard::Vec3{origin.x - o0.x, origin.y - o0.y, 0.0f};
            } else {
                const orchard::Vec3 n = world_up();
                const float denom = orchard::dot(dir, n);
                if (std::fabs(denom) < 0.02f) return;
                const float t = orchard::dot(m_move_grab - origin, n) / denom;
                if (t <= 0.0f || t > 20000.0f) return;
                target = m_move_start + (origin + dir * t - m_move_grab);
            }
        }
        set_world_position(m_move_entity, target);
        m_move_moved = true;
        m_light_dirty = true;
        m_dirty = true;
        redraw();
    }


    void touched() {
        m_dirty = true;
        m_light_dirty = true;
        redraw();
    }

    void redraw() {
        if (grove::Node* canvas = m_doc.find_by_id("viewport")) grove::redraw_canvas(m_doc, *canvas);
    }

    bool wants_animation() const {
        return m_session.playing() || m_session.building() || !m_fly_keys.empty() || pref("preview_effects", "0") == "1" || pref("viewport.trace", "off") != "off" || std::chrono::steady_clock::now() < m_particle_burst_until;
    }

    void set_animating(bool on) {
        if (grove::Node* canvas = m_doc.find_by_id("viewport")) grove::set_canvas_animating(m_doc, *canvas, on);
    }

    bool to_canvas(const std::string& xs, const std::string& ys, float& x, float& y) const {
        const grove::Node* canvas = m_doc.find_by_id("viewport");
        if (!canvas || m_view.width() == 0 || m_view.height() == 0) return false;
        const grove::CanvasRect r = grove::canvas_rect(m_doc, *canvas);
        if (r.width <= 0.0 || r.height <= 0.0) return false;
        x = static_cast<float>((std::atof(xs.c_str()) - r.x) * m_view.width() / r.width);
        y = static_cast<float>((std::atof(ys.c_str()) - r.y) * m_view.height() / r.height);
        return true;
    }

    orchard::Entity entity(const std::string& id) const {
        if (id.empty()) return orchard::Entity{};
        const orchard::Entity e = orchard::Entity::from_id(std::strtoull(id.c_str(), nullptr, 10));
        return m_scene.alive(e) ? e : orchard::Entity{};
    }

    void walk(orchard::Entity e, int depth, std::string& out) const {
        const orchard::Entity p = m_scene.parent(e);
        std::string tags;
        if (m_scene.body(e)) tags += "body";
        if (m_scene.colliders(e) && !m_scene.colliders(e)->empty()) tags += tags.empty() ? "collider" : " collider";
        if (!m_scene.script(e).empty()) tags += tags.empty() ? "script" : " script";
        out += std::to_string(e.id()) + "\t" + (p.valid() ? std::to_string(p.id()) : std::string()) + "\t" + std::to_string(depth) + "\t" + clean(m_scene.name(e)) + "\t" + tags + "\n";
        for (orchard::Entity c : m_scene.children(e)) walk(c, depth + 1, out);
    }

    std::string category(orchard::Entity e) const {
        if (m_scene.light(e)) return "lights";
        if (m_scene.fluid(e)) return "fluids";
        if (m_scene.emitter(e) && (!m_scene.colliders(e) || m_scene.colliders(e)->empty()) && !m_scene.body(e)) return "particles";
        if (m_scene.name(e).find("Camera") != std::string::npos) return "cameras";
        const std::vector<orchard::Collider>* cs = m_scene.colliders(e);
        const bool shapes = cs && !cs->empty();
        bool all_sensors = shapes;
        if (shapes) {
            for (const orchard::Collider& c : *cs) all_sensors = all_sensors && c.sensor;
        }
        const orchard::Body* b = m_scene.body(e);
        if (shapes && all_sensors) return "triggers";
        if (b) return b->type == orchard::BodyType::fixed ? "containers" : "bodies";
        if (shapes) {
            const orchard::Entity owner = m_scene.body_owner(e);
            const orchard::Body* ob = owner.valid() ? m_scene.body(owner) : nullptr;
            if (ob && ob->type != orchard::BodyType::fixed) return "bodies";
            return "containers";
        }
        if (!m_scene.script(e).empty()) return "scripted";
        if (!m_scene.children(e).empty()) return "groups";
        return "other";
    }

    std::string tags_of(orchard::Entity e) const {
        std::string tags;
        auto add = [&tags](const char* t) { tags += tags.empty() ? t : std::string(" ") + t; };
        if (const orchard::Body* b = m_scene.body(e)) add(orchard::to_string(b->type));
        if (m_scene.colliders(e) && !m_scene.colliders(e)->empty()) add("collider");
        if (const orchard::Light* l = m_scene.light(e)) {
            add(orchard::to_string(l->type));
            if (l->animation.active()) add(l->animation.mode == orchard::LightAnimationMode::none ? "color-fx" : orchard::to_string(l->animation.mode));
        }
        if (const orchard::Material* m = m_scene.material(e)) {
            if (m->emission.x + m->emission.y + m->emission.z > 0.0f && m->emission_strength > 0.0f) add("glows");
        }
        if (!m_scene.script(e).empty()) add("script");
        if (m_scene.emitter(e)) add("particles");
        if (!m_scene.instance_of(e).empty()) add("entity-copy");
        return tags;
    }

    std::string pref(const std::string& key, const std::string& fallback) const {
        const auto it = m_prefs.find(key);
        return it == m_prefs.end() ? fallback : it->second;
    }

    void load_prefs() {
        m_prefs.clear();
        if (!m_project) return;
        std::ifstream in(fs::path(m_project->folder()) / ".orchard" / "editor.prefs");
        for (std::string line; std::getline(in, line);) {
            const std::size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            m_prefs[line.substr(0, eq)] = line.substr(eq + 1);
        }
    }

    void save_prefs() const {
        if (!m_project) return;
        std::error_code ec;
        const fs::path dir = fs::path(m_project->folder()) / ".orchard";
        fs::create_directories(dir, ec);
        std::ofstream out(dir / "editor.prefs");
        for (const auto& [k, v] : m_prefs) out << k << "=" << v << "\n";
    }

    std::string outline(const std::string& group, const std::string& filter) const {
        const std::string want = lower(filter);
        const std::string collapsed = "," + pref("outliner.collapsed", "") + ",";
        auto matches = [&](orchard::Entity e) {
            if (want.empty()) return true;
            return lower(m_scene.name(e)).find(want) != std::string::npos || lower(tags_of(e)).find(want) != std::string::npos || lower(category(e)).find(want) != std::string::npos;
        };
        auto item = [&](orchard::Entity e, int depth) {
            const std::string cat = category(e);
            return "item\t" + std::to_string(e.id()) + "\t" + std::to_string(depth) + "\t" + clean(m_scene.name(e)) + "\t" + section_icon(cat) + "\t" + tags_of(e) + "\t" + cat + "\n";
        };
        std::string out;
        if (group == "hierarchy") {
            std::function<void(orchard::Entity, int)> visit = [&](orchard::Entity e, int depth) {
                if (matches(e)) out += item(e, depth);
                for (orchard::Entity c : m_scene.children(e)) visit(c, depth + 1);
            };
            for (orchard::Entity e : m_scene.roots()) visit(e, 0);
            return out;
        }
        std::vector<orchard::Entity> all = m_scene.entities();
        if (group == "flat" || pref("outliner.sort", "scene") == "name") {
            std::stable_sort(all.begin(), all.end(), [this](orchard::Entity a, orchard::Entity b) { return lower(m_scene.name(a)) < lower(m_scene.name(b)); });
        }
        if (group == "flat") {
            for (orchard::Entity e : all) {
                if (matches(e)) out += item(e, 0);
            }
            return out;
        }
        const bool show_empty = pref("outliner.empty_sections", "0") == "1";
        for (const Section& sec : outline_sections()) {
            std::string rows;
            int count = 0;
            for (orchard::Entity e : all) {
                if (category(e) != sec.key || !matches(e)) continue;
                ++count;
                rows += item(e, 0);
            }
            if (count == 0 && !show_empty) continue;
            const bool closed = collapsed.find(std::string(",") + sec.key + ",") != std::string::npos;
            out += std::string("section\t") + sec.key + "\t" + upper(sec.label) + "\t" + std::to_string(count) + "\t" + (closed ? "1" : "0") + "\t" + sec.icon + "\n";
            if (!closed) out += rows;
        }
        return out;
    }

    chlorophyll::World& light_world() {
        if (m_light_dirty || !m_light_built) {
            if (m_project) {
                m_gpu.set_texture_folder(m_project->folder());
                m_light_world.set_texture_loader(m_gpu.lighting_textures());
                if (!m_light_built) {
                    m_light_world.build(m_scene, m_project->info(), true);
                    m_light_built = true;
                } else {
                    m_light_world.sync(m_scene);
                }
            }
            m_light_dirty = false;
        }
        m_light_world.set_time(m_session.playing() ? m_session.time() : 0.0);
        return m_light_world;
    }

    float top_extent(orchard::Entity e) const {
        float top = 0.5f;
        if (const std::vector<orchard::Collider>* cs = m_scene.colliders(e)) {
            if (!cs->empty()) top = 0.0f;
            for (const orchard::Collider& c : *cs) top = std::max(top, c.offset.y + c.size.y * 0.5f);
        }
        const orchard::Transform* t = m_scene.transform(e);
        return top * (t ? std::fabs(t->scale.y) : 1.0f);
    }

    std::string details(orchard::Entity e) {
        if (!m_scene.alive(e)) return std::string();
        std::string out;
        bool titled = false;
        auto head = [&out, &titled](const std::string& t) {
            out += "head\t" + (titled ? upper(t) : t) + "\n";
            titled = true;
        };
        auto kv = [&out](const std::string& k, const std::string& v) { out += "kv\t" + clean(k) + "\t" + clean(v) + "\n"; };
        auto yes = [&out](const std::string& k, bool v) { out += "yes\t" + clean(k) + "\t" + (v ? "1" : "0") + "\n"; };
        auto swatch = [&out](const std::string& k, orchard::Vec3 c, const std::string& text) { out += "swatch\t" + clean(k) + "\t" + hex_color(c) + "\t" + clean(text) + "\n"; };
        auto deg = [](float v) { return number(v) + "°"; };
        const orchard::Transform* t = m_scene.transform(e);
        const std::string cat = category(e);
        std::string cat_label = "Empty";
        for (const Section& sec : outline_sections()) {
            if (cat == sec.key) cat_label = sec.one;
        }
        head(clean(m_scene.name(e)));
        kv("Kind", cat_label);
        kv("ID", std::to_string(e.id()));
        const orchard::Entity parent = m_scene.parent(e);
        if (parent.valid()) kv("Parent", m_scene.name(parent));
        const std::size_t kids = m_scene.children(e).size();
        if (kids > 0) kv("Children", std::to_string(kids));
        if (t) {
            head("Transform");
            kv("Position", number(t->position.x) + ", " + number(t->position.y) + ", " + number(t->position.z));
            const orchard::Vec3 r = orchard::to_euler(t->rotation);
            kv("Angle X (pitch)", deg(r.x));
            kv("Angle Y (yaw)", deg(r.y));
            kv("Angle Z (roll)", deg(r.z));
            kv("Scale", number(t->scale.x) + ", " + number(t->scale.y) + ", " + number(t->scale.z));
            if (parent.valid()) {
                const orchard::Vec3 wp = xylem::world_position(m_scene, e);
                kv("World position", number(wp.x) + ", " + number(wp.y) + ", " + number(wp.z));
                const orchard::Vec3 wr = orchard::to_euler(xylem::world_rotation(m_scene, e));
                kv("World angles", deg(wr.x) + ", " + deg(wr.y) + ", " + deg(wr.z));
            }
        }
        if (const orchard::Body* b = m_scene.body(e)) {
            head("Physics");
            kv("Body", orchard::to_string(b->type));
            yes("Enabled", b->enabled);
            float mass = 0.0f;
            if (const std::vector<orchard::Collider>* cs = m_scene.colliders(e)) {
                const orchard::Vec3 sc = t ? t->scale : orchard::Vec3{1.0f, 1.0f, 1.0f};
                for (const orchard::Collider& c : *cs) {
                    if (c.sensor || !c.enabled) continue;
                    const float x = c.size.x * std::fabs(sc.x), y = c.size.y * std::fabs(sc.y), z = c.size.z * std::fabs(sc.z);
                    float volume = x * y * z;
                    if (c.shape == orchard::ColliderShape::sphere) {
                        const float d = std::max({x, y, z});
                        volume = 3.14159265f * d * d * d / 6.0f;
                    } else if (c.shape == orchard::ColliderShape::capsule) {
                        const float rr = std::max(x, z) * 0.5f;
                        const float h = std::max(0.0f, y - 2.0f * rr);
                        volume = 3.14159265f * rr * rr * h + 4.0f / 3.0f * 3.14159265f * rr * rr * rr;
                    } else if (c.shape == orchard::ColliderShape::cylinder || c.shape == orchard::ColliderShape::cone) {
                        const float rr = std::max(x, z) * 0.5f;
                        volume = 3.14159265f * rr * rr * y * (c.shape == orchard::ColliderShape::cone ? 1.0f / 3.0f : 1.0f);
                    } else if (c.shape == orchard::ColliderShape::rounded_box) {
                        const float rr = std::clamp(c.rounding, 0.0f, 0.5f * std::min({x, y, z}));
                        const float cx = x - 2.0f * rr, cy = y - 2.0f * rr, cz = z - 2.0f * rr;
                        volume = cx * cy * cz + 2.0f * rr * (cx * cy + cy * cz + cz * cx) + 3.14159265f * rr * rr * (cx + cy + cz) + 4.0f / 3.0f * 3.14159265f * rr * rr * rr;
                    }
                    mass += volume * c.density;
                }
            }
            if (b->type == orchard::BodyType::dynamic) kv("Mass", number(mass) + " kg");
            kv("Gravity scale", number(b->gravity_scale));
            kv("Drag / spin drag", number(b->linear_damping) + " / " + number(b->angular_damping));
            if (xylem::World* w = m_session.world()) {
                if (w->has_body(e)) {
                    const orchard::Vec3 v = w->velocity(e);
                    kv("Velocity", number(v.x) + ", " + number(v.y) + ", " + number(v.z));
                    kv("Speed", number(orchard::length(v)) + " m/s");
                    const orchard::Vec3 sp = w->spin(e);
                    kv("Spin", deg(orchard::degrees(sp.x)) + "/s, " + deg(orchard::degrees(sp.y)) + "/s, " + deg(orchard::degrees(sp.z)) + "/s");
                    yes("Awake", w->awake(e));
                }
            }
        }
        if (const std::vector<orchard::Collider>* cs = m_scene.colliders(e)) {
            if (!cs->empty()) {
                std::string list;
                for (const orchard::Collider& c : *cs) list += (list.empty() ? "" : ", ") + std::string(orchard::to_string(c.shape)) + (c.sensor ? " (sensor)" : "");
                kv("Colliders", list);
            }
        }
        bool emits = false;
        if (const orchard::Light* l = m_scene.light(e)) {
            emits = l->enabled;
            head("Light");
            kv("Type", orchard::light_type_label(l->type));
            const orchard::LightAnimationSample now = l->sample(m_session.playing() ? m_session.time() : 0.0);
            swatch("Color", l->color, hex_color(l->color));
            if (l->temperature > 0.0f) kv("Temperature", number(l->temperature) + " K");
            kv("Intensity", number(l->intensity) + " " + orchard::to_string(l->unit));
            if (l->range > 0.0f) kv("Range", number(l->range) + " m");
            if (l->type == orchard::LightType::spot) kv("Cone", deg(l->inner_cone) + " inner, " + deg(l->outer_cone) + " outer");
            if (l->type == orchard::LightType::directional) kv("Sun size", deg(l->angular_diameter));
            if (l->type == orchard::LightType::sphere || l->type == orchard::LightType::disk) kv("Radius", number(l->radius) + " m");
            if (l->type == orchard::LightType::rectangle) kv("Size", number(l->width) + " x " + number(l->height) + " m");
            if (l->type == orchard::LightType::tube) kv("Tube", number(l->length) + " m long, " + number(l->radius) + " m thick");
            kv("Mobility", orchard::to_string(l->mobility));
            yes("Casts shadows", l->shadows);
            if (l->animation.active()) {
                const orchard::LightAnimation& a = l->animation;
                kv("Effect", orchard::light_animation_label(a.mode));
                kv("Speed", number(a.rate) + (a.rate_end != a.rate ? " to " + number(a.rate_end) : std::string()) + " per second");
                if (a.mode == orchard::LightAnimationMode::strobe) kv("Flash length", number(a.on_fraction * 100.0f) + "% of each flash");
                kv("Brightness", number(a.low) + " to " + number(a.high) + (a.high_end != a.high ? " (ends at " + number(a.high_end) + ")" : std::string()));
                kv("Runs for", a.duration > 0.0f ? number(a.duration) + " s" + (a.loop ? ", then repeats" : ", then stays steady") : std::string("ever"));
                if (a.color_mode != orchard::LightColorMode::none) swatch("Second color", a.color_b, orchard::light_color_mode_label(a.color_mode));
                if (m_session.playing()) {
                    swatch("Right now", now.color * std::min(1.0f, now.brightness), number(now.brightness * 100.0f) + "% bright");
                }
            }
        }
        if (const orchard::Material* m = m_scene.material(e)) {
            head("Material");
            if (!m->preset.empty()) kv("Preset", m->preset);
            swatch("Color", m->color, hex_color(m->color));
            kv("Metal / rough", number(m->metallic) + " / " + number(m->roughness));
            if (m->transmission > 0.0f) kv("See-through", number(m->transmission * 100.0f) + "%, IOR " + number(m->ior));
            const bool glows = m->emission.x + m->emission.y + m->emission.z > 0.0f && m->emission_strength > 0.0f;
            if (glows) swatch("Glow", m->emission, "strength " + number(m->emission_strength));
            emits = emits || glows;
            yes("Casts shadows", m->casts_shadows);
        }
        head("Lighting");
        yes("Emits light", emits);
        if (m_project && m_scene.dimension() == orchard::Dimension::three_d && !m_scene.light(e) && t) {
            chlorophyll::World& lw = light_world();
            const orchard::AxisSystem axes = m_project->info().axis_system();
            const orchard::Vec3 wp = xylem::world_position(m_scene, e);
            const orchard::Vec3 probe = wp + axes.up() * (top_extent(e) + 0.02f);
            int lights = 0, reaching = 0;
            std::string by;
            for (orchard::Entity le : m_scene.entities()) {
                const orchard::Light* l = m_scene.light(le);
                if (!l || !l->enabled) continue;
                ++lights;
                if (lw.lit_by(probe, le)) {
                    ++reaching;
                    if (by.size() < 80) by += (by.empty() ? "" : ", ") + m_scene.name(le);
                }
            }
            if (lights == 0) kv("In shadow", "no lights in the scene");
            else if (reaching == 0) kv("In shadow", "yes, no light reaches its top");
            else if (reaching < lights) kv("In shadow", "partly (" + std::to_string(reaching) + " of " + std::to_string(lights) + " lights reach it)");
            else kv("In shadow", "no");
            if (!by.empty()) kv("Lit by", by);
            kv("Light level", number(lw.light_level(probe, axes.up())));
        }
        return out;
    }

    std::string field(const std::string& key, const std::string& label, const std::string& kind, const std::string& value, const std::string& extra = std::string(), const std::string& help = std::string()) const {
        return "field\t" + key + "\t" + clean(label) + "\t" + kind + "\t" + clean(value) + "\t" + clean(extra) + "\t" + clean(help) + "\n";
    }

    std::string inspect(orchard::Entity e) const {
        if (!m_scene.alive(e)) return std::string();
        const bool flat = m_scene.dimension() == orchard::Dimension::two_d;
        std::string out = "title\t" + clean(m_scene.name(e)) + "\n";
        auto group = [&out](const std::string& g) { out += "group\t" + upper(g) + "\n"; };
        auto yesno = [](bool b) { return std::string(b ? "1" : "0"); };
        group("General");
        out += field("name", "Name", "string", m_scene.name(e));
        if (!m_scene.instance_of(e).empty()) {
            out += field("template_info", "Made from " + clean(m_scene.instance_of(e)), "button", "edit");
            out += field("template_unlink", "Unlink from its template", "button", "unlink");
        }
        if (const orchard::Transform* t = m_scene.transform(e)) {
            out += field("position", "Position", "vec3", vec_text(t->position), "0.1");
            out += field("rotation", "Angles (degrees)", "vec3", vec_text(orchard::to_euler(t->rotation)), "1");
            out += field("scale", "Scale", "vec3", vec_text(t->scale), "0.1");
        }
        std::vector<std::pair<std::string, std::string>> scripts = {{"", "No script"}};
        if (m_project) {
            for (const std::string& sc : m_project->scripts()) scripts.emplace_back(sc, sc);
            for (const std::string& sc : m_project->graphs()) scripts.emplace_back(sc, sc);
        }
        out += field("script", "Script", "select", m_scene.script(e), options_of(scripts));

        group("Physics");
        const orchard::Body* b = m_scene.body(e);
        out += field("has_body", "Rigid body", "bool", yesno(b != nullptr), "", "Off: it doesn't move on its own. Colliders without a body act as walls and floors.");
        if (b) {
            out += field("body_type", "Type", "select", orchard::to_string(b->type), options_of({{"dynamic", "Dynamic (falls, gets pushed)"}, {"static", "Container (wall, floor, never moves)"}, {"kinematic", "Kinematic (moved by scripts)"}}));
            out += field("body_enabled", "Enabled", "bool", yesno(b->enabled));
            out += field("gravity_scale", "Gravity scale", "float", number(b->gravity_scale), "0.1");
            out += field("linear_damping", "Drag", "float", number(b->linear_damping), "0.05");
            out += field("angular_damping", "Spin drag", "float", number(b->angular_damping), "0.05");
            group("Flags");
            for (const orchard::BodyFlagInfo& info : orchard::body_flags()) {
                if (flat ? !info.two_d : !info.three_d) continue;
                out += field(std::string("flag:") + info.name, info.label, "bool", yesno(b->has(info.flag)));
            }
        }
        if (const std::vector<orchard::Collider>* cs = m_scene.colliders(e)) {
            for (std::size_t i = 0; i < cs->size(); ++i) {
                const orchard::Collider& c = (*cs)[i];
                const std::string base = "collider:" + std::to_string(i) + ":";
                group("Collider " + std::to_string(i + 1));
                out += field(base + "shape", "Shape", "select", orchard::to_string(c.shape), options_of({{"box", "Box"}, {"sphere", "Sphere"}, {"capsule", "Capsule"}, {"cylinder", "Cylinder"}, {"cone", "Cone"}, {"rounded-box", "Rounded box"}, {"sphere-section", "Sphere section (hemisphere, dome, bowl, wedge)"}}));
                out += field(base + "size", "Size", "vec3", vec_text(c.size), "0.1");
                out += field(base + "offset", "Offset", "vec3", vec_text(c.offset), "0.1");
                out += field(base + "density", "Density", "float", number(c.density), "0.1");
                out += field(base + "friction", "Friction", "float", number(c.friction), "0.05");
                out += field(base + "bounce", "Bounce", "float", number(c.restitution), "0.05");
                out += field(base + "enabled", "Enabled", "bool", yesno(c.enabled));
                out += field(base + "sensor", "Sensor (trigger, nothing bumps it)", "bool", yesno(c.sensor));
                out += field("remove_collider", "Remove this collider", "button", std::to_string(i));
            }
        }
        out += field("add_collider", "Add a collider", "button", "box");

        group("Light");
        const orchard::Light* l = m_scene.light(e);
        out += field("has_light", "Gives off light", "bool", yesno(l != nullptr));
        if (l) {
            std::vector<std::pair<std::string, std::string>> types;
            for (orchard::LightType lt : orchard::light_types()) types.emplace_back(orchard::to_string(lt), orchard::light_type_label(lt));
            out += field("light.type", "Shape", "select", orchard::to_string(l->type), options_of(types));
            out += field("light.enabled", "On", "bool", yesno(l->enabled));
            out += field("light.color", "Color", "color", hex_color(l->color));
            out += field("light.temperature", "Temperature (K, 0 = use the color)", "float", number(l->temperature), "100");
            out += field("light.intensity", "Brightness", "float", number(l->intensity), "1");
            out += field("light.unit", "Unit", "select", orchard::to_string(l->unit), options_of({{"unitless", "Unitless"}, {"candela", "Candela"}, {"lumens", "Lumens"}, {"lux", "Lux"}, {"nits", "Nits"}, {"watts", "Watts"}}));
            if (l->type != orchard::LightType::directional) {
                out += field("light.range", "Range (m, 0 = no limit)", "float", number(l->range), "0.5");
                out += field("light.falloff", "Falloff", "select", orchard::to_string(l->falloff), options_of({{"physical", "Physical (inverse square)"}, {"linear", "Linear"}, {"smooth", "Smooth"}, {"constant", "Constant"}, {"custom", "Custom exponent"}}));
                out += field("light.spread", "How light spreads", "select", orchard::to_string(l->spread), options_of({{"natural", "Natural (spills and fades like real light)"}, {"blob", "Blob (a tight pool that stops at the range)"}}));
                out += field("light.spill", "Spill (light wrapping around and bouncing off nearby things)", "slider", number(l->spill), "0.05");
            }
            if (l->type == orchard::LightType::spot) {
                out += field("light.inner_cone", "Inner cone (degrees)", "float", number(l->inner_cone), "1");
                out += field("light.outer_cone", "Outer cone (degrees)", "float", number(l->outer_cone), "1");
            }
            if (l->type == orchard::LightType::directional) out += field("light.angular_diameter", "Sun size (degrees)", "float", number(l->angular_diameter), "0.1");
            if (l->type == orchard::LightType::sphere || l->type == orchard::LightType::disk || l->type == orchard::LightType::tube || l->type == orchard::LightType::point || l->type == orchard::LightType::spot) out += field("light.radius", "Radius (m)", "float", number(l->radius), "0.05");
            if (l->type == orchard::LightType::tube) out += field("light.length", "Length (m)", "float", number(l->length), "0.1");
            if (l->type == orchard::LightType::rectangle) {
                out += field("light.width", "Width (m)", "float", number(l->width), "0.1");
                out += field("light.height", "Height (m)", "float", number(l->height), "0.1");
            }
            if (l->type == orchard::LightType::disk || l->type == orchard::LightType::rectangle) out += field("light.two_sided", "Shines both ways", "bool", yesno(l->two_sided));
            out += field("light.mobility", "Mobility", "select", orchard::to_string(l->mobility), options_of({{"dynamic", "Dynamic (moves, changes)"}, {"stationary", "Stationary"}, {"baked", "Baked (static)"}}));
            out += field("light.shadows", "Casts shadows", "bool", yesno(l->shadows));
            out += field("light.shadow_strength", "Shadow strength", "float", number(l->shadow_strength), "0.05");
            out += field("light.visible", "Visible to the camera", "bool", yesno(l->visible));
            out += field("light.diffuse", "Diffuse", "float", number(l->diffuse), "0.1");
            out += field("light.specular", "Highlights", "float", number(l->specular), "0.1");
            out += field("light.volumetric", "Fog glow", "float", number(l->volumetric), "0.1");

            const orchard::LightAnimation& a = l->animation;
            group("Strobe & effects");
            out += field("anim.preset", "Quick pick", "select", "", options_of({{"", "Pick a preset..."}, {"strobe", "Club strobe"}, {"police", "Police lights"}, {"pulse", "Slow pulse"}, {"candle", "Candle"}, {"broken", "Broken bulb"}, {"rainbow", "Rainbow"}, {"speedup", "Speeding up"}, {"off", "Turn effects off"}}));
            std::vector<std::pair<std::string, std::string>> modes;
            for (orchard::LightAnimationMode m : orchard::light_animation_modes()) modes.emplace_back(orchard::to_string(m), orchard::light_animation_label(m));
            out += field("anim.mode", "Effect", "select", orchard::to_string(a.mode), options_of(modes));
            out += field("anim.rate", "Speed (flashes per second)", "float", number(a.rate), "0.5");
            out += field("anim.rate_end", "Speed at the end", "float", number(a.rate_end), "0.5", "The speed ramps to this over the duration. Same as the speed for a steady rate.");
            out += field("anim.on_fraction", "Flash length (0 to 1 of each flash)", "slider", number(a.on_fraction), "0.05");
            out += field("anim.softness", "Softness", "slider", number(a.softness), "0.05");
            out += field("anim.duration", "Duration (s, 0 = forever)", "float", number(a.duration), "0.5");
            out += field("anim.delay", "Start after (s)", "float", number(a.delay), "0.1");
            out += field("anim.loop", "Repeat after the duration", "bool", yesno(a.loop));
            out += field("anim.low", "Brightness when off", "float", number(a.low), "0.05");
            out += field("anim.high", "Brightness when on", "float", number(a.high), "0.05");
            out += field("anim.high_end", "Brightness at the end", "float", number(a.high_end), "0.05");
            std::vector<std::pair<std::string, std::string>> cmodes;
            for (orchard::LightColorMode m : orchard::light_color_modes()) cmodes.emplace_back(orchard::to_string(m), orchard::light_color_mode_label(m));
            out += field("anim.color_mode", "Color change", "select", orchard::to_string(a.color_mode), options_of(cmodes));
            out += field("anim.color_b", "Second color", "color", hex_color(a.color_b));
            out += field("anim.phase", "Offset (0 to 1, to chase other lights)", "slider", number(a.phase), "0.05");
        }

        group("Particles");
        const orchard::Emitter* pe = m_scene.emitter(e);
        out += field("has_emitter", "Particle emitter (sparks, smoke, fire...)", "bool", yesno(pe != nullptr));
        if (pe) {
            std::vector<std::pair<std::string, std::string>> ppresets = {{"", "Custom"}};
            for (const std::string& p : orchard::Emitter::presets()) ppresets.emplace_back(p, p);
            out += field("emitter.preset", "Preset", "select", pe->preset, options_of(ppresets));
            out += field("emitter.enabled", "On", "bool", yesno(pe->enabled));
            out += field("emitter.autoplay", "Starts by itself", "bool", yesno(pe->autoplay));
            out += field("emitter.loop", "Keeps going (loop)", "bool", yesno(pe->loop));
            out += field("emitter.duration", "Runs for (s, 0 = forever)", "float", number(pe->duration), "0.1");
            out += field("emitter.rate", "Particles per second", "float", number(pe->rate), "5");
            out += field("emitter.burst", "Burst of particles at once", "float", std::to_string(pe->burst), "1");
            out += field("emitter.burst_every", "Burst again every (s, 0 = once)", "float", number(pe->burst_every), "0.1");
            out += field("emitter.lifetime", "Each lives for (s)", "float", number(pe->lifetime), "0.1");
            out += field("emitter.lifetime_spread", "Life varies by (s)", "float", number(pe->lifetime_spread), "0.1");
            out += field("emitter.speed", "Speed (m/s)", "float", number(pe->speed), "0.25");
            out += field("emitter.speed_spread", "Speed varies by", "float", number(pe->speed_spread), "0.25");
            out += field("emitter.direction", "Direction", "vec3", vec_text(pe->direction), "0.1");
            out += field("emitter.spread", "Spread (degrees, 180 = all around)", "float", number(pe->spread), "1");
            std::vector<std::pair<std::string, std::string>> shapes;
            for (orchard::EmitterShape sh : orchard::emitter_shapes()) shapes.emplace_back(orchard::to_string(sh), orchard::to_string(sh));
            out += field("emitter.shape", "Comes from", "select", orchard::to_string(pe->shape), options_of(shapes));
            if (pe->shape != orchard::EmitterShape::point) out += field("emitter.shape_size", "Shape size (m)", "vec3", vec_text(pe->shape_size), "0.1");
            out += field("emitter.gravity", "Gravity (1 = normal, below 0 rises)", "float", number(pe->gravity), "0.05");
            out += field("emitter.drag", "Air drag", "float", number(pe->drag), "0.05");
            out += field("emitter.wind", "Wind (m/s)", "vec3", vec_text(pe->wind), "0.1");
            out += field("emitter.turbulence", "Swirl", "float", number(pe->turbulence), "0.1");
            out += field("emitter.size", "Size at start (m)", "float", number(pe->size), "0.01");
            out += field("emitter.size_end", "Size at the end (m)", "float", number(pe->size_end), "0.01");
            out += field("emitter.color", "Color at start", "color", hex_color(pe->color));
            out += field("emitter.color_end", "Color at the end", "color", hex_color(pe->color_end));
            out += field("emitter.alpha", "See-through at start (1 = solid)", "slider", number(pe->alpha), "0.05");
            out += field("emitter.alpha_end", "See-through at the end", "slider", number(pe->alpha_end), "0.05");
            out += field("emitter.glow", "Glow", "float", number(pe->glow), "0.25");
            out += field("emitter.stretch", "Stretch along movement", "float", number(pe->stretch), "0.1");
            out += field("emitter.blend", "Blend", "select", orchard::to_string(pe->blend), options_of({{"additive", "Adds light (sparks, fire)"}, {"alpha", "Covers (smoke, dust)"}}));
            out += field("emitter.lit", "Lit by the scene's lights", "bool", yesno(pe->lit));
            out += field("emitter.collide", "Bounces off things", "bool", yesno(pe->collide));
            if (pe->collide) {
                out += field("emitter.bounce", "Bounce", "slider", number(pe->bounce), "0.05");
                out += field("emitter.friction", "Friction", "float", number(pe->friction), "0.05");
                out += field("emitter.die_on_hit", "Disappear when they hit", "bool", yesno(pe->die_on_hit));
            }
            std::vector<std::pair<std::string, std::string>> images = {{"", "Soft dot"}};
            if (m_project)
                for (const std::string& img : m_project->images()) images.emplace_back(img, img);
            out += field("emitter.texture", "Texture", "select", pe->texture, options_of(images));
            if (!pe->texture.empty()) out += field("emitter.frames", "Frames in the texture (flipbook)", "float", std::to_string(pe->frames), "1");
            std::vector<std::pair<std::string, std::string>> deaths = {{"", "Nothing"}};
            for (const std::string& p : orchard::Emitter::presets()) deaths.emplace_back(p, p);
            out += field("emitter.on_death", "When one dies, it makes", "select", pe->on_death, options_of(deaths));
            out += field("emitter.max_particles", "Most at once", "float", std::to_string(pe->max_particles), "100");
            out += field("emitter.priority", "Priority when busy (0 low to 3 high)", "float", std::to_string(pe->priority), "1");
            out += field("emitter.local_space", "Move with the entity", "bool", yesno(pe->local_space));
            out += field("emitter.inherit_velocity", "Keep the entity's speed", "float", number(pe->inherit_velocity), "0.1");
            out += field("emitter_burst", "Burst now (while playing)", "button", "1");
        }

        group("Fluid");
        const orchard::Fluid* w = m_scene.fluid(e);
        out += field("has_fluid", "Fluid volume (water, oil, lava...)", "bool", yesno(w != nullptr));
        if (w) {
            std::vector<std::pair<std::string, std::string>> levels;
            for (orchard::FluidLevel lv : orchard::fluid_levels()) levels.emplace_back(orchard::to_string(lv), orchard::fluid_level_label(lv));
            out += field("fluid.level", "How it behaves", "select", orchard::to_string(w->level), options_of(levels));
            std::vector<std::pair<std::string, std::string>> fpresets = {{"", "Custom"}};
            for (const std::string& p : orchard::Fluid::presets()) fpresets.emplace_back(p, p);
            out += field("fluid.preset", "Liquid", "select", w->preset, options_of(fpresets));
            out += field("fluid.size", "Volume size (m)", "vec3", vec_text(w->size), "0.5");
            out += field("fluid.fill", "How full (0 to 1)", "slider", number(w->fill), "0.05");
            out += field("fluid.density", "Density (kg/m3)", "float", number(w->density), "10");
            out += field("fluid.viscosity", "Viscosity (Pa s; water 0.001, honey 10)", "float", number(w->viscosity), "0.001");
            out += field("fluid.surface_tension", "Surface tension (N/m)", "float", number(w->surface_tension), "0.005");
            out += field("fluid.drag", "Drag on things in it", "float", number(w->drag), "0.1");
            out += field("fluid.quadratic_drag", "Drag at speed (grows with speed squared)", "float", number(w->quadratic_drag), "0.1");
            out += field("fluid.angular_drag", "Spin drag", "float", number(w->angular_drag), "0.1");
            out += field("fluid.friction", "Friction against things", "float", number(w->friction), "0.05");
            out += field("fluid.bounce", "Bounce off things", "slider", number(w->restitution), "0.05");
            out += field("fluid.color", "Color", "color", hex_color(w->color));
            out += field("fluid.opacity", "Opacity", "slider", number(w->opacity), "0.05");
            out += field("fluid.current", "Current (m/s)", "vec3", vec_text(w->current), "0.1");
            out += field("fluid.wave_height", "Wind waves (m)", "float", number(w->wave_height), "0.05");
            out += field("fluid.resolution", "Cell or particle size (m, 0 = automatic)", "float", number(w->resolution), "0.05");
            out += field("fluid.infinite", "Refills itself (infinite source)", "bool", yesno(w->infinite));
            out += field("fluid.enabled", "On", "bool", yesno(w->enabled));
        }

        group("Material");
        const orchard::Material* m = m_scene.material(e);
        out += field("has_material", "Custom material", "bool", yesno(m != nullptr));
        if (m) {
            std::vector<std::pair<std::string, std::string>> presets = {{"", "Custom"}};
            for (const std::string& p : orchard::Material::presets()) presets.emplace_back(p, p);
            out += field("material.preset", "Preset", "select", m->preset, options_of(presets));
            out += field("material.color", "Color", "color", hex_color(m->color));
            out += field("material.metallic", "Metallic", "slider", number(m->metallic), "0.05");
            out += field("material.roughness", "Roughness", "slider", number(m->roughness), "0.05");
            out += field("material.transmission", "See-through", "slider", number(m->transmission), "0.05");
            out += field("material.ior", "Index of refraction", "float", number(m->ior), "0.01");
            out += field("material.emission", "Glow color", "color", hex_color(m->emission));
            out += field("material.emission_strength", "Glow strength", "float", number(m->emission_strength), "0.5");
            out += field("material.opacity", "Opacity", "slider", number(m->opacity), "0.05");
            out += field("material.reflectivity", "Reflects (0 = matte, 1 = mirror)", "slider", number(m->reflectivity), "0.05");
            std::vector<std::pair<std::string, std::string>> images = {{"", "None"}};
            if (m_project)
                for (const std::string& img : m_project->images()) images.emplace_back(img, img);
            for (const std::string& used : m->texture_paths()) {
                bool listed = false;
                for (const auto& it : images) listed = listed || it.first == used;
                if (!listed) images.emplace_back(used, used + " (missing)");
            }
            const std::string imgs = options_of(images);
            out += field("material.base_map", "Color texture", "select", m->base_map, imgs, "Put images in the project's Assets or Textures folder.");
            out += field("material.normal_map", "Bump (normal map)", "select", m->normal_map, imgs);
            if (!m->normal_map.empty()) out += field("material.normal_strength", "Bump strength", "float", number(m->normal_strength), "0.1");
            out += field("material.orm_map", "Packed occlusion/roughness/metal (ORM)", "select", m->orm_map, imgs);
            out += field("material.roughness_map", "Roughness texture", "select", m->roughness_map, imgs);
            out += field("material.metallic_map", "Metallic texture", "select", m->metallic_map, imgs);
            out += field("material.emission_map", "Glow texture", "select", m->emission_map, imgs);
            out += field("material.opacity_map", "Opacity texture", "select", m->opacity_map, imgs);
            if (m->has_textures()) {
                out += field("material.alpha_cutoff", "Cut-out below alpha (0 = blend)", "slider", number(m->alpha_cutoff), "0.05");
                out += field("material.tiling_u", "Repeat across", "float", number(m->tiling_u), "0.25");
                out += field("material.tiling_v", "Repeat down", "float", number(m->tiling_v), "0.25");
                out += field("material.offset_u", "Shift across", "float", number(m->offset_u), "0.05");
                out += field("material.offset_v", "Shift down", "float", number(m->offset_v), "0.05");
                out += field("material.uv_rotation", "Texture angle (degrees)", "float", number(m->uv_rotation), "5");
                out += field("material.pixelated", "Pixelated (no smoothing)", "bool", yesno(m->pixelated));
                out += field("material.triplanar", "Project from the sides (no UVs needed)", "bool", yesno(m->triplanar));
                if (m->triplanar) out += field("material.triplanar_scale", "Projection size (m per repeat)", "float", number(m->triplanar_scale), "0.1");
            }
            out += field("material.casts_shadows", "Casts shadows", "bool", yesno(m->casts_shadows));
            out += field("material.visible", "Visible", "bool", yesno(m->visible));
        }
        return out;
    }

    void light_preset(orchard::Light& l, const std::string& name) {
        orchard::LightAnimation a;
        if (name == "strobe") a = orchard::LightAnimation::strobe(10.0f, 0.15f);
        else if (name == "police") {
            a = orchard::LightAnimation::strobe(3.0f, 0.5f);
            a.low = 1.0f;
            a.color_mode = orchard::LightColorMode::alternate;
            a.color_b = {0.05f, 0.1f, 1.0f};
            l.color = {1.0f, 0.03f, 0.02f};
        } else if (name == "pulse") a = orchard::LightAnimation::pulse(0.5f, 0.15f);
        else if (name == "candle") {
            a = orchard::LightAnimation::flicker(9.0f, 0.65f);
            l.temperature = 1900.0f;
        } else if (name == "broken") {
            a = orchard::LightAnimation::flicker(14.0f, 0.0f);
            a.softness = 0.0f;
        } else if (name == "rainbow") {
            a.color_mode = orchard::LightColorMode::rainbow;
            a.rate = a.rate_end = 0.25f;
            l.temperature = 0.0f;
        } else if (name == "speedup") {
            a = orchard::LightAnimation::strobe(1.0f, 0.4f);
            a.rate_end = 16.0f;
            a.duration = 6.0f;
        } else if (name != "off") {
            return;
        }
        l.animation = a;
    }

    std::string inspect_set(orchard::Entity e, const std::string& key, const std::string& value) {
        if (!m_scene.alive(e)) return std::string();
        const bool on = value == "1" || value == "true";
        const float v = static_cast<float>(std::atof(value.c_str()));
        orchard::Vec3 vec;
        if (key == "name") return m_scene.set_name(e, clean(value)) ? "ok" : "";
        if (key == "position" || key == "rotation" || key == "scale") {
            orchard::Transform* t = m_scene.transform(e);
            if (!t || !parse_vec3(value, vec)) return std::string();
            if (key == "position") t->position = vec;
            else if (key == "scale") t->scale = vec;
            else t->rotation = orchard::from_euler(vec);
            if (m_session.world()) m_session.world()->moved(e);
            return "ok";
        }
        if (key == "has_body") return set_component(e, "body", value) ? "rebuild" : "";
        if (key == "add_collider") return set_component(e, "add_collider", value.empty() ? "box" : value) ? "rebuild" : "";
        if (key == "remove_collider") return set_component(e, "remove_collider", value) ? "rebuild" : "";
        auto ends = [&key](const std::string& suffix) { return key.size() >= suffix.size() && key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0; };
        if (key.rfind("collider:", 0) == 0 && (ends(":size") || ends(":offset"))) {
            if (!parse_vec3(value, vec)) return std::string();
            const std::string base = key.substr(0, key.rfind(':') + 1);
            const bool size = ends(":size");
            const bool ok = set_component(e, base + (size ? "sx" : "ox"), number(vec.x)) && set_component(e, base + (size ? "sy" : "oy"), number(vec.y)) && set_component(e, base + (size ? "sz" : "oz"), number(vec.z));
            return ok ? "ok" : "";
        }
        if (key.rfind("collider:", 0) == 0 && ends(":shape")) return set_component(e, key, value) ? "rebuild" : "";
        if (key == "body_type") return set_component(e, key, value) ? "rebuild" : "";
        if (key == "has_light") {
            if (on) {
                if (!m_scene.light(e)) m_scene.add_light(e);
            } else {
                m_scene.remove_light(e);
            }
            m_light_dirty = true;
            return "rebuild";
        }
        if (key == "has_fluid") {
            if (on) {
                if (!m_scene.fluid(e)) m_scene.add_fluid(e);
            } else {
                m_scene.remove_fluid(e);
            }
            return "rebuild";
        }
        if (key.rfind("fluid.", 0) == 0) {
            orchard::Fluid* w = m_scene.fluid(e);
            if (!w) return std::string();
            const std::string k = key.substr(6);
            if (k == "level") {
                const auto lv = orchard::parse_fluid_level(value);
                if (!lv) return std::string();
                w->level = *lv;
                return "ok";
            }
            if (k == "preset") {
                const std::optional<orchard::Fluid> named = orchard::Fluid::named(value);
                if (!named) return std::string();
                const orchard::Fluid keep = *w;
                *w = *named;
                w->level = keep.level;
                w->size = keep.size;
                w->fill = keep.fill;
                w->current = keep.current;
                w->wave_height = keep.wave_height;
                w->resolution = keep.resolution;
                w->infinite = keep.infinite;
                return "rebuild";
            }
            if (k == "color") return parse_hex_color(value, w->color) ? "ok" : "";
            if (k == "size") return parse_vec3(value, w->size) ? "ok" : "";
            if (k == "current") return parse_vec3(value, w->current) ? "ok" : "";
            if (k == "infinite") {
                w->infinite = on;
                return "ok";
            }
            if (k == "enabled") {
                w->enabled = on;
                return "ok";
            }
            float* target = nullptr;
            if (k == "fill") target = &w->fill;
            else if (k == "density") target = &w->density;
            else if (k == "viscosity") target = &w->viscosity;
            else if (k == "surface_tension") target = &w->surface_tension;
            else if (k == "drag") target = &w->drag;
            else if (k == "quadratic_drag") target = &w->quadratic_drag;
            else if (k == "angular_drag") target = &w->angular_drag;
            else if (k == "friction") target = &w->friction;
            else if (k == "bounce") target = &w->restitution;
            else if (k == "opacity") target = &w->opacity;
            else if (k == "wave_height") target = &w->wave_height;
            else if (k == "resolution") target = &w->resolution;
            if (!target) return std::string();
            float x = std::max(0.0f, v);
            if (k == "fill" || k == "opacity" || k == "bounce") x = std::clamp(v, 0.0f, 1.0f);
            if (k == "density") x = std::max(1.0f, v);
            *target = x;
            w->preset.clear();
            return "ok";
        }
        if (key == "template_info") {
            const std::string r = begin_template_edit(m_scene.instance_of(e));
            return r.rfind("error", 0) == 0 ? std::string() : std::string("template");
        }
        if (key == "template_unlink") {
            m_scene.set_instance_of(e, std::string());
            return "rebuild";
        }
        if (key == "has_emitter") {
            if (on) {
                if (!m_scene.emitter(e)) m_scene.add_emitter(e);
            } else {
                m_scene.remove_emitter(e);
            }
            return "rebuild";
        }
        if (key == "emitter_burst") {
            m_particle_commands.push_back(orchard::ParticleCommand{e, 2, 0});
            m_particle_burst_until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            set_animating(true);
            redraw();
            return "ok";
        }
        if (key.rfind("emitter.", 0) == 0) {
            orchard::Emitter* pe = m_scene.emitter(e);
            if (!pe) return std::string();
            const std::string k = key.substr(8);
            const int n = std::atoi(value.c_str());
            if (k == "preset") {
                const std::optional<orchard::Emitter> named = orchard::Emitter::named(value);
                if (!named) return std::string();
                const orchard::Emitter keep = *pe;
                *pe = *named;
                pe->enabled = keep.enabled;
                pe->autoplay = keep.autoplay;
                pe->local_space = keep.local_space;
                pe->texture = keep.texture;
                pe->frames = keep.frames;
                return "rebuild";
            }
            auto flag = [&](bool& b, bool rebuild) {
                b = on;
                return std::string(rebuild ? "rebuild" : "ok");
            };
            if (k == "enabled") return flag(pe->enabled, false);
            if (k == "autoplay") return flag(pe->autoplay, false);
            if (k == "loop") return flag(pe->loop, false);
            if (k == "lit") return flag(pe->lit, false);
            if (k == "collide") return flag(pe->collide, true);
            if (k == "die_on_hit") return flag(pe->die_on_hit, false);
            if (k == "local_space") return flag(pe->local_space, false);
            if (k == "color") return parse_hex_color(value, pe->color) ? "ok" : "";
            if (k == "color_end") return parse_hex_color(value, pe->color_end) ? "ok" : "";
            if (k == "direction") return parse_vec3(value, pe->direction) ? "ok" : "";
            if (k == "shape_size") return parse_vec3(value, pe->shape_size) ? "ok" : "";
            if (k == "wind") return parse_vec3(value, pe->wind) ? "ok" : "";
            if (k == "shape") {
                const auto sh = orchard::parse_emitter_shape(value);
                if (!sh) return std::string();
                pe->shape = *sh;
                return "rebuild";
            }
            if (k == "blend") {
                const auto bl = orchard::parse_particle_blend(value);
                if (!bl) return std::string();
                pe->blend = *bl;
                return "ok";
            }
            if (k == "texture") {
                const bool had = !pe->texture.empty();
                pe->texture = value;
                return had != !pe->texture.empty() ? "rebuild" : "ok";
            }
            if (k == "on_death") {
                pe->on_death = value;
                return "ok";
            }
            if (k == "burst") {
                pe->burst = std::max(0, n);
                return "ok";
            }
            if (k == "frames") {
                pe->frames = std::clamp(n, 1, 256);
                return "ok";
            }
            if (k == "max_particles") {
                pe->max_particles = std::clamp(n, 1, 1000000);
                return "ok";
            }
            if (k == "priority") {
                pe->priority = std::clamp(n, 0, 3);
                return "ok";
            }
            float* target = nullptr;
            if (k == "duration") target = &pe->duration;
            else if (k == "rate") target = &pe->rate;
            else if (k == "burst_every") target = &pe->burst_every;
            else if (k == "lifetime") target = &pe->lifetime;
            else if (k == "lifetime_spread") target = &pe->lifetime_spread;
            else if (k == "speed") target = &pe->speed;
            else if (k == "speed_spread") target = &pe->speed_spread;
            else if (k == "spread") target = &pe->spread;
            else if (k == "gravity") target = &pe->gravity;
            else if (k == "drag") target = &pe->drag;
            else if (k == "turbulence") target = &pe->turbulence;
            else if (k == "size") target = &pe->size;
            else if (k == "size_end") target = &pe->size_end;
            else if (k == "alpha") target = &pe->alpha;
            else if (k == "alpha_end") target = &pe->alpha_end;
            else if (k == "glow") target = &pe->glow;
            else if (k == "stretch") target = &pe->stretch;
            else if (k == "bounce") target = &pe->bounce;
            else if (k == "friction") target = &pe->friction;
            else if (k == "inherit_velocity") target = &pe->inherit_velocity;
            if (!target) return std::string();
            float x = v;
            if (k != "gravity" && k != "speed" && k != "inherit_velocity") x = std::max(0.0f, v);
            if (k == "alpha" || k == "alpha_end" || k == "bounce") x = std::clamp(v, 0.0f, 1.0f);
            if (k == "spread") x = std::clamp(v, 0.0f, 180.0f);
            if (k == "lifetime") x = std::max(0.01f, v);
            *target = x;
            pe->preset.clear();
            return "ok";
        }
        if (key == "has_material") {
            if (on) {
                if (!m_scene.material(e)) m_scene.add_material(e);
            } else {
                m_scene.remove_material(e);
            }
            m_light_dirty = true;
            return "rebuild";
        }
        if (key.rfind("light.", 0) == 0 || key.rfind("anim.", 0) == 0) {
            orchard::Light* l = m_scene.light(e);
            if (!l) return std::string();
            m_light_dirty = true;
            const std::string k = key.substr(key.find('.') + 1);
            if (key.rfind("anim.", 0) == 0) {
                orchard::LightAnimation& a = l->animation;
                if (k == "mode") {
                    const auto m = orchard::parse_light_animation_mode(value);
                    if (!m) return std::string();
                    a.mode = *m;
                    return "ok";
                }
                if (k == "color_mode") {
                    const auto m = orchard::parse_light_color_mode(value);
                    if (!m) return std::string();
                    a.color_mode = *m;
                    return "ok";
                }
                if (k == "preset") {
                    if (value.empty()) return "ok";
                    light_preset(*l, value);
                    return "rebuild";
                }
                if (k == "color_b") return parse_hex_color(value, a.color_b) ? "ok" : "";
                if (k == "loop") {
                    a.loop = on;
                    return "ok";
                }
                if (k == "rate") {
                    const bool linked = a.rate_end == a.rate;
                    a.rate = std::max(0.0f, v);
                    if (linked) a.rate_end = a.rate;
                    return linked ? "rebuild" : "ok";
                }
                if (k == "high") {
                    const bool linked = a.high_end == a.high;
                    a.high = std::max(0.0f, v);
                    if (linked) a.high_end = a.high;
                    return linked ? "rebuild" : "ok";
                }
                float* target = nullptr;
                if (k == "rate_end") target = &a.rate_end;
                else if (k == "on_fraction") target = &a.on_fraction;
                else if (k == "softness") target = &a.softness;
                else if (k == "duration") target = &a.duration;
                else if (k == "delay") target = &a.delay;
                else if (k == "low") target = &a.low;
                else if (k == "high_end") target = &a.high_end;
                else if (k == "phase") target = &a.phase;
                if (!target) return std::string();
                float x = v;
                if (k == "on_fraction" || k == "softness" || k == "phase") x = std::clamp(x, 0.0f, 1.0f);
                else x = std::max(0.0f, x);
                *target = x;
                return "ok";
            }
            if (k == "type") {
                const auto t = orchard::parse_light_type(value);
                if (!t) return std::string();
                l->type = *t;
                return "rebuild";
            }
            if (k == "unit") {
                const auto u = orchard::parse_light_unit(value);
                if (!u) return std::string();
                l->unit = *u;
                return "ok";
            }
            if (k == "falloff") {
                const auto fo = orchard::parse_light_falloff(value);
                if (!fo) return std::string();
                l->falloff = *fo;
                return "ok";
            }
            if (k == "spread") {
                const auto sp = orchard::parse_light_spread(value);
                if (!sp) return std::string();
                l->spread = *sp;
                return "ok";
            }
            if (k == "mobility") {
                const auto mo = orchard::parse_light_mobility(value);
                if (!mo) return std::string();
                l->mobility = *mo;
                return "ok";
            }
            if (k == "color") return parse_hex_color(value, l->color) ? "ok" : "";
            bool* flag = nullptr;
            if (k == "enabled") flag = &l->enabled;
            else if (k == "two_sided") flag = &l->two_sided;
            else if (k == "shadows") flag = &l->shadows;
            else if (k == "visible") flag = &l->visible;
            if (flag) {
                *flag = on;
                return "ok";
            }
            float* target = nullptr;
            if (k == "temperature") target = &l->temperature;
            else if (k == "intensity") target = &l->intensity;
            else if (k == "range") target = &l->range;
            else if (k == "inner_cone") target = &l->inner_cone;
            else if (k == "outer_cone") target = &l->outer_cone;
            else if (k == "angular_diameter") target = &l->angular_diameter;
            else if (k == "radius") target = &l->radius;
            else if (k == "length") target = &l->length;
            else if (k == "width") target = &l->width;
            else if (k == "height") target = &l->height;
            else if (k == "shadow_strength") target = &l->shadow_strength;
            else if (k == "diffuse") target = &l->diffuse;
            else if (k == "specular") target = &l->specular;
            else if (k == "volumetric") target = &l->volumetric;
            else if (k == "spill") target = &l->spill;
            if (!target) return std::string();
            float x = std::max(0.0f, v);
            if (k == "inner_cone" || k == "outer_cone") x = std::clamp(v, 0.0f, 179.0f);
            if (k == "shadow_strength") x = std::clamp(v, 0.0f, 1.0f);
            *target = x;
            return "ok";
        }
        if (key.rfind("material.", 0) == 0) {
            orchard::Material* m = m_scene.material(e);
            if (!m) return std::string();
            m_light_dirty = true;
            const std::string k = key.substr(9);
            if (k == "preset") {
                if (value.empty()) {
                    m->preset.clear();
                    return "ok";
                }
                const std::optional<orchard::Material> named = orchard::Material::named(value);
                if (!named) return std::string();
                orchard::Material keep = *m;
                *m = *named;
                m->base_map = keep.base_map;
                m->normal_map = keep.normal_map;
                m->orm_map = keep.orm_map;
                m->roughness_map = keep.roughness_map;
                m->metallic_map = keep.metallic_map;
                m->emission_map = keep.emission_map;
                m->opacity_map = keep.opacity_map;
                m->normal_strength = keep.normal_strength;
                m->alpha_cutoff = keep.alpha_cutoff;
                m->tiling_u = keep.tiling_u;
                m->tiling_v = keep.tiling_v;
                m->offset_u = keep.offset_u;
                m->offset_v = keep.offset_v;
                m->uv_rotation = keep.uv_rotation;
                m->pixelated = keep.pixelated;
                m->triplanar = keep.triplanar;
                m->triplanar_scale = keep.triplanar_scale;
                return "rebuild";
            }
            {
                std::string* map = nullptr;
                if (k == "base_map") map = &m->base_map;
                else if (k == "normal_map") map = &m->normal_map;
                else if (k == "orm_map") map = &m->orm_map;
                else if (k == "roughness_map") map = &m->roughness_map;
                else if (k == "metallic_map") map = &m->metallic_map;
                else if (k == "emission_map") map = &m->emission_map;
                else if (k == "opacity_map") map = &m->opacity_map;
                if (map) {
                    const bool had = m->has_textures();
                    const bool had_normal = !m->normal_map.empty();
                    *map = value;
                    return had != m->has_textures() || had_normal != !m->normal_map.empty() ? "rebuild" : "ok";
                }
            }
            if (k == "pixelated") {
                m->pixelated = on;
                return "ok";
            }
            if (k == "triplanar") {
                m->triplanar = on;
                return "rebuild";
            }
            if (k == "color") return parse_hex_color(value, m->color) ? "ok" : "";
            if (k == "emission") return parse_hex_color(value, m->emission) ? "ok" : "";
            if (k == "casts_shadows") {
                m->casts_shadows = on;
                return "ok";
            }
            if (k == "visible") {
                m->visible = on;
                return "ok";
            }
            float* target = nullptr;
            if (k == "metallic") target = &m->metallic;
            else if (k == "roughness") target = &m->roughness;
            else if (k == "transmission") target = &m->transmission;
            else if (k == "ior") target = &m->ior;
            else if (k == "emission_strength") target = &m->emission_strength;
            else if (k == "opacity") target = &m->opacity;
            else if (k == "reflectivity") target = &m->reflectivity;
            else if (k == "normal_strength") target = &m->normal_strength;
            else if (k == "alpha_cutoff") target = &m->alpha_cutoff;
            else if (k == "triplanar_scale") target = &m->triplanar_scale;
            if (k == "tiling_u" || k == "tiling_v" || k == "offset_u" || k == "offset_v" || k == "uv_rotation") {
                float* uv = k == "tiling_u" ? &m->tiling_u : k == "tiling_v" ? &m->tiling_v : k == "offset_u" ? &m->offset_u : k == "offset_v" ? &m->offset_v : &m->uv_rotation;
                *uv = (k == "tiling_u" || k == "tiling_v") && std::fabs(v) < 0.001f ? 0.001f : v;
                return "ok";
            }
            if (!target) return std::string();
            float x = std::max(0.0f, v);
            if (k == "metallic" || k == "roughness" || k == "transmission" || k == "opacity" || k == "reflectivity" || k == "alpha_cutoff") x = std::clamp(v, 0.0f, 1.0f);
            if (k == "ior") x = std::max(1.0f, v);
            if (k == "triplanar_scale") x = std::max(0.01f, v);
            *target = x;
            return "ok";
        }
        return set_component(e, key, value) ? "ok" : "";
    }

    orchard::Transform spawn_spot() {
        orchard::Transform at;
        if (m_view.dimension() == orchard::Dimension::three_d) {
            const orchard::Vec3 t = m_view.orbit_camera().target;
            at.position = t + m_view.axis_system().up() * 1.0f;
        } else {
            at.position = orchard::Vec3{m_view.pan_camera().center.x, m_view.pan_camera().center.y, 0.0f};
        }
        return at;
    }

    std::string template_label(const std::string& path) const {
        std::string label = path;
        const std::size_t slash = label.find_last_of('/');
        if (slash != std::string::npos) label = label.substr(slash + 1);
        const std::size_t dot = label.find_last_of('.');
        if (dot != std::string::npos && dot > 0) label = label.substr(0, dot);
        return label;
    }

    orchard::Scene& main_scene() {
        return m_main_scene ? *m_main_scene : m_scene;
    }

    std::string template_summary(const orchard::Scene& t) const {
        std::size_t colliders = 0, scripts = 0, lights = 0, materials = 0, emitters = 0, bodies = 0;
        for (orchard::Entity e : t.entities()) {
            if (const auto* c = t.colliders(e)) colliders += c->size();
            if (!t.script(e).empty()) ++scripts;
            if (t.light(e)) ++lights;
            if (t.material(e)) ++materials;
            if (t.emitter(e)) ++emitters;
            if (t.body(e)) ++bodies;
        }
        std::string out = std::to_string(t.size()) + (t.size() == 1 ? " part" : " parts");
        if (bodies) out += ", " + std::to_string(bodies) + (bodies == 1 ? " body" : " bodies");
        if (colliders) out += ", " + std::to_string(colliders) + (colliders == 1 ? " collider" : " colliders");
        if (scripts) out += ", " + std::to_string(scripts) + (scripts == 1 ? " script" : " scripts");
        if (materials) out += ", " + std::to_string(materials) + (materials == 1 ? " material" : " materials");
        if (lights) out += ", " + std::to_string(lights) + (lights == 1 ? " light" : " lights");
        if (emitters) out += ", " + std::to_string(emitters) + (emitters == 1 ? " particle emitter" : " particle emitters");
        return out;
    }

    std::string templates_list() {
        std::string out;
        if (!m_project) return out;
        const orchard::Scene& scene = m_main_scene ? *m_main_scene : m_scene;
        for (const std::string& path : m_project->entity_templates()) {
            std::string error;
            const std::optional<orchard::Scene> t = orchard::Scene::load_file(m_project->path_of(path), &error);
            const std::string summary = t ? template_summary(*t) : "can't read: " + error;
            out += clean(path) + "\t" + clean(template_label(path)) + "\t" + std::to_string(scene.instances_of(path).size()) + "\t" + clean(summary) + "\t" + (path == m_template_path ? "1" : "0") + "\n";
        }
        return out;
    }

    std::string begin_template_edit(const std::string& path) {
        if (!m_project) return "error\tno project is open";
        if (m_session.playing() || m_session.building()) return "error\tstop playing first";
        std::string error;
        std::optional<orchard::Scene> t = orchard::Scene::load_file(m_project->path_of(path), &error);
        if (!t) return "error\t" + path + ": " + error;
        if (!m_template_path.empty()) {
            const std::string r = end_template_edit(true);
            if (r.rfind("error", 0) == 0) return r;
        }
        m_main_scene = std::move(m_scene);
        m_main_selected = m_selected;
        m_main_dirty = m_dirty;
        m_scene = std::move(*t);
        m_template_path = path;
        const std::vector<orchard::Entity> roots = m_scene.roots();
        m_selected = roots.empty() ? orchard::Entity{} : roots.front();
        m_dirty = false;
        m_light_built = false;
        m_light_dirty = true;
        redraw();
        return "ok\t" + std::string(m_scene.alive(m_selected) ? std::to_string(m_selected.id()) : "");
    }

    std::string end_template_edit(bool save) {
        if (m_template_path.empty() || !m_main_scene) return "ok";
        if (m_session.playing() || m_session.building()) return "error\tstop playing first";
        if (save && !m_scene.save_file(m_project->path_of(m_template_path))) return "error\tcan't write " + m_template_path;
        const orchard::Scene edited = std::move(m_scene);
        m_scene = std::move(*m_main_scene);
        m_main_scene.reset();
        m_selected = m_scene.alive(m_main_selected) ? m_main_selected : orchard::Entity{};
        m_dirty = m_main_dirty;
        std::size_t refreshed = 0;
        if (save) refreshed = m_scene.refresh_instances(m_template_path, edited);
        if (refreshed) m_dirty = true;
        m_template_path.clear();
        m_light_built = false;
        m_light_dirty = true;
        redraw();
        return "ok\t" + std::to_string(refreshed);
    }

    std::string new_template(const std::string& name, bool from_selection) {
        if (!m_project) return "error\tno project is open";
        if (m_session.playing() || m_session.building()) return "error\tstop playing first";
        std::string base = name;
        orchard::Scene& scene = main_scene();
        const bool use_selection = from_selection && m_template_path.empty() && scene.alive(m_selected);
        if (base.empty()) base = use_selection ? scene.name(m_selected) : std::string("Entity");
        std::string path = orchard::Project::entity_template_path(base);
        const std::string stem = path.substr(0, path.size() - 7);
        for (int n = 2; fs::exists(m_project->path_of(path)) && n < 10000; ++n) path = stem + " " + std::to_string(n) + ".entity";
        std::error_code ec;
        fs::create_directories(fs::path(m_project->path_of(path)).parent_path(), ec);
        orchard::Scene t(m_project->info().dimension);
        if (use_selection) {
            t = scene.extract(m_selected);
        } else {
            const orchard::Entity root = t.create(template_label(path));
            t.add_collider(root, orchard::ColliderShape::box);
        }
        if (!t.save_file(m_project->path_of(path))) return "error\tcan't write " + path;
        if (use_selection) {
            scene.set_instance_of(m_selected, path);
            touched();
        }
        return "ok\t" + path;
    }

    std::string place_template(const std::string& path) {
        if (!m_project) return "error\tno project is open";
        if (path == m_template_path) return "error\tan entity can't contain itself";
        std::string error;
        const std::optional<orchard::Scene> t = orchard::Scene::load_file(m_project->path_of(path), &error);
        if (!t) return "error\t" + path + ": " + error;
        const orchard::Entity e = m_scene.instantiate(*t, path, orchard::Entity{}, spawn_spot());
        if (!m_scene.alive(e)) return "error\tthis entity template is empty";
        if (m_session.playing()) {
            if (xylem::World* w = m_session.world()) w->added(e);
        }
        m_selected = e;
        touched();
        return "ok\t" + std::to_string(e.id());
    }

    std::string duplicate_template(const std::string& path) {
        if (!m_project) return "error\tno project is open";
        std::string error;
        const std::optional<orchard::Scene> t = orchard::Scene::load_file(m_project->path_of(path), &error);
        if (!t) return "error\t" + path + ": " + error;
        std::string copy = orchard::Project::entity_template_path(template_label(path) + " copy");
        const std::string stem = copy.substr(0, copy.size() - 7);
        for (int n = 2; fs::exists(m_project->path_of(copy)) && n < 10000; ++n) copy = stem + " " + std::to_string(n) + ".entity";
        if (!t->save_file(m_project->path_of(copy))) return "error\tcan't write " + copy;
        return "ok\t" + copy;
    }

    std::string rename_template(const std::string& path, const std::string& name) {
        if (!m_project) return "error\tno project is open";
        if (path == m_template_path) return "error\tfinish editing it first";
        const std::string to = orchard::Project::entity_template_path(name);
        if (to == path) return "ok\t" + to;
        if (fs::exists(m_project->path_of(to))) return "error\t" + to + " already exists";
        std::error_code ec;
        fs::rename(m_project->path_of(path), m_project->path_of(to), ec);
        if (ec) return "error\t" + ec.message();
        orchard::Scene& scene = main_scene();
        for (orchard::Entity e : scene.instances_of(path)) scene.set_instance_of(e, to);
        touched();
        return "ok\t" + to;
    }

    std::string delete_template(const std::string& path) {
        if (!m_project) return "error\tno project is open";
        if (path == m_template_path) end_template_edit(false);
        std::error_code ec;
        fs::remove(m_project->path_of(path), ec);
        if (ec) return "error\t" + ec.message();
        orchard::Scene& scene = main_scene();
        for (orchard::Entity e : scene.instances_of(path)) scene.set_instance_of(e, std::string());
        touched();
        return "ok";
    }

    std::vector<grove::MenuItem> menu_items(orchard::Entity e) const {
        std::vector<grove::MenuItem> items;
        if (!m_scene.alive(e)) {
            items.push_back(grove::menu_item("add:box", "Add a physics box"));
            items.push_back(grove::menu_item("add:ball", "Add a physics ball"));
            items.push_back(grove::menu_item("add:ground", "Add a floor"));
            items.push_back(grove::menu_item("add:trigger", "Add a trigger zone"));
            items.push_back(grove::menu_item("add:light", "Add a light"));
            items.push_back(grove::menu_item("add:water", "Add water"));
            items.push_back(grove::menu_item("add:pool", "Add a pool (walls and water)"));
            items.push_back(grove::menu_item("add:hemisphere", "Add a hemisphere"));
            items.push_back(grove::menu_item("add:sparks", "Add sparks"));
            items.push_back(grove::menu_item("add:smoke", "Add smoke"));
            items.push_back(grove::menu_item("add:fire", "Add fire"));
            items.push_back(grove::menu_item("add:", "Add an empty"));
            return items;
        }
        const bool busy = m_session.playing() || m_session.building();
        items.push_back(grove::menu_item("inspect", "Edit properties…", "Enter"));
        items.push_back(grove::menu_item("details", "Show details"));
        items.push_back(grove::menu_item("rename", "Rename…", "F2"));
        items.push_back(grove::menu_item("focus", "Look at it", "F"));
        items.push_back(grove::menu_item("move", "Move it (drag in the view)", "G"));
        items.push_back(grove::menu_separator());
        const orchard::Body* b = m_scene.body(e);
        if (!b) items.push_back(grove::menu_item("body:on", "Make it a rigid body"));
        else {
            if (b->type != orchard::BodyType::fixed) items.push_back(grove::menu_item("body:fixed", "Make it a container (never moves)"));
            if (b->type != orchard::BodyType::dynamic) items.push_back(grove::menu_item("body:dynamic", "Make it dynamic (falls, gets pushed)"));
            items.push_back(grove::menu_item("body:off", "Remove its rigid body"));
        }
        items.push_back(grove::menu_item("collider", "Add a collider"));
        const orchard::Light* l = m_scene.light(e);
        if (!l) items.push_back(grove::menu_item("light:on", "Make it give off light"));
        else {
            items.push_back(grove::menu_item(l->enabled ? "light:disable" : "light:enable", l->enabled ? "Switch the light off" : "Switch the light on"));
            if (l->animation.active()) items.push_back(grove::menu_item("light:steady", "Stop strobing and effects"));
            else items.push_back(grove::menu_item("light:strobe", "Make it strobe"));
            items.push_back(grove::menu_item("light:off", "Remove the light"));
        }
        if (!m_scene.material(e)) items.push_back(grove::menu_item("material:on", "Give it a material"));
        else items.push_back(grove::menu_item("material:off", "Remove its material"));
        items.push_back(grove::menu_separator());
        if (!m_scene.instance_of(e).empty()) {
            items.push_back(grove::menu_item("template:edit", "Edit its entity template", "", busy));
            items.push_back(grove::menu_item("template:unlink", "Unlink from its template"));
        } else if (m_template_path.empty()) {
            items.push_back(grove::menu_item("template:make", "Make an entity template from this", "", busy));
        }
        if (!m_scene.emitter(e)) items.push_back(grove::menu_item("particles:on", "Give it particles"));
        else items.push_back(grove::menu_item("particles:off", "Remove its particles"));
        items.push_back(grove::menu_item("duplicate", "Duplicate", "Ctrl+D", busy));
        items.push_back(grove::menu_separator());
        items.push_back(grove::menu_item("child", "Add a child"));
        items.push_back(grove::menu_item("delete", "Delete", "Delete", busy));
        return items;
    }

    std::string menu_action(orchard::Entity e, const std::string& action) {
        if (action.rfind("add:", 0) == 0) {
            const std::string kind = action.substr(4);
            orchard::Entity made;
            if (kind.empty()) {
                made = m_scene.create("Entity", orchard::Entity{});
            } else {
                made = add_preset(kind, orchard::Entity{});
            }
            m_selected = made;
            m_light_dirty = true;
            touched();
            return "select\t" + std::to_string(made.id());
        }
        if (!m_scene.alive(e)) return std::string();
        m_selected = e;
        if (action == "inspect") return "inspect\t" + std::to_string(e.id());
        if (action == "details") return "details\t" + std::to_string(e.id());
        if (action == "rename") return "rename\t" + std::to_string(e.id());
        if (action == "move") {
            m_tool = "move";
            redraw();
            return "tool\tmove\t" + std::to_string(e.id());
        }
        if (action == "focus") {
            if (m_scene.dimension() == orchard::Dimension::three_d) m_view.orbit_camera().target = xylem::world_position(m_scene, e);
            else {
                const orchard::Vec3 p = xylem::world_position(m_scene, e);
                m_view.pan_camera().center = {p.x, p.y};
            }
            moved_view();
            redraw();
            return "ok";
        }
        if (action == "delete") {
            m_scene.destroy(e);
            if (!m_scene.alive(m_selected)) m_selected = orchard::Entity{};
            m_light_dirty = true;
            touched();
            return "deleted";
        }
        if (action == "template:edit") {
            const std::string r = begin_template_edit(m_scene.instance_of(e));
            if (r.rfind("error", 0) == 0) return r;
            return "template\t" + r.substr(3);
        }
        if (action == "template:unlink") {
            m_scene.set_instance_of(e, std::string());
            touched();
            return "changed\t" + std::to_string(e.id());
        }
        if (action == "template:make") {
            const std::string r = new_template(std::string(), true);
            if (r.rfind("error", 0) == 0) return r;
            return "templates\t" + std::to_string(e.id());
        }
        if (action == "duplicate") {
            const orchard::Entity c = m_scene.duplicate(e);
            if (!m_scene.alive(c)) return std::string();
            if (orchard::Transform* t = m_scene.transform(c)) t->position.x += 1.0f;
            m_selected = c;
            touched();
            return "select\t" + std::to_string(c.id());
        }
        if (action == "particles:on") {
            if (!m_scene.emitter(e)) m_scene.add_emitter(e);
            touched();
            return "changed\t" + std::to_string(e.id());
        }
        if (action == "particles:off") {
            m_scene.remove_emitter(e);
            touched();
            return "changed\t" + std::to_string(e.id());
        }
        if (action == "child") {
            const orchard::Entity c = m_scene.create("Entity", e);
            m_selected = c;
            touched();
            return "select\t" + std::to_string(c.id());
        }
        if (action == "body:on") set_component(e, "body", "1");
        else if (action == "body:off") set_component(e, "body", "0");
        else if (action == "body:fixed") set_component(e, "body_type", "fixed");
        else if (action == "body:dynamic") set_component(e, "body_type", "dynamic");
        else if (action == "collider") set_component(e, "add_collider", "box");
        else if (action == "light:on") {
            if (!m_scene.light(e)) m_scene.add_light(e);
        } else if (action == "light:off") m_scene.remove_light(e);
        else if (action == "light:disable" || action == "light:enable") {
            if (orchard::Light* l = m_scene.light(e)) l->enabled = action == "light:enable";
        } else if (action == "light:strobe") {
            if (orchard::Light* l = m_scene.light(e)) light_preset(*l, "strobe");
        } else if (action == "light:steady") {
            if (orchard::Light* l = m_scene.light(e)) l->animation = orchard::LightAnimation{};
        } else if (action == "material:on") {
            if (!m_scene.material(e)) m_scene.add_material(e);
        } else if (action == "material:off") m_scene.remove_material(e);
        else return std::string();
        m_light_dirty = true;
        touched();
        return "changed\t" + std::to_string(e.id());
    }

    void redraw_axes_preview() {
        if (grove::Node* canvas = m_doc.find_by_id("axes-preview")) grove::redraw_canvas(m_doc, *canvas);
    }

    std::string axes_choice_fields() const {
        const orchard::editor::AxesChoice& c = m_axes_choice;
        std::string style, flips;
        if (c.dimension == orchard::Dimension::two_d) {
            style = orchard::to_string(c.y_axis);
            if (c.flip_x_2d) flips = "x";
        } else {
            style = orchard::to_string(c.system.base());
            if (c.system.flipped(orchard::AxisFlip::x)) flips += "x";
            if (c.system.flipped(orchard::AxisFlip::y)) flips += "y";
            if (c.system.flipped(orchard::AxisFlip::z)) flips += "z";
        }
        std::string match;
        if (c.dimension == orchard::Dimension::two_d) {
            match = c.flip_x_2d ? std::string() : std::string(orchard::to_string(c.y_axis));
        } else {
            for (orchard::Axes a : {orchard::Axes::y_up, orchard::Axes::y_up_left, orchard::Axes::z_up, orchard::Axes::z_up_left}) {
                if (c.system == orchard::AxisSystem(a)) match = orchard::to_string(a);
            }
            if (c.system == orchard::AxisSystem::source()) match = "source";
        }
        std::string out = orchard::to_string(c.dimension) + std::string("\t") + style + "\t" + flips + "\t" + (c.dimension == orchard::Dimension::two_d ? std::string("y-") + orchard::to_string(c.y_axis) + (c.flip_x_2d ? " flip-x" : "") : c.system.name()) + "\t" + match;
        for (const auto& [k, v] : orchard::editor::describe_axes(c)) out += "\t" + clean(k) + ": " + clean(v);
        return out;
    }

    void sync_view_axes() {
        if (!m_project) return;
        const orchard::ProjectInfo& info = m_project->info();
        m_view.set_axis_system(info.axis_system());
        m_view.set_y_axis(info.y_axis);
        m_view.set_flip_x_2d(orchard::has_flip(info.axis_flips, orchard::AxisFlip::x));
    }

    std::string flip_letters() const {
        if (!m_project) return std::string();
        const orchard::ProjectInfo& info = m_project->info();
        std::string out;
        if (orchard::has_flip(info.axis_flips, orchard::AxisFlip::x)) out += "x";
        if (info.dimension == orchard::Dimension::two_d) {
            if (info.y_axis == orchard::YAxis2D::down) out += "y";
        } else {
            if (orchard::has_flip(info.axis_flips, orchard::AxisFlip::y)) out += "y";
            if (orchard::has_flip(info.axis_flips, orchard::AxisFlip::z)) out += "z";
        }
        return out;
    }

    std::string mode() const {
        if (m_session.building()) return "building";
        if (m_session.playing()) return "playing";
        return "edit";
    }

    std::string state() const {
        std::vector<std::string> f;
        f.push_back(m_project ? "1" : "0");
        f.push_back(m_project ? clean(m_project->info().name) : std::string());
        f.push_back(m_project ? orchard::to_string(m_project->info().dimension) : std::string());
        f.push_back(m_project ? clean(m_project->folder()) : std::string());
        f.push_back(std::to_string(m_scene.size()));
        f.push_back(m_scene.alive(m_selected) ? std::to_string(m_selected.id()) : std::string());
        f.push_back(m_dirty ? "1" : "0");
        f.push_back(orchard::to_string(m_view.dimension()));
        f.push_back(ORCHARD_VERSION_STRING);
        f.push_back(mode());
        if (!m_project) f.push_back(std::string());
        else if (m_project->info().dimension == orchard::Dimension::two_d) f.push_back(orchard::to_string(m_project->info().y_axis));
        else f.push_back(orchard::to_string(m_project->info().axes));
        f.push_back(m_project ? flip_letters() : std::string());
        std::string out;
        for (std::size_t i = 0; i < f.size(); ++i) out += (i ? "\t" : "") + f[i];
        return out;
    }

    std::string components(orchard::Entity e) const {
        if (!m_scene.alive(e)) return std::string();
        const bool flat = m_scene.dimension() == orchard::Dimension::two_d;
        std::string out = "script\t" + clean(m_scene.script(e)) + "\n";
        const orchard::Entity owner = m_scene.body_owner(e);
        if (owner.valid() && owner != e) out += "owner\t" + clean(m_scene.name(owner)) + "\n";
        if (const orchard::Body* b = m_scene.body(e)) {
            out += std::string("body\t") + orchard::to_string(b->type) + "\t" + (b->enabled ? "1" : "0") + "\t" + number(b->gravity_scale) + "\t" + number(b->linear_damping) + "\t" + number(b->angular_damping) + "\n";
            for (const orchard::BodyFlagInfo& info : orchard::body_flags()) {
                if (flat ? !info.two_d : !info.three_d) continue;
                out += std::string("flag\t") + info.name + "\t" + info.label + "\t" + (b->has(info.flag) ? "1" : "0") + "\n";
            }
            auto num = [&out](const char* field, const char* label, double value, double lo, double hi, double step, const char* kind) {
                out += std::string("num\t") + field + "\t" + label + "\t" + number(static_cast<float>(value)) + "\t" + number(static_cast<float>(lo)) + "\t" + number(static_cast<float>(hi)) + "\t" + number(static_cast<float>(step)) + "\t" + kind + "\n";
            };
            auto bits = [&out](const char* field, const char* label, std::uint64_t value) {
                out += std::string("num\t") + field + "\t" + label + "\t" + std::to_string(value & 0xffffffffull) + "\t0\t4294967295\t1\tint\n";
            };
            auto pick = [&out](const char* field, const char* label, const std::string& value, const char* options) {
                out += std::string("pick\t") + field + "\t" + label + "\t" + value + "\t" + options + "\n";
            };
            num("gravity_scale", "Gravity scale", b->gravity_scale, -5.0, 5.0, 0.05, "slider");
            num("linear_damping", "Drag", b->linear_damping, 0.0, 10.0, 0.05, "slider");
            num("angular_damping", "Spin drag", b->angular_damping, 0.0, 10.0, 0.05, "slider");
            num("max_speed", "Max speed", b->max_speed, 0.0, 500.0, 0.5, "slider");
            num("max_spin", "Max spin", b->max_spin, 0.0, 200.0, 0.5, "slider");
            bits("layer", "Layer bits", b->collision_layer);
            bits("mask", "Mask bits", b->collision_mask);
            if (!flat) {
                pick("quality", "Quality", orchard::to_string(b->quality), "default=Project default|arcade=Arcade|normal=Normal|high=High|ultra=Ultra");
                pick("ccd", "Fast-move checks", orchard::to_string(b->continuous), "default=Project default|on=Always|off=Never");
                num("viters", "Velocity passes", b->velocity_iterations, 0.0, 64.0, 1.0, "int-slider");
                num("piters", "Position passes", b->position_iterations, 0.0, 32.0, 1.0, "int-slider");
                num("sleep_speed", "Sleep speed", b->sleep_speed, 0.0, 1.0, 0.005, "slider");
                num("sleep_spin", "Sleep spin", b->sleep_spin, 0.0, 2.0, 0.005, "slider");
                num("sleep_delay", "Sleep after (s)", b->sleep_delay, 0.0, 10.0, 0.05, "slider");
            }
            for (const xylem::FlagOption& o : xylem::flag_options()) {
                if (flat && !o.two_d) continue;
                out += std::string("aflag\t") + o.name + "\t" + o.label + "\t" + (xylem::flag_on(*b, o.bit) ? "1" : "0") + "\t" + o.group + "\n";
            }
        }
        if (const std::vector<orchard::Collider>* cs = m_scene.colliders(e)) {
            for (std::size_t i = 0; i < cs->size(); ++i) {
                const orchard::Collider& c = (*cs)[i];
                out += "collider\t" + std::to_string(i) + "\t" + orchard::to_string(c.shape) + "\t" + number(c.size.x) + "\t" + number(c.size.y) + "\t" + number(c.size.z)
                     + "\t" + number(c.offset.x) + "\t" + number(c.offset.y) + "\t" + number(c.offset.z) + "\t" + (c.enabled ? "1" : "0") + "\t" + (c.sensor ? "1" : "0")
                     + "\t" + number(c.density) + "\t" + number(c.friction) + "\t" + number(c.restitution)
                     + "\t" + orchard::to_string(c.friction_combine) + "\t" + orchard::to_string(c.bounce_combine)
                     + "\t" + (m_project ? orchard::combine_label(m_project->info().friction_combine) : "Geometric mean")
                     + "\t" + (m_project ? orchard::combine_label(m_project->info().bounce_combine) : "Maximum")
                     + "\t" + number(c.rounding) + "\t" + number(c.rolling_friction) + "\t" + number(c.spinning_friction)
                     + "\t" + number(c.grip.x) + "\t" + number(c.grip.y) + "\t" + number(c.grip.z) + "\t" + shape_names(flat)
                     + "\t" + number(c.polar_min) + "\t" + number(c.polar_max) + "\t" + number(c.azimuth_min) + "\t" + number(c.azimuth_max) + "\t" + number(c.shell) + "\n";
            }
        }
        return out;
    }

    static std::string shape_names(bool flat) {
        std::string names;
        for (orchard::ColliderShape shape : orchard::collider_shapes()) {
            if (flat && orchard::collider_shape_3d_only(shape)) continue;
            names += (names.empty() ? "" : " ") + std::string(orchard::to_string(shape));
        }
        return names;
    }

    bool set_component(orchard::Entity e, const std::string& field, const std::string& value) {
        if (!m_scene.alive(e)) return false;
        const bool on = value == "1" || value == "true";
        const float v = static_cast<float>(std::atof(value.c_str()));
        if (!std::isfinite(v)) return false;
        if (field == "script") return m_scene.set_script(e, value);
        if (field == "body") {
            if (on) m_scene.add_body(e);
            else m_scene.remove_body(e);
            return true;
        }
        if (field == "add_collider") {
            const std::optional<orchard::ColliderShape> shape = orchard::parse_collider_shape(value);
            return m_scene.add_collider(e, shape.value_or(orchard::ColliderShape::box)) != nullptr;
        }
        if (field == "remove_collider") return m_scene.remove_collider(e, static_cast<std::size_t>(std::atoi(value.c_str())));
        if (orchard::Body* b = m_scene.body(e)) {
            if (field == "body_type") {
                const std::optional<orchard::BodyType> t = orchard::parse_body_type(value);
                if (!t) return false;
                b->type = *t;
                return true;
            }
            if (field == "body_enabled") {
                b->enabled = on;
                return true;
            }
            if (field == "gravity_scale") {
                b->gravity_scale = v;
                return true;
            }
            if (field == "linear_damping") {
                b->linear_damping = std::max(0.0f, v);
                return true;
            }
            if (field == "angular_damping") {
                b->angular_damping = std::max(0.0f, v);
                return true;
            }
            if (field == "max_speed") {
                b->max_speed = std::clamp(v, 0.0f, 100000.0f);
                return true;
            }
            if (field == "max_spin") {
                b->max_spin = std::clamp(v, 0.0f, 100000.0f);
                return true;
            }
            if (field == "layer" || field == "mask") {
                char* end = nullptr;
                const unsigned long long bitsv = std::strtoull(value.c_str(), &end, 10);
                if (!end || *end != '\0' || value.empty() || value[0] == '-') return false;
                (field == "layer" ? b->collision_layer : b->collision_mask) = bitsv;
                return true;
            }
            if (field == "quality") {
                const std::optional<orchard::BodyQuality> q = orchard::parse_body_quality(value);
                if (!q) return false;
                b->quality = *q;
                return true;
            }
            if (field == "ccd") {
                const std::optional<orchard::BodyContinuous> c = orchard::parse_body_continuous(value);
                if (!c) return false;
                b->continuous = *c;
                return true;
            }
            if (field == "viters" || field == "piters") {
                if (!std::isfinite(v)) return false;
                (field == "viters" ? b->velocity_iterations : b->position_iterations) = std::clamp(static_cast<int>(std::lround(v)), 0, field == "viters" ? 64 : 32);
                return true;
            }
            if (field == "sleep_speed" || field == "sleep_spin" || field == "sleep_delay") {
                if (!std::isfinite(v)) return false;
                float& dst = field == "sleep_speed" ? b->sleep_speed : field == "sleep_spin" ? b->sleep_spin : b->sleep_delay;
                dst = std::clamp(v, 0.0f, 1000.0f);
                return true;
            }
            if (field.rfind("aflag:", 0) == 0) {
                for (const xylem::FlagOption& o : xylem::flag_options()) {
                    if (field.compare(6, std::string::npos, o.name) == 0) {
                        xylem::set_flag(*b, o.bit, on);
                        return true;
                    }
                }
                return false;
            }
            if (field.rfind("flag:", 0) == 0) {
                for (const orchard::BodyFlagInfo& info : orchard::body_flags()) {
                    if (field.compare(5, std::string::npos, info.name) == 0) {
                        b->set(info.flag, on);
                        return true;
                    }
                }
                return false;
            }
        }
        if (field.rfind("collider:", 0) == 0) {
            const std::size_t colon = field.find(':', 9);
            if (colon == std::string::npos) return false;
            const std::size_t index = static_cast<std::size_t>(std::atoi(field.substr(9, colon - 9).c_str()));
            const std::string what = field.substr(colon + 1);
            std::vector<orchard::Collider>* cs = m_scene.colliders(e);
            if (!cs || index >= cs->size()) return false;
            orchard::Collider& c = (*cs)[index];
            if (what == "shape") {
                const std::optional<orchard::ColliderShape> shape = orchard::parse_collider_shape(value);
                if (!shape) return false;
                c.shape = *shape;
            } else if (what == "sx") c.size.x = std::max(0.001f, v);
            else if (what == "sy") c.size.y = std::max(0.001f, v);
            else if (what == "sz") c.size.z = std::max(0.001f, v);
            else if (what == "ox") c.offset.x = v;
            else if (what == "oy") c.offset.y = v;
            else if (what == "oz") c.offset.z = v;
            else if (what == "enabled") c.enabled = on;
            else if (what == "sensor") c.sensor = on;
            else if (what == "density") c.density = std::max(0.0001f, v);
            else if (what == "friction") c.friction = std::max(0.0f, v);
            else if (what == "bounce") c.restitution = std::clamp(v, 0.0f, 1.0f);
            else if (what == "rounding") c.rounding = std::clamp(v, 0.0f, 0.5f * std::min({c.size.x, c.size.y, c.size.z}));
            else if (what == "roll") c.rolling_friction = std::clamp(v, 0.0f, 10.0f);
            else if (what == "spin") c.spinning_friction = std::clamp(v, 0.0f, 10.0f);
            else if (what == "gx") c.grip.x = std::clamp(v, 0.0f, 10.0f);
            else if (what == "gy") c.grip.y = std::clamp(v, 0.0f, 10.0f);
            else if (what == "gz") c.grip.z = std::clamp(v, 0.0f, 10.0f);
            else if (what == "pmin") c.polar_min = std::clamp(v, 0.0f, std::min(180.0f, c.polar_max));
            else if (what == "pmax") c.polar_max = std::clamp(v, c.polar_min, 180.0f);
            else if (what == "amin") c.azimuth_min = std::clamp(v, -360.0f, c.azimuth_max);
            else if (what == "amax") c.azimuth_max = std::clamp(v, c.azimuth_min, c.azimuth_min + 360.0f);
            else if (what == "shell") c.shell = std::clamp(v, 0.0f, 0.5f * std::max({c.size.x, c.size.y, c.size.z}));
            else if (what == "section") {
                c.shape = orchard::ColliderShape::sphere_section;
                c.shell = 0.0f;
                c.azimuth_min = 0.0f;
                c.azimuth_max = 360.0f;
                c.polar_min = 0.0f;
                c.polar_max = 90.0f;
                const float d = std::max({c.size.x, c.size.y, c.size.z});
                if (value == "dome") c.polar_max = 60.0f;
                else if (value == "bowl") {
                    c.polar_min = 90.0f;
                    c.polar_max = 180.0f;
                    c.shell = d * 0.06f;
                } else if (value == "shell") {
                    c.polar_max = 180.0f;
                    c.shell = d * 0.06f;
                } else if (value == "wedge") {
                    c.polar_max = 180.0f;
                    c.azimuth_max = 90.0f;
                } else if (value == "quarter") c.azimuth_max = 180.0f;
            }
            else if (what == "friction_mix" || what == "bounce_mix") {
                const std::optional<orchard::CombineMode> mode = orchard::parse_combine_mode(value);
                if (!mode) return false;
                (what == "friction_mix" ? c.friction_combine : c.bounce_combine) = *mode;
            }
            else return false;
            return true;
        }
        return false;
    }

    orchard::Entity add_preset(const std::string& kind, orchard::Entity parent) {
        const bool flat = m_scene.dimension() == orchard::Dimension::two_d;
        std::string name = "Entity";
        if (kind == "box") name = flat ? "Crate" : "Box";
        else if (kind == "ball") name = "Ball";
        else if (kind == "ground") name = "Ground";
        else if (kind == "trigger") name = "Trigger";
        else if (kind == "light") name = "Light";
        else if (kind == "water") name = "Water";
        else if (kind == "pool") name = "Pool";
        else if (kind == "hemisphere") name = "Dome";
        else if (kind == "sparks") name = "Sparks";
        else if (kind == "smoke") name = "Smoke";
        else if (kind == "fire") name = "Fire";
        const orchard::Entity e = m_scene.create(name, parent);
        if (flat) {
            const orchard::Vec2 c = m_view.pan_camera().center;
            m_scene.transform(e)->position = {c.x, c.y, 0.0f};
        } else {
            m_scene.transform(e)->position = m_view.orbit_camera().target;
        }
        if (kind == "box" || kind == "ball") {
            m_scene.add_body(e);
            m_scene.add_collider(e, kind == "ball" ? orchard::ColliderShape::sphere : orchard::ColliderShape::box);
            m_scene.transform(e)->position.y += 3.0f;
        } else if (kind == "ground") {
            m_scene.add_body(e).type = orchard::BodyType::fixed;
            orchard::Collider* c = m_scene.add_collider(e);
            c->size = {20.0f, 1.0f, flat ? 1.0f : 20.0f};
            m_scene.transform(e)->position.y = -0.5f;
        } else if (kind == "light") {
            m_scene.add_light(e);
            m_scene.transform(e)->position.y += 3.0f;
        } else if (kind == "trigger") {
            m_scene.add_body(e).type = orchard::BodyType::fixed;
            orchard::Collider* c = m_scene.add_collider(e);
            c->size = {3.0f, 2.0f, flat ? 1.0f : 3.0f};
            c->sensor = true;
        } else if (kind == "water") {
            orchard::Fluid& w = m_scene.add_fluid(e);
            w.size = {8.0f, 2.0f, flat ? 1.0f : 8.0f};
            m_scene.transform(e)->position.y = 1.0f;
        } else if (kind == "pool") {
            orchard::Fluid& w = m_scene.add_fluid(e);
            w.size = {6.0f, 2.0f, flat ? 1.0f : 6.0f};
            w.fill = 0.6f;
            m_scene.transform(e)->position.y = 1.0f;
            m_scene.add_body(e).type = orchard::BodyType::fixed;
            const float t = 0.2f;
            const orchard::Vec3 walls[5][2] = {{{6.0f + 2 * t, t, 6.0f + 2 * t}, {0.0f, -1.0f - t * 0.5f, 0.0f}}, {{t, 2.0f, 6.0f}, {3.0f + t * 0.5f, 0.0f, 0.0f}}, {{t, 2.0f, 6.0f}, {-3.0f - t * 0.5f, 0.0f, 0.0f}}, {{6.0f + 2 * t, 2.0f, t}, {0.0f, 0.0f, 3.0f + t * 0.5f}}, {{6.0f + 2 * t, 2.0f, t}, {0.0f, 0.0f, -3.0f - t * 0.5f}}};
            for (const auto& w2 : walls) {
                orchard::Collider* c = m_scene.add_collider(e);
                c->size = w2[0];
                c->offset = w2[1];
            }
        } else if (kind == "hemisphere") {
            m_scene.add_body(e);
            orchard::Collider* c = m_scene.add_collider(e, orchard::ColliderShape::sphere_section);
            c->size = {1.0f, 1.0f, 1.0f};
            m_scene.transform(e)->position.y += 3.0f;
        } else if (kind == "sparks" || kind == "smoke" || kind == "fire") {
            orchard::Emitter& pe = m_scene.add_emitter(e);
            if (const std::optional<orchard::Emitter> named = orchard::Emitter::named(kind)) pe = *named;
            m_scene.transform(e)->position.y += 0.5f;
        }
        return e;
    }

    void register_natives() {
        native("state", [this](const std::vector<std::string>&) { return state(); });

        native("new_project", [this](const std::vector<std::string>& f) {
            const std::optional<orchard::Dimension> d = orchard::parse_dimension(arg(f, 2));
            std::string error;
            if (!orchard::Project::create(arg(f, 0), arg(f, 1), d.value_or(orchard::Dimension::three_d), &error)) return "error\t" + error;
            if (!open(arg(f, 0), error)) return "error\t" + error;
            return std::string("ok");
        });

        native("open_project", [this](const std::vector<std::string>& f) {
            std::string error;
            if (!open(arg(f, 0), error)) return "error\t" + error;
            return std::string("ok");
        });

        native("save", [this](const std::vector<std::string>&) {
            if (!m_project) return std::string("error\tno project is open");
            if (m_session.playing() || m_session.building()) return std::string("error\tstop playing before saving");
            std::string error;
            if (!m_project->save(&error)) return "error\t" + error;
            if (!m_template_path.empty() && m_main_scene) {
                if (!m_scene.save_file(m_project->path_of(m_template_path))) return "error\tcan't write " + m_template_path;
                if (m_main_scene->refresh_instances(m_template_path, m_scene)) m_main_dirty = true;
                if (!m_main_scene->save_file(m_project->path_of(m_project->info().main_scene))) return "error\tcan't write " + m_project->info().main_scene;
                m_main_dirty = false;
                m_dirty = false;
                return std::string("ok");
            }
            if (!m_scene.save_file(m_project->path_of(m_project->info().main_scene))) return "error\tcan't write " + m_project->info().main_scene;
            m_dirty = false;
            return std::string("ok");
        });

        native("entities", [this](const std::vector<std::string>&) {
            std::string out;
            for (orchard::Entity e : m_scene.roots()) walk(e, 0, out);
            return out;
        });

        native("add_entity", [this](const std::vector<std::string>& f) {
            const orchard::Entity e = arg(f, 2).empty() ? m_scene.create(arg(f, 0).empty() ? "Entity" : arg(f, 0), entity(arg(f, 1))) : add_preset(arg(f, 2), entity(arg(f, 1)));
            if (arg(f, 2).empty()) {
                if (m_scene.dimension() == orchard::Dimension::three_d) {
                    m_scene.transform(e)->position = m_view.orbit_camera().target;
                } else {
                    const orchard::Vec2 c = m_view.pan_camera().center;
                    m_scene.transform(e)->position = {c.x, c.y, 0.0f};
                }
            }
            m_selected = e;
            touched();
            return std::to_string(e.id());
        });

        native("remove_entity", [this](const std::vector<std::string>& f) {
            const bool ok = m_scene.destroy(entity(arg(f, 0)));
            if (!m_scene.alive(m_selected)) m_selected = orchard::Entity{};
            if (ok) touched();
            return std::string(ok ? "ok" : "");
        });

        native("select", [this](const std::vector<std::string>& f) {
            m_selected = entity(arg(f, 0));
            redraw();
            return m_scene.alive(m_selected) ? std::to_string(m_selected.id()) : std::string();
        });

        native("rename", [this](const std::vector<std::string>& f) {
            const bool ok = m_scene.set_name(entity(arg(f, 0)), clean(arg(f, 1)));
            if (ok) touched();
            return std::string(ok ? "ok" : "");
        });

        native("transform", [this](const std::vector<std::string>& f) {
            const orchard::Transform* t = m_scene.transform(entity(arg(f, 0)));
            if (!t) return std::string();
            const orchard::Vec3 r = orchard::to_euler(t->rotation);
            const float v[9] = {t->position.x, t->position.y, t->position.z, r.x, r.y, r.z, t->scale.x, t->scale.y, t->scale.z};
            std::string out = clean(m_scene.name(entity(arg(f, 0))));
            for (float x : v) out += "\t" + number(x);
            return out;
        });

        native("set_transform", [this](const std::vector<std::string>& f) {
            const orchard::Entity e = entity(arg(f, 0));
            orchard::Transform* t = m_scene.transform(e);
            if (!t) return std::string();
            const std::string field = arg(f, 1);
            const float v = static_cast<float>(std::atof(arg(f, 2).c_str()));
            orchard::Vec3 r = orchard::to_euler(t->rotation);
            float* target = nullptr;
            if (field == "px") target = &t->position.x;
            else if (field == "py") target = &t->position.y;
            else if (field == "pz") target = &t->position.z;
            else if (field == "sx") target = &t->scale.x;
            else if (field == "sy") target = &t->scale.y;
            else if (field == "sz") target = &t->scale.z;
            if (target) {
                *target = v;
            } else if (field == "rx" || field == "ry" || field == "rz") {
                (field == "rx" ? r.x : field == "ry" ? r.y : r.z) = v;
                t->rotation = orchard::from_euler(r);
            } else {
                return std::string();
            }
            if (m_session.world()) m_session.world()->moved(e);
            touched();
            return std::string("ok");
        });

        native("components", [this](const std::vector<std::string>& f) { return components(entity(arg(f, 0))); });

        native("set_component", [this](const std::vector<std::string>& f) {
            if (!set_component(entity(arg(f, 0)), arg(f, 1), arg(f, 2))) return std::string();
            touched();
            return std::string("ok");
        });

        native("scripts", [this](const std::vector<std::string>&) {
            std::string out;
            if (!m_project) return out;
            for (const std::string& s : m_project->scripts()) out += s + "\n";
            for (const std::string& s : m_project->graphs()) out += s + "\n";
            return out;
        });

        native("new_script", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string("error\tno project is open");
            const orchard::Entity e = orchard::Entity::from_id(std::strtoull(arg(f, 0).c_str(), nullptr, 10));
            std::string ext = arg(f, 1);
            if (ext.empty() || ext[0] != '.') ext = "." + ext;
            std::string base;
            for (char c : m_scene.alive(e) ? m_scene.name(e) : std::string("Script")) {
                if (std::isalnum(static_cast<unsigned char>(c))) base += c;
            }
            if (base.empty() || std::isdigit(static_cast<unsigned char>(base[0]))) base = "Script" + base;
            if (ext == ".wal" || ext == ".lua" || ext == ".c" || ext == ".cpp") {
                for (char& c : base) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            std::string rel = "Scripts/" + base + ext;
            for (int i = 2; fs::exists(m_project->path_of(rel)); ++i) rel = "Scripts/" + base + std::to_string(i) + ext;
            std::string error;
            if (!m_session.scripts().create_script(*m_project, rel, m_project->info().dimension, &error)) return "error\t" + error;
            orchard::ScriptLanguage* lang = m_session.scripts().language_for(rel);
            std::string why;
            if (lang && !lang->available(script_toolchain(m_project->folder()), &why)) add_log(rel + ": " + why, true);
            return rel;
        });

        native("play", [this](const std::vector<std::string>&) {
            if (!m_project) return std::string("error\tno project is open");
            m_session.begin(*m_project, m_scene);
            set_animating(true);
            return mode();
        });

        native("stop", [this](const std::vector<std::string>&) {
            const orchard::Entity keep = m_selected;
            m_session.stop(m_scene);
            m_selected = m_scene.alive(keep) ? keep : orchard::Entity{};
            set_animating(wants_animation());
            redraw();
            return mode();
        });

        native("logs", [this](const std::vector<std::string>& f) {
            const std::uint64_t since = std::strtoull(arg(f, 0).c_str(), nullptr, 10);
            std::string out = std::to_string(m_log_seq) + "\n";
            for (const LogLine& l : m_logs) {
                if (l.seq > since) out += std::string(l.error ? "!" : "-") + "\t" + clean(l.text) + "\n";
            }
            return out;
        });

        native("key", [this](const std::vector<std::string>& f) {
            m_session.scripts().set_key(arg(f, 0), arg(f, 1) == "1");
            return std::string("ok");
        });

        native("viewport_resize", [this](const std::vector<std::string>& f) {
            const int w = std::atoi(arg(f, 0).c_str());
            const int h = std::atoi(arg(f, 1).c_str());
            if (w <= 0 || h <= 0) return std::string();
            m_view.set_view_size(static_cast<unsigned>(w), static_cast<unsigned>(h));
            redraw();
            return std::string("ok");
        });

        native("viewport_press", [this](const std::vector<std::string>& f) {
            float x, y;
            if (!to_canvas(arg(f, 0), arg(f, 1), x, y)) return std::string();
            m_drag = true;
            m_drag_button = std::atoi(arg(f, 2).c_str());
            const bool shift = arg(f, 3) == "1";
            m_drag_pan = arg(f, 2) != "1" || (shift && m_tool != "move") || m_tool == "pan";
            m_move_entity = orchard::Entity{};
            if (m_drag_button == 1 && m_tool == "move") begin_move(x, y, shift);
            m_last_x = m_start_x = x;
            m_last_y = m_start_y = y;
            return std::string("ok");
        });

        native("viewport_move", [this](const std::vector<std::string>& f) {
            float x, y;
            if (!m_drag || !to_canvas(arg(f, 0), arg(f, 1), x, y)) return std::string();
            if (m_move_entity.valid()) {
                continue_move(x, y);
                m_last_x = x;
                m_last_y = y;
                return std::string("ok");
            }
            if (m_drag_pan) m_view.pan(x - m_last_x, y - m_last_y);
            else m_view.orbit(x - m_last_x, y - m_last_y);
            moved_view();
            m_last_x = x;
            m_last_y = y;
            redraw();
            return std::string("ok");
        });

        native("viewport_release", [this](const std::vector<std::string>& f) {
            float x, y;
            const bool was = m_drag;
            m_drag = false;
            const orchard::Entity dragged = m_move_entity;
            const bool dragged_moved = m_move_moved;
            m_move_entity = orchard::Entity{};
            m_move_moved = false;
            if (dragged.valid()) {
                redraw();
                if (dragged_moved) {
                    touched();
                    return "moved\t" + std::to_string(dragged.id());
                }
                return "pick\t" + (m_scene.alive(dragged) ? std::to_string(dragged.id()) : std::string());
            }
            if (!was || !to_canvas(arg(f, 0), arg(f, 1), x, y)) return std::string();
            const float moved = std::fabs(x - m_start_x) + std::fabs(y - m_start_y);
            if (moved <= 4.0f && m_drag_button == 2) {
                const orchard::Entity hit = m_view.pick(m_scene, x, y);
                if (m_scene.alive(hit)) m_selected = hit;
                redraw();
                return "menu	" + (m_scene.alive(hit) ? std::to_string(hit.id()) : std::string());
            }
            if (moved > 4.0f || m_drag_pan || m_tool == "orbit") return std::string("drag");
            m_selected = m_view.pick(m_scene, x, y);
            redraw();
            return "pick\t" + (m_scene.alive(m_selected) ? std::to_string(m_selected.id()) : std::string());
        });

        native("viewport_zoom", [this](const std::vector<std::string>& f) {
            m_view.zoom(static_cast<float>(std::atof(arg(f, 0).c_str())));
            moved_view();
            redraw();
            return std::string("ok");
        });

        native("view", [this](const std::vector<std::string>& f) {
            const std::optional<orchard::Dimension> d = orchard::parse_dimension(arg(f, 0));
            if (d) m_view.set_dimension(*d);
            redraw();
            return std::string(orchard::to_string(m_view.dimension()));
        });

        native("axes", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string("error\topen a project first");
            const std::string v = arg(f, 0);
            if (m_session.playing() || m_session.building()) return std::string("error\tstop playing before changing the axes");
            if (m_project->info().dimension == orchard::Dimension::two_d) {
                const std::optional<orchard::YAxis2D> y = orchard::parse_y_axis(v);
                if (!y) return std::string("error\tunknown y axis '") + v + "'";
                m_project->info().y_axis = *y;
                m_view.set_y_axis(*y);
            } else {
                const std::optional<orchard::Axes> a = orchard::parse_axes(v);
                if (!a) return std::string("error\tunknown axes '") + v + "'";
                m_project->info().axes = *a;
                sync_view_axes();
            }
            std::string error;
            if (!m_project->save(&error)) return "error\t" + error;
            add_log("Axes: " + v + " (saved to project.orchard)", false);
            redraw();
            return std::string("ok");
        });

        native("combine_modes", [](const std::vector<std::string>&) {
            std::string out;
            for (orchard::CombineMode m : orchard::combine_modes()) out += std::string(orchard::to_string(m)) + "\t" + orchard::combine_label(m) + "\t" + orchard::combine_explanation(m) + "\n";
            return out;
        });

        native("physics_settings", [this](const std::vector<std::string>&) {
            if (!m_project) return std::string("error\topen a project first");
            return std::string(orchard::to_string(m_project->info().friction_combine)) + "\t" + orchard::to_string(m_project->info().bounce_combine);
        });

        native("set_physics_setting", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string("error\topen a project first");
            if (m_session.playing() || m_session.building()) return std::string("error\tstop playing first");
            const std::optional<orchard::CombineMode> mode = orchard::parse_combine_mode(arg(f, 1));
            if (!mode || *mode == orchard::CombineMode::project_default) return std::string("error\tunknown mix '") + arg(f, 1) + "'";
            if (arg(f, 0) == "friction") m_project->info().friction_combine = *mode;
            else if (arg(f, 0) == "bounce") m_project->info().bounce_combine = *mode;
            else return std::string("error\tunknown setting '") + arg(f, 0) + "'";
            std::string error;
            if (!m_project->save(&error)) return "error\t" + error;
            add_log(std::string(arg(f, 0) == "friction" ? "Friction" : "Bounce") + " mix: " + orchard::combine_label(*mode) + " (saved to project.orchard)", false);
            return std::string("ok");
        });

        native("physics_fields", [this](const std::vector<std::string>&) {
            if (!m_project) return std::string("error\topen a project first");
            const orchard::PhysicsSettings& ph = m_project->info().physics;
            const bool flat = m_project->info().dimension == orchard::Dimension::two_d;
            std::string out;
            auto num = [&out](const char* field, const char* label, double value, double lo, double hi, double step, const char* kind) {
                out += std::string("num\t") + field + "\t" + label + "\t" + number(static_cast<float>(value)) + "\t" + number(static_cast<float>(lo)) + "\t" + number(static_cast<float>(hi)) + "\t" + number(static_cast<float>(step)) + "\t" + kind + "\n";
            };
            auto yes = [&out](const char* field, const char* label, bool value) {
                out += std::string("bool\t") + field + "\t" + label + "\t" + (value ? "1" : "0") + "\n";
            };
            const int cores = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
            num("substeps", "Substeps", ph.substeps, 1, 16, 1, "int-slider");
            num("velocity_iterations", "Velocity iterations", ph.velocity_iterations, 1, 64, 1, "int-slider");
            num("position_iterations", "Position iterations", ph.position_iterations, 0, 32, 1, "int-slider");
            num("slop", "Allowed overlap (m)", ph.slop, 0.0, 0.05, 0.0005, "slider");
            num("correction", "Overlap correction", ph.correction, 0.0, 1.0, 0.01, "slider");
            num("world_size", "World size (m)", ph.world_size, 10.0, 100000.0, 10.0, "box");
            yes("continuous", "Fast-move checks", ph.continuous);
            yes("continuous_all", "Fast-move checks for everything", ph.continuous_all);
            yes("sleeping", "Sleeping", ph.sleeping);
            yes("warm_starting", "Warm starting", ph.warm_starting);
            yes("octree", "Use the old octree broadphase", ph.octree);
            if (!flat) {
                num("speculative_distance", "Speculative distance (m)", ph.speculative_distance, 0.0, 0.1, 0.001, "slider");
                yes("gyroscopic", "Gyroscopic spin", ph.gyroscopic);
                yes("soft_solver", "Soft-step solver", ph.soft_solver);
                num("soft_substeps", "Soft substeps", ph.soft_substeps, 1, 16, 1, "int-slider");
                num("contact_hertz", "Contact stiffness (Hz)", ph.contact_hertz, 1.0, 120.0, 1.0, "slider");
                num("contact_damping", "Contact damping", ph.contact_damping, 0.0, 20.0, 0.1, "slider");
                num("joint_hertz", "Joint stiffness (Hz)", ph.joint_hertz, 1.0, 240.0, 1.0, "slider");
                num("push_speed", "Max push-out speed (m/s)", ph.push_speed, 0.0, 20.0, 0.1, "slider");
                num("threads", "Threads", ph.threads, 1, std::max(cores, ph.threads), 1, "int-slider");
                yes("wide_solver", "Wide (SIMD) solver", ph.wide_solver);
                yes("deterministic", "Deterministic", ph.deterministic);
                yes("reuse_contacts", "Reuse resting contacts", ph.reuse_contacts);
                yes("spin_aware_ccd", "Spin-aware fast-move checks", ph.spin_aware_ccd);
            }
            std::string levels;
            for (orchard::FluidLevel lv : orchard::fluid_levels()) {
                if (lv == orchard::FluidLevel::project_default) continue;
                levels += (levels.empty() ? "" : "|") + std::string(orchard::to_string(lv)) + "=" + (flat && lv == orchard::FluidLevel::blocky ? std::string("Sand box (falling sand, lava, steam...)") : std::string(orchard::fluid_level_label(lv)));
            }
            out += std::string("pick\tfluid_level\tFluids (default for every fluid volume)\t") + orchard::to_string(ph.fluid_level) + "\t" + levels + "\n";
            out += std::string("pick\tfluid_quality\tFluid quality\t") + std::to_string(ph.fluid_quality) + "\t0=Low (fastest)|1=Medium|2=High|3=Ultra\n";
            num("particle_budget", "Most particles alive at once", ph.particle_budget, 1000, 1000000, 1000, "box");
            out += std::string("pick\tparticle_quality\tParticle quality (how far away they stay detailed)\t") + std::to_string(ph.particle_quality) + "\t0=Low (fastest)|1=Medium|2=High|3=Ultra\n";
            return out;
        });

        native("set_world_setting", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string("error\topen a project first");
            if (m_session.playing() || m_session.building()) return std::string("error\tstop playing first");
            orchard::PhysicsSettings ph = m_project->info().physics;
            const std::string key = arg(f, 0), value = arg(f, 1);
            char* end = nullptr;
            const double v = std::strtod(value.c_str(), &end);
            const bool numeric = end && *end == '\0' && !value.empty() && std::isfinite(v);
            const bool on = value == "1" || value == "true";
            auto whole = [&](int lo, int hi) { return std::clamp(static_cast<int>(std::lround(v)), lo, hi); };
            auto real = [&](double lo, double hi) { return static_cast<float>(std::clamp(v, lo, hi)); };
            if (key == "continuous") ph.continuous = on;
            else if (key == "continuous_all") ph.continuous_all = on;
            else if (key == "sleeping") ph.sleeping = on;
            else if (key == "warm_starting") ph.warm_starting = on;
            else if (key == "gyroscopic") ph.gyroscopic = on;
            else if (key == "octree") ph.octree = on;
            else if (key == "soft_solver") ph.soft_solver = on;
            else if (key == "wide_solver") ph.wide_solver = on;
            else if (key == "deterministic") ph.deterministic = on;
            else if (key == "reuse_contacts") ph.reuse_contacts = on;
            else if (key == "spin_aware_ccd") ph.spin_aware_ccd = on;
            else if (key == "fluid_level") {
                const std::optional<orchard::FluidLevel> lv = orchard::parse_fluid_level(value);
                if (!lv || *lv == orchard::FluidLevel::project_default) return std::string("error\tpick a fluid level");
                ph.fluid_level = *lv;
            }
            else if (!numeric) return std::string("error\t'") + value + "' is not a number";
            else if (key == "substeps") ph.substeps = whole(1, 16);
            else if (key == "velocity_iterations") ph.velocity_iterations = whole(1, 64);
            else if (key == "position_iterations") ph.position_iterations = whole(0, 32);
            else if (key == "slop") ph.slop = real(0.0, 0.05);
            else if (key == "correction") ph.correction = real(0.0, 1.0);
            else if (key == "world_size") ph.world_size = real(10.0, 100000.0);
            else if (key == "speculative_distance") ph.speculative_distance = real(0.0, 0.1);
            else if (key == "soft_substeps") ph.soft_substeps = whole(1, 16);
            else if (key == "contact_hertz") ph.contact_hertz = real(1.0, 120.0);
            else if (key == "contact_damping") ph.contact_damping = real(0.0, 20.0);
            else if (key == "joint_hertz") ph.joint_hertz = real(1.0, 240.0);
            else if (key == "push_speed") ph.push_speed = real(0.0, 20.0);
            else if (key == "threads") ph.threads = whole(1, 64);
            else if (key == "fluid_quality") ph.fluid_quality = whole(0, 3);
            else if (key == "particle_budget") ph.particle_budget = whole(100, 2000000);
            else if (key == "particle_quality") ph.particle_quality = whole(0, 3);
            else return std::string("error\tunknown setting '") + key + "'";
            ph.quality = orchard::PhysicsQuality::custom;
            m_project->info().physics = ph;
            std::string error;
            if (!m_project->save(&error)) return "error\t" + error;
            return std::string("ok");
        });

        native("axes_dialog", [this](const std::vector<std::string>&) {
            if (!m_project) return std::string("error\topen a project first");
            const orchard::ProjectInfo& info = m_project->info();
            m_axes_choice.dimension = info.dimension;
            m_axes_choice.system = info.axis_system();
            m_axes_choice.y_axis = info.y_axis;
            m_axes_choice.flip_x_2d = orchard::has_flip(info.axis_flips, orchard::AxisFlip::x);
            m_axes_preview.reset();
            redraw_axes_preview();
            return axes_choice_fields();
        });

        native("axes_pending", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string("error\topen a project first");
            const std::string style = arg(f, 0);
            const std::string flips = arg(f, 1);
            if (m_axes_choice.dimension == orchard::Dimension::two_d) {
                const std::optional<orchard::YAxis2D> y = orchard::parse_y_axis(style);
                if (!y) return std::string("error\tunknown y axis '") + style + "'";
                m_axes_choice.y_axis = *y;
                m_axes_choice.flip_x_2d = flips.find('x') != std::string::npos;
            } else {
                const std::optional<orchard::AxisSystem> base = orchard::AxisSystem::parse(style);
                if (!base) return std::string("error\tunknown axes '") + style + "'";
                orchard::AxisSystem s = *base;
                if (flips.find('x') != std::string::npos) s.flip_x();
                if (flips.find('y') != std::string::npos) s.flip_y();
                if (flips.find('z') != std::string::npos) s.flip_z();
                m_axes_choice.system = s;
            }
            redraw_axes_preview();
            return axes_choice_fields();
        });

        native("axes_apply", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string("error\topen a project first");
            if (m_session.playing() || m_session.building()) return std::string("error\tstop playing before changing the axes");
            const bool keep = arg(f, 0) == "1";
            orchard::ProjectInfo& info = m_project->info();
            orchard::AxisChange change;
            if (info.dimension == orchard::Dimension::two_d) {
                const bool was_x = orchard::has_flip(info.axis_flips, orchard::AxisFlip::x);
                change = orchard::AxisChange::flips_2d(was_x != m_axes_choice.flip_x_2d, info.y_axis != m_axes_choice.y_axis);
                info.y_axis = m_axes_choice.y_axis;
                info.axis_flips = m_axes_choice.flip_x_2d ? orchard::AxisFlip::x : orchard::AxisFlip::none;
            } else {
                change = orchard::AxisChange::between(info.axis_system(), m_axes_choice.system);
                info.set_axis_system(m_axes_choice.system);
            }
            std::string error;
            if (!m_project->save(&error)) return "error\t" + error;
            int converted = 0;
            if (keep && !change.identity()) {
                orchard::remap_scene(m_scene, change);
                m_dirty = true;
                ++converted;
                for (const std::string& rel : m_project->scenes()) {
                    if (rel == info.main_scene) continue;
                    std::optional<orchard::Scene> other = orchard::Scene::load_file(m_project->path_of(rel));
                    if (!other) continue;
                    orchard::remap_scene(*other, change);
                    if (other->save_file(m_project->path_of(rel))) ++converted;
                }
            }
            sync_view_axes();
            std::string what = info.dimension == orchard::Dimension::two_d ? std::string("y-") + orchard::to_string(info.y_axis) + (m_axes_choice.flip_x_2d ? " flip-x" : "") : info.axis_system().name();
            add_log("Axes: " + what + (converted ? " (converted " + std::to_string(converted) + (converted == 1 ? " scene" : " scenes") + " so nothing moves; save to keep the open one)" : "") + ", saved to project.orchard", false);
            redraw();
            return std::string("ok");
        });

        native("axes_preview_press", [this](const std::vector<std::string>& f) {
            m_axes_drag = true;
            m_axes_x = static_cast<float>(std::atof(arg(f, 0).c_str()));
            m_axes_y = static_cast<float>(std::atof(arg(f, 1).c_str()));
            return std::string("ok");
        });

        native("axes_preview_move", [this](const std::vector<std::string>& f) {
            if (!m_axes_drag) return std::string();
            const float x = static_cast<float>(std::atof(arg(f, 0).c_str()));
            const float y = static_cast<float>(std::atof(arg(f, 1).c_str()));
            m_axes_preview.orbit(x - m_axes_x, y - m_axes_y);
            m_axes_x = x;
            m_axes_y = y;
            redraw_axes_preview();
            return std::string("ok");
        });

        native("axes_preview_release", [this](const std::vector<std::string>&) {
            m_axes_drag = false;
            return std::string("ok");
        });

        native("axes_preview_reset", [this](const std::vector<std::string>&) {
            m_axes_preview.reset();
            redraw_axes_preview();
            return std::string("ok");
        });

        native("flip_axis", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string("error\topen a project first");
            if (m_session.playing() || m_session.building()) return std::string("error\tstop playing before changing the axes");
            const std::string v = arg(f, 0);
            orchard::ProjectInfo& info = m_project->info();
            const bool flat = info.dimension == orchard::Dimension::two_d;
            if (flat && v == "y") {
                info.y_axis = info.y_axis == orchard::YAxis2D::up ? orchard::YAxis2D::down : orchard::YAxis2D::up;
            } else if (v == "x" || v == "y" || (v == "z" && !flat)) {
                info.axis_flips = info.axis_flips ^ (v == "x" ? orchard::AxisFlip::x : v == "y" ? orchard::AxisFlip::y : orchard::AxisFlip::z);
            } else {
                return std::string("error\tcan't flip '") + v + "'";
            }
            sync_view_axes();
            std::string error;
            if (!m_project->save(&error)) return "error\t" + error;
            add_log("Axes: " + (flat ? std::string(orchard::to_string(info.y_axis)) + (orchard::has_flip(info.axis_flips, orchard::AxisFlip::x) ? " flip-x" : "") : info.axis_system().name()) + " (saved to project.orchard)", false);
            redraw();
            return flip_letters();
        });

        native("fly_key", [this](const std::vector<std::string>& f) {
            std::string k = arg(f, 0);
            if (k.size() == 1) k[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(k[0])));
            if (arg(f, 1) == "1") m_fly_keys.insert(k);
            else if (k == "*") m_fly_keys.clear();
            else m_fly_keys.erase(k);
            set_animating(!m_fly_keys.empty() || wants_animation());
            redraw();
            return std::string("ok");
        });

        native("set_tool", [this](const std::vector<std::string>& f) {
            const std::string t = arg(f, 0);
            if (t == "select" || t == "move" || t == "orbit" || t == "pan") m_tool = t;
            redraw();
            return m_tool;
        });

        native("reset_camera", [this](const std::vector<std::string>&) {
            moved_view();
            m_view.reset_camera();
            redraw();
            return std::string("ok");
        });

        native("templates", [this](const std::vector<std::string>&) { return templates_list(); });
        native("template_state", [this](const std::vector<std::string>&) {
            if (m_template_path.empty()) return std::string();
            return clean(m_template_path) + "\t" + clean(template_label(m_template_path)) + "\t" + (m_dirty ? "1" : "0");
        });
        native("new_template", [this](const std::vector<std::string>& f) { return new_template(arg(f, 0), arg(f, 1) == "1"); });
        native("edit_template", [this](const std::vector<std::string>& f) { return begin_template_edit(arg(f, 0)); });
        native("finish_template", [this](const std::vector<std::string>& f) { return end_template_edit(arg(f, 0) != "0"); });
        native("place_template", [this](const std::vector<std::string>& f) { return place_template(arg(f, 0)); });
        native("duplicate_template", [this](const std::vector<std::string>& f) { return duplicate_template(arg(f, 0)); });
        native("rename_template", [this](const std::vector<std::string>& f) { return rename_template(arg(f, 0), arg(f, 1)); });
        native("delete_template", [this](const std::vector<std::string>& f) { return delete_template(arg(f, 0)); });

        native("files", [this](const std::vector<std::string>&) {
            std::string out;
            if (!m_project) return out;
            for (const std::string& s : m_project->scenes()) out += "scene\t" + s + "\n";
            for (const std::string& s : m_project->scripts()) out += "script\t" + s + "\n";
            for (const std::string& s : m_project->graphs()) out += "graph\t" + s + "\n";
            for (const std::string& s : m_project->entity_templates()) out += "entity\t" + s + "\n";
            return out;
        });

        native("read_file", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string();
            const std::string rel = arg(f, 0);
            if (rel.find("..") != std::string::npos) return std::string();
            const std::string text = read_file(m_project->path_of(rel));
            if (fs::path(rel).extension() != ".graph") return text;
            std::string error;
            const std::optional<orchard::graph::Graph> g = orchard::graph::Graph::load(text, &error);
            if (!g) return "// " + error + "\n";
            std::vector<std::string> errors;
            std::string code = g->to_walnut(&errors);
            for (const std::string& e : errors) code = "// error: " + e + "\n" + code;
            return code;
        });

        native("outline", [this](const std::vector<std::string>& f) {
            const std::string group = arg(f, 0).empty() ? pref("outliner.group", "sections") : arg(f, 0);
            return outline(group, arg(f, 1));
        });

        native("outliner_prefs", [this](const std::vector<std::string>&) {
            return pref("outliner.side", "left") + "\t" + pref("outliner.group", "sections") + "\t" + pref("outliner.sort", "scene") + "\t" + pref("outliner.empty_sections", "0") + "\t" + pref("outliner.collapsed", "") + "\t" + pref("preview_effects", "0") + "\t" + pref("details.open", "1") + "\t" + pref("viewport.trace", "off") + "\t" + pref("viewport.cull", "1") + "\t" + pref("viewport.shadows", "1") + "\t" + pref("viewport.lights", "1") + "\t" + pref("viewport.materials", "1") + "\t" + pref("viewport.particles", "1");
        });

        native("set_pref", [this](const std::vector<std::string>& f) {
            const std::string key = arg(f, 0);
            if (key.empty() || key.find('=') != std::string::npos) return std::string();
            m_prefs[key] = clean(arg(f, 1));
            save_prefs();
            if (key == "preview_effects") m_preview_start = std::chrono::steady_clock::now();
            if (key == "viewport.trace" && arg(f, 1) == "off") m_gpu.stop_tracing();
            set_animating(wants_animation());
            redraw();
            return std::string("ok");
        });

        native("toggle_section", [this](const std::vector<std::string>& f) {
            const std::string key = arg(f, 0);
            std::string list = "," + pref("outliner.collapsed", "") + ",";
            const std::string token = "," + key + ",";
            const std::size_t at = list.find(token);
            if (at == std::string::npos) list += key + ",";
            else list.erase(at, token.size() - 1);
            std::string out;
            std::string cur;
            for (char c : list) {
                if (c == ',') {
                    if (!cur.empty()) out += (out.empty() ? "" : ",") + cur;
                    cur.clear();
                } else {
                    cur += c;
                }
            }
            m_prefs["outliner.collapsed"] = out;
            save_prefs();
            return out;
        });

        native("details", [this](const std::vector<std::string>& f) { return details(entity(arg(f, 0))); });

        native("inspect", [this](const std::vector<std::string>& f) { return inspect(entity(arg(f, 0))); });

        native("inspect_set", [this](const std::vector<std::string>& f) {
            const orchard::Entity e = entity(arg(f, 0));
            const std::string r = inspect_set(e, arg(f, 1), arg(f, 2));
            if (!r.empty()) {
                m_light_dirty = true;
                touched();
            }
            return r;
        });

        native("context_menu", [this](const std::vector<std::string>& f) {
            grove::Node* owner = m_doc.find_by_id(arg(f, 0));
            if (!owner) return std::string();
            const orchard::Entity e = entity(arg(f, 1));
            if (m_scene.alive(e)) {
                m_selected = e;
                redraw();
            }
            const bool ok = grove::open_menu(m_doc, *owner, menu_items(e), std::atof(arg(f, 2).c_str()), std::atof(arg(f, 3).c_str()));
            return ok ? (m_scene.alive(e) ? std::to_string(e.id()) : std::string("none")) : std::string();
        });

        native("trace_status", [this](const std::vector<std::string>&) {
            return pref("viewport.trace", "off") + "\t" + std::to_string(m_gpu.traced_samples()) + "\t" + clean(m_gpu.trace_error());
        });

        native("menu_action", [this](const std::vector<std::string>& f) { return menu_action(entity(arg(f, 0)), arg(f, 1)); });

        native("viewport_pick", [this](const std::vector<std::string>& f) {
            float x, y;
            if (!to_canvas(arg(f, 0), arg(f, 1), x, y)) return std::string();
            const orchard::Entity e = m_view.pick(m_scene, x, y);
            return m_scene.alive(e) ? std::to_string(e.id()) : std::string();
        });
    }

    struct LogLine {
        std::uint64_t seq = 0;
        std::string text;
        bool error = false;
    };

    grove::Document& m_doc;
    std::optional<orchard::Project> m_project;
    orchard::Scene m_scene;
    orchard::Entity m_selected;
    orchard::Viewport m_view;
    orchard::editor::GpuView m_gpu;
    orchard::editor::AxesPreview m_axes_preview;
    orchard::editor::AxesChoice m_axes_choice;
    bool m_axes_drag = false;
    float m_axes_x = 0.0f;
    float m_axes_y = 0.0f;
    std::deque<LogLine> m_logs;
    std::uint64_t m_log_seq = 0;
    Session m_session;
    bool m_dirty = false;
    bool m_drag = false;
    int m_drag_button = 1;
    bool m_drag_pan = false;
    std::map<std::string, std::string> m_prefs;
    chlorophyll::World m_light_world;
    bool m_light_built = false;
    bool m_light_dirty = true;
    std::chrono::steady_clock::time_point m_preview_start = std::chrono::steady_clock::now();
    float m_start_x = 0.0f;
    float m_start_y = 0.0f;
    float m_last_x = 0.0f;
    float m_last_y = 0.0f;
    std::set<std::string> m_fly_keys;
    std::vector<xylem::FluidDraw> m_fluid_draws;
    std::vector<orchard::ParticleCommand> m_particle_commands;
    bool m_particles_were_playing = false;
    std::chrono::steady_clock::time_point m_particle_burst_until{};
    std::string m_template_path;
    std::optional<orchard::Scene> m_main_scene;
    orchard::Entity m_main_selected;
    bool m_main_dirty = false;
    std::string m_tool = "select";
    orchard::Entity m_move_entity;
    int m_move_axis = -1;
    bool m_move_moved = false;
    orchard::Vec3 m_move_start;
    orchard::Vec3 m_move_grab;
    float m_move_press_x = 0.0f;
    float m_move_press_y = 0.0f;
    std::chrono::steady_clock::time_point m_view_moved = std::chrono::steady_clock::now();
};

int simulate(const std::string& path, double seconds) {
    std::string error;
    std::optional<orchard::Project> project = orchard::Project::open(path, &error);
    if (!project) {
        std::cerr << "orchard: " << error << "\n";
        return 1;
    }
    std::optional<orchard::Scene> scene = orchard::Scene::load_file(project->path_of(project->info().main_scene), &error);
    if (!scene) {
        std::cerr << "orchard: " << project->info().main_scene << ": " << error << "\n";
        return 1;
    }
    bool failed = false;
    orchard::ScriptRuntime scripts;
    scripts.set_log([&failed](const std::string& m, bool bad) {
        (bad ? std::cerr : std::cout) << m << "\n";
        failed = failed || bad;
    });
    scripts.set_toolchain(script_toolchain(project->folder()));
    xylem::World world(scene->dimension(), xylem::Settings::for_project(project->info()));
    world.build(*scene);
    scripts.set_physics(&world);
    for (const orchard::ScriptBuildResult& r : scripts.build_all(*project, *scene)) {
        if (!r.ok) {
            std::cerr << r.script << " didn't build:\n" << r.log << "\n";
            return 1;
        }
    }
    scripts.start(*project, *scene);
    const double dt = 1.0 / 60.0;
    for (int i = 0; i < static_cast<int>(seconds / dt + 0.5); ++i) {
        world.step(dt);
        scripts.tick(dt);
    }
    scripts.stop();
    std::cout << "after " << seconds << "s: " << world.body_count() << " bodies, " << world.contacts().size() << " contacts\n";
    for (orchard::Entity e : scene->entities()) {
        const orchard::Transform* t = scene->transform(e);
        const orchard::Vec3 r = orchard::to_euler(t->rotation);
        std::cout << "  " << scene->name(e) << " at " << number(t->position.x) << " " << number(t->position.y) << " " << number(t->position.z)
                  << ", turned " << number(r.x) << " " << number(r.y) << " " << number(r.z) << "\n";
    }
    return failed ? 1 : 0;
}

int bundle_ui(const std::string& output) {
    const std::string ui = find_ui();
    if (ui.empty()) {
        std::cerr << "orchard: can't find the UI (orchard.grove) to bundle; set ORCHARD_UI_DIR to the folder that holds it\n";
        return 1;
    }
    grove::AppBuildOptions options;
    options.output = output;
    options.stub = grove::executable_path();
    options.tools = grove::default_script_tools(ui);
    grove::Diagnostics diagnostics;
    const grove::AppBuildResult result = grove::build_app(ui, options, diagnostics, &std::cout);
    if (!result.ok) {
        diagnostics.print(std::cerr);
        std::cerr << "orchard: couldn't bundle the UI into " << output << "\n";
        return 1;
    }
    std::cout << "orchard: wrote " << result.output << " (" << result.files << " files, " << result.modules << " compiled scripts)\n";
    return 0;
}

int new_project(const std::string& folder, const std::string& name, orchard::Dimension d, const orchard::AxisSystem& axes) {
    std::string error;
    const std::optional<orchard::Project> p = orchard::Project::create(folder, name, d, axes, &error);
    if (!p) {
        std::cerr << "orchard: " << error << "\n";
        return 1;
    }
    std::cout << "orchard: made a " << orchard::to_string(d) << " project '" << p->info().name << "' in " << p->folder() << "\n";
    return 0;
}

int compile_graph(const std::string& path) {
    std::string error;
    const std::optional<orchard::graph::Graph> g = orchard::graph::Graph::load(read_file(path), &error);
    if (!g) {
        std::cerr << path << ": " << error << "\n";
        return 1;
    }
    std::vector<std::string> errors;
    const std::string code = g->to_walnut(&errors);
    for (const std::string& e : errors) std::cerr << path << ": " << e << "\n";
    std::cout << code;
    return errors.empty() ? 0 : 1;
}

const char* usage =
    "usage: orchard [PROJECT]\n"
    "       orchard --new FOLDER [--2d | --3d] [--name NAME] [--axes \"STYLE [flip-x] [flip-y] [flip-z]\"]\n"
    "       orchard --graph FILE.graph\n"
    "       orchard --simulate PROJECT [SECONDS]\n"
    "       orchard [--check] [--bundle OUTPUT] [--version]\n\n"
    "  PROJECT          a project folder or its project.orchard to open\n"
    "  --new FOLDER     make a new project (3d unless --2d) and exit\n"
    "  --axes AXES      which way is up in a new 3d project (default y-up; unity, unreal, blender and source work too), plus any of flip-x flip-y flip-z\n"
    "  --graph FILE     print the Walnut a script graph turns into\n"
    "  --simulate       run a project's physics and scripts without a window and print where everything ends up\n"
    "  --check          load the editor window and its script without opening it, then exit\n"
    "  --bundle OUTPUT  write a copy of orchard with the editor window and its compiled script built in\n\n"
    "Set ORCHARD_UI_DIR to load the editor UI from another folder.\n";

#if defined(__ANDROID__)
int android_error(fizmo::android::App& app, const std::string& message) {
    fizmo::android::log(fizmo::android::LogLevel::Error, "orchard", message);
    fizmo::graphics::Renderer renderer(app.surface());
    renderer.bind();
    fizmo::android::Event e;
    while (app.running()) {
        bool redraw = false;
        while (app.poll(e, 250)) {
            if (e.type == fizmo::android::EventType::Back) app.finish();
            redraw = true;
        }
        if (!redraw || !renderer.is_bound()) continue;
        const float k = app.density();
        const fizmo::android::Insets in = app.content_insets();
        const int x = in.left + static_cast<int>(24 * k);
        const int y = in.top + static_cast<int>(32 * k);
        const unsigned w = app.surface().width() > static_cast<unsigned>(x * 2) ? app.surface().width() - static_cast<unsigned>(x * 2) : app.surface().width();
        renderer.begin_frame();
        renderer.clear(fizmo::colors::Color(30, 30, 36));
        renderer.draw_text(x, y, w, static_cast<unsigned>(60 * k), std::string("Orchard couldn't start"), fizmo::text::TextStyle(22.0 * k, fizmo::colors::Color(243, 139, 168)));
        renderer.draw_text(x, y + static_cast<int>(48 * k), w, app.surface().height(), message, fizmo::text::TextStyle(15.0 * k, fizmo::colors::Color(205, 214, 244)));
        renderer.present();
    }
    return 1;
}

std::string android_home(fizmo::android::App& app) {
    const std::string data = app.internal_data_path().empty() ? std::string("/data/local/tmp") : app.internal_data_path();
    std::error_code ec;
    const fs::path cache = app.cache_path().empty() ? fs::path(data) / "cache" : fs::path(app.cache_path());
    fs::create_directories(fs::path(data) / "config", ec);
    fs::create_directories(cache, ec);
    setenv("HOME", data.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (fs::path(data) / "config").string().c_str(), 1);
    setenv("XDG_DATA_HOME", (fs::path(data) / "share").string().c_str(), 1);
    setenv("XDG_CACHE_HOME", cache.string().c_str(), 1);
    setenv("TMPDIR", cache.string().c_str(), 1);
    const fs::path projects = fs::path(data) / "Projects";
    fs::create_directories(projects, ec);
    fs::current_path(projects, ec);
    return data;
}
#endif

} // namespace

#if defined(__ANDROID__)
int fizmo_android_main(fizmo::android::App& app) {
    fizmo::android::redirect_stdio("orchard");
    const std::string data = android_home(app);

    const std::optional<std::string> bundle_text = app.assets().read("grove/app.bundle");
    if (!bundle_text) return android_error(app, "this APK has no grove/app.bundle asset; build it again with grove build");
    std::optional<grove::Bundle> bundle = grove::parse_bundle(*bundle_text);
    if (!bundle) return android_error(app, "grove/app.bundle is damaged; build Orchard again");

    grove::AppLoadOptions load;
    load.root = data;
    load.cache_dir = app.cache_path();
    load.module_dir = app.native_library_dir();

    grove::LoadedApp loaded;
    std::string error;
    if (!grove::load_app(*bundle, loaded, error, load)) return android_error(app, error);

    auto editor = std::make_unique<Editor>(loaded.project);
    grove::runtime::RunOptions run;
    run.window_id = loaded.window_id;
    run.scripts = true;
    return grove::runtime::run_android(loaded.project, app, &loaded.scripts, run);
}
#else
int main(int argc, char** argv) {
    bool check = false;
    std::string project_path;
    std::string new_folder;
    std::string new_name;
    orchard::Dimension new_dim = orchard::Dimension::three_d;
    orchard::AxisSystem new_axes;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--version") {
            std::cout << "orchard " << ORCHARD_VERSION_STRING << "\n";
            return 0;
        }
        if (a == "--help" || a == "-h") {
            std::cout << usage;
            return 0;
        }
        if (a == "--simulate") {
            if (i + 1 >= argc) {
                std::cerr << "orchard: --simulate needs a project\n";
                return 2;
            }
            const std::string where = argv[i + 1];
            const double seconds = i + 2 < argc ? std::atof(argv[i + 2]) : 3.0;
            return simulate(where, seconds > 0.0 ? seconds : 3.0);
        }
        if (a == "--bundle" || a == "--new" || a == "--name" || a == "--graph" || a == "--axes") {
            if (i + 1 >= argc) {
                std::cerr << "orchard: " << a << " needs a value\n";
                return 2;
            }
            const std::string v = argv[++i];
            if (a == "--bundle") return bundle_ui(v);
            if (a == "--graph") return compile_graph(v);
            if (a == "--new") {
                new_folder = v;
            } else if (a == "--axes") {
                const std::optional<orchard::AxisSystem> parsed = orchard::AxisSystem::parse(v);
                if (!parsed) {
                    std::cerr << "orchard: --axes must be y-up, y-up-left, z-up or z-up-left (or unity, unreal, blender, source), optionally followed by flip-x, flip-y or flip-z\n";
                    return 2;
                }
                new_axes = *parsed;
            } else {
                new_name = v;
            }
        } else if (a == "--2d") {
            new_dim = orchard::Dimension::two_d;
        } else if (a == "--3d") {
            new_dim = orchard::Dimension::three_d;
        } else if (a == "--check") {
            check = true;
        } else if (!a.empty() && a[0] == '-') {
            std::cerr << "orchard: unknown option " << a << "\n" << usage;
            return 2;
        } else {
            project_path = a;
        }
    }
    if (!new_folder.empty()) return new_project(new_folder, new_name, new_dim, new_axes);

    grove::LoadedApp loaded;
    grove::Project from_files;
    grove::Project* ui = nullptr;
    grove::runtime::RunOptions options;
    std::optional<grove::Bundle> bundle;
    if (env("ORCHARD_UI_DIR").empty()) bundle = grove::read_embedded_bundle(grove::executable_path());
    if (bundle) {
        std::string error;
        if (!grove::load_app(*bundle, loaded, error)) {
            std::cerr << "orchard: " << error << "\n";
            return 1;
        }
        ui = &loaded.project;
        options.window_id = loaded.window_id;
        options.prebuilt = &loaded.scripts;
        if (check) {
            std::cout << "orchard " << ORCHARD_VERSION_STRING << ": built-in window ok, " << loaded.scripts.runnable_count() << " compiled script(s)\n";
            return loaded.scripts.ok() ? 0 : 1;
        }
    } else {
        const std::string path = find_ui();
        if (path.empty()) {
            std::cerr << "orchard: can't find the editor UI (orchard.grove); set ORCHARD_UI_DIR to the folder that holds it\n";
            return 1;
        }
        from_files = grove::load_project(path);
        if (!from_files.diagnostics.ok()) {
            from_files.diagnostics.print(std::cerr);
            return 1;
        }
        if (check) {
            std::cout << "orchard " << ORCHARD_VERSION_STRING << ": window ok (" << path << ")\n";
            return 0;
        }
        ui = &from_files;
        options.tools = grove::default_script_tools(path);
    }

    auto editor = std::make_unique<Editor>(*ui);
    if (!project_path.empty()) {
        std::string error;
        if (!editor->open(project_path, error)) std::cerr << "orchard: " << error << "\n";
    }
    return grove::runtime::run(*ui, options);
}
#endif // __ANDROID__
