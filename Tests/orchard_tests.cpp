#include <orchard/orchard.hpp>
#include <orchard/scripting.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

int failures = 0;
int checks = 0;

void check(bool ok, const std::string& what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << what << "\n";
    }
}

bool near(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) < eps;
}

void test_math() {
    const orchard::Quat q = orchard::from_euler({0.0f, 90.0f, 0.0f});
    const orchard::Vec3 v = orchard::rotate(q, {1.0f, 0.0f, 0.0f});
    check(near(v.x, 0.0f) && near(v.z, -1.0f), "90 degree yaw turns +X into -Z");
    const orchard::Vec3 e = orchard::to_euler(orchard::from_euler({10.0f, 20.0f, 30.0f}));
    check(near(e.x, 10.0f) && near(e.y, 20.0f) && near(e.z, 30.0f), "euler round trip");
    const orchard::Mat4 m = orchard::trs({1.0f, 2.0f, 3.0f}, orchard::Quat{}, {2.0f, 2.0f, 2.0f});
    const orchard::Vec4 p = m * orchard::Vec4{1.0f, 1.0f, 1.0f, 1.0f};
    check(near(p.x, 3.0f) && near(p.y, 4.0f) && near(p.z, 5.0f), "trs applies scale then translation");
}

void test_scene() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity a = s.create("Parent");
    const orchard::Entity b = s.create("Child", a);
    s.transform(a)->position = {1.0f, 0.0f, 0.0f};
    s.transform(b)->position = {0.0f, 2.0f, 0.0f};
    const orchard::Vec4 w = s.world_matrix(b) * orchard::Vec4{0.0f, 0.0f, 0.0f, 1.0f};
    check(near(w.x, 1.0f) && near(w.y, 2.0f), "child inherits parent transform");
    check(!s.set_parent(a, b), "can't parent to a descendant");
    check(s.children(a).size() == 1 && s.roots().size() == 1, "hierarchy queries");

    const std::string text = s.save();
    std::string error;
    const std::optional<orchard::Scene> loaded = orchard::Scene::load(text, &error);
    check(loaded.has_value(), "scene loads: " + error);
    if (loaded) {
        const orchard::Entity child = loaded->find("Child");
        check(loaded->size() == 2 && loaded->parent(child) == loaded->find("Parent"), "scene round trip keeps parents");
    }

    check(s.destroy(a) && s.size() == 0, "destroying a parent destroys its children");
    check(!s.alive(b), "old handles die");
    const orchard::Entity c = s.create("Reused");
    check(c.index == a.index || c.index == b.index, "slots are reused");
    check(c != a && c != b, "reused slots get a new generation");
}

void test_project() {
    const fs::path dir = fs::temp_directory_path() / "orchard-test-project";
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::string error;
    const std::optional<orchard::Project> p = orchard::Project::create(dir.string(), "Test Game", orchard::Dimension::two_d, &error);
    check(p.has_value(), "project creates: " + error);
    const std::optional<orchard::Project> q = orchard::Project::open(dir.string(), &error);
    check(q.has_value(), "project opens: " + error);
    if (q) {
        check(q->info().name == "Test Game" && q->info().dimension == orchard::Dimension::two_d, "project info round trip");
        check(q->scripts().size() == 1 && q->graphs().size() == 1 && q->scenes().size() == 1, "starter files exist");
        const std::optional<orchard::Scene> scene = orchard::Scene::load_file(q->path_of(q->info().main_scene), &error);
        check(scene && scene->dimension() == orchard::Dimension::two_d, "main scene is 2d");
    }
    check(!orchard::Project::read_info("project {\n    dimension: 4d;\n}\n").has_value(), "bad dimension is an error");
    fs::remove_all(dir, ec);
}

void test_graph() {
    orchard::graph::Graph g;
    const int tick = g.add_node("on_tick");
    const int rotate = g.add_node("rotate");
    const int mul = g.add_node("multiply");
    g.set_value(mul, "a", "90");
    check(g.connect(tick, "dt", mul, "b"), "dt connects to a number input");
    check(g.connect(mul, "result", rotate, "y"), "result connects to y");
    check(g.connect(tick, "then", rotate, "in"), "exec connects");
    std::string why;
    check(!g.connect(tick, "dt", rotate, "in", &why), "number can't connect to exec");

    std::vector<std::string> errors;
    const std::string code = g.to_walnut(&errors);
    check(errors.empty(), "graph compiles cleanly");
    check(code.find("function on_tick(orchard::Entity&: entity, double: dt) -> void {") != std::string::npos, "tick function generated");
    check(code.find("entity.rotate(0.0, (90.0 * dt), 0.0);") != std::string::npos, "rotate statement generated");

    const std::optional<orchard::graph::Graph> again = orchard::graph::Graph::load(g.save(), &why);
    check(again.has_value(), "graph loads: " + why);
    if (again) check(again->to_walnut() == code, "graph round trip generates the same Walnut");

    orchard::graph::Graph bad;
    bad.add_node("on_start");
    bad.add_node("on_start");
    bad.to_walnut(&errors);
    check(errors.size() == 1, "two On Start nodes is an error");
}

