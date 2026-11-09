#include "terrain_view.hpp"
#include "particle_textures.hpp"

#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace orchard {
namespace editor {

namespace fg = fizmo::graphics;
using fizmo::colors::Color;
using fizmo::linalg::vector3d;

namespace {

fizmo::linalg::Matrix4d to_fizmo(const Mat4& m) {
    return fizmo::linalg::Matrix4d{
        m.at(0, 0), m.at(0, 1), m.at(0, 2), m.at(0, 3),
        m.at(1, 0), m.at(1, 1), m.at(1, 2), m.at(1, 3),
        m.at(2, 0), m.at(2, 1), m.at(2, 2), m.at(2, 3),
        m.at(3, 0), m.at(3, 1), m.at(3, 2), m.at(3, 3)};
}

std::uint8_t to_byte(float linear) {
    const float c = std::clamp(linear, 0.0f, 1.0f);
    const float s = c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    return static_cast<std::uint8_t>(std::lround(std::clamp(s, 0.0f, 1.0f) * 255.0f));
}

float to_linear(std::uint8_t b) {
    const float s = static_cast<float>(b) / 255.0f;
    return s <= 0.04045f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
}

vector3d dv(Vec3 v) {
    return vector3d{v.x, v.y, v.z};
}

std::string fnum(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6g", static_cast<double>(v));
    return buf;
}

std::string layer_key(const Terrain& t) {
    std::string k;
    for (const TerrainLayer& l : t.layers) {
        k += l.texture + "|" + l.normal_map + "|" + fnum(l.color.x) + "," + fnum(l.color.y) + "," + fnum(l.color.z) + "|" + fnum(l.tiling) + "|" + fnum(l.min_height) + "|" + fnum(l.max_height) + "|" + fnum(l.min_slope) + "|" + fnum(l.max_slope) + "|" + fnum(l.blend) + ";";
    }
    return k;
}

std::string scatter_key(const Terrain& t) {
    std::string k = std::to_string(t.seed);
    for (const TerrainScatter& s : t.scatter) {
        k += "|" + s.model + "|" + fnum(s.density) + "|" + fnum(s.min_scale) + "|" + fnum(s.max_scale) + "|" + fnum(s.min_height) + "|" + fnum(s.max_height) + "|" + fnum(s.max_slope) + "|" + (s.align_to_slope ? "a" : "-") + (s.collide ? "c" : "-") + "|" + fnum(s.draw_distance) + "|" + std::to_string(s.seed);
    }
    return k;
}

std::string gen_key(const Terrain& t) {
    return t.generator + "|" + std::to_string(t.seed) + "|" + fnum(t.roughness);
}

std::string shape_key(const Terrain& t) {
    return fnum(t.size.x) + "," + fnum(t.size.y) + "," + fnum(t.size.z) + "|" + std::to_string(clamp_terrain_resolution(t.resolution));
}

bool same(const Mat4& a, const Mat4& b) {
    for (int i = 0; i < 16; ++i)
        if (a.m[i] != b.m[i]) return false;
    return true;
}

Vec3 column(const Mat4& m, int c) {
    return {m.at(0, c), m.at(1, c), m.at(2, c)};
}

struct Chunk {
    int cx = 0;
    int cz = 0;
    Vec3 lo;
    Vec3 hi;
    bool bounds_stale = true;
    std::vector<std::unique_ptr<fg::Mesh3D>> lods;
    std::vector<char> stale;
    std::vector<std::uint64_t> used;
};

struct Entry {
    std::shared_ptr<TerrainHeights> heights;
    Terrain terrain;
    std::string gen;
    std::string map;
    std::string data;
    std::string shape;
    std::string look;
    std::string scatter_look;
    std::uint64_t seen_revision = 0;
    std::uint64_t scatter_revision = 0;
    bool unsaved = false;
    bool alive = true;
    int grid_w = 0;
    int grid_d = 0;
    TerrainChunking chunking;
    std::vector<Chunk> chunks;
    std::vector<Vec3> tints;
    const fg::Texture* base_texture = nullptr;
    std::vector<const fg::Texture*> layer_textures;
    std::vector<const fg::Texture*> layer_normals;
    bool use_splat = false;
    std::string splat_key;
    TerrainSplat splat;
    std::shared_ptr<fizmo::images::BitmapImage> splat_images[2];
    fg::Texture splat_textures[2];
    std::vector<ScatterInstance> scatter;
    Mat4 placed;
    bool placed_valid = false;
};

} // namespace

struct TerrainView::Impl {
    std::string root;
    Vec3 up{0.0f, 1.0f, 0.0f};
    std::function<const fg::Texture*(const std::string&)> lookup;
    float lod_distance = 1.0f;
    std::size_t budget = 48;
    bool wire_selected = true;
    std::unordered_map<std::uint64_t, Entry> entries;
    std::vector<ScatterInstance> all_instances;
    std::vector<TerrainScatterDraw> draws;
    bool draws_stale = true;
    TerrainViewStats stats;
    std::vector<std::string> warnings;
    std::map<const fg::Texture*, Vec3> averages;
    std::unordered_map<std::string, std::unique_ptr<fg::Texture>> builtin_albedo;
    std::unordered_map<std::string, std::unique_ptr<fg::Texture>> builtin_normal;
    std::uint64_t frame = 0;
    int chunk_quads = 32;

