#ifndef CHLOROPHYLL_CHLOROPHYLL_HPP
#define CHLOROPHYLL_CHLOROPHYLL_HPP

#include <orchard/axes.hpp>
#include <orchard/project.hpp>
#include <orchard/scene.hpp>
#include <orchard/terrain.hpp>
#include <orchard/viewport.hpp>

#include <fizmo/../lighting.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace graphics {
class Model3D;
class ModelAnimator;
struct ModelMaterial;
} // namespace graphics
} // namespace fizmo

namespace chlorophyll {

namespace fl = fizmo::lighting;

fl::LightingQuality to_fizmo(orchard::LightingLevel level);
fl::Light to_light(const orchard::Light& light, const orchard::Mat4& world, const orchard::AxisSystem& axes);
fl::Light to_light(const orchard::Light& light, const orchard::Mat4& world, const orchard::AxisSystem& axes, double seconds);
fl::Material to_material(const orchard::Material& material);
using TextureLoader = std::function<std::shared_ptr<const fl::Texture>(const std::string& path, bool color)>;
fl::Material to_material(const orchard::Material& material, const TextureLoader& textures);
bool apply_setting(fl::LightingSettings& settings, fl::Environment& environment, const std::string& key, const std::string& value, std::string* problem = nullptr);
const std::vector<std::string>& setting_names();
fl::LightingSettings settings_for(const orchard::ProjectInfo& info, orchard::LightingLevel level, std::vector<std::string>* problems = nullptr);
fl::Environment environment_for(const orchard::ProjectInfo& info, std::vector<std::string>* problems = nullptr);

struct Stats {
    std::size_t lights = 0;
    std::size_t objects = 0;
    std::size_t triangles = 0;
    std::size_t instanced = 0;
    std::size_t traced_triangles = 0;
    std::size_t models = 0;
    std::size_t terrains = 0;
    std::size_t scatter = 0;
};

struct ModelPiece {
    fl::Mesh mesh;
    std::size_t material = 0;
};

struct ModelGeometry {
    std::vector<ModelPiece> pieces;
    std::vector<fl::Material> materials;
    std::uint64_t revision = 0;
    bool empty() const noexcept { return pieces.empty(); }
};

using ModelSource = std::function<std::shared_ptr<const ModelGeometry>(orchard::Entity entity, const std::string& source)>;
using TerrainSource = std::function<std::shared_ptr<const orchard::TerrainHeights>(orchard::Entity entity, const orchard::Terrain& terrain)>;

bool models_supported() noexcept;
fl::Material to_material(const fizmo::graphics::ModelMaterial& material, const fizmo::graphics::Model3D& model);
std::shared_ptr<const ModelGeometry> model_geometry(const fizmo::graphics::Model3D& model);
std::shared_ptr<const ModelGeometry> model_geometry(const fizmo::graphics::Model3D& model, const fizmo::graphics::ModelAnimator& pose);
std::shared_ptr<const ModelGeometry> load_model(const std::string& path, bool z_up, std::string* error = nullptr);
void clear_model_cache();

class ModelPoser {
public:
    ModelPoser();
    ~ModelPoser();
    ModelPoser(const ModelPoser&) = delete;
    ModelPoser& operator=(const ModelPoser&) = delete;

    std::shared_ptr<const ModelGeometry> rest(const fizmo::graphics::Model3D& model);
    std::shared_ptr<const ModelGeometry> posed(orchard::Entity entity, const fizmo::graphics::Model3D& model, const std::string& clip, double time);
    void forget(orchard::Entity entity);
    void clear();

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

class World {
public:
    World();
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void build(const orchard::Scene& scene, const orchard::ProjectInfo& info, bool editor = false);
    void sync(const orchard::Scene& scene);
    void animate(const orchard::Scene& scene, double seconds);
    void set_time(double seconds) noexcept { m_time = seconds; }
    double time() const noexcept { return m_time; }
    void set_level(orchard::LightingLevel level);
    orchard::LightingLevel level() const noexcept { return m_level; }
    void set_settings(const fl::LightingSettings& settings);
    void set_texture_loader(TextureLoader loader) { m_textures = std::move(loader); }
    void set_model_source(ModelSource source) { m_model_source = std::move(source); }
    void set_terrain_source(TerrainSource source) { m_terrain_source = std::move(source); }
    void set_asset_root(const std::string& folder) { m_asset_root = folder; }
    const std::string& asset_root() const noexcept { return m_asset_root; }
    void set_geometry(bool on) { m_geometry = on; }
    bool geometry() const noexcept { return m_geometry; }
    std::size_t refresh_geometry(const orchard::Scene& scene);
    const fl::LightingSettings& settings() const noexcept { return m_settings; }
    const std::vector<std::string>& problems() const noexcept { return m_problems; }