void test_components() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity crate = s.create("Crate");
    const orchard::Entity wheel = s.create("Wheel", crate);
    orchard::Body& b = s.add_body(crate);
    b.type = orchard::BodyType::dynamic;
    b.set(orchard::body_bounce, false);
    b.set(orchard::body_lock_rotation_y, true);
    b.gravity_scale = 0.5f;
    s.add_collider(crate, orchard::ColliderShape::box)->size = {2.0f, 1.0f, 1.0f};
    orchard::Collider* w = s.add_collider(wheel, orchard::ColliderShape::sphere);
    w->sensor = true;
    w->size = {0.5f, 0.5f, 0.5f};
    s.set_script(crate, "Scripts/spin.wal");
    check(s.body_owner(wheel) == crate, "a child collider belongs to its parent's body");

    std::string error;
    const std::optional<orchard::Scene> again = orchard::Scene::load(s.save(), &error);
    check(again.has_value(), "scene with components loads: " + error);
    if (again) {
        const orchard::Entity c = again->find("Crate");
        const orchard::Entity wh = again->find("Wheel");
        const orchard::Body* body = again->body(c);
        check(body && body->type == orchard::BodyType::dynamic && !body->has(orchard::body_bounce) && body->has(orchard::body_lock_rotation_y) && near(body->gravity_scale, 0.5f), "body round trip");
        check(again->colliders(c)->size() == 1 && near((*again->colliders(c))[0].size.x, 2.0f), "box collider round trip");
        check(again->colliders(wh)->size() == 1 && (*again->colliders(wh))[0].sensor && (*again->colliders(wh))[0].shape == orchard::ColliderShape::sphere, "sensor collider round trip");
        check(again->script(c) == "Scripts/spin.wal", "script round trip");
    }
    const std::optional<orchard::Scene> old = orchard::Scene::load("orchard-scene 1 2d\nentity 1 0 0 0 0 0 0 0 1 1 1 Old\n", &error);
    check(old && old->size() == 1, "version 1 scenes still load");
    check(s.remove_collider(wheel, 0) && s.colliders(wheel)->empty(), "removing a collider");
    check(s.remove_body(crate) && !s.body(crate), "removing a body");
    orchard::Scene flat(orchard::Dimension::two_d);
    const orchard::Entity p = flat.create("Player");
    check(flat.add_body(p).has(orchard::body_lock_position_z), "2d bodies stay on their plane");
}

void test_script_hooks() {
    const std::string src = "import module.orchard\nfunction on_start(orchard::Entity&: entity) -> void {\n} end function on_start\n  function on_tick(orchard::Entity&: entity, double: dt) -> void {\n} end function on_tick\n";
    const orchard::ScriptHooks hooks = orchard::ScriptRuntime::find_hooks(src);
    check(hooks.start_params == 1 && hooks.tick_params == 2 && hooks.stop_params == -1, "finding a script's hooks");
    const std::string full = orchard::ScriptRuntime::with_hooks(src, hooks);
    check(full.find("orchard::on_tick([]() -> void {") != std::string::npos && full.find("on_tick(orchard_self, orchard::dt());") != std::string::npos, "registering the tick hook");
    check(full.find("orchard::on_stop") == std::string::npos, "no stop hook without on_stop");
    check(orchard::ScriptRuntime::find_hooks("function on_stop() -> void {\n}").stop_params == 0, "a hook with no parameters");
    check(!orchard::ScriptRuntime::find_hooks("// nothing").any(), "a script without hooks");
}

