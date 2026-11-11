#ifndef ORCHARD_SOLID_HPP
#define ORCHARD_SOLID_HPP

#include <orchard/math.hpp>
#include <orchard/scene.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace orchard {

struct SolidMesh {
    std::vector<Vec3> positions;
    std::vector<int> colors;
    std::vector<Vec3> palette;

    std::size_t triangles() const { return colors.size(); }
    bool empty() const { return colors.empty(); }
    void add_triangle(Vec3 a, Vec3 b, Vec3 c, int color = 0);
    int add_color(Vec3 color);
    void append(const SolidMesh& other);
    void transform(const Mat4& m);
    void set_color(Vec3 color);
    bool bounds(Vec3& lo, Vec3& hi) const;
    float volume() const;
};

using SolidMeshLoader = std::function<bool(const std::string& source, std::vector<Vec3>& vertices, std::vector<std::uint32_t>& indices)>;

SolidMesh solid_box(Vec3 size);
SolidMesh solid_sphere(float diameter, int rings = 16, int segments = 24);
SolidMesh solid_cylinder(float diameter, float height, float top = 1.0f, int segments = 24);
SolidMesh solid_capsule(float diameter, float height, int rings = 16, int segments = 24);

SolidMesh solid_union(const SolidMesh& a, const SolidMesh& b);
SolidMesh solid_subtract(const SolidMesh& a, const SolidMesh& b);
SolidMesh solid_intersect(const SolidMesh& a, const SolidMesh& b);

std::optional<SolidMesh> collider_solid(const Collider& c, const SolidMeshLoader& loader = {}, std::string* why = nullptr);
std::optional<SolidMesh> entity_solid(const Scene& scene, Entity e, const Mat4& into, const SolidMeshLoader& loader = {}, std::string* why = nullptr);

bool write_solid_obj(const SolidMesh& mesh, const std::string& obj_path, bool z_up, bool with_colors, std::string* error = nullptr);

Transform transform_from_matrix(const Mat4& m);

struct SolidOptions {
    std::string project_root;
    std::string folder = "Assets/Solids";
    bool z_up = false;
    bool one_color = false;
    SolidMeshLoader loader;
};

struct SolidResult {
    Entity entity;
    std::vector<Entity> entities;
    std::string mesh_file;
    std::string error;
    std::vector<std::string> notes;

    bool ok() const { return error.empty(); }
};

bool is_negative_part(const Scene& scene, Entity e);
SolidResult combine_solids(Scene& scene, const std::vector<Entity>& parts, SolidOperation operation, const SolidOptions& options);
SolidResult separate_solid(Scene& scene, Entity combined, const SolidOptions& options);
SolidResult rebuild_solid(Scene& scene, Entity combined, const SolidOptions& options);

} // namespace orchard

#endif // ORCHARD_SOLID_HPP
