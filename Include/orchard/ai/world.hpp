#ifndef ORCHARD_AI_WORLD_HPP
#define ORCHARD_AI_WORLD_HPP

#include "../math.hpp"
#include "../random.hpp"
#include "../scene.hpp"
#include "behavior_tree.hpp"
#include "chess.hpp"
#include "goap.hpp"
#include "learning.hpp"
#include "navgrid.hpp"
#include "tactics.hpp"
#include "voxel.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace orchard {

class ScriptPhysics;
class Gameplay;

namespace ai {

enum class AlertPhase { calm = 0, caution = 1, alert = 2, evasion = 3 };
const char* to_string(AlertPhase p);

struct NoiseEvent {
    Vec3 at;
    float loudness = 10.0f;
    Entity source;
    std::string kind;
    double time = 0.0;
};

struct CoverSpot {
    Vec3 position;
    float score = 0.0f;
    bool full = false;
    bool found = false;
};

struct PositionWeights {
    float near_self = 0.3f;
    float near_target = 0.0f;
    float ideal_distance = 0.0f;
    float ideal_weight = 0.0f;
    float hidden_from_target = 0.0f;
    float sees_target = 0.0f;
    float danger = 1.0f;
    float away_from_allies = 0.0f;
};

const std::string& brain_behavior(const std::string& brain);

class AiWorld {
public:
    using Spawner = std::function<Entity(const std::string& entity_template, Vec3 at)>;
    using SoundPlayer = std::function<void(const std::string& clip, Vec3 at, Entity follow, float volume)>;

    AiWorld();
    ~AiWorld();
    AiWorld(const AiWorld&) = delete;
    AiWorld& operator=(const AiWorld&) = delete;

    void start(Scene* scene, ScriptPhysics* physics, Gameplay* gameplay);
    void stop();
    void tick(double dt, double now);
    bool running() const;
    void set_spawner(Spawner spawner);
    void set_sound_player(SoundPlayer player);
    void set_log(std::function<void(const std::string&, bool)> log);

    bool is_agent(Entity e) const;
    bool add_agent(Entity e, const std::string& brain);
    bool set_brain(Entity e, const std::string& brain);
    bool set_behavior(Entity e, const std::string& text, std::string* error = nullptr);
    bool define_behavior(const std::string& name, const std::string& text, std::string* error = nullptr);
    std::string behavior_text(Entity e) const;
    bool set_enabled(Entity e, bool on);
    bool set_team(Entity e, const std::string& team);
    std::string team(Entity e) const;
    void set_relation(const std::string& a, const std::string& b, int relation);
    int relation(const std::string& a, const std::string& b) const;
    bool hostile(Entity a, Entity b) const;

    bool move_to(Entity e, Vec3 at, bool run = false);
    void stop_moving(Entity e);
    bool arrived(Entity e) const;
    bool moving(Entity e) const;
    bool set_target(Entity e, Entity target);
    Entity target(Entity e) const;
    bool can_see(Entity e, Entity other) const;
    float awareness(Entity e, Entity other) const;
    float suspicion(Entity e) const;
    int alert_level(Entity e) const;
    std::string state(Entity e) const;
    std::string action(Entity e) const;
    std::string next_event(Entity e);
    std::string pending_action(Entity e) const;
    std::string pending_argument(Entity e, int index) const;
    bool finish_action(Entity e, bool success);
    std::vector<Vec3> current_path(Entity e) const;

    Blackboard* blackboard(Entity e);
    void make_noise(Vec3 at, float loudness, Entity source, const std::string& kind);
    void set_visibility(Entity e, float factor);
    float visibility(Entity e) const;

    AlertPhase team_alert(const std::string& team) const;
    void set_team_alert(const std::string& team, AlertPhase phase, Vec3 last_known);

    NavGrid& nav(float radius = 0.4f, float height = 1.8f, float step = 0.45f, float jump = 1.1f, float drop = 4.0f);
    NavPath find_path(Vec3 from, Vec3 to, const NavPathOptions& options = NavPathOptions());
    Vec3 random_point(Vec3 near, float radius);
    int add_danger(Vec3 at, float radius, float amount, float seconds);
    void clear_danger();
    std::size_t bake(Vec3 low, Vec3 high);
    void invalidate(Vec3 low, Vec3 high);

    CoverSpot find_cover(Entity e, Vec3 threat, float radius = 12.0f);
    std::vector<Vec3> best_positions(Vec3 center, float radius, Entity self, Entity target, const PositionWeights& weights, int count = 1);
    bool line_of_sight(Vec3 from, Vec3 to, Entity ignore_a = Entity{}, Entity ignore_b = Entity{}) const;

    Director& director();
    void set_spawns(const std::string& entity_template, int max_alive, float min_distance, float max_distance, float interval);
    int spawned_alive() const;
    Entity main_player() const;

    VoxelWorld& voxels();
    GoapPlanner& planner(const std::string& name);
    Chess* chess(int id);
    int new_chess(const std::string& fen = std::string());
    void drop_chess(int id);
    NeuralNet* net(const std::string& name);
    NeuralNet& make_net(const std::string& name, const std::vector<int>& layers, Activation hidden, Activation output);
    QLearner& learner(const std::string& name, int actions);
    MarkovChain& markov(const std::string& name, int order);
    Random& random();

    std::size_t agent_count() const;
    std::vector<Entity> agents() const;
    double now() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_WORLD_HPP