void test_script_runtime() {
    const char* walnut = std::getenv("ORCHARD_TEST_WALNUT");
    if (!walnut || !*walnut) {
        std::cout << "skipping the script build test (set ORCHARD_TEST_WALNUT to a walnut compiler to run it)\n";
        return;
    }
    const fs::path dir = fs::temp_directory_path() / "orchard-test-scripts";
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::string error;
    std::optional<orchard::Project> project = orchard::Project::create(dir.string(), "Scripts", orchard::Dimension::three_d, &error);
    check(project.has_value(), "script test project: " + error);
    if (!project) return;
    {
        std::ofstream out(project->path_of("Scripts/mover.wal"));
        out << "import module.orchard\n\nlong int: ticks = 0l;\n\n"
               "function on_start(orchard::Entity&: entity) -> void {\n    orchard::log(\"start \" + entity.name());\n} end function on_start\n\n"
               "function on_tick(orchard::Entity&: entity, double: dt) -> void {\n    ticks += 1l;\n    entity.translate(dt, 0.0, 0.0);\n    if (orchard::key_down(\"Space\")) then {\n        entity.set_y(5.0);\n    }\n} end function on_tick\n";
    }
    orchard::Scene scene(orchard::Dimension::three_d);
    const orchard::Entity a = scene.create("Mover");
    const orchard::Entity b = scene.create("Spinner");
    scene.set_script(a, "Scripts/mover.wal");
    scene.set_script(b, "Graphs/spin.graph");
    orchard::ScriptRuntime runtime;
    orchard::ScriptToolchain tools;
    tools.walnut = walnut;
    if (const char* cxx = std::getenv("CXX")) tools.cxx = cxx;
    tools.include_dir = orchard::ScriptToolchain::find_include_dir("");
    tools.cache_dir = (dir / ".orchard" / "scripts").string();
    runtime.set_toolchain(tools);
    std::vector<std::string> logs;
    runtime.set_log([&logs](const std::string& m, bool bad) { logs.push_back((bad ? "! " : "") + m); });
    for (const orchard::ScriptBuildResult& r : runtime.build_all(*project, scene)) {
        check(r.ok, "building " + r.script + ": " + r.log);
    }
    check(runtime.start(*project, scene), "starting scripts");
    check(runtime.instance_count() == 2, "two script instances");
    runtime.tick(0.5);
    runtime.set_key("Space", true);
    runtime.tick(0.5);
    runtime.stop();
    const orchard::Transform* t = scene.transform(a);
    check(t && near(t->position.x, 1.0f) && near(t->position.y, 5.0f), "the script moved its entity");
    const orchard::Vec3 r = orchard::to_euler(scene.transform(b)->rotation);
    check(near(r.y, 45.0f, 0.05f), "the graph script turned its entity");
    bool started = false, greeted = false;
    for (const std::string& l : logs) {
        started = started || l == "start Mover";
        greeted = greeted || l == "Hello from Spinner";
    }
    check(started && greeted, "scripts can log");
    const orchard::ScriptBuildResult cached = runtime.build(*project, "Scripts/mover.wal");
    check(cached.ok && cached.cached, "unchanged scripts come from the cache");
    {
        std::ofstream out(project->path_of("Scripts/broken.wal"));
        out << "import module.orchard\nfunction on_tick(orchard::Entity&: entity, double: dt) -> void {\n    nope();\n} end function on_tick\n";
    }
    const orchard::ScriptBuildResult broken = runtime.build(*project, "Scripts/broken.wal");
    check(!broken.ok && broken.log.find("Scripts/broken.wal") != std::string::npos, "build errors name the script: " + broken.log);
    fs::remove_all(dir, ec);
}

void test_native_hooks() {
    const std::string src = "#include <orchard/script/orchard.hpp>\nvoid helper() {}\nvoid on_start(orchard::script::Entity self)\n{\n}\nstatic void on_tick_later();\nvoid on_tick(orchard::script::Entity self, double dt) {\n}\n";
    const orchard::ScriptHooks hooks = orchard::ScriptRuntime::find_native_hooks(src);
    check(hooks.start_params == 1 && hooks.tick_params == 2 && hooks.stop_params == -1, "finding C++ hooks");
    check(orchard::ScriptRuntime::find_native_hooks("void on_stop(void) {}").stop_params == 0, "a C hook with (void)");
    check(!orchard::ScriptRuntime::find_native_hooks("void on_tick(double dt);").any(), "declarations aren't hooks");
    const std::string reg = orchard::ScriptRuntime::native_registration(hooks, false, "/x/y.cpp");
    check(reg.find("#include \"/x/y.cpp\"") != std::string::npos && reg.find("on_tick(orchard_self, orchard::script::World::dt());") != std::string::npos, "C++ hook registration");
    const std::string creg = orchard::ScriptRuntime::native_registration(hooks, true, "/x/y.c");
    check(creg.find("extern \"C\" void on_tick(orchard_entity, double);") != std::string::npos && creg.find("#include \"/x/y.c\"") == std::string::npos, "C hook registration");
    orchard::ScriptRuntime runtime;
    auto name_of = [&runtime](const char* path) {
        orchard::ScriptLanguage* l = runtime.language_for(path);
        return l ? l->name() : std::string("none");
    };
    check(name_of("Scripts/a.wal") == "Walnut" && name_of("Graphs/a.graph") == "Walnut", "Walnut files");
    check(name_of("Scripts/a.cpp") == "C++" && name_of("Scripts/a.CC") == "C++" && name_of("Scripts/a.c") == "C", "C and C++ files");
    check(name_of("Scripts/a.cs") == "C#" && name_of("Scripts/a.txt") == "none", "C# files and unknown files");
#if defined(ORCHARD_TEST_LUA)
    check(name_of("Scripts/a.lua") == "Lua", "Lua files");
#endif
}

