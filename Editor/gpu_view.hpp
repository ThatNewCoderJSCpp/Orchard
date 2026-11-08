#ifndef ORCHARD_EDITOR_GPU_VIEW_HPP
#define ORCHARD_EDITOR_GPU_VIEW_HPP

#include <orchard/scene.hpp>
#include <orchard/viewport.hpp>

#include <grove/canvas.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace fizmo {
namespace lighting {
class Scene;
struct LightingSettings;
struct Camera;
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
    std::uint32_t traced_samples() const;
    std::string trace_error() const;
    void stop_tracing();
    void set_options(const GpuViewOptions& options) { m_options = options; }
    const GpuViewOptions& options() const noexcept { return m_options; }
    const GpuViewStats& stats() const noexcept { return m_stats; }

private:
    GpuViewOptions m_options;
    GpuViewStats m_stats;
    struct Meshes;
    std::unique_ptr<Meshes> m_meshes;
};

} // namespace editor
} // namespace orchard

#endif // ORCHARD_EDITOR_GPU_VIEW_HPP
