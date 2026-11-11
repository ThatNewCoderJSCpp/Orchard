#ifndef ORCHARD_EFFECTS_HPP
#define ORCHARD_EFFECTS_HPP

#include "scene.hpp"

#include <string>
#include <vector>

namespace orchard {

const std::vector<std::string>& effect_names();
const char* effect_label(const std::string& name);
bool effect_is_one_shot(const std::string& name);
float effect_length(const std::string& name);
Entity add_effect(Scene& scene, const std::string& name, Entity parent = Entity{}, float scale = 1.0f, double start_time = 0.0);

} // namespace orchard

#endif // ORCHARD_EFFECTS_HPP