void test_script_languages() {
    const fs::path dir = fs::temp_directory_path() / "orchard-test-languages";
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::string error;
    std::optional<orchard::Project> project = orchard::Project::create(dir.string(), "Languages", orchard::Dimension::three_d, &error);
    check(project.has_value(), "language test project: " + error);
    if (!project) return;
    auto write = [&](const char* rel, const std::string& text) {
        std::ofstream out(project->path_of(rel), std::ios::binary);
        out << text;
    };
    write("Scripts/mover.cpp",
          "#include <orchard/script/orchard.hpp>\n#include <map>\nusing namespace orchard::script;\n\nstd::map<orchard_entity, int> ticks;\n\n"
          "void on_start(Entity self) {\n    log(text(\"start C++ \", self.name()));\n}\n\n"
          "void on_tick(Entity self, double dt) {\n    ++ticks[self.id()];\n    self.translate({dt, 0.0, 0.0});\n    if (key_down(\"Space\")) self.set_position({self.position().x, 5.0, 0.0});\n}\n\n"
          "void on_stop(Entity self) {\n    log(text(\"ticks \", ticks[self.id()]));\n}\n");
    write("Scripts/mover_c.c",
          "#include <orchard/script/orchard.h>\n#include <stdio.h>\n\nstatic int ticks = 0;\n\n"
          "void on_start(orchard_entity self) {\n    char line[128];\n    snprintf(line, sizeof line, \"start C %s\", orchard_name(self));\n    orchard_log(line);\n}\n\n"
          "void on_tick(orchard_entity self, double dt) {\n    ++ticks;\n    orchard_translate(self, dt, 0.0, 0.0);\n    if (orchard_key_down(\"Space\")) {\n        orchard_vec3 p = orchard_position(self);\n        orchard_set_position(self, p.x, 5.0, p.z);\n    }\n}\n\n"
          "void on_stop(orchard_entity self) {\n    char line[64];\n    (void)self;\n    snprintf(line, sizeof line, \"ticks %d\", ticks);\n    orchard_log(line);\n}\n");
    write("Scripts/mover.lua",
          "function on_start(self)\n    self.ticks = 0\n    print(\"start Lua \" .. self:name())\nend\n\n"
          "function on_tick(self, dt)\n    self.ticks = self.ticks + 1\n    self:translate(dt, 0, 0)\n    if orchard.key_down(\"Space\") then\n        local p = self:position()\n        self:set_position(p.x, 5, p.z)\n    end\nend\n\n"
          "function on_stop(self)\n    orchard.log(\"ticks \" .. self.ticks)\nend\n");
    write("Scripts/Mover.cs",
          "using Orchard;\n\npublic class Mover : Script\n{\n    int ticks;\n\n    public override void OnStart() => World.Log(\"start C# \" + Entity.Name);\n\n"
          "    public override void OnTick(double dt)\n    {\n        ticks++;\n        Entity.Translate(dt, 0, 0);\n        if (World.KeyDown(\"Space\")) Entity.Position = new Vec3(Entity.Position.X, 5, 0);\n    }\n\n"
          "    public override void OnStop() => World.Log(\"ticks \" + ticks);\n}\n");
    write("Scripts/JavaMover.java",
          "import orchard.*;\n\npublic class JavaMover extends Script {\n    int ticks;\n\n    @Override\n    public void onStart() {\n        World.log(\"start Java \" + entity.name());\n    }\n\n"
          "    @Override\n    public void onTick(double dt) {\n        ticks++;\n        entity.translate(dt, 0, 0);\n        if (World.keyDown(\"Space\")) entity.setPosition(entity.position().x, 5, 0);\n    }\n\n"
          "    @Override\n    public void onStop() {\n        World.log(\"ticks \" + ticks);\n    }\n}\n");

    orchard::ScriptRuntime runtime;
    orchard::ScriptToolchain tools;
    if (const char* walnut = std::getenv("ORCHARD_TEST_WALNUT")) tools.walnut = walnut;
    if (const char* cxx = std::getenv("CXX")) tools.cxx = cxx;
    if (const char* cc = std::getenv("CC")) tools.cc = cc;
    tools.include_dir = orchard::ScriptToolchain::find_include_dir("");
    tools.cache_dir = (dir / ".orchard" / "scripts").string();
    runtime.set_toolchain(tools);

    struct Case {
        const char* script;
        const char* entity;
        const char* language;
    };
    const std::vector<Case> cases = {
        {"Scripts/mover.cpp", "CppThing", "C++"},
        {"Scripts/mover_c.c", "CThing", "C"},
        {"Scripts/mover.lua", "LuaThing", "Lua"},
        {"Scripts/Mover.cs", "CsThing", "C#"},
        {"Scripts/JavaMover.java", "JavaThing", "Java"},
    };
    orchard::Scene scene(orchard::Dimension::three_d);
    std::vector<std::pair<Case, orchard::Entity>> running;
    for (const Case& c : cases) {
        orchard::ScriptLanguage* lang = runtime.language(c.language);
        std::string why;
        if (!lang || !lang->available(tools, &why)) {
            std::cout << "skipping " << c.language << " scripts: " << (lang ? why : std::string("not built in")) << "\n";
            continue;
        }
        const orchard::ScriptBuildResult built = runtime.build(*project, c.script);
        check(built.ok && built.language == c.language, std::string("building ") + c.script + ": " + built.log);
        if (!built.ok) continue;
        const orchard::Entity e = scene.create(c.entity);
        scene.set_script(e, c.script);
        running.push_back({c, e});
    }
    std::vector<std::string> logs;
    runtime.set_log([&logs](const std::string& m, bool bad) { logs.push_back((bad ? "! " : "") + m); });
    check(runtime.start(*project, scene), "starting scripts in every language");
    check(runtime.instance_count() == running.size(), "one instance per scripted entity");
    runtime.tick(0.5);
    runtime.set_key("Space", true);
    runtime.tick(0.5);
    runtime.stop();
    for (const auto& [c, e] : running) {
        const orchard::Transform* t = scene.transform(e);
        check(t && near(t->position.x, 1.0f) && near(t->position.y, 5.0f), std::string(c.language) + " scripts move their entity");
        bool started = false;
        for (const std::string& l : logs) started = started || l == std::string("start ") + c.language + " " + c.entity;
        check(started, std::string(c.language) + " scripts can log");
    }
    std::size_t tick_lines = 0;
    for (const std::string& l : logs) {
        if (l == "ticks 2") ++tick_lines;
        else if (l.rfind("! ", 0) == 0) check(false, "script error: " + l);
    }
    check(tick_lines == running.size(), "every language keeps per-entity state until on_stop");

    for (const Case& c : cases) {
        orchard::ScriptLanguage* lang = runtime.language(c.language);
        if (!lang || !lang->available(tools)) continue;
        const std::string ext = fs::path(c.script).extension().string();
        const std::string fresh = std::string("Scripts/Fresh") + (ext == ".c" ? "C" : ext == ".cpp" ? "Cpp" : "") + ext;
        check(runtime.create_script(*project, fresh, orchard::Dimension::three_d, &error), "new " + std::string(c.language) + " script: " + error);
        const orchard::ScriptBuildResult built = runtime.build(*project, fresh);
        check(built.ok, "the new " + std::string(c.language) + " script builds: " + built.log);
    }
    check(runtime.create_script(*project, "Scripts/Twice.cpp", orchard::Dimension::three_d, &error), "create_script makes a new file: " + error);
    check(!runtime.create_script(*project, "Scripts/Twice.cpp", orchard::Dimension::three_d), "create_script won't overwrite");

    write("Scripts/broken.lua", "function on_tick(self, dt)\n    self:translate(\nend\n");
    const orchard::ScriptBuildResult broken = runtime.build(*project, "Scripts/broken.lua");
    check(!broken.ok && broken.log.find("Scripts/broken.lua:3") != std::string::npos, "Lua syntax errors name the script and line: " + broken.log);
    write("Scripts/broken.cpp", "#include <orchard/script/orchard.hpp>\nvoid on_tick(orchard::script::Entity self, double dt) {\n    nope();\n}\n");
    if (runtime.language("C++")->available(tools)) {
        const orchard::ScriptBuildResult bad = runtime.build(*project, "Scripts/broken.cpp");
        check(!bad.ok && bad.log.find("Scripts/broken.cpp:3") != std::string::npos, "C++ errors name the script and line: " + bad.log);
    }
    const std::vector<std::string> listed = project->scripts();
    check(std::find(listed.begin(), listed.end(), "Scripts/Mover.cs") != listed.end() && std::find(listed.begin(), listed.end(), "Scripts/mover.lua") != listed.end(), "the project lists scripts in every language");
    fs::remove_all(dir, ec);
}

