#ifndef ORCHARD_EDITOR_MODEL_VIEW_HPP
#define ORCHARD_EDITOR_MODEL_VIEW_HPP

#include <orchard/scene.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace graphics {
class Renderer;
class Model3D;
struct Material3D;
} // namespace graphics
} // namespace fizmo

namespace orchard {
namespace editor {

struct ModelClip {
    std::string name;
    float duration = 0.0f;
};

struct ModelInfo {
    bool ok = false;
    std::string error;
    std::vector<ModelClip> clips;
    std::vector<std::string> bones;
    std::vector<std::string> nodes;
    std::vector<std::string> materials;
    Vec3 low;
    Vec3 high;
    std::size_t vertices = 0;
    std::size_t triangles = 0;
    std::size_t textures = 0;
    bool skinned = false;
};

struct AnimationState {
    float time = 0.0f;
    float duration = 0.0f;
    bool finished = false;
    bool playing = false;
};

struct FiredAnimationEvent {
    Entity entity;
    std::string name;
};

struct ModelInstance {
    std::string source;
    Mat4 world;
    bool cast_shadows = true;
};

class ModelView {
public:
    ModelView();
    ~ModelView();
    ModelView(const ModelView&) = delete;
    ModelView& operator=(const ModelView&) = delete;

    void set_root(const std::filesystem::path& project_root);
    const std::filesystem::path& root() const;
    void set_z_up(bool z_up);
    const fizmo::graphics::Model3D* model(const std::string& source, std::string* error = nullptr);
    ModelInfo info(const std::string& source);
    bool reload_changed();
    void forget(const std::string& source);
    void clear();

    void animate(const Scene& scene, double dt, bool running);
    void reset_animation();
    std::vector<Entity> follow_bones(Scene& scene);
    std::vector<FiredAnimationEvent> take_events();
    AnimationState state(Entity e) const;
    bool bone_world(const Scene& scene, Entity owner, const std::string& bone, Mat4& out);
    bool bone_segment(const std::string& source, const std::string& bone, Vec3& head, Vec3& tail);

    void draw(fizmo::graphics::Renderer& renderer, const Scene& scene, const std::function<bool(const Mat4& world, Vec3 low, Vec3 high)>& visible, const std::function<bool(Entity, fizmo::graphics::Material3D&)>& material_for);
    void draw_instances(fizmo::graphics::Renderer& renderer, const std::vector<ModelInstance>& instances, const std::function<bool(const Mat4& world, Vec3 low, Vec3 high)>& visible);
    void draw_source(fizmo::graphics::Renderer& renderer, const std::string& source, const Mat4& world, const fizmo::graphics::Material3D* override_material, bool cast_shadows);
    void draw_bones(fizmo::graphics::Renderer& renderer, const Scene& scene, Entity e);

    bool local_bounds(const std::string& source, Vec3& low, Vec3& high);
    bool world_bounds(const Scene& scene, Entity e, Vec3& low, Vec3& high);
    bool pick(const Scene& scene, Vec3 origin, Vec3 direction, Entity& hit, float& distance);
    bool triangles(const std::string& source, std::vector<Vec3>& positions, std::vector<std::uint32_t>& indices);

    std::size_t drawn() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

std::string guess_bone(const std::vector<std::string>& bones, const std::string& part);

} // namespace editor
} // namespace orchard

#endif // ORCHARD_EDITOR_MODEL_VIEW_HPP
