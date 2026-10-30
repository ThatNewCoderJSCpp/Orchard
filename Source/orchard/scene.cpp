#include <orchard/scene.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace orchard {

namespace {

const std::string empty_name;

std::string number(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(v));
    return buf;
}

} // namespace

const char* to_string(Dimension d) {
    return d == Dimension::two_d ? "2d" : "3d";
}

const std::vector<BodyFlagInfo>& body_flags() {
    static const std::vector<BodyFlagInfo> all = {
        {body_gravity, "gravity", "Gravity", true, true},
        {body_collisions, "collisions", "Collides", true, true},
        {body_friction, "friction", "Friction", true, true},
        {body_bounce, "bounce", "Bounces", true, true},
        {body_sleep, "sleep", "Can sleep", true, true},
        {body_start_awake, "awake", "Starts awake", true, true},
        {body_continuous, "continuous", "Fast-moving (CCD)", true, true},
        {body_contact_events, "contacts", "Contact events", true, true},
        {body_lock_position_x, "lock-x", "Lock X", true, true},
        {body_lock_position_y, "lock-y", "Lock Y", true, true},
        {body_lock_position_z, "lock-z", "Lock Z", false, true},
        {body_lock_rotation_x, "lock-rot-x", "Lock rotation X", false, true},
        {body_lock_rotation_y, "lock-rot-y", "Lock rotation Y", false, true},
        {body_lock_rotation_z, "lock-rot-z", "Lock rotation", true, true},
    };
    return all;
}

const char* to_string(BodyType type) {
    switch (type) {
    case BodyType::fixed: return "static";
    case BodyType::dynamic: return "dynamic";
    case BodyType::kinematic: return "kinematic";
    }
    return "dynamic";
}

std::optional<BodyType> parse_body_type(const std::string& text) {
    if (text == "static" || text == "fixed") return BodyType::fixed;
    if (text == "dynamic") return BodyType::dynamic;
    if (text == "kinematic") return BodyType::kinematic;
    return std::nullopt;
}

const char* to_string(ColliderShape shape) {
    switch (shape) {
    case ColliderShape::box: return "box";
    case ColliderShape::sphere: return "sphere";
    case ColliderShape::capsule: return "capsule";
    }
    return "box";
}

std::optional<ColliderShape> parse_collider_shape(const std::string& text) {
    if (text == "box" || text == "rect") return ColliderShape::box;
    if (text == "sphere" || text == "circle") return ColliderShape::sphere;
    if (text == "capsule") return ColliderShape::capsule;
    return std::nullopt;
}

const char* to_string(CombineMode mode) {
    switch (mode) {
    case CombineMode::project_default: return "default";
    case CombineMode::average: return "average";
    case CombineMode::geometric_mean: return "geometric-mean";
    case CombineMode::harmonic_mean: return "harmonic-mean";
    case CombineMode::quadratic_mean: return "quadratic-mean";
    case CombineMode::minimum: return "minimum";
    case CombineMode::multiply: return "multiply";
    case CombineMode::maximum: return "maximum";
    case CombineMode::sum: return "sum";
    }
    return "default";
}

std::optional<CombineMode> parse_combine_mode(const std::string& text) {
    if (text == "default" || text == "project") return CombineMode::project_default;
    if (text == "average" || text == "mean") return CombineMode::average;
    if (text == "geometric-mean" || text == "geometric") return CombineMode::geometric_mean;
    if (text == "harmonic-mean" || text == "harmonic") return CombineMode::harmonic_mean;
    if (text == "quadratic-mean" || text == "quadratic" || text == "rms") return CombineMode::quadratic_mean;
    if (text == "minimum" || text == "min") return CombineMode::minimum;
    if (text == "multiply" || text == "product") return CombineMode::multiply;
    if (text == "maximum" || text == "max") return CombineMode::maximum;
    if (text == "sum" || text == "add") return CombineMode::sum;
    return std::nullopt;
}

const char* combine_label(CombineMode mode) {
    switch (mode) {
    case CombineMode::project_default: return "Project default";
    case CombineMode::average: return "Average";
    case CombineMode::geometric_mean: return "Geometric mean";
    case CombineMode::harmonic_mean: return "Harmonic mean";
    case CombineMode::quadratic_mean: return "Quadratic mean";
    case CombineMode::minimum: return "Minimum";
    case CombineMode::multiply: return "Multiply";
    case CombineMode::maximum: return "Maximum";
    case CombineMode::sum: return "Sum";
    }
    return "Project default";
}