void test_axes() {
    for (orchard::Axes a : {orchard::Axes::y_up, orchard::Axes::y_up_left, orchard::Axes::z_up, orchard::Axes::z_up_left}) {
        const orchard::Vec3 v{0.3f, -1.2f, 2.5f};
        const orchard::Vec3 back = orchard::from_render(a, orchard::to_render(a, v));
        check(near(back.x, v.x) && near(back.y, v.y) && near(back.z, v.z), std::string("axes round trip ") + orchard::to_string(a));
        check(orchard::parse_axes(orchard::to_string(a)) == a, std::string("axes names ") + orchard::to_string(a));
        orchard::Viewport view;
        view.set_view_size(320, 200);
        view.set_axes(a);
        float cx = 0, cy = 0, ux = 0, uy = 0;
        view.project({0.0f, 0.0f, 0.0f}, cx, cy);
        view.project(orchard::up_of(a), ux, uy);
        check(uy < cy - 5.0f, std::string("up is up on screen for ") + orchard::to_string(a));
        orchard::Viewport front;
        front.set_view_size(320, 200);
        front.set_axes(a);
        front.orbit_camera().yaw = 0.0f;
        front.orbit_camera().pitch = 0.0f;
        float rx = 0, ry = 0;
        front.project(orchard::right_of(a), rx, ry);
        front.project({0.0f, 0.0f, 0.0f}, cx, cy);
        check(rx > cx + 5.0f && near(ry, cy, 0.5f), std::string("right is right on screen for ") + orchard::to_string(a));
    }
    const orchard::Vec3 gz = orchard::gravity_of(orchard::Axes::z_up);
    check(near(gz.z, -9.80665f) && near(gz.y, 0.0f), "z-up gravity points down z");
    check(near(orchard::gravity_of(orchard::YAxis2D::down).y, 9.80665f), "y-down 2d gravity");
    orchard::ProjectInfo info;
    info.axes = orchard::Axes::z_up_left;
    const std::optional<orchard::ProjectInfo> again = orchard::Project::read_info(orchard::Project::write_info(info));
    check(again && again->axes == orchard::Axes::z_up_left, "project axes round trip");
    orchard::ProjectInfo flat;
    flat.dimension = orchard::Dimension::two_d;
    flat.y_axis = orchard::YAxis2D::down;
    const std::optional<orchard::ProjectInfo> flat_again = orchard::Project::read_info(orchard::Project::write_info(flat));
    check(flat_again && flat_again->y_axis == orchard::YAxis2D::down, "project 2d y-axis round trip");
    std::string error;
    check(!orchard::Project::read_info("project {\n    axes: sideways;\n}\n", &error) && error.find("axes") != std::string::npos, "bad axes are reported");
    check(orchard::Project::read_info("project {\n    name: \"Old\";\n}\n")->axes == orchard::Axes::y_up, "old projects default to y-up");
    orchard::Viewport flatview;
    flatview.set_view_size(320, 200);
    flatview.set_dimension(orchard::Dimension::two_d);
    float ax = 0, ay = 0, bx = 0, by = 0;
    flatview.project({0.0f, 0.0f, 0.0f}, ax, ay);
    flatview.project({0.0f, 1.0f, 0.0f}, bx, by);
    check(by < ay, "2d y-up draws +y upward");
    flatview.set_y_axis(orchard::YAxis2D::down);
    flatview.project({0.0f, 1.0f, 0.0f}, bx, by);
    check(by > ay, "2d y-down draws +y downward");
}

