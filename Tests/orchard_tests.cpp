#include <orchard/orchard.hpp>
#include <orchard/scripting.hpp>
#include <orchard/ai/behavior_tree.hpp>
#include <orchard/ai/search.hpp>
#include <orchard/ai/navgrid.hpp>
#include <orchard/gameplay.hpp>
#include <orchard/random.hpp>
#include <orchard/solid.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
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
    {
        orchard::Scene m(orchard::Dimension::three_d);
        const orchard::Entity e = m.create("Tuned");
        orchard::Body& tb = m.add_body(e);
        tb.collision_layer = 4;
        tb.collision_mask = 5;
        tb.quality = orchard::BodyQuality::high;
        tb.continuous = orchard::BodyContinuous::on;
        tb.velocity_iterations = 12;
        tb.max_speed = 30.0f;
        tb.sleep_delay = 2.0f;
        tb.flags_on = 1ull << 50;
        tb.flags_off = 1ull << 45;
        orchard::Collider* rc = m.add_collider(e, orchard::ColliderShape::rounded_box);
        rc->rounding = 0.2f;
        rc->rolling_friction = 0.05f;
        rc->grip = {2.0f, 1.0f, 0.5f};
        m.add_collider(e, orchard::ColliderShape::cone);
        const std::optional<orchard::Scene> back = orchard::Scene::load(m.save(), &error);
        check(back.has_value(), "tuned body loads: " + error);
        if (back) {
            const orchard::Body* bb = back->body(back->find("Tuned"));
            check(bb && bb->collision_layer == 4 && bb->collision_mask == 5 && bb->quality == orchard::BodyQuality::high && bb->continuous == orchard::BodyContinuous::on, "layers, quality and fast-move mode round trip");
            check(bb && bb->velocity_iterations == 12 && near(bb->max_speed, 30.0f) && near(bb->sleep_delay, 2.0f) && bb->flags_on == (1ull << 50) && bb->flags_off == (1ull << 45), "iterations, caps, sleep and flag overrides round trip");
            const std::vector<orchard::Collider>* cs = back->colliders(back->find("Tuned"));
            check(cs && cs->size() == 2 && (*cs)[0].shape == orchard::ColliderShape::rounded_box && near((*cs)[0].rounding, 0.2f) && near((*cs)[0].rolling_friction, 0.05f) && near((*cs)[0].grip.x, 2.0f) && near((*cs)[0].grip.z, 0.5f), "rounded box with rolling friction and grip round trips");
            check(cs && cs->size() == 2 && (*cs)[1].shape == orchard::ColliderShape::cone, "cone collider round trips");
        }
        const orchard::Body plain;
        check(m.save().find("layer:") != std::string::npos && orchard::Scene(orchard::Dimension::three_d).save().find("layer:") == std::string::npos && plain.collision_mask == ~0ull, "only changed settings are written");
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
    {
        orchard::graph::Graph g;
        const int start = g.add_node("on_start");
        const int tick = g.add_node("on_tick");
        const int seed = g.add_node("random_seed");
        const int add = g.add_node("bag_add");
        const int brain = g.add_node("ai_brain");
        const int sound = g.add_node("play_sound_here");
        const int noise = g.add_node("ai_noise");
        const int branch = g.add_node("branch");
        const int chance = g.add_node("random_chance");
        const int move = g.add_node("ai_move_to");
        const int rnd = g.add_node("random_number");
        const int logn = g.add_node("log");
        const int draw = g.add_node("bag_next");
        const int stop = g.add_node("stop_all_sounds");
        const int see = g.add_node("ai_can_see");
        const int both = g.add_node("and");
        g.connect(start, "then", seed, "in");
        g.connect(seed, "then", add, "in");
        g.connect(add, "then", brain, "in");
        g.connect(brain, "then", sound, "in");
        g.connect(tick, "then", branch, "in");
        g.connect(chance, "result", both, "a");
        g.connect(see, "result", both, "b");
        g.connect(both, "result", branch, "condition");
        g.connect(branch, "true", move, "in");
        g.connect(rnd, "value", move, "x");
        g.connect(move, "then", noise, "in");
        g.connect(branch, "false", logn, "in");
        g.connect(draw, "item", logn, "message");
        g.connect(logn, "then", stop, "in");
        const int log2 = g.add_node("log");
        const int place = g.add_node("set_position");
        const int j1 = g.add_node("join");
        const int j2 = g.add_node("join");
        const int as_text = g.add_node("to_text");
        g.connect(stop, "then", log2, "in");
        g.connect(log2, "then", place, "in");
        g.connect(g.add_node("secure_uuid"), "text", j1, "a");
        g.connect(g.add_node("secure_token"), "text", j1, "b");
        g.connect(j1, "text", j2, "a");
        g.connect(g.add_node("secure_text"), "text", j2, "b");
        g.connect(g.add_node("secure_whole"), "value", as_text, "value");
        g.connect(j2, "text", log2, "message");
        g.connect(g.add_node("secure_random"), "value", place, "x");
        // Items, weapons and weather nodes.
        int last = place;
        for (const char* t : {"give_item", "take_item", "equip_item", "hold_item", "holster_item", "draw_item", "select_slot", "use_item", "use_held", "release_held", "reload_weapon", "fire_weapon", "set_aim", "set_stance", "set_aiming", "set_blocking", "set_ballistics", "drop_item", "pick_up_nearest", "move_item", "define_item", "set_inventory_setting", "spawn_item", "start_weather", "start_weather_at", "stop_all_weather", "set_climate"}) {
            const int n = g.add_node(t);
            check(n >= 0, std::string("the ") + t + " node exists");
            g.connect(last, "then", n, "in");
            last = n;
        }
        const int cond = g.add_node("branch");
        g.connect(last, "then", cond, "in");
        g.connect(g.add_node("has_item"), "result", cond, "condition");
        const int say = g.add_node("log");
        g.connect(cond, "true", say, "in");
        const int tj = g.add_node("join");
        g.connect(g.add_node("held_item"), "item", tj, "a");
        g.connect(g.add_node("inventory_setting"), "value", tj, "b");
        g.connect(tj, "text", say, "message");
        const int warm = g.add_node("set_position");
        g.connect(say, "then", warm, "in");
        g.connect(g.add_node("temperature_at"), "celsius", warm, "x");
        g.connect(g.add_node("rain_at"), "amount", warm, "y");
        g.connect(g.add_node("worn_armor"), "value", warm, "z");
        std::vector<std::string> errors;
        const std::string code = g.to_walnut(&errors);
        check(errors.empty(), "a graph with random, bag, sound and AI nodes makes Walnut");
        std::ofstream(project->path_of("Graphs/lib.graph")) << g.save();
        const orchard::ScriptBuildResult built = runtime.build(*project, "Graphs/lib.graph");
        check(built.ok, "the random, bag, sound and AI graph builds: " + built.log + "\n" + code);
    }
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

void test_script_keys() {
    orchard::ScriptRuntime names;
    names.set_key("Space", true);
    names.set_key("LeftShift", true);
    check(names.key_down("space") && names.key_down("Space") && names.key_down("shift"), "without a resolver, keys are kept by name");
    check(names.physical_key_down("Space") && names.either_key_down("space"), "without a resolver, every kind reads the name");
    names.set_key("Space", false);
    check(!names.key_down("space") && names.key_down("shift"), "releasing one key by name keeps the others");
    names.clear_keys();
    check(!names.key_down("shift"), "clear_keys releases everything");

    enum : int { Q = 17, W = 23, Z = 26, LeftShift = 113, RightShift = 114, Space = 65 };
    orchard::ScriptRuntime codes;
    codes.set_key_resolver([](std::string_view n) {
        std::string l(n);
        for (char& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (l == "q") return static_cast<int>(Q);
        if (l == "w") return static_cast<int>(W);
        if (l == "z") return static_cast<int>(Z);
        if (l == "space") return static_cast<int>(Space);
        if (l == "leftshift") return static_cast<int>(LeftShift);
        if (l == "rightshift") return static_cast<int>(RightShift);
        return 0;
    });
    codes.set_key(W, Z, true);
    check(codes.key_down("w") && codes.logical_key_down("W") && !codes.key_down("z"), "key_down goes by the layout");
    check(codes.physical_key_down("z") && !codes.physical_key_down("w"), "physical_key_down goes by position");
    check(codes.either_key_down("w") && codes.either_key_down("z") && !codes.either_key_down("q"), "either_key_down takes both");
    check(codes.logical_key_down(static_cast<int>(W)) && codes.physical_key_down(static_cast<int>(Z)), "keys can be read by code");
    codes.set_key(Z, Z, false);
    check(!codes.key_down("w") && !codes.physical_key_down("z"), "releasing a key releases its layout key too");
    codes.set_key(RightShift, RightShift, true);
    check(codes.key_down("shift") && codes.physical_key_down("Shift") && codes.key_down("RightShift") && codes.key_down("LeftShift"), "shift, LeftShift and RightShift all mean either Shift key, as before");
    codes.set_key("Space", true);
    check(codes.key_down("space") && codes.physical_key_down(" "), "a key set by name still works with a resolver");
    codes.set_key(Q, W, true);
    codes.set_key(W, W, true);
    check(codes.key_down("w") && !codes.key_down("q"), "a key held again with a new layout key moves to it");
    codes.clear_keys();
    check(!codes.key_down("shift") && !codes.key_down("space") && !codes.physical_key_down("w"), "clear_keys releases every code");
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

void test_damage_scripts() {
    const fs::path dir = fs::temp_directory_path() / "orchard-test-damage";
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::string error;
    std::optional<orchard::Project> project = orchard::Project::create(dir.string(), "Damage", orchard::Dimension::three_d, &error);
    check(project.has_value(), "damage test project: " + error);
    if (!project) return;
    auto write = [&](const char* rel, const std::string& text) {
        std::ofstream out(project->path_of(rel), std::ios::binary);
        out << text;
    };
    write("Scripts/hurt.cpp",
          "#include <orchard/script/orchard.hpp>\nusing namespace orchard::script;\n\n"
          "void on_start(Entity self) {\n    apply_damage(self, 10.0, \"fire\");\n    apply_effect(self, \"poison\", 3.0);\n    set_stat(self, \"max_hunger\", 30.0);\n    damage_over_time(self, 4.0, \"bleed\", 1.0, 0.0);\n    define_effect(\"venom\", \"damage\", \"3\");\n    set_health_setting(self, \"armor_mode\", \"flat\");\n    set_stat(self, \"max_shield\", convert_units(1.0, \"lb\", \"kg\"));\n}\n");
    write("Scripts/hurt_c.c",
          "#include <orchard/script/orchard.h>\n\n"
          "void on_start(orchard_entity self) {\n    orchard_apply_damage(self, 10.0, \"fire\");\n    orchard_apply_effect(self, \"poison\", 3.0, 1.0);\n    orchard_set_stat(self, \"max_hunger\", 30.0);\n    orchard_damage_over_time(self, 4.0, \"bleed\", 1.0, 0.0, 0);\n    orchard_define_effect(\"venom\", \"damage\", \"3\");\n    orchard_set_health_setting(self, \"armor_mode\", \"flat\");\n    orchard_set_stat(self, \"max_shield\", orchard_convert_units(1.0, \"lb\", \"kg\"));\n}\n");
    write("Scripts/hurt.lua",
          "function on_start(self)\n    orchard.apply_damage(self, 10, \"fire\")\n    orchard.apply_effect(self, \"poison\", 3)\n    orchard.set_stat(self, \"max_hunger\", 30)\n    orchard.damage_over_time(self, 4, \"bleed\", 1, 0)\n    orchard.define_effect(\"venom\", \"damage\", \"3\")\n    orchard.set_health_setting(self, \"armor_mode\", \"flat\")\n    orchard.set_stat(self, \"max_shield\", orchard.convert_units(1, \"lb\", \"kg\"))\nend\n");
    write("Scripts/Hurt.cs",
          "using Orchard;\n\npublic class Hurt : Script\n{\n    public override void OnStart()\n    {\n        World.ApplyDamage(Entity, 10, \"fire\");\n        World.ApplyEffect(Entity, \"poison\", 3);\n        World.SetStat(Entity, \"max_hunger\", 30);\n        World.DamageOverTime(Entity, 4, \"bleed\", 1, 0);\n        World.DefineEffect(\"venom\", \"damage\", \"3\");\n        World.SetHealthSetting(Entity, \"armor_mode\", \"flat\");\n        World.SetStat(Entity, \"max_shield\", World.ConvertUnits(1, \"lb\", \"kg\"));\n    }\n}\n");
    write("Scripts/Hurt.java",
          "import orchard.*;\n\npublic class Hurt extends Script {\n    @Override\n    public void onStart() {\n        World.applyDamage(entity, 10, \"fire\");\n        World.applyEffect(entity, \"poison\", 3);\n        World.setStat(entity, \"max_hunger\", 30);\n        World.damageOverTime(entity, 4, \"bleed\", 1, 0);\n        World.defineEffect(\"venom\", \"damage\", \"3\");\n        World.setHealthSetting(entity, \"armor_mode\", \"flat\");\n        World.setStat(entity, \"max_shield\", World.convertUnits(1, \"lb\", \"kg\"));\n    }\n}\n");
    {
        orchard::graph::Graph g;
        const int start = g.add_node("on_start");
        const int hit = g.add_node("apply_damage");
        g.set_value(hit, "type", "fire");
        const int poison = g.add_node("apply_effect");
        g.set_value(poison, "duration", "3");
        const int stat = g.add_node("set_stat");
        g.set_value(stat, "stat", "max_hunger");
        g.set_value(stat, "value", "30");
        const int bleed = g.add_node("apply_damage");
        g.set_value(bleed, "amount", "4");
        g.set_value(bleed, "type", "bleed");
        g.set_value(bleed, "passive", "true");
        g.set_value(bleed, "duration", "1");
        g.set_value(bleed, "interval", "0");
        g.set_value(bleed, "push_repeat", "every pulse");
        const int define = g.add_node("define_effect");
        const int armor = g.add_node("set_health_setting");
        const int shield = g.add_node("set_stat");
        g.set_value(shield, "stat", "max_shield");
        const int pounds = g.add_node("convert_units");
        g.set_value(pounds, "from", "lb");
        g.set_value(pounds, "to", "kg");
        g.connect(pounds, "result", shield, "value");
        const int branch = g.add_node("branch");
        const int has = g.add_node("has_effect");
        const int stagger = g.add_node("stagger");
        g.connect(start, "then", hit, "in");
        g.connect(hit, "then", poison, "in");
        g.connect(poison, "then", stat, "in");
        g.connect(stat, "then", bleed, "in");
        g.connect(bleed, "then", define, "in");
        g.connect(define, "then", armor, "in");
        g.connect(armor, "then", shield, "in");
        g.connect(shield, "then", branch, "in");
        g.connect(has, "result", branch, "condition");
        g.connect(branch, "true", stagger, "in");
        std::vector<std::string> errors;
        const std::string code = g.to_walnut(&errors);
        check(errors.empty(), "a graph with damage, effect and stat nodes makes Walnut");
        check(code.find("orchard::damage_over_time_from(entity, entity, 4.0, \"bleed\", 1.0, 0.0, false, 0.0, 0.0, 0.0, 1.0, -1.0, false, \"\")") != std::string::npos, "passive damage nodes pass their duration, interval and push repeat:\n" + code);
        check(code.find("orchard::apply_effect_from(entity, entity, \"poison\", 3.0, 1.0)") != std::string::npos, "effect nodes pass their duration and strength");
        g.set_value(bleed, "push_repeat", "sometimes");
        g.to_walnut(&errors);
        check(!errors.empty(), "a bad push repeat is reported");
        g.set_value(bleed, "push_repeat", "every pulse");
        write("Graphs/hurt.graph", g.save());
    }
    orchard::ScriptRuntime runtime;
    orchard::ScriptToolchain tools;
    const char* walnut = std::getenv("ORCHARD_TEST_WALNUT");
    if (walnut) tools.walnut = walnut;
    if (const char* cxx = std::getenv("CXX")) tools.cxx = cxx;
    if (const char* cc = std::getenv("CC")) tools.cc = cc;
    tools.include_dir = orchard::ScriptToolchain::find_include_dir("");
    tools.cache_dir = (fs::temp_directory_path() / "orchard-test-languages" / ".orchard" / "scripts").string();
    runtime.set_toolchain(tools);
    struct Case {
        const char* script;
        const char* language;
    };
    const std::vector<Case> cases = {{"Scripts/hurt.cpp", "C++"}, {"Scripts/hurt_c.c", "C"}, {"Scripts/hurt.lua", "Lua"}, {"Scripts/Hurt.cs", "C#"}, {"Scripts/Hurt.java", "Java"}, {"Graphs/hurt.graph", "Walnut"}};
    orchard::Scene scene(orchard::Dimension::three_d);
    std::vector<std::pair<std::string, orchard::Entity>> running;
    for (const Case& c : cases) {
        orchard::ScriptLanguage* lang = runtime.language(c.language);
        if (!lang || !lang->available(tools) || (std::string(c.language) == "Walnut" && !walnut)) continue;
        const orchard::ScriptBuildResult built = runtime.build(*project, c.script);
        check(built.ok, std::string("building ") + c.script + ": " + built.log);
        if (!built.ok) continue;
        const orchard::Entity e = scene.create(c.language);
        orchard::Health& h = scene.add_health(e);
        h.resist["fire"] = 0.5f;
        scene.add_survival(e);
        scene.set_script(e, c.script);
        running.push_back({c.language, e});
    }
    std::vector<std::string> logs;
    runtime.set_log([&logs](const std::string& m, bool bad) { if (bad) logs.push_back(m); });
    check(runtime.start(*project, scene), "starting the damage scripts");
    runtime.tick(0.25);
    runtime.tick(0.25);
    orchard::Gameplay& g = runtime.gameplay();
    for (const auto& [language, e] : running) {
        const float hp = g.health(e);
        check(hp < 95.0f && hp > 85.0f, language + " scripts deal typed damage through resistances and keep bleeding (" + std::to_string(hp) + ")");
        check(g.effect_stacks(e, "poison") == 1 && g.effect_time_left(e, "poison") > 2.0f, language + " scripts apply effects");
        check(g.stat(e, "max_hunger") == 30.0f, language + " scripts set survival stats");
        check(g.last_damage_type(e) == "bleed", language + " scripts' passive damage is typed (" + g.last_damage_type(e) + ")");
        check(g.health_setting(e, "armor_mode") == "flat", language + " scripts change how armor works");
        check(std::fabs(g.stat(e, "max_shield") - 0.45359237f) < 1e-4f, language + " scripts convert units (" + std::to_string(g.stat(e, "max_shield")) + ")");
    }
    check(g.effect_def("venom") && g.effect_def("venom")->damage == 3.0f, "scripts can define effects");
    for (const std::string& l : logs) check(false, "script error: " + l);
    runtime.stop();
    fs::remove_all(dir, ec);
}

void test_item_scripts() {
    const fs::path dir = fs::temp_directory_path() / "orchard-test-items";
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::string error;
    std::optional<orchard::Project> project = orchard::Project::create(dir.string(), "Items", orchard::Dimension::three_d, &error);
    check(project.has_value(), "item test project: " + error);
    if (!project) return;
    auto write = [&](const char* rel, const std::string& text) {
        std::ofstream out(project->path_of(rel), std::ios::binary);
        out << text;
    };
    write("Scripts/gear.cpp",
          "#include <orchard/script/orchard.hpp>\nusing namespace orchard::script;\n\n"
          "void on_start(Entity self) {\n    set_inventory_setting(self, \"preset\", \"tarkov\");\n    give_item(self, \"glock17\", 1.0);\n    give_item(self, \"9mm\", 20.0);\n    Entity w = start_weather(\"rain\", 30.0, 0.0, 0.0);\n    set_climate(w, \"temperature\", \"5\");\n    spawn_item(\"apple\", 2.0, 0.0, 1.0, 0.0);\n}\n");
    write("Scripts/gear_c.c",
          "#include <orchard/script/orchard.h>\n\n"
          "void on_start(orchard_entity self) {\n    orchard_set_inventory_setting(self, \"preset\", \"tarkov\");\n    orchard_give_item(self, \"glock17\", 1.0);\n    orchard_give_item(self, \"9mm\", 20.0);\n    orchard_entity w = orchard_start_weather(\"rain\", 30.0, 0.0, 0.0);\n    orchard_set_climate(w, \"temperature\", \"5\");\n    orchard_spawn_item(\"apple\", 2.0, 0.0, 1.0, 0.0, \"detailed\");\n}\n");
    write("Scripts/gear.lua",
          "function on_start(self)\n    orchard.set_inventory_setting(self, \"preset\", \"tarkov\")\n    orchard.give_item(self, \"glock17\", 1)\n    orchard.give_item(self, \"9mm\", 20)\n    local w = orchard.start_weather(\"rain\", 30, 0, 0)\n    orchard.set_climate(w, \"temperature\", \"5\")\n    orchard.spawn_item(\"apple\", 2, 0, 1, 0)\nend\n");
    write("Scripts/Gear.cs",
          "using Orchard;\n\npublic class Gear : Script\n{\n    public override void OnStart()\n    {\n        World.SetInventorySetting(Entity, \"preset\", \"tarkov\");\n        World.GiveItem(Entity, \"glock17\", 1);\n        World.GiveItem(Entity, \"9mm\", 20);\n        Entity w = World.StartWeather(\"rain\", 30, 0, 0);\n        World.SetClimate(w, \"temperature\", \"5\");\n        World.SpawnItem(\"apple\", 2, 0, 1, 0);\n    }\n}\n");
    write("Scripts/Gear.java",
          "import orchard.*;\n\npublic class Gear extends Script {\n    @Override\n    public void onStart() {\n        World.setInventorySetting(entity, \"preset\", \"tarkov\");\n        World.giveItem(entity, \"glock17\", 1);\n        World.giveItem(entity, \"9mm\", 20);\n        Entity w = World.startWeather(\"rain\", 30, 0, 0);\n        World.setClimate(w, \"temperature\", \"5\");\n        World.spawnItem(\"apple\", 2, 0, 1, 0);\n    }\n}\n");
    {
        orchard::graph::Graph g;
        const int start = g.add_node("on_start");
        const int preset = g.add_node("set_inventory_setting");
        g.set_value(preset, "value", "tarkov");
        const int gun = g.add_node("give_item");
        g.set_value(gun, "item", "glock17");
        const int ammo = g.add_node("give_item");
        g.set_value(ammo, "item", "9mm");
        g.set_value(ammo, "amount", "20");
        const int rain = g.add_node("start_weather");
        g.set_value(rain, "duration", "30");
        const int apple = g.add_node("spawn_item");
        g.set_value(apple, "amount", "2");
        g.connect(start, "then", preset, "in");
        g.connect(preset, "then", gun, "in");
        g.connect(gun, "then", ammo, "in");
        g.connect(ammo, "then", rain, "in");
        g.connect(rain, "then", apple, "in");
        std::vector<std::string> errors;
        g.to_walnut(&errors);
        check(errors.empty(), "a graph with item and weather nodes makes Walnut");
        write("Graphs/gear.graph", g.save());
    }
    orchard::ScriptRuntime runtime;
    orchard::ScriptToolchain tools;
    const char* walnut = std::getenv("ORCHARD_TEST_WALNUT");
    if (walnut) tools.walnut = walnut;
    if (const char* cxx = std::getenv("CXX")) tools.cxx = cxx;
    if (const char* cc = std::getenv("CC")) tools.cc = cc;
    tools.include_dir = orchard::ScriptToolchain::find_include_dir("");
    tools.cache_dir = (fs::temp_directory_path() / "orchard-test-languages" / ".orchard" / "scripts").string();
    runtime.set_toolchain(tools);
    const std::vector<std::pair<const char*, const char*>> cases = {{"Scripts/gear.cpp", "C++"}, {"Scripts/gear_c.c", "C"}, {"Scripts/gear.lua", "Lua"}, {"Scripts/Gear.cs", "C#"}, {"Scripts/Gear.java", "Java"}, {"Graphs/gear.graph", "Walnut"}};
    orchard::Scene scene(orchard::Dimension::three_d);
    std::vector<std::pair<std::string, orchard::Entity>> running;
    for (const auto& [script, language] : cases) {
        orchard::ScriptLanguage* lang = runtime.language(language);
        if (!lang || !lang->available(tools) || (std::string(language) == "Walnut" && !walnut)) continue;
        const orchard::ScriptBuildResult built = runtime.build(*project, script);
        check(built.ok, std::string("building ") + script + ": " + built.log);
        if (!built.ok) continue;
        const orchard::Entity e = scene.create(language);
        scene.set_script(e, script);
        running.push_back({language, e});
    }
    std::vector<std::string> logs;
    runtime.set_log([&logs](const std::string& m, bool bad) { if (bad) logs.push_back(m); });
    check(runtime.start(*project, scene), "starting the item scripts");
    runtime.tick(0.1);
    orchard::Gameplay& g = runtime.gameplay();
    for (const auto& [language, e] : running) {
        const orchard::Inventory* inv = scene.inventory(e);
        check(inv && inv->preset == "tarkov", language + " scripts give things an inventory and set it up");
        check(g.count(e, "glock17") == 1.0f && g.count(e, "9mm") == 20.0f, language + " scripts give items (" + std::to_string(g.count(e, "9mm")) + ")");
    }
    int zones = 0, apples = 0;
    for (orchard::Entity e : scene.entities()) {
        if (const orchard::Climate* c = scene.climate(e)) zones += c->preset == "rain" && c->temperature == 5.0f ? 1 : 0;
        if (const orchard::Pickup* p = scene.pickup(e)) apples += p->stack.item == "apple" && p->stack.amount == 2.0f ? 1 : 0;
    }
    check(zones >= static_cast<int>(running.size()) - (walnut ? 1 : 0), "scripts start weather and change it (" + std::to_string(zones) + " zones)");
    check(apples == static_cast<int>(running.size()), "scripts spawn item pickups (" + std::to_string(apples) + ")");
    for (const std::string& l : logs) check(false, "script error: " + l);
    runtime.stop();
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

void test_lights_and_materials() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity lamp = s.create("Lamp");
    orchard::Light& l = s.add_light(lamp, orchard::LightType::rectangle);
    check(l.type == orchard::LightType::rectangle && l.width == 1.0f, "a new rectangle light starts 1 by 1");
    l.color = {1.0f, 0.5f, 0.25f};
    l.temperature = 3200.0f;
    l.unit = orchard::LightUnit::lumens;
    l.intensity = 800.0f;
    l.two_sided = true;
    l.mobility = orchard::LightMobility::stationary;
    l.ies = "Assets/My Lights/down light.ies";
    l.channels = 3u;
    const orchard::Entity sun = s.create("Sun");
    s.add_light(sun, orchard::LightType::directional).angular_diameter = 2.0f;
    const orchard::Entity ball = s.create("Ball");
    s.add_collider(ball, orchard::ColliderShape::sphere);
    orchard::Material& m = s.add_material(ball);
    m = *orchard::Material::named("diamond");
    m.light_channels = 2u;
    m.is_static = true;
    const std::string text = s.save();
    check(text.find("orchard-scene 3") == 0, "scenes with lights save as version 3");
    std::string err;
    const auto back = orchard::Scene::load(text, &err);
    check(back.has_value(), "the scene loads back " + err);
    if (!back) return;
    const orchard::Light* bl = back->light(back->find("Lamp"));
    check(bl && bl->type == orchard::LightType::rectangle && bl->unit == orchard::LightUnit::lumens && bl->intensity == 800.0f && bl->two_sided && bl->temperature == 3200.0f, "light round trip");
    check(bl && bl->mobility == orchard::LightMobility::stationary && bl->channels == 3u && near(bl->color.y, 0.5f), "light mobility, channels and color round trip");
    check(bl && bl->ies == "Assets/My Lights/down light.ies", "paths with spaces survive");
    const orchard::Light* bs = back->light(back->find("Sun"));
    check(bs && bs->type == orchard::LightType::directional && bs->angular_diameter == 2.0f, "sun round trip");
    const orchard::Material* bm = back->material(back->find("Ball"));
    check(bm && bm->preset == "diamond" && near(bm->ior, 2.417f) && bm->transmission == 1.0f && bm->light_channels == 2u && bm->is_static, "material round trip");
    check(orchard::Material::named("gold")->metallic == 1.0f && !orchard::Material::named("nonsense"), "material presets");
    check(orchard::parse_light_type("sun") == orchard::LightType::directional && orchard::parse_light_type("window") == orchard::LightType::rectangle && orchard::parse_light_type("neon") == std::nullopt, "light type names");
    for (orchard::LightType t : orchard::light_types()) check(orchard::parse_light_type(orchard::to_string(t)) == t, std::string("light type round trip ") + orchard::to_string(t));
    orchard::Scene copy = *back;
    check(copy.remove_light(copy.find("Lamp")) && !copy.light(copy.find("Lamp")), "lights can be removed");
    check(copy.remove_material(copy.find("Ball")) && !copy.material(copy.find("Ball")), "materials can be removed");
    const auto old = orchard::Scene::load("orchard-scene 2 3d\nentity 1 0 0 0 0 0 0 0 1 1 1 Cube\n", &err);
    check(old && old->size() == 1, "version 2 scenes still load");
}

void test_textures_sections_and_fluids() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity crate = s.create("Crate");
    s.add_collider(crate, orchard::ColliderShape::box);
    orchard::Material& m = s.add_material(crate);
    m.base_map = "Assets/Wood Crate/albedo.png";
    m.normal_map = "Assets/crate_n.png";
    m.roughness_map = "Textures/rough.jpg";
    m.normal_strength = 0.5f;
    m.alpha_cutoff = 0.4f;
    m.tiling_u = 3.0f;
    m.tiling_v = 2.0f;
    m.offset_u = 0.25f;
    m.uv_rotation = 45.0f;
    m.pixelated = true;
    m.triplanar = true;
    m.triplanar_scale = 2.0f;
    m.reflectivity = 0.7f;
    const orchard::Entity dome = s.create("Dome");
    orchard::Collider* c = s.add_collider(dome, orchard::ColliderShape::sphere_section);
    check(c != nullptr, "a sphere section collider can be added");
    if (c) {
        c->polar_max = 90.0f;
        c->azimuth_max = 180.0f;
        c->shell = 0.1f;
    }
    const orchard::Entity lake = s.create("Lake");
    orchard::Fluid& w = s.add_fluid(lake);
    w.level = orchard::FluidLevel::realistic;
    w.viscosity = 0.25f;
    w.fill = 0.6f;
    std::string err;
    const auto back = orchard::Scene::load(s.save(), &err);
    check(back.has_value(), "textured scene loads back " + err);
    if (!back) return;
    const orchard::Material* bm = back->material(back->find("Crate"));
    check(bm && bm->base_map == "Assets/Wood Crate/albedo.png" && bm->normal_map == "Assets/crate_n.png" && bm->roughness_map == "Textures/rough.jpg", "texture paths round trip");
    check(bm && near(bm->normal_strength, 0.5f) && near(bm->alpha_cutoff, 0.4f) && near(bm->tiling_u, 3.0f) && near(bm->tiling_v, 2.0f) && near(bm->offset_u, 0.25f) && near(bm->uv_rotation, 45.0f), "texture settings round trip");
    check(bm && bm->pixelated && bm->triplanar && near(bm->triplanar_scale, 2.0f) && near(bm->reflectivity, 0.7f), "texture flags and reflectivity round trip");
    check(bm && bm->has_textures() && bm->texture_paths().size() == 3, "texture paths are listed");
    check(!orchard::Material{}.has_textures(), "plain materials have no textures");
    check(orchard::is_image_file("a/b.PNG") && orchard::is_image_file("x.jpeg") && !orchard::is_image_file("x.wal") && !orchard::is_image_file("png"), "image file names");
    const std::vector<orchard::Collider>* bc = back->colliders(back->find("Dome"));
    check(bc && bc->size() == 1 && (*bc)[0].shape == orchard::ColliderShape::sphere_section && near((*bc)[0].azimuth_max, 180.0f) && near((*bc)[0].shell, 0.1f) && near((*bc)[0].polar_max, 90.0f), "sphere section round trip");
    const orchard::Fluid* bf = back->fluid(back->find("Lake"));
    check(bf && bf->level == orchard::FluidLevel::realistic && near(bf->viscosity, 0.25f) && near(bf->fill, 0.6f), "fluid round trip");
    for (orchard::FluidLevel v : orchard::fluid_levels()) check(orchard::parse_fluid_level(orchard::to_string(v)) == v, std::string("fluid level round trip ") + orchard::to_string(v));
}

void test_entity_templates_and_emitters() {
    orchard::Scene tmpl(orchard::Dimension::three_d);
    const orchard::Entity car = tmpl.create("Cart");
    tmpl.add_body(car);
    tmpl.add_collider(car, orchard::ColliderShape::box)->size = {2.0f, 0.5f, 1.0f};
    tmpl.set_script(car, "Scripts/drive.wal");
    tmpl.add_material(car).base_map = "Textures/wood.png";
    const orchard::Entity wheel = tmpl.create("Wheel", car);
    tmpl.add_collider(wheel, orchard::ColliderShape::sphere);
    tmpl.transform(wheel)->position = {0.8f, -0.25f, 0.5f};
    orchard::Emitter& fx = tmpl.add_emitter(wheel);
    fx = *orchard::Emitter::named("dust");
    fx.rate = 25.0f;
    fx.on_death = "smoke";
    const auto back_t = orchard::Scene::load(tmpl.save());
    check(back_t.has_value(), "template with emitter loads");
    if (!back_t) return;
    const orchard::Emitter* be = back_t->emitter(back_t->find("Wheel"));
    check(be && be->preset == "dust" && near(be->rate, 25.0f) && be->on_death == "smoke" && be->blend == orchard::ParticleBlend::alpha && be->shape == orchard::EmitterShape::box, "emitter round trip");
    for (const std::string& p : orchard::Emitter::presets()) check(orchard::Emitter::named(p).has_value(), "emitter preset " + p);
    for (orchard::EmitterShape sh : orchard::emitter_shapes()) check(orchard::parse_emitter_shape(orchard::to_string(sh)) == sh, std::string("emitter shape ") + orchard::to_string(sh));

    orchard::Scene world(orchard::Dimension::three_d);
    orchard::Transform at;
    at.position = {10.0f, 0.0f, 0.0f};
    const orchard::Entity a = world.instantiate(*back_t, "Entities/Cart.entity", orchard::Entity{}, at);
    at.position = {-5.0f, 0.0f, 2.0f};
    const orchard::Entity b = world.instantiate(*back_t, "Entities/Cart.entity", orchard::Entity{}, at);
    check(world.alive(a) && world.alive(b) && world.size() == 4, "two copies with their children");
    check(world.instances_of("Entities/Cart.entity").size() == 2, "copies remember their template");
    check(near(world.transform(a)->position.x, 10.0f) && world.script(a) == "Scripts/drive.wal" && world.script(b) == "Scripts/drive.wal", "copies share the script and sit where placed");
    check(world.children(a).size() == 1 && world.emitter(world.children(a)[0]) != nullptr, "children and emitters are copied");
    const auto saved = orchard::Scene::load(world.save());
    check(saved && saved->instances_of("Entities/Cart.entity").size() == 2, "instance links survive saving");

    orchard::Scene edited = *back_t;
    edited.add_light(edited.find("Cart"));
    edited.create("Bumper", edited.find("Cart"));
    world.set_name(a, "Red cart");
    check(world.refresh_instances("Entities/Cart.entity", edited) == 2, "editing the template updates both copies");
    check(world.light(a) && world.light(b) && world.children(a).size() == 2, "updated copies get the new parts");
    check(world.name(a) == "Red cart" && near(world.transform(a)->position.x, 10.0f), "copies keep their own name and place");
    check(world.size() == 6, "old children were replaced, not doubled");

    const orchard::Entity c = world.duplicate(a);
    check(world.alive(c) && world.children(c).size() == 2 && world.instance_of(c) == "Entities/Cart.entity" && world.size() == 9, "duplicate copies the whole entity");
    const orchard::Scene part = world.extract(a);
    check(part.size() == 3 && part.instance_of(part.roots().front()).empty() && near(part.transform(part.roots().front())->position.x, 0.0f), "extract makes a template from a subtree");
    world.set_instance_of(b, "");
    check(world.instances_of("Entities/Cart.entity").size() == 2, "unlinking drops one copy from the template");

    orchard::Scene two(orchard::Dimension::three_d);
    two.create("A");
    two.create("B");
    const orchard::Entity group = world.instantiate(two, "Entities/Pair.entity", orchard::Entity{}, orchard::Transform{});
    check(world.name(group) == "Pair" && world.children(group).size() == 2, "templates with several roots get a group");
    orchard::Scene cams(orchard::Dimension::three_d);
    const orchard::Entity named_cam = cams.create("Main Camera");
    check(cams.active_camera() == named_cam, "an entity named Camera is the fallback game camera");
    const orchard::Entity real = cams.create("Eye");
    orchard::Camera& cc = cams.add_camera(real);
    cc.fov = 75.0f;
    cc.priority = 2;
    check(cams.active_camera() == real, "a camera component wins over a name");
    const auto cams_back = orchard::Scene::load(cams.save());
    check(cams_back && cams_back->camera(cams_back->find("Eye")) && near(cams_back->camera(cams_back->find("Eye"))->fov, 75.0f) && cams_back->camera(cams_back->find("Eye"))->priority == 2, "camera round trip");
    check(orchard::ScriptRuntime::key_name("W") == "w" && orchard::ScriptRuntime::key_name("ArrowLeft") == "left" && orchard::ScriptRuntime::key_name("LeftShift") == "shift", "key names");
    check(orchard::Project::entity_template_path("My Crate!") == "Entities/My Crate.entity", "template file names");
    check(orchard::ScriptRuntime::entity_template_file("Crate") == "Entities/Crate.entity" && orchard::ScriptRuntime::entity_template_file("Entities/Crate.entity") == "Entities/Crate.entity" && orchard::ScriptRuntime::entity_template_file("More/Thing") == "More/Thing.entity", "template names from scripts");
}

void test_light_animation() {
    orchard::LightAnimation a = orchard::LightAnimation::strobe(2.0f, 0.25f);
    check(a.active() && a.mode == orchard::LightAnimationMode::strobe, "strobe: the preset turns the effect on");
    const orchard::Vec3 white{1.0f, 1.0f, 1.0f};
    check(near(a.evaluate(0.05, white).brightness, 1.0f) && a.evaluate(0.05, white).on, "strobe: lit at the start of a flash");
    check(near(a.evaluate(0.3, white).brightness, 0.0f) && !a.evaluate(0.3, white).on, "strobe: dark after the flash length");
    check(near(a.evaluate(0.55, white).brightness, 1.0f), "strobe: two flashes a second means lit again at 0.55 s");
    a.low = 0.2f;
    a.high = 3.0f;
    check(near(a.evaluate(0.3, white).brightness, 0.2f) && near(a.evaluate(0.05, white).brightness, 3.0f), "strobe: brightness when off and on");
    orchard::LightAnimation ramp = orchard::LightAnimation::strobe(1.0f, 0.5f);
    ramp.rate_end = 9.0f;
    ramp.duration = 2.0f;
    ramp.loop = false;
    int flips = 0;
    bool was = true;
    for (int i = 0; i < 2000; ++i) {
        const bool on = ramp.evaluate(i * 0.001, white).on;
        if (on != was) ++flips;
        was = on;
    }
    check(flips >= 18 && flips <= 22, "strobe: speeding from 1 to 9 flashes a second gives about 10 flashes over 2 s (" + std::to_string(flips / 2) + ")");
    check(!ramp.evaluate(2.5, white).running && near(ramp.evaluate(2.5, white).brightness, 1.0f), "strobe: after the duration it stays steady when it doesn't loop");
    ramp.loop = true;
    check(ramp.evaluate(2.05, white).running, "strobe: a looping effect starts over");
    orchard::LightAnimation delayed = orchard::LightAnimation::pulse(1.0f, 0.0f);
    delayed.delay = 1.0f;
    check(!delayed.evaluate(0.5, white).running && near(delayed.evaluate(1.5, white).brightness, 1.0f) && near(delayed.evaluate(1.0, white).brightness, 0.0f), "pulse: waits for its delay, then breathes");
    orchard::LightAnimation colors;
    colors.mode = orchard::LightAnimationMode::strobe;
    colors.low = 1.0f;
    colors.color_mode = orchard::LightColorMode::alternate;
    colors.color_b = {0.0f, 0.0f, 1.0f};
    colors.rate = colors.rate_end = 1.0f;
    const orchard::Vec3 red{1.0f, 0.0f, 0.0f};
    check(near(colors.evaluate(0.2, red).color.x, 1.0f) && near(colors.evaluate(1.2, red).color.z, 1.0f), "color: alternate swaps to the second color every other flash");
    orchard::LightAnimation flick = orchard::LightAnimation::flicker(10.0f, 0.5f);
    float lo = 10.0f, hi = -10.0f;
    for (int i = 0; i < 400; ++i) {
        const float b = flick.evaluate(i * 0.01, white).brightness;
        lo = std::min(lo, b);
        hi = std::max(hi, b);
    }
    check(lo >= 0.5f - 1e-4f && hi <= 1.0f + 1e-4f && hi - lo > 0.2f, "flicker: wanders between its low and high brightness");
    orchard::LightAnimation rainbow;
    rainbow.color_mode = orchard::LightColorMode::rainbow;
    rainbow.rate = rainbow.rate_end = 1.0f;
    check(rainbow.active() && rainbow.evaluate(0.0, white).color.x > rainbow.evaluate(0.0, white).color.z && rainbow.evaluate(0.66, white).color.z > rainbow.evaluate(0.66, white).color.x, "color: rainbow goes around the hues");
    for (orchard::LightAnimationMode m : orchard::light_animation_modes()) check(orchard::parse_light_animation_mode(orchard::to_string(m)) == m, std::string("animation name round trip ") + orchard::to_string(m));
    for (orchard::LightColorMode m : orchard::light_color_modes()) check(orchard::parse_light_color_mode(orchard::to_string(m)) == m, std::string("color mode name round trip ") + orchard::to_string(m));

    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity club = s.create("Club");
    orchard::Light& l = s.add_light(club, orchard::LightType::spot);
    l.animation = orchard::LightAnimation::strobe(12.0f, 0.1f);
    l.animation.rate_end = 3.0f;
    l.animation.duration = 4.5f;
    l.animation.delay = 0.25f;
    l.animation.loop = false;
    l.animation.low = 0.05f;
    l.animation.high_end = 2.0f;
    l.animation.softness = 0.3f;
    l.animation.color_mode = orchard::LightColorMode::blend;
    l.animation.color_b = {0.1f, 0.9f, 0.2f};
    l.animation.phase = 0.5f;
    l.animation.seed = 7u;
    const std::string text = s.save();
    check(text.find("anim:strobe") != std::string::npos && text.find("anim-rate:12") != std::string::npos, "strobe settings are saved on the light line");
    std::string err;
    const auto back = orchard::Scene::load(text, &err);
    const orchard::Light* bl = back ? back->light(back->find("Club")) : nullptr;
    check(bl && bl->animation.mode == orchard::LightAnimationMode::strobe && near(bl->animation.rate, 12.0f) && near(bl->animation.rate_end, 3.0f) && near(bl->animation.on_fraction, 0.1f), "strobe speed and flash length round trip");
    check(bl && near(bl->animation.duration, 4.5f) && near(bl->animation.delay, 0.25f) && !bl->animation.loop && near(bl->animation.low, 0.05f) && near(bl->animation.high_end, 2.0f) && near(bl->animation.softness, 0.3f), "strobe duration, delay and brightness round trip");
    check(bl && bl->animation.color_mode == orchard::LightColorMode::blend && near(bl->animation.color_b.y, 0.9f) && near(bl->animation.phase, 0.5f) && bl->animation.seed == 7u, "strobe colors round trip");
    orchard::Scene plain(orchard::Dimension::three_d);
    plain.add_light(plain.create("Lamp"));
    check(plain.save().find("anim") == std::string::npos, "steady lights don't write any strobe settings");
}

void test_project_physics_and_lighting() {
    orchard::ProjectInfo info;
    info.physics = orchard::PhysicsSettings::preset(orchard::PhysicsQuality::high);
    info.lighting.quality = orchard::LightingLevel::realistic;
    info.lighting.editor_quality = orchard::LightingLevel::simple;
    info.lighting.players_choose = false;
    info.lighting.set("shadows", "hard");
    info.lighting.set("sky", "physical");
    info.lighting.set("photon.photons", "1000000");
    const std::string text = orchard::Project::write_info(info);
    check(text.find("physics {") != std::string::npos && text.find("lighting {") != std::string::npos, "project.orchard gets physics and lighting blocks");
    std::string err;
    const auto back = orchard::Project::read_info(text, &err);
    check(back.has_value(), "the blocks read back " + err);
    if (!back) return;
    check(back->physics == info.physics && back->physics.substeps == 2, "physics quality round trip");
    check(back->lighting.quality == orchard::LightingLevel::realistic && back->lighting.editor_quality == orchard::LightingLevel::simple && !back->lighting.players_choose, "lighting quality round trip");
    check(back->lighting.get("shadows") && *back->lighting.get("shadows") == "hard" && back->lighting.get("photon.photons"), "lighting settings round trip");
    orchard::ProjectInfo custom;
    custom.physics.quality = orchard::PhysicsQuality::custom;
    custom.physics.substeps = 7;
    custom.physics.continuous_all = true;
    const auto c = orchard::Project::read_info(orchard::Project::write_info(custom), &err);
    check(c && c->physics.quality == orchard::PhysicsQuality::custom && c->physics.substeps == 7 && c->physics.continuous_all, "custom physics keeps its own numbers");
    const auto plain = orchard::Project::read_info("project {\n    name: \"Old\";\n}\n", &err);
    check(plain && plain->physics == orchard::PhysicsSettings() && plain->lighting.quality == orchard::LightingLevel::shadowed, "old project files get the defaults");
    check(!orchard::Project::read_info("project {\n}\nphysics {\n    quality: warp;\n}\n", &err) && err.find("physics quality") != std::string::npos, "bad physics quality is reported");
    check(!orchard::Project::read_info("project {\n}\nphysics {\n    substeps: 0;\n}\n", &err), "substeps must be at least 1");
    for (orchard::PhysicsQuality q : orchard::physics_qualities()) check(orchard::parse_physics_quality(orchard::to_string(q)) == q, std::string("physics quality name ") + orchard::to_string(q));
    for (orchard::LightingLevel q : orchard::lighting_levels()) check(orchard::parse_lighting_level(orchard::to_string(q)) == q, std::string("lighting level name ") + orchard::to_string(q));
    check(orchard::PhysicsSettings::preset(orchard::PhysicsQuality::arcade).velocity_iterations < orchard::PhysicsSettings::preset(orchard::PhysicsQuality::realistic).velocity_iterations, "arcade is cheaper than realistic");
}

} // namespace

#include "../Editor/graph_view.hpp"

namespace {

void test_graph_view() {
    using orchard::editor::GraphEventKind;
    using orchard::editor::GraphHitKind;
    orchard::graph::Graph g;
    const int start = g.add_node("on_start", 0.0f, 0.0f);
    const int log = g.add_node("log", 300.0f, 0.0f);
    orchard::editor::GraphView v;
    v.set_size(800.0f, 600.0f);
    v.load(g);
    v.set_camera(orchard::editor::GraphCamera{});
    check(!v.dirty() && v.graph().nodes().size() == 2, "graph view loads a graph clean");

    float ox = 0.0f, oy = 0.0f, ix = 0.0f, iy = 0.0f;
    check(v.pin_position(start, "then", true, ox, oy) && v.pin_position(log, "in", false, ix, iy), "graph view knows where pins are");
    orchard::editor::GraphHit hit = v.hit_test(ox, oy);
    check(hit.kind == GraphHitKind::pin && hit.node == start && hit.output && hit.pin == "then", "hit test finds an output pin");
    check(v.hit_test(320.0f, 10.0f).kind == GraphHitKind::header && v.hit_test(320.0f, 10.0f).node == log, "hit test finds a node header");
    check(v.hit_test(700.0f, 500.0f).kind == GraphHitKind::empty, "hit test finds empty space");

    v.press(ox, oy, 1, false, false);
    v.drag((ox + ix) * 0.5f, oy + 20.0f);
    v.drag(ix + 2.0f, iy + 1.0f);
    orchard::editor::GraphEvent ev = v.release(ix + 2.0f, iy + 1.0f);
    check(ev.kind == GraphEventKind::changed && v.graph().links().size() == 1 && v.dirty(), "dragging from an output to an input connects them");
    check(v.hit_test((ox + ix) * 0.5f, oy).kind == GraphHitKind::link, "hit test finds a link");

    orchard::editor::GraphEditRequest edit;
    float mx = 0.0f, my = 0.0f;
    v.pin_position(log, "message", false, mx, my);
    for (float x = mx; x < mx + 200.0f; x += 2.0f) {
        if (v.hit_test(x, my).kind == GraphHitKind::value) {
            v.press(x, my, 1, false, false);
            ev = v.release(x, my);
            edit = ev.edit;
            break;
        }
    }
    check(ev.kind == GraphEventKind::edit && edit.node == log && edit.pin == "message" && edit.value == "Hello", "clicking a value asks the host to edit it");
    check(v.set_value(log, "message", "Hi") && v.compile().code.find("orchard::log(\"Hi\");") != std::string::npos, "edited values reach the Walnut");

    v.press(ix, iy, 1, false, false);
    v.drag(ix + 100.0f, iy + 200.0f);
    ev = v.release(ix + 100.0f, iy + 200.0f);
    check(ev.kind == GraphEventKind::changed && v.graph().links().empty(), "dragging off a connected input detaches it");
    check(v.undo() && v.graph().links().size() == 1, "undo brings the link back");
    check(v.redo() && v.graph().links().empty(), "redo takes it away again");
    v.undo();

    v.press(320.0f, 10.0f, 1, false, false);
    v.drag(330.0f, 20.0f);
    v.drag(340.0f, 30.0f);
    ev = v.release(340.0f, 30.0f);
    check(ev.kind == GraphEventKind::changed && v.graph().node(log)->x == 320.0f && v.graph().node(log)->y == 20.0f, "dragging a header moves the node");

    v.press(790.0f, 590.0f, 1, false, false);
    v.drag(400.0f, 300.0f);
    v.drag(1.0f, 1.0f);
    v.release(1.0f, 1.0f);
    check(v.selection().size() == 2, "box select picks both nodes");
    check(v.key("d", true) && v.graph().nodes().size() == 4 && v.graph().links().size() == 2 && v.selection().size() == 2, "ctrl+d duplicates nodes and the links between them");
    check(v.key("Delete") && v.graph().nodes().size() == 2 && v.graph().links().size() == 1, "delete removes the selected copies");

    float tx = 0.0f, ty = 0.0f;
    v.pin_position(log, "then", true, tx, ty);
    v.press(tx, ty, 1, false, false);
    v.drag(tx + 120.0f, ty + 150.0f);
    ev = v.release(tx + 120.0f, ty + 150.0f);
    check(ev.kind == GraphEventKind::menu && v.has_pending_link(), "dropping a link on empty space asks for a node");
    const int next = v.add_node("log", tx + 120.0f, ty + 150.0f);
    check(next != 0 && v.graph().links().size() == 2 && !v.has_pending_link(), "the new node gets the dropped link");

    const int again = v.add_node_centered("on_start");
    const orchard::editor::GraphCompile c = v.compile();
    check(!c.errors.empty() && c.error_nodes.count(again) == 1, "compile errors point at their nodes");

    float wx = 0.0f, wy = 0.0f, sx = 0.0f, sy = 0.0f;
    v.to_graph(400.0f, 300.0f, wx, wy);
    check(v.wheel(400.0f, 300.0f, 2.0f) && v.camera().zoom > 1.0f, "wheel zooms in");
    v.to_canvas(wx, wy, sx, sy);
    check(near(sx, 400.0f) && near(sy, 300.0f), "zoom keeps the point under the cursor");

    const std::vector<orchard::editor::GraphCategory> cats = orchard::editor::GraphView::catalog();
    std::size_t total = 0;
    for (const auto& cat : cats) total += cat.types.size();
    check(!cats.empty() && cats.front().name == "Events" && total == orchard::graph::node_types(orchard::graph::GraphKind::script).size(), "catalog groups every script node type");

    const std::string saved = v.save_text();
    orchard::editor::GraphView w;
    std::string why;
    check(w.load_text(saved, &why) && w.graph().nodes().size() == v.graph().nodes().size(), "graph view text round trips: " + why);
}

} // namespace

void test_humanoids_cameras_and_reflections() {
    for (orchard::HumanoidKind k : orchard::humanoid_kinds()) {
        check(orchard::parse_humanoid_kind(orchard::to_string(k)) == k, std::string("humanoid kind ") + orchard::to_string(k));
        orchard::Scene s(orchard::Dimension::three_d);
        const orchard::Entity body = orchard::add_humanoid(s, k);
        check(s.alive(body) && s.body(body) != nullptr, std::string("humanoid has a body: ") + orchard::to_string(k));
        std::size_t colliders = 0;
        for (orchard::Entity e : s.subtree(body))
            if (const std::vector<orchard::Collider>* cs = s.colliders(e)) colliders += cs->size();
        const std::size_t want = k == orchard::HumanoidKind::basic ? 1u : k == orchard::HumanoidKind::head_and_body ? 2u : k == orchard::HumanoidKind::full_body ? 10u : 30u;
        check(colliders >= want, std::string("humanoid collider count: ") + orchard::to_string(k));
        const auto back = orchard::Scene::load(s.save());
        check(back && back->size() == s.size(), std::string("humanoid saves: ") + orchard::to_string(k));
    }

    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity cam = s.create("Player camera");
    orchard::Camera& c = s.add_camera(cam);
    c.active = true;
    c.control = orchard::CameraControl::walk;
    c.move_speed = 7.5f;
    c.eye_height = 1.6f;
    c.jump_speed = 7.0f;
    c.mouse_look = false;
    const orchard::Entity pool = s.create("Pool");
    orchard::Fluid& f = s.add_fluid(pool);
    f.reflectivity = 0.6f;
    f.reflections = orchard::ReflectionMode::realistic;
    orchard::Emitter& steam = s.add_emitter(pool);
    steam.shape = orchard::EmitterShape::fluid_surface;
    const orchard::Entity mirror = s.create("Mirror");
    s.add_material(mirror).reflections = orchard::ReflectionMode::surroundings;
    const orchard::Entity torch = s.create("Torch");
    orchard::Emitter& fire = s.add_emitter(torch);
    fire.shape = orchard::EmitterShape::parts;
    fire.part = "head, hand";
    const auto back = orchard::Scene::load(s.save());
    check(back.has_value(), "camera/reflection scene loads");
    if (!back) return;
    const orchard::Camera* bc = back->camera(back->find("Player camera"));
    check(bc && bc->active && bc->control == orchard::CameraControl::walk && near(bc->move_speed, 7.5f) && near(bc->eye_height, 1.6f) && near(bc->jump_speed, 7.0f) && !bc->mouse_look, "player camera round trip");
    check(back->active_camera() == back->find("Player camera"), "player camera is the one Play uses");
    const orchard::Fluid* bf = back->fluid(back->find("Pool"));
    check(bf && near(bf->reflectivity, 0.6f) && bf->reflections == orchard::ReflectionMode::realistic, "fluid reflections round trip");
    const orchard::Material* bm = back->material(back->find("Mirror"));
    check(bm && bm->reflections == orchard::ReflectionMode::surroundings, "material reflections round trip");
    const orchard::Emitter* bs = back->emitter(back->find("Pool"));
    check(bs && bs->shape == orchard::EmitterShape::fluid_surface, "fluid surface emitter round trip");
    const orchard::Emitter* bp = back->emitter(back->find("Torch"));
    check(bp && bp->shape == orchard::EmitterShape::parts && bp->part == "head, hand", "emitter parts round trip");
    for (orchard::ReflectionMode m : {orchard::ReflectionMode::highlights, orchard::ReflectionMode::surroundings, orchard::ReflectionMode::realistic})
        check(orchard::parse_reflection_mode(orchard::to_string(m)) == m, std::string("reflection mode ") + orchard::to_string(m));
}

#include "../Editor/particle_textures.hpp"

namespace {

void test_particle_presets() {
    for (const std::string& name : orchard::Emitter::presets()) {
        const std::optional<orchard::Emitter> e = orchard::Emitter::named(name);
        check(e.has_value(), "particle preset " + name + " exists");
        if (!e) continue;
        check(orchard::editor::is_builtin_particle_texture(e->texture), "particle preset " + name + " uses a built-in texture");
        const char* fallback = orchard::editor::builtin_particle_texture_for_preset(name);
        check(fallback && e->texture == fallback, "particle preset " + name + " texture matches its fallback");
    }
    {
        orchard::Emitter smoke = *orchard::Emitter::named("smoke");
        check(orchard::editor::particle_look_of(smoke) == "better", "the smoke preset uses the better look");
        check(orchard::editor::set_particle_look(smoke, "simple") && smoke.texture == "builtin:smoke", "the simple look goes back to the older smoke puffs");
        check(orchard::editor::particle_look_of(smoke) == "simple", "and reads back as simple");
        check(orchard::editor::set_particle_look(smoke, "plain") && smoke.texture == "builtin:glow", "the plain look uses soft dots");
        check(orchard::editor::set_particle_look(smoke, "better") && smoke.texture == "builtin:smoke-soft", "and better comes back from plain");
        orchard::Emitter boom = *orchard::Emitter::named("explosion");
        orchard::editor::set_particle_look(boom, "simple");
        check(boom.blend == orchard::ParticleBlend::additive && orchard::editor::particle_look_of(boom) == "simple", "the simple explosion adds light like it used to");
        orchard::Emitter fire = *orchard::Emitter::named("fire");
        check(orchard::editor::particle_look_options(orchard::editor::particle_look_for(fire.texture, fire.preset)).size() == 3, "fire offers plain, simple and better looks");
        orchard::Emitter mark = *orchard::Emitter::named("scorch");
        check(mark.flat && orchard::editor::is_builtin_particle_texture(mark.texture), "the scorch preset lies flat");
        orchard::Scene sc(orchard::Dimension::three_d);
        const orchard::Entity me = sc.create("Mark");
        sc.add_emitter(me) = mark;
        std::string error;
        const std::optional<orchard::Scene> back = orchard::Scene::load(sc.save(), &error);
        check(back && back->emitter(back->find("Mark")) && back->emitter(back->find("Mark"))->flat, "a flat emitter saves and loads");
    }
    for (const orchard::editor::BuiltinParticleTexture& t : orchard::editor::builtin_particle_textures())
        check(t.columns >= 1 && t.rows >= 1 && std::string(t.name).rfind("builtin:", 0) == 0 && t.label && *t.label, std::string("built-in particle texture ") + t.name);
    float means[5][3] = {};
    int k = 0;
    for (const orchard::editor::BuiltinTerrainTexture& t : orchard::editor::builtin_terrain_textures()) {
        const orchard::editor::TerrainPixels px = orchard::editor::generate_builtin_terrain(t.name, 64);
        const bool sized = px.albedo.width == 64 && px.albedo.height == 64 && px.albedo.rgba.size() == 64u * 64u * 4u && px.normal.rgba.size() == px.albedo.rgba.size();
        check(sized, std::string("built-in terrain texture ") + t.name + " is generated");
        if (!sized || k >= 5) continue;
        int seam = 0, inner = 0;
        for (int y = 0; y < 64; ++y) {
            for (int c = 0; c < 3; ++c) {
                seam += std::abs(int(px.albedo.rgba[(y * 64 + 63) * 4 + c]) - int(px.albedo.rgba[(y * 64) * 4 + c]));
                inner += std::abs(int(px.albedo.rgba[(y * 64 + 31) * 4 + c]) - int(px.albedo.rgba[(y * 64 + 32) * 4 + c]));
            }
        }
        check(seam <= inner * 2 + 64 * 3 * 4, std::string("built-in terrain texture ") + t.name + " tiles without a seam");
        for (std::size_t i = 0; i < px.albedo.rgba.size(); i += 4)
            for (int c = 0; c < 3; ++c) means[k][c] += px.albedo.rgba[i + static_cast<std::size_t>(c)] / (64.0f * 64.0f);
        ++k;
    }
    auto apart = [&](int i, int j) { return std::fabs(means[i][0] - means[j][0]) + std::fabs(means[i][1] - means[j][1]) + std::fabs(means[i][2] - means[j][2]) > 40.0f; };
    check(k == 5 && apart(0, 1) && apart(0, 2) && apart(1, 2), "grass, dirt and rock look clearly different");
    check(orchard::editor::is_builtin_texture("builtin:grass") && orchard::editor::is_builtin_texture("builtin:fire") && !orchard::editor::is_builtin_texture("Assets/grass.png"), "built-in texture names");
}

} // namespace

void test_models_and_animation() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity guy = s.create("Guy");
    orchard::Model& m = s.add_model(guy);
    m.source = "Assets/My Models/guy.glb";
    m.cast_shadows = false;
    orchard::Animator& a = s.add_animator(guy);
    a.clip = "Walk";
    a.speed = 1.5f;
    a.loop = false;
    a.blend = 0.4f;
    a.start = 0.25f;
    a.events.push_back({"Walk", 0.3f, "footstep"});
    a.events.push_back({"Walk", 0.8f, "footstep"});
    const orchard::Entity head = s.create("Head", guy);
    s.add_collider(head, orchard::ColliderShape::sphere);
    s.set_bone(head, "mixamorig:Head");
    const orchard::Entity house = s.create("House");
    orchard::Collider* c = s.add_collider(house, orchard::ColliderShape::mesh);
    c->source = "Assets/house.glb";
    check(orchard::is_model_file("Assets/a.GLB") && orchard::is_model_file("x.gltf") && orchard::is_model_file("y.obj") && !orchard::is_model_file("z.png"), "model file types");
    check(s.bone_owner(head) == guy, "bone owner is the model above it");
    const auto back = orchard::Scene::load(s.save());
    check(back.has_value(), "model scene loads");
    if (!back) return;
    const orchard::Model* bm = back->model(back->find("Guy"));
    check(bm && bm->source == "Assets/My Models/guy.glb" && !bm->cast_shadows && bm->visible, "model round trip (with a space in the path)");
    const orchard::Animator* ba = back->animator(back->find("Guy"));
    check(ba && ba->clip == "Walk" && near(ba->speed, 1.5f) && !ba->loop && near(ba->blend, 0.4f) && near(ba->start, 0.25f) && ba->events.size() == 2 && ba->events[1].name == "footstep" && near(ba->events[1].time, 0.8f), "animator round trip");
    check(back->bone(back->find("Head")) == "mixamorig:Head", "bone attachment round trip");
    const std::vector<orchard::Collider>* hc = back->colliders(back->find("House"));
    check(hc && hc->size() == 1 && (*hc)[0].shape == orchard::ColliderShape::mesh && (*hc)[0].source == "Assets/house.glb", "mesh collider round trip");
    orchard::Scene tmpl(orchard::Dimension::three_d);
    const orchard::Entity tg = tmpl.create("Guy");
    tmpl.add_model(tg).source = "Assets/guy.glb";
    tmpl.add_animator(tg).clip = "Idle";
    const orchard::Entity th = tmpl.create("Head", tg);
    tmpl.add_collider(th, orchard::ColliderShape::sphere);
    tmpl.set_bone(th, "mixamorig:Head");
    orchard::Scene copy(orchard::Dimension::three_d);
    const orchard::Entity inst = copy.instantiate(tmpl, "Entities/Guy.entity", orchard::Entity{}, orchard::Transform{});
    check(copy.model(inst) && copy.animator(inst) && !copy.children(inst).empty() && copy.bone(copy.children(inst)[0]) == "mixamorig:Head", "templates copy models, animators and bones");
}

