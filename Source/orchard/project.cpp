#include <orchard/project.hpp>
#include <orchard/graph.hpp>
#include <orchard/version.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace orchard {

namespace {

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string unquote(const std::string& s) {
    if (s.size() < 2 || s.front() != '"' || s.back() != '"') return s;
    std::string out;
    for (std::size_t i = 1; i + 1 < s.size(); ++i) {
        if (s[i] == '\\' && i + 2 < s.size()) {
            ++i;
            out += s[i] == 'n' ? '\n' : s[i];
        } else {
            out += s[i];
        }
    }
    return out;
}

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') {
            out += "\\n";
            continue;
        }
        out += c;
    }
    return out + "\"";
}

std::string number(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g", v);
    return buf;
}

bool write_text(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

std::string spin_axis(Dimension d, Axes axes) {
    if (d == Dimension::two_d) return "z";
    return axes == Axes::z_up || axes == Axes::z_up_left ? "z" : "y";
}

std::string starter_script(Dimension d, Axes axes) {
    const std::string axis = spin_axis(d, axes);
    std::string s;
    s += "import module.orchard\n\n";
    s += "function on_start(orchard::Entity&: entity) -> void {\n";
    s += "    orchard::log(\"Hello from \" + entity.name());\n";
    s += "} end function on_start\n\n";
    s += "function on_tick(orchard::Entity&: entity, double: dt) -> void {\n";
    if (axis == "z") {
        s += "    entity.rotate(0.0, 0.0, 45.0 * dt);\n";
    } else {
        s += "    entity.rotate(0.0, 45.0 * dt, 0.0);\n";
    }
    s += "} end function on_tick\n";
    return s;
}

std::string starter_graph(Dimension d, Axes axes) {
    graph::Graph g;
    const int start = g.add_node("on_start", 40.0f, 40.0f);
    const int hello = g.add_node("join", 40.0f, 160.0f);
    const int log = g.add_node("log", 300.0f, 40.0f);
    const int name = g.add_node("self_name", -200.0f, 200.0f);
    g.set_value(hello, "a", "Hello from ");
    g.connect(name, "name", hello, "b");
    g.connect(hello, "text", log, "message");
    g.connect(start, "then", log, "in");
    const int tick = g.add_node("on_tick", 40.0f, 320.0f);
    const int speed = g.add_node("multiply", 40.0f, 440.0f);
    const int spin = g.add_node("rotate", 300.0f, 320.0f);
    g.set_value(speed, "a", "45");
    g.connect(tick, "dt", speed, "b");
    g.connect(speed, "result", spin, spin_axis(d, axes));
    g.connect(tick, "then", spin, "in");
    return g.save();
}

} // namespace

std::string Project::file() const {
    return (fs::path(m_folder) / file_name).string();
}

std::string Project::path_of(const std::string& relative) const {
    return (fs::path(m_folder) / relative).string();
}

namespace {

std::string dash_lower(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '_' || c == ' ') c = '-';
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        out.push_back(c);
    }
    return out;
}

bool parse_bool(const std::string& v, bool& out) {
    const std::string s = dash_lower(v);
    if (s == "on" || s == "true" || s == "yes" || s == "1") {
        out = true;
        return true;
    }
    if (s == "off" || s == "false" || s == "no" || s == "0") {
        out = false;
        return true;
    }
    return false;
}

const char* on_off(bool v) {
    return v ? "on" : "off";
}

} // namespace

const char* to_string(PhysicsQuality q) {
    switch (q) {
    case PhysicsQuality::arcade: return "arcade";
    case PhysicsQuality::standard: return "standard";
    case PhysicsQuality::high: return "high";
    case PhysicsQuality::realistic: return "realistic";
    case PhysicsQuality::custom: return "custom";
    }
    return "standard";
}