void test_axis_flips() {
    orchard::AxisSystem a;
    check(a == orchard::AxisSystem::standard() && a.flips() == orchard::AxisFlip::none, "axis system starts standard");
    a.flip_z();
    check(a == orchard::AxisSystem::unity() && a.mirrored() && a.name() == "y-up-left", "flipping z gives Unity's axes");
    a.flip_z().flip_y();
    check(a.name() == "y-up flip-y" && orchard::AxisSystem::parse(a.name()) == a, "custom axis names round trip");
    check(orchard::AxisSystem::parse("unreal flip-y") == orchard::AxisSystem::source() && orchard::AxisSystem::parse("source") == orchard::AxisSystem::source(), "Source axes");
    check(!orchard::AxisSystem::parse("y-up flip-w"), "bad flips are rejected");
    for (orchard::AxisSystem sys : {orchard::AxisSystem::y_up().with_flips(orchard::AxisFlip::y), orchard::AxisSystem::z_up().with_flips(orchard::AxisFlip::x | orchard::AxisFlip::z), orchard::AxisSystem::source()}) {
        const orchard::Vec3 v{0.3f, -1.2f, 2.5f};
        const orchard::Vec3 back = sys.from_render(sys.to_render(v));
        check(near(back.x, v.x) && near(back.y, v.y) && near(back.z, v.z), "flipped round trip " + sys.name());
        orchard::Viewport view;
        view.set_view_size(320, 200);
        view.set_axis_system(sys);
        view.orbit_camera().yaw = 0.0f;
        view.orbit_camera().pitch = 0.0f;
        float cx = 0, cy = 0, ux = 0, uy = 0, rx = 0, ry = 0;
        view.project({0.0f, 0.0f, 0.0f}, cx, cy);
        view.project(sys.up(), ux, uy);
        view.project(sys.right(), rx, ry);
        check(uy < cy - 5.0f && rx > cx + 5.0f, "up is up and right is right for " + sys.name());
        const orchard::Vec3 g = sys.gravity();
        check(near(sys.height(g), -9.80665f, 1e-3f), "gravity points down for " + sys.name());
    }
    orchard::ProjectInfo info;
    info.set_axis_system(orchard::AxisSystem::z_up().with_flips(orchard::AxisFlip::x));
    const std::string text = orchard::Project::write_info(info);
    check(text.find("flip-axes: x;") != std::string::npos, "flips are saved");
    const std::optional<orchard::ProjectInfo> again = orchard::Project::read_info(text);
    check(again && again->axis_system() == info.axis_system(), "flips round trip through project.orchard");
    check(orchard::Project::write_info(orchard::ProjectInfo()).find("flip-axes") == std::string::npos, "no flips, no flip-axes line");
    orchard::Viewport flat;
    flat.set_view_size(320, 200);
    flat.set_dimension(orchard::Dimension::two_d);
    float ax = 0, ay = 0, bx = 0, by = 0;
    flat.project({0.0f, 0.0f, 0.0f}, ax, ay);
    flat.project({1.0f, 0.0f, 0.0f}, bx, by);
    check(bx > ax, "2d +x is right");
    flat.set_flip_x_2d(true);
    flat.project({1.0f, 0.0f, 0.0f}, bx, by);
    check(bx < ax, "2d flipped x draws +x to the left");
}

