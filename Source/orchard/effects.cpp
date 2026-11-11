#include <orchard/effects.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>

namespace orchard {

namespace {

struct Builder {
    Scene& scene;
    Entity root;
    float k;
    double start;

    Emitter* layer(const std::string& name, const std::string& preset) {
        std::optional<Emitter> e = Emitter::named(preset);
        if (!e) return nullptr;
        const Entity child = scene.create(name, root);
        Emitter& out = scene.add_emitter(child);
        out = *e;
        out.size *= k;
        out.size_end *= k;
        out.speed *= std::sqrt(k);
        out.speed_spread *= std::sqrt(k);
        out.shape_size = out.shape_size * k;
        return &out;
    }

    Emitter* once(const std::string& name, const std::string& preset, int count) {
        Emitter* e = layer(name, preset);
        if (!e) return nullptr;
        e->loop = false;
        e->rate = 0.0f;
        e->burst = std::max(1, static_cast<int>(std::lround(static_cast<float>(count) * std::max(0.4f, k))));
        e->duration = 0.1f;
        return e;
    }

    void scorch(float size) {
        if (Emitter* e = once("Scorch", "scorch", 1)) {
            e->size = e->size_end = size * k;
            e->burst = 1;
            if (Entity at = scene.children(root).back(); scene.alive(at)) scene.transform(at)->position = Vec3{0.0f, 0.02f, 0.0f};
        }
    }

    Light& light(Vec3 color, float intensity, float range) {
        const Entity child = scene.create("Light", root);
        Light& l = scene.add_light(child);
        l.color = color;
        l.intensity = intensity * k * k;
        l.range = range * k;
        l.shadows = false;
        return l;
    }

    void flicker(Light& l, float speed, float low) {
        l.animation = LightAnimation::flicker(speed, low);
        l.animation.softness = 1.0f;
        l.animation.seed = static_cast<std::uint32_t>(root.id() & 0xffffffffu) | 1u;
    }

