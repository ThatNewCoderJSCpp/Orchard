#include "model_view.hpp"

#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace orchard {
namespace editor {

namespace fg = fizmo::graphics;
namespace mm = fizmo::graphics::model_math;
using fizmo::linalg::vector3d;

namespace {

mm::Mat4 to_model(const Mat4& m) {
    mm::Mat4 out{};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) out[static_cast<std::size_t>(r * 4 + c)] = m.at(r, c);
    return out;
}

Mat4 from_model(const mm::Mat4& m) {
    Mat4 out;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) out.at(r, c) = static_cast<float>(m[static_cast<std::size_t>(r * 4 + c)]);
    return out;
}

fizmo::linalg::Matrix4d to_fizmo(const Mat4& m) {
    return fizmo::linalg::Matrix4d{
        m.at(0, 0), m.at(0, 1), m.at(0, 2), m.at(0, 3),
        m.at(1, 0), m.at(1, 1), m.at(1, 2), m.at(1, 3),
        m.at(2, 0), m.at(2, 1), m.at(2, 2), m.at(2, 3),
        m.at(3, 0), m.at(3, 1), m.at(3, 2), m.at(3, 3)};
}

Quat quat_from_matrix(const Mat4& m, Vec3 scale) {
    const float sx = scale.x != 0.0f ? scale.x : 1.0f, sy = scale.y != 0.0f ? scale.y : 1.0f, sz = scale.z != 0.0f ? scale.z : 1.0f;
    const float m00 = m.at(0, 0) / sx, m01 = m.at(0, 1) / sy, m02 = m.at(0, 2) / sz;
    const float m10 = m.at(1, 0) / sx, m11 = m.at(1, 1) / sy, m12 = m.at(1, 2) / sz;
    const float m20 = m.at(2, 0) / sx, m21 = m.at(2, 1) / sy, m22 = m.at(2, 2) / sz;
    Quat q;
    const float trace = m00 + m11 + m22;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    } else {
        const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }
    return normalize(q);
}

Transform decompose(const Mat4& m) {
    Transform t;
    t.position = {m.at(0, 3), m.at(1, 3), m.at(2, 3)};
    t.scale = {length(Vec3{m.at(0, 0), m.at(1, 0), m.at(2, 0)}), length(Vec3{m.at(0, 1), m.at(1, 1), m.at(2, 1)}), length(Vec3{m.at(0, 2), m.at(1, 2), m.at(2, 2)})};
    t.rotation = quat_from_matrix(m, t.scale);
    return t;
}

bool ray_box(Vec3 o, Vec3 d, Vec3 lo, Vec3 hi, float& t_hit) {
    float t0 = 0.0f, t1 = std::numeric_limits<float>::max();
    const float os[3] = {o.x, o.y, o.z}, ds[3] = {d.x, d.y, d.z}, ls[3] = {lo.x, lo.y, lo.z}, hs[3] = {hi.x, hi.y, hi.z};
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(ds[i]) < 1e-12f) {
            if (os[i] < ls[i] || os[i] > hs[i]) return false;
            continue;
        }
        float a = (ls[i] - os[i]) / ds[i], b = (hs[i] - os[i]) / ds[i];
        if (a > b) std::swap(a, b);
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
        if (t0 > t1) return false;
    }
    t_hit = t0;
    return true;
}