const char* combine_explanation(CombineMode mode) {
    switch (mode) {
    case CombineMode::project_default: return "use the project's setting";
    case CombineMode::average: return "(a + b) / 2, the middle; Unity's and Unreal's default";
    case CombineMode::geometric_mean: return "sqrt(a * b); a 0 on either side wins, like Box2D's friction";
    case CombineMode::harmonic_mean: return "2ab / (a + b); leans hard toward the smaller one";
    case CombineMode::quadratic_mean: return "sqrt((a² + b²) / 2); leans toward the bigger one";
    case CombineMode::minimum: return "the smaller of the two";
    case CombineMode::multiply: return "a * b; Bullet's default, ice stays slippery against anything";
    case CombineMode::maximum: return "the bigger of the two; one bouncy thing bounces off everything";
    case CombineMode::sum: return "a + b; both add up";
    }
    return "";
}

const std::vector<CombineMode>& combine_modes() {
    static const std::vector<CombineMode> modes = {CombineMode::project_default, CombineMode::average, CombineMode::geometric_mean, CombineMode::harmonic_mean, CombineMode::quadratic_mean, CombineMode::minimum, CombineMode::multiply, CombineMode::maximum, CombineMode::sum};
    return modes;
}

std::optional<Dimension> parse_dimension(const std::string& text) {
    if (text == "2d" || text == "2D" || text == "two_d") return Dimension::two_d;
    if (text == "3d" || text == "3D" || text == "three_d") return Dimension::three_d;
    return std::nullopt;
}

Scene::Scene(Dimension dimension) : m_dimension(dimension) {}

Scene::Slot* Scene::slot(Entity e) {
    if (!e.valid() || e.index >= m_slots.size()) return nullptr;
    Slot& s = m_slots[e.index];
    return s.used && s.generation == e.generation ? &s : nullptr;
}

const Scene::Slot* Scene::slot(Entity e) const {
    if (!e.valid() || e.index >= m_slots.size()) return nullptr;
    const Slot& s = m_slots[e.index];
    return s.used && s.generation == e.generation ? &s : nullptr;
}

Entity Scene::create(const std::string& name, Entity parent) {
    std::uint32_t index;
    if (!m_free.empty()) {
        index = m_free.back();
        m_free.pop_back();
    } else {
        index = static_cast<std::uint32_t>(m_slots.size());
        m_slots.emplace_back();
    }
    Slot& s = m_slots[index];
    s.used = true;
    s.name = name;
    s.transform = Transform{};
    s.parent = alive(parent) ? parent : Entity{};
    s.body.reset();
    s.colliders.clear();
    s.script.clear();
    ++m_alive;
    return {index, s.generation};
}

bool Scene::destroy(Entity e) {
    if (!slot(e)) return false;
    for (Entity child : children(e)) destroy(child);
    Slot& s = m_slots[e.index];
    s.used = false;
    s.name.clear();
    s.parent = Entity{};
    s.body.reset();
    s.colliders.clear();
    s.script.clear();
    ++s.generation;
    if (s.generation == 0) s.generation = 1;
    m_free.push_back(e.index);
    --m_alive;
    return true;
}

bool Scene::alive(Entity e) const {
    return slot(e) != nullptr;
}

void Scene::clear() {
    m_slots.clear();
    m_free.clear();
    m_alive = 0;
}

const std::string& Scene::name(Entity e) const {
    const Slot* s = slot(e);
    return s ? s->name : empty_name;
}

bool Scene::set_name(Entity e, const std::string& name) {
    Slot* s = slot(e);
    if (!s) return false;
    s->name = name;
    return true;
}

Transform* Scene::transform(Entity e) {
    Slot* s = slot(e);
    return s ? &s->transform : nullptr;
}

const Transform* Scene::transform(Entity e) const {
    const Slot* s = slot(e);
    return s ? &s->transform : nullptr;
}

Mat4 Scene::world_matrix(Entity e) const {
    Mat4 m;
    const Slot* s = slot(e);
    int depth = 0;
    while (s && depth < 1024) {
        m = trs(s->transform.position, s->transform.rotation, s->transform.scale) * m;
        s = slot(s->parent);
        ++depth;
    }
    return m;
}

Entity Scene::parent(Entity e) const {
    const Slot* s = slot(e);
    return s ? s->parent : Entity{};
}

bool Scene::is_ancestor(Entity maybe_ancestor, Entity e) const {
    Entity p = parent(e);
    while (p.valid()) {
        if (p == maybe_ancestor) return true;
        p = parent(p);
    }
    return false;
}

bool Scene::set_parent(Entity e, Entity parent) {
    Slot* s = slot(e);
    if (!s) return false;
    if (parent.valid() && (!alive(parent) || parent == e || is_ancestor(e, parent))) return false;
    s->parent = parent;
    return true;
}