std::optional<PhysicsQuality> parse_physics_quality(const std::string& text) {
    const std::string s = dash_lower(text);
    if (s == "arcade" || s == "simple" || s == "fast" || s == "low") return PhysicsQuality::arcade;
    if (s == "standard" || s == "default" || s == "medium" || s == "normal") return PhysicsQuality::standard;
    if (s == "high") return PhysicsQuality::high;
    if (s == "realistic" || s == "ultra" || s == "accurate" || s == "max") return PhysicsQuality::realistic;
    if (s == "custom") return PhysicsQuality::custom;
    return std::nullopt;
}

const std::vector<PhysicsQuality>& physics_qualities() {
    static const std::vector<PhysicsQuality> all = {PhysicsQuality::arcade, PhysicsQuality::standard, PhysicsQuality::high, PhysicsQuality::realistic, PhysicsQuality::custom};
    return all;
}

const char* physics_quality_label(PhysicsQuality q) {
    switch (q) {
    case PhysicsQuality::arcade: return "Arcade (fastest)";
    case PhysicsQuality::standard: return "Standard";
    case PhysicsQuality::high: return "High";
    case PhysicsQuality::realistic: return "Realistic (most accurate)";
    case PhysicsQuality::custom: return "Custom";
    }
    return "Standard";
}

const char* physics_quality_explanation(PhysicsQuality q) {
    switch (q) {
    case PhysicsQuality::arcade: return "Few solver passes and no continuous collision. Cheapest, fine for simple games; tall stacks wobble and very fast things can pass through thin walls.";
    case PhysicsQuality::standard: return "The default. Solid stacking, and bodies marked Fast-moving never tunnel through walls.";
    case PhysicsQuality::high: return "Two substeps and more solver passes, for stable stacks, joints-heavy scenes and fast vehicles.";
    case PhysicsQuality::realistic: return "Four substeps, many solver passes, and every moving body is swept so nothing tunnels. The most accurate and the slowest.";
    case PhysicsQuality::custom: return "Your own substeps, iterations and collision settings.";
    }
    return "";
}

PhysicsSettings PhysicsSettings::preset(PhysicsQuality q) {
    PhysicsSettings s;
    s.quality = q;
    switch (q) {
    case PhysicsQuality::arcade:
        s.substeps = 1;
        s.velocity_iterations = 4;
        s.position_iterations = 1;
        s.continuous = false;
        s.gyroscopic = false;
        break;
    case PhysicsQuality::standard: break;
    case PhysicsQuality::high:
        s.substeps = 2;
        s.velocity_iterations = 12;
        s.position_iterations = 4;
        break;
    case PhysicsQuality::realistic:
        s.substeps = 4;
        s.velocity_iterations = 20;
        s.position_iterations = 6;
        s.continuous_all = true;
        s.slop = 0.001f;
        break;
    case PhysicsQuality::custom: break;
    }
    return s;
}

bool PhysicsSettings::operator==(const PhysicsSettings& o) const {
    return quality == o.quality && substeps == o.substeps && velocity_iterations == o.velocity_iterations && position_iterations == o.position_iterations && continuous == o.continuous
        && continuous_all == o.continuous_all && sleeping == o.sleeping && warm_starting == o.warm_starting && gyroscopic == o.gyroscopic && speculative_distance == o.speculative_distance
        && slop == o.slop && correction == o.correction && world_size == o.world_size
        && soft_solver == o.soft_solver && octree == o.octree && threads == o.threads && wide_solver == o.wide_solver && deterministic == o.deterministic
        && reuse_contacts == o.reuse_contacts && spin_aware_ccd == o.spin_aware_ccd && soft_substeps == o.soft_substeps && contact_hertz == o.contact_hertz
        && contact_damping == o.contact_damping && joint_hertz == o.joint_hertz && push_speed == o.push_speed;
}

