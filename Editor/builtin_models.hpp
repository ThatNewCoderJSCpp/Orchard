#ifndef ORCHARD_EDITOR_BUILTIN_MODELS_HPP
#define ORCHARD_EDITOR_BUILTIN_MODELS_HPP

#include <string>
#include <vector>

namespace orchard {
struct ItemDef;
namespace editor {

const std::vector<std::string>& builtin_model_names();
std::string builtin_model_glb(const std::string& name);
bool write_builtin_model(const std::string& name, const std::string& path, std::string* error = nullptr);
// Item meshes: variant is simple, detailed, realistic, worn (on a body or in a hand) or liquid (the part that sloshes).
std::string item_model_glb(const ItemDef& def, const std::string& variant);
bool write_item_model(const ItemDef& def, const std::string& variant, const std::string& path, std::string* error = nullptr);

} // namespace editor
} // namespace orchard

#endif // ORCHARD_EDITOR_BUILTIN_MODELS_HPP