    const std::vector<std::uint8_t>& render(const orchard::Viewport& view, unsigned width, unsigned height);
    const std::vector<std::uint8_t>& render(const orchard::Vec3& eye, const orchard::Vec3& target, float fov_degrees, unsigned width, unsigned height);
    const std::vector<std::uint8_t>& pixels() const noexcept { return m_rgba; }
    fl::Camera camera_for(const orchard::Viewport& view, unsigned width, unsigned height) const;

    fl::BakedLighting bake(const fl::BakeOptions& options = fl::BakeOptions());
    void use_bake(std::shared_ptr<const fl::BakedLighting> baked);

    float light_level(const orchard::Vec3& point, const orchard::Vec3& normal);
    float light_level(const orchard::Scene& scene, orchard::Entity e);
    bool lit_by(const orchard::Vec3& point, orchard::Entity light);
    bool has_light(orchard::Entity e) const;

    std::vector<fl::Light> lights() const;
    const fl::Environment& environment() const;
    fl::Scene& lighting_scene() noexcept { return m_scene; }
    fl::Renderer& renderer() noexcept { return m_renderer; }
    const orchard::AxisSystem& axes() const noexcept { return m_axes; }
    Stats stats() const;

private:
    struct Piece;
    void apply();
    fl::Vec3 to_render(const orchard::Vec3& world) const;
    std::shared_ptr<const ModelGeometry> model_for(orchard::Entity e, const std::string& source);
    std::shared_ptr<const orchard::TerrainHeights> heights_for(orchard::Entity e, const orchard::Terrain& t);
    bool add_model(const orchard::Scene& scene, orchard::Entity e, const orchard::Model& model);
    bool add_terrain(const orchard::Scene& scene, orchard::Entity e, const orchard::Terrain& t);
    std::vector<std::size_t> scatter_meshes(const std::string& source, std::vector<std::size_t>& materials);
    bool update_piece(const orchard::Scene& scene, orchard::Entity e, Piece& piece, bool& rebuild);

    fl::Scene m_scene;
    fl::Renderer m_renderer;
    fl::LightingSettings m_settings;
    orchard::AxisSystem m_axes;
    orchard::ProjectInfo m_info;
    orchard::LightingLevel m_level = orchard::LightingLevel::shadowed;
    std::vector<std::string> m_problems;
    std::unordered_map<std::uint64_t, std::size_t> m_light_of;
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> m_instances_of;
    std::unordered_map<std::uint64_t, std::vector<orchard::Mat4>> m_parts_of;
    std::vector<std::uint8_t> m_rgba;
    bool m_custom_settings = false;
    TextureLoader m_textures;
    double m_time = 0.0;
    ModelSource m_model_source;
    TerrainSource m_terrain_source;
    std::string m_asset_root;
    bool m_geometry = true;
    std::unordered_map<std::uint64_t, std::shared_ptr<Piece>> m_pieces;
    struct ScatterMesh {
        std::shared_ptr<const ModelGeometry> geometry;
        std::vector<std::size_t> meshes;
        std::vector<std::size_t> materials;
    };
    std::unordered_map<std::string, ScatterMesh> m_scatter_meshes;
    std::unordered_map<std::uint64_t, std::pair<std::string, std::shared_ptr<const orchard::TerrainHeights>>> m_default_heights;
    std::unordered_map<std::uint64_t, orchard::Mat4> m_world_of;
};

} // namespace chlorophyll

#endif // CHLOROPHYLL_CHLOROPHYLL_HPP
