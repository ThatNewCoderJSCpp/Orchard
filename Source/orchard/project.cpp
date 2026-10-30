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
    return out;
}

std::optional<ProjectInfo> Project::read_info(const std::string& text, std::string* error) {
    ProjectInfo info;
    info.engine_version.clear();
    std::istringstream in(text);
    std::string line;
    bool open = false, closed = false;
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
        if (!open) {
            if (line != "project {") return fail("expected 'project {'");
            open = true;
            continue;
        }
        if (line == "}") {
            closed = true;
            break;
        }
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos || line.back() != ';') return fail("expected 'key: value;'");
        const std::string key = trim(line.substr(0, colon));
        const std::string value = unquote(trim(line.substr(colon + 1, line.size() - colon - 2)));
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
        scene.transform(scene.create("Light"))->position = system.from_render({3.0f, 4.0f, 2.0f});
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