std::vector<Entity> Scene::children(Entity e) const {
    std::vector<Entity> out;
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        const Slot& s = m_slots[i];
        if (s.used && s.parent == e && e.valid()) out.push_back({i, s.generation});
    }
    return out;
}

std::vector<Entity> Scene::roots() const {
    std::vector<Entity> out;
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        const Slot& s = m_slots[i];
        if (s.used && !alive(s.parent)) out.push_back({i, s.generation});
    }
    return out;
}

std::vector<Entity> Scene::entities() const {
    std::vector<Entity> out;
    out.reserve(m_alive);
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].used) out.push_back({i, m_slots[i].generation});
    }
    return out;
}

Entity Scene::find(const std::string& name) const {
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        const Slot& s = m_slots[i];
        if (s.used && s.name == name) return {i, s.generation};
    }
    return Entity{};
}

Body* Scene::body(Entity e) {
    Slot* s = slot(e);
    return s && s->body ? &*s->body : nullptr;
}

const Body* Scene::body(Entity e) const {
    const Slot* s = slot(e);
    return s && s->body ? &*s->body : nullptr;
}

Body& Scene::add_body(Entity e) {
    static Body none;
    Slot* s = slot(e);
    if (!s) {
        none = Body{};
        return none;
    }
    if (!s->body) {
        s->body = Body{};
        if (m_dimension == Dimension::two_d) s->body->set(body_lock_position_z, true);
    }
    return *s->body;
}

bool Scene::remove_body(Entity e) {
    Slot* s = slot(e);
    if (!s || !s->body) return false;
    s->body.reset();
    return true;
}

std::vector<Collider>* Scene::colliders(Entity e) {
    Slot* s = slot(e);
    return s ? &s->colliders : nullptr;
}

const std::vector<Collider>* Scene::colliders(Entity e) const {
    const Slot* s = slot(e);
    return s ? &s->colliders : nullptr;
}

Collider* Scene::add_collider(Entity e, ColliderShape shape) {
    Slot* s = slot(e);
    if (!s) return nullptr;
    Collider c;
    c.shape = shape;
    if (shape == ColliderShape::capsule) c.size = {0.5f, 1.0f, 0.5f};
    if (m_dimension == Dimension::two_d && shape == ColliderShape::box) c.size.z = 1.0f;
    s->colliders.push_back(c);
    return &s->colliders.back();
}

