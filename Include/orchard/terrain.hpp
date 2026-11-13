#ifndef ORCHARD_TERRAIN_HPP
#define ORCHARD_TERRAIN_HPP

#include "math.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace orchard {

struct TerrainLayer {
    std::string texture;
    std::string normal_map;
    Vec3 color{0.08f, 0.16f, 0.045f};
    float tiling = 8.0f;
    float min_height = 0.0f;
    float max_height = 1.0f;
    float max_slope = 90.0f;
    float blend = 0.1f;
    float min_slope = 0.0f;
    std::string surface;
};

struct TerrainScatter {
    std::string model;
    float density = 0.02f;
    float min_scale = 0.8f;
    float max_scale = 1.2f;
    float min_height = 0.0f;
    float max_height = 1.0f;
    float max_slope = 30.0f;
    bool align_to_slope = false;
    bool collide = true;
    float draw_distance = 300.0f;
    std::uint32_t seed = 1;
    std::string surface;
};

struct Terrain {
    Vec3 size{100.0f, 20.0f, 100.0f};
    int resolution = 129;
    std::string heightmap;
    std::string data;
    std::string generator = "hills";
    std::uint32_t seed = 1;
    float roughness = 0.5f;
    float friction = 0.6f;
    float restitution = 0.0f;
    std::vector<TerrainLayer> layers;
    std::vector<TerrainScatter> scatter;
};

std::string terrain_lines(std::size_t n, const Terrain& t);
bool is_terrain_line(const std::string& kind);
bool read_terrain_line(const std::string& kind, std::istringstream& row, Terrain& t, std::string* error = nullptr);

constexpr int terrain_min_resolution = 2;
constexpr int terrain_max_resolution = 4097;

int clamp_terrain_resolution(int resolution);
const std::vector<std::string>& terrain_generators();
Terrain terrain_preset(const std::string& name);
const std::vector<std::string>& terrain_presets();

struct TerrainDirty {
    int x0 = 0;
    int z0 = 0;
    int x1 = 0;
    int z1 = 0;
    std::uint64_t revision = 0;
};

struct TerrainHeights {
    int width = 0;
    int depth = 0;
    Vec3 size{100.0f, 20.0f, 100.0f};
    std::vector<float> heights;
    std::uint64_t revision = 1;
    std::uint64_t oldest_dirty = 1;
    std::vector<TerrainDirty> dirty;

    bool valid() const;
    void resize(int w, int d, float fill = 0.0f);
    void set_size(Vec3 s);
    float at(int x, int z) const;
    void set(int x, int z, float h);
    float spacing_x() const;
    float spacing_z() const;
    Vec2 to_grid(float x, float z) const;
    Vec3 local_position(int x, int z) const;
    float height_at(float x, float z) const;
    Vec3 normal_at(float x, float z) const;
    Vec3 grid_normal(int x, int z) const;
    float slope_at(float x, float z) const;
    float min_height() const;
    float max_height() const;
    bool contains(float x, float z) const;
    void touch(int x0, int z0, int x1, int z1);
    void touch_all();
    std::vector<std::uint8_t> encode() const;
    bool decode(const std::uint8_t* data, std::size_t size, std::string* error = nullptr);
    bool save(const std::string& path, std::string* error = nullptr) const;
    bool load(const std::string& path, std::string* error = nullptr);
    void resample(int w, int d);
};

bool raycast_terrain(const TerrainHeights& h, Vec3 origin, Vec3 direction, float max_distance, float& distance, Vec3* point = nullptr, Vec3* normal = nullptr);

void generate_heights(const Terrain& t, TerrainHeights& out);
float terrain_noise(float x, float z, std::uint32_t seed);
float terrain_fbm(float x, float z, std::uint32_t seed, int octaves, float gain, float lacunarity = 2.0f);

struct HeightImage {
    int width = 0;
    int height = 0;
    int bits = 8;
    std::vector<float> values;
};

using HeightImageDecoder = std::function<bool(const std::string& path, HeightImage& out, std::string* error)>;