    void warn(const std::string& w) {
        if (std::find(warnings.begin(), warnings.end(), w) != warnings.end()) return;
        if (warnings.size() >= 32) warnings.erase(warnings.begin());
        warnings.push_back(w);
    }

    std::string path(const std::string& file) const {
        return terrain_file_path(root, file);
    }

    Mat4 world_of(const Scene& scene, Entity e) const {
        return terrain_matrix(scene.world_matrix(e), up);
    }

    static std::unique_ptr<fg::Texture> texture_of(const ParticlePixels& p) {
        if (p.width <= 0 || p.height <= 0) return nullptr;
        fizmo::images::BitmapImage img(static_cast<unsigned>(p.width), static_cast<unsigned>(p.height));
        std::vector<Color>& px = img.mutable_pixels();
        const std::size_t n = std::min(px.size(), p.rgba.size() / 4);
        for (std::size_t i = 0; i < n; ++i) px[i] = Color(p.rgba[i * 4], p.rgba[i * 4 + 1], p.rgba[i * 4 + 2], p.rgba[i * 4 + 3]);
        auto tex = std::make_unique<fg::Texture>(std::move(img), fg::SampleFilter::Bilinear, fg::WrapMode::Repeat);
        tex->set_mipmaps(true);
        return tex;
    }

    const fg::Texture* builtin(const std::string& name, bool normal) {
        if (!is_builtin_terrain_texture(name)) return nullptr;
        auto& cache = normal ? builtin_normal : builtin_albedo;
        auto it = cache.find(name);
        if (it != cache.end()) return it->second.get();
        TerrainPixels px = generate_builtin_terrain(name);
        builtin_albedo[name] = texture_of(px.albedo);
        builtin_normal[name] = texture_of(px.normal);
        return (normal ? builtin_normal : builtin_albedo)[name].get();
    }

    const fg::Texture* resolve(const std::string& name, bool normal) {
        if (name.empty()) return nullptr;
        if (const fg::Texture* b = builtin(name, normal)) return b;
        const fg::Texture* t = lookup ? lookup(name) : nullptr;
        return t && t->image() ? t : nullptr;
    }

    void push_splat(Entry& en, int x0, int y0, int x1, int y1) {
        for (int k = 0; k < 2; ++k) {
            const std::vector<std::uint8_t>& src = en.splat.rgba[k];
            if (src.empty()) continue;
            std::shared_ptr<fizmo::images::BitmapImage>& img = en.splat_images[k];
            const bool fresh = !img || img->width() != static_cast<unsigned>(en.splat.width) || img->height() != static_cast<unsigned>(en.splat.height);
            if (fresh) {
                img = std::make_shared<fizmo::images::BitmapImage>(static_cast<unsigned>(en.splat.width), static_cast<unsigned>(en.splat.height));
                x0 = 0;
                y0 = 0;
                x1 = en.splat.width - 1;
                y1 = en.splat.height - 1;
            }
            std::vector<Color>& px = img->mutable_pixels();
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) {
                    const std::size_t i = static_cast<std::size_t>(y) * static_cast<std::size_t>(en.splat.width) + static_cast<std::size_t>(x);
                    px[i] = Color(src[i * 4], src[i * 4 + 1], src[i * 4 + 2], src[i * 4 + 3]);
                }
            if (fresh) en.splat_textures[k] = fg::Texture(std::shared_ptr<const fizmo::images::BitmapImage>(img), fg::SampleFilter::Bilinear, fg::WrapMode::Clamp);
        }
    }

    void rebuild_splat(Entry& en) {
        if (!en.use_splat || !en.heights || !en.heights->valid()) return;
        const TerrainHeights& h = *en.heights;
        const std::string key = en.look + "|" + std::to_string(h.width) + "x" + std::to_string(h.depth) + "|" + shape_key(en.terrain);
        if (key == en.splat_key && en.splat.width > 0) return;
        en.splat_key = key;
        build_terrain_splat(en.terrain, h, en.splat);
        if (en.splat.layers <= 4) {
            en.splat_images[1].reset();
            en.splat_textures[1] = fg::Texture();
        }
        push_splat(en, 0, 0, en.splat.width - 1, en.splat.height - 1);
    }

