#ifndef ORCHARD_EDITOR_TERRAIN_VIEW_HPP
#define ORCHARD_EDITOR_TERRAIN_VIEW_HPP

#include <orchard/scene.hpp>
#include <orchard/terrain.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace graphics {
class Renderer;
class Camera3D;
class Texture;
} // namespace graphics
} // namespace fizmo

namespace orchard {
namespace editor {

struct TerrainViewStats {
    std::size_t terrains = 0;
    std::size_t chunks = 0;
    std::size_t drawn = 0;
    std::size_t culled = 0;
    std::size_t rebuilt = 0;
    std::size_t pending = 0;
    std::size_t triangles = 0;
    std::size_t instances = 0;
};

struct TerrainScatterDraw {
    Entity terrain;
    std::uint32_t scatter = 0;
    const std::string* model = nullptr;
    Mat4 matrix;
    Vec3 position;
    float scale = 1.0f;
    float draw_distance = 300.0f;
};

class TerrainView {
public:
    TerrainView();
    ~TerrainView();
    TerrainView(const TerrainView&) = delete;
    TerrainView& operator=(const TerrainView&) = delete;

    void set_asset_root(const std::string& folder);
    const std::string& asset_root() const;
    void set_up(Vec3 up);
    Vec3 up() const;
    void set_texture_lookup(std::function<const fizmo::graphics::Texture*(const std::string&)> lookup);
    void set_lod_distance(float multiplier);
    void set_rebuild_budget(std::size_t chunks_per_frame);
    void set_wireframe_selected(bool on);

    void sync(const Scene& scene);
    std::size_t draw(fizmo::graphics::Renderer& renderer, const fizmo::graphics::Camera3D& camera, const Scene& scene, Entity selected = {});
    void draw_brush(fizmo::graphics::Renderer& renderer, const Scene& scene, Entity terrain, Vec3 world_point, float radius);
    void reset();

    std::shared_ptr<TerrainHeights> heights(Entity e);
    std::shared_ptr<const TerrainHeights> heights(Entity e) const;
    std::function<std::shared_ptr<const TerrainHeights>(Entity, const Terrain&)> source() const;
    Mat4 terrain_world(const Scene& scene, Entity e) const;

    bool pick(const Scene& scene, Vec3 origin, Vec3 direction, float max_distance, Entity& hit, Vec3& point, Vec3* normal = nullptr) const;
    float local_height(const Scene& scene, Entity e, Vec3 world_point) const;
    bool sculpt(const Scene& scene, Entity e, Vec3 world_point, const TerrainBrush& brush, float dt = 1.0f);
    bool regenerate(const Scene& scene, Entity e);
    bool import_heightmap(const Scene& scene, Entity e, const std::string& path, std::string* error = nullptr);
    bool unsaved(Entity e) const;
    bool any_unsaved() const;
    bool save(const Scene& scene, Entity e, std::string* error = nullptr);
    std::size_t save_all(Scene& scene, std::vector<std::string>* errors = nullptr);

    const std::vector<ScatterInstance>& instances() const;
    const std::vector<TerrainScatterDraw>& scatter() const;
    void visible_scatter(const fizmo::graphics::Camera3D& camera, std::vector<std::size_t>& out, float radius_per_scale = 4.0f) const;

    TerrainViewStats stats() const;
    const std::vector<std::string>& warnings() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

} // namespace editor
} // namespace orchard

#endif // ORCHARD_EDITOR_TERRAIN_VIEW_HPP
