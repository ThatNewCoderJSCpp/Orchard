#ifndef ORCHARD_EDITOR_GPU_VIEW_HPP
#define ORCHARD_EDITOR_GPU_VIEW_HPP

#include <orchard/scene.hpp>
#include <orchard/viewport.hpp>

#include <grove/canvas.hpp>

#include <memory>

namespace orchard {
namespace editor {

class GpuView {
public:
    GpuView();
    ~GpuView();
    GpuView(const GpuView&) = delete;
    GpuView& operator=(const GpuView&) = delete;

    void draw(grove::GpuCanvasFrame& frame, const Scene& scene, Entity selected, Viewport& view, bool playing);

private:
    struct Meshes;
    std::unique_ptr<Meshes> m_meshes;
};

} // namespace editor
} // namespace orchard

#endif // ORCHARD_EDITOR_GPU_VIEW_HPP