    void touch_splat(Entry& en, int x0, int z0, int x1, int z1) {
        if (!en.use_splat || en.splat.width <= 0) return;
        int px0 = 0, py0 = 0, px1 = 0, py1 = 0;
        if (update_terrain_splat(en.terrain, *en.heights, en.splat, x0, z0, x1, z1, &px0, &py0, &px1, &py1)) push_splat(en, px0, py0, px1, py1);
    }

    Vec3 average(const fg::Texture* tex) {
        auto it = averages.find(tex);
        if (it != averages.end()) return it->second;
        Vec3 sum{0.0f, 0.0f, 0.0f};
        const fizmo::images::BitmapImage* img = tex ? tex->image() : nullptr;
        float n = 0.0f;
        if (img && img->width() > 0 && img->height() > 0) {
            const unsigned w = img->width(), h = img->height();
            const unsigned sx = std::max(1u, w / 32), sy = std::max(1u, h / 32);
            for (unsigned y = 0; y < h; y += sy)
                for (unsigned x = 0; x < w; x += sx) {
                    const Color c = img->get_pixel(x, y);
                    sum += Vec3{to_linear(c.red()), to_linear(c.green()), to_linear(c.blue())};
                    n += 1.0f;
                }
        }
        const Vec3 avg = n > 0.0f ? sum * (1.0f / n) : Vec3{1.0f, 1.0f, 1.0f};
        if (img) averages[tex] = avg;
        return avg;
    }

    void update_look(Entry& en) {
        const Terrain& t = en.terrain;
        en.base_texture = nullptr;
        std::vector<Vec3> tints(t.layers.size(), Vec3{1.0f, 1.0f, 1.0f});
        std::string key = layer_key(t);
        std::vector<const fg::Texture*> texes(t.layers.size(), nullptr);
        std::vector<const fg::Texture*> normals(t.layers.size(), nullptr);
        bool any = false;
        for (std::size_t i = 0; i < t.layers.size(); ++i) {
            texes[i] = resolve(t.layers[i].texture, false);
            normals[i] = !t.layers[i].normal_map.empty() ? resolve(t.layers[i].normal_map, false) : builtin(t.layers[i].texture, true);
            key += texes[i] ? "T" : "-";
            key += normals[i] ? "N" : "-";
            any = any || texes[i];
        }
        const bool splat = any && t.layers.size() <= static_cast<std::size_t>(terrain_max_splat_layers);
        key += splat ? "S" : "V";
        en.use_splat = splat;
        en.layer_textures = texes;
        en.layer_normals = normals;
        if (splat) {
            for (std::size_t i = 0; i < t.layers.size(); ++i)
                if (texes[i]) tints[i] = average(texes[i]);
        }
        if (splat) {
        } else if (!texes.empty() && texes[0]) {
            en.base_texture = texes[0];
            const Vec3 a0 = average(texes[0]);
            const Vec3 inv{1.0f / std::max(0.05f, a0.x), 1.0f / std::max(0.05f, a0.y), 1.0f / std::max(0.05f, a0.z)};
            for (std::size_t i = 1; i < t.layers.size(); ++i) tints[i] = (texes[i] ? average(texes[i]) : Vec3{1.0f, 1.0f, 1.0f}) * inv;
        } else {
            for (std::size_t i = 0; i < t.layers.size(); ++i)
                if (texes[i]) tints[i] = average(texes[i]);
        }
        if (key != en.look) {
            en.look = key;
            en.tints = std::move(tints);
            for (Chunk& c : en.chunks) std::fill(c.stale.begin(), c.stale.end(), 1);
        }
        rebuild_splat(en);
    }

    void layout(Entry& en) {
        const TerrainHeights& h = *en.heights;
        if (en.grid_w == h.width && en.grid_d == h.depth && !en.chunks.empty()) return;
        en.grid_w = h.width;
        en.grid_d = h.depth;
        en.chunking = terrain_chunking(h, chunk_quads);
        en.chunks.clear();
        en.chunks.resize(static_cast<std::size_t>(std::max(0, en.chunking.chunks_x * en.chunking.chunks_z)));
        for (int z = 0; z < en.chunking.chunks_z; ++z)
            for (int x = 0; x < en.chunking.chunks_x; ++x) {
                Chunk& c = en.chunks[static_cast<std::size_t>(z * en.chunking.chunks_x + x)];
                c.cx = x;
                c.cz = z;
                c.lods.resize(static_cast<std::size_t>(en.chunking.lods));
                c.stale.assign(static_cast<std::size_t>(en.chunking.lods), 1);
                c.used.assign(static_cast<std::size_t>(en.chunking.lods), 0);
            }
        en.seen_revision = h.revision;
    }

