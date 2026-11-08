#ifndef ORCHARD_SCRIPT_ORCHARD_HPP
#define ORCHARD_SCRIPT_ORCHARD_HPP

#include "script_api.hpp"

#include <sstream>
#include <string>
#include <utility>

namespace orchard {
namespace script {

inline void log(const std::string& message) { World::log(message); }
inline void warn(const std::string& message) { World::warn(message); }
inline double dt() { return World::dt(); }
inline double time() { return World::time(); }
inline bool is_2d() { return World::is_2d(); }
inline bool key_down(const std::string& key) { return World::key_down(key); }
inline Entity self() { return Entity::current(); }
inline Entity find(const std::string& name) { return World::find(name); }
inline Entity spawn(const std::string& name) { return World::spawn(name); }
inline Entity spawn(const std::string& name, const Entity& parent) { return World::spawn_child(name, parent); }
inline Entity instantiate(const std::string& entity_template, double x, double y, double z) { return World::instantiate(entity_template, x, y, z); }
inline Entity instantiate(const std::string& entity_template, double x, double y, double z, const Entity& parent) { return World::instantiate_child(entity_template, x, y, z, parent); }
inline void play_particles(const Entity& e) { World::play_particles(e); }
inline void stop_particles(const Entity& e) { World::stop_particles(e); }
inline void burst_particles(const Entity& e, long long count) { World::burst_particles(e, count); }
inline void clear_particles(const Entity& e) { World::clear_particles(e); }
inline long long pour_fluid(double x, double y, double radius) { return World::pour_fluid(x, y, radius); }
inline long long erase_fluid(double x, double y, double radius) { return World::erase_fluid(x, y, radius); }
inline long long push_fluid(double x, double y, double radius, double vx, double vy) { return World::push_fluid(x, y, radius, vx, vy); }
inline long long splash_fluid(double x, double y, double radius, double strength) { return World::splash_fluid(x, y, radius, strength); }
inline long long paint_sand(double x, double y, double radius, long long cell) { return World::paint_sand(x, y, radius, cell); }

template <class... Parts>
std::string text(Parts&&... parts) {
    std::ostringstream out;
    (out << ... << std::forward<Parts>(parts));
    return out.str();
}

inline std::ostream& operator<<(std::ostream& out, const Vec3& v) {
    return out << "(" << v.x << ", " << v.y << ", " << v.z << ")";
}

} // namespace script
} // namespace orchard

#endif // ORCHARD_SCRIPT_ORCHARD_HPP
