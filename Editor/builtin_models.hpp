#ifndef ORCHARD_EDITOR_BUILTIN_MODELS_HPP
#define ORCHARD_EDITOR_BUILTIN_MODELS_HPP

#include <string>
#include <vector>

namespace orchard {
namespace editor {

const std::vector<std::string>& builtin_model_names();
std::string builtin_model_glb(const std::string& name);
bool write_builtin_model(const std::string& name, const std::string& path, std::string* error = nullptr);

} // namespace editor
} // namespace orchard

#endif // ORCHARD_EDITOR_BUILTIN_MODELS_HPP