    void refresh(Entry& en) {
        if (!en.heights || !en.heights->valid()) return;
        layout(en);
        TerrainHeights& h = *en.heights;
        if (en.seen_revision == h.revision) return;
        const int cq = en.chunking.chunk_quads;
        auto mark = [&](int x0, int z0, int x1, int z1) {
            const int c0 = std::max(0, (x0 - 1) / cq), c1 = std::min(en.chunking.chunks_x - 1, (x1 + 1) / cq);
            const int r0 = std::max(0, (z0 - 1) / cq), r1 = std::min(en.chunking.chunks_z - 1, (z1 + 1) / cq);
            for (int z = r0; z <= r1; ++z)
                for (int x = c0; x <= c1; ++x) {
                    Chunk& c = en.chunks[static_cast<std::size_t>(z * en.chunking.chunks_x + x)];
                    std::fill(c.stale.begin(), c.stale.end(), 1);
                    c.bounds_stale = true;
                }
        };
        if (en.seen_revision + 1 < h.oldest_dirty || en.seen_revision == 0) {
            mark(0, 0, h.width - 1, h.depth - 1);
            touch_splat(en, 0, 0, h.width - 1, h.depth - 1);
        } else {
            for (const TerrainDirty& d : h.dirty)
                if (d.revision > en.seen_revision) {
                    mark(d.x0, d.z0, d.x1, d.z1);
                    touch_splat(en, d.x0, d.z0, d.x1, d.z1);
                }
        }
        en.seen_revision = h.revision;
    }

    void update_scatter(Entry& en) {
        const std::string key = scatter_key(en.terrain);
        if (key == en.scatter_look && en.scatter_revision == en.heights->revision) return;
        en.scatter_look = key;
        en.scatter_revision = en.heights->revision;
        en.scatter = scatter_instances(en.terrain, *en.heights);
        draws_stale = true;
    }

    void set_keys(Entry& en, const Terrain& t) {
        en.gen = gen_key(t);
        en.map = t.heightmap;
        en.data = t.data;
        en.shape = shape_key(t);
    }

    void update_heights(Entry& en, const Terrain& t, const std::string& name) {
        if (!en.heights) {
            en.heights = std::make_shared<TerrainHeights>();
            std::string why;
            if (!build_terrain_heights(t, root, *en.heights, &why) && !why.empty()) warn(name + ": " + why);
            en.unsaved = false;
            set_keys(en, t);
            return;
        }
        TerrainHeights& h = *en.heights;
        bool rebuilt = false;
        if (gen_key(t) != en.gen) {
            generate_heights(t, h);
            en.unsaved = true;
            rebuilt = true;
        } else if (t.heightmap != en.map && !t.heightmap.empty()) {
            std::string why;
            if (load_heightmap(t, path(t.heightmap), h, &why)) {
                en.unsaved = true;
                rebuilt = true;
            } else {
                warn(name + ": " + why);
            }
        } else if (t.data != en.data && !t.data.empty() && !en.unsaved) {
            TerrainHeights loaded;
            if (loaded.load(path(t.data))) {
                h = std::move(loaded);
                h.touch_all();
            }
        }
        if (!rebuilt && shape_key(t) != en.shape) {
            const int res = clamp_terrain_resolution(t.resolution);
            const float old_y = h.size.y;
            h.resample(res, res);
            if (std::fabs(old_y) > 1e-6f && std::fabs(old_y - t.size.y) > 1e-6f)
                for (float& v : h.heights) v *= t.size.y / old_y;
            h.set_size(t.size);
            en.unsaved = true;
        }
        set_keys(en, t);
    }

    void rebuild_draws(const Scene& scene) {
        bool moved = draws_stale;
        for (auto& kv : entries) {
            Entry& en = kv.second;
            const Entity e = Entity::from_id(kv.first);
            const Mat4 w = world_of(scene, e);
            if (!en.placed_valid || !same(w, en.placed)) {
                en.placed = w;
                en.placed_valid = true;
                moved = true;
            }
        }
        if (!moved) return;
        draws_stale = false;
        all_instances.clear();
        draws.clear();
        std::vector<std::pair<Entity, const Entry*>> order;
        for (Entity e : scene.entities()) {
            auto it = entries.find(e.id());
            if (it != entries.end()) order.emplace_back(e, &it->second);
        }
        std::size_t total = 0;
        for (const auto& o : order) total += o.second->scatter.size();
        all_instances.reserve(total);
        for (const auto& o : order) all_instances.insert(all_instances.end(), o.second->scatter.begin(), o.second->scatter.end());
        draws.reserve(total);
        const Quat frame = terrain_frame(up);
        const Mat4 unimport = rotation(Quat{-frame.x, -frame.y, -frame.z, frame.w});
        std::size_t at = 0;
        for (const auto& o : order) {
            for (std::size_t i = 0; i < o.second->scatter.size(); ++i, ++at) {
                const ScatterInstance& s = all_instances[at];
                TerrainScatterDraw d;
                d.terrain = o.first;
                d.scatter = s.scatter;
                d.model = &s.model;
                d.matrix = scatter_matrix(o.second->placed, s) * unimport;
                d.position = mul_point(o.second->placed, s.position);
                d.scale = s.scale;
                d.draw_distance = s.draw_distance;
                draws.push_back(d);
            }
        }
    }

