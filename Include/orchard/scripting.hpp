#ifndef ORCHARD_SCRIPTING_HPP
#define ORCHARD_SCRIPTING_HPP

#include "axes.hpp"
#include "project.hpp"
#include "scene.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace orchard {

struct ScriptToolchain {
    std::string walnut = "walnut";
    std::string cxx = "c++";
    std::string cc = "cc";
    std::string dotnet = "dotnet";
    std::string java_home;
    std::string javac = "javac";
    std::string include_dir;
    std::string cache_dir;
    std::vector<std::string> cxx_flags;
    bool rebuild = false;
    bool verbose = false;

    bool usable(std::string* why = nullptr) const;
    bool has_headers(std::string* why = nullptr) const;
    static std::string find_include_dir(const std::string& exe_dir);
};

class ScriptPhysics {
public:
    virtual ~ScriptPhysics() = default;
    virtual bool has_body(Entity e) const = 0;
    virtual Vec3 velocity(Entity e) const = 0;
    virtual void set_velocity(Entity e, Vec3 v) = 0;
    virtual Vec3 spin(Entity e) const = 0;
    virtual void set_spin(Entity e, Vec3 v) = 0;
    virtual void impulse(Entity e, Vec3 v) = 0;
    virtual void force(Entity e, Vec3 v) = 0;
    virtual void torque(Entity e, Vec3 v) = 0;
    virtual void moved(Entity e) = 0;
    virtual bool touching(Entity a, Entity b) const = 0;
};

struct ScriptHooks {
    int start_params = -1;
    int tick_params = -1;
    int stop_params = -1;

    bool any() const { return start_params >= 0 || tick_params >= 0 || stop_params >= 0; }
};

struct ScriptBuildResult {
    std::string script;
    std::string module;
    bool ok = false;
    bool cached = false;
    std::string log;
    std::string language;
};

class ScriptRuntime;

class ScriptModule {
public:
    virtual ~ScriptModule() = default;
    virtual int hooks() const = 0;
    virtual bool call(int hook, std::uint64_t entity, double dt, std::string& error) = 0;
};

class ScriptLanguage {
public:
    virtual ~ScriptLanguage() = default;
    virtual std::string name() const = 0;
    virtual std::vector<std::string> extensions() const = 0;
    virtual bool available(const ScriptToolchain& tools, std::string* why = nullptr) const = 0;
    virtual ScriptBuildResult build(ScriptRuntime& runtime, const Project& project, const std::string& script) = 0;
    virtual std::unique_ptr<ScriptModule> load(ScriptRuntime& runtime, const ScriptBuildResult& built, std::string* error) = 0;
    virtual std::string starter(const std::string& class_name, Dimension dimension) const = 0;
    virtual std::string starter(const std::string& class_name, Dimension dimension, Axes axes) const;

    bool handles(const std::string& path) const;
};

class ScriptRuntime {
public:
    using Log = std::function<void(const std::string& message, bool error)>;

    ScriptRuntime();
    ~ScriptRuntime();
    ScriptRuntime(const ScriptRuntime&) = delete;
    ScriptRuntime& operator=(const ScriptRuntime&) = delete;

    void set_toolchain(ScriptToolchain toolchain) { m_tools = std::move(toolchain); }
    const ScriptToolchain& toolchain() const { return m_tools; }
    void set_log(Log log) { m_log = std::move(log); }
    void set_physics(ScriptPhysics* physics) { m_physics = physics; }
    void set_key(const std::string& key, bool down);
    void clear_keys() { m_keys.clear(); }

    void add_language(std::unique_ptr<ScriptLanguage> language);
    ScriptLanguage* language_for(const std::string& script) const;
    ScriptLanguage* language(const std::string& name) const;
    std::vector<ScriptLanguage*> languages() const;
    std::string starter(const std::string& script, Dimension dimension) const;
    std::string starter(const std::string& script, Dimension dimension, Axes axes) const;
    bool create_script(const Project& project, const std::string& script, Dimension dimension, std::string* error = nullptr) const;

    std::string walnut_source(const Project& project, const std::string& script, std::string* error = nullptr) const;
    ScriptBuildResult build_walnut(const Project& project, const std::string& script);
    ScriptBuildResult build(const Project& project, const std::string& script);
    std::vector<ScriptBuildResult> build_all(const Project& project, const Scene& scene);

    bool start(const Project& project, Scene& scene);
    void tick(double dt);
    void stop();

    bool running() const { return m_running; }
    double time() const { return m_time; }
    std::size_t instance_count() const { return m_instances.size(); }
    Scene* scene() const { return m_scene; }
    ScriptPhysics* physics() const { return m_physics; }
    bool key_down(const std::string& key) const { return m_keys.count(key) > 0; }
    void log(const std::string& message, bool error) const;

    static ScriptHooks find_hooks(const std::string& source);
    static std::string with_hooks(const std::string& source, const ScriptHooks& hooks);
    static ScriptHooks find_native_hooks(const std::string& source);
    static std::string native_registration(const ScriptHooks& hooks, bool c_linkage, const std::string& include_path);

    std::vector<std::string> loaded_modules() const;

private:
    struct Module;
    struct Instance {
        Entity entity;
        Module* module = nullptr;
        bool broken = false;
    };

    Module* load(const ScriptBuildResult& built);
    void call(Instance& instance, int hook, double dt);
    void unload_all();

    ScriptToolchain m_tools;
    Log m_log;
    ScriptPhysics* m_physics = nullptr;
    Scene* m_scene = nullptr;
    std::set<std::string> m_keys;
    std::map<std::string, ScriptBuildResult> m_built;
    std::map<std::string, std::unique_ptr<Module>> m_modules;
    std::vector<std::unique_ptr<ScriptLanguage>> m_languages;
    std::vector<Instance> m_instances;
    bool m_running = false;
    double m_time = 0.0;
};

} // namespace orchard

#endif // ORCHARD_SCRIPTING_HPP