void test_remap_scene() {
    auto world_point = [](const orchard::Scene& sc, orchard::Entity e, orchard::Vec3 local) {
        const orchard::Vec4 p = sc.world_matrix(e) * orchard::Vec4{local.x, local.y, local.z, 1.0f};
        return orchard::Vec3{p.x, p.y, p.z};
    };
    const std::vector<std::pair<orchard::AxisSystem, orchard::AxisSystem>> changes = {
        {orchard::AxisSystem::y_up(), orchard::AxisSystem::z_up()},
        {orchard::AxisSystem::y_up(), orchard::AxisSystem::unity()},
        {orchard::AxisSystem::z_up(), orchard::AxisSystem::unreal()},
        {orchard::AxisSystem::blender(), orchard::AxisSystem::source()},
        {orchard::AxisSystem::y_up().with_flips(orchard::AxisFlip::x | orchard::AxisFlip::z), orchard::AxisSystem::z_up_left()},
    };
    for (const auto& [from, to] : changes) {
        orchard::Scene sc(orchard::Dimension::three_d);
        const orchard::Entity parent = sc.create("Parent");
        sc.transform(parent)->position = {1.0f, 2.0f, 3.0f};
        sc.transform(parent)->rotation = orchard::from_euler({10.0f, 35.0f, -20.0f});
        sc.transform(parent)->scale = {1.0f, 2.0f, 1.5f};
        sc.add_collider(parent)->offset = {0.5f, -0.25f, 0.75f};
        const orchard::Entity child = sc.create("Child", parent);
        sc.transform(child)->position = {0.3f, 1.0f, -0.6f};
        sc.transform(child)->rotation = orchard::from_euler({-30.0f, 5.0f, 60.0f});
        sc.add_collider(child)->offset = {0.0f, 0.4f, 0.2f};
        const orchard::Vec3 probes[3] = {{0.0f, 0.0f, 0.0f}, {0.5f, 0.0f, 0.0f}, {0.0f, 0.5f, 0.5f}};
        std::vector<orchard::Vec3> before;
        for (orchard::Entity e : {parent, child}) {
            before.push_back(world_point(sc, e, sc.colliders(e)->front().offset));
            for (const orchard::Vec3& p : probes) before.push_back(world_point(sc, e, p));
        }
        const orchard::AxisChange change = orchard::AxisChange::between(from, to);
        orchard::remap_scene(sc, change);
        std::size_t i = 0;
        bool same = true;
        bool symmetric = true;
        for (orchard::Entity e : {parent, child}) {
            const orchard::Vec3 want = change.apply(before[i++]);
            const orchard::Vec3 got = world_point(sc, e, sc.colliders(e)->front().offset);
            same = same && near(want.x, got.x, 1e-3f) && near(want.y, got.y, 1e-3f) && near(want.z, got.z, 1e-3f);
            const orchard::Vec3 origin = world_point(sc, e, {0.0f, 0.0f, 0.0f});
            const orchard::Vec3 want_origin = change.apply(before[i]);
            same = same && near(origin.x, want_origin.x, 1e-3f) && near(origin.y, want_origin.y, 1e-3f) && near(origin.z, want_origin.z, 1e-3f);
            i += 3;
            const float len = std::sqrt(sc.transform(e)->rotation.x * sc.transform(e)->rotation.x + sc.transform(e)->rotation.y * sc.transform(e)->rotation.y + sc.transform(e)->rotation.z * sc.transform(e)->rotation.z + sc.transform(e)->rotation.w * sc.transform(e)->rotation.w);
            symmetric = symmetric && near(len, 1.0f, 1e-4f);
        }
        check(same, "remapping " + from.name() + " to " + to.name() + " keeps every collider where it was");
        check(symmetric, "remapped rotations stay proper rotations (" + to.name() + ")");
    }
    orchard::Scene flat(orchard::Dimension::two_d);
    const orchard::Entity sprite = flat.create("Sprite");
    flat.transform(sprite)->position = {2.0f, 3.0f, 0.0f};
    flat.transform(sprite)->rotation = orchard::from_euler({0.0f, 0.0f, 30.0f});
    orchard::remap_scene(flat, orchard::AxisChange::flips_2d(false, true));
    const orchard::Vec3 r = orchard::to_euler(flat.transform(sprite)->rotation);
    check(near(flat.transform(sprite)->position.y, -3.0f) && near(r.z, -30.0f, 0.01f) && near(r.x, 0.0f, 0.01f) && near(r.y, 0.0f, 0.01f), "flipping 2d y mirrors positions and angles and stays flat");
    check(orchard::AxisChange::between(orchard::AxisSystem::unity(), orchard::AxisSystem::y_up().with_flips(orchard::AxisFlip::z)).identity(), "equal axes need no change");
}