    static float splat_span(const Entry& en) {
        return std::max(1e-3f, std::fabs(en.heights ? en.heights->size.x : en.terrain.size.x));
    }

    void splat_material(const Entry& en, fg::Material3D& mat) const {
        if (!en.use_splat || !en.splat_textures[0].valid()) return;
        const Terrain& t = en.terrain;
        const float span = splat_span(en);
        const float depth = std::max(1e-3f, std::fabs(en.heights->size.z));
        mat.texture = nullptr;
        mat.with_splat_map(en.splat_textures[0], en.splat.layers > 4 && en.splat_textures[1].valid() ? &en.splat_textures[1] : nullptr);
        for (std::size_t i = 0; i < t.layers.size() && i < static_cast<std::size_t>(terrain_max_splat_layers); ++i) {
            const TerrainLayer& l = t.layers[i];
            const fg::Texture* tex = i < en.layer_textures.size() ? en.layer_textures[i] : nullptr;
            const fg::Texture* nrm = tex && i < en.layer_normals.size() ? en.layer_normals[i] : nullptr;
            mat.with_layer(static_cast<int>(i), tex, span / std::max(0.05f, l.tiling), Color(to_byte(l.color.x), to_byte(l.color.y), to_byte(l.color.z)), nrm);
        }
        mat.with_height_blend(0.22f);
        mat.with_splat_uv(fg::UvTransform::tiling(1.0f, span / depth));
        mat.with_splat_triplanar(1.0f / span, 6.0f);
    }

    const fg::Mesh3D* build(Entry& en, Chunk& c, int lod) {
        TerrainMesh tm;
        TerrainMeshOptions o;
        o.chunk_quads = en.chunking.chunk_quads;
        o.layer_tints = &en.tints;
        if (en.use_splat) o.uv_tiling = splat_span(en);
        build_terrain_chunk(en.terrain, *en.heights, c.cx, c.cz, lod, tm, o);
        std::unique_ptr<fg::Mesh3D>& slot = c.lods[static_cast<std::size_t>(lod)];
        if (!slot) slot = std::make_unique<fg::Mesh3D>();
        fg::Mesh3D& mesh = *slot;
        mesh.clear();
        mesh.reserve(tm.positions.size(), tm.indices.size());
        for (std::size_t i = 0; i < tm.positions.size(); ++i) {
            const Vec3 col = en.use_splat ? Vec3{1.0f, 1.0f, 1.0f} : tm.colors[i];
            mesh.add_vertex(fg::Vertex3D(dv(tm.positions[i]), dv(tm.normals[i]), Color(to_byte(col.x), to_byte(col.y), to_byte(col.z)), tm.uvs[i].x, tm.uvs[i].y));
        }
        mesh.edit_indices() = tm.indices;
        c.stale[static_cast<std::size_t>(lod)] = 0;
        ++stats.rebuilt;
        return &mesh;
    }
};

TerrainView::TerrainView() : m(std::make_unique<Impl>()) {}

TerrainView::~TerrainView() = default;

void TerrainView::set_asset_root(const std::string& folder) {
    m->root = folder;
}

const std::string& TerrainView::asset_root() const {
    return m->root;
}

void TerrainView::set_up(Vec3 up) {
    if (length(up) < 1e-6f) up = {0.0f, 1.0f, 0.0f};
    const Vec3 u = normalize(up);
    if (length(u - m->up) < 1e-6f) return;
    m->up = u;
    m->draws_stale = true;
}

Vec3 TerrainView::up() const {
    return m->up;
}

void TerrainView::set_texture_lookup(std::function<const fizmo::graphics::Texture*(const std::string&)> lookup) {
    m->lookup = std::move(lookup);
    m->averages.clear();
    for (auto& kv : m->entries) kv.second.look.clear();
}

void TerrainView::set_lod_distance(float multiplier) {
    m->lod_distance = std::max(0.05f, multiplier);
}

void TerrainView::set_rebuild_budget(std::size_t chunks_per_frame) {
    m->budget = std::max<std::size_t>(1, chunks_per_frame);
}

void TerrainView::set_wireframe_selected(bool on) {
    m->wire_selected = on;
}

void TerrainView::reset() {
    m->entries.clear();
    m->all_instances.clear();
    m->draws.clear();
    m->averages.clear();
    m->warnings.clear();
    m->draws_stale = true;
}

void TerrainView::sync(const Scene& scene) {
    for (auto& kv : m->entries) kv.second.alive = false;
    for (Entity e : scene.entities()) {
        const Terrain* t = scene.terrain(e);
        if (!t) continue;
        Entry& en = m->entries[e.id()];
        en.alive = true;
        m->update_heights(en, *t, scene.name(e));
        en.terrain = *t;
        if (!en.heights->valid()) continue;
        m->refresh(en);
        m->update_look(en);
        m->update_scatter(en);
    }
    for (auto it = m->entries.begin(); it != m->entries.end();) {
        if (!it->second.alive) {
            it = m->entries.erase(it);
            m->draws_stale = true;
        } else {
            ++it;
        }
    }
    m->rebuild_draws(scene);
}

