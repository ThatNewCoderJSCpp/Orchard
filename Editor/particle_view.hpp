#ifndef ORCHARD_EDITOR_PARTICLE_VIEW_HPP
#define ORCHARD_EDITOR_PARTICLE_VIEW_HPP

#include <orchard/scene.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace fizmo {
namespace graphics {
class Renderer;
class Camera3D;
class Texture;
} // namespace graphics
namespace particles {
struct EmitterSettings;
} // namespace particles
} // namespace fizmo

namespace orchard {
namespace editor {

struct ParticleViewStats {
    std::size_t particles = 0;
    std::size_t emitters = 0;
    std::size_t budget = 0;
    double step_ms = 0.0;
    float budget_scale = 1.0f;
};

fizmo::particles::EmitterSettings particle_settings(const Emitter& emitter, bool flat, Vec3 up, int depth = 0);
const fizmo::graphics::Texture* builtin_particle_texture(const std::string& name);

class ParticleView {
public:
    ParticleView();
    ~ParticleView();
    ParticleView(const ParticleView&) = delete;
    ParticleView& operator=(const ParticleView&) = delete;

    void set_budget(int budget, int quality);
    void set_texture_lookup(std::function<const fizmo::graphics::Texture*(const std::string&)> lookup);
    void sync(const Scene& scene, Vec3 up, bool running);
    void command(Entity e, int action, int count);
    void reset();
    void step_3d(double dt, const fizmo::graphics::Camera3D& camera);
    void step_2d(double dt, float left, float bottom, float right, float top);
    std::size_t draw_3d(fizmo::graphics::Renderer& renderer);
    std::size_t draw_2d(fizmo::graphics::Renderer& renderer, const std::function<void(float x, float y, float& sx, float& sy)>& to_screen);
    ParticleViewStats stats() const;
    std::size_t emitter_count() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

} // namespace editor
} // namespace orchard

#endif // ORCHARD_EDITOR_PARTICLE_VIEW_HPP
