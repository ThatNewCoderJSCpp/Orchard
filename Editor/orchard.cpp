#include <orchard/orchard.hpp>
#include <orchard/scripting.hpp>
#include <orchard/effects.hpp>
#include <orchard/solid.hpp>
#include <orchard/units.hpp>
#include <orchard/hud.hpp>
#include <xylem/xylem.hpp>
#include <chlorophyll/chlorophyll.hpp>

#include "axes_preview.hpp"
#include "gpu_view.hpp"
#include "graph_view.hpp"
#include "builtin_models.hpp"
#include "particle_textures.hpp"
#include "sound_player.hpp"

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
        {"models", "Models", "◈", "Model"},
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

std::string shell_quote(const std::string& s) {
#ifdef _WIN32
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else out += c;
    }
    return out + "\"";
#else
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
#endif
}

bool launch_detached(const std::string& command) {
    if (command.empty()) return false;
#ifdef _WIN32
    return std::system(("start \"\" " + command).c_str()) == 0;
#else
    return std::system(("(" + command + ") >/dev/null 2>&1 &").c_str()) == 0;
#endif
}

std::string default_editor_command() {
    if (const char* v = std::getenv("VISUAL"); v && *v) {
        const std::string s = v;
        if (s.find("vi") == std::string::npos && s.find("nano") == std::string::npos && s.find("emacs -nw") == std::string::npos) return s;
    }
#ifdef _WIN32
    return "notepad";
#elif defined(__APPLE__)
    return "open -t";
#else
    return "xdg-open";
#endif
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
        m_scripts.set_key_resolver([](std::string_view name) { return static_cast<int>(fizmo::input::key_from_name(name.data(), name.size())); });
    }

    ~Session() {
        if (m_build.valid()) m_build.wait();
        stop_now();
    }

    bool playing() const { return m_playing; }
    bool building() const { return m_build.valid(); }
    orchard::ScriptRuntime& scripts() { return m_scripts; }
    xylem::World* world() { return m_world.get(); }
    void set_terrain_source(xylem::TerrainSource source) { m_terrain_source = std::move(source); }
    void set_play_dimension(std::optional<orchard::Dimension> d) { m_play_dimension = d; }
    orchard::Dimension play_dimension() const { return m_world ? m_world->dimension() : (m_play_dimension ? *m_play_dimension : (m_scene ? m_scene->dimension() : orchard::Dimension::three_d)); }

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
        const orchard::Dimension dim = m_play_dimension ? *m_play_dimension : m_scene->dimension();
        xylem::Settings settings = xylem::Settings::for_project(m_project->info());
        if (dim == orchard::Dimension::two_d && m_scene->dimension() == orchard::Dimension::three_d) settings = xylem::Settings::flattened(settings);
        m_world = std::make_unique<xylem::World>(dim, settings);
        m_world->set_asset_root(m_project->folder());
        if (m_terrain_source) m_world->set_terrain_source(m_terrain_source);
        m_world->build(*m_scene);
        for (const std::string& w : m_world->warnings()) m_log(w, true);
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
    xylem::TerrainSource m_terrain_source;
    std::optional<orchard::Dimension> m_play_dimension;
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
        if (grove::Node* canvas = m_doc.find_by_id("graph-canvas")) {
            grove::set_canvas_draw(m_doc, *canvas, [this](grove::GpuCanvasFrame& f) {
                orchard::editor::GraphView* g = active_graph();
                (g ? *g : m_no_graph).draw(f);
            });
        }
        m_session.set_terrain_source(m_gpu.terrain().source());
        orchard::set_height_image_decoder([](const std::string& path, orchard::HeightImage& out, std::string* error) {
            fizmo::images::BitmapImage img;
            if (!fizmo::images::load_image(path, img, error)) return false;
            out.width = static_cast<int>(img.width());
            out.height = static_cast<int>(img.height());
            out.bits = 8;
            out.values.resize(img.pixels().size());
            for (std::size_t i = 0; i < img.pixels().size(); ++i) {
                const auto& c = img.pixels()[i];
                out.values[i] = (c.red() + c.green() + c.blue()) / (3.0f * 255.0f);
            }
            return true;
        });
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
        ensure_character_models();
        m_graphs.clear();
        m_graph_open.clear();
        m_graph_close_warned.clear();
        redraw_graph();
        m_view.set_dimension(m_project->info().dimension);
        sync_view_axes();
        m_view.reset_camera();
        load_prefs();
        m_trace_poser.clear();
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
        pump_sounds();
        double seconds = 0.0;
        if (m_session.playing()) seconds = m_session.time();
        else if (pref("preview_effects", "0") == "1") seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_preview_start).count();
        const std::string trace = pref("viewport.trace", "off");
        if (trace != "off" && m_project && m_view.dimension() == orchard::Dimension::three_d) {
            chlorophyll::World& lw = light_world();
            const fizmo::lighting::LightingSettings s = chlorophyll::settings_for(m_project->info(), trace == "photon" ? orchard::LightingLevel::photon : orchard::LightingLevel::path_traced);
            fizmo::lighting::LightingSettings preview = s;
            if (preview.firefly_clamp <= 0.0f) preview.firefly_clamp = 8.0f;
            preview.denoise = true;
            preview.denoise_passes = trace == "rt" ? 5 : 4;
            if (trace == "rt") {
                preview.max_bounces = std::min(preview.max_bounces, 4);
                preview.russian_roulette_depth = std::min(preview.russian_roulette_depth, 2);
                preview.caustics = false;
            }
            if (f.dt > 0.0 && f.dt < 1.0 / 75.0 && m_trace_spp < (trace == "rt" ? 4 : 8)) ++m_trace_spp;
            else if (f.dt > 1.0 / 40.0 && m_trace_spp > 1) --m_trace_spp;
            m_gpu.set_trace_samples(m_trace_spp);
            if (m_session.playing() || seconds > 0.0) lw.animate(m_scene, seconds);
            else lw.refresh_geometry(m_scene);
            if (m_gpu.draw_traced_over(f, lw.lighting_scene(), preview, lw.camera_for(m_view, f.viewport_width, f.viewport_height), 0u)) {
                m_overlay_only = true;
                draw_editor_view(f, seconds);
                m_overlay_only = false;
                return;
            }
            draw_editor_view(f, seconds);
            return;
        }
        draw_editor_view(f, seconds);
    }

    void draw_editor_view(grove::GpuCanvasFrame& f, double seconds) {
        orchard::editor::GpuViewOptions view_options;
        view_options.culling = pref("viewport.cull", "1") == "1";
        view_options.occlusion = view_options.culling;
        view_options.shadows = pref("viewport.shadows", "1") == "1";
        {
            const std::string q = pref("viewport.quality", "medium");
            view_options.lighting_quality = q == "low" ? 0 : q == "high" ? 2 : q == "ultra" ? 3 : 1;
        }
        view_options.overlay_only = m_overlay_only;
        view_options.move_gizmo = m_tool == "move" && !m_session.playing();
        view_options.rotate_gizmo = m_tool == "rotate" && !m_session.playing();
        view_options.scale_gizmo = m_tool == "scale" && !m_session.playing();
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
            m_player_ready = false;
            if (m_session.playing()) {
                m_view_before_play = m_view;
                m_fly_keys.clear();
            } else if (m_view_before_play) {
                const unsigned w = m_view.width(), h = m_view.height();
                m_view = *m_view_before_play;
                m_view.resize(w, h);
                m_view_before_play.reset();
            }
        }
        if (m_view.dimension() == orchard::Dimension::three_d) step_animation(f.dt);
        if (++m_model_check % 60 == 0 && m_gpu.models().reload_changed()) {
            m_trace_poser.clear();
            m_light_dirty = true;
        }
        if (m_session.playing() && pref("play.use_camera", "1") == "1") drive_player_camera(static_cast<float>(f.dt));
        if (m_session.playing()) play_tick(static_cast<float>(f.dt));
        else if (m_inv_open || m_bags_open || !m_play_down.empty()) {
            close_screen();
            m_play_down.clear();
            m_use_down = false;
        }
        if (m_session.playing() || ++m_item_model_check % 15 == 0) ensure_item_models();
        smooth_bodies_for_drawing();
        if (m_session.playing() && pref("play.use_camera", "1") == "1") follow_game_camera();
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
        m_gpu.set_also_selected(m_also);
        m_gpu.draw(f, m_scene, m_selected, m_view, m_session.playing(), seconds);
        if (m_session.playing()) draw_play_hud(f);
        unsmooth_bodies();
        if (m_gpu.terrain().stats().pending > 0) redraw();
    }

    std::string group_particle_look(orchard::Entity e) const {
        std::string seen;
        for (orchard::Entity c : m_scene.subtree(e)) {
            const orchard::Emitter* ce = m_scene.emitter(c);
            if (!ce) continue;
            const std::string look = orchard::editor::particle_look_of(*ce);
            if (ce->flat || look == "custom") continue;
            const orchard::editor::ParticleLook pl = orchard::editor::particle_look_for(ce->texture, ce->preset);
            if (pl.simple == pl.better && !pl.better_covers && look == "better") continue;
            if (seen.empty()) seen = look;
            else if (seen != look) return "mixed";
        }
        return seen.empty() ? "mixed" : seen;
    }

    void pump_sounds() {
        std::vector<orchard::SoundCommand> commands = m_session.scripts().take_sound_commands();
        if (m_project) m_sounds.set_folder(m_project->folder());
        if (!commands.empty()) {
            const std::string before = m_sounds.error();
            m_sounds.apply(commands, m_scene);
            if (m_sounds.error() != before && !m_sounds.error().empty()) add_log("Sound: " + m_sounds.error(), true);
        }
        if (!m_session.playing()) {
            if (m_sounds.playing() > 0) m_sounds.stop_all();
            return;
        }
        const orchard::Vec3 eye = m_view.eye();
        const orchard::Vec3 fwd = camera_forward_axis();
        const orchard::Vec3 up = camera_up_axis();
        orchard::Vec3 right = orchard::cross(fwd, up);
        if (m_view.dimension() != orchard::Dimension::three_d) right = orchard::Vec3{1.0f, 0.0f, 0.0f};
        std::vector<int> finished;
        m_sounds.update(m_scene, eye, right, m_view.dimension() != orchard::Dimension::three_d, finished);
        for (int id : finished) m_session.scripts().sound_finished(id);
    }

    void smooth_bodies_for_drawing() {
        m_smoothed.clear();
        xylem::World* world = m_session.playing() ? m_session.world() : nullptr;
        if (!world || pref("play.smooth", "1") != "1") return;
        std::vector<orchard::Entity> todo = m_scene.roots();
        while (!todo.empty()) {
            const orchard::Entity e = todo.back();
            todo.pop_back();
            orchard::Vec3 pos;
            orchard::Quat rot;
            orchard::Transform* t = m_scene.transform(e);
            if (t && world->has_body(e) && world->render_pose(e, pos, rot)) {
                m_smoothed.push_back({e, *t});
                const orchard::Entity parent = m_scene.parent(e);
                if (m_scene.alive(parent)) {
                    const orchard::Vec3 pp = xylem::world_position(m_scene, parent);
                    const orchard::Quat pr = xylem::world_rotation(m_scene, parent);
                    const orchard::Vec3 ps = xylem::world_scale(m_scene, parent);
                    const orchard::Quat pinv{-pr.x, -pr.y, -pr.z, pr.w};
                    const orchard::Vec3 local = orchard::rotate(pinv, pos - pp);
                    t->position = {ps.x != 0.0f ? local.x / ps.x : local.x, ps.y != 0.0f ? local.y / ps.y : local.y, ps.z != 0.0f ? local.z / ps.z : local.z};
                    t->rotation = orchard::normalize(pinv * rot);
                } else {
                    t->position = pos;
                    t->rotation = rot;
                }
                continue;
            }
            for (orchard::Entity c : m_scene.children(e)) todo.push_back(c);
        }
    }

    void unsmooth_bodies() {
        for (const auto& [e, t] : m_smoothed)
            if (orchard::Transform* now = m_scene.transform(e)) *now = t;
        m_smoothed.clear();
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

    int ring_axis_at(float x, float y) const {
        if (!m_scene.alive(m_selected) || m_view.dimension() == orchard::Dimension::two_d) return -1;
        const orchard::Vec3 at = xylem::world_position(m_scene, m_selected);
        const float len = gizmo_length(at);
        int best = -1;
        float best_d = 9.0f;
        for (int i = 0; i < 3; ++i) {
            orchard::Vec3 a{}, b{};
            (i == 0 ? a.y : i == 1 ? a.z : a.x) = 1.0f;
            (i == 0 ? b.z : i == 1 ? b.x : b.y) = 1.0f;
            float px = 0.0f, py = 0.0f;
            bool have = false;
            for (int k = 0; k <= 48; ++k) {
                const float t = static_cast<float>(k) / 48.0f * 6.2831853f;
                float qx, qy;
                if (!m_view.project(at + (a * std::cos(t) + b * std::sin(t)) * len, qx, qy)) {
                    have = false;
                    continue;
                }
                if (have) {
                    const float vx = qx - px, vy = qy - py, l2 = vx * vx + vy * vy;
                    const float u = l2 > 1e-4f ? std::clamp(((x - px) * vx + (y - py) * vy) / l2, 0.0f, 1.0f) : 0.0f;
                    const float d = std::hypot(x - (px + vx * u), y - (py + vy * u));
                    if (d < best_d) {
                        best_d = d;
                        best = i;
                    }
                }
                px = qx;
                py = qy;
                have = true;
            }
        }
        return best;
    }

    static orchard::Quat axis_angle(orchard::Vec3 axis, float radians) {
        const orchard::Vec3 n = orchard::normalize(axis);
        const float h = radians * 0.5f;
        const float s = std::sin(h);
        return orchard::Quat{n.x * s, n.y * s, n.z * s, std::cos(h)};
    }

    bool begin_gizmo(float x, float y) {
        if (m_session.playing() || m_session.building() || m_view.dimension() == orchard::Dimension::two_d) return false;
        int axis = -1;
        orchard::Entity hit;
        if (m_scene.alive(m_selected)) {
            const orchard::Vec3 at = xylem::world_position(m_scene, m_selected);
            float x0, y0;
            const bool center = m_view.project(at, x0, y0) && std::hypot(x - x0, y - y0) < 16.0f;
            if (m_tool == "rotate") axis = ring_axis_at(x, y);
            else axis = center ? 3 : gizmo_axis_at(x, y);
            if (axis >= 0) hit = m_selected;
        }
        if (!m_scene.alive(hit)) {
            hit = pick_entity(x, y);
            if (!m_scene.alive(hit)) return false;
            m_selected = hit;
            redraw();
            return false;
        }
        const orchard::Transform* t = m_scene.transform(hit);
        if (!t) return false;
        m_move_entity = hit;
        m_move_axis = axis;
        m_gizmo_mode = m_tool;
        m_move_start = xylem::world_position(m_scene, hit);
        m_gizmo_start_rotation = t->rotation;
        m_gizmo_start_scale = t->scale;
        m_move_moved = false;
        m_move_press_x = x;
        m_move_press_y = y;
        redraw();
        return true;
    }

    void continue_gizmo(float x, float y) {
        orchard::Transform* t = m_scene.transform(m_move_entity);
        if (!t) return;
        if (m_gizmo_mode == "rotate") {
            float cx, cy;
            if (!m_view.project(m_move_start, cx, cy)) return;
            const float a0 = std::atan2(m_move_press_y - cy, m_move_press_x - cx);
            const float a1 = std::atan2(y - cy, x - cx);
            float delta = a1 - a0;
            orchard::Vec3 dir{};
            (m_move_axis == 0 ? dir.x : m_move_axis == 1 ? dir.y : dir.z) = 1.0f;
            if (orchard::dot(dir, m_view.view_direction()) < 0.0f) delta = -delta;
            if (m_snap_rotation) delta = orchard::radians(std::round(orchard::degrees(delta) / 15.0f) * 15.0f);
            orchard::Quat parent_rot{0.0f, 0.0f, 0.0f, 1.0f};
            const orchard::Entity parent = m_scene.parent(m_move_entity);
            if (m_scene.alive(parent)) parent_rot = xylem::world_rotation(m_scene, parent);
            const orchard::Quat parent_inv{-parent_rot.x, -parent_rot.y, -parent_rot.z, parent_rot.w};
            t->rotation = orchard::normalize(parent_inv * axis_angle(dir, -delta) * parent_rot * m_gizmo_start_rotation);
        } else {
            float factor = 1.0f;
            if (m_move_axis == 3) {
                factor = 1.0f + ((x - m_move_press_x) - (y - m_move_press_y)) / 120.0f;
            } else {
                const orchard::Quat rot = xylem::world_rotation(m_scene, m_move_entity);
                orchard::Vec3 local{};
                (m_move_axis == 0 ? local.x : m_move_axis == 1 ? local.y : local.z) = 1.0f;
                const orchard::Vec3 dir = orchard::rotate(rot, local);
                const float len = gizmo_length(m_move_start);
                float x0, y0, x1, y1;
                if (!m_view.project(m_move_start, x0, y0) || !m_view.project(m_move_start + dir * len, x1, y1)) return;
                const float vx = x1 - x0, vy = y1 - y0, l2 = vx * vx + vy * vy;
                if (l2 < 1.0f) return;
                factor = 1.0f + ((x - m_move_press_x) * vx + (y - m_move_press_y) * vy) / l2;
            }
            factor = std::max(0.01f, factor);
            orchard::Vec3 s = m_gizmo_start_scale;
            if (m_move_axis == 3) s = s * factor;
            else if (m_move_axis == 0) s.x *= factor;
            else if (m_move_axis == 1) s.y *= factor;
            else s.z *= factor;
            t->scale = s;
        }
        m_move_moved = true;
        m_light_dirty = true;
        m_dirty = true;
        redraw();
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
        orchard::Entity hit = axis >= 0 ? m_selected : pick_entity(x, y);
        if (!m_scene.alive(hit)) return false;
        if (axis < 0 && vertical) axis = 3;
        m_selected = hit;
        m_move_entity = hit;
        m_move_axis = axis;
        m_gizmo_mode = "move";
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
        if (m_gizmo_mode == "rotate" || m_gizmo_mode == "scale") {
            continue_gizmo(x, y);
            return;
        }
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

    orchard::editor::GraphView* active_graph() {
        const auto it = m_graphs.find(m_graph_open);
        return it == m_graphs.end() ? nullptr : &it->second;
    }

    void redraw_graph() {
        if (grove::Node* canvas = m_doc.find_by_id("graph-canvas")) grove::redraw_canvas(m_doc, *canvas);
    }

    bool graph_point(const std::string& xs, const std::string& ys, float& x, float& y) {
        const grove::Node* canvas = m_doc.find_by_id("graph-canvas");
        orchard::editor::GraphView* g = active_graph();
        if (!canvas || !g) return false;
        const grove::CanvasRect r = grove::canvas_rect(m_doc, *canvas);
        if (r.width <= 0.0 || r.height <= 0.0) return false;
        const double sx = g->width() > 0.0f ? g->width() / r.width : 1.0;
        const double sy = g->height() > 0.0f ? g->height() / r.height : 1.0;
        x = static_cast<float>((std::atof(xs.c_str()) - r.x) * sx);
        y = static_cast<float>((std::atof(ys.c_str()) - r.y) * sy);
        return true;
    }

    std::string graph_status_text() {
        orchard::editor::GraphView* g = active_graph();
        if (!g) return std::string();
        const orchard::editor::GraphCompile c = g->compile();
        const std::string problems = c.errors.empty() ? std::string(g->is_brain() ? "Compiles to a behavior tree" : "Compiles to Walnut") : std::to_string(c.errors.size()) + (c.errors.size() == 1 ? " problem: " : " problems: ") + clean(c.errors.front());
        return std::string(g->dirty() ? "1" : "0") + "\t" + problems + "\t" + (g->can_undo() ? "1" : "0") + "\t" + (g->can_redo() ? "1" : "0");
    }

    orchard::graph::GraphKind open_graph_kind() const {
        const auto it = m_graphs.find(m_graph_open);
        return it == m_graphs.end() ? orchard::graph::GraphKind::script : it->second.graph().kind();
    }

    std::vector<grove::MenuItem> graph_add_items() const {
        std::vector<grove::MenuItem> items;
        for (const orchard::editor::GraphCategory& cat : orchard::editor::GraphView::catalog(open_graph_kind())) {
            if (!items.empty()) items.push_back(grove::menu_separator());
            items.push_back(grove::menu_item("", cat.name, "", true));
            for (const orchard::editor::GraphCatalogEntry& t : cat.types) items.push_back(grove::menu_item("add:" + t.id, "  " + t.title));
        }
        return items;
    }

    std::vector<grove::MenuItem> graph_menu_items(const orchard::editor::GraphHit& hit) const {
        using orchard::editor::GraphHitKind;
        std::vector<grove::MenuItem> items;
        if (hit.kind == GraphHitKind::link) {
            items.push_back(grove::menu_item("unlink", "Remove this link"));
            return items;
        }
        if (hit.node) {
            if (hit.kind == GraphHitKind::pin && !hit.output) items.push_back(grove::menu_item("unpin:" + hit.pin, "Disconnect " + hit.pin));
            items.push_back(grove::menu_item("duplicate", "Duplicate", "Ctrl+D"));
            items.push_back(grove::menu_item("delete", "Delete", "Del"));
            items.push_back(grove::menu_separator());
            items.push_back(grove::menu_item("frame_selected", "Frame selected"));
            return items;
        }
        items = graph_add_items();
        items.push_back(grove::menu_separator());
        items.push_back(grove::menu_item("frame", "Frame all", "F"));
        return items;
    }

    std::string graph_event_text(const orchard::editor::GraphEvent& ev) {
        using orchard::editor::GraphEventKind;
        switch (ev.kind) {
        case GraphEventKind::edit:
            m_graph_edit_node = ev.edit.node;
            m_graph_edit_pin = ev.edit.pin;
            return "edit\t" + std::to_string(ev.edit.node) + "\t" + ev.edit.pin + "\t" + orchard::graph::to_string(ev.edit.kind) + "\t" + clean(ev.edit.value);
        case GraphEventKind::error:
            return "error\t" + clean(ev.message);
        default:
            break;
        }
        return orchard::editor::to_string(ev.kind);
    }

    orchard::Entity pick_entity(float x, float y) {
        const orchard::Entity by_shape = m_view.pick(m_scene, x, y);
        if (m_view.dimension() != orchard::Dimension::three_d) return by_shape;
        orchard::Vec3 origin, dir;
        if (!m_view.ray(x, y, origin, dir)) return by_shape;
        orchard::Entity by_model;
        float model_t = 0.0f;
        if (!m_gpu.models().pick(m_scene, origin, dir, by_model, model_t)) return by_shape;
        if (!m_scene.alive(by_shape)) return by_model;
        if (m_scene.colliders(by_shape) == nullptr || m_scene.colliders(by_shape)->empty()) return by_shape;
        const orchard::Vec3 at = xylem::world_position(m_scene, by_shape);
        const float shape_t = orchard::dot(at - origin, dir);
        for (orchard::Entity up = by_shape; m_scene.alive(up); up = m_scene.parent(up))
            if (up == by_model) return by_shape;
        return shape_t < model_t ? by_shape : by_model;
    }

    void fit_model_collider(orchard::Entity e, const std::string& how) {
        const orchard::Model* md = m_scene.model(e);
        if (!md) return;
        orchard::Vec3 lo, hi;
        if (!m_gpu.models().local_bounds(md->source, lo, hi)) return;
        std::vector<orchard::Collider>* cs = m_scene.colliders(e);
        if (cs) cs->erase(std::remove_if(cs->begin(), cs->end(), [&md](const orchard::Collider& c) { return c.source == md->source || c.source.empty(); }), cs->end());
        if (how == "none") return;
        orchard::Collider& c = *m_scene.add_collider(e, how == "mesh" ? orchard::ColliderShape::mesh : how == "hull" ? orchard::ColliderShape::convex_hull : how == "capsule" ? orchard::ColliderShape::capsule : orchard::ColliderShape::box);
        if (how == "mesh" || how == "hull") {
            c.source = md->source;
            c.size = {1.0f, 1.0f, 1.0f};
            c.offset = {};
            if (how == "mesh")
                if (orchard::Body* b = m_scene.body(e))
                    if (b->type == orchard::BodyType::dynamic) c.shape = orchard::ColliderShape::convex_hull;
            return;
        }
        c.size = {std::max(0.01f, hi.x - lo.x), std::max(0.01f, hi.y - lo.y), std::max(0.01f, hi.z - lo.z)};
        c.offset = (lo + hi) * 0.5f;
    }

    orchard::Entity place_model(const std::string& source, orchard::Entity parent) {
        const std::string stem = fs::path(source).stem().string();
        const orchard::Entity e = m_scene.create(stem.empty() ? "Model" : stem, parent);
        m_scene.add_model(e).source = source;
        orchard::Transform* t = m_scene.transform(e);
        orchard::Vec3 lo, hi;
        const bool sized = m_gpu.models().local_bounds(source, lo, hi);
        if (m_scene.dimension() == orchard::Dimension::two_d) {
            const orchard::Vec2 c = m_view.pan_camera().center;
            t->position = {c.x, c.y, 0.0f};
        } else {
            const orchard::Vec3 up = camera_up_axis();
            orchard::Vec3 at = m_view.orbit_camera().target;
            if (xylem::World* w = m_session.world()) {
                float hit = 0.0f;
                if (w->raycast(at + up * 50.0f, up * -1.0f, 200.0f, nullptr, nullptr, &hit).valid()) at = at + up * (50.0f - hit);
            }
            t->position = sized ? at - up * orchard::dot(lo, up) : at;
        }
        if (sized) fit_model_collider(e, "box");
        if (const fizmo::graphics::Model3D* model = m_gpu.models().model(source))
            if (!model->animations.empty()) m_scene.add_animator(e);
        return e;
    }

    orchard::Entity add_character_hitboxes(orchard::Entity e, orchard::HumanoidKind kind) {
        const orchard::Model* md = m_scene.model(e);
        if (!md) return orchard::Entity{};
        const orchard::editor::ModelInfo info = m_gpu.models().info(md->source);
        if (!info.ok) return orchard::Entity{};
        const orchard::Vec3 up = camera_up_axis();
        const float height = std::max(0.2f, orchard::dot(info.high - info.low, up));
        const orchard::Entity rig = orchard::add_humanoid(m_scene, kind, e, "Hitboxes", height);
        if (orchard::Transform* rt = m_scene.transform(rig)) {
            rt->position = up * orchard::dot(info.low, up);
            rt->rotation = orchard::Quat{};
            rt->scale = {1.0f, 1.0f, 1.0f};
        }
        m_scene.remove_body(rig);
        attach_parts_to_bones(e);
        fit_parts_to_bones(e);
        return rig;
    }

    void fit_parts_to_bones(orchard::Entity e) {
        const orchard::Model* md = m_scene.model(e);
        if (!md) return;
        const std::string source = md->source;
        const orchard::Mat4 model_world = m_scene.world_matrix(e);
        for (orchard::Entity c : m_scene.subtree(e)) {
            if (c == e || m_scene.bone(c).empty()) continue;
            std::vector<orchard::Collider>* cs = m_scene.colliders(c);
            if (!cs || cs->size() != 1 || (*cs)[0].sensor) continue;
            orchard::Vec3 head, tail;
            if (!m_gpu.models().bone_segment(source, m_scene.bone(c), head, tail)) continue;
            const orchard::Vec3 a = orchard::mul_point(model_world, head), b = orchard::mul_point(model_world, tail);
            orchard::Vec3 dir = b - a;
            const float len = orchard::length(dir);
            if (len < 1e-4f) continue;
            dir = dir * (1.0f / len);
            orchard::Collider& col = (*cs)[0];
            const float thick = std::max(0.02f, std::min(col.size.x, col.size.z));
            orchard::Quat q;
            const orchard::Vec3 y{0.0f, 1.0f, 0.0f};
            const float d = orchard::dot(y, dir);
            if (d < -0.9999f) q = orchard::Quat{1.0f, 0.0f, 0.0f, 0.0f};
            else {
                const orchard::Vec3 ax = orchard::cross(y, dir);
                q = orchard::normalize(orchard::Quat{ax.x, ax.y, ax.z, 1.0f + d});
            }
            const orchard::Mat4 target = orchard::trs((a + b) * 0.5f, q, {1.0f, 1.0f, 1.0f});
            orchard::Mat4 local = target;
            const orchard::Entity parent = m_scene.parent(c);
            orchard::Mat4 pinv;
            if (m_scene.alive(parent) && orchard::invert(m_scene.world_matrix(parent), pinv)) local = pinv * target;
            orchard::Transform* t = m_scene.transform(c);
            if (!t) continue;
            t->position = {local.at(0, 3), local.at(1, 3), local.at(2, 3)};
            if (m_scene.alive(parent)) {
                const orchard::Quat pr = xylem::world_rotation(m_scene, parent);
                t->rotation = orchard::normalize(orchard::Quat{-pr.x, -pr.y, -pr.z, pr.w} * q);
            } else {
                t->rotation = q;
            }
            t->scale = {1.0f, 1.0f, 1.0f};
            col.offset = {};
            if (col.shape == orchard::ColliderShape::capsule || col.shape == orchard::ColliderShape::cylinder || col.shape == orchard::ColliderShape::box) col.size = {thick, std::max(len, thick), thick};
        }
    }

    std::size_t attach_parts_to_bones(orchard::Entity e) {
        const orchard::Model* md = m_scene.model(e);
        if (!md) return 0;
        const orchard::editor::ModelInfo info = m_gpu.models().info(md->source);
        if (!info.ok || info.bones.empty()) return 0;
        std::size_t attached = 0;
        for (orchard::Entity c : m_scene.subtree(e)) {
            if (c == e) continue;
            const std::vector<orchard::Collider>* cs = m_scene.colliders(c);
            if (!cs || cs->empty()) continue;
            std::string bone = m_scene.bone(c);
            if (bone.empty()) bone = orchard::editor::guess_bone(info.bones, m_scene.name(c));
            if (bone.empty()) continue;
            m_scene.set_bone(c, bone);
            if (!m_scene.body(c)) m_scene.add_body(c).type = orchard::BodyType::kinematic;
            else m_scene.body(c)->type = orchard::BodyType::kinematic;
            ++attached;
        }
        return attached;
    }

    std::string import_files(const std::string& list) {
        if (!m_project) return "error\topen a project first";
        std::vector<std::string> picked;
        std::string line;
        std::istringstream in(list);
        while (std::getline(in, line))
            if (!line.empty()) picked.push_back(line);
        const fs::path assets = fs::path(m_project->folder()) / "Assets";
        std::error_code ec;
        fs::create_directories(assets, ec);
        std::size_t copied = 0;
        std::string first;
        auto copy_one = [&](const fs::path& from, const fs::path& to) {
            fs::create_directories(to.parent_path(), ec);
            if (fs::equivalent(from, to, ec)) return true;
            ec.clear();
            fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
            return !ec;
        };
        for (const std::string& p : picked) {
            const fs::path from(p);
            if (!fs::is_regular_file(from, ec)) continue;
            const fs::path to = assets / from.filename();
            if (!copy_one(from, to)) continue;
            ++copied;
            if (first.empty()) first = fs::path(to).lexically_relative(m_project->folder()).generic_string();
            std::string ext = from.extension().string();
            for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (ext == ".fbx") {
                for (const fs::path& folder : {from.parent_path() / (from.stem().string() + ".fbm"), from.parent_path() / "textures", from.parent_path() / "Textures"}) {
                    if (!fs::is_directory(folder, ec)) continue;
                    for (fs::recursive_directory_iterator it(folder, ec), end; it != end && !ec; it.increment(ec)) {
                        if (!it->is_regular_file(ec)) continue;
                        copy_one(it->path(), assets / folder.filename() / it->path().lexically_relative(folder));
                    }
                    ec.clear();
                }
            }
            if (ext == ".gltf" || ext == ".obj") {
                const std::string text = read_file(from.string());
                const std::string key = ext == ".gltf" ? "\"uri\"" : "";
                std::vector<std::string> refs;
                if (ext == ".gltf") {
                    for (std::size_t at = text.find(key); at != std::string::npos; at = text.find(key, at + 1)) {
                        const std::size_t q1 = text.find('"', text.find(':', at) + 1);
                        const std::size_t q2 = q1 == std::string::npos ? q1 : text.find('"', q1 + 1);
                        if (q2 == std::string::npos) break;
                        const std::string uri = text.substr(q1 + 1, q2 - q1 - 1);
                        if (uri.rfind("data:", 0) != 0) refs.push_back(uri);
                    }
                } else {
                    std::istringstream lines(text);
                    std::string l;
                    while (std::getline(lines, l))
                        if (l.rfind("mtllib ", 0) == 0) refs.push_back(l.substr(7));
                    for (std::size_t i = 0; i < refs.size(); ++i) {
                        const std::string mtl = read_file((from.parent_path() / refs[i]).string());
                        std::istringstream ml(mtl);
                        while (std::getline(ml, l)) {
                            const std::size_t sp = l.find(' ');
                            if (sp != std::string::npos && l.rfind("map_", 0) == 0) refs.push_back(l.substr(l.find_last_of(' ') + 1));
                        }
                    }
                }
                for (std::string r : refs) {
                    while (!r.empty() && (r.back() == '\r' || r.back() == ' ')) r.pop_back();
                    if (r.empty() || r.find("..") != std::string::npos) continue;
                    const fs::path src = from.parent_path() / fs::path(r);
                    if (fs::is_regular_file(src, ec)) copy_one(src, assets / fs::path(r));
                }
            }
        }
        if (copied == 0) return "error\tnothing was imported";
        m_gpu.models().reload_changed();
        return "ok\t" + std::to_string(copied) + "\t" + first;
    }

    void step_animation(double dt) {
        const bool running = m_session.playing();
        orchard::editor::ModelView& mv = m_gpu.models();
        if (running != m_animation_was_running) {
            mv.reset_animation();
            m_animation_was_running = running;
        }
        mv.animate(m_scene, dt, running || pref("preview_effects", "0") == "1");
        if (!running) return;
        const std::vector<orchard::Entity> moved = mv.follow_bones(m_scene);
        if (xylem::World* w = m_session.world())
            for (orchard::Entity e : moved) w->moved(e);
        orchard::ScriptRuntime& rt = m_session.scripts();
        for (const orchard::editor::FiredAnimationEvent& ev : mv.take_events()) rt.push_animation_event(ev.entity, ev.name);
        for (orchard::Entity e : m_scene.entities()) {
            if (!m_scene.animator(e)) continue;
            const orchard::editor::AnimationState st = mv.state(e);
            rt.set_animation_status(e, st.time, st.duration, st.playing);
        }
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
        if (m_scene.model(e)) tags += tags.empty() ? "model" : " model";
        if (m_scene.animator(e)) tags += tags.empty() ? "animated" : " animated";
        if (!m_scene.bone(e).empty()) tags += (tags.empty() ? "on " : " on ") + clean(m_scene.bone(e));
        out += std::to_string(e.id()) + "\t" + (p.valid() ? std::to_string(p.id()) : std::string()) + "\t" + std::to_string(depth) + "\t" + clean(m_scene.name(e)) + "\t" + tags + "\n";
        for (orchard::Entity c : m_scene.children(e)) walk(c, depth + 1, out);
    }

    std::string category(orchard::Entity e) const {
        if (m_scene.terrain(e)) return "containers";
        if (m_scene.model(e) && !m_scene.body(e)) return "models";
        if (m_scene.light(e)) return "lights";
        if (m_scene.fluid(e)) return "fluids";
        if (m_scene.emitter(e) && (!m_scene.colliders(e) || m_scene.colliders(e)->empty()) && !m_scene.body(e)) return "particles";
        if (m_scene.camera(e) || m_scene.name(e).find("Camera") != std::string::npos) return "cameras";
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

    struct OutlinePart {
        std::string label;
        std::string icon;
        std::string detail;
        std::string kind;
        int index = -1;
    };

    std::vector<OutlinePart> outline_parts(orchard::Entity e) const {
        std::vector<OutlinePart> parts;
        const std::vector<orchard::Collider>* cs = m_scene.colliders(e);
        const std::size_t shapes = cs ? cs->size() : 0;
        const int extras = (m_scene.fluid(e) ? 1 : 0) + (m_scene.light(e) ? 1 : 0) + (m_scene.emitter(e) ? 1 : 0) + (m_scene.model(e) ? 1 : 0);
        if (shapes < 2 && !(shapes >= 1 && extras >= 1)) return parts;
        if (const orchard::Fluid* f = m_scene.fluid(e)) {
            std::string label = f->preset.empty() ? std::string("Fluid") : f->preset;
            if (!label.empty()) label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
            parts.push_back({label, section_icon("fluids"), number(f->size.x) + " \u00D7 " + number(f->size.y) + " \u00D7 " + number(f->size.z), "fluid", -1});
        }
        if (const orchard::Model* md = m_scene.model(e)) parts.push_back({"Model", section_icon("models"), md->source, "model", -1});
        if (const orchard::Light* l = m_scene.light(e)) parts.push_back({orchard::light_type_label(l->type), section_icon("lights"), "light", "light", -1});
        if (m_scene.emitter(e)) parts.push_back({"Particles", section_icon("particles"), m_scene.emitter(e)->preset, "particles", -1});
        for (std::size_t i = 0; i < shapes; ++i) {
            const orchard::Collider& c = (*cs)[i];
            std::string shape = orchard::to_string(c.shape);
            for (char& ch : shape) if (ch == '-') ch = ' ';
            if (!shape.empty()) shape[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(shape[0])));
            const std::string label = c.name.empty() ? shape + " collider " + std::to_string(i + 1) : c.name;
            std::string detail = c.name.empty() ? std::string() : std::string(orchard::to_string(c.shape));
            if (c.sensor) detail += detail.empty() ? "sensor" : " sensor";
            if (!c.enabled) detail += detail.empty() ? "off" : " off";
            parts.push_back({label, c.sensor ? section_icon("triggers") : "\u25A1", detail, "collider", static_cast<int>(i)});
        }
        return parts;
    }

    std::string outline(const std::string& group, const std::string& filter) const {
        const std::string want = lower(filter);
        const std::string collapsed = "," + pref("outliner.collapsed", "") + ",";
        const std::string opened = "," + pref("outliner.open", "") + ",";
        std::set<std::uint64_t> forced;
        for (orchard::Entity at = m_scene.parent(m_selected); m_scene.alive(at); at = m_scene.parent(at)) forced.insert(at.id());
        auto is_open = [&](orchard::Entity e) {
            return forced.count(e.id()) > 0 || opened.find("," + std::to_string(e.id()) + ",") != std::string::npos;
        };
        auto matches = [&](orchard::Entity e) {
            if (want.empty()) return true;
            return lower(m_scene.name(e)).find(want) != std::string::npos || lower(tags_of(e)).find(want) != std::string::npos || lower(category(e)).find(want) != std::string::npos;
        };
        auto item = [&](orchard::Entity e, int depth, const std::string& fold, std::size_t count) {
            const std::string cat = category(e);
            return "item\t" + std::to_string(e.id()) + "\t" + std::to_string(depth) + "\t" + clean(m_scene.name(e)) + "\t" + section_icon(cat) + "\t" + tags_of(e) + "\t" + cat + "\t" + fold + "\t" + std::to_string(count) + "\n";
        };
        std::function<void(orchard::Entity, int, std::string&)> tree = [&](orchard::Entity e, int depth, std::string& out) {
            const std::vector<orchard::Entity> kids = m_scene.children(e);
            const std::vector<OutlinePart> parts = outline_parts(e);
            const std::size_t count = kids.size() + parts.size();
            const bool open = count > 0 && is_open(e);
            out += item(e, depth, count == 0 ? std::string() : open ? std::string("open") : std::string("closed"), count);
            if (!open) return;
            for (const OutlinePart& p : parts) out += "part\t" + std::to_string(e.id()) + "\t" + std::to_string(depth + 1) + "\t" + clean(p.label) + "\t" + p.icon + "\t" + clean(p.detail) + "\t" + p.kind + "\t" + std::to_string(p.index) + "\n";
            for (orchard::Entity c : kids) tree(c, depth + 1, out);
        };
        std::vector<orchard::Entity> all = m_scene.entities();
        if (group == "flat" || pref("outliner.sort", "scene") == "name") {
            std::stable_sort(all.begin(), all.end(), [this](orchard::Entity a, orchard::Entity b) { return lower(m_scene.name(a)) < lower(m_scene.name(b)); });
        }
        std::string out;
        if (group == "flat" || !want.empty()) {
            for (orchard::Entity e : all) {
                if (!matches(e)) continue;
                const std::string cat = category(e);
                const orchard::Entity p = m_scene.parent(e);
                std::string tags = tags_of(e);
                if (m_scene.alive(p)) tags += (tags.empty() ? "in " : " \u00B7 in ") + clean(m_scene.name(p));
                out += "item\t" + std::to_string(e.id()) + "\t0\t" + clean(m_scene.name(e)) + "\t" + section_icon(cat) + "\t" + tags + "\t" + cat + "\t\t0\n";
            }
            return out;
        }
        std::vector<orchard::Entity> roots;
        for (orchard::Entity e : all)
            if (!m_scene.alive(m_scene.parent(e))) roots.push_back(e);
        if (group == "hierarchy") {
            const bool closed = collapsed.find(",scene,") != std::string::npos;
            out += std::string("section\tscene\tTHIS SCENE\t") + std::to_string(roots.size()) + "\t" + (closed ? "1" : "0") + "\t\u25A4\n";
            if (closed) return out;
            for (orchard::Entity e : roots) tree(e, 0, out);
            return out;
        }
        const bool show_empty = pref("outliner.empty_sections", "0") == "1";
        for (const Section& sec : outline_sections()) {
            std::string rows;
            int count = 0;
            for (orchard::Entity e : roots) {
                if (category(e) != sec.key) continue;
                ++count;
                tree(e, 0, rows);
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
                const auto base_textures = m_gpu.lighting_textures();
                m_light_world.set_texture_loader([this, base_textures](const std::string& path, bool color) -> std::shared_ptr<const fizmo::lighting::Texture> {
                    if (orchard::editor::is_builtin_terrain_texture(path)) {
                        std::shared_ptr<const fizmo::lighting::Texture>& slot = m_trace_builtin[path];
                        if (!slot) {
                            orchard::editor::ParticlePixels px = orchard::editor::generate_builtin_terrain_texture(path, 256);
                            if (px.width > 0 && px.height > 0) slot = std::make_shared<fizmo::lighting::Texture>(fizmo::lighting::Texture::from_rgba8(static_cast<unsigned>(px.width), static_cast<unsigned>(px.height), std::move(px.rgba), fizmo::lighting::ColorSpace::srgb));
                        }
                        return slot;
                    }
                    return base_textures ? base_textures(path, color) : nullptr;
                });
                m_light_world.set_asset_root(m_project->folder());
                m_light_world.set_terrain_source(m_gpu.terrain().source());
                m_light_world.set_model_source([this](orchard::Entity e, const std::string& source) -> std::shared_ptr<const chlorophyll::ModelGeometry> {
                    const fizmo::graphics::Model3D* model = m_gpu.models().model(source);
                    if (!model) return nullptr;
                    const orchard::Animator* an = e.valid() && m_scene.alive(e) ? m_scene.animator(e) : nullptr;
                    if (!an || pref("viewport.trace", "off") == "off") return m_trace_poser.rest(*model);
                    return m_trace_poser.posed(e, *model, an->clip, m_gpu.models().state(e).time);
                });
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
            const orchard::Vec3 sc = xylem::world_scale(m_scene, e);
            std::vector<orchard::Entity> parts{e};
            while (!parts.empty()) {
                const orchard::Entity at = parts.back();
                parts.pop_back();
                if (at != e && m_scene.body(at)) continue;
                if (const std::vector<orchard::Collider>* cs = m_scene.colliders(at)) {
                    const orchard::Vec3 part_scale = at == e ? sc : xylem::world_scale(m_scene, at);
                    for (const orchard::Collider& c : *cs) {
                        if (c.sensor || !c.enabled) continue;
                        mass += orchard::collider_mass(c, part_scale, m_scene.dimension());
                    }
                }
                for (orchard::Entity child : m_scene.children(at)) parts.push_back(child);
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

    static std::optional<orchard::UnitKind> unit_kind_of(const std::string& key) {
        using K = orchard::UnitKind;
        static const std::map<std::string, K> keys = {
            {"survival.carried", K::weight}, {"survival.carry_limit", K::weight}, {"inventory.max_weight", K::weight},
            {"climate.wind_speed", K::speed}, {"climate.gust", K::speed}, {"climate.size_x", K::distance}, {"climate.size_y", K::distance}, {"climate.size_z", K::distance}, {"climate.blend", K::distance},
            {"survival.temperature", K::temperature}, {"survival.ambient", K::temperature}, {"survival.comfort_low", K::temperature}, {"survival.comfort_high", K::temperature},
            {"survival.insulation", K::temperature_change}, {"survival.cold_damage", K::per_degree}, {"survival.heat_damage", K::per_degree},
            {"camera.move_speed", K::speed}, {"camera.sprint_speed", K::speed}, {"camera.jump_speed", K::speed}, {"agent.walk_speed", K::speed}, {"agent.run_speed", K::speed}, {"health.poise", K::speed},
            {"agent.sight_range", K::distance}, {"agent.attack_range", K::distance}, {"agent.eye_height", K::distance}, {"agent.fly_height", K::distance}, {"agent.radius", K::distance}, {"agent.height", K::distance}, {"agent.step", K::distance}, {"agent.max_drop", K::distance},
            {"camera.eye_height", K::distance}, {"camera.view_height", K::distance}, {"camera.near_clip", K::distance}, {"camera.far_clip", K::distance}, {"survival.head_height", K::distance}, {"health.explode_radius", K::distance},
            {"sound.range", K::distance}, {"sound.min_distance", K::distance}, {"light.radius", K::distance}, {"light.length", K::distance}, {"light.width", K::distance}, {"light.height", K::distance},
            {"agent.attack_knockback", K::impulse},
        };
        if (const auto it = keys.find(key); it != keys.end()) return it->second;
        if (key.rfind("collider:", 0) == 0 && key.size() > 5 && key.compare(key.size() - 5, 5, ":mass") == 0) return K::weight;
        return std::nullopt;
    }

    static std::string unit_pref_key(orchard::UnitKind k) {
        using K = orchard::UnitKind;
        if (k == K::temperature_change || k == K::per_degree) k = K::temperature;
        return std::string("units.") + orchard::to_string(k);
    }

    std::string unit_of(orchard::UnitKind k) const {
        const std::string u = pref(unit_pref_key(k), orchard::units_of(k).front().name);
        return orchard::find_unit(k, u) ? u : std::string(orchard::units_of(k).front().name);
    }

    std::string unit_field(orchard::UnitKind k, const std::string& label) const {
        std::vector<std::pair<std::string, std::string>> options;
        for (const orchard::UnitDef& u : orchard::units_of(k)) options.emplace_back(u.name, u.label);
        return field(unit_pref_key(k), label, "select", unit_of(k), options_of(options), "Only changes how the editor shows these numbers; the game and scripts always use metres, kilograms, seconds and Celsius.");
    }

    std::string field(const std::string& key, const std::string& label, const std::string& kind, const std::string& value, const std::string& extra = std::string(), const std::string& help = std::string()) const {
        std::string shown = value, named = label;
        if (kind == "float")
            if (const std::optional<orchard::UnitKind> uk = unit_kind_of(key)) {
                using K = orchard::UnitKind;
                const std::string unit = unit_of(*uk);
                shown = number(static_cast<float>(orchard::from_si(std::atof(value.c_str()), *uk, unit)));
                const std::string si = *uk == K::weight ? "kg" : *uk == K::speed ? "m/s" : *uk == K::distance ? "m" : *uk == K::impulse ? "N\u00B7s" : *uk == K::per_degree ? "" : "C";
                const std::string to = orchard::unit_short(*uk, unit);
                bool done = false;
                for (const std::string& open : {std::string("(") + si + ")", std::string("(") + si + ","}) {
                    const std::size_t at = si.empty() ? std::string::npos : named.find(open);
                    if (at != std::string::npos) {
                        named.replace(at + 1, si.size(), to);
                        done = true;
                        break;
                    }
                }
                if (!done) named += " (" + to + ")";
            }
        return "field\t" + key + "\t" + clean(named) + "\t" + kind + "\t" + clean(shown) + "\t" + clean(extra) + "\t" + clean(help) + "\n";
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

        const std::optional<orchard::HumanoidKind> human = orchard::humanoid_kind_of(m_scene, e);
        const std::string look = orchard::character_style(m_scene, e);
        if (human || !look.empty()) {
            group("Character");
            std::vector<std::pair<std::string, std::string>> looks = {{"", "Colliders only (no mesh)"}};
            for (const std::string& st : orchard::character_styles()) looks.emplace_back(st, orchard::character_style_label(st));
            out += field("character_style", "Looks like", "select", look, options_of(looks), "In 3D it's a mesh over the colliders; in 2D it's drawn as pixel art facing the way it moves.");
            std::vector<std::pair<std::string, std::string>> kinds;
            for (orchard::HumanoidKind hk : orchard::humanoid_kinds()) kinds.emplace_back(orchard::to_string(hk), orchard::humanoid_label(hk));
            if (!human) kinds.insert(kinds.begin(), {"", "Its own colliders"});
            out += field("character_colliders", "Colliders", "select", human ? std::string(orchard::to_string(*human)) : std::string(), options_of(kinds), is_physical_player(e) ? "Swapping them keeps the player at 75 kg with no friction." : "");
        }

        group("Physics");
        const orchard::Body* b = m_scene.body(e);
        out += field("has_body", "Rigid body", "bool", yesno(b != nullptr), "", "Off: it doesn't move on its own. Colliders without a body act as walls and floors.");
        if (b) {
            out += field("body_type", "Type", "select", orchard::to_string(b->type), options_of({{"dynamic", "Dynamic (falls, gets pushed)"}, {"static", "Container (wall, floor, never moves)"}, {"kinematic", "Kinematic (moved by scripts)"}}));
            out += field("body_enabled", "Enabled", "bool", yesno(b->enabled));
            out += field("gravity_scale", "Gravity scale", "float", number(b->gravity_scale), "0.1");
            out += field("linear_damping", "Drag", "float", number(b->linear_damping), "0.05");
            out += field("angular_damping", "Spin drag", "float", number(b->angular_damping), "0.05");
            if (b->type != orchard::BodyType::fixed) {
                group("Start motion");
                out += field("v0", "Starting velocity (m/s)", "vec3", vec_text(b->start_velocity), "0.5");
                out += field("spin0", "Starting spin (degrees/s)", "vec3", vec_text(b->start_spin), "5");
                out += field("accel", "Constant acceleration (m/s\u00B2)", "vec3", vec_text(b->acceleration), "0.5", "Pushes it the same way every moment, like extra gravity. A rocket, a conveyor, wind.");
                out += field("motion_local", "Relative to its own rotation", "bool", yesno(b->motion_local));
            }
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
                out += field(base + "shape", "Shape", "select", orchard::to_string(c.shape), options_of({{"box", "Box"}, {"sphere", "Sphere"}, {"capsule", "Capsule"}, {"cylinder", "Cylinder"}, {"cone", "Cone"}, {"rounded-box", "Rounded box"}, {"sphere-section", "Sphere section (hemisphere, dome, bowl, wedge)"}, {"triangle", "Triangle prism"}}));
                if (c.shape == orchard::ColliderShape::triangle) {
                    std::vector<std::pair<std::string, std::string>> rules;
                    for (orchard::TriangleRule r : orchard::triangle_rules()) rules.emplace_back(orchard::to_string(r), orchard::triangle_rule_label(r));
                    out += field(base + "tri_rule", "Triangle built from", "select", orchard::to_string(c.triangle), options_of(rules));
                    const std::string sa = number(c.triangle_sides.x), sb = number(c.triangle_sides.y), sc = number(c.triangle_sides.z);
                    const std::string aa = number(c.triangle_angles.x), ab = number(c.triangle_angles.y), ac = number(c.triangle_angles.z);
                    orchard::Vec3 ts, ta;
                    std::string why;
                    const std::string info = orchard::solve_triangle(c, ts, ta, &why) ? "Sides " + number(ts.x) + ", " + number(ts.y) + ", " + number(ts.z) + " m; angles " + number(std::round(ta.x * 10.0f) / 10.0f) + ", " + number(std::round(ta.y * 10.0f) / 10.0f) + ", " + number(std::round(ta.z * 10.0f) / 10.0f) + " degrees. Depth is size Z." : "No triangle: " + why;
                    switch (c.triangle) {
                    case orchard::TriangleRule::sss:
                        out += field(base + "tri_a", "Side a (m)", "float", sa, "0.05");
                        out += field(base + "tri_b", "Side b (m)", "float", sb, "0.05");
                        out += field(base + "tri_c", "Side c (m)", "float", sc, "0.05", info);
                        break;
                    case orchard::TriangleRule::sas:
                        out += field(base + "tri_b", "Side b (m)", "float", sb, "0.05");
                        out += field(base + "tri_A", "Angle A between them (degrees)", "float", aa, "1");
                        out += field(base + "tri_c", "Side c (m)", "float", sc, "0.05", info);
                        break;
                    case orchard::TriangleRule::asa:
                        out += field(base + "tri_B", "Angle B (degrees)", "float", ab, "1");
                        out += field(base + "tri_a", "Side a between them (m)", "float", sa, "0.05");
                        out += field(base + "tri_C", "Angle C (degrees)", "float", ac, "1", info);
                        break;
                    case orchard::TriangleRule::aas:
                        out += field(base + "tri_A", "Angle A (degrees)", "float", aa, "1");
                        out += field(base + "tri_B", "Angle B (degrees)", "float", ab, "1");
                        out += field(base + "tri_a", "Side a, across from A (m)", "float", sa, "0.05", info);
                        break;
                    case orchard::TriangleRule::ssa:
                        out += field(base + "tri_a", "Side a (m)", "float", sa, "0.05");
                        out += field(base + "tri_b", "Side b (m)", "float", sb, "0.05");
                        out += field(base + "tri_A", "Angle A, across from a (degrees)", "float", aa, "1");
                        out += field(base + "tri_wide", "Use the wider triangle (when two fit)", "bool", yesno(c.triangle_wide), "", info);
                        break;
                    case orchard::TriangleRule::right:
                        out += field(base + "tri_b", "Leg b (m)", "float", sb, "0.05");
                        out += field(base + "tri_c", "Leg c (m)", "float", sc, "0.05", info);
                        break;
                    }
                }
                out += field(base + "size", "Size", "vec3", vec_text(c.size), "0.1");
                out += field(base + "offset", "Offset", "vec3", vec_text(c.offset), "0.1");
                const orchard::Vec3 wscale = xylem::world_scale(m_scene, e);
                const float cmass = orchard::collider_mass(c, wscale, m_scene.dimension());
                const float cdensity = c.locked(orchard::collider_lock_mass) && c.mass > 0.0f ? cmass / std::max(1e-9f, orchard::collider_density_unit(m_scene.dimension()) * orchard::collider_volume(c, wscale, m_scene.dimension())) : c.density;
                out += field(base + "name", "Name", "string", c.name);
                if (i == 0) out += unit_field(orchard::UnitKind::weight, "Weight unit");
                out += field(base + "mass", "Mass (kg)", "float", number(cmass), "0.1");
                out += field(base + "density", flat ? "Density (kg/m\u00B2)" : "Density (\u00D7 water)", "float", number(cdensity), "0.1");
                out += field(base + "lock_mass", "Lock mass", "bool", yesno(c.locked(orchard::collider_lock_mass)));
                out += field(base + "lock_density", "Lock density", "bool", yesno(c.locked(orchard::collider_lock_density)));
                out += field(base + "lock_sx", "Lock size X", "bool", yesno(c.locked(orchard::collider_lock_size_x)));
                out += field(base + "lock_sy", "Lock size Y", "bool", yesno(c.locked(orchard::collider_lock_size_y)));
                if (!flat) out += field(base + "lock_sz", "Lock size Z", "bool", yesno(c.locked(orchard::collider_lock_size_z)));
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

        group("Camera");
        const orchard::Camera* cam = m_scene.camera(e);
        out += field("has_camera", "Game camera (Play looks through it)", "bool", yesno(cam != nullptr));
        if (cam) {
            out += field("camera.active", "Player view (Play looks through it)", "bool", yesno(cam->active));
            out += field("camera.control", "Built-in controls", "select", orchard::to_string(cam->control), options_of({{"none", "None (scripts move it)"}, {"fly", "Fly (W A S D, Q / E, drag to look)"}, {"walk", "Walk (W A S D, Space jumps, drag to look)"}}));
            if (cam->control != orchard::CameraControl::none) {
                out += unit_field(orchard::UnitKind::speed, "Speed unit");
                out += unit_field(orchard::UnitKind::distance, "Distance unit");
                out += field("camera.move_speed", "Speed (m/s)", "float", number(cam->move_speed), "0.5");
                out += field("camera.sprint_speed", "Shift speed (m/s, 0 = 3x speed)", "float", number(cam->sprint_speed), "0.5");
                out += field("camera.mouse_look", "Drag to look around", "bool", yesno(cam->mouse_look));
                out += field("camera.look_speed", "Look speed (degrees per pixel)", "float", number(cam->look_speed), "0.05");
                if (cam->control == orchard::CameraControl::walk) {
                    out += field("camera.eye_height", "Eye height (m)", "float", number(cam->eye_height), "0.1");
                    const orchard::Entity rider = m_scene.body_owner(e);
                    const orchard::Body* rb = rider.valid() ? m_scene.body(rider) : nullptr;
                    std::string how = "Give it (or a parent) a dynamic rigid body and a capsule collider to make it a physical player that bumps into things and gets pushed.";
                    if (rb && rb->type == orchard::BodyType::dynamic) how = rider == e ? "Walks as a physical body: its own body and colliders bump into things." : "Walks as a physical body: " + m_scene.name(rider) + "'s body and colliders bump into things.";
                    out += field("camera.jump_speed", "Jump speed (m/s)", "float", number(cam->jump_speed), "0.5", how);
                }
            }
            out += field("camera.priority", "Priority (highest active one wins)", "float", std::to_string(cam->priority), "1");
            if (m_scene.dimension() == orchard::Dimension::two_d) {
                out += field("camera.view_height", "Shows this much height (m)", "float", number(cam->view_height), "0.5");
            } else {
                out += field("camera.fov", "Field of view (degrees)", "float", number(cam->fov), "1");
                out += field("camera.near_clip", "Nearest it draws (m)", "float", number(cam->near_clip), "0.01");
                out += field("camera.far_clip", "Farthest it draws (m)", "float", number(cam->far_clip), "10");
            }
            out += field("camera_from_view", "Move it to where the view is now", "button", "1");
        }

        group("Health and tags");
        const orchard::Health* hp = m_scene.health(e);
        out += field("has_health", "Has health (can be hurt, burn, explode)", "bool", yesno(hp != nullptr));
        if (hp) {
            out += field("health.value", "Health", "float", number(hp->value), "5");
            out += field("health.max", "Max health", "float", number(hp->max), "5");
            out += field("health.regen", "Heals per second", "float", number(hp->regen), "1");
            if (hp->regen != 0.0f) out += field("health.regen_delay", "Starts healing after (s without damage)", "float", number(hp->regen_delay), "0.5");
            out += field("health.invulnerable", "Can't be hurt", "bool", yesno(hp->invulnerable));
            out += field("health.impact_damage", "Hurt by hard hits (0 = off)", "float", number(hp->impact_damage), "0.1");
            out += field("health.flammable", "Catches fire", "bool", yesno(hp->flammable));
            if (hp->flammable) out += field("health.burn_damage", "Burn damage per second", "float", number(hp->burn_damage), "1");
            std::vector<std::pair<std::string, std::string>> deaths;
            for (orchard::DeathAction a : orchard::death_actions()) deaths.emplace_back(orchard::to_string(a), orchard::death_action_label(a));
            out += field("health.on_death", "When health runs out", "select", orchard::to_string(hp->on_death), options_of(deaths));
            if (hp->on_death == orchard::DeathAction::explode) {
                out += field("health.explode_radius", "Blast radius (m)", "float", number(hp->explode_radius), "0.5");
                out += field("health.explode_force", "Blast push", "float", number(hp->explode_force), "1000");
                out += field("health.explode_damage", "Blast damage", "float", number(hp->explode_damage), "5");
            }
            if (hp->on_death == orchard::DeathAction::respawn) out += field("health.respawn_delay", "Comes back after (s)", "float", number(hp->respawn_delay), "0.5");
            out += unit_field(orchard::UnitKind::speed, "Speed unit");
            out += unit_field(orchard::UnitKind::distance, "Distance unit");
            out += field("health.armor", "Armor", "float", number(hp->armor), "1");
            {
                std::vector<std::pair<std::string, std::string>> modes;
                for (const std::string& m : orchard::armor_modes()) modes.emplace_back(m, orchard::armor_mode_label(m));
                out += field("health.armor_mode", "How armor works", "select", hp->armor_mode, options_of(modes));
            }
            if (hp->armor_mode == "diminishing") out += field("health.armor_constant", "Armor that halves damage", "float", number(hp->armor_constant), "5");
            if (hp->armor_mode == "percent") out += field("health.armor_cap", "Blocks at most (0 to 1)", "slider", number(hp->armor_cap), "0.05");
            if (hp->armor_mode == "flat") out += field("health.armor_min", "Always lets through at least (0 to 1 of the hit)", "slider", number(hp->armor_min), "0.05");
            if (hp->armor_mode == "formula") out += field("health.armor_formula", "Armor formula (damage left after armor)", "string", hp->armor_formula, std::string(), "Uses amount, armor, constant, health, max_health, shield, max_shield, resist, knockback, distance, is_melee, has_poison, random(a, b), min, max, clamp, pow. Example: max(amount - armor * 0.5, amount * 0.2)");
            if (hp->armor_mode != "none") {
                out += field("health.armor_types", "Armor blocks these damage types (comma separated, all = everything but true)", "string", hp->armor_types);
                out += field("health.armor_wear", "Armor lost per damage it blocks", "float", number(hp->armor_wear), "0.01");
            }
            out += field("health.max_shield", "Shield (soaks up damage before health, 0 = none)", "float", number(hp->max_shield), "5");
            if (hp->max_shield > 0.0f) {
                out += field("health.shield", "Shield now", "float", number(hp->shield), "5");
                out += field("health.shield_regen", "Shield recharges per second", "float", number(hp->shield_regen), "1");
                out += field("health.shield_delay", "Starts recharging after (s without damage)", "float", number(hp->shield_delay), "0.5");
                out += field("health.shield_absorb", "Share of each hit the shield takes (0 to 1)", "slider", number(hp->shield_absorb), "0.05");
                out += field("health.shield_types", "Shield takes these damage types (empty = all but true)", "string", hp->shield_types);
                out += field("health.shield_formula", "Shield formula (how much it soaks up, empty = share above)", "string", hp->shield_formula, std::string(), "Uses the same names as the armor formula, with amount being the hit after armor. Example: amount * (0.5 + 0.5 * shield_fraction)");
            }
            out += field("health.resist", "Resistances (type=amount, 0.5 = half, 1 = immune, -0.5 = weak)", "string", orchard::resist_text(hp->resist), std::string(), "Works for damage types (fire, poison, cold, bleed, shock, explosion, ...) and for effect names (stunned, frozen, ...), which also shortens how long they last.");
            out += field("health.poise", "Staggers when knocked faster than (m/s)", "float", number(hp->poise), "0.1");
            out += field("health.stagger_scale", "Stagger time multiplier (0 = never staggers)", "float", number(hp->stagger_scale), "0.1");
            out += field("health.damage_formula", "Damage formula (empty = normal)", "string", hp->damage_formula, std::string(), "Changes every hit after armor and resistances. Uses amount, base, health, max_health, armor, resist, over_time, staggered, knockback, distance, is_fire, has_poison, random(a, b), min, max, clamp. Example: amount * (1 + is_fire)");
            out += field("health.effect_formula", "Effect length formula (empty = normal)", "string", hp->effect_formula, std::string(), "Changes how long effects last. Uses duration, strength, stacks, resist, health, max_health, forever, is_poison. Return 0 to block the effect.");
        }
        const orchard::Survival* sv = m_scene.survival(e);
        out += field("has_survival", "Has survival stats (stamina, hunger, thirst, breath, temperature)", "bool", yesno(sv != nullptr));
        if (sv) {
            out += field("survival.stamina_on", "Stamina (sprinting and jumping use it)", "bool", yesno(sv->stamina_on));
            if (sv->stamina_on) {
                out += field("survival.stamina", "Stamina", "float", number(sv->stamina), "5");
                out += field("survival.max_stamina", "Max stamina", "float", number(sv->max_stamina), "5");
                out += field("survival.stamina_regen", "Recovers per second", "float", number(sv->stamina_regen), "1");
                out += field("survival.stamina_delay", "Starts recovering after (s)", "float", number(sv->stamina_delay), "0.1");
                out += field("survival.sprint_cost", "Sprinting uses per second", "float", number(sv->sprint_cost), "1");
                out += field("survival.jump_cost", "Each jump uses", "float", number(sv->jump_cost), "1");
                out += field("survival.sprint_min", "Needs this much to start sprinting", "float", number(sv->sprint_min), "1");
            }
            out += field("survival.hunger_on", "Hunger", "bool", yesno(sv->hunger_on));
            if (sv->hunger_on) {
                out += field("survival.hunger", "Food", "float", number(sv->hunger), "1");
                out += field("survival.max_hunger", "Max food", "float", number(sv->max_hunger), "1");
                out += field("survival.saturation", "Saturation (used up before food)", "float", number(sv->saturation), "1");
                out += field("survival.idle_exhaustion", "Tiredness per second resting (4 = one food)", "float", number(sv->idle_exhaustion), "0.001");
                out += field("survival.sprint_exhaustion", "Extra tiredness per second sprinting", "float", number(sv->sprint_exhaustion), "0.01");
                out += field("survival.jump_exhaustion", "Tiredness per jump", "float", number(sv->jump_exhaustion), "0.01");
                out += field("survival.fed_level", "Heals while food is at least", "float", number(sv->fed_level), "1");
                out += field("survival.fed_heal", "Heals per second when fed", "float", number(sv->fed_heal), "0.1");
                out += field("survival.heal_exhaustion", "Tiredness per health healed", "float", number(sv->heal_exhaustion), "0.5");
                out += field("survival.starve_damage", "Starving damage per second", "float", number(sv->starve_damage), "0.1");
            }
            out += field("survival.thirst_on", "Thirst", "bool", yesno(sv->thirst_on));
            if (sv->thirst_on) {
                out += field("survival.thirst", "Water", "float", number(sv->thirst), "5");
                out += field("survival.max_thirst", "Max water", "float", number(sv->max_thirst), "5");
                out += field("survival.thirst_rate", "Water used per second", "float", number(sv->thirst_rate), "0.01");
                out += field("survival.sprint_thirst", "Sprinting multiplies that by", "float", number(sv->sprint_thirst), "0.1");
                out += field("survival.dehydrate_damage", "Thirst damage per second", "float", number(sv->dehydrate_damage), "0.1");
            }
            out += field("survival.breath_on", "Breath (drowns under water)", "bool", yesno(sv->breath_on));
            if (sv->breath_on) {
                out += field("survival.max_breath", "Holds breath for (s)", "float", number(sv->max_breath), "1");
                out += field("survival.breath_recover", "Breath back per second", "float", number(sv->breath_recover), "0.5");
                out += field("survival.drown_damage", "Drowning damage per second", "float", number(sv->drown_damage), "1");
                out += unit_field(orchard::UnitKind::distance, "Distance unit");
                out += field("survival.head_height", "Head height (m)", "float", number(sv->head_height), "0.1");
            }
            out += field("survival.temperature_on", "Body temperature", "bool", yesno(sv->temperature_on));
            if (sv->temperature_on) {
                out += unit_field(orchard::UnitKind::temperature, "Temperature unit");
                out += field("survival.temperature", "Body temperature (C)", "float", number(sv->temperature), "0.5");
                out += field("survival.ambient", "Air temperature (C)", "float", number(sv->ambient), "1");
                out += field("survival.insulation", "Clothing warmth (C)", "float", number(sv->insulation), "1");
                out += field("survival.comfort_low", "Gets cold below (C)", "float", number(sv->comfort_low), "1");
                out += field("survival.comfort_high", "Gets hot above (C)", "float", number(sv->comfort_high), "1");
                out += field("survival.exposure", "How fast the body cools or heats", "float", number(sv->exposure), "0.005");
                out += field("survival.cold_damage", "Cold damage per second per degree of hypothermia (below 35 C, 95 F)", "float", number(sv->cold_damage), "0.1");
                out += field("survival.heat_damage", "Heat damage per second per degree of heatstroke (above 40 C, 104 F)", "float", number(sv->heat_damage), "0.1");
            }
            out += field("survival.weight_on", "Weight and encumbrance", "bool", yesno(sv->weight_on));
            if (sv->weight_on) {
                out += unit_field(orchard::UnitKind::weight, "Weight unit");
                out += field("survival.carry_limit", "Can carry", "float", number(sv->carry_limit), "1");
                out += field("survival.carried", "Extra weight carried (scripts change this)", "float", number(sv->carried), "1");
                out += field("survival.count_items", "Also count children tagged item", "bool", yesno(sv->count_items));
                out += field("survival.light_load", "No slowdown below (share of what it can carry)", "slider", number(sv->light_load), "0.05");
                out += field("survival.heavy_speed", "Speed when fully loaded (0 to 1)", "slider", number(sv->heavy_speed), "0.05");
                out += field("survival.overload", "Can't move at all at (share of what it can carry, 1.5 = 150%)", "float", number(sv->overload), "0.05");
                out += field("survival.sprint_load", "Can't sprint above (share)", "float", number(sv->sprint_load), "0.05");
                out += field("survival.load_stamina", "Extra stamina used when fully loaded (1 = double)", "float", number(sv->load_stamina), "0.1");
            }
        }

        group("Items and inventory");
        const orchard::Inventory* inv = m_scene.inventory(e);
        out += field("has_inventory", "Has an inventory (players, chests, backpacks, NPCs)", "bool", yesno(inv != nullptr));
        if (inv) {
            auto iv = [&](const char* k) { return orchard::inventory_field(*inv, k); };
            std::vector<std::pair<std::string, std::string>> presets, styles, anchors, measures, modes, ball;
            for (const std::string& p : orchard::inventory_presets()) presets.emplace_back(p, orchard::inventory_preset_label(p));
            for (orchard::InventoryStyle st : orchard::inventory_styles()) styles.emplace_back(orchard::to_string(st), orchard::inventory_style_label(st));
            for (const std::string& a : orchard::hud_anchors()) anchors.emplace_back(a, a);
            measures = {{"count", "Count (whole items, like Minecraft)"}, {"weight", "Weight (stack holds up to this much weight)"}, {"volume", "Volume (stack holds up to this many litres)"}};
            modes = {{"beside", "Shown next to the inventory"}, {"own_key", "Opened with their own key"}, {"both", "Both"}};
            ball = {{"", "Use the game's mode"}};
            for (orchard::Ballistics b : orchard::ballistics_modes()) ball.emplace_back(orchard::to_string(b), orchard::ballistics_label(b));
            out += field("inventory.enabled", "Inventory on", "bool", iv("enabled"));
            out += field("inventory.preset", "Works like", "select", inv->preset, options_of(presets), "Picking one fills in every setting below; change any of them afterwards.");
            out += field("inventory.style", "Style", "select", iv("style"), options_of(styles));
            out += field("inventory.title", "Title", "string", inv->title);
            if (inv->style != orchard::InventoryStyle::weight && inv->style != orchard::InventoryStyle::volume) {
                out += field("inventory.columns", "Columns", "float", iv("columns"), "1");
                out += field("inventory.rows", "Rows", "float", iv("rows"), "1");
            }
            out += unit_field(orchard::UnitKind::weight, "Weight unit");
            out += field("inventory.max_weight", "Can carry at most (0 = no limit)", "float", iv("max_weight"), "1");
            out += field("inventory.max_volume", "Holds at most litres (0 = no limit)", "float", iv("max_volume"), "1");
            out += field("inventory.stack_measure", "Stacks are measured by", "select", inv->stack_measure, options_of(measures));
            out += field("inventory.stack_limit", "Stack limit (0 = each item's own, never below 0)", "float", iv("stack_limit"), "1", "With count it is how many fit in one slot. With weight or volume it is how much weight or how many litres fit. Fractions are allowed unless whole counts is on.");
            out += field("inventory.whole_counts", "Whole counts only (no 0.5 of an item)", "bool", iv("whole_counts"));
            out += field("inventory.count_weight", "Items count towards carry weight", "bool", iv("count_weight"));
            out += field("inventory.show_weight", "Show weight and volume on screen", "bool", iv("show_weight"));
            out += field("inventory.open_keys", "Opens with (keys, comma separated, empty = never)", "string", inv->open_keys);
            out += field("inventory.anchor", "Screen position", "select", inv->anchor, options_of(anchors));
            out += field("inventory.offset_x", "Move right (px)", "float", iv("offset_x"), "4");
            out += field("inventory.offset_y", "Move down (px)", "float", iv("offset_y"), "4");
            out += field("inventory.cell", "Cell size (px)", "float", iv("cell"), "2");
            out += field("inventory.padding", "Gap between cells (px)", "float", iv("padding"), "1");
            out += field("inventory.opacity", "Background opacity (0 to 1)", "slider", iv("opacity"), "0.05");
            out += field("inventory.hotbar", "Hotbar", "bool", iv("hotbar"));
            if (inv->hotbar) {
                out += field("inventory.hotbar_slots", "Hotbar slots", "float", iv("hotbar_slots"), "1");
                out += field("inventory.hotbar_own", "Hotbar has its own slots (off = shows the first inventory row)", "bool", iv("hotbar_own"));
                out += field("inventory.hotbar_anchor", "Hotbar position", "select", inv->hotbar_anchor, options_of(anchors));
                out += field("inventory.hotbar_x", "Hotbar move right (px)", "float", iv("hotbar_x"), "4");
                out += field("inventory.hotbar_y", "Hotbar move down (px)", "float", iv("hotbar_y"), "4");
                out += field("inventory.hotbar_cell", "Hotbar cell size (px)", "float", iv("hotbar_cell"), "2");
                out += field("inventory.hotbar_padding", "Hotbar gap (px)", "float", iv("hotbar_padding"), "1");
                out += field("inventory.hotbar_vertical", "Vertical hotbar", "bool", iv("hotbar_vertical"));
                out += field("inventory.hotbar_keys", "Slot keys (comma separated, in order)", "string", inv->hotbar_keys);
                out += field("inventory.hotbar_scroll", "Mouse wheel changes slot", "bool", iv("hotbar_scroll"));
            }
            out += field("inventory.equipment", "Equipment slots (clothing, armor, trinkets, holsters)", "bool", iv("equipment"));
            out += field("inventory.require_holsters", "Putting away a weapon needs a holster, sling or big enough bag", "bool", iv("require_holsters"));
            out += field("inventory.container_mode", "Worn bags and rigs", "select", inv->container_mode, options_of(modes));
            if (inv->container_mode != "beside") out += field("inventory.container_keys", "Bags open with", "string", inv->container_keys);
            out += field("inventory.use_keys", "Use / attack", "string", inv->use_keys);
            out += field("inventory.alt_keys", "Aim / block / offhand", "string", inv->alt_keys);
            out += field("inventory.reload_keys", "Reload", "string", inv->reload_keys);
            out += field("inventory.interact_keys", "Pick up / open", "string", inv->interact_keys);
            out += field("inventory.drop_keys", "Drop", "string", inv->drop_keys);
            out += field("inventory.holster_keys", "Holster / draw", "string", inv->holster_keys);
            out += field("inventory.ballistics", "Ballistics for this one", "select", inv->ballistics, options_of(ball));
            out += field("inventory.stance_accuracy", "Stance and movement change accuracy", "bool", iv("stance_accuracy"));
            std::string items;
            for (const orchard::ItemStack& st : inv->items) items += (items.empty() ? "" : ", ") + orchard::stack_text(st);
            out += field("inventory.start_items", "Starts with (item*amount, comma separated)", "string", items, std::string(), "Example: pistol_9mm, ammo_9mm*30, bandage*3, water*1.5. Item names are in the right-click Add Item menu.");
        }
        const orchard::Pickup* pk = m_scene.pickup(e);
        out += field("has_pickup", "Is an item pickup", "bool", yesno(pk != nullptr));
        if (pk) {
            out += field("pickup.item", "Item", "string", pk->stack.item);
            out += field("pickup.amount", "Amount", "float", number(pk->stack.amount), "1");
            out += field("pickup.auto", "Picked up by walking over it", "bool", yesno(pk->auto_pickup));
            out += field("pickup.respawn", "Comes back after (s, 0 = never)", "float", number(pk->respawn), "1");
            out += field("pickup.look", "Look", "select", pk->look, options_of({{"simple", "Simple"}, {"detailed", "Detailed"}, {"realistic", "Realistic (liquids move)"}}));
        }

        group("Weather and climate");
        const orchard::Climate* cl = m_scene.climate(e);
        out += field("has_climate", "Is a climate zone (heat, cold, wind, rain, snow, fog)", "bool", yesno(cl != nullptr));
        if (cl) {
            std::vector<std::pair<std::string, std::string>> pre;
            for (const std::string& p : orchard::climate_presets()) pre.emplace_back(p, p);
            out += field("climate.preset", "Weather", "select", cl->preset, options_of(pre), "Picking one fills in the values below.");
            out += field("climate.active", "On", "bool", yesno(cl->active));
            out += field("climate.global", "Whole world (off = only inside its shape)", "bool", yesno(cl->global));
            if (!cl->global) {
                out += field("climate.shape", "Shape", "select", cl->shape, options_of({{"box", "Box"}, {"sphere", "Sphere"}}));
                out += field("climate.size_x", cl->shape == "sphere" ? "Radius (m)" : "Width (m)", "float", number(cl->size.x), "1");
                if (cl->shape != "sphere") {
                    out += field("climate.size_y", "Height (m)", "float", number(cl->size.y), "1");
                    out += field("climate.size_z", "Depth (m)", "float", number(cl->size.z), "1");
                }
                out += field("climate.blend", "Fades out over the edge (m)", "float", number(cl->blend), "0.5");
            }
            out += field("climate.priority", "Priority (higher zones win where they overlap)", "float", std::to_string(cl->priority), "1");
            out += unit_field(orchard::UnitKind::speed, "Speed unit");
            out += field("climate.relative", "Temperature adds to the world's (off = sets it)", "bool", yesno(cl->relative));
            out += field("climate.temperature", "Temperature (C)", "float", number(cl->temperature), "1");
            out += field("climate.wind_speed", "Wind (m/s)", "float", number(cl->wind_speed), "0.5");
            out += field("climate.wind_x", "Wind direction X", "float", number(cl->wind_dir.x), "0.1");
            out += field("climate.wind_z", "Wind direction Z", "float", number(cl->wind_dir.z), "0.1");
            out += field("climate.gust", "Gusts (m/s extra)", "float", number(cl->gust), "0.5");
            out += field("climate.rain", "Rain (0 to 1)", "slider", number(cl->rain), "0.05");
            out += field("climate.snow", "Snow (0 to 1)", "slider", number(cl->snow), "0.05");
            out += field("climate.fog", "Fog (0 to 1)", "slider", number(cl->fog), "0.05");
            out += field("climate.humidity", "Humidity (0 to 1)", "slider", number(cl->humidity), "0.05");
            out += field("climate.lightning", "Lightning strikes per minute", "float", number(cl->lightning), "0.5");
            out += field("climate.delay", "Starts after (s)", "float", number(cl->delay), "1");
            out += field("climate.fade_in", "Builds up over (s)", "float", number(cl->fade_in), "1");
            out += field("climate.duration", "Lasts (s, -1 = forever)", "float", number(cl->duration), "1");
            out += field("climate.fade_out", "Dies down over (s)", "float", number(cl->fade_out), "1");
            out += field("climate.loop", "Starts again when it ends", "bool", yesno(cl->loop));
            out += field("climate.after", "Then switches to (weather preset, empty = stops)", "string", cl->after);
            out += field("climate.visuals", "Shows rain, snow and lightning", "bool", yesno(cl->visuals));
        }
        {
            std::string joined;
            for (const std::string& t : m_scene.tags(e)) joined += (joined.empty() ? "" : ", ") + t;
            out += field("tags", "Tags (comma separated, scripts find things by tag)", "string", joined);
        }

        group("AI");
        const orchard::Agent* ag = m_scene.agent(e);
        out += field("has_agent", "Has an AI brain (moves, sees, hears, fights)", "bool", yesno(ag != nullptr));
        if (ag) {
            std::vector<std::pair<std::string, std::string>> brains;
            for (const std::string& b : orchard::Agent::brains()) brains.emplace_back(b, orchard::Agent::brain_label(b));
            out += field("agent.brain", "Brain", "select", ag->brain, options_of(brains), "Picking a brain resets the settings below to that brain's.");
            std::vector<std::pair<std::string, std::string>> behaviors = {{"", "The brain's own behavior"}};
            for (const std::string& b : orchard::Agent::brains())
                if (b != ag->brain && b != "custom") behaviors.emplace_back(b, std::string("Another brain's: ") + b);
            if (m_project)
                for (const std::string& f : m_project->behaviors()) behaviors.emplace_back(f, f);
            if (!ag->behavior.empty()) {
                bool listed = false;
                for (const auto& b : behaviors) listed = listed || b.first == ag->behavior;
                if (!listed) behaviors.emplace_back(ag->behavior, ag->behavior);
            }
            out += field("agent.behavior", "Behavior tree", "select", ag->behavior, options_of(behaviors), "A .bt file or a .brain graph in Behaviors/ uses your own behavior tree.");
            out += field("agent_copy_behavior", "Copy this behavior to a file to edit", "button", "1");
            out += field("agent_brain_graph", orchard::graph::is_brain_file(ag->behavior) ? "Open its brain graph" : "Edit this behavior as a graph", "button", "1");
            out += field("agent.enabled", "On", "bool", yesno(ag->enabled));
            out += field("agent.team", "Team", "string", ag->team, std::string(), "Agents on the same team are friends. Players are on the team 'player'.");
            out += field("agent.targets", "Only hunts (teams or tags, comma separated)", "string", ag->targets);
            out += unit_field(orchard::UnitKind::speed, "Speed unit");
            out += unit_field(orchard::UnitKind::distance, "Distance unit");
            out += unit_field(orchard::UnitKind::impulse, "Knockback unit");
            out += field("agent.walk_speed", "Walk speed (m/s)", "float", number(ag->walk_speed), "0.1");
            out += field("agent.run_speed", "Run speed (m/s)", "float", number(ag->run_speed), "0.1");
            out += field("agent.turn_speed", "Turn speed (degrees/s)", "float", number(ag->turn_speed), "10");
            out += field("agent.sight_range", "Sees this far (m)", "float", number(ag->sight_range), "1");
            out += field("agent.sight_angle", "Field of view (degrees)", "float", number(ag->sight_angle), "5");
            out += field("agent.eye_height", "Eye height (m)", "float", number(ag->eye_height), "0.1");
            out += field("agent.hearing", "Hearing (1 = normal)", "float", number(ag->hearing), "0.1");
            out += field("agent.awareness", "Notices you this fast (1 = normal)", "float", number(ag->awareness), "0.1");
            out += field("agent.reaction", "Reaction time (s)", "float", number(ag->reaction), "0.05");
            out += field("agent.ranged", "Shoots (off = melee)", "bool", yesno(ag->ranged));
            out += field("agent.attack_range", ag->ranged ? "Shooting range (m)" : "Reach (m)", "float", number(ag->attack_range), "0.1");
            out += field("agent.attack_damage", "Damage per hit", "float", number(ag->attack_damage), "1");
            out += field("agent.attack_knockback", "Knockback per hit (N·s)", "float", number(ag->attack_knockback), "5");
            {
                std::vector<std::pair<std::string, std::string>> types;
                for (const std::string& t : orchard::damage_types()) types.emplace_back(t, orchard::damage_type_label(t));
                out += field("agent.damage_type", "Damage type", "select", ag->damage_type, options_of(types));
                std::vector<std::pair<std::string, std::string>> effects{{"", "None"}};
                for (const orchard::EffectDef& d : orchard::effect_presets()) effects.emplace_back(d.name, d.name);
                out += field("agent.hit_effect", "Hits also cause", "select", ag->hit_effect, options_of(effects));
                if (!ag->hit_effect.empty()) {
                    out += field("agent.hit_effect_time", "For (s, negative = forever)", "float", number(ag->hit_effect_time), "0.5");
                    out += field("agent.hit_effect_chance", "Chance per hit (0 to 1)", "slider", number(ag->hit_effect_chance), "0.05");
                }
                out += field("agent.damage_formula", "Damage formula (empty = Damage per hit)", "string", ag->damage_formula, std::string(), "Uses damage, knockback, distance, health, max_health, health_fraction, target_health, target_max_health, target_mass, ranged, random(a, b).");
                out += field("agent.knockback_formula", "Knockback formula (empty = Knockback per hit)", "string", ag->knockback_formula, std::string(), "Same names as the damage formula. Example: knockback * (1 + 2 * (target_health < 20))");
            }
            out += field("agent.attack_rate", ag->ranged ? "Shots per second" : "Hits per second", "float", number(ag->attack_rate), "0.1");
            if (ag->ranged) {
                out += field("agent.accuracy", "Accuracy (0 to 1)", "slider", number(ag->accuracy), "0.05");
                out += field("agent.magazine", "Magazine (0 = never reloads)", "float", std::to_string(ag->magazine), "1");
                out += field("agent.reload_time", "Reload time (s)", "float", number(ag->reload_time), "0.1");
                out += field("agent.shoot_effect", "Muzzle effect", "string", ag->shoot_effect);
            }
            out += field("agent.aggression", "Aggression (0 to 1)", "slider", number(ag->aggression), "0.05");
            out += field("agent.flee_health", "Runs away below this much health (0 to 1)", "slider", number(ag->flee_health), "0.05");
            out += field("agent.patrol", "Patrol route (an entity whose children are the points, or a tag)", "string", ag->patrol);
            out += field("agent.squad", "Squad (shares sightings and takes turns attacking)", "string", ag->squad);
            out += field("agent.leader", "Follows (entity name, empty = the player)", "string", ag->leader);
            out += field("agent.flying", "Flies", "bool", yesno(ag->flying));
            if (ag->flying) out += field("agent.fly_height", "Flies this high (m)", "float", number(ag->fly_height), "0.5");
            out += field("agent.radius", "Body radius (m)", "float", number(ag->radius), "0.05");
            out += field("agent.height", "Body height (m)", "float", number(ag->height), "0.1");
            out += field("agent.step", "Steps up (m)", "float", number(ag->step), "0.05");
            out += field("agent.jump", "Jumps up (m, 0 = never)", "float", number(ag->jump), "0.1");
            out += field("agent.max_drop", "Drops down at most (m)", "float", number(ag->max_drop), "0.5");
            std::vector<std::pair<std::string, std::string>> clips = {{"", "None"}};
            if (m_project)
                for (const std::string& c : m_project->sounds()) clips.emplace_back(c, c);
            out += field("agent.attack_sound", "Attack sound", "select", ag->attack_sound, options_of(clips));
            out += field("agent.alert_sound", "Spotted-you sound", "select", ag->alert_sound, options_of(clips));
        }

        group("Sound");
        const orchard::SoundSource* snd = m_scene.sound(e);
        out += field("has_sound", "Plays a sound", "bool", yesno(snd != nullptr));
        if (snd) {
            std::vector<std::pair<std::string, std::string>> clips = {{"", "None (pick one, or put sounds in Assets or Sounds)"}};
            if (m_project)
                for (const std::string& c : m_project->sounds()) clips.emplace_back(c, c);
            bool listed = snd->clip.empty();
            for (const auto& c : clips) listed = listed || c.first == snd->clip;
            if (!listed) clips.emplace_back(snd->clip, snd->clip);
            out += field("sound.clip", "Sound file", "select", snd->clip, options_of(clips), "WAV, OGG, MP3, FLAC and Opus files from Assets or Sounds.");
            out += field("sound_preview", "Play it now", "button", "1");
            out += field("sound.volume", "Volume", "slider", number(snd->volume), "0.05");
            out += field("sound.pitch", "Pitch (speed)", "float", number(snd->pitch), "0.05");
            out += field("sound.loop", "Loops", "bool", yesno(snd->loop));
            out += field("sound.autoplay", "Starts when Play starts", "bool", yesno(snd->autoplay));
            out += field("sound.spatial", "Comes from this entity (quieter far away)", "bool", yesno(snd->spatial));
            if (snd->spatial) {
                out += unit_field(orchard::UnitKind::distance, "Distance unit");
                out += field("sound.range", "Heard up to (m)", "float", number(snd->range), "1");
                out += field("sound.min_distance", "Full volume within (m)", "float", number(snd->min_distance), "0.5");
            }
            out += field("sound.ai_hears", "AI can hear it", "bool", yesno(snd->ai_hears));
            if (snd->ai_hears) out += field("sound.loudness", "How far AI hears it (m)", "float", number(snd->loudness), "1");
        }

        if (!flat && m_scene.colliders(e) && !m_scene.colliders(e)->empty()) {
            group("Solid modelling");
            const orchard::Solid* sd = m_scene.solid(e);
            out += field("solid.negative", "Negative (cut out of unions)", "bool", yesno(sd && sd->negative), "", "Ctrl+click parts to pick several, then right-click > Union (Ctrl+Shift+G) or Intersect (Ctrl+Shift+I). Negative parts are cut out of the result. While playing a negative part is hidden and nothing hits it.");
            if (sd && !sd->parts.empty()) {
                out += field("solid.one_color", "One colour (use the first part's material)", "bool", yesno(sd->one_color), "", std::string(sd->operation == orchard::SolidOperation::intersect_parts ? "An intersection" : "A union") + " made from " + sd->parts + ".");
                out += field("solid_separate", "Separate back into parts", "button", "1");
            }
        }

        if (!flat) {
            orchard::editor::ModelView& models = const_cast<orchard::editor::GpuView&>(m_gpu).models();
            group("Model");
            const orchard::Model* md = m_scene.model(e);
            out += field("has_model", "3D model (.glb, .gltf or .obj)", "bool", yesno(md != nullptr), "", "What you see. Colliders stay separate: they're what physics uses.");
            orchard::editor::ModelInfo info;
            if (md) {
                info = models.info(md->source);
                std::vector<std::pair<std::string, std::string>> files = {{"", "Pick a model file"}};
                bool listed = md->source.empty();
                if (m_project)
                    for (const std::string& f : m_project->models()) {
                        files.emplace_back(f, f);
                        listed = listed || f == md->source;
                    }
                if (!listed) files.emplace_back(md->source, md->source);
                std::string about;
                if (!info.ok) about = info.error.empty() ? std::string("Import a model with Files > Import, or put it in Assets.") : info.error;
                else {
                    about = std::to_string(info.triangles) + " triangles, " + std::to_string(info.textures) + (info.textures == 1 ? " texture" : " textures");
                    if (!info.clips.empty()) about += ", " + std::to_string(info.clips.size()) + (info.clips.size() == 1 ? " animation" : " animations");
                    if (!info.bones.empty()) about += ", skeleton with " + std::to_string(info.bones.size()) + " bones";
                    const orchard::Vec3 sz = info.high - info.low;
                    about += ". Size " + number(sz.x) + " x " + number(sz.y) + " x " + number(sz.z) + " m.";
                }
                out += field("model.source", "File", "select", md->source, options_of(files), about);
                out += field("model.visible", "Visible", "bool", yesno(md->visible));
                out += field("model.cast_shadows", "Casts shadows", "bool", yesno(md->cast_shadows));
                if (info.ok) {
                    std::string shape = "none";
                    if (const std::vector<orchard::Collider>* cs = m_scene.colliders(e))
                        for (const orchard::Collider& c : *cs) {
                            if (c.source == md->source) shape = c.shape == orchard::ColliderShape::mesh ? "mesh" : "hull";
                            else if (shape == "none") shape = c.shape == orchard::ColliderShape::capsule ? "capsule" : c.shape == orchard::ColliderShape::box ? "box" : "custom";
                        }
                    std::vector<std::pair<std::string, std::string>> fits = {{"none", "None (walk through it)"}, {"box", "Box around it (fast)"}, {"capsule", "Capsule around it (characters)"}, {"hull", "Wrapped shape (convex hull)"}, {"mesh", "Exact shape (static things, maps)"}};
                    if (shape == "custom") fits.emplace_back("custom", "Custom colliders");
                    out += field("model.collision", "Collision", "select", shape, options_of(fits), "Box and wrapped shapes can move. The exact shape is for things that never move, like buildings and maps.");
                    if (!info.bones.empty()) {
                        out += field("model.hitboxes", "Add hitboxes on the skeleton", "select", "", options_of({{"", "Choose..."}, {"basic", "One capsule"}, {"head-and-body", "Head and body"}, {"full-body", "Head, torso, arms, legs"}, {"detailed", "Every limb, bones and organs"}}), "Colliders that follow the animated bones, for hit detection.");
                        out += field("model.attach_parts", "Attach child colliders to the nearest bones", "button", "1");
                    }
                }
                if (info.ok && !info.clips.empty()) {
                    group("Animation");
                    const orchard::Animator* an = m_scene.animator(e);
                    out += field("has_animator", "Plays animations", "bool", yesno(an != nullptr));
                    if (an) {
                        std::vector<std::pair<std::string, std::string>> clips;
                        float length = 0.0f;
                        for (const orchard::editor::ModelClip& c : info.clips) {
                            clips.emplace_back(c.name, c.name + " (" + number(c.duration) + " s)");
                            if (c.name == an->clip || (an->clip.empty() && length == 0.0f)) length = c.duration;
                        }
                        out += field("anim_clip", "Animation", "select", an->clip.empty() ? info.clips.front().name : an->clip, options_of(clips));
                        out += field("anim_playing", "Playing", "bool", yesno(an->playing));
                        out += field("anim_loop", "Loop", "bool", yesno(an->loop));
                        out += field("anim_speed", "Speed (1 = normal, below 0 = backwards)", "float", number(an->speed), "0.1");
                        out += field("anim_blend", "Blend into the next one (s)", "float", number(an->blend), "0.05");
                        out += field("anim_start", "Starts at (s); also the pose shown in the editor", "float", number(an->start), "0.05", "The animation is " + number(length) + " s long.");
                        std::string evs;
                        const std::string current = an->clip.empty() ? info.clips.front().name : an->clip;
                        for (const orchard::AnimationEvent& ev : an->events)
                            if (ev.clip == current) evs += (evs.empty() ? "" : ", ") + number(ev.time) + " " + ev.name;
                        out += field("anim_events", "Events in this animation (time name, ...)", "string", evs, "", "e.g. 0.3 footstep, 0.8 footstep. Scripts read them with animation_event(entity).");
                        out += field("anim_preview", "Preview in the editor (also previews particles)", "bool", yesno(pref("preview_effects", "0") == "1"));
                    }
                }
            }
            const orchard::Entity owner = [&]() {
                for (orchard::Entity at = m_scene.parent(e); m_scene.alive(at); at = m_scene.parent(at))
                    if (m_scene.model(at)) return at;
                return orchard::Entity{};
            }();
            if (m_scene.alive(owner)) {
                const orchard::editor::ModelInfo oi = models.info(m_scene.model(owner)->source);
                if (oi.ok && !oi.nodes.empty()) {
                    std::vector<std::pair<std::string, std::string>> bones = {{"", "Nothing (stays where it is)"}};
                    for (const std::string& b : oi.bones.empty() ? oi.nodes : oi.bones) bones.emplace_back(b, b);
                    out += field("bone", "Follows bone of " + clean(m_scene.name(owner)), "select", m_scene.bone(e), options_of(bones), "It keeps its offset from the bone and moves with the animation while playing.");
                }
            }
        }

        if (!flat) {
            group("Terrain");
            const orchard::Terrain* tr = m_scene.terrain(e);
            out += field("has_terrain", "Terrain (ground with hills you can sculpt)", "bool", yesno(tr != nullptr));
            if (tr) {
                std::vector<std::pair<std::string, std::string>> presets = {{"", "Pick a preset..."}};
                for (const std::string& p : orchard::terrain_presets()) presets.emplace_back(p, p);
                out += field("terrain.preset", "Quick pick", "select", "", options_of(presets));
                std::vector<std::pair<std::string, std::string>> gens;
                for (const std::string& g : orchard::terrain_generators()) gens.emplace_back(g, g);
                out += field("terrain.generator", "Shape", "select", tr->generator, options_of(gens), "Changing it regenerates the heights and replaces sculpting.");
                out += field("terrain.seed", "Seed", "float", std::to_string(tr->seed), "1");
                out += field("terrain.roughness", "Roughness", "slider", number(tr->roughness), "0.05");
                out += field("terrain.size", "Size (width, max height, depth in m)", "vec3", vec_text(tr->size), "1");
                out += field("terrain.resolution", "Detail (samples per side)", "select", std::to_string(tr->resolution), options_of({{"65", "65"}, {"129", "129"}, {"257", "257"}, {"513", "513"}, {"1025", "1025 (large worlds)"}, {"2049", "2049 (huge, slow)"}}));
                out += field("terrain.friction", "Friction", "float", number(tr->friction), "0.05");
                out += field("terrain.bounce", "Bounce", "slider", number(tr->restitution), "0.05");
                out += field("terrain.heightmap", "Heightmap image (16-bit PNG, PGM or .r16 in Assets)", "string", tr->heightmap);
                out += field("terrain_import", "Import the heightmap (replaces sculpting)", "button", "1");
                out += field("terrain_regenerate", "Regenerate from the shape and seed", "button", "1");
                out += field("terrain.data", "Sculpted heights file (saved with the project)", "string", tr->data);
                for (std::size_t i = 0; i < tr->layers.size(); ++i) {
                    const orchard::TerrainLayer& l = tr->layers[i];
                    const std::string b = "terrain.layer:" + std::to_string(i) + ":";
                    group("Terrain layer " + std::to_string(i + 1) + (i == 0 ? " (base)" : ""));
                    std::vector<std::pair<std::string, std::string>> layer_images = {{"", "None (just the colour)"}};
                    for (const orchard::editor::BuiltinTerrainTexture& bt : orchard::editor::builtin_terrain_textures()) layer_images.emplace_back(bt.name, std::string("Built-in: ") + bt.label);
                    if (m_project)
                        for (const std::string& img : m_project->images()) layer_images.emplace_back(img, img);
                    out += field(b + "texture", "Texture", "select", l.texture, options_of(layer_images), "Built-in textures come with their own bumps. The colour tints the texture (white keeps it as it is).");
                    out += field(b + "normal_map", "Bump map (normal map, optional)", "string", l.normal_map);
                    out += field(b + "color", "Color", "color", hex_color(l.color));
                    out += field(b + "tiling", "Texture repeats every (m)", "float", number(l.tiling), "0.5");
                    if (i > 0) {
                        out += field(b + "min_height", "From height (0 to 1)", "slider", number(l.min_height), "0.01");
                        out += field(b + "max_height", "To height (0 to 1)", "slider", number(l.max_height), "0.01");
                        out += field(b + "min_slope", "From slope (degrees)", "float", number(l.min_slope), "1");
                        out += field(b + "max_slope", "To slope (degrees)", "float", number(l.max_slope), "1");
                        out += field(b + "blend", "Blend", "slider", number(l.blend), "0.01");
                    }
                    out += field("terrain_remove_layer", "Remove this layer", "button", std::to_string(i));
                }
                out += field("terrain_add_layer", "Add a texture layer", "button", "1");
                for (std::size_t i = 0; i < tr->scatter.size(); ++i) {
                    const orchard::TerrainScatter& sc = tr->scatter[i];
                    const std::string b = "terrain.scatter:" + std::to_string(i) + ":";
                    group("Scatter " + std::to_string(i + 1));
                    out += field(b + "model", "Model (glTF or OBJ)", "string", sc.model);
                    out += field(b + "density", "How many per square metre", "float", number(sc.density), "0.001");
                    out += field(b + "min_scale", "Smallest size", "float", number(sc.min_scale), "0.05");
                    out += field(b + "max_scale", "Biggest size", "float", number(sc.max_scale), "0.05");
                    out += field(b + "min_height", "From height (0 to 1)", "slider", number(sc.min_height), "0.01");
                    out += field(b + "max_height", "To height (0 to 1)", "slider", number(sc.max_height), "0.01");
                    out += field(b + "max_slope", "Steepest slope (degrees)", "float", number(sc.max_slope), "1");
                    out += field(b + "align", "Lean with the slope", "bool", yesno(sc.align_to_slope));
                    out += field(b + "collide", "Solid (the player bumps into it)", "bool", yesno(sc.collide));
                    out += field(b + "draw_distance", "Draw distance (m)", "float", number(sc.draw_distance), "10");
                    out += field(b + "seed", "Seed", "float", std::to_string(sc.seed), "1");
                    out += field("terrain_remove_scatter", "Remove this scatter", "button", std::to_string(i));
                }
                out += field("terrain_add_scatter", "Scatter models (trees, rocks...)", "button", "1");
            }
        }

        group("Particles");
        const orchard::Emitter* pe = m_scene.emitter(e);
        out += field("has_emitter", "Particle emitter (sparks, smoke, fire...)", "bool", yesno(pe != nullptr));
        if (!pe) {
            int inside = 0;
            for (orchard::Entity c : m_scene.subtree(e))
                if (c != e && m_scene.emitter(c)) ++inside;
            if (inside > 0)
                out += field("effect_look", "Look of all " + std::to_string(inside) + " particle emitters inside", "select", group_particle_look(e), options_of({{"mixed", "Mixed"}, {"plain", "Plain (soft dots, fastest)"}, {"simple", "Simple (the older look)"}, {"better", "Better (softer, more detailed)"}}));
        }
        if (pe) {
            std::vector<std::pair<std::string, std::string>> ppresets = {{"", "Custom"}};
            for (const std::string& p : orchard::Emitter::presets()) ppresets.emplace_back(p, p);
            out += field("emitter.preset", "Preset", "select", pe->preset, options_of(ppresets));
            {
                std::vector<std::pair<std::string, std::string>> looks = orchard::editor::particle_look_options(orchard::editor::particle_look_for(pe->texture, pe->preset));
                const std::string look = orchard::editor::particle_look_of(*pe);
                if (look == "custom") looks.emplace_back("custom", "Custom texture");
                out += field("emitter.look", "Look", "select", look, options_of(looks), "Plain is a soft dot. Simple is the look particles had before. Better uses the newer soft smoke, thin steam and flame textures.");
            }
            out += field("emitter.flat", "Lies flat on the ground (scorch marks, puddles)", "bool", yesno(pe->flat));
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
            for (orchard::EmitterShape sh : orchard::emitter_shapes()) shapes.emplace_back(orchard::to_string(sh), sh == orchard::EmitterShape::fluid_surface ? std::string("the water's surface") : sh == orchard::EmitterShape::parts ? std::string("its colliders (pick parts)") : std::string(orchard::to_string(sh)));
            out += field("emitter.shape", "Comes from", "select", orchard::to_string(pe->shape), options_of(shapes));
            if (pe->shape == orchard::EmitterShape::parts) {
                std::string names;
                for (orchard::Entity c : m_scene.subtree(e))
                    if (m_scene.colliders(c) && !m_scene.colliders(c)->empty()) names += (names.empty() ? "" : ", ") + m_scene.name(c);
                out += field("emitter.part", "Only these parts (names, comma separated; empty = all)", "string", pe->part, "", names.empty() ? std::string("This entity and its children have no colliders yet.") : "Parts: " + names);
            } else if (pe->shape != orchard::EmitterShape::point && pe->shape != orchard::EmitterShape::fluid_surface) {
                out += field("emitter.shape_size", "Shape size (m)", "vec3", vec_text(pe->shape_size), "0.1");
            }
            out += field("emitter.gravity", "Gravity (1 = normal, below 0 rises)", "float", number(pe->gravity), "0.05");
            out += field("emitter.drag", "Air drag", "float", number(pe->drag), "0.05");
            out += field("emitter.wind", "Wind (m/s)", "vec3", vec_text(pe->wind), "0.1");
            out += field("emitter.turbulence", "Swirl", "float", number(pe->turbulence), "0.1");
            out += field("emitter.size", "Size at start (m)", "float", number(pe->size), "0.01");
            out += field("emitter.size_end", "Size at the end (m)", "float", number(pe->size_end), "0.01");
            out += field("emitter.color", "Color at start", "color", hex_color(pe->color));
            out += field("emitter.use_mid", "Middle color", "bool", yesno(pe->color_mid_at >= 0.0f), "", "A third color part way through each particle's life (fire: white, then orange, then dark red).");
            if (pe->color_mid_at >= 0.0f) {
                out += field("emitter.color_mid", "Color in the middle", "color", hex_color(pe->color_mid));
                out += field("emitter.color_mid_at", "Middle color at (0 to 1 of its life)", "slider", number(pe->color_mid_at), "0.05");
            }
            out += field("emitter.color_end", "Color at the end", "color", hex_color(pe->color_end));
            out += field("emitter.variation", "Brightness varies between particles (0 to 1)", "slider", number(pe->variation), "0.05");
            out += field("emitter.alpha", "See-through at start (1 = solid)", "slider", number(pe->alpha), "0.05");
            out += field("emitter.alpha_end", "See-through at the end", "slider", number(pe->alpha_end), "0.05");
            out += field("emitter.glow", "Glow", "float", number(pe->glow), "0.25");
            out += field("emitter.stretch", "Stretch along movement", "float", number(pe->stretch), "0.1");
            out += field("emitter.fade_in", "Fade in over (0 to 1 of its life)", "slider", number(pe->fade_in), "0.05", "Particles appear gently instead of popping in. Smoke and steam look better with a little.");
            out += field("emitter.grow", "Grows fast then slows (0 = steady)", "slider", number(pe->grow), "0.05", "How quickly it reaches its end size. Smoke and explosions puff out quickly then drift.");
            out += field("emitter.spin", "Spin (degrees/s, random each way)", "float", number(pe->spin), "5");
            out += field("emitter.start_delay", "Starts after (s)", "float", number(pe->start_delay), "0.05");
            out += field("emitter.soft", "Soft edges where it meets walls and floors", "select", pe->soft, options_of({{"auto", "Automatic (smoke, fire, steam, dust)"}, {"on", "On"}, {"off", "Off"}}));
            if (pe->soft != "off") out += field("emitter.soft_distance", "Soft edge depth (m, 0 = automatic)", "float", number(pe->soft_distance), "0.05");
            out += field("emitter.blend", "Blend", "select", orchard::to_string(pe->blend), options_of({{"additive", "Adds light (sparks, fire)"}, {"alpha", "Covers (smoke, dust)"}}));
            out += field("emitter.lit", "Lit by the scene's lights", "bool", yesno(pe->lit));
            out += field("emitter.collide", "Bounces off things", "bool", yesno(pe->collide));
            if (pe->collide) {
                out += field("emitter.bounce", "Bounce", "slider", number(pe->bounce), "0.05");
                out += field("emitter.friction", "Friction", "float", number(pe->friction), "0.05");
                out += field("emitter.die_on_hit", "Disappear when they hit", "bool", yesno(pe->die_on_hit));
            }
            std::vector<std::pair<std::string, std::string>> images = {{"", "Automatic (the preset's built-in texture)"}};
            for (const orchard::editor::BuiltinParticleTexture& b : orchard::editor::builtin_particle_textures()) images.emplace_back(b.name, std::string("Built-in: ") + b.label);
            if (m_project)
                for (const std::string& img : m_project->images()) images.emplace_back(img, img);
            out += field("emitter.texture", "Texture", "select", pe->texture, options_of(images), "Built-in textures animate and rotate by themselves. Images come from the project's Assets folder.");
            if (!pe->texture.empty() && !orchard::editor::is_builtin_particle_texture(pe->texture))
                out += field("emitter.frames", "Frames in the texture (flipbook)", "float", std::to_string(pe->frames), "1", "A strip or a grid of square frames, played over each particle's life. Columns and rows are worked out from the image's shape.");
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
            out += field("fluid.reflectivity", "Reflects (0 = none, 1 = mirror)", "slider", number(w->reflectivity), "0.05");
            out += field("fluid.reflections", "Reflections", "select", orchard::to_string(w->reflections), reflection_options());
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
            out += field("material.reflections", "Reflections", "select", orchard::to_string(m->reflections), reflection_options());
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
        if (key.rfind("units.", 0) == 0) {
            for (orchard::UnitKind k : orchard::unit_kinds())
                if (unit_pref_key(k) == key && orchard::find_unit(k, value)) {
                    m_prefs[key] = value;
                    save_prefs();
                    return std::string("rebuild");
                }
            return std::string();
        }
        if (const std::optional<orchard::UnitKind> uk = unit_kind_of(key)) {
            char* end = nullptr;
            const double typed = std::strtod(value.c_str(), &end);
            if (end != value.c_str()) return inspect_set_si(e, key, number(static_cast<float>(orchard::to_si(typed, *uk, unit_of(*uk)))));
        }
        return inspect_set_si(e, key, value);
    }

    std::string inspect_set_si(orchard::Entity e, const std::string& key, const std::string& value) {
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
        if (key == "character_style") {
            if (!orchard::set_character_style(m_scene, e, value)) return std::string();
            ensure_character_models();
            m_light_dirty = true;
            return "rebuild";
        }
        if (key == "character_colliders") {
            const std::optional<orchard::HumanoidKind> hk = orchard::parse_humanoid_kind(value);
            if (!hk || !orchard::set_humanoid_colliders(m_scene, e, *hk)) return std::string();
            if (is_physical_player(e)) {
                make_player_colliders(e);
                for (orchard::Entity c : m_scene.children(e))
                    if (m_scene.camera(c)) m_scene.transform(c)->position.y = *hk == orchard::HumanoidKind::detailed || *hk == orchard::HumanoidKind::full_body ? 1.64f : m_scene.camera(c)->eye_height;
            }
            m_light_dirty = true;
            return "rebuild";
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
            if (size) return ok ? "rebuild" : "";
            return ok ? "ok" : "";
        }
        if (key.rfind("collider:", 0) == 0 && ends(":shape")) return set_component(e, key, value) ? "rebuild" : "";
        if (key.rfind("collider:", 0) == 0 && key.find(":tri_") != std::string::npos) return set_component(e, key, value) ? "rebuild" : "";
        if (key.rfind("collider:", 0) == 0 && (ends(":mass") || ends(":density") || key.find(":lock_") != std::string::npos)) return set_component(e, key, value) ? "rebuild" : "";
        if (key == "v0" || key == "spin0" || key == "accel") return set_component(e, key, value) ? "ok" : "";
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
                w->reflectivity = keep.reflectivity;
                w->reflections = keep.reflections;
                return "rebuild";
            }
            if (k == "color") return parse_hex_color(value, w->color) ? "ok" : "";
            if (k == "size") return parse_vec3(value, w->size) ? "ok" : "";
            if (k == "current") return parse_vec3(value, w->current) ? "ok" : "";
            if (k == "infinite") {
                w->infinite = on;
                return "ok";
            }
            if (k == "reflections") {
                const auto rm = orchard::parse_reflection_mode(value);
                if (!rm) return std::string();
                w->reflections = *rm;
                return "ok";
            }
            if (k == "reflectivity") {
                w->reflectivity = std::clamp(v, 0.0f, 1.0f);
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
        if (key == "has_terrain") {
            if (on) {
                if (!m_scene.terrain(e)) m_scene.add_terrain(e) = orchard::terrain_preset("hills");
            } else {
                m_scene.remove_terrain(e);
            }
            return "rebuild";
        }
        if (key == "terrain_import") {
            const orchard::Terrain* tr = m_scene.terrain(e);
            if (!tr || tr->heightmap.empty()) return std::string();
            std::string error;
            if (!m_gpu.terrain().import_heightmap(m_scene, e, tr->heightmap, &error)) return "error\t" + error;
            m_dirty = true;
            return "ok";
        }
        if (key == "terrain_regenerate") return m_gpu.terrain().regenerate(m_scene, e) ? "ok" : "";
        if (key.rfind("terrain", 0) == 0) {
            orchard::Terrain* tr = m_scene.terrain(e);
            if (!tr) return std::string();
            if (key == "terrain_add_layer") {
                orchard::TerrainLayer l;
                if (!tr->layers.empty()) l.min_slope = 35.0f;
                tr->layers.push_back(l);
                return "rebuild";
            }
            if (key == "terrain_remove_layer" || key == "terrain_remove_scatter") {
                const std::size_t i = static_cast<std::size_t>(std::atoi(value.c_str()));
                if (key == "terrain_remove_layer" && i < tr->layers.size()) tr->layers.erase(tr->layers.begin() + static_cast<std::ptrdiff_t>(i));
                else if (key == "terrain_remove_scatter" && i < tr->scatter.size()) tr->scatter.erase(tr->scatter.begin() + static_cast<std::ptrdiff_t>(i));
                else return std::string();
                return "rebuild";
            }
            if (key == "terrain_add_scatter") {
                orchard::TerrainScatter sc;
                sc.seed = static_cast<std::uint32_t>(tr->scatter.size() + 1);
                tr->scatter.push_back(sc);
                return "rebuild";
            }
            if (key.rfind("terrain.layer:", 0) == 0 || key.rfind("terrain.scatter:", 0) == 0) {
                const bool layer = key.rfind("terrain.layer:", 0) == 0;
                const std::string rest = key.substr(layer ? 14 : 16);
                const std::size_t colon = rest.find(':');
                if (colon == std::string::npos) return std::string();
                const std::size_t i = static_cast<std::size_t>(std::atoi(rest.substr(0, colon).c_str()));
                const std::string k = rest.substr(colon + 1);
                if (layer) {
                    if (i >= tr->layers.size()) return std::string();
                    orchard::TerrainLayer& l = tr->layers[i];
                    if (k == "texture") l.texture = value;
                    else if (k == "normal_map") l.normal_map = value;
                    else if (k == "color") return parse_hex_color(value, l.color) ? "ok" : "";
                    else if (k == "tiling") l.tiling = std::max(0.01f, v);
                    else if (k == "min_height") l.min_height = v;
                    else if (k == "max_height") l.max_height = v;
                    else if (k == "min_slope") l.min_slope = v;
                    else if (k == "max_slope") l.max_slope = v;
                    else if (k == "blend") l.blend = std::max(0.0f, v);
                    else return std::string();
                    return "ok";
                }
                if (i >= tr->scatter.size()) return std::string();
                orchard::TerrainScatter& sc = tr->scatter[i];
                if (k == "model") sc.model = value;
                else if (k == "density") sc.density = std::max(0.0f, v);
                else if (k == "min_scale") sc.min_scale = std::max(0.001f, v);
                else if (k == "max_scale") sc.max_scale = std::max(0.001f, v);
                else if (k == "min_height") sc.min_height = v;
                else if (k == "max_height") sc.max_height = v;
                else if (k == "max_slope") sc.max_slope = v;
                else if (k == "align") sc.align_to_slope = on;
                else if (k == "collide") sc.collide = on;
                else if (k == "draw_distance") sc.draw_distance = std::max(0.0f, v);
                else if (k == "seed") sc.seed = static_cast<std::uint32_t>(std::max(0.0f, v));
                else return std::string();
                return "ok";
            }
            const std::string k = key.substr(8);
            if (k == "preset") {
                if (value.empty()) return std::string();
                const orchard::Terrain keep = *tr;
                *tr = orchard::terrain_preset(value);
                tr->data = keep.data;
                return "rebuild";
            }
            if (k == "generator") tr->generator = value;
            else if (k == "seed") tr->seed = static_cast<std::uint32_t>(std::max(0.0f, v));
            else if (k == "roughness") tr->roughness = std::clamp(v, 0.0f, 1.0f);
            else if (k == "size") {
                if (!parse_vec3(value, vec)) return std::string();
                tr->size = {std::max(1.0f, vec.x), vec.y, std::max(1.0f, vec.z)};
            } else if (k == "resolution") tr->resolution = orchard::clamp_terrain_resolution(std::atoi(value.c_str()));
            else if (k == "friction") tr->friction = std::max(0.0f, v);
            else if (k == "bounce") tr->restitution = std::max(0.0f, v);
            else if (k == "heightmap") tr->heightmap = value;
            else if (k == "data") tr->data = value;
            else return std::string();
            return "ok";
        }
        if (key == "has_model") {
            if (on) {
                if (!m_scene.model(e)) {
                    orchard::Model& md = m_scene.add_model(e);
                    if (m_project && !m_project->models().empty()) md.source = m_project->models().front();
                }
            } else {
                m_scene.remove_model(e);
                m_scene.remove_animator(e);
            }
            return "rebuild";
        }
        if (key.rfind("model.", 0) == 0) {
            orchard::Model* md = m_scene.model(e);
            if (!md) return std::string();
            const std::string k = key.substr(6);
            if (k == "source") {
                const std::string old = md->source;
                md->source = value;
                if (std::vector<orchard::Collider>* cs = m_scene.colliders(e))
                    for (orchard::Collider& c : *cs)
                        if (c.source == old) c.source = value;
                if (const fizmo::graphics::Model3D* model = m_gpu.models().model(value))
                    if (!model->animations.empty() && !m_scene.animator(e)) m_scene.add_animator(e);
                if (orchard::Animator* an = m_scene.animator(e)) an->clip.clear();
                m_light_dirty = true;
                return "rebuild";
            }
            if (k == "visible") md->visible = on;
            else if (k == "cast_shadows") md->cast_shadows = on;
            else if (k == "collision") {
                if (value == "custom") return std::string();
                fit_model_collider(e, value);
                m_light_dirty = true;
                return "rebuild";
            } else if (k == "hitboxes") {
                const std::optional<orchard::HumanoidKind> hk = orchard::parse_humanoid_kind(value);
                if (!hk) return std::string();
                add_character_hitboxes(e, *hk);
                return "rebuild";
            } else if (k == "attach_parts") {
                attach_parts_to_bones(e);
                return "rebuild";
            }
            m_light_dirty = true;
            return "ok";
        }
        if (key == "bone") {
            m_scene.set_bone(e, value);
            if (!value.empty() && m_scene.colliders(e) && !m_scene.colliders(e)->empty()) {
                if (!m_scene.body(e)) m_scene.add_body(e).type = orchard::BodyType::kinematic;
            }
            return "rebuild";
        }
        if (key == "has_animator") {
            if (on) m_scene.add_animator(e);
            else m_scene.remove_animator(e);
            return "rebuild";
        }
        if (key == "anim_preview") {
            m_prefs["preview_effects"] = on ? "1" : "0";
            save_prefs();
            m_preview_start = std::chrono::steady_clock::now();
            m_gpu.models().reset_animation();
            set_animating(wants_animation());
            redraw();
            return "ok";
        }
        if (key.rfind("anim_", 0) == 0) {
            orchard::Animator* an = m_scene.animator(e);
            if (!an) return std::string();
            const std::string k = key.substr(5);
            if (k == "clip") {
                an->clip = value;
                an->start = 0.0f;
                ++an->restart;
                return "rebuild";
            }
            if (k == "playing") an->playing = on;
            else if (k == "loop") {
                an->loop = on;
                ++an->restart;
            } else if (k == "speed") an->speed = v;
            else if (k == "blend") an->blend = std::max(0.0f, v);
            else if (k == "start") {
                an->start = std::max(0.0f, v);
                ++an->restart;
            } else if (k == "events") {
                std::string current = an->clip;
                if (current.empty())
                    if (const orchard::Model* md = m_scene.model(e)) {
                        const orchard::editor::ModelInfo info = m_gpu.models().info(md->source);
                        if (!info.clips.empty()) current = info.clips.front().name;
                    }
                an->events.erase(std::remove_if(an->events.begin(), an->events.end(), [&current](const orchard::AnimationEvent& ev) { return ev.clip == current; }), an->events.end());
                std::string part;
                std::istringstream in(value);
                while (std::getline(in, part, ',')) {
                    std::istringstream one(part);
                    orchard::AnimationEvent ev;
                    ev.clip = current;
                    if (!(one >> ev.time >> ev.name)) continue;
                    an->events.push_back(ev);
                }
            }
            redraw();
            return "ok";
        }
        if (key == "has_agent") {
            if (on) {
                if (!m_scene.agent(e)) m_scene.add_agent(e) = orchard::Agent::preset("guard").value_or(orchard::Agent{});
                if (!m_scene.health(e)) m_scene.add_health(e);
            } else {
                m_scene.remove_agent(e);
            }
            return "rebuild";
        }
        if (key == "solid.negative") {
            orchard::Solid& sd = m_scene.add_solid(e);
            sd.negative = on;
            if (!sd.negative && sd.operation == orchard::SolidOperation::none && sd.parts.empty()) m_scene.remove_solid(e);
            touched();
            return "rebuild";
        }
        if (key == "solid.one_color") {
            const orchard::SolidResult r = orchard::rebuild_solid(m_scene, e, solid_options(on));
            for (const std::string& n : r.notes) add_log(n, false);
            if (!r.ok()) {
                add_log("Can't remake " + m_scene.name(e) + ": " + r.error, true);
                return std::string();
            }
            touched();
            return "rebuild";
        }
        if (key == "solid_separate") {
            const std::string r = solid_action(e, "solid:separate");
            if (r.rfind("error", 0) == 0) {
                add_log(r.substr(6), true);
                return std::string();
            }
            return "rebuild";
        }
        if (key == "has_sound") {
            if (on) {
                if (!m_scene.sound(e)) m_scene.add_sound(e);
            } else {
                m_scene.remove_sound(e);
            }
            return "rebuild";
        }
        if (key == "sound_preview") {
            if (const orchard::SoundSource* snd = m_scene.sound(e)) {
                if (m_project) m_sounds.set_folder(m_project->folder());
                m_sounds.preview(snd->clip, snd->volume);
                if (!m_sounds.error().empty()) add_log("Sound: " + m_sounds.error(), true);
            }
            return std::string("ok");
        }
        if (key == "agent_brain_graph") {
            orchard::Agent* ag = m_scene.agent(e);
            if (!ag || !m_project) return std::string();
            if (orchard::graph::is_brain_file(ag->behavior)) return "graph\t" + ag->behavior;
            std::string text;
            if (!ag->behavior.empty() && ag->behavior.find('.') != std::string::npos) {
                std::ifstream in(m_project->path_of(ag->behavior));
                std::stringstream ss;
                ss << in.rdbuf();
                text = ss.str();
            }
            if (text.empty()) text = orchard::ai::brain_behavior(ag->behavior.empty() ? ag->brain : ag->behavior);
            if (text.empty()) text = "selector\n  idle\n";
            std::string why;
            const std::optional<orchard::graph::Graph> g = orchard::graph::Graph::from_behavior(text, &why);
            if (!g) {
                add_log("Can't turn that behavior into a graph: " + why, true);
                return std::string();
            }
            std::string base = m_scene.name(e);
            for (char& c : base)
                if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
            std::error_code ec;
            std::filesystem::create_directories(m_project->path_of("Behaviors"), ec);
            std::string rel = "Behaviors/" + base + ".brain";
            for (int i = 2; std::filesystem::exists(m_project->path_of(rel), ec); ++i) rel = "Behaviors/" + base + std::to_string(i) + ".brain";
            std::ofstream(m_project->path_of(rel)) << g->save();
            ag->behavior = rel;
            add_log("Wrote " + rel + ": change it in the graph tab and press Play to use it", false);
            return "graph\t" + rel;
        }
        if (key == "agent_copy_behavior") {
            orchard::Agent* ag = m_scene.agent(e);
            if (!ag || !m_project) return std::string();
            std::string text;
            if (orchard::graph::is_brain_file(ag->behavior)) {
                std::ifstream in(m_project->path_of(ag->behavior));
                std::stringstream ss;
                ss << in.rdbuf();
                text = orchard::graph::brain_file_behavior(ss.str()).value_or(std::string());
            } else if (!ag->behavior.empty() && ag->behavior.find('.') != std::string::npos) {
                std::ifstream in(m_project->path_of(ag->behavior));
                std::stringstream ss;
                ss << in.rdbuf();
                text = ss.str();
            }
            if (text.empty()) text = orchard::ai::brain_behavior(ag->behavior.empty() ? ag->brain : ag->behavior);
            std::string base = m_scene.name(e);
            for (char& c : base)
                if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
            std::error_code ec;
            std::filesystem::create_directories(m_project->path_of("Behaviors"), ec);
            std::string rel = "Behaviors/" + base + ".bt";
            for (int i = 2; std::filesystem::exists(m_project->path_of(rel), ec); ++i) rel = "Behaviors/" + base + std::to_string(i) + ".bt";
            std::ofstream(m_project->path_of(rel)) << text;
            ag->behavior = rel;
            add_log("Wrote " + rel + ": edit it and press Play to use it", false);
            return "rebuild";
        }
        if (key.rfind("agent.", 0) == 0) {
            orchard::Agent* ag = m_scene.agent(e);
            if (!ag) return std::string();
            const std::string k = key.substr(6);
            if (k == "brain") {
                const std::optional<orchard::Agent> p = orchard::Agent::preset(value);
                if (!p) return std::string();
                const std::string patrol = ag->patrol, squad = ag->squad;
                *ag = *p;
                ag->patrol = patrol;
                if (!squad.empty()) ag->squad = squad;
                return std::string("rebuild");
            }
            auto f = [&](float& target, float lo, float hi) {
                target = std::clamp(v, lo, hi);
                return std::string("ok");
            };
            if (k == "behavior") {
                ag->behavior = value;
                return std::string("ok");
            }
            if (k == "enabled") ag->enabled = on;
            else if (k == "team") ag->team = value;
            else if (k == "targets") ag->targets = value;
            else if (k == "walk_speed") return f(ag->walk_speed, 0.0f, 100.0f);
            else if (k == "run_speed") return f(ag->run_speed, 0.0f, 100.0f);
            else if (k == "turn_speed") return f(ag->turn_speed, 0.0f, 5000.0f);
            else if (k == "sight_range") return f(ag->sight_range, 0.0f, 10000.0f);
            else if (k == "sight_angle") return f(ag->sight_angle, 0.0f, 360.0f);
            else if (k == "eye_height") return f(ag->eye_height, -100.0f, 100.0f);
            else if (k == "hearing") return f(ag->hearing, 0.0f, 100.0f);
            else if (k == "awareness") return f(ag->awareness, 0.0f, 100.0f);
            else if (k == "reaction") return f(ag->reaction, 0.0f, 10.0f);
            else if (k == "ranged") {
                ag->ranged = on;
                return std::string("rebuild");
            } else if (k == "attack_range") return f(ag->attack_range, 0.0f, 10000.0f);
            else if (k == "attack_damage") return f(ag->attack_damage, -100000.0f, 100000.0f);
            else if (k == "attack_knockback") return f(ag->attack_knockback, 0.0f, 100000.0f);
            else if (k == "damage_type") ag->damage_type = value;
            else if (k == "hit_effect") {
                ag->hit_effect = value;
                return std::string("rebuild");
            } else if (k == "hit_effect_time") ag->hit_effect_time = v;
            else if (k == "hit_effect_chance") return f(ag->hit_effect_chance, 0.0f, 1.0f);
            else if (k == "damage_formula") ag->damage_formula = value;
            else if (k == "knockback_formula") ag->knockback_formula = value;
            else if (k == "attack_rate") return f(ag->attack_rate, 0.0f, 100.0f);
            else if (k == "accuracy") return f(ag->accuracy, 0.0f, 1.0f);
            else if (k == "magazine") ag->magazine = std::max(0, std::atoi(value.c_str()));
            else if (k == "reload_time") return f(ag->reload_time, 0.0f, 60.0f);
            else if (k == "shoot_effect") ag->shoot_effect = value;
            else if (k == "aggression") return f(ag->aggression, 0.0f, 1.0f);
            else if (k == "flee_health") return f(ag->flee_health, 0.0f, 1.0f);
            else if (k == "patrol") ag->patrol = value;
            else if (k == "squad") ag->squad = value;
            else if (k == "leader") ag->leader = value;
            else if (k == "flying") {
                ag->flying = on;
                return std::string("rebuild");
            } else if (k == "fly_height") return f(ag->fly_height, -1000.0f, 1000.0f);
            else if (k == "radius") return f(ag->radius, 0.05f, 20.0f);
            else if (k == "height") return f(ag->height, 0.1f, 50.0f);
            else if (k == "step") return f(ag->step, 0.0f, 5.0f);
            else if (k == "jump") return f(ag->jump, 0.0f, 20.0f);
            else if (k == "max_drop") return f(ag->max_drop, 0.0f, 100.0f);
            else if (k == "attack_sound") ag->attack_sound = value;
            else if (k == "alert_sound") ag->alert_sound = value;
            else return std::string();
            return std::string("ok");
        }
        if (key.rfind("sound.", 0) == 0) {
            orchard::SoundSource* snd = m_scene.sound(e);
            if (!snd) return std::string();
            const std::string k = key.substr(6);
            if (k == "clip") snd->clip = value;
            else if (k == "volume") snd->volume = std::clamp(v, 0.0f, 4.0f);
            else if (k == "pitch") snd->pitch = std::clamp(v, 0.05f, 8.0f);
            else if (k == "loop") snd->loop = on;
            else if (k == "autoplay") snd->autoplay = on;
            else if (k == "spatial") {
                snd->spatial = on;
                return std::string("rebuild");
            } else if (k == "range") snd->range = std::max(0.1f, v);
            else if (k == "min_distance") snd->min_distance = std::max(0.0f, v);
            else if (k == "ai_hears") {
                snd->ai_hears = on;
                return std::string("rebuild");
            } else if (k == "loudness") snd->loudness = std::max(0.0f, v);
            else return std::string();
            return std::string("ok");
        }
        if (key == "has_health") {
            if (on) {
                if (!m_scene.health(e)) m_scene.add_health(e);
            } else {
                m_scene.remove_health(e);
            }
            return "rebuild";
        }
        if (key == "has_survival") {
            if (on) {
                if (!m_scene.survival(e)) m_scene.add_survival(e);
            } else {
                m_scene.remove_survival(e);
            }
            return "rebuild";
        }
        if (key.rfind("survival.", 0) == 0) {
            orchard::Survival* sv = m_scene.survival(e);
            if (!sv) return std::string();
            const std::string k = key.substr(9);
            if (bool* b = orchard::survival_switch(*sv, k)) {
                *b = on;
                return std::string("rebuild");
            }
            float* x = orchard::survival_value(*sv, k);
            if (!x) return std::string();
            *x = v;
            return std::string("ok");
        }
        if (key == "has_inventory") {
            if (on) {
                if (!m_scene.inventory(e)) orchard::apply_inventory_preset(m_scene.add_inventory(e), "minecraft");
            } else {
                m_scene.remove_inventory(e);
            }
            return "rebuild";
        }
        if (key.rfind("inventory.", 0) == 0) {
            orchard::Inventory* inv = m_scene.inventory(e);
            if (!inv) return std::string();
            const std::string k = key.substr(10);
            if (k == "preset") return orchard::apply_inventory_preset(*inv, value) ? std::string("rebuild") : std::string();
            if (k == "start_items") {
                inv->items.clear();
                std::string cur;
                for (char ch : value + ",") {
                    if (ch == ',') {
                        const std::size_t a = cur.find_first_not_of(" \t"), b = cur.find_last_not_of(" \t");
                        if (a != std::string::npos) {
                            const std::optional<orchard::ItemStack> st = orchard::parse_stack(cur.substr(a, b - a + 1));
                            if (st && !st->item.empty() && st->amount > 0.0f) orchard::inventory_add(m_scene.items(), *inv, st->item, st->amount);
                        }
                        cur.clear();
                    } else {
                        cur += ch;
                    }
                }
                return std::string("ok");
            }
            const bool rebuild = k == "style" || k == "hotbar" || k == "container_mode";
            if (!orchard::set_inventory_field(*inv, k, value)) return std::string();
            return rebuild ? std::string("rebuild") : std::string("ok");
        }
        if (key == "has_pickup") {
            if (on) {
                if (!m_scene.pickup(e)) {
                    orchard::Pickup& pk = m_scene.add_pickup(e);
                    pk.stack.item = "apple";
                    pk.stack.amount = 1.0f;
                }
            } else {
                m_scene.remove_pickup(e);
            }
            return "rebuild";
        }
        if (key.rfind("pickup.", 0) == 0) {
            orchard::Pickup* pk = m_scene.pickup(e);
            if (!pk) return std::string();
            const std::string k = key.substr(7);
            if (k == "item") pk->stack.item = value;
            else if (k == "amount") pk->stack.amount = std::max(0.0f, v);
            else if (k == "auto") pk->auto_pickup = on;
            else if (k == "respawn") pk->respawn = std::max(0.0f, v);
            else if (k == "look") pk->look = value;
            else return std::string();
            return std::string("ok");
        }
        if (key == "has_climate") {
            if (on) {
                if (!m_scene.climate(e)) orchard::apply_climate_preset(m_scene.add_climate(e), "rain");
            } else {
                m_scene.remove_climate(e);
            }
            return "rebuild";
        }
        if (key.rfind("climate.", 0) == 0) {
            orchard::Climate* cl = m_scene.climate(e);
            if (!cl) return std::string();
            const std::string k = key.substr(8);
            const std::string val = (k == "global" || k == "relative" || k == "loop" || k == "active" || k == "visuals") ? std::string(on ? "1" : "0") : value;
            if (k == "preset") return orchard::apply_climate_preset(*cl, value) ? std::string("rebuild") : std::string();
            if (float* f = orchard::climate_value(*cl, k)) {
                *f = v;
                return std::string("ok");
            }
            if (k == "global") cl->global = on;
            else if (k == "relative") cl->relative = on;
            else if (k == "loop") cl->loop = on;
            else if (k == "active") cl->active = on;
            else if (k == "visuals") cl->visuals = on;
            else if (k == "shape") cl->shape = val;
            else if (k == "after") cl->after = val;
            else if (k == "priority") cl->priority = static_cast<int>(v);
            else return std::string();
            return std::string(k == "global" || k == "shape" ? "rebuild" : "ok");
        }
        if (key == "tags") {
            const std::vector<std::string> old = m_scene.tags(e);
            for (const std::string& t : old) m_scene.remove_tag(e, t);
            std::string cur;
            for (char ch : value + ",") {
                if (ch == ',') {
                    const std::size_t a = cur.find_first_not_of(" \t");
                    const std::size_t b = cur.find_last_not_of(" \t");
                    if (a != std::string::npos) m_scene.add_tag(e, cur.substr(a, b - a + 1));
                    cur.clear();
                } else {
                    cur += ch;
                }
            }
            return std::string("ok");
        }
        if (key.rfind("health.", 0) == 0) {
            orchard::Health* h = m_scene.health(e);
            if (!h) return std::string();
            const std::string k = key.substr(7);
            if (k == "value") h->value = std::max(0.0f, v);
            else if (k == "max") h->max = std::max(0.0f, v);
            else if (k == "regen") {
                h->regen = v;
                return std::string("rebuild");
            } else if (k == "regen_delay") h->regen_delay = std::max(0.0f, v);
            else if (k == "invulnerable") h->invulnerable = on;
            else if (k == "impact_damage") h->impact_damage = std::max(0.0f, v);
            else if (k == "flammable") {
                h->flammable = on;
                return std::string("rebuild");
            } else if (k == "burn_damage") h->burn_damage = std::max(0.0f, v);
            else if (k == "on_death") {
                if (const auto a = orchard::parse_death_action(value)) h->on_death = *a;
                return std::string("rebuild");
            } else if (k == "explode_radius") h->explode_radius = std::max(0.1f, v);
            else if (k == "explode_force") h->explode_force = std::max(0.0f, v);
            else if (k == "explode_damage") h->explode_damage = std::max(0.0f, v);
            else if (k == "respawn_delay") h->respawn_delay = std::max(0.0f, v);
            else if (k == "armor") h->armor = std::max(0.0f, v);
            else if (k == "armor_mode") {
                if (std::find(orchard::armor_modes().begin(), orchard::armor_modes().end(), value) == orchard::armor_modes().end()) return std::string();
                h->armor_mode = value;
                return std::string("rebuild");
            } else if (k == "armor_constant") h->armor_constant = std::max(0.001f, v);
            else if (k == "armor_cap") h->armor_cap = std::clamp(v, 0.0f, 1.0f);
            else if (k == "armor_min") h->armor_min = std::clamp(v, 0.0f, 1.0f);
            else if (k == "armor_wear") h->armor_wear = std::max(0.0f, v);
            else if (k == "armor_types") h->armor_types = value;
            else if (k == "armor_formula") h->armor_formula = value;
            else if (k == "max_shield") {
                const bool had = h->max_shield > 0.0f;
                h->max_shield = std::max(0.0f, v);
                if (!had) h->shield = h->max_shield;
                h->shield = std::min(h->shield, h->max_shield);
                if (had != (h->max_shield > 0.0f)) return std::string("rebuild");
            } else if (k == "shield") h->shield = std::clamp(v, 0.0f, h->max_shield);
            else if (k == "shield_regen") h->shield_regen = v;
            else if (k == "shield_delay") h->shield_delay = std::max(0.0f, v);
            else if (k == "shield_absorb") h->shield_absorb = std::clamp(v, 0.0f, 1.0f);
            else if (k == "shield_types") h->shield_types = value;
            else if (k == "shield_formula") h->shield_formula = value;
            else if (k == "resist") h->resist = orchard::parse_resist(value);
            else if (k == "poise") h->poise = std::max(0.0f, v);
            else if (k == "stagger_scale") h->stagger_scale = std::max(0.0f, v);
            else if (k == "damage_formula") h->damage_formula = value;
            else if (k == "effect_formula") h->effect_formula = value;
            else return std::string();
            return std::string("ok");
        }
        if (key == "has_camera") {
            if (on) {
                if (!m_scene.camera(e)) m_scene.add_camera(e);
            } else {
                m_scene.remove_camera(e);
            }
            return "rebuild";
        }
        if (key == "camera_from_view") {
            orchard::Transform* t = m_scene.transform(e);
            if (!t) return std::string();
            if (m_scene.dimension() == orchard::Dimension::two_d) {
                t->position = orchard::Vec3{m_view.pan_camera().center.x, m_view.pan_camera().center.y, t->position.z};
                if (orchard::Camera* c = m_scene.camera(e)) c->view_height = static_cast<float>(m_view.height()) / std::max(1.0f, m_view.pan_camera().pixels_per_unit);
            } else {
                const orchard::Vec3 eye = m_view.eye();
                t->position = eye;
                t->rotation = camera_rotation(m_view.orbit_camera().target - eye);
                if (orchard::Camera* c = m_scene.camera(e)) c->fov = m_view.orbit_camera().fov;
            }
            return "rebuild";
        }
        if (key.rfind("camera.", 0) == 0) {
            orchard::Camera* c = m_scene.camera(e);
            if (!c) return std::string();
            const std::string k = key.substr(7);
            if (k == "active") {
                c->active = on;
                if (on)
                    for (orchard::Entity other : m_scene.entities())
                        if (other != e)
                            if (orchard::Camera* oc = m_scene.camera(other)) oc->active = false;
            } else if (k == "control") {
                const auto cc = orchard::parse_camera_control(value);
                if (!cc) return std::string();
                c->control = *cc;
                return std::string("rebuild");
            } else if (k == "move_speed") c->move_speed = std::max(0.0f, v);
            else if (k == "sprint_speed") c->sprint_speed = std::clamp(v, 0.0f, 500.0f);
            else if (k == "look_speed") c->look_speed = std::max(0.0f, v);
            else if (k == "eye_height") c->eye_height = std::max(0.0f, v);
            else if (k == "jump_speed") c->jump_speed = std::clamp(v, 0.0f, 100.0f);
            else if (k == "mouse_look") c->mouse_look = on;
            else if (k == "priority") c->priority = std::atoi(value.c_str());
            else if (k == "fov") c->fov = std::clamp(v, 1.0f, 179.0f);
            else if (k == "near_clip") c->near_clip = std::max(0.0001f, v);
            else if (k == "far_clip") c->far_clip = std::max(c->near_clip + 0.01f, v);
            else if (k == "view_height") c->view_height = std::max(0.01f, v);
            else return std::string();
            return "ok";
        }
        if (key == "effect_look") {
            if (value == "mixed") return std::string();
            for (orchard::Entity c : m_scene.subtree(e))
                if (orchard::Emitter* ce = m_scene.emitter(c); ce && !ce->flat) orchard::editor::set_particle_look(*ce, value);
            return std::string("rebuild");
        }
        if (key == "has_emitter") {
            if (on) {
                if (!m_scene.emitter(e)) {
                    orchard::Emitter& made = m_scene.add_emitter(e);
                    if (m_scene.fluid(e)) {
                        if (const std::optional<orchard::Emitter> steam = orchard::Emitter::named("steam")) made = *steam;
                        made.shape = orchard::EmitterShape::fluid_surface;
                    } else if (m_scene.colliders(e) && !m_scene.colliders(e)->empty()) {
                        made.shape = orchard::EmitterShape::parts;
                    }
                }
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
                if (!keep.texture.empty() && !orchard::editor::is_builtin_particle_texture(keep.texture)) {
                    pe->texture = keep.texture;
                    pe->frames = keep.frames;
                }
                if (keep.shape == orchard::EmitterShape::fluid_surface || keep.shape == orchard::EmitterShape::parts) {
                    pe->shape = keep.shape;
                    pe->part = keep.part;
                }
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
            if (k == "soft") {
                pe->soft = value == "on" || value == "off" ? value : std::string("auto");
                return std::string("rebuild");
            }
            if (k == "soft_distance") {
                pe->soft_distance = std::max(0.0f, v);
                return std::string("ok");
            }
            if (k == "local_space") return flag(pe->local_space, false);
            if (k == "color") return parse_hex_color(value, pe->color) ? "ok" : "";
            if (k == "color_end") return parse_hex_color(value, pe->color_end) ? "ok" : "";
            if (k == "color_mid") return parse_hex_color(value, pe->color_mid) ? "ok" : "";
            if (k == "use_mid") {
                const bool use = value == "1" || value == "true";
                if (use && pe->color_mid_at < 0.0f) {
                    pe->color_mid_at = 0.4f;
                    pe->color_mid = (pe->color + pe->color_end) * 0.5f;
                } else if (!use) {
                    pe->color_mid_at = -1.0f;
                }
                return "rebuild";
            }
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
            if (k == "look") return orchard::editor::set_particle_look(*pe, value) ? std::string("rebuild") : std::string();
            if (k == "flat") {
                pe->flat = on;
                return std::string("ok");
            }
            if (k == "texture") {
                auto image = [](const std::string& t) { return !t.empty() && !orchard::editor::is_builtin_particle_texture(t); };
                const bool had = image(pe->texture);
                pe->texture = value;
                return had != image(pe->texture) ? "rebuild" : "ok";
            }
            if (k == "on_death") {
                pe->on_death = value;
                return "ok";
            }
            if (k == "part") {
                pe->part = value;
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
            else if (k == "color_mid_at") target = &pe->color_mid_at;
            else if (k == "fade_in") target = &pe->fade_in;
            else if (k == "grow") target = &pe->grow;
            else if (k == "spin") target = &pe->spin;
            else if (k == "variation") target = &pe->variation;
            else if (k == "start_delay") target = &pe->start_delay;
            if (!target) return std::string();
            float x = v;
            if (k != "gravity" && k != "speed" && k != "inherit_velocity") x = std::max(0.0f, v);
            if (k == "alpha" || k == "alpha_end" || k == "bounce" || k == "color_mid_at" || k == "grow" || k == "variation") x = std::clamp(v, 0.0f, 1.0f);
            if (k == "fade_in") x = std::clamp(v, 0.0f, 0.9f);
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
            if (k == "reflections") {
                const auto rm = orchard::parse_reflection_mode(value);
                if (!rm) return std::string();
                m->reflections = *rm;
                return "ok";
            }
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

    orchard::Vec3 camera_forward_axis() const {
        return orchard::normalize(m_view.axis_system().from_render(orchard::Vec3{0.0f, 0.0f, -1.0f}));
    }

    orchard::Vec3 camera_up_axis() const {
        return orchard::normalize(m_view.axis_system().from_render(orchard::Vec3{0.0f, 1.0f, 0.0f}));
    }

    static orchard::Quat quat_from_basis(orchard::Vec3 x, orchard::Vec3 y, orchard::Vec3 z) {
        const float m00 = x.x, m10 = x.y, m20 = x.z, m01 = y.x, m11 = y.y, m21 = y.z, m02 = z.x, m12 = z.y, m22 = z.z;
        orchard::Quat q;
        const float tr = m00 + m11 + m22;
        if (tr > 0.0f) {
            const float s = std::sqrt(tr + 1.0f) * 2.0f;
            q.w = 0.25f * s;
            q.x = (m21 - m12) / s;
            q.y = (m02 - m20) / s;
            q.z = (m10 - m01) / s;
        } else if (m00 > m11 && m00 > m22) {
            const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
            q.w = (m21 - m12) / s;
            q.x = 0.25f * s;
            q.y = (m01 + m10) / s;
            q.z = (m02 + m20) / s;
        } else if (m11 > m22) {
            const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
            q.w = (m02 - m20) / s;
            q.x = (m01 + m10) / s;
            q.y = 0.25f * s;
            q.z = (m12 + m21) / s;
        } else {
            const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
            q.w = (m10 - m01) / s;
            q.x = (m02 + m20) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25f * s;
        }
        return orchard::normalize(q);
    }

    orchard::Quat camera_rotation(orchard::Vec3 look) const {
        const orchard::Vec3 up = camera_up_axis();
        orchard::Vec3 f = orchard::normalize(look);
        if (orchard::length(f) < 0.5f) f = camera_forward_axis();
        orchard::Vec3 r = orchard::cross(f, up);
        if (orchard::length(r) < 1e-4f) r = orchard::cross(f, camera_forward_axis());
        r = orchard::normalize(r);
        const orchard::Vec3 u = orchard::cross(r, f);
        const orchard::Vec3 f0 = camera_forward_axis();
        const orchard::Vec3 r0 = orchard::normalize(orchard::cross(f0, up));
        const orchard::Vec3 u0 = orchard::cross(r0, f0);
        const orchard::Quat target = quat_from_basis(r, u, f * -1.0f);
        const orchard::Quat base = quat_from_basis(r0, u0, f0 * -1.0f);
        const orchard::Quat base_inv{-base.x, -base.y, -base.z, base.w};
        return orchard::normalize(target * base_inv);
    }

    std::string reflection_options() const {
        std::vector<std::pair<std::string, std::string>> opts;
        for (orchard::ReflectionMode rm : {orchard::ReflectionMode::highlights, orchard::ReflectionMode::surroundings, orchard::ReflectionMode::realistic}) opts.emplace_back(orchard::to_string(rm), orchard::reflection_label(rm));
        return options_of(opts);
    }

    orchard::Vec3 look_direction(float yaw, float pitch) const {
        const float y = orchard::radians(yaw), p = orchard::radians(pitch);
        return orchard::normalize(m_view.axis_system().from_render(orchard::Vec3{-std::sin(y) * std::cos(p), std::sin(p), -std::cos(y) * std::cos(p)}));
    }

    void start_player_camera() {
        m_player_ready = false;
        const orchard::Entity cam = m_scene.active_camera();
        const orchard::Camera* c = m_scene.camera(cam);
        if (!c || c->control == orchard::CameraControl::none) return;
        if (m_view.dimension() != orchard::Dimension::three_d) {
            m_player_fall = 0.0f;
            m_player_ready = true;
            return;
        }
        const orchard::Mat4 w = m_scene.world_matrix(cam);
        orchard::Vec3 f = orchard::normalize(orchard::mul_direction(w, camera_forward_axis()));
        if (orchard::length(f) < 0.5f) f = camera_forward_axis();
        const orchard::Vec3 fr = orchard::normalize(m_view.axis_system().to_render(f));
        m_player_pitch = orchard::degrees(std::asin(std::clamp(fr.y, -0.999f, 0.999f)));
        m_player_yaw = orchard::degrees(std::atan2(-fr.x, -fr.z));
        m_player_fall = 0.0f;
        m_player_ready = true;
    }

    // ----- Items, inventory screens, hotbar and weapons while playing -----
    struct HudDrag {
        bool on = false;
        orchard::Entity owner;
        std::string list;
        int index = -1;
        std::string item;
        float start_x = 0.0f, start_y = 0.0f;
        bool rotate = false;
        int button = 1;
        bool shift = false;
    };
    bool m_inv_open = false;
    bool m_bags_open = false;
    orchard::Entity m_inv_other;
    HudDrag m_hud_drag;
    bool m_use_down = false;
    std::set<std::string> m_play_down;
    float m_mouse_x = 0.0f, m_mouse_y = 0.0f;
    std::set<std::string> m_item_models_checked;
    std::map<std::string, std::shared_ptr<fizmo::images::BitmapImage>> m_item_icons;

    orchard::Entity play_me() const {
        const orchard::Entity cam = m_scene.active_camera();
        const orchard::Entity rider = m_scene.body_owner(cam);
        return m_scene.alive(rider) ? rider : cam;
    }

    orchard::Entity play_owner() {
        orchard::Gameplay& g = m_session.scripts().gameplay();
        orchard::Entity o = g.inventory_owner(play_me());
        if (!m_scene.alive(o)) o = g.inventory_owner(m_scene.active_camera());
        return o;
    }

    bool screen_open() const { return m_session.playing() && (m_inv_open || m_bags_open); }

    static bool key_in(const std::string& keys, const std::string& name) {
        const std::string want = orchard::ScriptRuntime::key_name(name);
        std::string cur;
        for (char ch : keys + ",") {
            if (ch == ',') {
                if (!cur.empty() && orchard::ScriptRuntime::key_name(cur) == want) return true;
                cur.clear();
            } else if (ch != ' ') {
                cur += ch;
            }
        }
        return false;
    }

    static int key_index(const std::string& keys, const std::string& name) {
        const std::string want = orchard::ScriptRuntime::key_name(name);
        std::string cur;
        int i = 0;
        for (char ch : keys + ",") {
            if (ch == ',') {
                if (!cur.empty()) {
                    if (orchard::ScriptRuntime::key_name(cur) == want) return i;
                    ++i;
                }
                cur.clear();
            } else if (ch != ' ') {
                cur += ch;
            }
        }
        return -1;
    }

    void close_screen() {
        m_inv_open = m_bags_open = false;
        m_inv_other = orchard::Entity{};
        m_hud_drag = HudDrag{};
    }

    // Where the selected hotbar item lives, so drop and use can find it.
    bool selected_stack(orchard::Entity owner, std::string& list, int& index) {
        orchard::Inventory* inv = m_scene.inventory(owner);
        if (!inv) return false;
        if (!inv->worn["mainhand"].empty()) {
            list = "worn:mainhand";
            index = 0;
            return true;
        }
        const int sel = inv->selected;
        if (inv->style == orchard::InventoryStyle::slots) {
            list = inv->hotbar && inv->hotbar_own ? "hotbar" : "items";
            index = sel;
            orchard::ItemStack* s = m_session.scripts().gameplay().stack_at(owner, list, index);
            return s && !s->empty();
        }
        if (sel < 0 || sel >= static_cast<int>(inv->binds.size())) return false;
        const std::string& id = inv->binds[static_cast<std::size_t>(sel)];
        for (std::size_t i = 0; i < inv->items.size(); ++i)
            if (inv->items[i].item == id && !inv->items[i].empty()) {
                list = "items";
                index = static_cast<int>(i);
                return true;
            }
        return false;
    }

    orchard::Entity nearest_storage(orchard::Entity owner, float radius) {
        orchard::Gameplay& g = m_session.scripts().gameplay();
        const orchard::Vec3 at = g.position(owner);
        orchard::Entity best;
        float best_d = radius;
        for (orchard::Entity e : m_scene.entities()) {
            if (e == owner || !m_scene.inventory(e) || !m_scene.inventory(e)->enabled) continue;
            if (m_scene.agent(e) && m_scene.health(e) && !g.dead(e)) continue;
            const float d = orchard::length(g.position(e) - at);
            if (d < best_d) {
                best_d = d;
                best = e;
            }
        }
        return best;
    }

    // Keys and mouse buttons while playing. Returns true when the inventory screen used it.
    bool play_input(const std::string& name, bool down) {
        if (!m_session.playing()) return false;
        const std::string k = orchard::ScriptRuntime::key_name(name);
        const bool edge = down ? m_play_down.insert(k).second : m_play_down.erase(k) > 0;
        if (!edge) return screen_open();
        const orchard::Entity owner = play_owner();
        orchard::Inventory* inv = m_scene.inventory(owner);
        if (!inv || !inv->enabled) return false;
        orchard::Gameplay& g = m_session.scripts().gameplay();
        if (!down) {
            if (key_in(inv->use_keys, name) && m_use_down) {
                m_use_down = false;
                g.release_held(owner);
            }
            if (key_in(inv->alt_keys, name)) {
                g.set_aiming(owner, false);
                g.set_blocking(owner, false);
            }
            return screen_open();
        }
        if (screen_open()) {
            if (k == "escape" || key_in(inv->open_keys, name) || (m_bags_open && key_in(inv->container_keys, name)) || (m_scene.alive(m_inv_other) && key_in(inv->interact_keys, name))) {
                close_screen();
                redraw();
                return true;
            }
            if (m_hud_drag.on && k == "r") m_hud_drag.rotate = !m_hud_drag.rotate;
            if (const int slot = key_index(inv->hotbar_keys, name); slot >= 0) {
                // Hovering an item and pressing a slot key binds it there, like Minecraft and Tarkov.
                const orchard::HudLayout lay = inventory_screen(owner);
                if (const orchard::HudRect* it = orchard::hud_item_at(lay, m_mouse_x, m_mouse_y); it && !it->other) {
                    if (inv->style == orchard::InventoryStyle::slots && inv->hotbar_own) g.move_item(owner, it->list, it->index, owner, "hotbar", slot);
                    else {
                        if (static_cast<int>(inv->binds.size()) <= slot) inv->binds.resize(static_cast<std::size_t>(slot + 1));
                        inv->binds[static_cast<std::size_t>(slot)] = it->label;
                    }
                }
            }
            redraw();
            return true;
        }
        if (key_in(inv->open_keys, name)) {
            m_inv_open = true;
            m_use_down = false;
            redraw();
            return true;
        }
        if (inv->container_mode != "beside" && key_in(inv->container_keys, name)) {
            m_bags_open = true;
            redraw();
            return true;
        }
        if (const int slot = key_index(inv->hotbar_keys, name); slot >= 0 && inv->hotbar) {
            g.select_slot(owner, slot);
            return false;
        }
        if (key_in(inv->reload_keys, name)) g.reload(owner);
        if (key_in(inv->holster_keys, name)) {
            if (g.held(owner) && !g.held(owner)->empty() && !inv->worn["mainhand"].empty()) g.holster(owner);
            else
                for (const std::string& slot : g.holster_slots(owner))
                    if (const orchard::ItemStack* s = g.stack_at(owner, "holster:" + slot, 0); s && !s->empty()) {
                        g.draw(owner, slot);
                        break;
                    }
        }
        if (key_in(inv->drop_keys, name)) {
            std::string list;
            int index = 0;
            if (selected_stack(owner, list, index)) g.drop_item(owner, list, index, m_session.scripts().key_down("ctrl") ? -1.0f : 1.0f);
        }
        if (key_in(inv->interact_keys, name)) {
            const orchard::Entity near = g.nearest_pickup(owner, 2.5f);
            if (m_scene.alive(near)) {
                g.pick_up(owner, near);
            } else if (const orchard::Entity box = nearest_storage(owner, 2.5f); m_scene.alive(box)) {
                m_inv_other = box;
                m_inv_open = true;
                m_use_down = false;
            }
        }
        if (key_in(inv->use_keys, name)) {
            m_use_down = true;
            g.use_held(owner, true, false, 0.0f);
        }
        if (key_in(inv->alt_keys, name)) {
            const orchard::ItemDef* d = g.held_def(owner);
            if (d && (d->kind == orchard::ItemKind::melee || d->block > 0.0f)) g.set_blocking(owner, true);
            else g.set_aiming(owner, true);
            if (const orchard::ItemDef* off = g.held_def(owner, "offhand"); off && off->block > 0.0f) g.set_blocking(owner, true);
        }
        return false;
    }

    orchard::HudLayout inventory_screen(orchard::Entity owner) {
        const orchard::Inventory* inv = m_scene.inventory(owner);
        if (!inv) return {};
        const bool containers = inv->container_mode != "own_key" || m_bags_open;
        const orchard::Inventory* other = m_scene.alive(m_inv_other) ? m_scene.inventory(m_inv_other) : nullptr;
        const std::string other_title = other ? (other->title.empty() || other->title == "Inventory" ? m_scene.name(m_inv_other) : other->title) : std::string();
        return orchard::inventory_layout(m_scene.items(), *inv, static_cast<float>(m_view.width()), static_cast<float>(m_view.height()), containers, other, other_title);
    }

    bool hud_press(float x, float y, int button, bool shift) {
        m_mouse_x = x;
        m_mouse_y = y;
        if (!screen_open()) return false;
        const orchard::Entity owner = play_owner();
        if (!m_scene.inventory(owner)) return false;
        const orchard::HudLayout lay = inventory_screen(owner);
        const orchard::HudRect* it = orchard::hud_item_at(lay, x, y);
        m_hud_drag = HudDrag{};
        if (it && it->index >= -1) {
            m_hud_drag.on = true;
            m_hud_drag.owner = it->other ? m_inv_other : owner;
            m_hud_drag.list = it->list;
            m_hud_drag.index = it->list.rfind("worn:", 0) == 0 || it->list.rfind("holster:", 0) == 0 ? 0 : it->index;
            m_hud_drag.item = it->label;
            m_hud_drag.start_x = x;
            m_hud_drag.start_y = y;
            m_hud_drag.button = button;
            m_hud_drag.shift = shift;
        }
        redraw();
        return true;
    }

    bool hud_release(float x, float y) {
        if (!screen_open()) return false;
        m_mouse_x = x;
        m_mouse_y = y;
        HudDrag d = m_hud_drag;
        m_hud_drag = HudDrag{};
        if (!d.on) return true;
        const orchard::Entity owner = play_owner();
        orchard::Gameplay& g = m_session.scripts().gameplay();
        orchard::ItemStack* src = g.stack_at(d.owner, d.list, d.index);
        if (!src || src->empty()) return true;
        const float amount = d.button == 2 && src->amount > 1.0f ? std::floor(src->amount * 0.5f) : -1.0f;
        const bool clicked = std::fabs(x - d.start_x) + std::fabs(y - d.start_y) < 5.0f;
        if (clicked) {
            if (d.shift) {
                // Shift-click sends it across: to the open chest, or back from it.
                const orchard::Entity to = d.owner == owner ? m_inv_other : owner;
                if (m_scene.alive(to)) g.move_item(d.owner, d.list, d.index, to, "items", -1, amount);
            } else if (d.button == 2 && d.owner == owner) {
                const orchard::ItemDef* def = g.item_def(src->item);
                if (def && def->wearable() && d.list.rfind("worn:", 0) != 0) g.equip(owner, d.list, d.index);
                else if (def && d.list.rfind("worn:", 0) == 0) g.unequip(owner, d.list.substr(5));
                else g.use_item(owner, d.list, d.index);
            }
            redraw();
            return true;
        }
        const orchard::HudLayout lay = inventory_screen(owner);
        const orchard::HudRect* cell = orchard::hud_cell_at(lay, x, y);
        if (!cell) {
            bool inside = false;
            for (const orchard::HudRect& p : lay.panels) inside = inside || p.contains(x, y);
            if (!inside && d.owner == owner) g.drop_item(owner, d.list, d.index, amount);
            redraw();
            return true;
        }
        const orchard::Entity to = cell->other ? m_inv_other : owner;
        if (cell->list == "bind") {
            orchard::Inventory* inv = m_scene.inventory(owner);
            if (inv && cell->index >= 0) {
                if (static_cast<int>(inv->binds.size()) <= cell->index) inv->binds.resize(static_cast<std::size_t>(cell->index + 1));
                inv->binds[static_cast<std::size_t>(cell->index)] = d.item;
            }
        } else {
            g.move_item(d.owner, d.list, d.index, to, cell->list, cell->index, amount, cell->gx, cell->gy, d.rotate);
        }
        redraw();
        return true;
    }

    void play_tick(float dt) {
        const orchard::Entity owner = play_owner();
        orchard::Gameplay& g = m_session.scripts().gameplay();
        if (m_scene.alive(m_inv_other)) {
            if (orchard::length(g.position(m_inv_other) - g.position(owner)) > 4.0f) close_screen();
        }
        if (!m_scene.inventory(owner)) return;
        const orchard::Entity cam = m_scene.active_camera();
        orchard::Vec3 origin = g.position(cam), dir{0.0f, 0.0f, -1.0f};
        if (m_view.dimension() == orchard::Dimension::three_d) {
            if (m_player_ready) dir = look_direction(m_player_yaw, m_player_pitch);
            else dir = orchard::normalize(orchard::mul_direction(m_scene.world_matrix(cam), camera_forward_axis()));
            const orchard::Mat4 w = m_scene.world_matrix(cam);
            origin = orchard::Vec3{w.at(0, 3), w.at(1, 3), w.at(2, 3)};
        } else {
            // In 2D the player aims at the mouse.
            const orchard::PanCamera& pc = m_view.pan_camera();
            const float ppu = std::max(1e-3f, pc.pixels_per_unit);
            const orchard::Vec3 me = g.position(owner);
            const orchard::Vec3 mouse{pc.center.x + (m_mouse_x - m_view.width() * 0.5f) / ppu, pc.center.y - (m_mouse_y - m_view.height() * 0.5f) / ppu, 0.0f};
            origin = me + orchard::Vec3{0.0f, 0.4f, 0.0f};
            dir = mouse - origin;
            dir.z = 0.0f;
            if (orchard::length(dir) < 1e-4f) dir = orchard::Vec3{1.0f, 0.0f, 0.0f};
            dir = orchard::normalize(dir);
        }
        g.set_aim(owner, origin, dir);
        orchard::ScriptRuntime& keys = m_session.scripts();
        g.set_stance(owner, keys.key_down("ctrl") || keys.key_down("c") ? "crouch" : "stand");
        if (m_use_down && !screen_open()) g.use_held(owner, false, true, dt);
    }

    void ensure_item_models() {
        if (!m_project) return;
        const std::string prefix = "Assets/Models/Items/";
        for (orchard::Entity e : m_scene.entities()) {
            const orchard::Model* md = m_scene.model(e);
            if (!md || md->source.rfind(prefix, 0) != 0 || m_item_models_checked.count(md->source)) continue;
            m_item_models_checked.insert(md->source);
            const std::string path = m_project->path_of(md->source);
            if (fs::exists(path)) continue;
            std::string stem = md->source.substr(prefix.size());
            if (stem.size() > 4) stem.resize(stem.size() - 4);
            const std::size_t dash = stem.rfind('-');
            if (dash == std::string::npos) continue;
            const orchard::ItemDef* def = m_scene.items().find(stem.substr(0, dash));
            if (def) orchard::editor::write_item_model(*def, stem.substr(dash + 1), path);
        }
    }

    const fizmo::images::BitmapImage* item_icon(const orchard::ItemDef& d) {
        if (d.texture.empty() || !m_project) return nullptr;
        auto it = m_item_icons.find(d.texture);
        if (it == m_item_icons.end()) {
            auto img = std::make_shared<fizmo::images::BitmapImage>();
            std::string err;
            if (!fizmo::images::load_image(m_project->path_of(d.texture), *img, &err)) img.reset();
            it = m_item_icons.emplace(d.texture, img).first;
        }
        return it->second.get();
    }

    static fizmo::colors::Color hud_rgb(orchard::Vec3 c, int a = 255) {
        auto b = [](float v) { return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f); };
        return fizmo::colors::Color(b(c.x), b(c.y), b(c.z), static_cast<std::uint8_t>(std::clamp(a, 0, 255)));
    }

    // A small drawing of the item, so it's recognisable without art.
    void draw_item_icon(fizmo::graphics::Renderer& r, const orchard::ItemDef* d, float x, float y, float w, float h, bool rotated) {
        namespace fg = fizmo::graphics;
        if (!d) {
            r.draw_rect(static_cast<int>(x + w * 0.3f), static_cast<int>(y + h * 0.3f), static_cast<unsigned>(w * 0.4f), static_cast<unsigned>(h * 0.4f), fg::Paint::fill(fizmo::colors::Color(200, 60, 60)));
            return;
        }
        if (const fizmo::images::BitmapImage* img = item_icon(*d)) {
            r.draw_image(*img, static_cast<int>(x + 2), static_cast<int>(y + 2), static_cast<unsigned>(std::max(1.0f, w - 4)), static_cast<unsigned>(std::max(1.0f, h - 4)));
            return;
        }
        const fizmo::colors::Color c = hud_rgb(d->color), a = hud_rgb(d->accent), outline(16, 18, 15);
        const float cx = x + w * 0.5f, cy = y + h * 0.5f, s = std::min(w, h);
        const int ix = static_cast<int>(cx), iy = static_cast<int>(cy);
        using K = orchard::ItemKind;
        auto bar = [&](float x0, float y0, float x1, float y1, float thick, fizmo::colors::Color col) {
            if (rotated) {
                std::swap(x0, y0);
                std::swap(x1, y1);
                x0 = cx + (x0 - cy);
                x1 = cx + (x1 - cy);
                y0 = cy + (y0 - cx);
                y1 = cy + (y1 - cx);
            }
            r.draw_line(static_cast<int>(x0), static_cast<int>(y0), static_cast<int>(x1), static_cast<int>(y1), fg::Paint::stroke(col, std::max(1.0f, thick)));
        };
        switch (d->kind) {
        case K::gun:
            bar(x + w * 0.12f, cy - s * 0.06f, x + w * 0.9f, cy - s * 0.06f, s * 0.14f, c);
            bar(x + w * 0.2f, cy, x + w * 0.16f, cy + s * 0.28f, s * 0.12f, a);
            if (d->magazine > 1) bar(x + w * 0.4f, cy, x + w * 0.42f, cy + s * 0.22f, s * 0.08f, c);
            break;
        case K::melee: case K::tool:
            bar(x + w * 0.2f, y + h * 0.8f, x + w * 0.38f, y + h * 0.62f, s * 0.1f, a);
            bar(x + w * 0.38f, y + h * 0.62f, x + w * 0.85f, y + h * 0.15f, d->damage_type == "blunt" ? s * 0.16f : s * 0.08f, c);
            bar(x + w * 0.3f, y + h * 0.52f, x + w * 0.48f, y + h * 0.7f, s * 0.06f, a);
            break;
        case K::bow:
            r.draw_arc(ix, iy, static_cast<unsigned>(s * 0.32f), static_cast<unsigned>(s * 0.4f), fg::Paint::stroke(c, std::max(1.0f, s * 0.07f)));
            bar(cx, y + h * 0.1f, cx, y + h * 0.9f, 1.0f, fizmo::colors::Color(230, 230, 220));
            break;
        case K::potion: case K::drink:
            r.draw_circle(ix, static_cast<int>(cy + s * 0.08f), static_cast<unsigned>(s * 0.25f), fg::Paint::fill(c));
            r.draw_rect(static_cast<int>(cx - s * 0.07f), static_cast<int>(cy - s * 0.35f), static_cast<unsigned>(s * 0.14f), static_cast<unsigned>(s * 0.22f), fg::Paint::fill(fizmo::colors::Color(200, 215, 220, 200)));
            r.draw_circle(ix, static_cast<int>(cy + s * 0.08f), static_cast<unsigned>(s * 0.25f), fg::Paint::stroke(outline, 1));
            break;
        case K::ammo:
            for (int i = -1; i <= 1; ++i) bar(cx + i * s * 0.15f, cy + s * 0.25f, cx + i * s * 0.15f, cy - s * 0.2f, s * 0.09f, c);
            break;
        case K::magic:
            bar(x + w * 0.2f, y + h * 0.85f, x + w * 0.7f, y + h * 0.3f, s * 0.07f, a);
            r.draw_circle(static_cast<int>(x + w * 0.72f), static_cast<int>(y + h * 0.27f), static_cast<unsigned>(s * 0.13f), fg::Paint::fill(c));
            break;
        case K::clothing: case K::armor: case K::container: case K::holster:
            r.draw_polygon({fg::RenderPoint(cx - s * 0.3f, cy - s * 0.25f), fg::RenderPoint(cx + s * 0.3f, cy - s * 0.25f), fg::RenderPoint(cx + s * 0.25f, cy + s * 0.3f), fg::RenderPoint(cx - s * 0.25f, cy + s * 0.3f)}, fg::Paint::fill(c));
            if (d->kind == K::armor) r.draw_rect(static_cast<int>(cx - s * 0.1f), static_cast<int>(cy - s * 0.15f), static_cast<unsigned>(s * 0.2f), static_cast<unsigned>(s * 0.3f), fg::Paint::fill(a));
            break;
        case K::trinket:
            r.draw_circle(ix, iy, static_cast<unsigned>(s * 0.18f), fg::Paint::stroke(c, std::max(1.0f, s * 0.06f)));
            break;
        default:
            r.draw_circle(ix, iy, static_cast<unsigned>(s * 0.26f), fg::Paint::fill(c));
            r.draw_circle(ix, iy, static_cast<unsigned>(s * 0.26f), fg::Paint::stroke(outline, 1));
            break;
        }
    }

    void draw_play_hud(grove::GpuCanvasFrame& f) {
        namespace fg = fizmo::graphics;
        using fizmo::colors::Color;
        fg::Renderer& r = f.renderer;
        orchard::Gameplay& g = m_session.scripts().gameplay();
        const float sw = static_cast<float>(m_view.width()), sh = static_cast<float>(m_view.height());
        auto style = [](float size, Color c) {
            fizmo::text::TextStyle st;
            st.set_size(size);
            st.set_fg_color(c);
            return st;
        };
        auto text = [&](float x, float y, const std::string& s, float size, Color c) { r.draw_text(static_cast<int>(x), static_cast<int>(y), s, style(size, c)); };
        auto text_w = [&](const std::string& s, float size) { return static_cast<float>(r.measure_text(s, style(size, Color(255, 255, 255))).width); };
        auto label = [&](float x, float y, const std::string& s, float size, Color c) {
            r.draw_rect(static_cast<int>(x - 6.0f), static_cast<int>(y - 3.0f), static_cast<unsigned>(text_w(s, size) + 12.0f), static_cast<unsigned>(size + 8.0f), fg::Paint::fill(Color(12, 14, 11, 170)));
            text(x, y, s, size, c);
        };
        auto rect = [&](float x, float y, float w, float h, Color c) { r.draw_rect(static_cast<int>(x), static_cast<int>(y), static_cast<unsigned>(std::max(1.0f, w)), static_cast<unsigned>(std::max(1.0f, h)), fg::Paint::fill(c)); };
        auto frame = [&](float x, float y, float w, float h, Color c, float t = 1.0f) { r.draw_rect(static_cast<int>(x), static_cast<int>(y), static_cast<unsigned>(std::max(1.0f, w)), static_cast<unsigned>(std::max(1.0f, h)), fg::Paint::stroke(c, t)); };

        // Bullets in flight and hitscan tracers, drawn in both 2D and 3D.
        for (const orchard::Tracer& t : g.tracers()) {
            float ax, ay, bx, by;
            if (!m_view.project(t.from, ax, ay) || !m_view.project(t.to, bx, by)) continue;
            const int a = static_cast<int>(255.0f * std::clamp(1.0f - t.age / std::max(0.001f, t.life), 0.0f, 1.0f));
            r.draw_line(static_cast<int>(ax), static_cast<int>(ay), static_cast<int>(bx), static_cast<int>(by), fg::Paint::stroke(hud_rgb(t.color, a), 2));
        }
        for (const orchard::Projectile& p : g.projectiles()) {
            float ax, ay, bx, by;
            const orchard::Vec3 tail = p.position - p.velocity * 0.012f;
            if (!m_view.project(p.position, ax, ay) || !m_view.project(tail, bx, by)) continue;
            r.draw_line(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(ax), static_cast<int>(ay), fg::Paint::stroke(Color(255, 225, 140, 220), 2));
        }

        const orchard::Entity owner = play_owner();
        orchard::Inventory* inv = m_scene.inventory(owner);
        if (!inv || !inv->enabled) return;
        const orchard::ItemCatalog& cat = m_scene.items();
        const int alpha = static_cast<int>(std::clamp(inv->opacity, 0.0f, 1.0f) * 255.0f);
        const Color panel_c(22, 25, 21, alpha), cell_c(52, 58, 49, std::max(alpha, 120)), edge_c(98, 108, 92), sel_c(247, 180, 72), white(236, 240, 230), dim(150, 158, 144);

        auto stack_of = [&](orchard::Entity who, const orchard::HudRect& it) -> const orchard::ItemStack* {
            const int index = it.list.rfind("worn:", 0) == 0 || it.list.rfind("holster:", 0) == 0 ? 0 : it.index;
            return g.stack_at(who, it.list, index);
        };
        auto draw_items = [&](const orchard::HudLayout& lay, bool listy_owner) {
            for (const orchard::HudRect& it : lay.items) {
                const orchard::Entity who = it.other ? m_inv_other : owner;
                if (m_hud_drag.on && m_hud_drag.owner == who && m_hud_drag.list == it.list && (it.list.rfind("worn:", 0) == 0 || m_hud_drag.index == it.index)) continue;
                const orchard::ItemStack* st = it.kind == "bind" ? nullptr : stack_of(who, it);
                const orchard::ItemDef* d = cat.find(it.label);
                const bool row = it.w > it.h * 2.5f;
                if (row) {
                    draw_item_icon(r, d, it.x, it.y, it.h, it.h, false);
                    text(it.x + it.h + 4.0f, it.y + it.h * 0.2f, d ? d->name : it.label, std::max(10.0f, it.h * 0.5f), white);
                    if (st) {
                        std::string right = orchard::amount_text(d, st->amount);
                        if (d && inv->show_weight) right += "   " + orchard::amount_text(nullptr, orchard::stack_weight(cat, *st)) + " kg";
                        text(it.x + it.w - text_w(right, std::max(10.0f, it.h * 0.45f)) - 4.0f, it.y + it.h * 0.22f, right, std::max(10.0f, it.h * 0.45f), dim);
                    }
                    continue;
                }
                rect(it.x + 1, it.y + 1, it.w - 2, it.h - 2, Color(70, 78, 66, std::max(alpha, 140)));
                draw_item_icon(r, d, it.x, it.y, it.w, it.h, st && st->rotated);
                if (st && (st->amount != 1.0f || (d && d->continuous))) {
                    const std::string a = orchard::amount_text(d, st->amount);
                    const float ts = std::max(9.0f, std::min(it.w, it.h) * 0.28f);
                    text(it.x + it.w - text_w(a, ts) - 3.0f, it.y + it.h - ts - 3.0f, a, ts, white);
                }
                if (st && d && d->kind == orchard::ItemKind::gun && d->magazine > 0) {
                    const float k = std::clamp(st->loaded / static_cast<float>(d->magazine), 0.0f, 1.0f);
                    rect(it.x + 2, it.y + it.h - 3, (it.w - 4) * k, 2, Color(240, 200, 90));
                }
                if (st && st->durability >= 0.0f && st->durability < 1.0f) rect(it.x + 2, it.y + 2, (it.w - 4) * std::clamp(st->durability, 0.0f, 1.0f), 2, Color(110, 200, 110));
                static_cast<void>(listy_owner);
            }
        };

        if (!screen_open()) {
            const orchard::HudLayout bar = orchard::hotbar_layout(cat, *inv, sw, sh);
            for (const orchard::HudRect& p : bar.panels) rect(p.x, p.y, p.w, p.h, panel_c);
            for (const orchard::HudRect& c : bar.cells) {
                rect(c.x, c.y, c.w, c.h, cell_c);
                frame(c.x, c.y, c.w, c.h, c.selected ? sel_c : edge_c, c.selected ? 2.0f : 1.0f);
            }
            for (const orchard::HudRect& it : bar.items) {
                const orchard::ItemDef* d = cat.find(it.label);
                draw_item_icon(r, d, it.x, it.y, it.w, it.h, false);
                float amount = 0.0f;
                if (it.kind == "bind") amount = g.count(owner, it.label);
                else if (const orchard::ItemStack* st = g.stack_at(owner, it.list, it.index)) amount = st->amount;
                if (amount != 1.0f || (d && d->continuous)) {
                    const std::string a = orchard::amount_text(d, amount);
                    const float ts = std::max(9.0f, it.w * 0.28f);
                    text(it.x + it.w - text_w(a, ts) - 3.0f, it.y + it.h - ts - 3.0f, a, ts, amount <= 0.0f ? dim : white);
                }
            }
            // What's in hand, ammo and the crosshair.
            const orchard::ItemStack* held = g.held(owner);
            const orchard::ItemDef* hd = held && !held->empty() ? cat.find(held->item) : nullptr;
            if (hd) {
                std::string line = hd->name;
                if (hd->kind == orchard::ItemKind::gun || hd->kind == orchard::ItemKind::bow) {
                    char buf[64];
                    if (hd->magazine > 0) std::snprintf(buf, sizeof(buf), "  %d / %d", static_cast<int>(held->loaded), hd->magazine);
                    else buf[0] = '\0';
                    line += buf;
                    if (!hd->ammo.empty()) line += "  (" + orchard::amount_text(cat.find(hd->ammo), g.count(owner, hd->ammo)) + " spare)";
                }
                if (g.reloading(owner)) line += "  reloading...";
                label(sw - text_w(line, 14.0f) - 16.0f, sh - 34.0f, line, 14.0f, white);
                if (hd->weapon() && m_view.dimension() == orchard::Dimension::three_d) {
                    const orchard::Camera* cam = m_scene.camera(m_scene.active_camera());
                    const float fov = orchard::radians(cam ? cam->fov : 60.0f);
                    const float spread = orchard::radians(std::max(0.05f, hd->spread)) * g.spread_now(owner);
                    const float gap = std::clamp(spread / std::max(0.1f, fov) * sh, 3.0f, sh * 0.25f);
                    const float cx = sw * 0.5f, cy = sh * 0.5f, len = 7.0f;
                    const Color ch(240, 244, 236, 220);
                    for (int i = 0; i < 4; ++i) {
                        const float dx = i == 0 ? 1.0f : i == 1 ? -1.0f : 0.0f, dy = i == 2 ? 1.0f : i == 3 ? -1.0f : 0.0f;
                        r.draw_line(static_cast<int>(cx + dx * gap), static_cast<int>(cy + dy * gap), static_cast<int>(cx + dx * (gap + len)), static_cast<int>(cy + dy * (gap + len)), fg::Paint::stroke(ch, 2));
                    }
                }
            }
            const orchard::Entity near = g.nearest_pickup(owner, 2.5f);
            if (m_scene.alive(near)) {
                const orchard::Pickup* pk = m_scene.pickup(near);
                const orchard::ItemDef* d = pk ? cat.find(pk->stack.item) : nullptr;
                if (pk && !pk->auto_pickup) {
                    std::string first = inv->interact_keys.substr(0, inv->interact_keys.find(','));
                    const std::string tip = "[" + first + "] pick up " + (d ? d->name : pk->stack.item) + (pk->stack.amount != 1.0f ? " x" + orchard::amount_text(d, pk->stack.amount) : "");
                    label((sw - text_w(tip, 14.0f)) * 0.5f, sh * 0.5f + 40.0f, tip, 14.0f, white);
                }
            } else if (const orchard::Entity box = nearest_storage(owner, 2.5f); m_scene.alive(box)) {
                std::string first = inv->interact_keys.substr(0, inv->interact_keys.find(','));
                const std::string tip = "[" + first + "] open " + m_scene.name(box);
                label((sw - text_w(tip, 14.0f)) * 0.5f, sh * 0.5f + 40.0f, tip, 14.0f, white);
            }
            return;
        }

        rect(0, 0, sw, sh, Color(0, 0, 0, 90));
        const orchard::HudLayout lay = inventory_screen(owner);
        for (const orchard::HudRect& p : lay.panels) {
            rect(p.x, p.y, p.w, p.h, panel_c);
            frame(p.x, p.y, p.w, p.h, edge_c);
            text(p.x + 8.0f, p.y + 4.0f, p.label, 13.0f, white);
        }
        // Weight and volume line under the player's own panel.
        if (inv->show_weight) {
            const float wgt = orchard::inventory_weight(cat, *inv);
            char buf[96];
            if (inv->max_weight > 0.0f) std::snprintf(buf, sizeof(buf), "%.1f / %.1f kg", static_cast<double>(wgt), static_cast<double>(inv->max_weight));
            else std::snprintf(buf, sizeof(buf), "%.1f kg", static_cast<double>(wgt));
            std::string line = buf;
            if (inv->style == orchard::InventoryStyle::volume || inv->max_volume > 0.0f) {
                std::snprintf(buf, sizeof(buf), "   %.1f / %.1f L", static_cast<double>(orchard::inventory_volume(cat, *inv)), static_cast<double>(inv->max_volume));
                line += buf;
            }
            for (const orchard::HudRect& p : lay.panels)
                if (!p.other && p.label == inv->title) {
                    text(p.x + p.w - text_w(line, 12.0f) - 8.0f, p.y + 5.0f, line, 12.0f, inv->max_weight > 0.0f && wgt > inv->max_weight ? Color(240, 110, 100) : dim);
                    break;
                }
        }
        for (const orchard::HudRect& t : lay.texts) text(t.x, t.y, t.label, 11.0f, sel_c);
        const orchard::HudRect* hover_cell = orchard::hud_cell_at(lay, m_mouse_x, m_mouse_y);
        for (const orchard::HudRect& c : lay.cells) {
            if (c.w > c.h * 2.5f) {
                if (&c == hover_cell) rect(c.x, c.y, c.w, c.h, Color(80, 90, 74, 160));
                continue;
            }
            const bool hot = &c == hover_cell && m_hud_drag.on;
            rect(c.x, c.y, c.w, c.h, hot ? Color(90, 104, 80, 230) : cell_c);
            frame(c.x, c.y, c.w, c.h, hot ? sel_c : edge_c);
            if ((c.list.rfind("worn:", 0) == 0 || c.list.rfind("holster:", 0) == 0) && orchard::hud_item_at(lay, c.x + 1.0f, c.y + 1.0f) == nullptr) {
                std::string slot = c.list.rfind("holster:", 0) == 0 ? std::string("Holster") : orchard::wear_slot_label(c.label);
                slot = slot.substr(0, slot.find(' '));
                const float ts = std::clamp(c.w * 0.22f, 7.0f, 10.0f);
                while (slot.size() > 2 && text_w(slot, ts) > c.w - 4.0f) slot.pop_back();
                text(c.x + 2.0f, c.y + c.h - ts - 4.0f, slot, ts, dim);
            }
        }
        draw_items(lay, false);
        // Tooltip for whatever is under the mouse.
        if (!m_hud_drag.on)
            if (const orchard::HudRect* it = orchard::hud_item_at(lay, m_mouse_x, m_mouse_y))
                if (const orchard::ItemDef* d = cat.find(it->label)) {
                    std::vector<std::string> lines = {d->name};
                    char buf[160];
                    std::snprintf(buf, sizeof(buf), "%s   %.2f kg   %.2f L", d->category.c_str(), static_cast<double>(d->weight), static_cast<double>(d->volume));
                    lines.push_back(buf);
                    if (d->damage > 0.0f) {
                        std::snprintf(buf, sizeof(buf), "Damage %.0f %s   Rate %.1f/s", static_cast<double>(d->damage), d->damage_type.c_str(), static_cast<double>(d->rate));
                        lines.push_back(buf);
                    }
                    if (d->armor > 0.0f || d->insulation != 0.0f) {
                        std::snprintf(buf, sizeof(buf), "Armor %.0f   Warmth %.0f C", static_cast<double>(d->armor), static_cast<double>(d->insulation));
                        lines.push_back(buf);
                    }
                    if (d->heal > 0.0f || d->food > 0.0f || d->water > 0.0f) {
                        std::snprintf(buf, sizeof(buf), "Heals %.0f   Food %.0f   Water %.0f", static_cast<double>(d->heal), static_cast<double>(d->food), static_cast<double>(d->water));
                        lines.push_back(buf);
                    }
                    if (!d->description.empty()) lines.push_back(d->description.substr(0, 90));
                    lines.push_back("Drag to move, right-click to use or wear, shift-click to send across");
                    float tw = 0.0f;
                    for (const std::string& l : lines) tw = std::max(tw, text_w(l, 12.0f));
                    float tx = std::min(m_mouse_x + 16.0f, sw - tw - 20.0f), ty = std::min(m_mouse_y + 16.0f, sh - lines.size() * 16.0f - 12.0f);
                    rect(tx, ty, tw + 14.0f, lines.size() * 16.0f + 8.0f, Color(12, 14, 11, 235));
                    frame(tx, ty, tw + 14.0f, lines.size() * 16.0f + 8.0f, edge_c);
                    for (std::size_t i = 0; i < lines.size(); ++i) text(tx + 7.0f, ty + 4.0f + i * 16.0f, lines[i], 12.0f, i == 0 ? white : dim);
                }
        if (m_hud_drag.on) {
            const orchard::ItemDef* d = cat.find(m_hud_drag.item);
            float cell = inv->cell, pad = inv->padding;
            for (const orchard::HudRect& c : lay.cells)
                if (c.w <= c.h * 2.5f && c.list == "items") {
                    cell = c.w;
                    break;
                }
            pad *= cell / std::max(1.0f, inv->cell);
            float w = cell, h = cell;
            if (d && inv->style == orchard::InventoryStyle::grid) {
                w = (m_hud_drag.rotate ? d->grid_h : d->grid_w) * (cell + pad) - pad;
                h = (m_hud_drag.rotate ? d->grid_w : d->grid_h) * (cell + pad) - pad;
            }
            rect(m_mouse_x - cell * 0.5f, m_mouse_y - cell * 0.5f, w, h, Color(90, 100, 84, 180));
            draw_item_icon(r, d, m_mouse_x - cell * 0.5f, m_mouse_y - cell * 0.5f, w, h, m_hud_drag.rotate);
        }
    }

    void drive_player_camera(float dt) {
        if (!m_player_ready) start_player_camera();
        if (!m_player_ready) return;
        const orchard::Entity cam = m_scene.active_camera();
        const orchard::Camera* c = m_scene.camera(cam);
        orchard::Transform* t = m_scene.transform(cam);
        if (!c || !t || c->control == orchard::CameraControl::none) return;
        dt = std::clamp(dt, 0.0f, 0.1f);
        if (m_view.dimension() != orchard::Dimension::three_d) {
            drive_player_2d(cam, *c, dt);
            return;
        }
        orchard::ScriptRuntime& keys = m_session.scripts();
        const float turn = 90.0f * dt;
        if (keys.key_down("left")) m_player_yaw += turn;
        if (keys.key_down("right")) m_player_yaw -= turn;
        if (keys.key_down("up")) m_player_pitch += turn;
        if (keys.key_down("down")) m_player_pitch -= turn;
        m_player_pitch = std::clamp(m_player_pitch, -89.0f, 89.0f);
        const orchard::Vec3 up = camera_up_axis();
        const orchard::Vec3 look = look_direction(m_player_yaw, m_player_pitch);
        const orchard::Vec3 flat_forward = look_direction(m_player_yaw, 0.0f);
        const orchard::Vec3 right = orchard::normalize(orchard::cross(flat_forward, up));
        const orchard::Vec3 forward = c->control == orchard::CameraControl::fly ? look : flat_forward;
        orchard::Vec3 move{};
        if (keys.physical_key_down("w")) move += forward;
        if (keys.physical_key_down("s")) move += forward * -1.0f;
        if (keys.physical_key_down("d")) move += right;
        if (keys.physical_key_down("a")) move += right * -1.0f;
        xylem::World* world = m_session.world();
        const orchard::Entity rider = m_scene.body_owner(cam);
        orchard::Gameplay& game = keys.gameplay();
        const orchard::Entity me = m_scene.alive(rider) ? rider : cam;
        const orchard::Exertion effort = game.exert(me, c->move_speed, c->sprint(), keys.key_down("shift"), orchard::length(move) > 0.0f, keys.key_down("space"), dt);
        const float speed = effort.speed;
        const orchard::Body* rider_body = rider.valid() ? m_scene.body(rider) : nullptr;
        if (c->control == orchard::CameraControl::walk && world && rider_body && rider_body->type == orchard::BodyType::dynamic && world->has_body(rider)) {
            xylem::CharacterMove mv;
            mv.wish = orchard::length(move) > 0.0f ? orchard::normalize(move) * speed : orchard::Vec3{};
            mv.jump = effort.can_jump;
            mv.jump_speed = c->jump_speed * effort.jump;
            mv.ride_height = 0.05f;
            if (world->move_character(rider, mv, dt).jumped) game.jumped(me);
            if (orchard::Transform* rt = m_scene.transform(rider)) {
                orchard::Vec3 face = orchard::rotate(xylem::world_rotation(m_scene, rider), orchard::Vec3{0.0f, 0.0f, 1.0f});
                face = face - up * orchard::dot(face, up);
                if (orchard::length(face) > 1e-3f) {
                    face = orchard::normalize(face);
                    const float yaw = std::atan2(orchard::dot(orchard::cross(face, flat_forward), up), orchard::dot(face, flat_forward));
                    if (std::fabs(yaw) > 0.002f) {
                        const orchard::Quat turn{up.x * std::sin(yaw * 0.5f), up.y * std::sin(yaw * 0.5f), up.z * std::sin(yaw * 0.5f), std::cos(yaw * 0.5f)};
                        const orchard::Entity rp = m_scene.parent(rider);
                        if (m_scene.alive(rp)) {
                            const orchard::Quat pr = xylem::world_rotation(m_scene, rp);
                            const orchard::Quat pinv{-pr.x, -pr.y, -pr.z, pr.w};
                            rt->rotation = orchard::normalize(pinv * turn * pr * rt->rotation);
                        } else {
                            rt->rotation = orchard::normalize(turn * rt->rotation);
                        }
                        world->moved(rider);
                    }
                }
            }
            const orchard::Quat want = camera_rotation(look);
            const orchard::Entity parent = m_scene.parent(cam);
            if (m_scene.alive(parent)) {
                const orchard::Quat pr = xylem::world_rotation(m_scene, parent);
                t->rotation = orchard::normalize(orchard::Quat{-pr.x, -pr.y, -pr.z, pr.w} * want);
            } else {
                t->rotation = want;
            }
            return;
        }
        if (c->control == orchard::CameraControl::fly) {
            if (keys.physical_key_down("e") || keys.key_down("space")) move += up;
            if (keys.physical_key_down("q")) move += up * -1.0f;
            if (orchard::length(move) > 0.0f) t->position = t->position + orchard::normalize(move) * (speed * dt);
        } else {
            if (orchard::length(move) > 0.0f) {
                const orchard::Vec3 dir = orchard::normalize(move);
                float allowed = speed * dt;
                float hit = 0.0f;
                if (world && world->raycast_ignoring(rider, t->position + up * (-c->eye_height * 0.5f), dir, allowed + 0.3f, nullptr, nullptr, &hit).valid()) allowed = std::max(0.0f, hit - 0.3f);
                if (world && world->raycast_ignoring(rider, t->position, dir, allowed + 0.3f, nullptr, nullptr, &hit).valid()) allowed = std::min(allowed, std::max(0.0f, hit - 0.3f));
                t->position = t->position + dir * allowed;
            }
            float ground = 0.0f;
            const bool has_ground = world && world->raycast_ignoring(rider, t->position, up * -1.0f, c->eye_height + 50.0f, nullptr, nullptr, &ground).valid();
            const bool grounded = has_ground && ground <= c->eye_height + 0.05f && m_player_fall <= 0.0f;
            if (!has_ground && m_player_fall <= 0.0f) {
                m_player_fall = 0.0f;
            } else if (grounded) {
                m_player_fall = 0.0f;
                t->position = t->position + up * (c->eye_height - ground);
                if (keys.key_down("space")) m_player_fall = 5.0f;
            } else {
                m_player_fall -= 9.81f * dt;
                float step = m_player_fall * dt;
                if (step < 0.0f && has_ground && ground + step < c->eye_height) step = c->eye_height - ground;
                t->position = t->position + up * step;
            }
        }
        t->rotation = camera_rotation(look);
        if (world) world->moved(cam);
    }

    void face_along(orchard::Entity e, orchard::Vec3 dir, xylem::World* world) {
        orchard::Transform* rt = m_scene.transform(e);
        if (!rt) return;
        const orchard::Vec3 up{0.0f, 1.0f, 0.0f};
        orchard::Vec3 face = orchard::rotate(xylem::world_rotation(m_scene, e), orchard::Vec3{0.0f, 0.0f, 1.0f});
        face = face - up * orchard::dot(face, up);
        if (orchard::length(face) < 1e-3f) return;
        face = orchard::normalize(face);
        const float yaw = std::atan2(orchard::dot(orchard::cross(face, dir), up), orchard::dot(face, dir));
        if (std::fabs(yaw) < 0.002f) return;
        const float step = std::clamp(yaw, -0.35f, 0.35f);
        const orchard::Quat turn{0.0f, std::sin(step * 0.5f), 0.0f, std::cos(step * 0.5f)};
        const orchard::Entity rp = m_scene.parent(e);
        if (m_scene.alive(rp)) {
            const orchard::Quat pr = xylem::world_rotation(m_scene, rp);
            const orchard::Quat pinv{-pr.x, -pr.y, -pr.z, pr.w};
            rt->rotation = orchard::normalize(pinv * turn * pr * rt->rotation);
        } else {
            rt->rotation = orchard::normalize(turn * rt->rotation);
        }
        if (world) world->moved(e);
    }

    void drive_player_2d(orchard::Entity cam, const orchard::Camera& c, float dt) {
        orchard::ScriptRuntime& keys = m_session.scripts();
        orchard::Transform* t = m_scene.transform(cam);
        float side = 0.0f;
        if (keys.physical_key_down("d") || keys.key_down("right")) side += 1.0f;
        if (keys.physical_key_down("a") || keys.key_down("left")) side -= 1.0f;
        float lift = 0.0f;
        if (keys.physical_key_down("w") || keys.key_down("up")) lift += 1.0f;
        if (keys.physical_key_down("s") || keys.key_down("down")) lift -= 1.0f;
        xylem::World* world = m_session.world();
        const orchard::Entity rider = m_scene.body_owner(cam);
        orchard::Gameplay& game = keys.gameplay();
        const orchard::Entity me = m_scene.alive(rider) ? rider : cam;
        const orchard::Exertion effort = game.exert(me, c.move_speed, c.sprint(), keys.key_down("shift"), side != 0.0f, keys.key_down("space") || lift > 0.0f, dt);
        const float speed = effort.speed;
        const orchard::Body* rider_body = rider.valid() ? m_scene.body(rider) : nullptr;
        const bool riding = world && rider_body && rider_body->type == orchard::BodyType::dynamic && world->has_body(rider);
        if (c.control == orchard::CameraControl::walk && riding) {
            xylem::CharacterMove mv;
            mv.wish = orchard::Vec3{side * speed, 0.0f, 0.0f};
            mv.jump = effort.can_jump;
            mv.jump_speed = c.jump_speed * effort.jump;
            mv.ride_height = 0.05f;
            if (world->move_character(rider, mv, dt).jumped) game.jumped(me);
            if (side != 0.0f && m_scene.dimension() == orchard::Dimension::three_d) face_along(rider, orchard::Vec3{side, 0.0f, 0.0f}, world);
            return;
        }
        if (!t) return;
        if (c.control == orchard::CameraControl::fly) {
            t->position = t->position + orchard::Vec3{side, lift, 0.0f} * (speed * dt);
            if (world) world->moved(cam);
            return;
        }
        if (side != 0.0f) {
            float allowed = std::fabs(side) * speed * dt;
            float hit = 0.0f;
            const orchard::Vec3 dir{side > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f};
            if (world && world->raycast_ignoring(cam, t->position + orchard::Vec3{0.0f, -c.eye_height * 0.5f, 0.0f}, dir, allowed + 0.3f, nullptr, nullptr, &hit).valid()) allowed = std::max(0.0f, hit - 0.3f);
            if (world && world->raycast_ignoring(cam, t->position, dir, allowed + 0.3f, nullptr, nullptr, &hit).valid()) allowed = std::min(allowed, std::max(0.0f, hit - 0.3f));
            t->position.x += dir.x * allowed;
        }
        float ground = 0.0f;
        const bool has_ground = world && world->raycast_ignoring(cam, t->position, orchard::Vec3{0.0f, -1.0f, 0.0f}, c.eye_height + 50.0f, nullptr, nullptr, &ground).valid();
        const bool grounded = has_ground && ground <= c.eye_height + 0.05f && m_player_fall <= 0.0f;
        if (grounded) {
            m_player_fall = 0.0f;
            t->position.y += c.eye_height - ground;
            if (keys.key_down("space") || lift > 0.0f) m_player_fall = c.jump_speed > 0.0f ? c.jump_speed : 5.0f;
        } else {
            m_player_fall -= 9.81f * dt;
            float step = m_player_fall * dt;
            if (step < 0.0f && has_ground && ground + step < c.eye_height) step = c.eye_height - ground;
            t->position.y += step;
        }
        if (world) world->moved(cam);
    }

    void follow_game_camera() {
        const orchard::Entity cam = m_scene.active_camera();
        if (!m_scene.alive(cam)) return;
        const orchard::Mat4 w = m_scene.world_matrix(cam);
        const orchard::Vec3 pos = orchard::mul_point(w, orchard::Vec3{0.0f, 0.0f, 0.0f});
        const orchard::Camera* settings = m_scene.camera(cam);
        if (m_view.dimension() == orchard::Dimension::two_d) {
            m_view.pan_camera().center = orchard::Vec2{pos.x, pos.y};
            if (settings && m_view.height() > 0) m_view.pan_camera().pixels_per_unit = static_cast<float>(m_view.height()) / std::max(0.01f, settings->view_height);
            return;
        }
        orchard::Vec3 f = orchard::normalize(orchard::mul_direction(w, camera_forward_axis()));
        if (orchard::length(f) < 0.5f) f = camera_forward_axis();
        const orchard::Vec3 fr = orchard::normalize(m_view.axis_system().to_render(f));
        orchard::OrbitCamera& oc = m_view.orbit_camera();
        oc.distance = 0.01f;
        oc.target = pos + f * oc.distance;
        oc.pitch = orchard::degrees(std::asin(std::clamp(-fr.y, -0.999f, 0.999f)));
        oc.yaw = orchard::degrees(std::atan2(-fr.x, -fr.z));
        if (settings) oc.fov = settings->fov;
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

    static std::string title_case(const std::string& text) {
        std::string out = text;
        bool start = true;
        for (char& ch : out) {
            if (ch == '-' || ch == '_') {
                ch = ' ';
                start = true;
                continue;
            }
            if (start) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            start = ch == ' ';
        }
        return out;
    }

    std::vector<grove::MenuItem> add_menu_items() const {
        const bool flat = m_scene.dimension() == orchard::Dimension::two_d;
        std::vector<grove::MenuItem> items;
        auto add = [&](const std::string& value, const std::string& label) { items.push_back(grove::menu_item(value, label)); };
        auto page = [&](const std::string& id, const std::string& label) { items.push_back(grove::menu_item("page:" + id, label, "\u25B8")); };
        auto header = [&](const std::string& label) { items.push_back(grove::menu_item("", label, "", true)); };
        auto line = [&] {
            grove::MenuItem sep;
            sep.separator = true;
            items.push_back(sep);
        };
        const std::string& at = m_menu_page;
        if (at.empty()) {
            header("Add to the scene");
            page("player", "Physical player");
            page("physics", "Physics object");
            if (!flat) page("terrain", "Terrain");
            page("characters", "Characters");
            page("ai", "AI (enemies, animals, helpers)");
            page("particles", "Particles");
            line();
            page("water", "Water");
            page("lights", "Lights");
            page("cameras", "Cameras");
            page("level", "Floors and zones");
            page("items", "Items, weapons and loot");
            page("weather", "Weather and climate zones");
            add("add:sound", "Sound");
            line();
            add("add:", "Empty entity");
            return items;
        }
        std::string back = at == "triangle" ? "physics" : at == "effects" ? "particles" : at.rfind("player-look:", 0) == 0 ? "player" : at.rfind("character-look:", 0) == 0 ? "characters" : "";
        if (at.rfind("items:", 0) == 0) back = at.find('/') != std::string::npos ? at.substr(0, at.find('/')) : std::string("items");
        items.push_back(grove::menu_item("page:" + back, "\u2039 Back"));
        if (at == "player" || at == "characters") {
            const bool player = at == "player";
            header(player ? "A body you walk around in. Pick how it looks:" : "A character (no controls). Pick how it looks:");
            for (const std::string& style : orchard::character_styles()) page((player ? "player-look:" : "character-look:") + style, orchard::character_style_label(style));
            page(player ? "player-look:none" : "character-look:none", "Colliders only (no mesh)");
            if (player) {
                line();
                add("add:player-camera", "Walking camera only (no body)");
            }
        } else if (at.rfind("player-look:", 0) == 0 || at.rfind("character-look:", 0) == 0) {
            const bool player = at.rfind("player-look:", 0) == 0;
            std::string style = at.substr(at.find(':') + 1);
            if (style == "none") style.clear();
            header("Its colliders:");
            for (orchard::HumanoidKind hk : orchard::humanoid_kinds()) add(std::string(player ? "add:physical-player:" : "add:human:") + orchard::to_string(hk) + (style.empty() ? "" : ":" + style), orchard::humanoid_label(hk));
        } else if (at == "physics") {
            header("Falls and gets pushed around");
            add("add:box", "Box");
            add("add:ball", "Ball");
            add("add:shape:capsule", "Capsule");
            add("add:shape:rounded-box", "Rounded box");
            add("add:barrel", "Explosive barrel (blows up when hurt)");
            add("add:crate", "Wooden crate (burns, breaks)");
            add("add:triangle:sss", "Triangle (equilateral)");
            page("triangle", "Triangle from SSS, SAS, ASA...");
            if (!flat) {
                add("add:shape:cylinder", "Cylinder");
                add("add:shape:cone", "Cone");
                add("add:hemisphere", "Hemisphere");
                add("add:shape:dome", "Dome (shallow cap)");
                add("add:shape:bowl", "Bowl (hollow, open top)");
            }
        } else if (at == "triangle") {
            header("Triangle prism built from");
            for (orchard::TriangleRule r : orchard::triangle_rules()) add(std::string("add:triangle:") + orchard::to_string(r), orchard::triangle_rule_label(r));
        } else if (at == "terrain") {
            header("Ground to walk on (pick other shapes in its properties)");
            for (const std::string& name : orchard::terrain_presets()) {
                std::string label = title_case(name);
                if (name == "flat") label = "Flat (to sculpt with B)";
                else if (name == "forest") label = "Forest (1 km of hills with trees and rocks)";
                add("add:terrain:" + name, label);
            }
        } else if (at == "ai") {
            header("Characters with a brain");
            for (const std::string& b : orchard::Agent::brains())
                if (b != "custom") add("add:agent:" + b, orchard::Agent::brain_label(b));
            add("add:agent:custom", "Custom (start from a simple brain)");
            line();
            header("Helpers for AI");
            add("add:patrol-route", "Patrol route (move its points)");
            add("add:hiding-spot", "Hiding spot (lockers, vents)");
            add("add:cover", "Low wall (cover)");
        } else if (at == "particles") {
            page("effects", "Effects (explosions, fire, steam...)");
            line();
            header("Single particle emitters");
            for (const std::string& name : orchard::Emitter::presets()) add("add:particles:" + name, title_case(name));
        } else if (at == "effects") {
            header("Several emitters and a light, ready to use");
            for (const std::string& name : orchard::effect_names()) add("add:effect:" + name, orchard::effect_label(name));
        } else if (at == "water") {
            header("Water");
            add("add:pool", "Pool (walls and water)");
            add("add:water", "Water on its own");
        } else if (at == "lights") {
            header("Lights");
            add("add:light", "Point light");
            add("add:light:spot", "Spot light");
            add("add:light:sun", "Sun (lights everything from one direction)");
            add("add:light:rectangle", "Rectangle area light");
            add("add:light:disk", "Disk area light");
            add("add:light:sphere", "Sphere light");
            add("add:light:tube", "Tube light");
        } else if (at == "cameras") {
            header("Cameras");
            add("add:camera", "Camera (from this view)");
            add("add:player-camera", "Walking camera (W A S D)");
            if (!flat) add("add:camera:fly", "Flying camera (W A S D, E and Q)");
        } else if (at == "items") {
            header("Pick up, carry, wear and use (every item can be changed in Define Item)");
            std::vector<std::string> tops;
            for (const orchard::ItemDef& d : orchard::item_library()) {
                const std::string top = d.category.substr(0, d.category.find('/'));
                if (std::find(tops.begin(), tops.end(), top) == tops.end()) tops.push_back(top);
            }
            for (const std::string& t : tops) page("items:" + t, t);
            line();
            add("add:chest", "Storage chest (has its own inventory)");
            add("add:locker", "Locker (grid inventory, Tarkov style)");
        } else if (at.rfind("items:", 0) == 0) {
            const std::string cat = at.substr(6);
            const bool deeper = cat.find('/') != std::string::npos;
            std::vector<std::string> subs;
            for (const orchard::ItemDef& d : orchard::item_library()) {
                if (d.category != cat && d.category.rfind(cat + "/", 0) != 0) continue;
                if (!deeper && d.category.find('/') != std::string::npos) {
                    if (std::find(subs.begin(), subs.end(), d.category) == subs.end()) subs.push_back(d.category);
                } else {
                    add("add:item:" + d.id, d.name);
                }
            }
            if (!subs.empty()) header(cat);
            for (const std::string& sc : subs) page("items:" + sc, sc.substr(sc.find('/') + 1));
        } else if (at == "weather") {
            header("Covers the whole world");
            for (const std::string& pr : orchard::climate_presets())
                if (pr != "custom") add("add:climate-global:" + pr, title_case(pr));
            line();
            header("Only inside a box you can move and size");
            for (const std::string& pr : orchard::climate_presets()) add("add:climate:" + pr, title_case(pr));
        } else if (at == "level") {
            header("Floors and zones");
            add("add:ground", "Floor");
            add("add:trigger", "Trigger zone");
            add("add:", "Empty entity");
        }
        return items;
    }

    std::vector<grove::MenuItem> menu_items(orchard::Entity e) const {
        std::vector<grove::MenuItem> items;
        if (!m_scene.alive(e)) return add_menu_items();
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
        if (m_scene.dimension() == orchard::Dimension::three_d) {
            items.push_back(grove::menu_separator());
            const std::size_t count = selection_with(e).size();
            if (solid_group_possible(e)) {
                items.push_back(grove::menu_item("solid:union", count > 1 ? "Union the " + std::to_string(count) + " selected parts" : "Union its parts into one", "Ctrl+Shift+G", busy));
                items.push_back(grove::menu_item("solid:intersect", count > 1 ? "Intersect the " + std::to_string(count) + " selected parts" : "Intersect its parts", "Ctrl+Shift+I", busy));
            }
            const orchard::Solid* sd = m_scene.solid(e);
            items.push_back(grove::menu_item("solid:negate", sd && sd->negative ? "Un-negate (make it solid again)" : "Negate (cut it out when unioned)", "Ctrl+Shift+N", busy));
            if (sd && !sd->parts.empty()) items.push_back(grove::menu_item("solid:separate", "Separate back into its parts", "Ctrl+Shift+U", busy));
        }
        items.push_back(grove::menu_separator());
        items.push_back(grove::menu_item("child", "Add a child"));
        items.push_back(grove::menu_item("delete", "Delete", "Delete", busy));
        return items;
    }

    std::vector<orchard::Entity> selection_with(orchard::Entity e) const {
        std::vector<orchard::Entity> out;
        auto add = [&](orchard::Entity x) {
            if (m_scene.alive(x) && std::find(out.begin(), out.end(), x) == out.end()) out.push_back(x);
        };
        const bool in_selection = e == m_selected || std::find(m_also.begin(), m_also.end(), e) != m_also.end();
        if (m_scene.alive(e) && !in_selection) {
            add(e);
            return out;
        }
        for (orchard::Entity x : m_also) add(x);
        add(m_selected);
        return out;
    }

    void prune_selection() {
        m_also.erase(std::remove_if(m_also.begin(), m_also.end(), [&](orchard::Entity x) { return !m_scene.alive(x) || x == m_selected; }), m_also.end());
    }

    orchard::SolidOptions solid_options(bool one_color) const {
        orchard::SolidOptions o;
        if (m_project) o.project_root = m_project->folder();
        const bool z_up = m_project && m_project->info().axis_system().up().z > 0.5f;
        o.z_up = z_up;
        o.one_color = one_color;
        const std::string root = o.project_root;
        o.loader = [root, z_up](const std::string& source, std::vector<orchard::Vec3>& vertices, std::vector<std::uint32_t>& indices) {
            const std::string path = root.empty() || std::filesystem::path(source).is_absolute() ? source : (std::filesystem::path(root) / source).string();
            const std::shared_ptr<const xylem::ModelGeometry> g = xylem::load_model_geometry(path, z_up);
            if (!g || g->empty()) return false;
            vertices = g->vertices;
            indices = g->indices;
            return true;
        };
        return o;
    }

    bool solid_group_possible(orchard::Entity e) const {
        const std::vector<orchard::Entity> sel = selection_with(e);
        if (sel.size() >= 2) return true;
        for (orchard::Entity x : m_scene.subtree(e))
            if (x != e && m_scene.colliders(x) && !m_scene.colliders(x)->empty()) return true;
        return false;
    }

    std::string solid_action(orchard::Entity e, const std::string& action) {
        if (m_session.playing() || m_session.building()) return "error\tstop playing first";
        if (!m_project) return "error\topen a project first";
        if (m_scene.dimension() == orchard::Dimension::two_d) return "error\tunion, intersect and negate work in 3D scenes";
        const std::vector<orchard::Entity> sel = selection_with(e);
        if (sel.empty()) return "error\tpick some parts first";
        if (action == "solid:negate") {
            const orchard::Solid* first = m_scene.solid(sel.front());
            const bool make = !(first && first->negative);
            for (orchard::Entity x : sel) {
                orchard::Solid& sd = m_scene.add_solid(x);
                sd.negative = make;
                if (!sd.negative && sd.operation == orchard::SolidOperation::none && sd.parts.empty()) m_scene.remove_solid(x);
            }
            touched();
            add_log(std::string(make ? "Negated " : "Un-negated ") + (sel.size() == 1 ? m_scene.name(sel.front()) : std::to_string(sel.size()) + " parts") + (make ? ": it's cut out of whatever you union it with, and it's hidden and goes through things while playing" : ""), false);
            return std::string("ok");
        }
        if (action == "solid:separate") {
            std::vector<orchard::Entity> made;
            std::string errors;
            for (orchard::Entity x : sel) {
                if (!m_scene.solid(x) || m_scene.solid(x)->parts.empty()) continue;
                const orchard::SolidResult r = orchard::separate_solid(m_scene, x, solid_options(false));
                if (!r.ok()) errors = r.error;
                for (orchard::Entity p : r.entities) made.push_back(p);
            }
            if (made.empty()) return "error\t" + (errors.empty() ? std::string("only a union or intersection can be separated") : errors);
            m_selected = made.front();
            m_also.assign(made.begin() + 1, made.end());
            touched();
            return "keep\t" + std::to_string(m_selected.id());
        }
        std::vector<orchard::Entity> parts = sel;
        if (parts.size() == 1) parts = m_scene.children(parts.front()).empty() ? parts : m_scene.children(parts.front());
        const orchard::SolidOperation op = action == "solid:intersect" ? orchard::SolidOperation::intersect_parts : orchard::SolidOperation::union_parts;
        const orchard::SolidResult r = orchard::combine_solids(m_scene, parts, op, solid_options(false));
        for (const std::string& n : r.notes) add_log(n, false);
        if (!r.ok()) return "error\t" + r.error;
        m_selected = r.entity;
        m_also.clear();
        touched();
        add_log("Made " + m_scene.name(r.entity) + " (" + r.mesh_file + "). Separate gets the parts back.", false);
        return "select\t" + std::to_string(r.entity.id());
    }

    std::string menu_action(orchard::Entity e, const std::string& action) {
        if (action.rfind("page:", 0) == 0) {
            m_menu_page = action.substr(5);
            grove::Node* owner = m_doc.find_by_id(m_menu_owner);
            if (owner && grove::open_menu(m_doc, *owner, add_menu_items(), m_menu_x, m_menu_y)) return "menu";
            m_menu_page.clear();
            return "menu";
        }
        m_menu_page.clear();
        if (action.rfind("solid:", 0) == 0) return solid_action(e, action);
        if (action.rfind("add:", 0) == 0) {
            const std::string kind = action.substr(4);
            orchard::Entity made;
            if (kind.empty()) {
                made = m_scene.create("Entity", orchard::Entity{});
            } else {
                made = add_preset(kind, orchard::Entity{});
            }
            if (m_scene.alive(made) && m_scene.dimension() == orchard::Dimension::three_d && m_view.dimension() == orchard::Dimension::two_d) {
                if (orchard::Transform* t = m_scene.transform(made)) {
                    const orchard::Vec2 c = m_view.pan_camera().center;
                    const float lift = t->position.y - m_view.orbit_camera().target.y;
                    t->position = {c.x, c.y + lift, 0.0f};
                }
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
            if (!m_scene.emitter(e)) {
                orchard::Emitter& made = m_scene.add_emitter(e);
                if (m_scene.fluid(e)) {
                    if (const std::optional<orchard::Emitter> steam = orchard::Emitter::named("steam")) made = *steam;
                    made.shape = orchard::EmitterShape::fluid_surface;
                } else if (m_scene.colliders(e) && !m_scene.colliders(e)->empty()) {
                    made.shape = orchard::EmitterShape::parts;
                }
            }
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
            if (b->type != orchard::BodyType::fixed) {
                out += "title\tStart motion\n";
                auto vec = [&out](const char* field, const char* label, orchard::Vec3 v, const char* step) {
                    out += std::string("vec3\t") + field + "\t" + label + "\t" + number(v.x) + "\t" + number(v.y) + "\t" + number(v.z) + "\t" + step + "\n";
                };
                vec("v0", flat ? "Starting velocity (m/s, X Y)" : "Starting velocity (m/s)", b->start_velocity, "0.5");
                vec("spin0", flat ? "Starting spin (degrees/s, Z)" : "Starting spin (degrees/s)", b->start_spin, "5");
                vec("accel", flat ? "Constant acceleration (m/s\u00B2, X Y)" : "Constant acceleration (m/s\u00B2)", b->acceleration, "0.5");
                out += std::string("check\tmotion_local\tRelative to its own rotation\t") + (b->motion_local ? "1" : "0") + "\n";
            }
            for (const xylem::FlagOption& o : xylem::flag_options()) {
                if (flat && !o.two_d) continue;
                out += std::string("aflag\t") + o.name + "\t" + o.label + "\t" + (xylem::flag_on(*b, o.bit) ? "1" : "0") + "\t" + o.group + "\n";
            }
        }
        if (const std::vector<orchard::Collider>* cs = m_scene.colliders(e)) {
            const orchard::Vec3 scale = xylem::world_scale(m_scene, e);
            const orchard::Dimension dim = m_scene.dimension();
            for (std::size_t i = 0; i < cs->size(); ++i) {
                const orchard::Collider& c = (*cs)[i];
                const float mass = orchard::collider_mass(c, scale, dim);
                const float density = c.locked(orchard::collider_lock_mass) && c.mass > 0.0f ? mass / std::max(1e-9f, orchard::collider_density_unit(dim) * orchard::collider_volume(c, scale, dim)) : c.density;
                out += "collider\t" + std::to_string(i) + "\t" + orchard::to_string(c.shape) + "\t" + number(c.size.x) + "\t" + number(c.size.y) + "\t" + number(c.size.z)
                     + "\t" + number(c.offset.x) + "\t" + number(c.offset.y) + "\t" + number(c.offset.z) + "\t" + (c.enabled ? "1" : "0") + "\t" + (c.sensor ? "1" : "0")
                     + "\t" + number(density) + "\t" + number(c.friction) + "\t" + number(c.restitution)
                     + "\t" + orchard::to_string(c.friction_combine) + "\t" + orchard::to_string(c.bounce_combine)
                     + "\t" + (m_project ? orchard::combine_label(m_project->info().friction_combine) : "Geometric mean")
                     + "\t" + (m_project ? orchard::combine_label(m_project->info().bounce_combine) : "Maximum")
                     + "\t" + number(c.rounding) + "\t" + number(c.rolling_friction) + "\t" + number(c.spinning_friction)
                     + "\t" + number(c.grip.x) + "\t" + number(c.grip.y) + "\t" + number(c.grip.z) + "\t" + shape_names(flat)
                     + "\t" + number(c.polar_min) + "\t" + number(c.polar_max) + "\t" + number(c.azimuth_min) + "\t" + number(c.azimuth_max) + "\t" + number(c.shell) + "\t" + clean(c.source)
                     + "\t" + number(mass) + "\t" + lock_letters(c) + "\t" + clean(c.name) + "\t" + (flat ? "kg/m\u00B2" : "\u00D7 water")
                     + "\t" + triangle_fields(c) + "\n";
            }
        }
        return out;
    }

    static std::string triangle_fields(const orchard::Collider& c) {
        orchard::Vec3 sides, angles;
        std::string why;
        std::string info;
        if (orchard::solve_triangle(c, sides, angles, &why)) {
            info = "Sides a " + number(sides.x) + ", b " + number(sides.y) + ", c " + number(sides.z) + " m; angles A " + number(std::round(angles.x * 10.0f) / 10.0f) + "\u00B0, B " + number(std::round(angles.y * 10.0f) / 10.0f) + "\u00B0, C " + number(std::round(angles.z * 10.0f) / 10.0f) + "\u00B0";
        } else {
            info = "No triangle: " + why;
        }
        std::string rules;
        for (orchard::TriangleRule r : orchard::triangle_rules()) rules += (rules.empty() ? "" : ";") + std::string(orchard::to_string(r)) + "|" + orchard::triangle_rule_label(r);
        return std::string(orchard::to_string(c.triangle)) + "\t" + number(c.triangle_sides.x) + "\t" + number(c.triangle_sides.y) + "\t" + number(c.triangle_sides.z)
             + "\t" + number(c.triangle_angles.x) + "\t" + number(c.triangle_angles.y) + "\t" + number(c.triangle_angles.z) + "\t" + (c.triangle_wide ? "1" : "0")
             + "\t" + clean(info) + "\t" + rules;
    }

    static std::string lock_letters(const orchard::Collider& c) {
        std::string out;
        if (c.locked(orchard::collider_lock_mass)) out += "m";
        if (c.locked(orchard::collider_lock_density)) out += "d";
        if (c.locked(orchard::collider_lock_size_x)) out += "x";
        if (c.locked(orchard::collider_lock_size_y)) out += "y";
        if (c.locked(orchard::collider_lock_size_z)) out += "z";
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
            if (field == "v0" || field == "spin0" || field == "accel") {
                orchard::Vec3 vec;
                if (!parse_vec3(value, vec)) return false;
                (field == "v0" ? b->start_velocity : field == "spin0" ? b->start_spin : b->acceleration) = vec;
                return true;
            }
            if ((field.rfind("v0", 0) == 0 || field.rfind("spin0", 0) == 0 || field.rfind("accel", 0) == 0) && (field.back() == 'x' || field.back() == 'y' || field.back() == 'z')) {
                const std::string root = field.substr(0, field.size() - 1);
                if (root != "v0" && root != "spin0" && root != "accel") return false;
                orchard::Vec3& vec = root == "v0" ? b->start_velocity : root == "spin0" ? b->start_spin : b->acceleration;
                const float x = std::clamp(v, -100000.0f, 100000.0f);
                (field.back() == 'x' ? vec.x : field.back() == 'y' ? vec.y : vec.z) = x;
                return true;
            }
            if (field == "motion_local") {
                b->motion_local = on;
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
            const orchard::Vec3 scale = xylem::world_scale(m_scene, e);
            const orchard::Dimension dim = m_scene.dimension();
            if (what == "mass" || what == "density" || what == "sx" || what == "sy" || what == "sz") {
                const orchard::MassEdit me = what == "mass" ? orchard::MassEdit::mass : what == "density" ? orchard::MassEdit::density : what == "sx" ? orchard::MassEdit::size_x : what == "sy" ? orchard::MassEdit::size_y : orchard::MassEdit::size_z;
                const float now = what == "mass" ? orchard::collider_mass(c, scale, dim) : what == "density" ? c.density : what == "sx" ? c.size.x : what == "sy" ? c.size.y : c.size.z;
                if (std::fabs(now - v) <= 1e-6f * std::max(1.0f, std::fabs(v))) return true;
                std::string why;
                if (!orchard::edit_collider_mass(c, me, what == "density" ? std::max(0.0001f, v) : v, scale, dim, &why)) {
                    m_component_error = "Can't change the " + (what == "mass" ? std::string("mass") : what == "density" ? std::string("density") : std::string("size")) + ": " + why;
                    return false;
                }
                return true;
            }
            if (what.rfind("lock_", 0) == 0) {
                const std::string k = what.substr(5);
                const orchard::ColliderLock l = k == "mass" ? orchard::collider_lock_mass : k == "density" ? orchard::collider_lock_density : k == "sx" ? orchard::collider_lock_size_x : k == "sy" ? orchard::collider_lock_size_y : k == "sz" ? orchard::collider_lock_size_z : k == "size" ? orchard::collider_lock_size : static_cast<orchard::ColliderLock>(0);
                if (l == 0) return false;
                if (l == orchard::collider_lock_mass && on && !c.locked(orchard::collider_lock_mass)) c.mass = orchard::collider_mass(c, scale, dim);
                if (l == orchard::collider_lock_mass && !on && c.locked(orchard::collider_lock_mass)) {
                    c.density = std::max(0.0001f, orchard::collider_mass(c, scale, dim) / std::max(1e-9f, orchard::collider_density_unit(dim) * orchard::collider_volume(c, scale, dim)));
                }
                c.lock(l, on);
                return true;
            }
            if (what == "name") {
                c.name = clean(value);
                return true;
            }
            if (what == "shape") {
                const std::optional<orchard::ColliderShape> shape = orchard::parse_collider_shape(value);
                if (!shape) return false;
                const bool to_triangle = *shape == orchard::ColliderShape::triangle && c.shape != orchard::ColliderShape::triangle;
                c.shape = *shape;
                if (to_triangle) {
                    const float side = std::max(c.size.x, c.size.y);
                    c.triangle = orchard::TriangleRule::sss;
                    c.triangle_sides = {side, side, side};
                    orchard::fit_triangle_size(c);
                }
            } else if (what.rfind("tri_", 0) == 0) {
                if (what == "tri_rule") {
                    const std::optional<orchard::TriangleRule> rule = orchard::parse_triangle_rule(value);
                    if (!rule) return false;
                    orchard::Vec3 sides, angles;
                    if (orchard::solve_triangle(c, sides, angles)) {
                        c.triangle_sides = sides;
                        c.triangle_angles = angles;
                    }
                    c.triangle = *rule;
                    if (*rule == orchard::TriangleRule::right) {
                        c.triangle_angles.x = 90.0f;
                    }
                } else if (what == "tri_wide") {
                    c.triangle_wide = on;
                } else {
                    if (!(v > 0.0f)) return false;
                    if (what == "tri_a") c.triangle_sides.x = v;
                    else if (what == "tri_b") c.triangle_sides.y = v;
                    else if (what == "tri_c") c.triangle_sides.z = v;
                    else if (what == "tri_A") c.triangle_angles.x = std::min(v, 179.9f);
                    else if (what == "tri_B") c.triangle_angles.y = std::min(v, 179.9f);
                    else if (what == "tri_C") c.triangle_angles.z = std::min(v, 179.9f);
                    else return false;
                }
                orchard::fit_triangle_size(c);
            } else if (what == "source") {
                c.source = value;
            } else if (what == "ox") c.offset.x = v;
            else if (what == "oy") c.offset.y = v;
            else if (what == "oz") c.offset.z = v;
            else if (what == "enabled") c.enabled = on;
            else if (what == "sensor") c.sensor = on;
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

    void make_player_colliders(orchard::Entity e) {
        const orchard::Vec3 one{1.0f, 1.0f, 1.0f};
        std::vector<orchard::Collider*> solid;
        float total = 0.0f;
        std::vector<orchard::Entity> todo{e};
        while (!todo.empty()) {
            const orchard::Entity at = todo.back();
            todo.pop_back();
            if (std::vector<orchard::Collider>* cs = m_scene.colliders(at)) {
                for (orchard::Collider& c : *cs) {
                    if (c.sensor) continue;
                    c.lock(orchard::collider_lock_mass, false);
                    c.friction = 0.0f;
                    c.friction_combine = orchard::CombineMode::minimum;
                    total += orchard::collider_mass(c, one, m_scene.dimension());
                    solid.push_back(&c);
                }
            }
            for (orchard::Entity child : m_scene.children(at)) todo.push_back(child);
        }
        const float target = 75.0f;
        for (orchard::Collider* c : solid) {
            const float share = total > 0.0f ? orchard::collider_mass(*c, one, m_scene.dimension()) / total : 1.0f / static_cast<float>(solid.size());
            c->mass = std::max(0.01f, target * share);
            c->lock(orchard::collider_lock_mass, true);
        }
        if (solid.size() == 1) solid.front()->name = "Body";
    }

    bool is_physical_player(orchard::Entity e) const {
        for (orchard::Entity c : m_scene.children(e))
            if (const orchard::Camera* cam = m_scene.camera(c))
                if (cam->control == orchard::CameraControl::walk) return true;
        return false;
    }

    void ensure_character_models() {
        if (!m_project) return;
        for (orchard::Entity e : m_scene.entities()) {
            const orchard::Model* md = m_scene.model(e);
            if (!md || md->style.empty() || md->source.empty()) continue;
            if (!fs::exists(m_project->path_of(md->source))) orchard::editor::write_builtin_model("character-" + md->style, m_project->path_of(md->source));
        }
    }

    orchard::Entity add_agent_entity(const std::string& brain, orchard::Entity parent) {
        const std::optional<orchard::Agent> preset = orchard::Agent::preset(brain);
        if (!preset) return orchard::Entity{};
        const bool flat = m_scene.dimension() == orchard::Dimension::two_d;
        const orchard::Vec3 at = flat ? orchard::Vec3{m_view.pan_camera().center.x, m_view.pan_camera().center.y, 0.0f} : m_view.orbit_camera().target;
        std::string name = brain;
        if (!name.empty()) name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
        for (char& c : name)
            if (c == '-') c = ' ';
        orchard::Entity e;
        orchard::Vec3 color{0.6f, 0.6f, 0.6f};
        if (brain == "zombie" || brain == "runner" || brain == "crawler" || brain == "screamer") color = {0.35f, 0.55f, 0.3f};
        else if (brain == "guard" || brain == "stealth-guard") color = {0.25f, 0.35f, 0.7f};
        else if (brain == "soldier") color = {0.3f, 0.4f, 0.25f};
        else if (brain == "hunter") color = {0.08f, 0.08f, 0.1f};
        else if (brain == "prey") color = {0.6f, 0.45f, 0.3f};
        else if (brain == "predator") color = {0.85f, 0.5f, 0.15f};
        else if (brain == "companion") color = {0.2f, 0.7f, 0.75f};
        else if (brain == "civilian") color = {0.85f, 0.75f, 0.35f};
        else if (brain == "brawler") color = {0.6f, 0.25f, 0.2f};
        else if (brain == "turret" || brain == "flyer") color = {0.55f, 0.15f, 0.15f};
        if (brain == "turret") {
            e = m_scene.create("Turret", parent);
            m_scene.add_body(e).type = orchard::BodyType::fixed;
            m_scene.add_collider(e)->size = {0.8f, 0.8f, 0.8f};
            m_scene.transform(e)->position = at + orchard::Vec3{0.0f, 0.4f, 0.0f};
        } else if (brain == "flyer") {
            e = m_scene.create("Drone", parent);
            orchard::Body& b = m_scene.add_body(e);
            b.gravity_scale = 0.0f;
            b.set(orchard::body_lock_rotation_x, true);
            b.set(orchard::body_lock_rotation_y, true);
            b.set(orchard::body_lock_rotation_z, true);
            b.set(orchard::body_sleep, false);
            orchard::Collider* c = m_scene.add_collider(e, orchard::ColliderShape::sphere);
            c->size = {0.6f, 0.6f, 0.6f};
            m_scene.transform(e)->position = at + orchard::Vec3{0.0f, preset->fly_height, 0.0f};
        } else {
            const bool person = brain != "prey" && brain != "predator";
            if (person) {
                e = orchard::add_humanoid(m_scene, orchard::HumanoidKind::basic, parent, name);
            } else {
                e = m_scene.create(name, parent);
                m_scene.add_body(e);
                orchard::Collider* c = m_scene.add_collider(e, orchard::ColliderShape::capsule);
                c->size = {preset->radius * 2.0f, preset->height, preset->radius * 2.0f};
                c->offset = {0.0f, preset->height * 0.5f, 0.0f};
            }
            orchard::Body& b = *m_scene.body(e);
            b.type = orchard::BodyType::dynamic;
            b.set(orchard::body_lock_rotation_x, true);
            b.set(orchard::body_lock_rotation_y, true);
            b.set(orchard::body_lock_rotation_z, true);
            b.set(orchard::body_sleep, false);
            b.continuous = orchard::BodyContinuous::on;
            make_player_colliders(e);
            if (person && brain != "hunter" && brain != "crawler") {
                if (orchard::set_character_style(m_scene, e, brain == "guard" || brain == "soldier" || brain == "stealth-guard" || brain == "turret" ? "robot" : "mannequin")) ensure_character_models();
            }
            m_scene.transform(e)->position = at;
        }
        m_scene.add_material(e).color = color;
        m_scene.add_agent(e) = *preset;
        orchard::Health& h = m_scene.add_health(e);
        h.on_death = orchard::DeathAction::nothing;
        if (brain == "hunter") h.value = h.max = 400.0f;
        if (brain == "prey") h.value = h.max = 40.0f;
        return e;
    }

    orchard::Entity add_physical_player(orchard::HumanoidKind hk, orchard::Entity parent, const std::string& style) {
        const orchard::Entity e = add_physical_player(hk, parent);
        if (!style.empty() && orchard::set_character_style(m_scene, e, style)) ensure_character_models();
        return e;
    }

    orchard::Entity add_physical_player(orchard::HumanoidKind hk, orchard::Entity parent) {
        const bool flat = m_scene.dimension() == orchard::Dimension::two_d;
        for (orchard::Entity other : m_scene.entities())
            if (orchard::Camera* oc = m_scene.camera(other)) oc->active = false;
        const orchard::Entity e = orchard::add_humanoid(m_scene, hk, parent, "Player");
        orchard::Body& b = *m_scene.body(e);
        b.type = orchard::BodyType::dynamic;
        b.set(orchard::body_lock_rotation_x, !flat);
        b.set(orchard::body_lock_rotation_y, !flat);
        b.set(orchard::body_lock_rotation_z, true);
        b.set(orchard::body_sleep, false);
        b.continuous = orchard::BodyContinuous::on;
        b.angular_damping = 1.0f;
        make_player_colliders(e);
        orchard::Health& hp = m_scene.add_health(e);
        hp.on_death = orchard::DeathAction::respawn;
        hp.respawn_delay = 3.0f;
        m_scene.add_survival(e);
        orchard::apply_inventory_preset(m_scene.add_inventory(e), "minecraft");
        const orchard::Entity cam = m_scene.create("Camera", e);
        orchard::Camera& cc = m_scene.add_camera(cam);
        cc.control = orchard::CameraControl::walk;
        cc.fov = m_view.orbit_camera().fov;
        m_scene.transform(cam)->position = {0.0f, hk == orchard::HumanoidKind::detailed || hk == orchard::HumanoidKind::full_body ? 1.64f : cc.eye_height, flat ? 0.0f : 0.08f};
        orchard::Transform* t = m_scene.transform(e);
        if (flat) {
            const orchard::Vec2 c = m_view.pan_camera().center;
            t->position = {c.x, c.y, 0.0f};
            return e;
        }
        const orchard::Vec3 up = camera_up_axis();
        const orchard::Vec3 eye = m_view.eye();
        const orchard::Vec3 target_point = m_view.orbit_camera().target;
        orchard::Vec3 look = target_point - eye;
        look = look - up * orchard::dot(look, up);
        if (orchard::length(look) < 1e-4f) look = orchard::cross(up, orchard::Vec3{1.0f, 0.0f, 0.0f});
        t->position = target_point;
        const orchard::Vec3 y_axis{0.0f, 1.0f, 0.0f};
        const orchard::Vec3 axis = orchard::cross(y_axis, up);
        const float d = orchard::dot(y_axis, up);
        const orchard::Quat stand = d < -0.9999f ? orchard::Quat{1.0f, 0.0f, 0.0f, 0.0f} : orchard::normalize(orchard::Quat{axis.x, axis.y, axis.z, 1.0f + d});
        look = orchard::normalize(look);
        orchard::Vec3 face = orchard::rotate(stand, orchard::Vec3{0.0f, 0.0f, 1.0f});
        face = face - up * orchard::dot(face, up);
        const float yaw = std::atan2(orchard::dot(orchard::cross(face, look), up), orchard::dot(face, look));
        const orchard::Quat turn{up.x * std::sin(yaw * 0.5f), up.y * std::sin(yaw * 0.5f), up.z * std::sin(yaw * 0.5f), std::cos(yaw * 0.5f)};
        t->rotation = orchard::normalize(turn * stand);
        const orchard::Quat inv{-t->rotation.x, -t->rotation.y, -t->rotation.z, t->rotation.w};
        m_scene.transform(cam)->rotation = orchard::normalize(inv * camera_rotation(look));
        return e;
    }

    orchard::Entity add_preset(const std::string& kind, orchard::Entity parent) {
        const bool flat = m_scene.dimension() == orchard::Dimension::two_d;
        if (kind == "physical-player" || kind.rfind("physical-player:", 0) == 0) {
            std::optional<orchard::HumanoidKind> hk = orchard::HumanoidKind::basic;
            std::string style;
            if (kind.size() > 16) {
                std::string rest = kind.substr(16);
                const std::size_t colon = rest.find(':');
                if (colon != std::string::npos) {
                    style = rest.substr(colon + 1);
                    rest = rest.substr(0, colon);
                }
                hk = orchard::parse_humanoid_kind(rest);
            }
            if (!hk) return orchard::Entity{};
            return add_physical_player(*hk, parent, style);
        }
        if (kind.rfind("shape:", 0) == 0 || kind.rfind("triangle:", 0) == 0) {
            const bool tri = kind.rfind("triangle:", 0) == 0;
            const std::string what = kind.substr(tri ? 9 : 6);
            std::optional<orchard::ColliderShape> shape = tri ? std::optional<orchard::ColliderShape>(orchard::ColliderShape::triangle) : orchard::parse_collider_shape(what);
            if (!shape) return orchard::Entity{};
            if (flat && orchard::collider_shape_3d_only(*shape)) return orchard::Entity{};
            const orchard::Entity e = m_scene.create(tri ? "Triangle" : title_case(what), parent);
            m_scene.add_body(e);
            orchard::Collider* c = m_scene.add_collider(e, *shape);
            if (tri) {
                const std::optional<orchard::TriangleRule> rule = orchard::parse_triangle_rule(what);
                c->triangle = rule ? *rule : orchard::TriangleRule::sss;
                switch (c->triangle) {
                case orchard::TriangleRule::sss: c->triangle_sides = {1.0f, 1.0f, 1.0f}; break;
                case orchard::TriangleRule::sas: c->triangle_sides = {1.0f, 1.0f, 1.5f}; c->triangle_angles.x = 60.0f; break;
                case orchard::TriangleRule::asa: c->triangle_sides.x = 1.5f; c->triangle_angles.y = 50.0f; c->triangle_angles.z = 60.0f; break;
                case orchard::TriangleRule::aas: c->triangle_sides.x = 1.0f; c->triangle_angles.x = 40.0f; c->triangle_angles.y = 60.0f; break;
                case orchard::TriangleRule::ssa: c->triangle_sides = {1.2f, 1.5f, 1.0f}; c->triangle_angles.x = 40.0f; break;
                case orchard::TriangleRule::right: c->triangle_sides = {1.0f, 1.0f, 1.5f}; c->triangle_angles.x = 90.0f; break;
                }
                c->size.z = 1.0f;
                orchard::fit_triangle_size(*c);
            } else if (what == "capsule") {
                c->size = {0.6f, 1.6f, 0.6f};
            } else if (what == "dome" || what == "bowl") {
                c->shape = orchard::ColliderShape::sphere_section;
                c->polar_min = what == "bowl" ? 90.0f : 0.0f;
                c->polar_max = what == "bowl" ? 180.0f : 60.0f;
                c->shell = what == "bowl" ? 0.06f : 0.0f;
                c->size = {1.5f, 1.5f, 1.5f};
            }
            orchard::Transform* t = m_scene.transform(e);
            if (flat) {
                const orchard::Vec2 cc = m_view.pan_camera().center;
                t->position = {cc.x, cc.y + 3.0f, 0.0f};
            } else {
                t->position = m_view.orbit_camera().target + camera_up_axis() * 3.0f;
            }
            return e;
        }
        if (kind.rfind("light:", 0) == 0) {
            const std::string type = kind.substr(6);
            orchard::Light l;
            std::string name = "Light";
            if (type == "spot") {
                l = orchard::Light::spot();
                name = "Spot light";
            } else if (type == "sun" || type == "directional") {
                l = orchard::Light::sun();
                name = "Sun";
            } else if (type == "rectangle" || type == "area") {
                l = orchard::Light::area();
                name = "Area light";
            } else if (type == "disk") {
                l.type = orchard::LightType::disk;
                l.radius = 0.5f;
                name = "Disk light";
            } else if (type == "sphere") {
                l.type = orchard::LightType::sphere;
                l.radius = 0.25f;
                name = "Sphere light";
            } else if (type == "tube") {
                l.type = orchard::LightType::tube;
                l.length = 1.5f;
                l.radius = 0.05f;
                name = "Tube light";
            } else if (type != "point") {
                return orchard::Entity{};
            }
            const orchard::Entity e = m_scene.create(name, parent);
            m_scene.add_light(e) = l;
            orchard::Transform* t = m_scene.transform(e);
            if (flat) {
                const orchard::Vec2 cc = m_view.pan_camera().center;
                t->position = {cc.x, cc.y + 3.0f, 0.0f};
                return e;
            }
            const orchard::Vec3 up = camera_up_axis();
            const orchard::Vec3 target = m_view.orbit_camera().target;
            t->position = target + up * (l.type == orchard::LightType::directional ? 8.0f : 3.0f);
            if (l.type == orchard::LightType::spot || l.type == orchard::LightType::rectangle || l.type == orchard::LightType::disk) {
                t->rotation = camera_rotation(target - t->position + camera_forward_axis() * 0.01f);
            } else if (l.type == orchard::LightType::directional) {
                orchard::Vec3 side = orchard::cross(up, camera_forward_axis());
                if (orchard::length(side) < 1e-3f) side = orchard::Vec3{1.0f, 0.0f, 0.0f};
                t->rotation = camera_rotation(up * -1.0f + orchard::normalize(side) * 0.4f + camera_forward_axis() * 0.5f);
            }
            return e;
        }
        if (kind.rfind("effect:", 0) == 0) {
            const orchard::Entity e = orchard::add_effect(m_scene, kind.substr(7), parent);
            if (!m_scene.alive(e)) return orchard::Entity{};
            orchard::Transform* t = m_scene.transform(e);
            if (flat) {
                const orchard::Vec2 cc = m_view.pan_camera().center;
                t->position = {cc.x, cc.y + 0.5f, 0.0f};
            } else {
                t->position = m_view.orbit_camera().target;
            }
            return e;
        }
        if (kind.rfind("agent:", 0) == 0) return add_agent_entity(kind.substr(6), parent);
        if (kind.rfind("item:", 0) == 0 || kind == "chest" || kind == "locker" || kind.rfind("climate", 0) == 0) {
            const orchard::Vec3 at = flat ? orchard::Vec3{m_view.pan_camera().center.x, m_view.pan_camera().center.y, 0.0f} : m_view.orbit_camera().target;
            if (kind.rfind("item:", 0) == 0) {
                orchard::Gameplay g;
                g.start(&m_scene, nullptr);
                const orchard::Entity e = g.spawn_item(kind.substr(5), 1.0f, at + orchard::Vec3{0.0f, flat ? 0.5f : 0.3f, 0.0f}, "detailed");
                g.stop();
                if (m_scene.alive(e) && m_scene.alive(parent)) m_scene.set_parent(e, parent);
                ensure_item_models();
                return e;
            }
            if (kind == "chest" || kind == "locker") {
                const bool locker = kind == "locker";
                const orchard::Entity e = m_scene.create(locker ? "Locker" : "Chest", parent);
                m_scene.transform(e)->position = at + orchard::Vec3{0.0f, locker ? 0.9f : 0.3f, 0.0f};
                m_scene.add_body(e).type = orchard::BodyType::fixed;
                m_scene.add_collider(e)->size = locker ? orchard::Vec3{0.6f, 1.8f, 0.5f} : orchard::Vec3{0.9f, 0.6f, 0.55f};
                m_scene.add_material(e).color = locker ? orchard::Vec3{0.45f, 0.5f, 0.52f} : orchard::Vec3{0.52f, 0.34f, 0.18f};
                orchard::Inventory& inv = m_scene.add_inventory(e);
                orchard::apply_inventory_preset(inv, locker ? "tarkov" : "minecraft");
                inv.hotbar = false;
                inv.equipment = false;
                inv.open_keys.clear();
                inv.title = locker ? "Locker" : "Chest";
                if (locker) {
                    inv.columns = 6;
                    inv.rows = 8;
                } else {
                    inv.rows = 3;
                }
                m_scene.add_tag(e, "storage");
                return e;
            }
            const bool global = kind.rfind("climate-global:", 0) == 0;
            const std::string preset = kind.substr(kind.find(':') + 1);
            const orchard::Entity e = m_scene.create(std::string(global ? "Weather: " : "Climate zone: ") + title_case(preset), parent);
            m_scene.transform(e)->position = at;
            orchard::Climate& c = m_scene.add_climate(e);
            orchard::apply_climate_preset(c, preset);
            c.global = global;
            if (flat) c.size.z = 1.0f;
            return e;
        }
        if (kind == "patrol-route" || kind == "hiding-spot" || kind == "cover" || kind == "sound") {
            const orchard::Vec3 at = flat ? orchard::Vec3{m_view.pan_camera().center.x, m_view.pan_camera().center.y, 0.0f} : m_view.orbit_camera().target;
            if (kind == "sound") {
                const orchard::Entity e = m_scene.create("Sound", parent);
                m_scene.transform(e)->position = at;
                m_scene.add_sound(e);
                return e;
            }
            if (kind == "hiding-spot") {
                const orchard::Entity e = m_scene.create("Hiding spot", parent);
                m_scene.transform(e)->position = at;
                m_scene.add_tag(e, "hiding-spot");
                return e;
            }
            if (kind == "cover") {
                const orchard::Entity e = m_scene.create("Low wall", parent);
                m_scene.transform(e)->position = at + (flat ? orchard::Vec3{0.0f, 0.5f, 0.0f} : camera_up_axis() * 0.5f);
                m_scene.add_body(e).type = orchard::BodyType::fixed;
                m_scene.add_collider(e)->size = flat ? orchard::Vec3{0.4f, 1.0f, 1.0f} : orchard::Vec3{3.0f, 1.0f, 0.4f};
                return e;
            }
            const orchard::Entity route = m_scene.create("Patrol", parent);
            m_scene.transform(route)->position = at;
            const orchard::Vec3 offsets[4] = {{-4.0f, 0.0f, -4.0f}, {4.0f, 0.0f, -4.0f}, {4.0f, 0.0f, 4.0f}, {-4.0f, 0.0f, 4.0f}};
            for (int i = 0; i < (flat ? 2 : 4); ++i) {
                const orchard::Entity pt = m_scene.create("Point " + std::to_string(i + 1), route);
                m_scene.transform(pt)->position = flat ? orchard::Vec3{i == 0 ? -4.0f : 4.0f, 0.0f, 0.0f} : offsets[i];
            }
            return route;
        }
        if (kind == "barrel" || kind == "crate") {
            const bool barrel = kind == "barrel";
            const orchard::Entity e = m_scene.create(barrel ? "Explosive barrel" : "Crate", parent);
            m_scene.add_body(e);
            orchard::Collider* c = m_scene.add_collider(e, barrel && !flat ? orchard::ColliderShape::cylinder : orchard::ColliderShape::box);
            c->size = barrel ? orchard::Vec3{0.6f, 0.9f, 0.6f} : orchard::Vec3{0.8f, 0.8f, 0.8f};
            c->density = barrel ? 0.3f : 0.5f;
            orchard::Material& m = m_scene.add_material(e);
            m.color = barrel ? orchard::Vec3{0.75f, 0.12f, 0.08f} : orchard::Vec3{0.62f, 0.45f, 0.26f};
            orchard::Health& h = m_scene.add_health(e);
            h.value = h.max = barrel ? 25.0f : 60.0f;
            h.flammable = true;
            h.impact_damage = barrel ? 0.6f : 0.3f;
            h.on_death = barrel ? orchard::DeathAction::explode : orchard::DeathAction::shatter;
            m_scene.add_tag(e, kind);
            orchard::Transform* t = m_scene.transform(e);
            if (flat) {
                const orchard::Vec2 cc = m_view.pan_camera().center;
                t->position = {cc.x, cc.y + 3.0f, 0.0f};
            } else {
                t->position = m_view.orbit_camera().target + camera_up_axis() * 3.0f;
            }
            return e;
        }
        if (kind.rfind("particles:", 0) == 0) {
            const std::optional<orchard::Emitter> named = orchard::Emitter::named(kind.substr(10));
            if (!named) return orchard::Entity{};
            const orchard::Entity e = m_scene.create(title_case(kind.substr(10)), parent);
            m_scene.add_emitter(e) = *named;
            orchard::Transform* t = m_scene.transform(e);
            if (flat) {
                const orchard::Vec2 cc = m_view.pan_camera().center;
                t->position = {cc.x, cc.y + 0.5f, 0.0f};
            } else {
                t->position = m_view.orbit_camera().target + camera_up_axis() * 0.5f;
            }
            return e;
        }
        if (kind == "camera:fly") {
            const orchard::Entity e = add_preset("camera", parent);
            if (orchard::Camera* cam = m_scene.camera(e)) cam->control = orchard::CameraControl::fly;
            m_scene.set_name(e, "Flying camera");
            return e;
        }
        if (kind.rfind("human:", 0) == 0) {
            std::string rest = kind.substr(6);
            std::string style;
            const std::size_t colon = rest.find(':');
            if (colon != std::string::npos) {
                style = rest.substr(colon + 1);
                rest = rest.substr(0, colon);
            }
            const std::optional<orchard::HumanoidKind> hk = orchard::parse_humanoid_kind(rest);
            if (!hk) return orchard::Entity{};
            const orchard::Entity e = orchard::add_humanoid(m_scene, *hk, parent, *hk == orchard::HumanoidKind::basic ? "Player" : "Character");
            if (!style.empty() && orchard::set_character_style(m_scene, e, style)) ensure_character_models();
            orchard::Transform* t = m_scene.transform(e);
            if (flat) {
                const orchard::Vec2 c = m_view.pan_camera().center;
                t->position = {c.x, c.y, 0.0f};
            } else {
                t->position = m_view.orbit_camera().target;
            }
            return e;
        }
        if (kind.rfind("terrain:", 0) == 0) {
            if (flat) return orchard::Entity{};
            const std::string preset = kind.substr(8);
            const orchard::Entity e = m_scene.create(preset == "forest" ? "Forest" : "Terrain", parent);
            orchard::Terrain& t = m_scene.add_terrain(e);
            t = orchard::terrain_preset(preset);
            {
                orchard::TerrainHeights heights;
                orchard::generate_heights(t, heights);
                m_scene.transform(e)->position = m_view.orbit_camera().target - camera_up_axis() * heights.height_at(0.0f, 0.0f);
            }
            if (m_project)
                for (const orchard::TerrainScatter& sc : t.scatter)
                    for (const std::string& b : orchard::editor::builtin_model_names())
                        if (sc.model == "Assets/Models/" + b + ".glb" && !fs::exists(m_project->path_of(sc.model))) orchard::editor::write_builtin_model(b, m_project->path_of(sc.model));
            return e;
        }
        std::string name = "Entity";
        if (kind == "box") name = flat ? "Crate" : "Box";
        else if (kind == "ball") name = "Ball";
        else if (kind == "ground") name = "Ground";
        else if (kind == "trigger") name = "Trigger";
        else if (kind == "light") name = "Light";
        else if (kind == "camera") name = "Camera";
        else if (kind == "player-camera") name = "Player camera";
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
        } else if (kind == "camera" || kind == "player-camera") {
            for (orchard::Entity other : m_scene.entities())
                if (orchard::Camera* oc = m_scene.camera(other)) oc->active = false;
            m_scene.add_camera(e);
            if (kind == "player-camera") m_scene.camera(e)->control = flat ? orchard::CameraControl::none : orchard::CameraControl::walk;
            if (flat) {
                m_scene.camera(e)->view_height = static_cast<float>(m_view.height()) / std::max(1.0f, m_view.pan_camera().pixels_per_unit);
            } else {
                const orchard::Vec3 eye = m_view.eye();
                const orchard::Vec3 target = m_view.orbit_camera().target;
                orchard::Transform* t = m_scene.transform(e);
                t->position = eye;
                t->rotation = camera_rotation(target - eye);
                if (kind == "player-camera") {
                    const orchard::Vec3 up = camera_up_axis();
                    orchard::Vec3 look = target - eye;
                    look = look - up * orchard::dot(look, up);
                    if (orchard::length(look) < 1e-4f) look = orchard::cross(up, orchard::Vec3{1.0f, 0.0f, 0.0f});
                    t->position = target + up * m_scene.camera(e)->eye_height;
                    t->rotation = camera_rotation(look);
                }
                m_scene.camera(e)->fov = m_view.orbit_camera().fov;
            }
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
            static const char* const wall_names[5] = {"Floor", "Wall +X", "Wall -X", "Wall +Z", "Wall -Z"};
            int wall_index = 0;
            for (const auto& w2 : walls) {
                orchard::Collider* c = m_scene.add_collider(e);
                c->name = wall_names[wall_index++];
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

        native("graph_open", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string("error\topen a project first");
            const std::string rel = arg(f, 0);
            if (rel.empty() || rel.find("..") != std::string::npos) return std::string("error\tbad graph path");
            if (!m_graphs.count(rel)) {
                std::string error;
                orchard::editor::GraphView view;
                if (!view.load_text(read_file(m_project->path_of(rel)), &error)) return "error\t" + rel + ": " + clean(error);
                m_graphs[rel] = view;
            }
            m_graph_open = rel;
            m_graph_close_warned.clear();
            redraw_graph();
            return "ok\t" + fs::path(rel).stem().string();
        });

        native("graph_show", [this](const std::vector<std::string>& f) {
            const std::string rel = arg(f, 0);
            if (!rel.empty() && !m_graphs.count(rel)) return std::string("error\tthat graph isn't open");
            m_graph_open = rel;
            if (rel.empty()) redraw();
            else redraw_graph();
            return std::string("ok");
        });

        native("graph_tabs", [this](const std::vector<std::string>&) {
            std::string out;
            for (const auto& kv : m_graphs) out += kv.first + "\t" + fs::path(kv.first).stem().string() + "\t" + (kv.second.dirty() ? "1" : "0") + "\n";
            return out;
        });

        native("graph_close", [this](const std::vector<std::string>& f) {
            const std::string rel = arg(f, 0);
            const auto it = m_graphs.find(rel);
            if (it == m_graphs.end()) return std::string("ok");
            if (it->second.dirty() && m_graph_close_warned != rel && arg(f, 1) != "force") {
                m_graph_close_warned = rel;
                return std::string("dirty");
            }
            m_graphs.erase(it);
            m_graph_close_warned.clear();
            if (m_graph_open == rel) m_graph_open.clear();
            return std::string("ok");
        });

        native("graph_save", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string("error\tno project is open");
            const std::string rel = arg(f, 0).empty() ? m_graph_open : arg(f, 0);
            const auto it = m_graphs.find(rel);
            if (it == m_graphs.end()) return std::string("error\tno graph is open");
            std::ofstream out(m_project->path_of(rel), std::ios::binary | std::ios::trunc);
            out << it->second.save_text();
            if (!out) return "error\tcan't write " + rel;
            it->second.mark_saved();
            m_graph_close_warned.clear();
            redraw_graph();
            return std::string("ok");
        });

        native("graph_status", [this](const std::vector<std::string>&) { return graph_status_text(); });

        native("graph_press", [this](const std::vector<std::string>& f) {
            float x = 0.0f, y = 0.0f;
            orchard::editor::GraphView* g = active_graph();
            if (!g || !graph_point(arg(f, 0), arg(f, 1), x, y)) return std::string();
            if (g->press(x, y, std::atoi(arg(f, 2).c_str()), arg(f, 3) == "1", arg(f, 4) == "1")) redraw_graph();
            return std::string("ok");
        });

        native("graph_move", [this](const std::vector<std::string>& f) {
            float x = 0.0f, y = 0.0f;
            orchard::editor::GraphView* g = active_graph();
            if (!g || !graph_point(arg(f, 0), arg(f, 1), x, y)) return std::string();
            if (g->drag(x, y)) redraw_graph();
            return std::string("ok");
        });

        native("graph_release", [this](const std::vector<std::string>& f) {
            float x = 0.0f, y = 0.0f;
            orchard::editor::GraphView* g = active_graph();
            if (!g || !graph_point(arg(f, 0), arg(f, 1), x, y)) return std::string();
            const orchard::editor::GraphEvent ev = g->release(x, y);
            redraw_graph();
            if (ev.kind == orchard::editor::GraphEventKind::menu) {
                m_graph_menu_hit = ev.hit;
                m_graph_menu_x = x;
                m_graph_menu_y = y;
                m_graph_menu_at_cursor = true;
                if (grove::Node* canvas = m_doc.find_by_id("graph-canvas")) {
                    const std::vector<grove::MenuItem> items = g->has_pending_link() ? graph_add_items() : graph_menu_items(ev.hit);
                    grove::open_menu(m_doc, *canvas, items, std::atof(arg(f, 0).c_str()), std::atof(arg(f, 1).c_str()));
                }
                return std::string("menu");
            }
            return graph_event_text(ev);
        });

        native("graph_wheel", [this](const std::vector<std::string>& f) {
            float x = 0.0f, y = 0.0f;
            orchard::editor::GraphView* g = active_graph();
            if (!g || !graph_point(arg(f, 0), arg(f, 1), x, y)) return std::string();
            if (g->wheel(x, y, static_cast<float>(std::atof(arg(f, 2).c_str())))) redraw_graph();
            return std::string("ok");
        });

        native("graph_key", [this](const std::vector<std::string>& f) {
            orchard::editor::GraphView* g = active_graph();
            if (!g) return std::string();
            if (!g->key(arg(f, 0), arg(f, 1) == "1", arg(f, 2) == "1")) return std::string();
            redraw_graph();
            return std::string("changed");
        });

        native("graph_add_menu", [this](const std::vector<std::string>& f) {
            orchard::editor::GraphView* g = active_graph();
            grove::Node* canvas = m_doc.find_by_id("graph-canvas");
            if (!g || !canvas) return std::string("error\topen a graph first");
            g->clear_pending_link();
            m_graph_menu_at_cursor = false;
            grove::open_menu(m_doc, *canvas, graph_add_items(), std::atof(arg(f, 0).c_str()), std::atof(arg(f, 1).c_str()));
            return std::string("ok");
        });

        native("graph_menu_action", [this](const std::vector<std::string>& f) {
            orchard::editor::GraphView* g = active_graph();
            if (!g) return std::string();
            const std::string key = arg(f, 0);
            std::string out = "changed";
            if (key.rfind("add:", 0) == 0) {
                const std::string type = key.substr(4);
                const int id = m_graph_menu_at_cursor ? g->add_node(type, m_graph_menu_x, m_graph_menu_y) : g->add_node_centered(type);
                if (!id) out = "error\tunknown node type " + type;
            } else if (key == "delete") {
                g->delete_selected();
            } else if (key == "duplicate") {
                g->duplicate_selected();
            } else if (key == "unlink") {
                g->remove_link(m_graph_menu_hit.link);
            } else if (key.rfind("unpin:", 0) == 0) {
                g->disconnect(m_graph_menu_hit.node, key.substr(6));
            } else if (key == "frame") {
                g->frame_all();
                out = "redraw";
            } else if (key == "frame_selected") {
                g->frame_selected();
                out = "redraw";
            } else if (key == "undo") {
                g->undo();
            } else if (key == "redo") {
                g->redo();
            } else {
                out = "none";
            }
            g->clear_pending_link();
            m_graph_menu_at_cursor = false;
            redraw_graph();
            return out;
        });

        native("graph_set_value", [this](const std::vector<std::string>& f) {
            orchard::editor::GraphView* g = active_graph();
            if (!g || !m_graph_edit_node) return std::string("error\tnothing is being edited");
            if (!g->set_value(m_graph_edit_node, m_graph_edit_pin, arg(f, 0))) return std::string("error\tcan't set that value");
            redraw_graph();
            return std::string("changed");
        });

        native("graph_walnut", [this](const std::vector<std::string>&) {
            orchard::editor::GraphView* g = active_graph();
            if (!g) return std::string();
            const orchard::editor::GraphCompile c = g->compile();
            std::string code;
            for (const std::string& e : c.errors) code += (g->is_brain() ? "# error: " : "// error: ") + e + "\n";
            return code + c.code;
        });

        native("graph_catalog", [this](const std::vector<std::string>&) {
            std::string out;
            for (const orchard::editor::GraphCategory& cat : orchard::editor::GraphView::catalog(open_graph_kind())) {
                for (const orchard::editor::GraphCatalogEntry& t : cat.types) out += cat.name + "\t" + t.id + "\t" + t.title + "\n";
            }
            return out;
        });

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
            std::vector<std::string> terrain_errors;
            m_gpu.terrain().save_all(m_scene, &terrain_errors);
            if (!terrain_errors.empty()) return "error\t" + terrain_errors.front();

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
            const orchard::Entity e = entity(arg(f, 0));
            if (arg(f, 1) == "add") {
                if (!m_scene.alive(e)) {
                } else if (e == m_selected) {
                    m_selected = orchard::Entity{};
                    if (!m_also.empty()) {
                        m_selected = m_also.back();
                        m_also.pop_back();
                    }
                } else if (std::find(m_also.begin(), m_also.end(), e) != m_also.end()) {
                    m_also.erase(std::find(m_also.begin(), m_also.end(), e));
                } else {
                    if (m_scene.alive(m_selected)) m_also.push_back(m_selected);
                    m_selected = e;
                }
            } else if (arg(f, 1) != "keep") {
                m_selected = e;
                m_also.clear();
            } else {
                m_selected = e;
            }
            prune_selection();
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
            m_component_error.clear();
            if (!set_component(entity(arg(f, 0)), arg(f, 1), arg(f, 2))) return m_component_error.empty() ? std::string() : "error\t" + m_component_error;
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
            m_session.set_play_dimension(m_view.dimension());
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
            const bool down = arg(f, 1) == "1";
            if (arg(f, 0) == "*" && !down) {
                m_session.scripts().clear_keys();
                return std::string("ok");
            }
            const int logical = std::atoi(arg(f, 2).c_str());
            const int physical = std::atoi(arg(f, 3).c_str());
            if (logical > 0 || physical > 0) m_session.scripts().set_key(logical, physical, down);
            else m_session.scripts().set_key(arg(f, 0), down);
            if (play_input(arg(f, 0), down)) redraw();
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
            float x = 0.0f, y = 0.0f;
            if (!to_canvas(arg(f, 0), arg(f, 1), x, y)) return std::string();
            if (m_session.playing()) {
                if (hud_press(x, y, std::atoi(arg(f, 2).c_str()), arg(f, 3) == "1")) return std::string("ok");
                play_input("mouse" + arg(f, 2), true);
            }
            m_drag = true;
            m_drag_button = std::atoi(arg(f, 2).c_str());
            const bool shift = arg(f, 3) == "1";
            m_drag_pan = arg(f, 2) != "1" || (shift && m_tool != "move" && m_tool != "rotate" && m_tool != "scale") || m_tool == "pan";
            m_move_entity = orchard::Entity{};
            m_snap_rotation = shift;
            if (m_drag_button == 1 && m_tool == "move") begin_move(x, y, shift);
            if (m_drag_button == 1 && (m_tool == "rotate" || m_tool == "scale")) begin_gizmo(x, y);
            m_last_x = m_start_x = x;
            m_last_y = m_start_y = y;
            return std::string("ok");
        });

        native("viewport_move", [this](const std::vector<std::string>& f) {
            float x = 0.0f, y = 0.0f;
            if (m_session.playing() && to_canvas(arg(f, 0), arg(f, 1), x, y)) {
                m_mouse_x = x;
                m_mouse_y = y;
                if (screen_open()) {
                    redraw();
                    return std::string("ok");
                }
            }
            if (!m_drag || !to_canvas(arg(f, 0), arg(f, 1), x, y)) return std::string();
            if (m_move_entity.valid()) {
                continue_move(x, y);
                m_last_x = x;
                m_last_y = y;
                return std::string("ok");
            }
            if (m_session.playing() && m_player_ready) {
                const orchard::Camera* pc = m_scene.camera(m_scene.active_camera());
                if (pc && pc->mouse_look && pc->control != orchard::CameraControl::none) {
                    m_player_yaw -= (x - m_last_x) * pc->look_speed;
                    m_player_pitch = std::clamp(m_player_pitch - (y - m_last_y) * pc->look_speed, -89.0f, 89.0f);
                    m_last_x = x;
                    m_last_y = y;
                    redraw();
                    return std::string("ok");
                }
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
            float x = 0.0f, y = 0.0f;
            if (m_session.playing() && to_canvas(arg(f, 0), arg(f, 1), x, y) && hud_release(x, y)) {
                m_drag = false;
                return std::string("ok");
            }
            const bool was = m_drag;
            m_drag = false;
            if (m_session.playing() && was) {
                play_input("mouse" + std::to_string(m_drag_button), false);
                if (!m_move_entity.valid()) return std::string("drag");
            }
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
                const orchard::Entity hit = pick_entity(x, y);
                if (m_scene.alive(hit) && hit != m_selected && std::find(m_also.begin(), m_also.end(), hit) == m_also.end()) {
                    m_selected = hit;
                    m_also.clear();
                }
                redraw();
                return "menu	" + (m_scene.alive(hit) ? std::to_string(hit.id()) : std::string());
            }
            if (moved > 4.0f || m_drag_pan || m_tool == "orbit") return std::string("drag");
            if (arg(f, 2) == "1") {
                const orchard::Entity hit = pick_entity(x, y);
                return "pick\t" + (m_scene.alive(hit) ? std::to_string(hit.id()) : std::string()) + "\tadd";
            }
            m_selected = pick_entity(x, y);
            m_also.clear();
            redraw();
            return "pick\t" + (m_scene.alive(m_selected) ? std::to_string(m_selected.id()) : std::string());
        });

        native("viewport_zoom", [this](const std::vector<std::string>& f) {
            if (m_session.playing()) {
                const orchard::Entity owner = play_owner();
                const orchard::Inventory* inv = m_scene.inventory(owner);
                if (inv && inv->enabled && inv->hotbar && inv->hotbar_scroll && !screen_open()) {
                    const double d = std::atof(arg(f, 0).c_str());
                    if (d != 0.0) m_session.scripts().gameplay().scroll_slot(owner, d > 0.0 ? 1 : -1);
                    redraw();
                    return std::string("ok");
                }
            }
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
            num("settle", "Help slow things settle (0 = off)", ph.settle, 0.0, 10.0, 0.1, "slider");
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
            else if (key == "settle") ph.settle = real(0.0, 10.0);
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

        native("terrain_brush", [this](const std::vector<std::string>& f) {
            if (const std::optional<orchard::TerrainBrushKind> k = orchard::parse_terrain_brush(arg(f, 0))) m_brush.kind = *k;
            if (!arg(f, 1).empty()) m_brush.radius = std::max(0.1f, static_cast<float>(std::atof(arg(f, 1).c_str())));
            if (!arg(f, 2).empty()) m_brush.strength = std::max(0.0f, static_cast<float>(std::atof(arg(f, 2).c_str())));
            if (!arg(f, 3).empty()) m_brush.falloff = std::clamp(static_cast<float>(std::atof(arg(f, 3).c_str())), 0.0f, 1.0f);
            if (!arg(f, 4).empty()) m_brush.target = static_cast<float>(std::atof(arg(f, 4).c_str()));
            return std::string(orchard::to_string(m_brush.kind));
        });

        native("terrain_pick", [this](const std::vector<std::string>& f) {
            float x = 0.0f, y = 0.0f;
            orchard::Vec3 origin, dir, point;
            orchard::Entity hit;
            if (!to_canvas(arg(f, 0), arg(f, 1), x, y) || !m_view.ray(x, y, origin, dir) || !m_gpu.terrain().pick(m_scene, origin, dir, 1e5f, hit, point)) {
                m_gpu.set_terrain_brush(orchard::Entity{}, {}, 0.0f, false);
                redraw();
                return std::string();
            }
            m_gpu.set_terrain_brush(hit, point, m_brush.radius, m_tool == "sculpt");
            redraw();
            return std::to_string(hit.id()) + "\t" + number(point.x) + "\t" + number(point.y) + "\t" + number(point.z) + "\t" + number(m_gpu.terrain().local_height(m_scene, hit, point));
        });

        native("terrain_sculpt", [this](const std::vector<std::string>& f) {
            if (m_session.playing() || m_session.building()) return std::string("error\tstop playing before sculpting");
            const orchard::Entity e = entity(arg(f, 0));
            const orchard::Vec3 hit{static_cast<float>(std::atof(arg(f, 1).c_str())), static_cast<float>(std::atof(arg(f, 2).c_str())), static_cast<float>(std::atof(arg(f, 3).c_str()))};
            orchard::TerrainBrush b = m_brush;
            if (const std::optional<orchard::TerrainBrushKind> k = orchard::parse_terrain_brush(arg(f, 4))) b.kind = *k;
            if (!arg(f, 5).empty()) b.radius = std::max(0.1f, static_cast<float>(std::atof(arg(f, 5).c_str())));
            if (!arg(f, 6).empty()) b.strength = std::max(0.0f, static_cast<float>(std::atof(arg(f, 6).c_str())));
            if (!arg(f, 7).empty()) b.falloff = std::clamp(static_cast<float>(std::atof(arg(f, 7).c_str())), 0.0f, 1.0f);
            if (!arg(f, 8).empty()) b.target = static_cast<float>(std::atof(arg(f, 8).c_str()));
            const std::string phase = arg(f, 9);
            if (phase == "begin") {
                m_sculpt_entity = e;
                m_sculpting = true;
                if (b.kind == orchard::TerrainBrushKind::flatten && arg(f, 8).empty()) m_brush.target = m_gpu.terrain().local_height(m_scene, e, hit);
            }
            if (phase == "end") {
                m_sculpting = false;
                return std::string("ok");
            }
            if (b.kind == orchard::TerrainBrushKind::flatten && arg(f, 8).empty()) b.target = m_brush.target;
            if (!m_gpu.terrain().sculpt(m_scene, e, hit, b, 1.0f)) return std::string();
            m_gpu.set_terrain_brush(e, hit, b.radius, true);
            m_dirty = true;
            redraw();
            return std::string("ok");
        });

        native("terrain_import", [this](const std::vector<std::string>& f) {
            const orchard::Entity e = entity(arg(f, 0));
            orchard::Terrain* tr = m_scene.terrain(e);
            if (!tr) return std::string("error\tpick a terrain first");
            tr->heightmap = arg(f, 1);
            std::string error;
            if (!m_gpu.terrain().import_heightmap(m_scene, e, tr->heightmap, &error)) return "error\t" + error;
            touched();
            return std::string("ok");
        });

        native("set_tool", [this](const std::vector<std::string>& f) {
            const std::string t = arg(f, 0);
            if (t == "select" || t == "move" || t == "rotate" || t == "scale" || t == "orbit" || t == "pan" || t == "sculpt") m_tool = t;
            if (m_tool != "sculpt") m_gpu.set_terrain_brush(orchard::Entity{}, {}, 0.0f, false);
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
            for (const std::string& s : m_project->models()) out += "model\t" + s + "\n";
            return out;
        });

        native("file_tree", [this](const std::vector<std::string>&) {
            std::string out;
            if (!m_project) return out;
            std::vector<std::pair<std::string, std::string>> all;
            for (const std::string& s : m_project->scenes()) all.emplace_back(s, "scene");
            for (const std::string& s : m_project->scripts()) all.emplace_back(s, "script");
            for (const std::string& s : m_project->graphs()) all.emplace_back(s, "graph");
            for (const std::string& s : m_project->behaviors()) all.emplace_back(s, orchard::graph::is_brain_file(s) ? "graph" : "script");
            for (const std::string& s : m_project->entity_templates()) all.emplace_back(s, "entity");
            for (const std::string& s : m_project->models()) all.emplace_back(s, "model");
            for (const std::string& s : m_project->images()) all.emplace_back(s, "image");
            std::sort(all.begin(), all.end(), [](const auto& a, const auto& b) {
                const std::size_t sa = std::count(a.first.begin(), a.first.end(), '/'), sb = std::count(b.first.begin(), b.first.end(), '/');
                const std::string da = a.first.substr(0, a.first.rfind('/') == std::string::npos ? 0 : a.first.rfind('/'));
                const std::string db = b.first.substr(0, b.first.rfind('/') == std::string::npos ? 0 : b.first.rfind('/'));
                if (da != db) {
                    if (sa == 0 || sb == 0) return sa != 0;
                    std::string ka = da, kb = db;
                    std::replace(ka.begin(), ka.end(), '/', '\x01');
                    std::replace(kb.begin(), kb.end(), '/', '\x01');
                    return ka < kb;
                }
                return a.first < b.first;
            });
            all.erase(std::unique(all.begin(), all.end(), [](const auto& a, const auto& b) { return a.first == b.first; }), all.end());
            const std::string closed = pref("files.closed", "");
            auto is_closed = [&closed](const std::string& folder) { return closed.find("|" + folder + "|") != std::string::npos; };
            std::set<std::string> shown;
            for (const auto& [path, kind] : all) {
                std::vector<std::string> parts;
                std::size_t at = 0;
                while (true) {
                    const std::size_t slash = path.find('/', at);
                    if (slash == std::string::npos) break;
                    parts.push_back(path.substr(0, slash));
                    at = slash + 1;
                }
                bool hidden = false;
                for (std::size_t d = 0; d < parts.size() && !hidden; ++d) {
                    const std::string& folder = parts[d];
                    if (!shown.count(folder)) {
                        shown.insert(folder);
                        std::size_t count = 0;
                        for (const auto& other : all)
                            if (other.first.rfind(folder + "/", 0) == 0) ++count;
                        const std::size_t cut = folder.rfind('/');
                        out += std::to_string(d) + "\tfolder\t" + (cut == std::string::npos ? folder : folder.substr(cut + 1)) + "\t" + folder + "\t" + (is_closed(folder) ? "closed" : "open") + "\t" + std::to_string(count) + "\n";
                    }
                    if (is_closed(folder)) hidden = true;
                }
                if (hidden) continue;
                const std::size_t cut = path.rfind('/');
                out += std::to_string(parts.size()) + "\t" + kind + "\t" + (cut == std::string::npos ? path : path.substr(cut + 1)) + "\t" + path + "\t\t\n";
            }
            return out;
        });

        native("toggle_folder", [this](const std::vector<std::string>& f) {
            const std::string folder = arg(f, 0);
            if (folder.empty() || folder.find('|') != std::string::npos) return std::string();
            std::string closed = pref("files.closed", "");
            const std::string key = "|" + folder + "|";
            const std::size_t at = closed.find(key);
            if (at != std::string::npos) closed.erase(at, key.size() - 1);
            else closed += (closed.empty() ? "|" : "") + folder + "|";
            if (closed == "|") closed.clear();
            m_prefs["files.closed"] = closed;
            save_prefs();
            return std::string("ok");
        });

        native("place_model", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string("error\topen a project first");
            if (m_session.playing()) return std::string("error\tstop playing first");
            const std::string rel = arg(f, 0);
            if (rel.empty() || rel.find("..") != std::string::npos) return std::string("error\tbad model path");
            std::string error;
            if (!m_gpu.models().model(rel, &error)) return "error\t" + clean(error.empty() ? "can't read " + rel : error);
            const orchard::Entity e = place_model(rel, orchard::Entity{});
            m_selected = e;
            m_light_dirty = true;
            touched();
            redraw();
            return "select\t" + std::to_string(e.id());
        });

        native("import_files", [this](const std::vector<std::string>& f) {
            std::string list;
            for (std::size_t i = 0; i < f.size(); ++i) list += (i ? "\n" : "") + f[i];
            return import_files(list);
        });

        native("read_text", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string();
            const std::string rel = arg(f, 0);
            if (rel.find("..") != std::string::npos) return std::string();
            return read_file(m_project->path_of(rel));
        });

        native("write_text", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string("error\tno project is open");
            const std::string rel = arg(f, 0);
            if (rel.empty() || rel.find("..") != std::string::npos) return std::string("error\tbad file name");
            std::string text;
            for (std::size_t i = 1; i < f.size(); ++i) text += (i > 1 ? "\t" : "") + f[i];
            std::ofstream out(m_project->path_of(rel), std::ios::binary | std::ios::trunc);
            if (!out) return "error\tcan't write " + rel;
            out << text;
            if (!out) return "error\tcan't write " + rel;
            return std::string("ok");
        });

        native("editor_command", [this](const std::vector<std::string>&) {
            const std::string mine = pref("editor.command", "");
            return mine.empty() ? default_editor_command() : mine;
        });

        native("open_external", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string("error\tno project is open");
            const std::string rel = arg(f, 0);
            if (rel.find("..") != std::string::npos) return std::string("error\tbad file name");
            std::string command = pref("editor.command", "");
            if (command.empty()) command = default_editor_command();
            const std::string file = m_project->path_of(rel);
            std::string line;
            if (command.find("{file}") != std::string::npos) {
                line = command;
                line.replace(line.find("{file}"), 6, shell_quote(file));
            } else {
                line = command + " " + shell_quote(file);
            }
            if (!launch_detached(line)) return "error\tcouldn't run: " + command;
            return "ok\t" + command;
        });

        native("open_system", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string("error\tno project is open");
            const std::string rel = arg(f, 0);
            if (rel.find("..") != std::string::npos) return std::string("error\tbad file name");
#ifdef _WIN32
            const std::string line = shell_quote(m_project->path_of(rel));
#elif defined(__APPLE__)
            const std::string line = "open " + shell_quote(m_project->path_of(rel));
#else
            const std::string line = "xdg-open " + shell_quote(m_project->path_of(rel));
#endif
            if (!launch_detached(line)) return std::string("error\tcouldn't open it");
            return std::string("ok");
        });

        native("file_menu", [this](const std::vector<std::string>& f) {
            grove::Node* owner = m_doc.find_by_id("files");
            if (!owner) return std::string();
            const std::string rel = arg(f, 0);
            const bool graph = fs::path(rel).extension() == ".graph" || orchard::graph::is_brain_file(rel);
            const bool scene_file = fs::path(rel).extension() == ".scene" || fs::path(rel).extension() == ".entity";
            std::vector<grove::MenuItem> items;
            if (orchard::is_model_file(rel)) {
                items.push_back(grove::menu_item("place", "Place it in the scene"));
                items.push_back(grove::menu_item("system", "Open it with my apps"));
                const bool ok = grove::open_menu(m_doc, *owner, items, std::atof(arg(f, 1).c_str()), std::atof(arg(f, 2).c_str()));
                return ok ? std::string("ok") : std::string();
            }
            if (graph) items.push_back(grove::menu_item("graph", "Open the graph"));
            else if (!scene_file) items.push_back(grove::menu_item("here", "Edit in Orchard"));
            items.push_back(grove::menu_item("external", "Open in my editor (" + (pref("editor.command", "").empty() ? default_editor_command() : pref("editor.command", "")) + ")"));
            items.push_back(grove::menu_item("preview", orchard::graph::is_brain_file(rel) ? "Show the behavior tree it makes" : graph ? "Show the Walnut it makes" : "Show it below"));
            const bool ok = grove::open_menu(m_doc, *owner, items, std::atof(arg(f, 1).c_str()), std::atof(arg(f, 2).c_str()));
            return ok ? std::string("ok") : std::string();
        });

        native("read_file", [this](const std::vector<std::string>& f) {
            if (!m_project) return std::string();
            const std::string rel = arg(f, 0);
            if (rel.find("..") != std::string::npos) return std::string();
            const std::string text = read_file(m_project->path_of(rel));
            const bool brain = orchard::graph::is_brain_file(rel);
            if (fs::path(rel).extension() != ".graph" && !brain) return text;
            std::string error;
            const std::optional<orchard::graph::Graph> g = orchard::graph::Graph::load(text, &error);
            if (!g) return (brain ? "# " : "// ") + error + "\n";
            std::vector<std::string> errors;
            std::string code = g->compile(&errors);
            for (const std::string& e : errors) code = (brain ? "# error: " : "// error: ") + e + "\n" + code;
            return code;
        });

        native("outline", [this](const std::vector<std::string>& f) {
            const std::string group = arg(f, 0).empty() ? pref("outliner.group", "sections") : arg(f, 0);
            return outline(group, arg(f, 1));
        });

        native("outliner_prefs", [this](const std::vector<std::string>&) {
            return pref("outliner.side", "left") + "\t" + pref("outliner.group", "sections") + "\t" + pref("outliner.sort", "scene") + "\t" + pref("outliner.empty_sections", "0") + "\t" + pref("outliner.collapsed", "") + "\t" + pref("preview_effects", "0") + "\t" + pref("details.open", "1") + "\t" + pref("viewport.trace", "off") + "\t" + pref("viewport.cull", "1") + "\t" + pref("viewport.shadows", "1") + "\t" + pref("viewport.lights", "1") + "\t" + pref("viewport.materials", "1") + "\t" + pref("viewport.particles", "1") + "\t" + pref("viewport.quality", "medium") + "\t" + pref("files.folded", "0");
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

        native("toggle_fold", [this](const std::vector<std::string>& f) {
            const std::string id = arg(f, 0);
            if (id.empty() || id.find(',') != std::string::npos) return std::string();
            std::vector<std::string> keep;
            bool was = false;
            std::string cur;
            const std::string list = pref("outliner.open", "") + ",";
            for (char c : list) {
                if (c != ',') {
                    cur += c;
                    continue;
                }
                if (!cur.empty()) {
                    if (cur == id) was = true;
                    else if (m_scene.alive(entity(cur))) keep.push_back(cur);
                }
                cur.clear();
            }
            const orchard::Entity e = entity(id);
            if (!was && m_scene.alive(e)) keep.push_back(id);
            if (was && m_scene.alive(m_selected)) {
                for (orchard::Entity at = m_scene.parent(m_selected); m_scene.alive(at); at = m_scene.parent(at)) {
                    if (at == e) {
                        m_selected = e;
                        break;
                    }
                }
            }
            std::string out;
            for (const std::string& k : keep) out += (out.empty() ? "" : ",") + k;
            m_prefs["outliner.open"] = out;
            save_prefs();
            return std::string(was ? "closed" : "open") + "\t" + (m_scene.alive(m_selected) ? std::to_string(m_selected.id()) : std::string());
        });

        native("fold_all", [this](const std::vector<std::string>& f) {
            std::string out;
            if (arg(f, 0) == "open") {
                for (orchard::Entity e : m_scene.entities()) {
                    if (m_scene.children(e).empty() && outline_parts(e).empty()) continue;
                    out += (out.empty() ? "" : ",") + std::to_string(e.id());
                }
            }
            m_prefs["outliner.open"] = out;
            save_prefs();
            return std::string("ok");
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

        native("selection_more", [this](const std::vector<std::string>&) {
            prune_selection();
            std::string out;
            for (orchard::Entity x : m_also) out += (out.empty() ? "" : ",") + std::to_string(x.id());
            return out;
        });

        native("context_menu", [this](const std::vector<std::string>& f) {
            grove::Node* owner = m_doc.find_by_id(arg(f, 0));
            if (!owner) return std::string();
            const orchard::Entity e = entity(arg(f, 1));
            if (m_scene.alive(e) && e != m_selected && std::find(m_also.begin(), m_also.end(), e) == m_also.end()) {
                m_selected = e;
                m_also.clear();
                redraw();
            }
            m_menu_page.clear();
            m_menu_owner = arg(f, 0);
            m_menu_x = std::atof(arg(f, 2).c_str());
            m_menu_y = std::atof(arg(f, 3).c_str());
            const bool ok = grove::open_menu(m_doc, *owner, menu_items(e), m_menu_x, m_menu_y);
            return ok ? (m_scene.alive(e) && m_scene.alive(m_selected) ? std::to_string(m_selected.id()) : std::string("none")) : std::string();
        });

        native("trace_status", [this](const std::vector<std::string>&) {
            return pref("viewport.trace", "off") + "\t" + std::to_string(m_gpu.traced_samples()) + "\t" + clean(m_gpu.trace_error());
        });

        native("menu_action", [this](const std::vector<std::string>& f) { return menu_action(entity(arg(f, 0)), arg(f, 1)); });

        native("viewport_pick", [this](const std::vector<std::string>& f) {
            float x = 0.0f, y = 0.0f;
            if (!to_canvas(arg(f, 0), arg(f, 1), x, y)) return std::string();
            const orchard::Entity e = pick_entity(x, y);
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
    std::vector<orchard::Entity> m_also;
    orchard::Viewport m_view;
    orchard::editor::GpuView m_gpu;
    orchard::editor::AxesPreview m_axes_preview;
    orchard::editor::AxesChoice m_axes_choice;
    bool m_animation_was_running = false;
    unsigned m_model_check = 0;
    unsigned m_item_model_check = 0;
    std::map<std::string, orchard::editor::GraphView> m_graphs;
    orchard::editor::GraphView m_no_graph;
    std::string m_graph_open;
    std::string m_graph_close_warned;
    orchard::editor::GraphHit m_graph_menu_hit;
    float m_graph_menu_x = 0.0f;
    float m_graph_menu_y = 0.0f;
    bool m_graph_menu_at_cursor = false;
    int m_graph_edit_node = 0;
    std::string m_graph_edit_pin;
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
    chlorophyll::ModelPoser m_trace_poser;
    std::map<std::string, std::shared_ptr<const fizmo::lighting::Texture>> m_trace_builtin;
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
    std::optional<orchard::Viewport> m_view_before_play;
    bool m_player_ready = false;
    std::vector<std::pair<orchard::Entity, orchard::Transform>> m_smoothed;
    orchard::editor::SoundPlayer m_sounds;
    std::string m_component_error;
    int m_trace_spp = 1;
    std::string m_menu_page;
    std::string m_menu_owner;
    double m_menu_x = 0.0;
    double m_menu_y = 0.0;
    bool m_overlay_only = false;
    float m_player_yaw = 0.0f;
    float m_player_pitch = 0.0f;
    float m_player_fall = 0.0f;
    std::chrono::steady_clock::time_point m_particle_burst_until{};
    std::string m_template_path;
    std::optional<orchard::Scene> m_main_scene;
    orchard::Entity m_main_selected;
    bool m_main_dirty = false;
    std::string m_tool = "select";
    orchard::TerrainBrush m_brush;
    orchard::Entity m_sculpt_entity;
    bool m_sculpting = false;
    orchard::Entity m_move_entity;
    std::string m_gizmo_mode = "move";
    orchard::Quat m_gizmo_start_rotation;
    orchard::Vec3 m_gizmo_start_scale{1.0f, 1.0f, 1.0f};
    bool m_snap_rotation = false;
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
    world.set_asset_root(project->folder());
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
    const std::string code = g->compile(&errors);
    for (const std::string& e : errors) std::cerr << path << ": " << e << "\n";
    std::cout << code;
    return errors.empty() ? 0 : 1;
}

const char* usage =
    "usage: orchard [PROJECT]\n"
    "       orchard --new FOLDER [--2d | --3d] [--name NAME] [--axes \"STYLE [flip-x] [flip-y] [flip-z]\"]\n"
    "       orchard --graph FILE.graph | FILE.brain\n"
    "       orchard --simulate PROJECT [SECONDS]\n"
    "       orchard [--check] [--bundle OUTPUT] [--version]\n\n"
    "  PROJECT          a project folder or its project.orchard to open\n"
    "  --new FOLDER     make a new project (3d unless --2d) and exit\n"
    "  --axes AXES      which way is up in a new 3d project (default y-up; unity, unreal, blender and source work too), plus any of flip-x flip-y flip-z\n"
    "  --graph FILE     print the Walnut a script graph turns into, or the behavior tree a brain graph turns into\n"
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