std::size_t TerrainView::draw(fizmo::graphics::Renderer& r, const fizmo::graphics::Camera3D& camera, const Scene& scene, Entity selected) {
    ++m->frame;
    m->stats = TerrainViewStats{};
    const vector3d eye_d = camera.position();
    const Vec3 eye{static_cast<float>(eye_d.x), static_cast<float>(eye_d.y), static_cast<float>(eye_d.z)};
    for (Entity e : scene.entities()) {
        auto it = m->entries.find(e.id());
        if (it == m->entries.end() || !scene.terrain(e)) continue;
        Entry& en = it->second;
        if (!en.heights || !en.heights->valid()) continue;
        m->refresh(en);
        ++m->stats.terrains;
        const Mat4 w = m->world_of(scene, e);
        const fizmo::linalg::Matrix4d fw = to_fizmo(w);
        fg::Material3D mat = fg::Material3D::physical(0.0f, 0.92f);
        mat.cull = fg::Cull3D::Back;
        if (en.base_texture) mat.texture = en.base_texture;
        m->splat_material(en, mat);
        const int lods = std::max(1, en.chunking.lods);
        for (Chunk& c : en.chunks) {
            ++m->stats.chunks;
            if (c.bounds_stale) {
                terrain_chunk_bounds(*en.heights, c.cx, c.cz, en.chunking.chunk_quads, c.lo, c.hi);
                c.bounds_stale = false;
            }
            Vec3 wmin{1e30f, 1e30f, 1e30f}, wmax{-1e30f, -1e30f, -1e30f};
            for (int k = 0; k < 8; ++k) {
                const Vec3 p = mul_point(w, Vec3{k & 1 ? c.hi.x : c.lo.x, k & 2 ? c.hi.y : c.lo.y, k & 4 ? c.hi.z : c.lo.z});
                wmin = {std::min(wmin.x, p.x), std::min(wmin.y, p.y), std::min(wmin.z, p.z)};
                wmax = {std::max(wmax.x, p.x), std::max(wmax.y, p.y), std::max(wmax.z, p.z)};
            }
            if (!camera.is_visible(dv(wmin), dv(wmax))) {
                ++m->stats.culled;
                continue;
            }
            const Vec3 nearest{std::clamp(eye.x, wmin.x, wmax.x), std::clamp(eye.y, wmin.y, wmax.y), std::clamp(eye.z, wmin.z, wmax.z)};
            const float dist = length(eye - nearest);
            const float span = std::max(1e-3f, length(wmax - wmin)) * 1.5f * m->lod_distance;
            int lod = dist <= span ? 0 : static_cast<int>(std::floor(std::log2(dist / span))) + 1;
            lod = std::clamp(lod, 0, lods - 1);
            const fg::Mesh3D* mesh = nullptr;
            const std::size_t li = static_cast<std::size_t>(lod);
            if (c.lods[li] && !c.stale[li]) {
                mesh = c.lods[li].get();
            } else if (m->stats.rebuilt < m->budget) {
                mesh = m->build(en, c, lod);
            } else {
                ++m->stats.pending;
                for (int d = 1; d < lods && !mesh; ++d) {
                    for (int s : {lod + d, lod - d}) {
                        if (s >= 0 && s < lods && c.lods[static_cast<std::size_t>(s)] && !c.stale[static_cast<std::size_t>(s)]) {
                            mesh = c.lods[static_cast<std::size_t>(s)].get();
                            break;
                        }
                    }
                }
                if (!mesh && c.lods[li]) mesh = c.lods[li].get();
                if (!mesh) mesh = m->build(en, c, lods - 1);
            }
            for (std::size_t k = 0; k < c.used.size(); ++k) {
                if (mesh == c.lods[k].get()) c.used[k] = m->frame;
                else if (c.lods[k] && k < li && c.used[k] + 600 < m->frame) c.lods[k].reset();
            }
            r.draw_mesh(*mesh, fw, mat);
            ++m->stats.drawn;
            m->stats.triangles += mesh->triangle_count();
        }
        if (m->wire_selected && e == selected) {
            const TerrainHeights& h = *en.heights;
            const Color edge(247, 180, 72);
            const int sx = std::max(1, (h.width - 1) / 96), sz = std::max(1, (h.depth - 1) / 96);
            auto line = [&](int x0, int z0, int x1, int z1) {
                const Vec3 a = h.local_position(x0, z0) + Vec3{0.0f, 0.05f, 0.0f}, b = h.local_position(x1, z1) + Vec3{0.0f, 0.05f, 0.0f};
                r.draw_line_3d(dv(mul_point(w, a)), dv(mul_point(w, b)), edge, 2.0f, true);
            };
            for (int x = 0; x < h.width - 1; x += sx) {
                const int nx = std::min(x + sx, h.width - 1);
                line(x, 0, nx, 0);
                line(x, h.depth - 1, nx, h.depth - 1);
            }
            for (int z = 0; z < h.depth - 1; z += sz) {
                const int nz = std::min(z + sz, h.depth - 1);
                line(0, z, 0, nz);
                line(h.width - 1, z, h.width - 1, nz);
            }
        }
    }
    m->stats.instances = m->draws.size();
    return m->stats.drawn;
}

