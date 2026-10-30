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