const char* to_string(LightingLevel q) {
    switch (q) {
    case LightingLevel::off: return "off";
    case LightingLevel::simple: return "simple";
    case LightingLevel::shadowed: return "shadowed";
    case LightingLevel::realistic: return "realistic";
    case LightingLevel::path_traced: return "path-traced";
    case LightingLevel::photon: return "photon";
    }
    return "shadowed";
}

std::optional<LightingLevel> parse_lighting_level(const std::string& text) {
    const std::string s = dash_lower(text);
    if (s == "off" || s == "unlit" || s == "flat") return LightingLevel::off;
    if (s == "simple" || s == "basic" || s == "raycast" || s == "low" || s == "voxel") return LightingLevel::simple;
    if (s == "shadowed" || s == "shadows" || s == "medium" || s == "standard") return LightingLevel::shadowed;
    if (s == "realistic" || s == "high" || s == "lumen" || s == "gi") return LightingLevel::realistic;
    if (s == "path-traced" || s == "pathtraced" || s == "path" || s == "ultra" || s == "ray-traced") return LightingLevel::path_traced;
    if (s == "photon" || s == "photons" || s == "spectral" || s == "max" || s == "physical") return LightingLevel::photon;
    return std::nullopt;
}

const std::vector<LightingLevel>& lighting_levels() {
    static const std::vector<LightingLevel> all = {LightingLevel::off, LightingLevel::simple, LightingLevel::shadowed, LightingLevel::realistic, LightingLevel::path_traced, LightingLevel::photon};
    return all;
}

const char* lighting_level_label(LightingLevel q) {
    switch (q) {
    case LightingLevel::off: return "Off (unlit)";
    case LightingLevel::simple: return "Simple (ray-cast sun shadows)";
    case LightingLevel::shadowed: return "Shadowed (everything casts soft shadows)";
    case LightingLevel::realistic: return "Realistic (bounced light, reflections)";
    case LightingLevel::path_traced: return "Path traced";
    case LightingLevel::photon: return "Photon (caustics, rainbows)";
    }
    return "Shadowed";
}

const char* lighting_level_explanation(LightingLevel q) {
    switch (q) {
    case LightingLevel::off: return "Colors as they are, no lights.";
    case LightingLevel::simple: return "Lights shine from their centers and only the sun casts shadows, one ray per pixel, like Voxelspire.";
    case LightingLevel::shadowed: return "Every light casts soft shadows from every object, sized by how big the light is.";
    case LightingLevel::realistic: return "Light bounces off walls, glossy reflections, glass bends light, glowing things light the room. Follows moving lights.";
    case LightingLevel::path_traced: return "Follows light paths through the scene for exact shadows, bounces, reflections and refraction. Clears up over time.";
    case LightingLevel::photon: return "Traces light by wavelength from the lights, so glass focuses light into caustics and crystals split it into rainbows.";
    }
    return "";
}