bool ray_triangle(Vec3 o, Vec3 d, Vec3 a, Vec3 b, Vec3 c, float& t) {
    const Vec3 e1 = b - a, e2 = c - a;
    const Vec3 p = cross(d, e2);
    const float det = dot(e1, p);
    if (std::fabs(det) < 1e-12f) return false;
    const float inv = 1.0f / det;
    const Vec3 s = o - a;
    const float u = dot(s, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const Vec3 q = cross(s, e1);
    const float v = dot(d, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    t = dot(e2, q) * inv;
    return t >= 0.0f;
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

int side_of(const std::string& name) {
    const std::string n = lower(name);
    if (n.find("left") != std::string::npos) return 1;
    if (n.find("right") != std::string::npos) return 2;
    auto sep = [](char c) { return c == '.' || c == '_' || c == '-' || c == ' ' || c == ':'; };
    for (std::size_t i = 0; i < n.size(); ++i) {
        if (n[i] != 'l' && n[i] != 'r') continue;
        const bool before = i == 0 || sep(n[i - 1]);
        const bool after = i + 1 == n.size() || sep(n[i + 1]);
        if (before && after) return n[i] == 'l' ? 1 : 2;
    }
    return 0;
}

std::string base_of(const std::string& name) {
    std::string n = lower(name);
    const std::size_t colon = n.rfind(':');
    if (colon != std::string::npos) n = n.substr(colon + 1);
    for (const char* word : {"left", "right"}) {
        const std::size_t at = n.find(word);
        if (at != std::string::npos) n.erase(at, std::string(word).size());
    }
    std::string out;
    auto sep = [](char c) { return c == '.' || c == '_' || c == '-' || c == ' '; };
    for (std::size_t i = 0; i < n.size(); ++i) {
        const char c = n[i];
        if ((c == 'l' || c == 'r') && (i == 0 || sep(n[i - 1])) && (i + 1 == n.size() || sep(n[i + 1]))) continue;
        if (std::isalpha(static_cast<unsigned char>(c))) out += c;
    }
    return out;
}

} // namespace

struct ModelView::Impl {
    struct Entry {
        std::unique_ptr<fg::Model3D> model;
        std::string error;
        std::filesystem::file_time_type stamp{};
        std::vector<mm::Mat4> rest;
        Vec3 low{};
        Vec3 high{};
        bool has_bounds = false;
    };
    struct Live {
        const fg::Model3D* model = nullptr;
        fg::ModelAnimator anim;
        std::string clip;
        std::uint32_t restart = 0;
        bool started = false;
        bool posed = false;
        double time = 0.0;
        bool seen = false;
    };

    std::filesystem::path root;
    bool z_up = false;
    std::unordered_map<std::string, Entry> cache;
    std::unordered_map<std::uint64_t, Live> live;
    std::unordered_map<std::uint64_t, Mat4> bone_offsets;
    std::vector<FiredAnimationEvent> fired;
    std::size_t drawn = 0;

    std::filesystem::path path_of(const std::string& source) const {
        const std::filesystem::path p(source);
        return p.is_absolute() || root.empty() ? p : root / p;
    }

    Entry* entry(const std::string& source) {
        if (source.empty()) return nullptr;
        auto it = cache.find(source);
        if (it != cache.end()) return &it->second;
        Entry& e = cache[source];
        const std::filesystem::path p = path_of(source);
        std::error_code ec;
        e.stamp = std::filesystem::last_write_time(p, ec);
        auto model = std::make_unique<fg::Model3D>();
        fg::ModelImportOptions o;
        o.z_up = z_up;
        std::string error;
        if (!std::filesystem::exists(p, ec)) {
            e.error = "can't find " + source;
        } else if (!fg::Model3D::load(p, *model, &error, o)) {
            e.error = error.empty() ? "can't read " + source : error;
        } else {
            e.model = std::move(model);
            e.model->world_matrices(e.model->rest_pose(), e.rest);
            vector3d lo, hi;
            e.model->bounds(lo, hi);
            if (lo.x <= hi.x) {
                e.low = {static_cast<float>(lo.x), static_cast<float>(lo.y), static_cast<float>(lo.z)};
                e.high = {static_cast<float>(hi.x), static_cast<float>(hi.y), static_cast<float>(hi.z)};
                e.has_bounds = true;
            }
        }
        return &e;
    }

    const std::vector<mm::Mat4>* pose_of(Entity e, const Entry& en) const {
        const auto it = live.find(e.id());
        if (it != live.end() && it->second.model == en.model.get() && it->second.posed) return &it->second.anim.world();
        return &en.rest;
    }

    void draw_model(fg::Renderer& r, const Entry& en, const std::vector<mm::Mat4>& world, const fg::ModelAnimator* anim, const Mat4& placement, const fg::Material3D* override_material, bool cast_shadows) {
        const fg::Model3D& model = *en.model;
        const mm::Mat4 base = to_model(placement);
        const fizmo::linalg::Matrix4d root_m = to_fizmo(placement);
        for (std::size_t i = 0; i < model.nodes.size() && i < world.size(); ++i) {
            const fg::ModelNode& node = model.nodes[i];
            if (node.mesh < 0 || static_cast<std::size_t>(node.mesh) >= model.meshes.size()) continue;
            const auto& prims = model.meshes[static_cast<std::size_t>(node.mesh)].primitives;
            const fizmo::linalg::Matrix4d m = mm::to_matrix(mm::mul(base, world[i]));
            for (std::size_t p = 0; p < prims.size(); ++p) {
                fg::Material3D mat = model.material3d(prims[p].material, override_material);
                if (!cast_shadows) mat.shadow = fg::Shadow3D::None;
                const fg::Mesh3D* skinned = anim && node.skin >= 0 && prims[p].skinned() ? anim->skinned_mesh(static_cast<std::size_t>(node.mesh), p) : nullptr;
                if (skinned && !skinned->empty()) r.draw_mesh(*skinned, root_m, mat);
                else r.draw_mesh(prims[p].mesh, m, mat);
            }
        }
    }
};

ModelView::ModelView() : m(std::make_unique<Impl>()) {}
ModelView::~ModelView() = default;

void ModelView::set_root(const std::filesystem::path& project_root) {
    if (m->root == project_root) return;
    m->root = project_root;
    clear();
}

const std::filesystem::path& ModelView::root() const {
    return m->root;
}

void ModelView::set_z_up(bool z_up) {
    if (m->z_up == z_up) return;
    m->z_up = z_up;
    clear();
}

const fg::Model3D* ModelView::model(const std::string& source, std::string* error) {
    Impl::Entry* e = m->entry(source);
    if (!e) {
        if (error) *error = "no model file";
        return nullptr;
    }
    if (error) *error = e->error;
    return e->model.get();
}

ModelInfo ModelView::info(const std::string& source) {
    ModelInfo out;
    Impl::Entry* e = m->entry(source);
    if (!e) {
        out.error = "no model file";
        return out;
    }
    out.error = e->error;
    if (!e->model) return out;
    const fg::Model3D& model = *e->model;
    out.ok = true;
    for (const fg::ModelAnimation& a : model.animations) out.clips.push_back({a.name, a.duration});
    std::vector<char> joint(model.nodes.size(), 0);
    for (const fg::ModelSkin& s : model.skins)
        for (int j : s.joints)
            if (j >= 0 && static_cast<std::size_t>(j) < joint.size()) joint[static_cast<std::size_t>(j)] = 1;
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        out.nodes.push_back(model.nodes[i].name);
        if (joint[i]) out.bones.push_back(model.nodes[i].name);
    }
    for (const fg::ModelMaterial& mat : model.materials) out.materials.push_back(mat.name);
    for (const fg::ModelMeshData& md : model.meshes)
        for (const fg::ModelPrimitive& p : md.primitives) {
            out.vertices += p.mesh.vertices().size();
            out.triangles += p.mesh.indexed() ? p.mesh.indices().size() / 3 : p.mesh.vertices().size() / 3;
            out.skinned = out.skinned || p.skinned();
        }
    out.textures = model.textures.size();
    out.low = e->low;
    out.high = e->high;
    return out;
}

bool ModelView::reload_changed() {
    std::vector<std::string> stale;
    for (auto& kv : m->cache) {
        std::error_code ec;
        const auto stamp = std::filesystem::last_write_time(m->path_of(kv.first), ec);
        if (!ec && stamp != kv.second.stamp) stale.push_back(kv.first);
        if (ec && kv.second.model) stale.push_back(kv.first);
    }
    for (const std::string& s : stale) forget(s);
    return !stale.empty();
}

void ModelView::forget(const std::string& source) {
    auto it = m->cache.find(source);
    if (it == m->cache.end()) return;
    const fg::Model3D* gone = it->second.model.get();
    for (auto l = m->live.begin(); l != m->live.end();) {
        if (l->second.model == gone) l = m->live.erase(l);
        else ++l;
    }
    m->cache.erase(it);
}

void ModelView::clear() {
    m->live.clear();
    m->cache.clear();
    m->bone_offsets.clear();
}

void ModelView::animate(const Scene& scene, double dt, bool running) {
    for (auto& kv : m->live) kv.second.seen = false;
    for (Entity e : scene.entities()) {
        const Model* md = scene.model(e);
        const Animator* a = scene.animator(e);
        if (!md || !a) continue;
        Impl::Entry* en = m->entry(md->source);
        if (!en || !en->model || en->model->animations.empty()) continue;
        Impl::Live& l = m->live[e.id()];
        l.seen = true;
        if (l.model != en->model.get()) {
            l = Impl::Live{};
            l.seen = true;
            l.model = en->model.get();
            l.anim.attach(*l.model);
        }
        int index = a->clip.empty() ? 0 : l.model->find_animation(a->clip);
        if (index < 0) {
            const std::string want = lower(a->clip);
            for (std::size_t i = 0; i < l.model->animations.size(); ++i)
                if (lower(l.model->animations[i].name) == want) index = static_cast<int>(i);
        }
        if (index < 0) continue;
        const std::string clip_name = l.model->animations[static_cast<std::size_t>(index)].name;
        if (!l.started || l.clip != clip_name || l.restart != a->restart) {
            const bool fade = l.started && running && l.clip != clip_name;
            l.anim.play(index, a->loop, fade ? a->blend : 0.0, a->speed);
            l.anim.set_time(a->start);
            l.anim.update(0.0);
            l.clip = clip_name;
            l.restart = a->restart;
            l.started = true;
            l.posed = true;
            l.time = l.anim.time();
            continue;
        }
        l.anim.set_speed(a->speed);
        if (!running || !a->playing || l.anim.finished()) continue;
        const double before = l.anim.time();
        l.anim.update(dt);
        l.posed = true;
        const double after = l.anim.time();
        const double length = l.anim.duration();
        const bool wrapped = a->loop && a->speed >= 0.0f && after < before;
        for (const AnimationEvent& ev : a->events) {
            if (!ev.clip.empty() && lower(ev.clip) != lower(clip_name)) continue;
            const double t = ev.time;
            bool hit = false;
            if (a->speed >= 0.0f) hit = wrapped ? (t > before && t <= length) || (t >= 0.0 && t <= after) : t > before && t <= after;
            else hit = t < before && t >= after;
            if (hit) m->fired.push_back({e, ev.name});
        }
        l.time = after;
    }
    for (auto it = m->live.begin(); it != m->live.end();) {
        if (!it->second.seen) it = m->live.erase(it);
        else ++it;
    }
}

void ModelView::reset_animation() {
    m->live.clear();
    m->bone_offsets.clear();
    m->fired.clear();
}

std::vector<Entity> ModelView::follow_bones(Scene& scene) {
    std::vector<Entity> moved;
    for (Entity e : scene.entities()) {
        if (scene.bone(e).empty()) continue;
        const Entity owner = scene.bone_owner(e);
        if (!scene.alive(owner)) continue;
        const Model* md = scene.model(owner);
        Impl::Entry* en = md ? m->entry(md->source) : nullptr;
        if (!en || !en->model) continue;
        const int node = en->model->find_node(scene.bone(e));
        if (node < 0 || static_cast<std::size_t>(node) >= en->rest.size()) continue;
        const Mat4 owner_world = scene.world_matrix(owner);
        auto off = m->bone_offsets.find(e.id());
        if (off == m->bone_offsets.end()) {
            Mat4 inv;
            if (!invert(owner_world * from_model(en->rest[static_cast<std::size_t>(node)]), inv)) continue;
            off = m->bone_offsets.emplace(e.id(), inv * scene.world_matrix(e)).first;
        }
        const std::vector<mm::Mat4>* pose = m->pose_of(owner, *en);
        if (!pose || static_cast<std::size_t>(node) >= pose->size()) continue;
        const Mat4 target = owner_world * from_model((*pose)[static_cast<std::size_t>(node)]) * off->second;
        Mat4 local = target;
        const Entity parent = scene.parent(e);
        if (scene.alive(parent)) {
            Mat4 pinv;
            if (!invert(scene.world_matrix(parent), pinv)) continue;
            local = pinv * target;
        }
        Transform* t = scene.transform(e);
        if (!t) continue;
        const Transform next = decompose(local);
        t->position = next.position;
        t->rotation = next.rotation;
        t->scale = next.scale;
        moved.push_back(e);
    }
    return moved;
}

std::vector<FiredAnimationEvent> ModelView::take_events() {
    std::vector<FiredAnimationEvent> out;
    out.swap(m->fired);
    return out;
}

AnimationState ModelView::state(Entity e) const {
    AnimationState s;
    const auto it = m->live.find(e.id());
    if (it == m->live.end()) return s;
    s.time = static_cast<float>(it->second.anim.time());
    s.duration = static_cast<float>(it->second.anim.duration());
    s.finished = it->second.anim.finished();
    s.playing = it->second.started && !s.finished;
    return s;
}

bool ModelView::bone_world(const Scene& scene, Entity owner, const std::string& bone, Mat4& out) {
    const Model* md = scene.model(owner);
    Impl::Entry* en = md ? m->entry(md->source) : nullptr;
    if (!en || !en->model) return false;
    const int node = en->model->find_node(bone);
    const std::vector<mm::Mat4>* pose = m->pose_of(owner, *en);
    if (node < 0 || !pose || static_cast<std::size_t>(node) >= pose->size()) return false;
    out = scene.world_matrix(owner) * from_model((*pose)[static_cast<std::size_t>(node)]);
    return true;
}

bool ModelView::bone_segment(const std::string& source, const std::string& bone, Vec3& head, Vec3& tail) {
    Impl::Entry* en = m->entry(source);
    if (!en || !en->model) return false;
    const fg::Model3D& model = *en->model;
    const int node = model.find_node(bone);
    if (node < 0 || static_cast<std::size_t>(node) >= en->rest.size()) return false;
    std::vector<char> joint(model.nodes.size(), 0);
    for (const fg::ModelSkin& s : model.skins)
        for (int j : s.joints)
            if (j >= 0 && static_cast<std::size_t>(j) < joint.size()) joint[static_cast<std::size_t>(j)] = 1;
    auto pos = [&](int i) { return mul_point(from_model(en->rest[static_cast<std::size_t>(i)]), Vec3{}); };
    head = pos(node);
    Vec3 sum{};
    int count = 0;
    for (int c : model.nodes[static_cast<std::size_t>(node)].children)
        if (c >= 0 && static_cast<std::size_t>(c) < joint.size() && joint[static_cast<std::size_t>(c)]) {
            sum = sum + pos(c);
            ++count;
        }
    if (count > 0) {
        tail = sum * (1.0f / static_cast<float>(count));
        return true;
    }
    const int parent = model.nodes[static_cast<std::size_t>(node)].parent;
    if (parent >= 0) {
        const Vec3 along = head - pos(parent);
        tail = head + along * 0.6f;
    } else {
        tail = head + Vec3{0.0f, 0.1f, 0.0f};
    }
    return true;
}

void ModelView::draw(fg::Renderer& r, const Scene& scene, const std::function<bool(const Mat4&, Vec3, Vec3)>& visible, const std::function<bool(Entity, fg::Material3D&)>& material_for) {
    m->drawn = 0;
    for (Entity e : scene.entities()) {
        const Model* md = scene.model(e);
        if (!md || !md->visible) continue;
        Impl::Entry* en = m->entry(md->source);
        if (!en || !en->model) continue;
        const Mat4 world = scene.world_matrix(e);
        if (en->has_bounds && visible && !visible(world, en->low, en->high)) continue;
        fg::Material3D over;
        const bool overridden = material_for && material_for(e, over);
        const auto it = m->live.find(e.id());
        const bool animated = it != m->live.end() && it->second.model == en->model.get() && it->second.posed;
        m->draw_model(r, *en, animated ? it->second.anim.world() : en->rest, animated ? &it->second.anim : nullptr, world, overridden ? &over : nullptr, md->cast_shadows);
        ++m->drawn;
    }
}

void ModelView::draw_instances(fg::Renderer& r, const std::vector<ModelInstance>& instances, const std::function<bool(const Mat4&, Vec3, Vec3)>& visible) {
    for (const ModelInstance& inst : instances) {
        Impl::Entry* en = m->entry(inst.source);
        if (!en || !en->model) continue;
        if (en->has_bounds && visible && !visible(inst.world, en->low, en->high)) continue;
        m->draw_model(r, *en, en->rest, nullptr, inst.world, nullptr, inst.cast_shadows);
        ++m->drawn;
    }
}

void ModelView::draw_source(fg::Renderer& r, const std::string& source, const Mat4& world, const fg::Material3D* override_material, bool cast_shadows) {
    Impl::Entry* en = m->entry(source);
    if (!en || !en->model) return;
    m->draw_model(r, *en, en->rest, nullptr, world, override_material, cast_shadows);
}

void ModelView::draw_bones(fg::Renderer& r, const Scene& scene, Entity e) {
    const Model* md = scene.model(e);
    Impl::Entry* en = md ? m->entry(md->source) : nullptr;
    if (!en || !en->model || en->model->skins.empty()) return;
    const std::vector<mm::Mat4>* pose = m->pose_of(e, *en);
    const Mat4 world = scene.world_matrix(e);
    std::vector<char> joint(en->model->nodes.size(), 0);
    for (const fg::ModelSkin& s : en->model->skins)
        for (int j : s.joints)
            if (j >= 0 && static_cast<std::size_t>(j) < joint.size()) joint[static_cast<std::size_t>(j)] = 1;
    const fizmo::colors::Color line(120, 220, 255, 220), dot_c(255, 255, 255, 230);
    for (std::size_t i = 0; i < en->model->nodes.size() && i < pose->size(); ++i) {
        if (!joint[i]) continue;
        const Vec3 a = mul_point(world * from_model((*pose)[i]), Vec3{});
        const int parent = en->model->nodes[i].parent;
        if (parent >= 0 && static_cast<std::size_t>(parent) < joint.size() && joint[static_cast<std::size_t>(parent)]) {
            const Vec3 b = mul_point(world * from_model((*pose)[static_cast<std::size_t>(parent)]), Vec3{});
            r.draw_line_3d(vector3d{a.x, a.y, a.z}, vector3d{b.x, b.y, b.z}, line, 2.0f, false);
        }
        r.draw_line_3d(vector3d{a.x - 0.01, a.y, a.z}, vector3d{a.x + 0.01, a.y, a.z}, dot_c, 4.0f, false);
    }
}

bool ModelView::local_bounds(const std::string& source, Vec3& low, Vec3& high) {
    Impl::Entry* en = m->entry(source);
    if (!en || !en->model || !en->has_bounds) return false;
    low = en->low;
    high = en->high;
    return true;
}

bool ModelView::world_bounds(const Scene& scene, Entity e, Vec3& low, Vec3& high) {
    const Model* md = scene.model(e);
    Vec3 lo, hi;
    if (!md || !local_bounds(md->source, lo, hi)) return false;
    const Mat4 w = scene.world_matrix(e);
    low = {1e30f, 1e30f, 1e30f};
    high = {-1e30f, -1e30f, -1e30f};
    for (int i = 0; i < 8; ++i) {
        const Vec3 c{(i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z};
        const Vec3 p = mul_point(w, c);
        low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
        high = {std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
    }
    return true;
}

bool ModelView::pick(const Scene& scene, Vec3 origin, Vec3 direction, Entity& hit, float& distance) {
    bool any = false;
    float best = std::numeric_limits<float>::max();
    for (Entity e : scene.entities()) {
        const Model* md = scene.model(e);
        if (!md || !md->visible) continue;
        Impl::Entry* en = m->entry(md->source);
        if (!en || !en->model || !en->has_bounds) continue;
        const Mat4 w = scene.world_matrix(e);
        Mat4 inv;
        if (!invert(w, inv)) continue;
        const Vec3 lo_o = mul_point(inv, origin);
        const Vec3 lo_d = mul_direction(inv, direction);
        float tb = 0.0f;
        if (!ray_box(lo_o, lo_d, en->low, en->high, tb)) continue;
        const std::vector<mm::Mat4>* pose = m->pose_of(e, *en);
        const fg::Model3D& model = *en->model;
        float local_best = std::numeric_limits<float>::max();
        bool skinned_only = true;
        for (std::size_t i = 0; i < model.nodes.size() && i < pose->size(); ++i) {
            const fg::ModelNode& node = model.nodes[i];
            if (node.mesh < 0 || static_cast<std::size_t>(node.mesh) >= model.meshes.size()) continue;
            const Mat4 nm = from_model((*pose)[i]);
            for (const fg::ModelPrimitive& p : model.meshes[static_cast<std::size_t>(node.mesh)].primitives) {
                if (p.skinned()) continue;
                skinned_only = false;
                const auto& v = p.mesh.vertices();
                const auto& idx = p.mesh.indices();
                const std::size_t count = idx.empty() ? v.size() : idx.size();
                for (std::size_t k = 0; k + 2 < count; k += 3) {
                    const std::size_t ia = idx.empty() ? k : idx[k], ib = idx.empty() ? k + 1 : idx[k + 1], ic = idx.empty() ? k + 2 : idx[k + 2];
                    if (ia >= v.size() || ib >= v.size() || ic >= v.size()) continue;
                    const Vec3 a = mul_point(nm, Vec3{v[ia].x, v[ia].y, v[ia].z});
                    const Vec3 b = mul_point(nm, Vec3{v[ib].x, v[ib].y, v[ib].z});
                    const Vec3 c = mul_point(nm, Vec3{v[ic].x, v[ic].y, v[ic].z});
                    float t = 0.0f;
                    if (ray_triangle(lo_o, lo_d, a, b, c, t) && t < local_best) local_best = t;
                }
            }
        }
        if (skinned_only) local_best = tb;
        if (local_best == std::numeric_limits<float>::max()) continue;
        const Vec3 at = mul_point(w, lo_o + lo_d * local_best);
        const float dist = length(at - origin);
        if (dist < best) {
            best = dist;
            hit = e;
            any = true;
        }
    }
    if (any) distance = best;
    return any;
}

bool ModelView::triangles(const std::string& source, std::vector<Vec3>& positions, std::vector<std::uint32_t>& indices) {
    Impl::Entry* en = m->entry(source);
    if (!en || !en->model) return false;
    const fg::Model3D& model = *en->model;
    for (std::size_t i = 0; i < model.nodes.size() && i < en->rest.size(); ++i) {
        const fg::ModelNode& node = model.nodes[i];
        if (node.mesh < 0 || static_cast<std::size_t>(node.mesh) >= model.meshes.size()) continue;
        const Mat4 nm = from_model(en->rest[i]);
        for (const fg::ModelPrimitive& p : model.meshes[static_cast<std::size_t>(node.mesh)].primitives) {
            const auto base = static_cast<std::uint32_t>(positions.size());
            for (const fg::Vertex3D& v : p.mesh.vertices()) positions.push_back(mul_point(nm, Vec3{v.x, v.y, v.z}));
            if (p.mesh.indexed())
                for (std::uint32_t k : p.mesh.indices()) indices.push_back(base + k);
            else
                for (std::uint32_t k = 0; k < p.mesh.vertices().size(); ++k) indices.push_back(base + k);
        }
    }
    return !indices.empty();
}

std::size_t ModelView::drawn() const {
    return m->drawn;
}

std::string guess_bone(const std::vector<std::string>& bones, const std::string& part) {
    const std::string p = lower(part);
    const int want_side = p.find("left") != std::string::npos ? 1 : p.find("right") != std::string::npos ? 2 : 0;
    std::vector<std::string> wanted;
    auto has = [&p](const char* w) { return p.find(w) != std::string::npos; };
    if (has("forearm") || has("lower arm") || has("elbow")) wanted = {"forearm", "lowerarm", "elbow"};
    else if (has("upper arm") || has("shoulder")) wanted = {"arm", "upperarm", "shoulder"};
    else if (has("hand") || has("wrist") || has("finger")) wanted = {"hand", "wrist"};
    else if (has("thigh") || has("upper leg") || has("femur") || has("hip joint")) wanted = {"upleg", "thigh", "upperleg"};
    else if (has("shin") || has("calf") || has("lower leg") || has("knee")) wanted = {"leg", "calf", "shin", "lowerleg", "knee"};
    else if (has("foot") || has("ankle") || has("toe")) wanted = {"foot", "ankle", "toe", "toebase"};
    else if (has("head") || has("skull") || has("brain") || has("eye") || has("jaw")) wanted = {"head"};
    else if (has("neck") || has("throat")) wanted = {"neck"};
    else if (has("pelvis") || has("hips") || has("abdomen") || has("stomach") || has("kidney") || has("liver") || has("bladder")) wanted = {"hips", "pelvis", "spine", "spinea"};
    else if (has("torso") || has("chest") || has("heart") || has("lung") || has("spine") || has("body") || has("rib") || has("artery")) wanted = {"spine", "chest", "upperchest", "spineb", "spinec", "torso", "hips"};
    else wanted = {base_of(part)};
    for (const std::string& w : wanted) {
        for (const std::string& b : bones) {
            const int side = side_of(b);
            if (want_side != 0 && side != want_side) continue;
            if (want_side == 0 && side != 0) continue;
            if (base_of(b) == w) return b;
        }
    }
    for (const std::string& w : wanted) {
        for (const std::string& b : bones) {
            const int side = side_of(b);
            if (want_side != 0 && side != want_side) continue;
            const std::string bb = base_of(b);
            if (bb.rfind(w, 0) == 0 && bb.find("fore") == std::string::npos && bb.find("up") == std::string::npos) return b;
        }
    }
    return std::string();
}

} // namespace editor
} // namespace orchard
