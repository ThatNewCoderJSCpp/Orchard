#ifndef ORCHARD_AI_STEERING_HPP
#define ORCHARD_AI_STEERING_HPP

#include "../math.hpp"
#include "../random.hpp"

#include <functional>
#include <vector>

namespace orchard {
namespace ai {

struct Mover {
    Vec3 position;
    Vec3 velocity;
    float max_speed = 4.0f;
    float max_force = 20.0f;
    float radius = 0.4f;
};

Vec3 limit(Vec3 v, float max_length);
Vec3 flatten(Vec3 v);

Vec3 seek(const Mover& m, Vec3 target);
Vec3 flee(const Mover& m, Vec3 threat, float panic_distance = 1.0e9f);
Vec3 arrive(const Mover& m, Vec3 target, float slow_radius = 2.0f, float stop_radius = 0.1f);
Vec3 pursue(const Mover& m, Vec3 target, Vec3 target_velocity, float max_prediction = 2.0f);
Vec3 evade(const Mover& m, Vec3 threat, Vec3 threat_velocity, float max_prediction = 2.0f);
Vec3 wander(const Mover& m, float& wander_angle, Random& random, float radius = 1.2f, float distance = 2.0f, float jitter = 0.6f, bool flat = true);
Vec3 separation(const Mover& m, const std::vector<Vec3>& neighbors, float range);
Vec3 alignment(const Mover& m, const std::vector<Vec3>& neighbor_velocities);
Vec3 cohesion(const Mover& m, const std::vector<Vec3>& neighbors);
Vec3 flock(const Mover& m, const std::vector<Mover>& neighbors, float range, float separation_weight = 1.6f, float alignment_weight = 1.0f, float cohesion_weight = 0.8f);
Vec3 avoid_obstacles(const Mover& m, const std::function<bool(Vec3 origin, Vec3 direction, float distance, Vec3& normal, float& hit)>& ray, float look_ahead = 2.5f, int feelers = 3, bool flat = true);
Vec3 follow_path(const Mover& m, const std::vector<Vec3>& path, std::size_t& index, float reach = 0.6f, bool loop = false);
Vec3 interpose(const Mover& m, Vec3 a, Vec3 b);
Vec3 hide(const Mover& m, Vec3 threat, const std::vector<Vec3>& obstacles, float obstacle_radius = 1.0f);
Vec3 offset_pursuit(const Mover& m, Vec3 leader, Vec3 leader_forward, Vec3 offset);
Vec3 avoid_neighbors(const Mover& m, const std::vector<Mover>& others, float time_horizon = 1.5f);

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_STEERING_HPP