bool Scene::remove_collider(Entity e, std::size_t index) {
    Slot* s = slot(e);
    if (!s || index >= s->colliders.size()) return false;
    s->colliders.erase(s->colliders.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

Entity Scene::body_owner(Entity e) const {
    Entity at = e;
    int depth = 0;
    while (alive(at) && depth < 1024) {
        if (body(at)) return at;
        at = parent(at);
        ++depth;
    }
    return Entity{};
}

const std::string& Scene::script(Entity e) const {
    const Slot* s = slot(e);
    return s ? s->script : empty_name;
}

bool Scene::set_script(Entity e, const std::string& path) {
    Slot* s = slot(e);
    if (!s) return false;
    s->script = path;
    return true;
}

std::string Scene::save() const {
    std::ostringstream out;
    out << "orchard-scene 2 " << to_string(m_dimension) << "\n";
    std::unordered_map<std::uint64_t, std::size_t> numbers;
    const std::vector<Entity> all = entities();
    for (std::size_t i = 0; i < all.size(); ++i) numbers[all[i].id()] = i + 1;
    for (std::size_t i = 0; i < all.size(); ++i) {
        const Slot& s = m_slots[all[i].index];
        const Vec3 r = to_euler(s.transform.rotation);
        const auto p = numbers.find(s.parent.id());
        out << "entity " << (i + 1) << " " << (alive(s.parent) && p != numbers.end() ? p->second : 0)
            << " " << number(s.transform.position.x) << " " << number(s.transform.position.y) << " " << number(s.transform.position.z)
            << " " << number(r.x) << " " << number(r.y) << " " << number(r.z)
            << " " << number(s.transform.scale.x) << " " << number(s.transform.scale.y) << " " << number(s.transform.scale.z)
            << " " << s.name << "\n";
        const std::size_t n = i + 1;
        if (!s.script.empty()) out << "script " << n << " " << s.script << "\n";
        if (s.body) {
            const Body& b = *s.body;
            out << "body " << n << " " << to_string(b.type) << " " << (b.enabled ? 1 : 0) << " " << b.flags << " "
                << number(b.gravity_scale) << " " << number(b.linear_damping) << " " << number(b.angular_damping) << "\n";
        }
        for (const Collider& c : s.colliders) {
            out << "collider " << n << " " << to_string(c.shape)
                << " " << number(c.size.x) << " " << number(c.size.y) << " " << number(c.size.z)
                << " " << number(c.offset.x) << " " << number(c.offset.y) << " " << number(c.offset.z)
                << " " << (c.enabled ? 1 : 0) << " " << (c.sensor ? 1 : 0)
                << " " << number(c.density) << " " << number(c.friction) << " " << number(c.restitution);
            if (c.friction_combine != CombineMode::project_default) out << " friction-mix:" << to_string(c.friction_combine);
            if (c.bounce_combine != CombineMode::project_default) out << " bounce-mix:" << to_string(c.bounce_combine);
            out << "\n";
        }
    }
    return out.str();
}

std::optional<Scene> Scene::load(const std::string& text, std::string* error) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line)) {
        if (error) *error = "empty scene file";
        return std::nullopt;
    }
    std::istringstream head(line);
    std::string magic, dim;
    int version = 0;
    head >> magic >> version >> dim;
    const std::optional<Dimension> d = parse_dimension(dim);
    if (magic != "orchard-scene" || version < 1 || version > 2 || !d) {
        if (error) *error = "not an Orchard scene (version 1 or 2)";
        return std::nullopt;
    }
    Scene scene(*d);
    std::unordered_map<std::size_t, Entity> by_number;
    std::vector<std::pair<Entity, std::size_t>> parents;
    int line_no = 1;
    while (std::getline(in, line)) {
        ++line_no;
        if (line.empty()) continue;
        std::istringstream row(line);
        std::string kind;
        std::size_t num = 0, parent = 0;
        row >> kind >> num;
        if (kind == "script" || kind == "body" || kind == "collider") {
            const auto owner = by_number.find(num);
            if (!row || owner == by_number.end()) {
                if (error) *error = "line " + std::to_string(line_no) + ": " + kind + " for an entity that isn't there";
                return std::nullopt;
            }
            if (kind == "script") {
                std::string path;
                std::getline(row >> std::ws, path);
                scene.set_script(owner->second, path);
            } else if (kind == "body") {
                std::string type;
                int enabled = 1;
                Body b;
                row >> type >> enabled >> b.flags >> b.gravity_scale >> b.linear_damping >> b.angular_damping;
                const std::optional<BodyType> bt = parse_body_type(type);
                if (!row || !bt) {
                    if (error) *error = "line " + std::to_string(line_no) + ": bad body line";
                    return std::nullopt;
                }
                b.type = *bt;
                b.enabled = enabled != 0;
                scene.add_body(owner->second) = b;
            } else {
                std::string shape;
                int enabled = 1, sensor = 0;
                Collider c;
                row >> shape >> c.size.x >> c.size.y >> c.size.z >> c.offset.x >> c.offset.y >> c.offset.z >> enabled >> sensor >> c.density >> c.friction >> c.restitution;
                const std::optional<ColliderShape> cs = parse_collider_shape(shape);
                if (!row || !cs) {
                    if (error) *error = "line " + std::to_string(line_no) + ": bad collider line";
                    return std::nullopt;
                }
                c.shape = *cs;
                c.enabled = enabled != 0;
                c.sensor = sensor != 0;
                for (std::string extra; row >> extra;) {
                    const std::size_t colon = extra.find(':');
                    if (colon == std::string::npos) continue;
                    const std::optional<CombineMode> mode = parse_combine_mode(extra.substr(colon + 1));
                    if (!mode) continue;
                    if (extra.compare(0, colon, "friction-mix") == 0) c.friction_combine = *mode;
                    else if (extra.compare(0, colon, "bounce-mix") == 0) c.bounce_combine = *mode;
                }
                scene.colliders(owner->second)->push_back(c);
            }
            continue;
        }
        Transform t;
        Vec3 r;
        row >> parent >> t.position.x >> t.position.y >> t.position.z >> r.x >> r.y >> r.z >> t.scale.x >> t.scale.y >> t.scale.z;
        if (kind != "entity" || !row) {
            if (error) *error = "line " + std::to_string(line_no) + ": bad entity line";
            return std::nullopt;
        }
        std::string name;
        std::getline(row >> std::ws, name);
        t.rotation = from_euler(r);
        const Entity e = scene.create(name);
        *scene.transform(e) = t;
        by_number[num] = e;
        if (parent != 0) parents.emplace_back(e, parent);
    }
    for (const auto& [e, p] : parents) {
        const auto it = by_number.find(p);
        if (it != by_number.end()) scene.set_parent(e, it->second);
    }
    return scene;
}

bool Scene::save_file(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << save();
    return static_cast<bool>(out);
}

std::optional<Scene> Scene::load_file(const std::string& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "can't open " + path;
        return std::nullopt;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return load(ss.str(), error);
}

} // namespace orchard