const std::string* LightingProject::get(const std::string& key) const {
    for (const auto& kv : settings) {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

void LightingProject::set(const std::string& key, const std::string& value) {
    for (auto& kv : settings) {
        if (kv.first == key) {
            kv.second = value;
            return;
        }
    }
    settings.emplace_back(key, value);
}

bool LightingProject::erase(const std::string& key) {
    for (auto it = settings.begin(); it != settings.end(); ++it) {
        if (it->first == key) {
            settings.erase(it);
            return true;
        }
    }
    return false;
}

std::string Project::write_info(const ProjectInfo& info) {
    std::string out = "project {\n";
    out += "    name: " + quote(info.name) + ";\n";
    out += "    dimension: " + std::string(to_string(info.dimension)) + ";\n";
    out += "    engine: " + quote(info.engine_version) + ";\n";
    out += "    main-scene: " + quote(info.main_scene) + ";\n";
    out += "    max-fps: " + (info.max_fps <= 0.0 ? std::string("unlimited") : number(info.max_fps)) + ";\n";
    out += "    tick-rate: " + number(info.tick_rate) + ";\n";
    if (info.dimension == Dimension::two_d) out += "    y-axis: " + std::string(to_string(info.y_axis)) + ";\n";
    else out += "    axes: " + std::string(to_string(info.axes)) + ";\n";
    const AxisFlip shown = info.dimension == Dimension::two_d ? info.axis_flips & AxisFlip::x : info.axis_flips;
    if (shown != AxisFlip::none) out += "    flip-axes: " + to_string(shown) + ";\n";
    out += "    friction-mix: " + std::string(to_string(info.friction_combine)) + ";\n";
    out += "    bounce-mix: " + std::string(to_string(info.bounce_combine)) + ";\n";
    out += "}\n";
    const PhysicsSettings& ph = info.physics;
    const PhysicsSettings base = PhysicsSettings::preset(ph.quality == PhysicsQuality::custom ? PhysicsQuality::standard : ph.quality);
    out += "\nphysics {\n";
    out += "    quality: " + std::string(to_string(ph.quality)) + ";\n";
    auto num = [&out](const char* key, double v, double def, bool always) {
        if (always || v != def) out += std::string("    ") + key + ": " + number(v) + ";\n";
    };
    auto flag = [&out](const char* key, bool v, bool def, bool always) {
        if (always || v != def) out += std::string("    ") + key + ": " + on_off(v) + ";\n";
    };
    const bool all = ph.quality == PhysicsQuality::custom;
    num("substeps", ph.substeps, base.substeps, all);
    num("velocity-iterations", ph.velocity_iterations, base.velocity_iterations, all);
    num("position-iterations", ph.position_iterations, base.position_iterations, all);
    flag("continuous", ph.continuous, base.continuous, all);
    flag("continuous-all", ph.continuous_all, base.continuous_all, all);
    flag("sleeping", ph.sleeping, base.sleeping, all);
    flag("warm-starting", ph.warm_starting, base.warm_starting, all);
    flag("gyroscopic", ph.gyroscopic, base.gyroscopic, all);
    num("speculative-distance", ph.speculative_distance, base.speculative_distance, all);
    num("slop", ph.slop, base.slop, all);
    num("correction", ph.correction, base.correction, all);
    num("world-size", ph.world_size, base.world_size, all);
    flag("soft-solver", ph.soft_solver, base.soft_solver, all);
    flag("octree", ph.octree, base.octree, all);
    num("threads", ph.threads, base.threads, all);
    flag("wide-solver", ph.wide_solver, base.wide_solver, all);
    flag("deterministic", ph.deterministic, base.deterministic, all);
    flag("reuse-contacts", ph.reuse_contacts, base.reuse_contacts, all);
    flag("spin-aware-ccd", ph.spin_aware_ccd, base.spin_aware_ccd, all);
    num("soft-substeps", ph.soft_substeps, base.soft_substeps, all);
    num("contact-hertz", ph.contact_hertz, base.contact_hertz, all);
    num("contact-damping", ph.contact_damping, base.contact_damping, all);
    num("joint-hertz", ph.joint_hertz, base.joint_hertz, all);
    num("push-speed", ph.push_speed, base.push_speed, all);
    out += "}\n";
    const LightingProject& li = info.lighting;
    out += "\nlighting {\n";
    out += "    quality: " + std::string(to_string(li.quality)) + ";\n";
    out += "    editor-quality: " + std::string(to_string(li.editor_quality)) + ";\n";
    out += "    players-choose: " + std::string(on_off(li.players_choose)) + ";\n";
    for (const auto& kv : li.settings) out += "    " + kv.first + ": " + (kv.second.find_first_of(" ;\"/") != std::string::npos ? quote(kv.second) : kv.second) + ";\n";
    out += "}\n";
    return out;
}

std::optional<ProjectInfo> Project::read_info(const std::string& text, std::string* error) {
    ProjectInfo info;
    info.engine_version.clear();
    std::istringstream in(text);
    std::string line;
    bool open = false, closed = false;
    std::string block;
    int line_no = 0;
    auto fail = [&](const std::string& msg) -> std::optional<ProjectInfo> {
        if (error) *error = "project.orchard:" + std::to_string(line_no) + ": " + msg;
        return std::nullopt;
    };
    while (std::getline(in, line)) {
        ++line_no;
        const std::size_t comment = line.find("//");
        if (comment != std::string::npos && line.find('"') == std::string::npos) line.erase(comment);
        line = trim(line);
        if (line.empty()) continue;
        if (block.empty()) {
            if (line.size() < 2 || line.back() != '{') return fail(open ? "expected a block like 'physics {'" : "expected 'project {'");
            block = trim(line.substr(0, line.size() - 1));
            if (!open && block != "project") return fail("expected 'project {'");
            open = true;
            continue;
        }
        if (line == "}") {
            if (block == "project") closed = true;
            block.clear();
            continue;
        }
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos || line.back() != ';') return fail("expected 'key: value;'");
        const std::string key = trim(line.substr(0, colon));
        const std::string value = unquote(trim(line.substr(colon + 1, line.size() - colon - 2)));
        if (block == "physics") {
            PhysicsSettings& ph = info.physics;
            bool b = false;
            if (key == "quality") {
                const std::optional<PhysicsQuality> q = parse_physics_quality(value);
                if (!q) return fail("physics quality must be arcade, standard, high, realistic or custom");
                ph = PhysicsSettings::preset(*q == PhysicsQuality::custom ? PhysicsQuality::standard : *q);
                ph.quality = *q;
            } else if (key == "substeps") {
                ph.substeps = std::atoi(value.c_str());
                if (ph.substeps < 1 || ph.substeps > 64) return fail("substeps must be 1 to 64");
            } else if (key == "velocity-iterations") {
                ph.velocity_iterations = std::atoi(value.c_str());
                if (ph.velocity_iterations < 1) return fail("velocity-iterations must be at least 1");
            } else if (key == "position-iterations") {
                ph.position_iterations = std::atoi(value.c_str());
                if (ph.position_iterations < 0) return fail("position-iterations can't be negative");
            } else if (key == "continuous" || key == "continuous-all" || key == "sleeping" || key == "warm-starting" || key == "gyroscopic") {
                if (!parse_bool(value, b)) return fail(key + " must be on or off");
                if (key == "continuous") ph.continuous = b;
                else if (key == "continuous-all") ph.continuous_all = b;
                else if (key == "sleeping") ph.sleeping = b;
                else if (key == "warm-starting") ph.warm_starting = b;
                else ph.gyroscopic = b;
            } else if (key == "speculative-distance") {
                ph.speculative_distance = static_cast<float>(std::atof(value.c_str()));
            } else if (key == "slop") {
                ph.slop = static_cast<float>(std::atof(value.c_str()));
            } else if (key == "correction") {
                ph.correction = static_cast<float>(std::atof(value.c_str()));
            } else if (key == "world-size") {
                ph.world_size = static_cast<float>(std::atof(value.c_str()));
                if (ph.world_size <= 0.0f) return fail("world-size must be above 0");
            } else if (key == "soft-solver" || key == "octree" || key == "wide-solver" || key == "deterministic" || key == "reuse-contacts" || key == "spin-aware-ccd") {
                if (!parse_bool(value, b)) return fail(key + " must be on or off");
                if (key == "soft-solver") ph.soft_solver = b;
                else if (key == "octree") ph.octree = b;
                else if (key == "wide-solver") ph.wide_solver = b;
                else if (key == "deterministic") ph.deterministic = b;
                else if (key == "reuse-contacts") ph.reuse_contacts = b;
                else ph.spin_aware_ccd = b;
            } else if (key == "threads") {
                ph.threads = std::atoi(value.c_str());
                if (ph.threads < 1 || ph.threads > 64) return fail("threads must be 1 to 64");
            } else if (key == "soft-substeps") {
                ph.soft_substeps = std::atoi(value.c_str());
                if (ph.soft_substeps < 1 || ph.soft_substeps > 32) return fail("soft-substeps must be 1 to 32");
            } else if (key == "contact-hertz") {
                ph.contact_hertz = std::max(0.0f, static_cast<float>(std::atof(value.c_str())));
            } else if (key == "contact-damping") {
                ph.contact_damping = std::max(0.0f, static_cast<float>(std::atof(value.c_str())));
            } else if (key == "joint-hertz") {
                ph.joint_hertz = std::max(0.0f, static_cast<float>(std::atof(value.c_str())));
            } else if (key == "push-speed") {
                ph.push_speed = std::max(0.0f, static_cast<float>(std::atof(value.c_str())));
            }
            continue;
        }
        if (block == "lighting") {
            LightingProject& li = info.lighting;
            if (key == "quality" || key == "editor-quality") {
                const std::optional<LightingLevel> q = parse_lighting_level(value);
                if (!q) return fail(key + " must be off, simple, shadowed, realistic, path-traced or photon");
                if (key == "quality") li.quality = *q;
                else li.editor_quality = *q;
            } else if (key == "players-choose") {
                bool b = true;
                if (!parse_bool(value, b)) return fail("players-choose must be on or off");
                li.players_choose = b;
            } else {
                li.set(key, value);
            }
            continue;
        }
        if (block != "project") continue;
        if (key == "name") {
            info.name = value;
        } else if (key == "dimension") {
            const std::optional<Dimension> d = parse_dimension(value);
            if (!d) return fail("dimension must be 2d or 3d");
            info.dimension = *d;
        } else if (key == "engine") {
            info.engine_version = value;
        } else if (key == "main-scene") {
            info.main_scene = value;
        } else if (key == "max-fps") {
            info.max_fps = value == "unlimited" ? 0.0 : std::atof(value.c_str());
        } else if (key == "tick-rate") {
            info.tick_rate = std::atof(value.c_str());
            if (info.tick_rate <= 0.0) return fail("tick-rate must be above 0");
        } else if (key == "axes") {
            const std::optional<Axes> a = parse_axes(value);
            if (!a) return fail("axes must be y-up, y-up-left, z-up or z-up-left");
            info.axes = *a;
        } else if (key == "friction-mix" || key == "bounce-mix") {
            const std::optional<CombineMode> m = parse_combine_mode(value);
            if (!m || *m == CombineMode::project_default) return fail(key + " must be average, geometric-mean, harmonic-mean, quadratic-mean, minimum, multiply, maximum or sum");
            if (key == "friction-mix") info.friction_combine = *m;
            else info.bounce_combine = *m;
        } else if (key == "flip-axes") {
            const std::optional<AxisFlip> f = parse_axis_flips(value);
            if (!f) return fail("flip-axes takes x, y and z (or none)");
            info.axis_flips = *f;
        } else if (key == "y-axis") {
            const std::optional<YAxis2D> y = parse_y_axis(value);
            if (!y) return fail("y-axis must be up or down");
            info.y_axis = *y;
        }
    }
    if (!open || !closed) return fail("missing 'project { ... }'");
    return info;
}

std::optional<Project> Project::create(const std::string& folder, const std::string& name, Dimension dimension, std::string* error) {
    return create(folder, name, dimension, Axes::y_up, error);
}

std::optional<Project> Project::create(const std::string& folder, const std::string& name, Dimension dimension, Axes axes, std::string* error) {
    return create(folder, name, dimension, AxisSystem(axes), error);
}

std::optional<Project> Project::create(const std::string& folder, const std::string& name, Dimension dimension, const AxisSystem& system, std::string* error) {
    const Axes axes = system.base();
    std::error_code ec;
    const fs::path root(folder);
    if (fs::exists(root / file_name, ec)) {
        if (error) *error = folder + " already has a project";
        return std::nullopt;
    }
    for (const char* sub : {"Scenes", "Scripts", "Graphs", "Assets"}) {
        fs::create_directories(root / sub, ec);
        if (ec) {
            if (error) *error = "can't create " + (root / sub).string() + ": " + ec.message();
            return std::nullopt;
        }
    }
    Project p;
    p.m_folder = fs::absolute(root, ec).lexically_normal().string();
    p.m_info.name = name.empty() ? root.filename().string() : name;
    p.m_info.dimension = dimension;
    p.m_info.engine_version = ORCHARD_VERSION_STRING;
    if (dimension == Dimension::three_d) p.m_info.set_axis_system(system);
    if (!p.save(error)) return std::nullopt;

    Scene scene(dimension);
    const Entity camera = scene.create("Camera");
    if (dimension == Dimension::three_d) {
        scene.transform(camera)->position = system.from_render({0.0f, 2.0f, 6.0f});
        const Entity light = scene.create("Light");
        scene.transform(light)->position = system.from_render({3.0f, 4.0f, 2.0f});
        scene.add_light(light, LightType::point).intensity = 40.0f;
    }
    const Entity spinner = scene.create(dimension == Dimension::two_d ? "Sprite" : "Cube");
    scene.set_script(spinner, "Graphs/spin.graph");
    if (!scene.save_file(p.path_of(p.m_info.main_scene))) {
        if (error) *error = "can't write " + p.m_info.main_scene;
        return std::nullopt;
    }
    write_text(root / "Scripts" / "spin.wal", starter_script(dimension, axes));
    write_text(root / "Graphs" / "spin.graph", starter_graph(dimension, axes));
    write_text(root / ".gitignore", ".orchard/\nbuild*/\n");
    return p;
}

std::optional<Project> Project::open(const std::string& path, std::string* error) {
    std::error_code ec;
    fs::path file(path);
    if (fs::is_directory(file, ec)) file /= file_name;
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        if (error) *error = "can't open " + file.string();
        return std::nullopt;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    std::optional<ProjectInfo> info = read_info(ss.str(), error);
    if (!info) return std::nullopt;
    Project p;
    p.m_folder = fs::absolute(file.parent_path(), ec).lexically_normal().string();
    p.m_info = *info;
    return p;
}

bool Project::save(std::string* error) const {
    if (!write_text(file(), write_info(m_info))) {
        if (error) *error = "can't write " + file();
        return false;
    }
    return true;
}

std::vector<std::string> Project::files_with(const std::string& folder, const std::string& extension) const {
    std::vector<std::string> out;
    std::error_code ec;
    const fs::path root = fs::path(m_folder) / folder;
    if (!fs::is_directory(root, ec)) return out;
    for (fs::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == extension) {
            out.push_back(it->path().lexically_relative(m_folder).generic_string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::string> Project::files_with(const std::string& folder, const std::vector<std::string>& extensions) const {
    std::vector<std::string> out;
    std::error_code ec;
    const fs::path root = fs::path(m_folder) / folder;
    if (!fs::is_directory(root, ec)) return out;
    for (fs::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string ext = it->path().extension().string();
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) out.push_back(it->path().lexically_relative(m_folder).generic_string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

const std::vector<std::string>& Project::script_extensions() {
    static const std::vector<std::string> exts = {".wal", ".cpp", ".cc", ".cxx", ".c", ".lua", ".cs", ".java"};
    return exts;
}

std::vector<std::string> Project::scripts() const {
    return files_with("Scripts", script_extensions());
}

std::vector<std::string> Project::graphs() const {
    return files_with("Graphs", ".graph");
}

std::vector<std::string> Project::scenes() const {
    return files_with("Scenes", ".scene");
}

} // namespace orchard
