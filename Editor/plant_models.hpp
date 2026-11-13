#ifndef ORCHARD_EDITOR_PLANT_MODELS_HPP
#define ORCHARD_EDITOR_PLANT_MODELS_HPP

#include <string>
#include <vector>

namespace orchard {
namespace editor {

struct PlantInfo {
    std::string name;
    std::string label;
    std::string surface;
    bool solid = true;
    float trunk_radius = 0.0f;
    float height = 1.0f;
};

const std::vector<PlantInfo>& plant_models();
const PlantInfo* plant_info(const std::string& name);
bool is_plant_model(const std::string& name);
std::string plant_model_glb(const std::string& name);
bool write_plant_model(const std::string& name, const std::string& path, std::string* error = nullptr);
bool plant_model_outdated(const std::string& path);

} // namespace editor
} // namespace orchard

#endif // ORCHARD_EDITOR_PLANT_MODELS_HPP
