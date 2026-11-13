#ifndef ORCHARD_EDITOR_BUILTIN_MODELS_HPP
#define ORCHARD_EDITOR_BUILTIN_MODELS_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace orchard {
struct ItemDef;
struct BulletType;
namespace editor {

const std::vector<std::string>& builtin_model_names();
std::string builtin_model_glb(const std::string& name);
bool write_builtin_model(const std::string& name, const std::string& path, std::string* error = nullptr);
std::string item_model_glb(const ItemDef& def, const std::string& variant);
bool write_item_model(const ItemDef& def, const std::string& variant, const std::string& path, std::string* error = nullptr);
std::vector<std::uint8_t> item_icon_rgba(const ItemDef& def, const std::string& variant, const BulletType* bullet, float fill, int size = 64);

} // namespace editor
} // namespace orchard

#endif // ORCHARD_EDITOR_BUILTIN_MODELS_HPP
