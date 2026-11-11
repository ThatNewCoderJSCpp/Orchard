#ifndef ORCHARD_AI_TACTICS_HPP
#define ORCHARD_AI_TACTICS_HPP

#include "../math.hpp"
#include "../random.hpp"

#include <map>
#include <string>
#include <vector>

namespace orchard {
namespace ai {

bool ballistic_velocity(Vec3 from, Vec3 to, float speed, float gravity, bool high_arc, Vec3& out);
Vec3 lob_velocity(Vec3 from, Vec3 to, float apex_height, float gravity);
float ballistic_range(float speed, float gravity, float height = 0.0f);
bool intercept(Vec3 shooter, Vec3 target, Vec3 target_velocity, float projectile_speed, Vec3& aim_point, float* time = nullptr);
Vec3 aim_with_error(Vec3 direction, float accuracy, Random& random, float max_spread_degrees = 12.0f);
float hit_chance(float distance, float accuracy, float target_size = 0.5f, float max_spread_degrees = 12.0f);

class Director {
public:
    enum class Phase { build_up, peak, relax };

    float build_up_rate = 0.02f;
    float damage_weight = 0.02f;
    float nearby_weight = 0.04f;
    float sight_weight = 0.15f;
    float decay = 0.08f;
    float peak_at = 0.85f;
    float peak_time = 8.0f;
    float relax_time = 25.0f;
    float menace_rise = 0.35f;
    float menace_fall = 0.08f;
    float menace_limit = 1.0f;
    int max_spawns = 12;

    void tick(float dt);
    void report_damage(float amount);
    void report_nearby(int threats, float closest_distance);
    void report_seen(bool hunter_visible, float hunter_distance);
    void note(const std::string& habit, float amount = 1.0f);
    float habit(const std::string& name) const;
    bool unlocked(const std::string& name, float threshold) const { return habit(name) >= threshold; }
    const std::map<std::string, float>& habits() const { return m_habits; }

    Phase phase() const { return m_phase; }
    const char* phase_name() const;
    float intensity() const { return m_intensity; }
    float menace() const { return m_menace; }
    bool wants_spawn(int alive) const;
    bool wants_backoff() const { return m_backoff; }
    bool wants_pressure() const { return !m_backoff && m_menace < 0.35f; }
    float hint_radius() const;
    void reset();

private:
    Phase m_phase = Phase::build_up;
    float m_intensity = 0.0f;
    float m_menace = 0.0f;
    float m_phase_time = 0.0f;
    float m_pending_damage = 0.0f;
    int m_nearby = 0;
    float m_closest = 1e9f;
    bool m_seen = false;
    float m_hunter_distance = 1e9f;
    bool m_backoff = false;
    std::map<std::string, float> m_habits;
};

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_TACTICS_HPP
