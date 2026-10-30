#include <orchard/orchard.hpp>
#include <orchard/scripting.hpp>
#include <xylem/xylem.hpp>

#include "axes_preview.hpp"
#include "gpu_view.hpp"

#include <grove/app.hpp>
#include <grove/bundle.hpp>
#include <grove/canvas.hpp>
#include <grove/grove.hpp>
#include <grove/native.hpp>
#include <grove/runtime/runtime.hpp>

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
#include <memory>
#include <optional>
#include <sstream>
#include <string>
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

    void advance(double dt) {
        if (!m_playing) return;
        dt = std::min(std::max(dt, 0.0), 0.1);
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
        m_project = std::move(p);
        m_scene = std::move(*s);
        m_selected = orchard::Entity{};
        m_dirty = false;
        m_view.set_dimension(m_project->info().dimension);
        sync_view_axes();
        m_view.reset_camera();
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
        if (m_session.playing()) {
            m_session.advance(f.dt);
            if (!m_scene.alive(m_selected)) m_selected = orchard::Entity{};
        }
        m_gpu.draw(f, m_scene, m_selected, m_view, m_session.playing());
    }

    void touched() {
        m_dirty = true;
        redraw();
    }

    void redraw() {
        if (grove::Node* canvas = m_doc.find_by_id("viewport")) grove::redraw_canvas(m_doc, *canvas);
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
        }
        if (const std::vector<orchard::Collider>* cs = m_scene.colliders(e)) {
            for (std::size_t i = 0; i < cs->size(); ++i) {
                const orchard::Collider& c = (*cs)[i];
                out += "collider\t" + std::to_string(i) + "\t" + orchard::to_string(c.shape) + "\t" + number(c.size.x) + "\t" + number(c.size.y) + "\t" + number(c.size.z)
                     + "\t" + number(c.offset.x) + "\t" + number(c.offset.y) + "\t" + number(c.offset.z) + "\t" + (c.enabled ? "1" : "0") + "\t" + (c.sensor ? "1" : "0")
                     + "\t" + number(c.density) + "\t" + number(c.friction) + "\t" + number(c.restitution)
                     + "\t" + orchard::to_string(c.friction_combine) + "\t" + orchard::to_string(c.bounce_combine)
                     + "\t" + (m_project ? orchard::combine_label(m_project->info().friction_combine) : "Geometric mean")
                     + "\t" + (m_project ? orchard::combine_label(m_project->info().bounce_combine) : "Maximum") + "\n";
            }
        }
        return out;
    }

    bool set_component(orchard::Entity e, const std::string& field, const std::string& value) {
        if (!m_scene.alive(e)) return false;
        const bool on = value == "1" || value == "true";
        const float v = static_cast<float>(std::atof(value.c_str()));
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
        } else if (kind == "trigger") {
            m_scene.add_body(e).type = orchard::BodyType::fixed;
            orchard::Collider* c = m_scene.add_collider(e);
            c->size = {3.0f, 2.0f, flat ? 1.0f : 3.0f};
            c->sensor = true;
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
            set_animating(false);
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
            m_drag_pan = arg(f, 2) != "1" || arg(f, 3) == "1";
            m_last_x = m_start_x = x;
            m_last_y = m_start_y = y;
            return std::string("ok");
        });

        native("viewport_move", [this](const std::vector<std::string>& f) {
            float x, y;
            if (!m_drag || !to_canvas(arg(f, 0), arg(f, 1), x, y)) return std::string();
            if (m_drag_pan) m_view.pan(x - m_last_x, y - m_last_y);
            else m_view.orbit(x - m_last_x, y - m_last_y);
            m_last_x = x;
            m_last_y = y;
            redraw();
            return std::string("ok");
        });

        native("viewport_release", [this](const std::vector<std::string>& f) {
            float x, y;
            const bool was = m_drag;
            m_drag = false;
            if (!was || !to_canvas(arg(f, 0), arg(f, 1), x, y)) return std::string();
            const float moved = std::fabs(x - m_start_x) + std::fabs(y - m_start_y);
            if (moved > 4.0f || m_drag_pan) return std::string("drag");
            m_selected = m_view.pick(m_scene, x, y);
            redraw();
            return "pick\t" + (m_scene.alive(m_selected) ? std::to_string(m_selected.id()) : std::string());
        });

        native("viewport_zoom", [this](const std::vector<std::string>& f) {
            m_view.zoom(static_cast<float>(std::atof(arg(f, 0).c_str())));
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

        native("reset_camera", [this](const std::vector<std::string>&) {
            m_view.reset_camera();
            redraw();
            return std::string("ok");
        });

        native("files", [this](const std::vector<std::string>&) {
            std::string out;
            if (!m_project) return out;
            for (const std::string& s : m_project->scenes()) out += "scene\t" + s + "\n";
            for (const std::string& s : m_project->scripts()) out += "script\t" + s + "\n";
            for (const std::string& s : m_project->graphs()) out += "graph\t" + s + "\n";
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
    bool m_drag_pan = false;
    float m_start_x = 0.0f;
    float m_start_y = 0.0f;
    float m_last_x = 0.0f;
    float m_last_y = 0.0f;
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

} // namespace

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