void set_height_image_decoder(HeightImageDecoder decoder);
bool decode_height_image(const std::uint8_t* data, std::size_t size, const std::string& name, HeightImage& out, std::string* error = nullptr);
bool load_height_image(const std::string& path, HeightImage& out, std::string* error = nullptr);
void heights_from_image(const HeightImage& image, const Terrain& t, TerrainHeights& out);
bool load_heightmap(const Terrain& t, const std::string& path, TerrainHeights& out, std::string* error = nullptr);
std::string terrain_file_path(const std::string& root, const std::string& file);
bool build_terrain_heights(const Terrain& t, const std::string& root, TerrainHeights& out, std::string* error = nullptr);
std::string terrain_signature(const Terrain& t);
std::string default_terrain_data_path(const std::string& entity_name);

enum class TerrainBrushKind {
    raise,
    lower,
    smooth,
    flatten,
    noise,
    set_height
};

const char* to_string(TerrainBrushKind kind);
std::optional<TerrainBrushKind> parse_terrain_brush(const std::string& text);
const std::vector<TerrainBrushKind>& terrain_brushes();

struct TerrainBrush {
    TerrainBrushKind kind = TerrainBrushKind::raise;
    float radius = 5.0f;
    float strength = 0.5f;
    float falloff = 0.5f;
    float target = 0.0f;
    std::uint32_t seed = 1;
};

float brush_weight(const TerrainBrush& b, float distance);
bool apply_brush(TerrainHeights& h, const TerrainBrush& brush, float x, float z, float dt = 1.0f);

Quat terrain_frame(Vec3 up);
Mat4 terrain_matrix(const Mat4& entity_world, Vec3 up);
Vec3 terrain_to_local(const Mat4& terrain_world, Vec3 world_point);

Vec4 terrain_layer_weights(const Terrain& t, float height01, float slope_degrees);
Vec3 terrain_layer_color(const Terrain& t, float height01, float slope_degrees, const std::vector<Vec3>* layer_tints = nullptr);

constexpr int terrain_max_splat_layers = 8;

void terrain_layer_weights(const Terrain& t, float height01, float slope_degrees, float* out, std::size_t count);

struct TerrainSplat {
    int width = 0;
    int height = 0;
    int layers = 0;
    std::vector<std::uint8_t> rgba[2];
};

int terrain_splat_size(int grid);
void build_terrain_splat(const Terrain& t, const TerrainHeights& h, TerrainSplat& out, int width = 0, int height = 0);
bool update_terrain_splat(const Terrain& t, const TerrainHeights& h, TerrainSplat& out, int grid_x0, int grid_z0, int grid_x1, int grid_z1, int* px0 = nullptr, int* py0 = nullptr, int* px1 = nullptr, int* py1 = nullptr);

struct TerrainMesh {
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;
    std::vector<Vec4> weights;
    std::vector<Vec3> colors;
    std::vector<std::uint32_t> indices;
    Vec3 bounds_min;
    Vec3 bounds_max;

    void clear();
    std::size_t vertex_count() const { return positions.size(); }
    std::size_t triangle_count() const { return indices.size() / 3; }
};

struct TerrainChunking {
    int chunk_quads = 32;
    int chunks_x = 0;
    int chunks_z = 0;
    int lods = 1;
};

struct TerrainMeshOptions {
    int chunk_quads = 32;
    bool skirts = true;
    float skirt_depth = 0.0f;
    float uv_tiling = 0.0f;
    const std::vector<Vec3>* layer_tints = nullptr;
};

TerrainChunking terrain_chunking(const TerrainHeights& h, int chunk_quads = 32);
void terrain_chunk_bounds(const TerrainHeights& h, int cx, int cz, int chunk_quads, Vec3& lo, Vec3& hi);
void build_terrain_chunk(const Terrain& t, const TerrainHeights& h, int cx, int cz, int lod, TerrainMesh& out, const TerrainMeshOptions& options = {});
void build_terrain_mesh(const Terrain& t, const TerrainHeights& h, TerrainMesh& out, int lod = 0, const TerrainMeshOptions& options = {});

struct ScatterInstance {
    std::string model;
    std::uint32_t scatter = 0;
    Vec3 position;
    Quat rotation;
    float yaw = 0.0f;
    float scale = 1.0f;
    bool collide = true;
    float draw_distance = 300.0f;
    std::string surface;
};

std::vector<ScatterInstance> scatter_instances(const Terrain& t, const TerrainHeights& h, std::size_t limit = 250000);
Mat4 scatter_matrix(const Mat4& terrain_world, const ScatterInstance& s);

} // namespace orchard

#endif // ORCHARD_TERRAIN_HPP