void TerrainView::draw_brush(fizmo::graphics::Renderer& r, const Scene& scene, Entity e, Vec3 world_point, float radius) {
    auto it = m->entries.find(e.id());
    if (it == m->entries.end() || !it->second.heights || !it->second.heights->valid()) return;
    const TerrainHeights& h = *it->second.heights;
    const Mat4 w = m->world_of(scene, e);
    const float sh = 0.5f * (length(column(w, 0)) + length(column(w, 2)));
    const float rl = radius / std::max(1e-4f, sh);
    const Vec3 c = terrain_to_local(w, world_point);
    const int segments = 48;
    const Color ring(255, 255, 255, 220);
    Vec3 prev;
    for (int i = 0; i <= segments; ++i) {
        const float a = static_cast<float>(i) * 2.0f * pi / static_cast<float>(segments);
        const float x = c.x + std::cos(a) * rl, z = c.z + std::sin(a) * rl;
        const Vec3 p = mul_point(w, Vec3{x, h.height_at(x, z) + 0.08f, z});
        if (i > 0) r.draw_line_3d(dv(prev), dv(p), ring, 2.0f, false);
        prev = p;
    }
    const Vec3 top = mul_point(w, Vec3{c.x, h.height_at(c.x, c.z) + rl * 0.15f, c.z});
    r.draw_line_3d(dv(mul_point(w, Vec3{c.x, h.height_at(c.x, c.z), c.z})), dv(top), ring, 2.0f, false);
}

std::shared_ptr<TerrainHeights> TerrainView::heights(Entity e) {
    auto it = m->entries.find(e.id());
    return it == m->entries.end() ? nullptr : it->second.heights;
}

std::shared_ptr<const TerrainHeights> TerrainView::heights(Entity e) const {
    auto it = m->entries.find(e.id());
    return it == m->entries.end() ? nullptr : it->second.heights;
}

std::function<std::shared_ptr<const TerrainHeights>(Entity, const Terrain&)> TerrainView::source() const {
    const Impl* impl = m.get();
    return [impl](Entity e, const Terrain&) -> std::shared_ptr<const TerrainHeights> {
        auto it = impl->entries.find(e.id());
        if (it == impl->entries.end() || !it->second.heights || !it->second.heights->valid()) return nullptr;
        return it->second.heights;
    };
}

Mat4 TerrainView::terrain_world(const Scene& scene, Entity e) const {
    return m->world_of(scene, e);
}

bool TerrainView::pick(const Scene& scene, Vec3 origin, Vec3 direction, float max_distance, Entity& hit, Vec3& point, Vec3* normal) const {
    bool any = false;
    float best = max_distance;
    for (const auto& kv : m->entries) {
        const Entity e = Entity::from_id(kv.first);
        if (!scene.alive(e) || !scene.terrain(e) || !kv.second.heights || !kv.second.heights->valid()) continue;
        const Mat4 w = m->world_of(scene, e);
        Mat4 inv;
        if (!invert(w, inv)) continue;
        const Vec3 lo = mul_point(inv, origin);
        const Vec3 ld = mul_direction(inv, normalize(direction));
        const float k = length(ld);
        if (k < 1e-9f) continue;
        float d = 0.0f;
        Vec3 p, n;
        if (!raycast_terrain(*kv.second.heights, lo, ld, best * k, d, &p, &n)) continue;
        const Vec3 wp = mul_point(w, p);
        const float wd = length(wp - origin);
        if (wd > best) continue;
        best = wd;
        hit = e;
        point = wp;
        if (normal) *normal = normalize(mul_direction(w, n));
        any = true;
    }
    return any;
}

float TerrainView::local_height(const Scene& scene, Entity e, Vec3 world_point) const {
    auto it = m->entries.find(e.id());
    if (it == m->entries.end() || !it->second.heights) return 0.0f;
    const Vec3 l = terrain_to_local(m->world_of(scene, e), world_point);
    return it->second.heights->height_at(l.x, l.z);
}

