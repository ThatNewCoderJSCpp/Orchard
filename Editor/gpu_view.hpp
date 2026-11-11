#ifndef ORCHARD_EDITOR_GPU_VIEW_HPP
#define ORCHARD_EDITOR_GPU_VIEW_HPP

#include <orchard/scene.hpp>
#include <orchard/viewport.hpp>
#include <xylem/xylem.hpp>

#include "particle_view.hpp"
#include "model_view.hpp"
#include "terrain_view.hpp"

#include <grove/canvas.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace fizmo {
namespace lighting {
class Scene;
struct LightingSettings;
struct Camera;
class Texture;
} // namespace lighting
} // namespace fizmo

namespace orchard {
namespace editor {

struct GpuViewOptions {
    bool culling = true;
    bool occlusion = true;
    bool lod = true;
    bool batching = true;
    bool shadows = true;
    double shadow_distance = 48.0;
    double blob_distance = 160.0;
    bool move_gizmo = false;
    bool rotate_gizmo = false;
    bool scale_gizmo = false;
    int hot_axis = -1;
    bool icons = true;
    bool orientation_gizmo = true;
    bool scene_lights = true;
    bool materials = true;
    bool particles = true;
    bool particles_running = false;
    int particle_budget = 60000;
    int particle_quality = 1;
    bool show_bones = true;
    bool ambient_occlusion = true;
    int lighting_quality = 1;
    bool overlay_only = false;
};

struct GpuViewStats {
    std::size_t items = 0;
    std::size_t drawn = 0;
    std::size_t culled = 0;
    std::size_t occluded = 0;
    std::size_t batches = 0;
    std::size_t instanced = 0;
    std::size_t real_shadows = 0;
    std::size_t blob_shadows = 0;
};

class GpuView {
public:
    GpuView();
    ~GpuView();
    GpuView(const GpuView&) = delete;
    GpuView& operator=(const GpuView&) = delete;

    void draw(grove::GpuCanvasFrame& frame, const Scene& scene, Entity selected, Viewport& view, bool playing);
    void draw(grove::GpuCanvasFrame& frame, const Scene& scene, Entity selected, Viewport& view, bool playing, double seconds);
    bool draw_traced(grove::GpuCanvasFrame& frame, fizmo::lighting::Scene& scene, const fizmo::lighting::LightingSettings& settings, const fizmo::lighting::Camera& camera);
    bool draw_traced_over(grove::GpuCanvasFrame& frame, fizmo::lighting::Scene& scene, const fizmo::lighting::LightingSettings& settings, const fizmo::lighting::Camera& camera, std::uint32_t full_after_samples);
    std::uint32_t traced_samples() const;
    void set_trace_samples(int samples);
    std::string trace_error() const;
    void stop_tracing();
    void set_options(const GpuViewOptions& options) { m_options = options; }
    void set_lights(const fizmo::lighting::Scene* lights);
    void set_fluids(const std::vector<xylem::FluidDraw>* fluids);
    void set_texture_folder(const std::string& folder);
    std::function<std::shared_ptr<const fizmo::lighting::Texture>(const std::string& path, bool color)> lighting_textures();
    ParticleView& particles() noexcept { return m_particles; }
    ModelView& models() noexcept { return m_models; }
    TerrainView& terrain() noexcept { return m_terrain; }
    void set_terrain_brush(Entity terrain, Vec3 point, float radius, bool visible) {
        m_brush_entity = terrain;
        m_brush_point = point;
        m_brush_radius = radius;
        m_brush_visible = visible;
    }
    void set_extra_models(const std::vector<ModelInstance>* instances) { m_extra_models = instances; }
    void set_also_selected(const std::vector<Entity>& also) { m_also_selected = also; }
    const GpuViewOptions& options() const noexcept { return m_options; }
    const GpuViewStats& stats() const noexcept { return m_stats; }

private:
    bool prepare_tracer(grove::GpuCanvasFrame& frame, fizmo::lighting::Scene& scene, const fizmo::lighting::LightingSettings& settings, const fizmo::lighting::Camera& camera);

    GpuViewOptions m_options;
    GpuViewStats m_stats;
    struct Meshes;
    std::unique_ptr<Meshes> m_meshes;
    ParticleView m_particles;
    ModelView m_models;
    std::vector<Entity> m_also_selected;
    const std::vector<ModelInstance>* m_extra_models = nullptr;
    TerrainView m_terrain;
    std::vector<ModelInstance> m_scatter_models;
    std::vector<std::size_t> m_scatter_visible;
    Entity m_brush_entity;
    Vec3 m_brush_point;
    float m_brush_radius = 0.0f;
    bool m_brush_visible = false;
};

} // namespace editor
} // namespace orchard

#endif // ORCHARD_EDITOR_GPU_VIEW_HPP