#ifdef ORCHARD_TEST_XYLEM
#include <xylem/xylem.hpp>
#endif // ORCHARD_TEST_XYLEM

void test_terrain() {
    const fs::path dir = fs::temp_directory_path() / "orchard terrain test";
    fs::remove_all(dir);
    fs::create_directories(dir / "Assets");

    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity land = s.create("Big Land");
    orchard::Terrain& t = s.add_terrain(land);
    t.size = {320.0f, 42.5f, 280.0f};
    t.resolution = 129;
    t.heightmap = "Assets/my height map.png";
    t.data = "Assets/Terrain/big land.heights";
    t.generator = "mountains";
    t.seed = 4242;
    t.roughness = 0.71f;
    t.friction = 0.83f;
    t.restitution = 0.12f;
    orchard::TerrainLayer grass;
    grass.texture = "Assets/grass 1.png";
    grass.normal_map = "Assets/grass normal%.png";
    grass.color = {0.1f, 0.6f, 0.2f};
    grass.tiling = 4.5f;
    grass.min_height = 0.05f;
    grass.max_height = 0.7f;
    grass.min_slope = 3.0f;
    grass.max_slope = 33.0f;
    grass.blend = 0.2f;
    orchard::TerrainLayer rock;
    rock.color = {0.5f, 0.5f, 0.52f};
    rock.min_slope = 40.0f;
    t.layers = {grass, rock};
    orchard::TerrainScatter trees;
    trees.model = "Assets/Models/pine tree.glb";
    trees.density = 0.031f;
    trees.min_scale = 0.7f;
    trees.max_scale = 1.6f;
    trees.min_height = 0.1f;
    trees.max_height = 0.8f;
    trees.max_slope = 27.0f;
    trees.align_to_slope = true;
    trees.collide = false;
    trees.draw_distance = 512.0f;
    trees.seed = 99;
    t.scatter = {trees, orchard::TerrainScatter{}};
    t.scatter[1].model = "Assets/rock.obj";

    std::string error;
    const std::optional<orchard::Scene> back = orchard::Scene::load(s.save(), &error);
    check(back.has_value(), "terrain scene loads: " + error);
    const orchard::Terrain* bt = back ? back->terrain(back->find("Big Land")) : nullptr;
    check(bt != nullptr, "terrain survives a save");
    if (bt) {
        check(near(bt->size.x, 320.0f) && near(bt->size.y, 42.5f) && near(bt->size.z, 280.0f) && bt->resolution == 129, "terrain size and resolution round trip");
        check(bt->heightmap == t.heightmap && bt->data == t.data && bt->generator == "mountains" && bt->seed == 4242u, "terrain paths with spaces round trip");
        check(near(bt->roughness, 0.71f) && near(bt->friction, 0.83f) && near(bt->restitution, 0.12f), "terrain roughness, friction and bounce round trip");
        check(bt->layers.size() == 2 && bt->scatter.size() == 2, "terrain layers and scatter round trip");
        if (bt->layers.size() == 2) {
            const orchard::TerrainLayer& l = bt->layers[0];
            check(l.texture == grass.texture && l.normal_map == grass.normal_map && near(l.color.y, 0.6f) && near(l.tiling, 4.5f), "terrain layer textures round trip");
            check(near(l.min_height, 0.05f) && near(l.max_height, 0.7f) && near(l.min_slope, 3.0f) && near(l.max_slope, 33.0f) && near(l.blend, 0.2f), "terrain layer limits round trip");
            check(near(bt->layers[1].min_slope, 40.0f) && bt->layers[1].texture.empty(), "second terrain layer round trip");
        }
        if (bt->scatter.size() == 2) {
            const orchard::TerrainScatter& c = bt->scatter[0];
            check(c.model == trees.model && near(c.density, 0.031f) && near(c.min_scale, 0.7f) && near(c.max_scale, 1.6f), "terrain scatter model and scale round trip");
            check(near(c.min_height, 0.1f) && near(c.max_height, 0.8f) && near(c.max_slope, 27.0f) && c.align_to_slope && !c.collide && near(c.draw_distance, 512.0f) && c.seed == 99u, "terrain scatter limits round trip");
            check(bt->scatter[1].model == "Assets/rock.obj" && bt->scatter[1].collide, "second terrain scatter round trip");
        }
        check(back->save() == s.save(), "terrain save is stable");
    }

    for (const std::string& g : orchard::terrain_generators()) {
        orchard::Terrain gt;
        gt.generator = g;
        gt.resolution = 65;
        gt.seed = 7;
        orchard::TerrainHeights a, b, c;
        orchard::generate_heights(gt, a);
        orchard::generate_heights(gt, b);
        gt.seed = 8;
        orchard::generate_heights(gt, c);
        check(a.valid() && a.width == 65 && a.heights == b.heights, "generator " + g + " is deterministic");
        check(a.min_height() >= -1e-4f && a.max_height() <= gt.size.y + 1e-3f, "generator " + g + " stays inside the terrain height");
        if (g != "flat") check(a.heights != c.heights && a.max_height() - a.min_height() > gt.size.y * 0.3f, "generator " + g + " uses the seed and isn't flat");
    }

    orchard::TerrainHeights ramp;
    ramp.size = {20.0f, 10.0f, 20.0f};
    ramp.resize(21, 21);
    for (int z = 0; z < 21; ++z)
        for (int x = 0; x < 21; ++x) ramp.set(x, z, 5.0f + 0.5f * ramp.local_position(x, z).x);
    check(near(ramp.height_at(3.3f, -2.0f), 5.0f + 1.65f) && near(ramp.height_at(-7.25f, 6.6f), 5.0f - 3.625f), "terrain heights sample between grid points");
    const orchard::Vec3 rn = ramp.normal_at(1.0f, 1.0f);
    check(near(rn.x, -0.4472f) && near(rn.y, 0.8944f) && near(rn.z, 0.0f), "terrain normal on a ramp");
    check(near(ramp.slope_at(0.0f, 0.0f), 26.565f, 0.05f), "terrain slope on a ramp");
    float hit_d = 0.0f;
    orchard::Vec3 hit_p;
    check(orchard::raycast_terrain(ramp, {2.0f, 50.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, 100.0f, hit_d, &hit_p) && near(hit_p.y, 6.0f, 0.01f), "terrain raycast hits the surface");

    orchard::TerrainHeights flat;
    flat.size = {40.0f, 10.0f, 40.0f};
    flat.resize(41, 41, 2.0f);
    orchard::TerrainBrush brush;
    brush.radius = 5.0f;
    brush.strength = 1.0f;
    brush.falloff = 0.5f;
    const std::uint64_t rev = flat.revision;
    check(orchard::apply_brush(flat, brush, 0.0f, 0.0f) && near(flat.height_at(0.0f, 0.0f), 3.0f) && near(flat.height_at(10.0f, 10.0f), 2.0f) && flat.revision > rev && !flat.dirty.empty(), "raise brush lifts the middle only");
    check(flat.height_at(4.0f, 0.0f) > 2.0f && flat.height_at(4.0f, 0.0f) < 3.0f, "brush falloff softens the edge");
    brush.kind = orchard::TerrainBrushKind::lower;
    brush.strength = 3.0f;
    orchard::apply_brush(flat, brush, 0.0f, 0.0f);
    check(near(flat.height_at(0.0f, 0.0f), 0.0f), "lower brush digs");
    brush.kind = orchard::TerrainBrushKind::set_height;
    brush.target = 7.0f;
    orchard::apply_brush(flat, brush, 10.0f, 10.0f);
    check(near(flat.height_at(10.0f, 10.0f), 7.0f), "set-height brush sets the height");
    brush.kind = orchard::TerrainBrushKind::flatten;
    brush.target = 2.0f;
    brush.strength = 1.0f;
    orchard::apply_brush(flat, brush, 10.0f, 10.0f);
    check(near(flat.height_at(10.0f, 10.0f), 2.0f), "flatten brush pulls to the target");
    orchard::TerrainHeights spike = flat;
    spike.resize(41, 41, 0.0f);
    spike.set(20, 20, 9.0f);
    brush.kind = orchard::TerrainBrushKind::smooth;
    brush.strength = 1.0f;
    orchard::apply_brush(spike, brush, 0.0f, 0.0f);
    check(spike.at(20, 20) < 2.0f && spike.at(21, 20) > 0.5f, "smooth brush spreads a spike");
    orchard::TerrainHeights bumpy = flat;
    bumpy.resize(41, 41, 1.0f);
    brush.kind = orchard::TerrainBrushKind::noise;
    brush.radius = 15.0f;
    orchard::apply_brush(bumpy, brush, 0.0f, 0.0f);
    check(bumpy.max_height() - bumpy.min_height() > 0.1f, "noise brush roughens");
    check(orchard::parse_terrain_brush("set-height") == orchard::TerrainBrushKind::set_height && orchard::parse_terrain_brush("smooth") == orchard::TerrainBrushKind::smooth, "brush names parse");

    const std::string heights_file = (dir / "Assets" / "test land.heights").string();
    check(flat.save(heights_file, &error), "heights file saves: " + error);
    orchard::TerrainHeights loaded;
    check(loaded.load(heights_file, &error) && loaded.width == 41 && loaded.heights == flat.heights && near(loaded.size.x, 40.0f), "heights file loads back exactly");

    {
        std::ofstream pgm(dir / "Assets" / "hm.pgm", std::ios::binary);
        pgm << "P5\n3 3\n65535\n";
        const unsigned v[9] = {0, 32768, 65535, 65535, 0, 16384, 1, 2, 3};
        for (unsigned x : v) pgm.put(static_cast<char>(x >> 8)).put(static_cast<char>(x & 255));
    }
    orchard::Terrain ht;
    ht.size = {10.0f, 100.0f, 10.0f};
    ht.resolution = 3;
    ht.heightmap = "Assets/hm.pgm";
    orchard::TerrainHeights from_image;
    check(orchard::build_terrain_heights(ht, dir.string(), from_image, &error) && from_image.width == 3, "16-bit heightmap loads: " + error);
    check(near(from_image.at(1, 0), 50.0f, 0.01f) && near(from_image.at(2, 0), 100.0f) && near(from_image.at(2, 1), 25.0f, 0.01f), "16-bit heightmap keeps its precision");
    ht.data = "Assets/test land.heights";
    ht.resolution = 41;
    ht.size = {40.0f, 10.0f, 40.0f};
    orchard::TerrainHeights from_data;
    check(orchard::build_terrain_heights(ht, dir.string(), from_data) && near(from_data.height_at(10.0f, 10.0f), 2.0f), "sculpted data wins over the heightmap");

    orchard::Terrain forest = orchard::terrain_preset("forest");
    forest.resolution = 257;
    forest.size = {300.0f, 30.0f, 300.0f};
    orchard::TerrainHeights fh;
    orchard::generate_heights(forest, fh);
    const std::vector<orchard::ScatterInstance> a = orchard::scatter_instances(forest, fh);
    const std::vector<orchard::ScatterInstance> b = orchard::scatter_instances(forest, fh);
    bool same = a.size() == b.size() && !a.empty();
    bool inside = true;
    for (std::size_t i = 0; same && i < a.size(); ++i) same = a[i].model == b[i].model && a[i].position.x == b[i].position.x && a[i].position.z == b[i].position.z && a[i].yaw == b[i].yaw && a[i].scale == b[i].scale;
    for (const orchard::ScatterInstance& in : a) {
        const orchard::TerrainScatter& sc = forest.scatter[in.scatter];
        inside = inside && near(in.position.y, fh.height_at(in.position.x, in.position.z)) && in.scale >= sc.min_scale - 1e-4f && in.scale <= sc.max_scale + 1e-4f && fh.slope_at(in.position.x, in.position.z) <= sc.max_slope + 1e-3f;
    }
    check(same, "scatter is the same every time (" + std::to_string(a.size()) + " instances)");
    check(inside, "scatter sits on the ground inside its limits");
    forest.seed = 2;
    const std::vector<orchard::ScatterInstance> c = orchard::scatter_instances(forest, fh);
    check(!c.empty() && (c.size() != a.size() || c[0].position.x != a[0].position.x), "scatter follows the terrain seed");

    {
        orchard::Terrain st = orchard::terrain_preset("mountains");
        st.resolution = 65;
        st.size = {200.0f, 60.0f, 100.0f};
        check(!st.layers.empty() && st.layers[0].texture == "builtin:grass" && st.layers[1].texture == "builtin:rock", "terrain presets use built-in textures");
        orchard::TerrainHeights sh;
        orchard::generate_heights(st, sh);
        orchard::TerrainSplat sp;
        orchard::build_terrain_splat(st, sh, sp);
        check(sp.width == orchard::terrain_splat_size(sh.width) && sp.layers == 3 && sp.rgba[0].size() == static_cast<std::size_t>(sp.width * sp.height * 4) && sp.rgba[1].empty(), "splat map size and layers");
        bool sums = true, rocky = false;
        for (std::size_t i = 0; i < sp.rgba[0].size(); i += 4) {
            const int t = sp.rgba[0][i] + sp.rgba[0][i + 1] + sp.rgba[0][i + 2] + sp.rgba[0][i + 3];
            sums = sums && t >= 250 && t <= 260;
            rocky = rocky || sp.rgba[0][i + 1] > 128;
        }
        check(sums && rocky, "splat weights add up to one and include steep rock");
        float w8[orchard::terrain_max_splat_layers];
        orchard::terrain_layer_weights(st, 0.5f, 60.0f, w8, orchard::terrain_max_splat_layers);
        const orchard::Vec4 w4 = orchard::terrain_layer_weights(st, 0.5f, 60.0f);
        check(near(w8[0], w4.x) && near(w8[1], w4.y) && near(w8[2], w4.z) && near(w8[4], 0.0f), "layer weights for many layers match the four-layer version");
        const orchard::TerrainSplat before = sp;
        const int cx = sh.width / 2, cz = sh.depth / 2;
        for (int z = cz - 3; z <= cz + 3; ++z)
            for (int x = cx - 3; x <= cx + 3; ++x) sh.set(x, z, sh.at(x, z) + ((x + z) % 2 ? 8.0f : -8.0f));
        int px0 = 0, py0 = 0, px1 = 0, py1 = 0;
        check(orchard::update_terrain_splat(st, sh, sp, cx - 3, cz - 3, cx + 3, cz + 3, &px0, &py0, &px1, &py1), "splat region updates");
        bool outside_same = true, inside_changed = false;
        for (int y = 0; y < sp.height; ++y)
            for (int x = 0; x < sp.width; ++x) {
                const std::size_t i = (static_cast<std::size_t>(y) * sp.width + x) * 4;
                const bool in = x >= px0 && x <= px1 && y >= py0 && y <= py1;
                const bool diff = before.rgba[0][i + 1] != sp.rgba[0][i + 1] || before.rgba[0][i] != sp.rgba[0][i];
                if (!in && diff) outside_same = false;
                if (in && diff) inside_changed = true;
            }
        orchard::TerrainSplat full;
        orchard::build_terrain_splat(st, sh, full);
        check(outside_same && inside_changed && full.rgba[0] == sp.rgba[0], "sculpting only rebuilds the touched part of the splat map");
        st.layers.resize(6, st.layers[0]);
        orchard::build_terrain_splat(st, sh, sp);
        check(sp.layers == 6 && sp.rgba[1].size() == sp.rgba[0].size(), "more than four layers use a second splat map");
    }

    const orchard::TerrainChunking chunks = orchard::terrain_chunking(fh, 32);
    check(chunks.chunks_x == 8 && chunks.chunks_z == 8 && chunks.lods == 6, "terrain splits into 32x32 chunks with LODs");
    orchard::TerrainMesh near_mesh, far_mesh;
    orchard::build_terrain_chunk(forest, fh, 2, 3, 0, near_mesh);
    orchard::build_terrain_chunk(forest, fh, 2, 3, 3, far_mesh);
    check(near_mesh.positions.size() == 33 * 33 + 4 * 33 && near_mesh.triangle_count() == 32 * 32 * 2 + 4 * 32 * 4, "LOD 0 chunk mesh has every sample and skirts");
    check(far_mesh.triangle_count() < near_mesh.triangle_count() / 16 && far_mesh.normals.size() == far_mesh.positions.size() && far_mesh.weights.size() == far_mesh.positions.size(), "far LOD is much lighter");
    const orchard::Vec4 w = orchard::terrain_layer_weights(forest, 0.4f, 60.0f);
    check(near(w.x + w.y + w.z + w.w, 1.0f) && w.z > 0.9f, "steep ground gets the rock layer");

#ifdef ORCHARD_TEST_XYLEM
    {
        orchard::Scene ps(orchard::Dimension::three_d);
        const orchard::Entity hill = ps.create("Hill");
        orchard::Terrain& pt = ps.add_terrain(hill);
        pt.size = {60.0f, 12.0f, 60.0f};
        pt.resolution = 65;
        pt.generator = "hills";
        pt.seed = 3;
        pt.friction = 0.9f;
        ps.transform(hill)->position = {10.0f, -3.0f, 5.0f};
        ps.transform(hill)->rotation = orchard::from_euler({0.0f, 30.0f, 0.0f});
        ps.transform(hill)->scale = {1.5f, 1.0f, 1.5f};
        const orchard::Entity ball = ps.create("Ball");
        ps.transform(ball)->position = {14.0f, 30.0f, 2.0f};
        orchard::Body& bb = ps.add_body(ball);
        bb.angular_damping = 2.0f;
        bb.linear_damping = 0.3f;
        ps.add_collider(ball, orchard::ColliderShape::sphere)->friction = 0.9f;
        xylem::World world(orchard::Dimension::three_d);
        world.build(ps);
        check(world.terrain_count() == 1, "play mode turns the terrain into a heightfield");
        for (int i = 0; i < 60 * 12; ++i) world.step(1.0f / 60.0f);
        orchard::TerrainHeights ph;
        orchard::generate_heights(*ps.terrain(hill), ph);
        const orchard::Mat4 tm = orchard::terrain_matrix(ps.world_matrix(hill), {0.0f, 1.0f, 0.0f});
        const orchard::Vec3 at = ps.transform(ball)->position;
        const orchard::Vec3 local = orchard::terrain_to_local(tm, at);
        const orchard::Vec3 ground = orchard::mul_point(tm, {local.x, ph.height_at(local.x, local.z), local.z});
        const float speed = orchard::length(world.velocity(ball));
        check(at.y > ground.y && at.y - ground.y < 0.75f && at.y > -2.0f, "ball comes to rest on the hill (ball " + std::to_string(at.y) + ", ground " + std::to_string(ground.y) + ")");
        check(speed < 0.5f, "ball stops on the hill (speed " + std::to_string(speed) + ")");
        orchard::Vec3 rp, rnorm;
        float rd = 0.0f;
        const orchard::Entity rh = world.raycast({12.0f, 40.0f, 8.0f}, {0.0f, -1.0f, 0.0f}, 100.0f, &rp, &rnorm, &rd);
        const orchard::Vec3 rl = orchard::terrain_to_local(tm, rp);
        check(rh == hill && near(rl.y, ph.height_at(rl.x, rl.z), 0.05f) && rnorm.y > 0.5f, "raycasts hit the terrain");
    }
    {
        {
            std::ofstream obj(dir / "Assets" / "floor plate.obj");
            obj << "v -10 0 -10\nv 10 0 -10\nv 10 0 10\nv -10 0 10\nv 0 3 0\nf 1 4 3\nf 1 3 2\nf 1 2 5\n";
        }
        orchard::Scene ms(orchard::Dimension::three_d);
        const orchard::Entity map = ms.create("Map");
        ms.add_body(map).type = orchard::BodyType::fixed;
        orchard::Collider* mc = ms.add_collider(map, orchard::ColliderShape::mesh);
        mc->source = "Assets/floor plate.obj";
        mc->size = {2.0f, 1.0f, 2.0f};
        const orchard::Entity ball = ms.create("Ball");
        ms.transform(ball)->position = {15.0f, 4.0f, 15.0f};
        ms.add_body(ball);
        ms.add_collider(ball, orchard::ColliderShape::sphere);
        const orchard::Entity rock = ms.create("Rock");
        ms.transform(rock)->position = {-12.0f, 3.0f, 12.0f};
        ms.add_body(rock);
        orchard::Collider* rc = ms.add_collider(rock, orchard::ColliderShape::convex_hull);
        rc->source = "Assets/floor plate.obj";
        rc->size = {0.1f, 0.5f, 0.1f};
        xylem::World world(orchard::Dimension::three_d);
        world.set_asset_root(dir.string());
        world.build(ms);
        for (int i = 0; i < 60 * 3; ++i) world.step(1.0f / 60.0f);
        if (xylem::models_supported()) {
            check(world.warnings().empty(), "model colliders load" + (world.warnings().empty() ? std::string() : ": " + world.warnings()[0]));
            check(near(ms.transform(ball)->position.y, 0.5f, 0.08f), "ball lands on an imported mesh scaled by the collider size (y = " + std::to_string(ms.transform(ball)->position.y) + ")");
            check(ms.transform(rock)->position.y > -0.2f && ms.transform(rock)->position.y < 1.0f, "a convex hull body lands on the mesh (y = " + std::to_string(ms.transform(rock)->position.y) + ")");
        }
    }
#endif // ORCHARD_TEST_XYLEM
    fs::remove_all(dir);
}

void test_triangles() {
    orchard::Collider c;
    c.shape = orchard::ColliderShape::triangle;
    orchard::Vec3 sides, angles;
    c.triangle = orchard::TriangleRule::sss;
    c.triangle_sides = {3.0f, 4.0f, 5.0f};
    check(orchard::solve_triangle(c, sides, angles) && near(angles.z, 90.0f, 0.05f), "SSS 3, 4, 5 has a right angle across from the 5");
    c.triangle_sides = {1.0f, 1.0f, 3.0f};
    std::string why;
    check(!orchard::solve_triangle(c, sides, angles, &why) && !why.empty(), "SSS that breaks the triangle inequality is refused with a reason");
    c.triangle = orchard::TriangleRule::sas;
    c.triangle_sides = {0.0f, 4.0f, 3.0f};
    c.triangle_angles.x = 90.0f;
    check(orchard::solve_triangle(c, sides, angles) && near(sides.x, 5.0f, 1e-3f), "SAS with a right angle between 3 and 4 gives 5");
    c.triangle = orchard::TriangleRule::asa;
    c.triangle_sides.x = 2.0f;
    c.triangle_angles.y = 60.0f;
    c.triangle_angles.z = 60.0f;
    check(orchard::solve_triangle(c, sides, angles) && near(sides.y, 2.0f, 1e-3f) && near(sides.z, 2.0f, 1e-3f), "ASA 60, 2, 60 is equilateral");
    c.triangle = orchard::TriangleRule::aas;
    c.triangle_sides.x = 1.0f;
    c.triangle_angles.x = 30.0f;
    c.triangle_angles.y = 90.0f;
    check(orchard::solve_triangle(c, sides, angles) && near(sides.y, 2.0f, 1e-3f), "AAS 30, 90 with a = 1 gives a hypotenuse of 2");
    c.triangle = orchard::TriangleRule::ssa;
    c.triangle_sides = {1.2f, 1.5f, 0.0f};
    c.triangle_angles.x = 40.0f;
    orchard::Vec3 narrow, wide;
    const bool n_ok = orchard::solve_triangle(c, narrow, angles);
    c.triangle_wide = true;
    const bool w_ok = orchard::solve_triangle(c, wide, angles);
    check(n_ok && w_ok && std::fabs(narrow.z - wide.z) > 0.1f, "SSA with two answers gives both");
    c.triangle_sides = {0.5f, 1.5f, 0.0f};
    check(!orchard::solve_triangle(c, sides, angles), "SSA where side a can't reach is refused");
    c.triangle = orchard::TriangleRule::right;
    c.triangle_sides = {0.0f, 3.0f, 4.0f};
    check(orchard::solve_triangle(c, sides, angles) && near(sides.x, 5.0f, 1e-3f) && near(angles.x, 90.0f, 0.05f), "a right triangle from legs 3 and 4");
    check(orchard::fit_triangle_size(c) && near(c.size.x * c.size.y * 0.5f, 6.0f, 1e-2f), "fitting the size keeps the real area (6 m\u00B2)");
    c.size.z = 2.0f;
    check(near(orchard::collider_volume(c), 12.0f, 0.05f), "a triangle prism's volume is area times depth");
    const std::vector<orchard::Vec3> corners = orchard::triangle_corners(c);
    float best = 0.0f;
    for (int i = 0; i < 3; ++i) best = std::max(best, orchard::length(corners[static_cast<std::size_t>(i)] - corners[static_cast<std::size_t>((i + 1) % 3)]));
    check(near(best, 5.0f, 1e-3f), "the drawn corners have the real side lengths");
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity e = s.create("Ramp");
    s.add_body(e);
    *s.add_collider(e, orchard::ColliderShape::triangle) = c;
    (*s.colliders(e))[0].triangle = orchard::TriangleRule::ssa;
    (*s.colliders(e))[0].triangle_wide = true;
    std::string err;
    const auto back = orchard::Scene::load(s.save(), &err);
    const std::vector<orchard::Collider>* bc = back ? back->colliders(back->find("Ramp")) : nullptr;
    check(bc && bc->size() == 1 && (*bc)[0].shape == orchard::ColliderShape::triangle && (*bc)[0].triangle == orchard::TriangleRule::ssa && (*bc)[0].triangle_wide && near((*bc)[0].triangle_sides.z, 4.0f), "triangle colliders round trip " + err);
    for (orchard::TriangleRule r : orchard::triangle_rules()) check(orchard::parse_triangle_rule(orchard::to_string(r)) == r, std::string("triangle rule round trip ") + orchard::to_string(r));
}

void test_character_looks() {
    for (orchard::Dimension dim : {orchard::Dimension::three_d, orchard::Dimension::two_d}) {
        orchard::Scene s(dim);
        const orchard::Entity e = orchard::add_humanoid(s, orchard::HumanoidKind::full_body, {}, "Guy");
        const orchard::Entity cam = s.create("Camera", e);
        s.add_camera(cam);
        check(orchard::humanoid_kind_of(s, e) == orchard::HumanoidKind::full_body, "a full body character is recognized");
        for (orchard::HumanoidKind hk : orchard::humanoid_kinds()) {
            check(orchard::set_humanoid_colliders(s, e, hk) && orchard::humanoid_kind_of(s, e) == hk, std::string("its colliders swap to ") + orchard::to_string(hk));
            check(s.alive(cam) && s.parent(cam) == e, std::string("swapping colliders keeps its camera (") + orchard::to_string(hk) + ")");
        }
        check(orchard::set_character_style(s, e, "robot") && orchard::character_style(s, e) == "robot" && s.model(e) && s.model(e)->source == orchard::character_model_path("robot"), "a character can look like a robot");
        check(!orchard::set_character_style(s, e, "dragon"), "an unknown look is refused");
        const auto back = orchard::Scene::load(s.save());
        check(back && orchard::character_style(*back, back->find("Guy")) == "robot", "the look is saved");
        check(orchard::set_character_style(s, e, "") && !s.model(e), "taking the look off removes the mesh");
    }
    check(orchard::character_styles().size() == 3, "there are three character looks");
}

#include "../Source/orchard/script_detail.hpp"
#include <orchard/script/orchard_api.h>
#include <set>

namespace {

void test_script_library() {
    const fs::path dir = fs::temp_directory_path() / "orchard-test-library";
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::string error;
    std::optional<orchard::Project> project = orchard::Project::create(dir.string(), "Library", orchard::Dimension::three_d, &error);
    check(project.has_value(), "library test project: " + error);
    if (!project) return;
    orchard::Scene scene(orchard::Dimension::three_d);
    const orchard::Entity speaker = scene.create("Speaker");
    orchard::SoundSource& src = scene.add_sound(speaker);
    src.clip = "Sounds/hum.ogg";
    src.loop = true;
    orchard::ScriptRuntime runtime;
    check(runtime.start(*project, scene), "a runtime with no scripts starts");
    const orchard_host_api& h = orchard::script_detail::host_table();
    check(ORCHARD_HOST_HAS(&h, game_text) && ORCHARD_HOST_HAS(&h, game_values), "the host table has text and list results");
    auto game = [&](int op, std::vector<double> a, const std::string& t, orchard_entity e = 0) { return h.game(op, e, 0, a.data(), static_cast<int>(a.size()), t.c_str()); };
    auto text = [&](int op, std::vector<double> a, const std::string& t, orchard_entity e = 0) { return std::string(h.game_text(op, e, 0, a.data(), static_cast<int>(a.size()), t.c_str())); };
    auto values = [&](int op, std::vector<double> a, const std::string& t) {
        std::vector<double> out(512);
        const int n = h.game_values(op, 0, 0, a.data(), static_cast<int>(a.size()), t.c_str(), out.data(), static_cast<int>(out.size()));
        out.resize(static_cast<std::size_t>(std::max(0, n)));
        return out;
    };
    const std::string sep(1, static_cast<char>(31));
    game(ORCHARD_GAME_RANDOM_SEED, {42}, "");
    const double r1 = game(ORCHARD_GAME_RANDOM_RANGE, {0, 100}, "");
    game(ORCHARD_GAME_RANDOM_SEED, {42}, "");
    const double r2 = game(ORCHARD_GAME_RANDOM_RANGE, {0, 100}, "");
    check(r1 == r2 && r1 >= 0.0 && r1 <= 100.0, "seeding the random numbers makes them repeat");
    const double die = game(ORCHARD_GAME_RANDOM_INTEGER, {1, 6}, "");
    check(die >= 1.0 && die <= 6.0 && std::floor(die) == die, "random whole numbers stay in range");
    const std::string pick = text(ORCHARD_GAME_RANDOM_PICK, {}, "red, green, blue");
    check(pick == "red" || pick == "green" || pick == "blue", "random pick chooses one of the choices (" + pick + ")");
    game(ORCHARD_GAME_BAG_ADD, {2}, "loot" + sep + "sword");
    game(ORCHARD_GAME_BAG_ADD, {1}, "loot" + sep + "shield");
    std::multiset<std::string> drawn;
    for (int i = 0; i < 3; ++i) drawn.insert(text(ORCHARD_GAME_BAG_NEXT, {}, "loot"));
    check(drawn.count("sword") == 2 && drawn.count("shield") == 1, "a shuffle bag hands out everything once before refilling");
    check(game(ORCHARD_GAME_BAG_REMAINING, {}, "loot") == 0.0 && game(ORCHARD_GAME_BAG_SIZE, {}, "loot") == 3.0, "and counts what's left");
    const std::vector<double> sphere = values(ORCHARD_GAME_RANDOM_IN_SPHERE, {2}, "");
    check(sphere.size() == 3 && std::sqrt(sphere[0] * sphere[0] + sphere[1] * sphere[1] + sphere[2] * sphere[2]) <= 2.0001, "random points land inside a sphere");
    const std::vector<orchard::SoundCommand> autoplay = runtime.take_sound_commands();
    check(autoplay.size() == 1 && autoplay[0].clip == "Sounds/hum.ogg" && autoplay[0].loop && autoplay[0].follow == speaker, "a sound component starts playing when Play starts");
    const double id = game(ORCHARD_GAME_SOUND_PLAY_AT, {1, 2, 3, 0.5}, "Sounds/boom.wav");
    const std::vector<orchard::SoundCommand> played = runtime.take_sound_commands();
    check(id > 0 && played.size() == 1 && played[0].positional && played[0].at.y == 2.0f && played[0].volume == 0.5f, "play_sound_at queues a sound at a point");
    check(game(ORCHARD_GAME_SOUND_PLAYING, {id}, "") == 1.0, "and says it's playing");
    runtime.sound_finished(static_cast<int>(id));
    check(game(ORCHARD_GAME_SOUND_PLAYING, {id}, "") == 0.0, "until it finishes");
    const double game_id = game(ORCHARD_GAME_CHESS_NEW, {}, "");
    check(game(ORCHARD_GAME_CHESS_MOVE, {game_id}, "e4") == 1.0 && game(ORCHARD_GAME_CHESS_MOVE, {game_id}, "e2e4") == 0.0, "chess moves are checked");
    check(text(ORCHARD_GAME_CHESS_FEN, {game_id}, "").rfind("rnbqkbnr/pppppppp/8/8/4P3", 0) == 0, "and show up in the position");
    const std::string reply = text(ORCHARD_GAME_CHESS_BEST, {game_id, 2}, "");
    check(reply.size() >= 4 && game(ORCHARD_GAME_CHESS_IS_LEGAL, {game_id}, reply) == 1.0, "the chess AI answers with a legal move (" + reply + ")");
    game(ORCHARD_GAME_GOAP_ACTION, {1}, "p" + sep + "open_door" + sep + "has_key" + sep + "door_open");
    game(ORCHARD_GAME_GOAP_ACTION, {1}, "p" + sep + "get_key" + sep + "" + sep + "has_key");
    check(text(ORCHARD_GAME_GOAP_PLAN, {}, "p" + sep + "" + sep + "door_open") == "get_key,open_door", "GOAP plans from a script");
    game(ORCHARD_GAME_VOXEL_FILL, {-5, -1, -5, 10, -1, 5}, "solid");
    const std::vector<double> path = values(ORCHARD_GAME_VOXEL_PATH, {0, 0, 0, 6, 0, 0}, "");
    check(path.size() >= 21 && path[path.size() - 3] == 6.0, "voxel paths come back as points");
    check(game(ORCHARD_GAME_NN_CREATE, {}, "xor" + sep + "2,6,1") == 1.0, "a neural net is made from a script");
    double loss = 1.0;
    for (int i = 0; i < 4000; ++i) {
        const double a = i % 2, b = (i / 2) % 2;
        loss = game(ORCHARD_GAME_NN_TRAIN, {0.5, a, b, a != b ? 1.0 : 0.0}, "xor");
    }
    const std::vector<double> yes = values(ORCHARD_GAME_NN_RUN, {1, 0}, "xor");
    const std::vector<double> no = values(ORCHARD_GAME_NN_RUN, {1, 1}, "xor");
    check(yes.size() == 1 && no.size() == 1 && yes[0] > 0.7 && no[0] < 0.3, "and learns XOR (" + std::to_string(yes.empty() ? 0.0 : yes[0]) + ", " + std::to_string(no.empty() ? 0.0 : no[0]) + ", loss " + std::to_string(loss) + ")");
    game(ORCHARD_GAME_Q_SETTINGS, {0.0}, "q");
    for (int i = 0; i < 200; ++i) {
        game(ORCHARD_GAME_Q_LEARN, {0, 0.0, 1}, "q" + sep + "start" + sep + "end");
        game(ORCHARD_GAME_Q_LEARN, {1, 1.0, 1}, "q" + sep + "start" + sep + "end");
    }
    check(game(ORCHARD_GAME_Q_BEST, {}, "q" + sep + "start") == 1.0, "Q-learning picks the rewarded action");
    game(ORCHARD_GAME_MARKOV_TRAIN, {1, 0}, "m" + sep + "the cat sat on the mat\nthe dog sat on the log");
    const std::string said = text(ORCHARD_GAME_MARKOV_GENERATE, {10, 0}, "m");
    check(said.rfind("the ", 0) == 0, "a Markov chain makes sentences (" + said + ")");
    check(text(ORCHARD_GAME_AI_BRAIN_BEHAVIOR, {}, "zombie").find("chase") != std::string::npos, "scripts can read a brain's behavior tree");
    runtime.stop();
    fs::remove_all(dir, ec);
}

void test_ai_presets() {
    for (const std::string& b : orchard::Agent::brains()) {
        check(orchard::Agent::preset(b).has_value(), "brain " + b + " has settings");
        std::string why;
        const std::string& text = orchard::ai::brain_behavior(b);
        check(!text.empty() && orchard::ai::BehaviorTree::parse(text, &why).has_value(), "brain " + b + " has a behavior tree that parses: " + why);
    }
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity e = s.create("Guard");
    orchard::Agent& a = s.add_agent(e);
    a = *orchard::Agent::preset("soldier");
    a.squad = "alpha";
    a.patrol = "Route";
    a.walk_speed = 1.5f;
    const orchard::Entity sp = s.create("Speaker");
    orchard::SoundSource& snd = s.add_sound(sp);
    snd.clip = "Sounds/a b.wav";
    snd.range = 40.0f;
    snd.ai_hears = true;
    std::string error;
    const std::optional<orchard::Scene> back = orchard::Scene::load(s.save(), &error);
    const orchard::Agent* ba = back ? back->agent(back->find("Guard")) : nullptr;
    check(ba && ba->brain == "soldier" && ba->squad == "alpha" && ba->patrol == "Route" && ba->walk_speed == 1.5f && ba->ranged, "AI agents save and load: " + error);
    const orchard::SoundSource* bs = back ? back->sound(back->find("Speaker")) : nullptr;
    check(bs && bs->clip == "Sounds/a b.wav" && bs->range == 40.0f && bs->ai_hears, "sound sources save and load");
    orchard::ai::BtNode dummy;
    std::string why;
    check(!orchard::ai::BehaviorTree::parse("selector\n  repeat\n", &why).has_value(), "a decorator without a child is refused (" + why + ")");
    const auto tree = orchard::ai::BehaviorTree::parse("utility\n  attack score=\"health() * 2\"\n  flee score=0.5\n", &why);
    check(tree && tree->root().children.size() == 2, "utility selectors parse scores: " + why);
    check(orchard::ai::evaluate_expression("clamp(3 * 2 - 1, 0, 4) + (2 > 1 && !false)", {}) == 5.0, "AI expressions evaluate");
}

void test_brain_graphs() {
    using orchard::graph::Graph;
    using orchard::graph::GraphKind;
    for (const std::string& b : orchard::Agent::brains()) {
        const std::string& text = orchard::ai::brain_behavior(b);
        if (text.empty()) continue;
        std::string why;
        const std::optional<Graph> g = Graph::from_behavior(text, &why);
        check(g.has_value() && g->is_brain(), "brain " + b + " turns into a graph: " + why);
        if (!g) continue;
        std::vector<std::string> errors;
        const std::string back = g->to_behavior(&errors);
        check(errors.empty(), "brain " + b + "'s graph compiles: " + (errors.empty() ? std::string() : errors.front()));
        const auto a = orchard::ai::BehaviorTree::parse(text);
        const auto c = orchard::ai::BehaviorTree::parse(back);
        check(a && c && a->to_text() == c->to_text(), "brain " + b + " survives the trip through a graph");
        const std::optional<Graph> loaded = Graph::load(g->save(), &why);
        check(loaded && loaded->is_brain() && loaded->to_behavior() == back, "brain " + b + "'s graph saves and loads: " + why);
    }
    Graph g(GraphKind::brain);
    const int root = g.add_node("bt_root", 0, 0);
    const int sel = g.add_node("bt_selector", 300, 0);
    const int seq = g.add_node("bt_sequence", 600, 0);
    const int see = g.add_node("bt_if_can_see_target", 900, 0);
    const int shoot = g.add_node("bt_do_shoot", 900, 100);
    const int score = g.add_node("bt_weight", 600, 300);
    const int wander = g.add_node("bt_do_wander", 900, 300);
    check(g.add_node("log", 0, 0) == 0 && Graph().add_node("bt_root", 0, 0) == 0, "script nodes and brain nodes stay in their own kind of graph");
    std::string error;
    check(g.connect(root, "start", sel, "in") && g.connect(sel, "children", seq, "in") && g.connect(sel, "children", score, "in"), "a selector takes several children");
    check(g.connect(seq, "children", shoot, "in") && g.connect(seq, "children", see, "in") && g.connect(score, "child", wander, "in"), "brain links connect");
    check(!g.connect(wander, "x", sel, "in", &error), "wires need real pins");
    const int loop = g.add_node("bt_inverter", 1200, 0);
    check(g.connect(shoot, "x", loop, "in") == false, "leaves have no children");
    check(g.connect(seq, "children", loop, "in") && !g.connect(loop, "child", sel, "in", &error), "a wire can't loop the tree back on itself (" + error + ")");
    g.remove_node(loop);
    g.set_value(wander, "radius", "6");
    g.set_value(wander, "run", "true");
    g.set_value(score, "weight", "3");
    std::vector<std::string> errors;
    const std::string text = g.to_behavior(&errors);
    check(errors.empty() && text == "selector\n  sequence\n    ? can_see_target\n    shoot\n  wander 6 run weight=3\n", "a brain graph makes the behavior tree, children ordered top to bottom: " + text + (errors.empty() ? "" : errors.front()));
    const int other = g.add_node("bt_sequence", 300, 500);
    check(g.connect(other, "children", shoot, "in") && g.parent(shoot) == other, "connecting a node to a new parent moves it");
    check(g.to_behavior(&errors).find("shoot") == std::string::npos, "a moved node leaves its old parent");
    const int empty_sel = g.add_node("bt_selector", 0, 900);
    g.connect(seq, "children", empty_sel, "in");
    g.to_behavior(&errors);
    bool named = false;
    for (const std::string& e : errors) named = named || e.find("(node " + std::to_string(empty_sel) + ")") != std::string::npos;
    check(named, "problems name the node they're on");
    Graph expr(GraphKind::brain);
    const int r2 = expr.add_node("bt_root", 0, 0);
    const int cond = expr.add_node("bt_if", 300, 0);
    const int act = expr.add_node("bt_action", 600, 0);
    expr.connect(r2, "start", cond, "in");
    expr.connect(cond, "child", act, "in");
    expr.set_value(cond, "expression", "health_fraction < ");
    expr.to_behavior(&errors);
    check(!errors.empty(), "a broken expression is caught in the graph");
    expr.set_value(cond, "expression", "health_fraction < 0.5 && distance > 3");
    expr.set_value(act, "name", "custom_thing");
    expr.set_value(act, "arguments", "1 2 speed=3");
    check(expr.to_behavior(&errors) == "if health_fraction < 0.5 && distance > 3\n  custom_thing 1 2 speed=3\n" && errors.empty(), "Only If and Any Action write what they hold");
    std::string why;
    const auto made = orchard::graph::brain_file_behavior(expr.save(), &why);
    check(made && *made == "if health_fraction < 0.5 && distance > 3\n  custom_thing 1 2 speed=3\n", "a .brain file turns into its behavior tree: " + why);
    check(orchard::graph::is_brain_file("Behaviors/x.brain") && !orchard::graph::is_brain_file("Graphs/x.graph"), "brain files are told apart by name");
    orchard::editor::GraphView v;
    v.load(g);
    bool script_nodes = false, brain_nodes = false;
    for (const auto& cat : orchard::editor::GraphView::catalog(GraphKind::brain))
        for (const auto& t : cat.types) script_nodes = script_nodes || t.id == "log";
    for (const auto& cat : orchard::editor::GraphView::catalog())
        for (const auto& t : cat.types) brain_nodes = brain_nodes || t.id.rfind("bt_", 0) == 0;
    check(!script_nodes && !brain_nodes && v.is_brain(), "the node menu only shows nodes for the kind of graph that's open");
    const auto set = Graph::from_behavior("sequence\n  set mode=angry\n  check seen\n  ? blackboard mode=angry\n  wait 1 3\n  move_to Player run reach=2\n  wander run\n");
    check(set && set->to_behavior() == "sequence\n  set mode=angry\n  check seen\n  ? blackboard mode=angry\n  wait 1 3\n  move_to Player run reach=2\n  wander run\n", "notes, waits, flags and named settings come back the same");
}

void test_secure_random() {
    std::uint8_t a[32] = {};
    std::uint8_t b[32] = {};
    check(orchard::os_entropy(a, sizeof(a)) && orchard::os_entropy(b, sizeof(b)) && !std::equal(a, a + 32, b), "the system's own secure random source works");
    orchard::SecureRandom& r = orchard::secure_random();
    check(r.ok(), "the system gives secure random bytes");
    const std::string id = r.uuid();
    check(id.size() == 36 && id[14] == '4' && id[8] == '-' && std::string("89ab").find(id[19]) != std::string::npos, "secure UUIDs are version 4: " + id);
    check(r.hex(16).size() == 32 && r.token(32).size() == 43 && r.uuid() != id, "secure tokens and hex have the right length and differ");
    bool in_range = true;
    int seen[6] = {};
    for (int i = 0; i < 6000; ++i) {
        const long long v = r.integer(1, 6);
        in_range = in_range && v >= 1 && v <= 6;
        if (v >= 1 && v <= 6) ++seen[v - 1];
    }
    check(in_range && *std::min_element(seen, seen + 6) > 800, "secure whole numbers stay in range and spread out");
    const double u = r.unit();
    check(u >= 0.0 && u < 1.0 && r.text(10, "ab").find_first_not_of("ab") == std::string::npos, "secure numbers and text");
}

void test_solids() {
    using namespace orchard;
    const fs::path dir = fs::temp_directory_path() / "orchard-test-solids";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    SolidOptions o;
    o.project_root = dir.string();
    o.loader = [&](const std::string& source, std::vector<Vec3>& v, std::vector<std::uint32_t>& idx) {
        std::ifstream in(dir / source);
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream ls(line);
            std::string key;
            ls >> key;
            if (key == "v") {
                Vec3 p;
                ls >> p.x >> p.y >> p.z;
                v.push_back(p);
            } else if (key == "f") {
                std::vector<std::uint32_t> face;
                for (std::string tok; ls >> tok;) face.push_back(static_cast<std::uint32_t>(std::atoi(tok.c_str()) - 1));
                for (std::size_t k = 2; k < face.size(); ++k) {
                    idx.push_back(face[0]);
                    idx.push_back(face[k - 1]);
                    idx.push_back(face[k]);
                }
            }
        }
        return !idx.empty();
    };
    auto world_position_of = [](const Scene& sc, Entity e) { return mul_point(sc.world_matrix(e), Vec3{}); };
    auto volume_of = [&](const Scene& sc, Entity e) {
        std::optional<SolidMesh> m = entity_solid(sc, e, Mat4{}, o.loader);
        return m ? m->volume() : 0.0f;
    };

    Scene scene;
    const Entity holder = scene.create("Holder");
    scene.transform(holder)->position = Vec3{5.0f, 0.0f, 0.0f};
    const Entity a = scene.create("A", holder);
    scene.add_collider(a, ColliderShape::box)->size = Vec3{2.0f, 2.0f, 2.0f};
    scene.add_material(a).color = Vec3{1.0f, 0.0f, 0.0f};
    scene.add_body(a).type = BodyType::fixed;
    const Entity b = scene.create("B", holder);
    scene.add_collider(b, ColliderShape::box)->size = Vec3{2.0f, 2.0f, 2.0f};
    scene.transform(b)->position = Vec3{1.0f, 0.0f, 0.0f};
    scene.transform(b)->rotation = from_euler(Vec3{0.0f, 90.0f, 0.0f});
    scene.add_material(b).color = Vec3{0.0f, 0.0f, 1.0f};

    SolidResult u = combine_solids(scene, {a, b}, SolidOperation::union_parts, o);
    check(u.ok() && scene.alive(u.entity), "two boxes union into one part: " + u.error);
    check(!scene.alive(a) && !scene.alive(b) && scene.parent(u.entity) == holder, "the union replaces its parts under the same parent");
    const std::vector<Collider>* uc = scene.colliders(u.entity);
    check(uc && uc->size() == 1 && uc->front().shape == ColliderShape::mesh && fs::exists(dir / uc->front().source), "a fixed union gets a mesh collider from its .obj");
    check(fs::exists(dir / "Assets/Solids/Union.mtl") && !scene.material(u.entity), "parts with different colours keep them in the union");
    check(std::fabs(volume_of(scene, u.entity) - 12.0f) < 0.01f, "the union's volume is right: " + std::to_string(volume_of(scene, u.entity)));
    check(std::fabs(world_position_of(scene, u.entity).x - 5.5f) < 1e-4f, "the union sits in the middle of its parts");
    const std::string saved = scene.save();
    const std::optional<Scene> again = Scene::load(saved);
    const Entity loaded = again ? again->find("Union") : Entity{};
    check(again && again->solid(loaded) && again->solid(loaded)->parts == "Assets/Solids/Union.parts" && again->solid(loaded)->operation == SolidOperation::union_parts, "a union's parts are saved with the scene");

    SolidResult s = separate_solid(scene, u.entity, o);
    check(s.ok() && s.entities.size() == 2 && !scene.alive(u.entity), "separate gives the two parts back: " + s.error);
    if (s.entities.size() == 2) {
        const Entity a2 = scene.find("A");
        const Entity b2 = scene.find("B");
        check(scene.alive(a2) && scene.alive(b2) && scene.parent(b2) == holder, "separated parts go back under the same parent");
        const Vec3 pb = world_position_of(scene, b2);
        check(std::fabs(pb.x - 6.0f) < 1e-4f && std::fabs(pb.y) < 1e-4f, "separated parts go back where they were");
        check(std::fabs(to_euler(scene.transform(b2)->rotation).y - 90.0f) < 0.05f, "separated parts keep their rotation");

        scene.add_solid(b2).negative = true;
        check(is_negative_part(scene, b2), "negate marks a part");
        SolidResult cut = combine_solids(scene, {a2, b2}, SolidOperation::union_parts, o);
        check(cut.ok() && std::fabs(volume_of(scene, cut.entity) - 4.0f) < 0.01f, "a negative part is cut out of the union");
        check(cut.ok() && scene.material(cut.entity) && scene.material(cut.entity)->color.x == 1.0f, "with one colour left, the union takes that part's material");
        SolidResult back = separate_solid(scene, cut.entity, o);
        const Entity b3 = scene.find("B");
        check(back.ok() && scene.alive(b3) && is_negative_part(scene, b3), "separating keeps the negative part negative");
        scene.solid(b3)->negative = false;
        SolidResult both = combine_solids(scene, {scene.find("A"), b3}, SolidOperation::intersect_parts, o);
        check(both.ok() && std::fabs(volume_of(scene, both.entity) - 4.0f) < 0.01f, "intersect keeps only where the parts overlap");
        SolidResult re = rebuild_solid(scene, both.entity, [&] { SolidOptions one = o; one.one_color = true; return one; }());
        check(re.ok() && scene.material(both.entity) && scene.solid(both.entity)->one_color, "one colour remakes it with a single material");
    }

    Scene apart;
    const Entity x = apart.create("X");
    apart.add_collider(x, ColliderShape::sphere);
    const Entity y = apart.create("Y");
    apart.add_collider(y, ColliderShape::sphere);
    apart.transform(y)->position = Vec3{5.0f, 0.0f, 0.0f};
    SolidResult none = combine_solids(apart, {x, y}, SolidOperation::intersect_parts, o);
    check(!none.ok() && apart.alive(x) && apart.alive(y), "intersecting parts that don't touch says so and changes nothing");
    apart.add_solid(x).negative = true;
    apart.add_solid(y).negative = true;
    check(!combine_solids(apart, {x, y}, SolidOperation::union_parts, o).ok(), "all-negative unions are refused");
    Scene flat(Dimension::two_d);
    const Entity f1 = flat.create("F1");
    flat.add_collider(f1, ColliderShape::box);
    const Entity f2 = flat.create("F2");
    flat.add_collider(f2, ColliderShape::box);
    check(!combine_solids(flat, {f1, f2}, SolidOperation::union_parts, o).ok(), "2D scenes say union is for 3D");
    const Transform t = transform_from_matrix(trs(Vec3{1.0f, 2.0f, 3.0f}, from_euler(Vec3{10.0f, 20.0f, 30.0f}), Vec3{2.0f, 3.0f, 4.0f}));
    const Vec3 e = to_euler(t.rotation);
    check(std::fabs(t.position.z - 3.0f) < 1e-4f && std::fabs(t.scale.y - 3.0f) < 1e-4f && std::fabs(e.x - 10.0f) < 0.05f && std::fabs(e.z - 30.0f) < 0.05f, "matrices turn back into transforms");
    fs::remove_all(dir, ec);
}

void test_nav_airborne_goal() {
    orchard::ai::NavGrid nav;
    orchard::ai::NavSettings ns;
    std::size_t sampled = 0;
    nav.configure(ns, [&sampled](float, float, float, float, std::vector<orchard::ai::NavSurface>& out) {
        ++sampled;
        out.clear();
        orchard::ai::NavSurface ground;
        ground.y = 0.0f;
        ground.bottom = -1.0f;
        out.push_back(ground);
    });
    orchard::ai::NavPathOptions o;
    o.max_expand = 40000;
    const orchard::ai::NavPath up = nav.find_path({0.0f, 0.0f, 0.0f}, {8.0f, 20.0f, 3.0f}, o);
    check(up.found && !up.points.empty(), "a path to a target 20 m in the air leads to the ground under it");
    if (!up.points.empty()) check(std::fabs(up.points.back().position.x - 8.0f) < 0.6f && std::fabs(up.points.back().position.z - 3.0f) < 0.6f && std::fabs(up.points.back().position.y) < 0.01f, "and ends on the ground right below the target");
    check(up.expanded < 2000 && sampled < 4000, "without searching the whole map (" + std::to_string(up.expanded) + " nodes, " + std::to_string(sampled) + " columns sampled)");
    const std::size_t before = sampled;
    const orchard::ai::NavPath lost = nav.find_path({0.0f, 0.0f, 0.0f}, {8.0f, 500.0f, 3.0f}, o);
    check(!lost.found && lost.expanded <= 2001 && sampled - before < 8000, "a target far out of reach gives a short partial search (" + std::to_string(lost.expanded) + " nodes)");
}

void test_parallel_search() {
    using namespace orchard::ai;
    GridPathfinder grid(60, 60);
    std::uint64_t h = 7;
    for (int i = 0; i < 700; ++i) {
        h = orchard::hash64(h + static_cast<std::uint64_t>(i));
        grid.set_blocked(static_cast<int>(h % 60), static_cast<int>((h >> 20) % 60));
    }
    grid.set_blocked(0, 0, false);
    grid.set_blocked(59, 59, false);
    ParallelSearchOptions one;
    one.threads = 1;
    ParallelSearchOptions many;
    many.threads = 6;
    const SearchResult<Cell> a = grid.find_breadth_first({0, 0}, {59, 59}, one);
    const SearchResult<Cell> b = grid.find_breadth_first({0, 0}, {59, 59}, many);
    check(a.found && b.found && a.path.size() == b.path.size() && b.runners == 6, "breadth-first with 6 runners finds the same shortest path length as 1");
    auto valid = [&](const SearchResult<Cell>& r) {
        if (r.path.empty() || r.path.front() != Cell{0, 0} || r.path.back() != Cell{59, 59}) return false;
        for (std::size_t i = 1; i < r.path.size(); ++i)
            if (std::abs(r.path[i].x - r.path[i - 1].x) > 1 || std::abs(r.path[i].y - r.path[i - 1].y) > 1 || !grid.walkable(r.path[i].x, r.path[i].y)) return false;
        return true;
    };
    const SearchResult<Cell> c = grid.find_depth_first({0, 0}, {59, 59}, many);
    check(c.found && valid(b) && valid(c), "multithreaded depth-first finds a real path");
    ParallelSearchOptions shortest = many;
    shortest.first_found = false;
    shortest.max_expand = 50000000;
    GridPathfinder small(14, 14);
    for (int y = 0; y < 12; ++y) small.set_blocked(7, y);
    const SearchResult<Cell> s1 = small.find_breadth_first({0, 0}, {13, 0}, one);
    const SearchResult<Cell> s2 = small.find_depth_first({0, 0}, {13, 0}, shortest);
    check(s1.found && s2.found && s1.path.size() == s2.path.size(), "depth-first runners that keep going end on the shortest path (shorter runners win)");
    auto ring = [](int n, auto&& emit) {
        emit((n + 1) % 12, 1.0);
        if (n == 2) emit(6, 1.0);
    };
    const std::vector<int> cycle = find_cycle<int>(0, ring);
    check(!cycle.empty() && cycle.front() == cycle.back(), "the on-path pointers spot a loop");
    const SearchResult<int> d = depth_first<int>(0, [](int n) { return n == 11; }, ring);
    const SearchResult<int> d2 = depth_first<int>(0, [](int n) { return n < 0; }, ring);
    check(d.found && d.path.size() == 12 && !d2.found && d2.loops > 0, "depth-first counts the loops it runs into");
    auto line = [](int n, auto&& emit) {
        if (n < 9) emit(n + 1, 1.0);
    };
    check(!has_cycle<int>(0, line), "a line has no loop");
    const SearchResult<int> e = parallel_breadth_first<int>(0, [](int n) { return n == 7; }, ring, many);
    check(e.found && e.path.size() == 5, "generic breadth-first runners take the shortcut");
}

} // namespace

int main() {
    test_brain_graphs();
    test_secure_random();
    test_parallel_search();
    test_solids();
    test_script_library();
    test_ai_presets();
    test_character_looks();
    test_triangles();
    test_terrain();
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
    test_lights_and_materials();
    test_textures_sections_and_fluids();
    test_entity_templates_and_emitters();
    test_light_animation();
    test_script_keys();
    test_nav_airborne_goal();
    test_damage_scripts();
    test_item_scripts();
    test_project_physics_and_lighting();
    test_graph_view();
    test_models_and_animation();
    test_humanoids_cameras_and_reflections();
    test_particle_presets();
    std::cout << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