bool TerrainView::sculpt(const Scene& scene, Entity e, Vec3 world_point, const TerrainBrush& brush, float dt) {
    auto it = m->entries.find(e.id());
    if (it == m->entries.end() || !it->second.heights || !it->second.heights->valid()) return false;
    const Mat4 w = m->world_of(scene, e);
    const float sx = length(column(w, 0)), sy = length(column(w, 1)), sz = length(column(w, 2));
    const Vec3 l = terrain_to_local(w, world_point);
    TerrainBrush b = brush;
    b.radius = brush.radius / std::max(1e-4f, 0.5f * (sx + sz));
    if (b.kind == TerrainBrushKind::raise || b.kind == TerrainBrushKind::lower || b.kind == TerrainBrushKind::noise) b.strength = brush.strength / std::max(1e-4f, sy);
    if (!apply_brush(*it->second.heights, b, l.x, l.z, dt)) return false;
    it->second.unsaved = true;
    m->refresh(it->second);
    return true;
}

bool TerrainView::regenerate(const Scene& scene, Entity e) {
    const Terrain* t = scene.terrain(e);
    auto it = m->entries.find(e.id());
    if (!t || it == m->entries.end() || !it->second.heights) return false;
    generate_heights(*t, *it->second.heights);
    it->second.unsaved = true;
    m->set_keys(it->second, *t);
    m->refresh(it->second);
    return true;
}

bool TerrainView::import_heightmap(const Scene& scene, Entity e, const std::string& file, std::string* error) {
    const Terrain* t = scene.terrain(e);
    auto it = m->entries.find(e.id());
    if (!t) {
        if (error) *error = "that entity has no terrain";
        return false;
    }
    if (it == m->entries.end()) {
        sync(scene);
        it = m->entries.find(e.id());
        if (it == m->entries.end()) return false;
    }
    if (!load_heightmap(*t, m->path(file), *it->second.heights, error)) return false;
    it->second.unsaved = true;
    m->set_keys(it->second, *t);
    it->second.map = t->heightmap;
    m->refresh(it->second);
    return true;
}

bool TerrainView::unsaved(Entity e) const {
    auto it = m->entries.find(e.id());
    return it != m->entries.end() && it->second.unsaved;
}

bool TerrainView::any_unsaved() const {
    for (const auto& kv : m->entries)
        if (kv.second.unsaved) return true;
    return false;
}

bool TerrainView::save(const Scene& scene, Entity e, std::string* error) {
    const Terrain* t = scene.terrain(e);
    auto it = m->entries.find(e.id());
    if (!t || it == m->entries.end() || !it->second.heights) {
        if (error) *error = "nothing to save";
        return false;
    }
    if (t->data.empty()) {
        if (error) *error = "the terrain has no data file";
        return false;
    }
    const std::string file = m->path(t->data);
    std::error_code ec;
    const std::filesystem::path parent = std::filesystem::path(file).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    if (!it->second.heights->save(file, error)) return false;
    it->second.unsaved = false;
    it->second.data = t->data;
    return true;
}

std::size_t TerrainView::save_all(Scene& scene, std::vector<std::string>* errors) {
    std::size_t saved = 0;
    std::vector<std::string> used;
    for (Entity e : scene.entities())
        if (const Terrain* t = scene.terrain(e))
            if (!t->data.empty()) used.push_back(t->data);
    for (Entity e : scene.entities()) {
        Terrain* t = scene.terrain(e);
        auto it = m->entries.find(e.id());
        if (!t || it == m->entries.end() || !it->second.unsaved) continue;
        if (t->data.empty()) {
            std::string file = default_terrain_data_path(scene.name(e));
            if (std::find(used.begin(), used.end(), file) != used.end()) file = default_terrain_data_path(scene.name(e) + "-" + std::to_string(e.index));
            t->data = file;
            used.push_back(file);
        }
        std::string why;
        if (save(scene, e, &why)) ++saved;
        else if (errors) errors->push_back(scene.name(e) + ": " + why);
    }
    return saved;
}

const std::vector<ScatterInstance>& TerrainView::instances() const {
    return m->all_instances;
}

const std::vector<TerrainScatterDraw>& TerrainView::scatter() const {
    return m->draws;
}

void TerrainView::visible_scatter(const fizmo::graphics::Camera3D& camera, std::vector<std::size_t>& out, float radius_per_scale) const {
    out.clear();
    const vector3d eye_d = camera.position();
    const Vec3 eye{static_cast<float>(eye_d.x), static_cast<float>(eye_d.y), static_cast<float>(eye_d.z)};
    for (std::size_t i = 0; i < m->draws.size(); ++i) {
        const TerrainScatterDraw& d = m->draws[i];
        const Vec3 off = d.position - eye;
        if (dot(off, off) > d.draw_distance * d.draw_distance) continue;
        if (!camera.is_visible(dv(d.position), static_cast<double>(radius_per_scale * d.scale))) continue;
        out.push_back(i);
    }
}

TerrainViewStats TerrainView::stats() const {
    return m->stats;
}

const std::vector<std::string>& TerrainView::warnings() const {
    return m->warnings;
}

} // namespace editor
} // namespace orchard