    void fade(Light& l, float seconds) {
        l.animation.mode = LightAnimationMode::fade;
        l.animation.duration = seconds;
        l.animation.rate = 1.0f / std::max(0.01f, seconds);
        l.animation.rate_end = l.animation.rate;
        l.animation.loop = false;
        l.animation.low = 0.0f;
        l.animation.high = 1.0f;
        l.animation.high_end = 1.0f;
        l.animation.delay = static_cast<float>(start);
    }
};

void explosion(Builder& b) {
    b.scorch(2.8f);
    if (Emitter* e = b.once("Flash", "flash", 2)) e->burst = 2;
    b.once("Fireball", "fireball", 18);
    if (Emitter* e = b.once("Flames", "explosion", 26)) e->on_death = "";
    if (Emitter* e = b.once("Smoke", "smoke-dark", 22)) {
        e->start_delay = 0.12f;
        e->speed = 2.2f * std::sqrt(b.k);
        e->spread = 180.0f;
        e->shape = EmitterShape::sphere;
        e->shape_size = Vec3{0.8f, 0.8f, 0.8f} * b.k;
        e->lifetime = 5.0f;
        e->drag = 1.6f;
    }
    if (Emitter* e = b.once("Sparks", "sparks", 80)) {
        e->spread = 180.0f;
        e->speed = 9.0f * std::sqrt(b.k);
        e->lifetime = 1.2f;
    }
    if (Emitter* e = b.once("Debris", "debris", 30)) e->spread = 70.0f;
    if (Emitter* e = b.once("Embers", "embers", 40)) {
        e->spread = 180.0f;
        e->speed = 4.0f * std::sqrt(b.k);
        e->lifetime = 3.0f;
    }
    b.once("Shockwave", "shockwave", 1);
    b.once("Dust", "dust-burst", 14);
    b.fade(b.light(Vec3{1.0f, 0.62f, 0.3f}, 260.0f, 18.0f), 0.8f);
}

void fire(Builder& b, bool smoke) {
    b.scorch(1.3f);
    b.layer("Flames", "flames");
    if (Emitter* e = b.layer("Fire", "fire")) {
        e->rate = 25.0f;
        e->on_death = "";
    }
    b.layer("Embers", "embers");
    if (smoke)
        if (Emitter* e = b.layer("Smoke", "smoke-dark")) {
            e->rate = 7.0f;
            e->start_delay = 0.3f;
            e->alpha = 0.45f;
        }
    Light& l = b.light(Vec3{1.0f, 0.55f, 0.2f}, 30.0f, 8.0f);
    b.flicker(l, 11.0f, 0.55f);
    if (Entity at = b.scene.children(b.root).back(); b.scene.alive(at)) b.scene.transform(at)->position = Vec3{0.0f, 0.5f * b.k, 0.0f};
}

void burning(Builder& b) {
    if (Emitter* e = b.layer("Flames", "flames")) {
        e->shape = EmitterShape::parts;
        e->rate = 110.0f;
        e->size = 0.35f;
        e->size_end = 0.08f;
    }
    if (Emitter* e = b.layer("Smoke", "smoke-dark")) {
        e->shape = EmitterShape::parts;
        e->rate = 8.0f;
        e->alpha = 0.4f;
    }
    if (Emitter* e = b.layer("Embers", "embers")) e->shape = EmitterShape::parts;
    b.flicker(b.light(Vec3{1.0f, 0.5f, 0.18f}, 24.0f, 7.0f), 12.0f, 0.5f);
}

void build(Builder& b, const std::string& name) {
    if (name == "explosion" || name == "explosion-small" || name == "explosion-big") {
        explosion(b);
    } else if (name == "scorch") {
        b.scorch(2.6f);
    } else if (name == "fire" || name == "inferno") {
        fire(b, true);
    } else if (name == "campfire") {
        fire(b, true);
        for (Entity c : b.scene.children(b.root))
            if (Emitter* e = b.scene.emitter(c)) e->shape_size = Vec3{0.35f, 0.35f, 0.35f} * b.k;
    } else if (name == "torch") {
        if (Emitter* e = b.layer("Flames", "flames")) {
            e->rate = 45.0f;
            e->shape = EmitterShape::point;
        }
        if (Emitter* e = b.layer("Embers", "embers")) e->rate = 3.0f;
        b.flicker(b.light(Vec3{1.0f, 0.6f, 0.25f}, 14.0f, 6.0f), 10.0f, 0.65f);
    } else if (name == "burning") {
        burning(b);
    } else if (name == "steam-vent") {
        b.layer("Jet", "steam-jet");
        if (Emitter* e = b.layer("Cloud", "steam")) {
            e->rate = 10.0f;
            e->speed = 2.5f;
            e->size_end = 2.2f * b.k;
        }
    } else if (name == "welding") {
        b.layer("Sparks", "spark-shower");
        if (Emitter* e = b.layer("Glare", "flash")) {
            e->loop = true;
            e->burst = 0;
            e->rate = 14.0f;
            e->duration = 0.0f;
            e->size = 0.35f * b.k;
            e->size_end = 0.45f * b.k;
            e->color = {0.85f, 0.92f, 1.0f};
            e->color_end = {0.6f, 0.8f, 1.0f};
        }
        b.flicker(b.light(Vec3{0.75f, 0.85f, 1.0f}, 20.0f, 6.0f), 24.0f, 0.2f);
    } else if (name == "electric") {
        b.layer("Arcs", "zap");
        b.flicker(b.light(Vec3{0.5f, 0.7f, 1.0f}, 16.0f, 5.0f), 30.0f, 0.0f);
    } else if (name == "smoke-grenade") {
        if (Emitter* e = b.layer("Smoke", "smoke")) {
            e->rate = 30.0f;
            e->speed = 2.0f;
            e->spread = 35.0f;
            e->gravity = -0.03f;
            e->lifetime = 9.0f;
            e->size_end = 4.5f * b.k;
            e->alpha = 0.65f;
            e->color = {0.55f, 0.56f, 0.58f};
            e->color_end = {0.75f, 0.75f, 0.77f};
            e->max_particles = 800;
        }
    } else if (name == "muzzle-flash") {
        if (Emitter* e = b.once("Flash", "flash", 1)) {
            e->size = 0.45f * b.k;
            e->size_end = 0.6f * b.k;
            e->lifetime = 0.06f;
        }
        if (Emitter* e = b.once("Sparks", "sparks", 10)) {
            e->spread = 12.0f;
            e->speed = 12.0f;
            e->lifetime = 0.25f;
            e->direction = {0.0f, 0.0f, 1.0f};
        }
        if (Emitter* e = b.once("Smoke", "smoke", 4)) {
            e->lifetime = 1.2f;
            e->size = 0.1f * b.k;
            e->size_end = 0.6f * b.k;
            e->direction = {0.0f, 0.0f, 1.0f};
        }
        b.fade(b.light(Vec3{1.0f, 0.75f, 0.4f}, 40.0f, 5.0f), 0.07f);
    } else if (name == "impact") {
        b.once("Dust", "dust-burst", 8);
        if (Emitter* e = b.once("Sparks", "sparks", 14)) {
            e->spread = 50.0f;
            e->lifetime = 0.4f;
        }
        if (Emitter* e = b.once("Chips", "debris", 6)) {
            e->size = 0.04f * b.k;
            e->size_end = 0.04f * b.k;
        }
    } else if (name == "splash") {
        b.once("Drops", "splash", 70);
        if (Emitter* e = b.once("Mist", "steam", 10)) {
            e->alpha = 0.25f;
            e->color = {0.85f, 0.92f, 1.0f};
            e->gravity = 0.1f;
        }
    }
}

float scale_for(const std::string& name) {
    if (name == "explosion-small") return 0.5f;
    if (name == "explosion-big") return 2.0f;
    if (name == "inferno") return 2.5f;
    if (name == "campfire") return 0.7f;
    return 1.0f;
}

} // namespace

const std::vector<std::string>& effect_names() {
    static const std::vector<std::string> all = {"explosion", "explosion-small", "explosion-big", "fire", "campfire", "inferno", "torch", "burning", "steam-vent", "welding", "electric", "smoke-grenade", "muzzle-flash", "impact", "splash", "scorch"};
    return all;
}

const char* effect_label(const std::string& name) {
    if (name == "explosion") return "Explosion (fireball, smoke, sparks, debris, flash)";
    if (name == "explosion-small") return "Small explosion";
    if (name == "explosion-big") return "Big explosion";
    if (name == "fire") return "Fire (flames, embers, smoke, flickering light)";
    if (name == "campfire") return "Campfire";
    if (name == "inferno") return "Inferno (huge fire)";
    if (name == "torch") return "Torch flame";
    if (name == "burning") return "Burning (flames over its parent's shape)";
    if (name == "steam-vent") return "Steam vent (jet and cloud)";
    if (name == "welding") return "Welding sparks (with glare)";
    if (name == "electric") return "Electric arcs";
    if (name == "smoke-grenade") return "Smoke grenade";
    if (name == "muzzle-flash") return "Muzzle flash";
    if (name == "impact") return "Bullet impact (dust, sparks, chips)";
    if (name == "splash") return "Water splash";
    if (name == "scorch") return "Scorch mark (black burn on the ground)";
    return "Effect";
}

bool effect_is_one_shot(const std::string& name) {
    return name.rfind("explosion", 0) == 0 || name == "muzzle-flash" || name == "impact" || name == "splash" || name == "scorch";
}

float effect_length(const std::string& name) {
    if (name.rfind("explosion", 0) == 0) return 7.0f * std::max(1.0f, scale_for(name));
    if (name == "muzzle-flash") return 1.5f;
    if (name == "impact") return 2.0f;
    if (name == "splash") return 2.0f;
    if (name == "scorch") return 300.0f;
    return 0.0f;
}

Entity add_effect(Scene& scene, const std::string& name, Entity parent, float scale, double start_time) {
    if (std::find(effect_names().begin(), effect_names().end(), name) == effect_names().end()) return Entity{};
    std::string title = name;
    for (char& ch : title)
        if (ch == '-') ch = ' ';
    if (!title.empty()) title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));
    const Entity root = scene.create(title, parent);
    Builder b{scene, root, std::max(0.05f, scale) * scale_for(name), start_time};
    build(b, name);
    return root;
}

} // namespace orchard
