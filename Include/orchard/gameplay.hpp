#ifndef ORCHARD_GAMEPLAY_HPP
#define ORCHARD_GAMEPLAY_HPP

#include "scene.hpp"

#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace orchard {

class ScriptPhysics;

class Gameplay {
public:
    void start(Scene* scene, ScriptPhysics* physics);
    void stop();
    void tick(double dt, double now);
    bool running() const { return m_scene != nullptr; }
    double now() const { return m_now; }

    Entity holder(Entity e) const;
    float health(Entity e) const;
    float max_health(Entity e) const;
    bool set_health(Entity e, float value);
    bool set_max_health(Entity e, float value);
    float heal(Entity e, float amount);
    float damage(Entity e, float amount, Entity attacker = Entity{});
    bool kill(Entity e, Entity attacker = Entity{});
    bool dead(Entity e) const;
    bool revive(Entity e, float to = -1.0f);
    bool set_invulnerable(Entity e, bool on);
    float damage_taken(Entity e) const;
    bool died(Entity e) const;
    Entity last_attacker(Entity e) const;

    bool ignite(Entity e, float seconds = 10.0f);
    bool extinguish(Entity e);
    bool burning(Entity e) const;
    int explode(Vec3 at, float radius, float force, float damage, Entity attacker = Entity{}, bool effect = true);
    Entity effect(const std::string& name, Vec3 at, float scale = 1.0f, Entity parent = Entity{});
    Entity scorch(Vec3 at, float reach = 3.0f, float scale = 1.0f, Entity skip = Entity{});

    Vec3 position(Entity e) const;
    bool teleport(Entity e, Vec3 to);
    bool look_at(Entity e, Vec3 target);
    float distance(Entity a, Entity b) const;
    Entity nearest_tagged(const std::string& tag, Vec3 from, Entity skip = Entity{}) const;
    double random(double low, double high);

private:
    struct State {
        float pending = 0.0f;
        float last = 0.0f;
        bool died_pending = false;
        bool died_now = false;
        bool dead = false;
        Entity attacker;
        double hurt_at = -1e9;
        Transform start;
        bool have_start = false;
        double respawn_at = -1.0;
        Vec3 velocity;
        bool have_velocity = false;
    };
    struct Fire {
        Entity target;
        Entity effect;
        double until = 0.0;
        double next_spread = 0.0;
    };
    struct Blast {
        Vec3 at;
        float radius = 0.0f;
        float force = 0.0f;
        float damage = 0.0f;
        Entity attacker;
    };

    State& state(Entity e);
    void die(Entity e, Entity attacker);
    float size_of(Entity e) const;
    void char_it(Entity e, float dt, Entity skip);

    Scene* m_scene = nullptr;
    ScriptPhysics* m_physics = nullptr;
    double m_now = 0.0;
    std::unordered_map<std::uint64_t, State> m_state;
    std::vector<Fire> m_fires;
    std::vector<std::pair<Entity, double>> m_temporary;
    std::vector<Entity> m_doomed;
    std::vector<Blast> m_blasts;
    std::mt19937 m_rng{12345u};
};

} // namespace orchard

#endif // ORCHARD_GAMEPLAY_HPP