void test_combine_settings() {
    orchard::Scene sc(orchard::Dimension::three_d);
    const orchard::Entity e = sc.create("Ice");
    orchard::Collider* c = sc.add_collider(e);
    c->friction_combine = orchard::CombineMode::harmonic_mean;
    c->bounce_combine = orchard::CombineMode::sum;
    sc.add_collider(e);
    const std::string text = sc.save();
    check(text.find("friction-mix:harmonic-mean bounce-mix:sum") != std::string::npos, "collider mixes are saved");
    const std::optional<orchard::Scene> back = orchard::Scene::load(text);
    check(back && back->colliders(back->find("Ice"))->at(0).friction_combine == orchard::CombineMode::harmonic_mean && back->colliders(back->find("Ice"))->at(0).bounce_combine == orchard::CombineMode::sum, "collider mixes round trip");
    check(back && back->colliders(back->find("Ice"))->at(1).friction_combine == orchard::CombineMode::project_default, "colliders without a mix use the project default");
    orchard::ProjectInfo info;
    check(orchard::Project::write_info(info).find("friction-mix: geometric-mean;") != std::string::npos, "projects save their friction mix");
    info.friction_combine = orchard::CombineMode::average;
    info.bounce_combine = orchard::CombineMode::multiply;
    const std::optional<orchard::ProjectInfo> again = orchard::Project::read_info(orchard::Project::write_info(info));
    check(again && again->friction_combine == orchard::CombineMode::average && again->bounce_combine == orchard::CombineMode::multiply, "project mixes round trip");
    check(!orchard::Project::read_info("project {\n    friction-mix: wobbly;\n}\n"), "bad mixes are reported");
    for (orchard::CombineMode m : orchard::combine_modes()) check(orchard::parse_combine_mode(orchard::to_string(m)) == m, std::string("mix names round trip: ") + orchard::to_string(m));
}

void test_viewport() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity cube = s.create("Cube");
    orchard::Viewport v;
    v.resize(320, 200);
    const std::vector<std::uint8_t>& px = v.render(s, cube);
    check(px.size() == 320u * 200u * 4u, "viewport buffer size");
    check(v.pick(s, 160.0f, 100.0f) == cube, "picking the cube at the center");
    v.set_dimension(orchard::Dimension::two_d);
    v.render(s, orchard::Entity{});
    check(v.pick(s, 160.0f, 100.0f) == cube, "picking in 2d");
}

} // namespace

int main() {
    test_math();
    test_scene();
    test_project();
    test_graph();
    test_viewport();
    test_components();
    test_script_hooks();
    test_script_runtime();
    test_native_hooks();
    test_axes();
    test_axis_flips();
    test_remap_scene();
    test_combine_settings();
    test_script_languages();
    std::cout << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
