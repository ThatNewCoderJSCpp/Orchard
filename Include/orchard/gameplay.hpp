#ifndef ORCHARD_GAMEPLAY_HPP
#define ORCHARD_GAMEPLAY_HPP

#include "scene.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace orchard {

class ScriptPhysics;

namespace ai {
class Expression;
}

enum class PushRepeat { once, every_pulse, continuous };
const char* to_string(PushRepeat v);
std::optional<PushRepeat> parse_push_repeat(const std::string& text);

struct Damage {
    float amount = 0.0f;
    std::string type = "physical";
    Entity attacker;
    float duration = 0.0f;
    float interval = 0.0f;
    Vec3 knockback;
    PushRepeat push = PushRepeat::once;
    float stagger = -1.0f;
    std::string key;
    bool stack = false;

    bool over_time() const { return duration != 0.0f; }
    bool forever() const { return duration < 0.0f; }
};

struct EffectDef {
    std::string name;
    std::string damage_type = "magic";
    float damage = 0.0f;
    float interval = 0.0f;
    float heal = 0.0f;
    float speed = 1.0f;
    float jump = 1.0f;
    float damage_taken = 1.0f;
    float damage_dealt = 1.0f;
    float stamina_regen = 1.0f;
    float hunger_rate = 1.0f;
    float warmth = 0.0f;
    std::string resists;
    float resist = 0.0f;
    bool stuns = false;
    int max_stacks = 1;
    float duration = 10.0f;
    std::string particle;
};

const std::vector<EffectDef>& effect_presets();
bool set_effect_field(EffectDef& def, const std::string& key, const std::string& value);

struct Projectile {
    Entity owner;
    std::string item;
    std::string ammo;
    Vec3 position;
    Vec3 velocity;
    Vec3 start;
    float mass = 0.008f;
    float drag = 0.0f;
    bool gravity = false;
    bool wind = false;
    float damage = 0.0f;
    std::string damage_type = "projectile";
    float speed0 = 1.0f;
    float penetration = 0.0f;
    float blast_radius = 0.0f;
    float fuse = 0.0f;
    float age = 0.0f;
    float life = 5.0f;
    bool sticks = false;
    bool magic = false;
    bool thrown = false;
    std::string effect;
    float effect_time = 0.0f;
    float effect_chance = 0.0f;
    std::vector<Vec3> trail;
    bool done = false;
};

struct Tracer {
    Vec3 from;
    Vec3 to;
    float age = 0.0f;
    float life = 0.08f;
    Vec3 color{1.0f, 0.85f, 0.4f};
};

struct ClimateSample {
    float temperature = 20.0f;
    Vec3 wind;
    float rain = 0.0f;
    float snow = 0.0f;
    float fog = 0.0f;
    float humidity = 0.5f;
    float lightning = 0.0f;
    bool any = false;
};

struct Exertion {
    float speed = 0.0f;
    float jump = 1.0f;
    bool sprinting = false;
    bool can_jump = true;
};

class Gameplay {
public:
    void start(Scene* scene, ScriptPhysics* physics);
    void stop();
    void tick(double dt, double now);
    bool running() const { return m_scene != nullptr; }
    double now() const { return m_now; }

    Entity holder(Entity e) const;
    float health(Entity e) const;
    float max_health(Entity e) const;
    bool set_health(Entity e, float value);
    bool set_max_health(Entity e, float value);
    float heal(Entity e, float amount);
    float damage(Entity e, float amount, Entity attacker = Entity{});
    bool kill(Entity e, Entity attacker = Entity{});
    bool dead(Entity e) const;
    bool revive(Entity e, float to = -1.0f);
    bool set_invulnerable(Entity e, bool on);
    float damage_taken(Entity e) const;
    bool died(Entity e) const;
    Entity last_attacker(Entity e) const;

    float apply_damage(Entity e, const Damage& d);
    float evaluate(const std::string& text, const std::function<bool(const std::string&, double&)>& vars, float fallback) { return formula(text, vars, fallback); }
    bool stop_damage(Entity e, const std::string& key);
    int damage_over_time_count(Entity e) const;
    std::string last_damage_type(Entity e) const;
    float resistance(Entity e, const std::string& type) const;
    bool set_resistance(Entity e, const std::string& type, float value);
    bool set_damage_formula(Entity e, const std::string& formula);
    bool set_effect_formula(Entity e, const std::string& formula);
    bool set_health_setting(Entity e, const std::string& key, const std::string& value);
    std::string health_setting(Entity e, const std::string& key) const;
    float carried_weight(Entity e) const;
    float encumbrance(Entity e) const;
    float load_speed(Entity e) const;

    bool apply_effect(Entity e, const std::string& name, float duration = 0.0f, float strength = 1.0f, Entity source = Entity{});
    bool remove_effect(Entity e, const std::string& name);
    int clear_effects(Entity e);
    int effect_stacks(Entity e, const std::string& name) const;
    float effect_time_left(Entity e, const std::string& name) const;
    float effect_strength(Entity e, const std::string& name) const;
    std::vector<std::string> effects(Entity e) const;
    void define_effect(const EffectDef& def);
    const EffectDef* effect_def(const std::string& name) const;
    bool set_effect_value(const std::string& name, const std::string& key, const std::string& value);

    bool stagger(Entity e, float seconds);
    float staggered(Entity e) const;
    bool stunned(Entity e) const;
    float move_scale(Entity e) const;
    float jump_scale(Entity e) const;
    float damage_dealt_scale(Entity e) const;

    Entity survivor(Entity e) const;
    float stat(Entity e, const std::string& key) const;
    bool set_stat(Entity e, const std::string& key, float value);
    float add_stat(Entity e, const std::string& key, float delta);
    float eat(Entity e, float food, float saturation = 0.0f);
    float drink(Entity e, float amount);
    bool use_stamina(Entity e, float amount);
    Exertion exert(Entity e, float walk_speed, float sprint_speed, bool sprint, bool moving, bool jump, float dt);
    void jumped(Entity e);
    bool underwater(Entity e) const;

    ItemCatalog& catalog();
    const ItemDef* item_def(const std::string& id) const;
    bool define_item(const std::string& id, const std::string& key, const std::string& value);
    Entity inventory_owner(Entity e) const;
    float give(Entity e, const std::string& item, float amount);
    float take(Entity e, const std::string& item, float amount);
    float count(Entity e, const std::string& item) const;
    ItemStack* stack_at(Entity e, const std::string& list, int index);
    bool move_item(Entity from, const std::string& from_list, int from_index, Entity to, const std::string& to_list, int to_index, float amount = -1.0f, int x = -1, int y = -1, bool rotate = false);
    Entity drop_item(Entity e, const std::string& list, int index, float amount = -1.0f);
    Entity spawn_item(const std::string& item, float amount, Vec3 at, const std::string& look = "detailed");
    bool pick_up(Entity e, Entity pickup);
    Entity nearest_pickup(Entity e, float radius) const;
    bool equip(Entity e, const std::string& list, int index);
    bool unequip(Entity e, const std::string& slot);
    bool hold(Entity e, const std::string& hand, const std::string& list, int index);
    ItemStack* held(Entity e, const std::string& hand = "mainhand");
    const ItemDef* held_def(Entity e, const std::string& hand = "mainhand");
    bool holster(Entity e, const std::string& hand = "mainhand");
    bool draw(Entity e, const std::string& holster_slot, const std::string& hand = "mainhand");
    std::vector<std::string> holster_slots(Entity e) const;
    bool select_slot(Entity e, int index);
    bool scroll_slot(Entity e, int delta);
    bool use_item(Entity e, const std::string& list, int index);
    bool use_held(Entity e, bool pressed, bool held, float dt, const std::string& hand = "mainhand");
    bool release_held(Entity e, const std::string& hand = "mainhand");
    bool reload(Entity e, const std::string& hand = "mainhand");
    bool reloading(Entity e) const;
    float worn_armor(Entity e, const std::string& type) const;
    float worn_insulation(Entity e) const;
    float worn_carry_bonus(Entity e) const;
    void set_aim(Entity e, Vec3 origin, Vec3 direction);
    bool set_stance(Entity e, const std::string& stance);
    std::string stance(Entity e) const;
    void set_aiming(Entity e, bool on);
    void set_blocking(Entity e, bool on);
    float spread_now(Entity e) const;
    void set_ballistics(Ballistics mode) { m_ballistics = mode; }
    Ballistics ballistics() const { return m_ballistics; }
    Ballistics ballistics_for(Entity e, const ItemDef& weapon) const;
    void set_gravity(Vec3 g) { m_gravity = g; }
    int fire(Entity shooter, Vec3 origin, Vec3 direction, const ItemDef& weapon, float power = 1.0f);
    const std::vector<Projectile>& projectiles() const { return m_projectiles; }
    const std::vector<Tracer>& tracers() const { return m_tracers; }
    std::vector<Entity> visual_parts(Entity e) const;
    void refresh_visuals(Entity e);

    ClimateSample climate_at(Vec3 point) const;
    float climate_weight(Entity zone, Vec3 point) const;
    Entity start_weather(const std::string& preset, float duration = -1.0f, float fade_in = 2.0f, float fade_out = 2.0f, Vec3 at = Vec3{}, float radius = 0.0f);
    bool stop_weather(Entity zone, float fade_out = 2.0f);
    int stop_all_weather(float fade_out = 2.0f);
    bool set_climate(Entity zone, const std::string& key, const std::string& value);
    float climate_intensity(Entity zone) const;
    bool sheltered(Vec3 point) const;

    bool ignite(Entity e, float seconds = 10.0f);
    bool extinguish(Entity e);
    bool burning(Entity e) const;
    int explode(Vec3 at, float radius, float force, float damage, Entity attacker = Entity{}, bool effect = true);
    Entity effect(const std::string& name, Vec3 at, float scale = 1.0f, Entity parent = Entity{});
    Entity scorch(Vec3 at, float reach = 3.0f, float scale = 1.0f, Entity skip = Entity{});

    Vec3 position(Entity e) const;
    bool teleport(Entity e, Vec3 to);
    bool look_at(Entity e, Vec3 target);
    float distance(Entity a, Entity b) const;
    Entity nearest_tagged(const std::string& tag, Vec3 from, Entity skip = Entity{}) const;
    double random(double low, double high);

private:
    struct State {
        float pending = 0.0f;
        float last = 0.0f;
        bool died_pending = false;
        bool died_now = false;
        bool dead = false;
        Entity attacker;
        double hurt_at = -1e9;
        Transform start;
        bool have_start = false;
        double respawn_at = -1.0;
        Vec3 velocity;
        bool have_velocity = false;
        std::string last_type;
        double stamina_used_at = -1e9;
        bool sprinting = false;
        bool sprinted = false;
    };
    struct Dot {
        Entity target;
        Damage damage;
        double until = 0.0;
        double next_pulse = 0.0;
        double next_push = 0.0;
        bool pushed = false;
    };
    struct Active {
        Entity target;
        std::string name;
        double until = 0.0;
        float strength = 1.0f;
        int stacks = 1;
        double next_pulse = 0.0;
        Entity source;
        Entity fx;
    };
    struct Fire {
        Entity target;
        Entity effect;
        double until = 0.0;
        double next_spread = 0.0;
    };
    struct Blast {
        Vec3 at;
        float radius = 0.0f;
        float force = 0.0f;
        float damage = 0.0f;
        Entity attacker;
    };

    State& state(Entity e);
    struct Carry {
        double next_use = 0.0;
        double reload_until = -1.0;
        float bloom = 0.0f;
        float draw = 0.0f;
        bool drawing = false;
        bool aiming = false;
        bool blocking = false;
        bool aim_set = false;
        Vec3 aim_origin;
        Vec3 aim_dir{0.0f, 0.0f, 1.0f};
        Vec3 last_pos;
        float speed = 0.0f;
        std::string stance = "stand";
        std::vector<Entity> visuals;
        std::string visual_key;
        std::vector<std::string> worn_effects;
    };
    struct Zone {
        double started = 0.0;
        double stopping = -1.0;
        float stop_fade = 2.0f;
        bool temporary = false;
        Entity rain_fx;
        Vec3 gust_phase;
        double next_strike = 0.0;
        double next_wet = 0.0;
    };
    Carry& carry(Entity e);
    const Carry* carry_of(Entity e) const;
    std::vector<ItemStack>* list_of(Entity owner, const std::string& list);
    bool fits_list(Entity owner, const std::string& list, int index, const ItemStack& s, int x, int y) const;
    void tick_items(double now, float dt);
    void tick_slosh(float dt);
    void tick_projectiles(float dt);
    void tick_climate(double now, float dt);
    void hit_with(Projectile& p, Entity hit, Vec3 point, Vec3 normal);
    void detonate(Projectile& p, Vec3 at);
    Entity ray_skip(Vec3 origin, Vec3 dir, float max_distance, Entity skip, Vec3* point, Vec3* normal, float* distance) const;
    bool melee_attack(Entity e, const ItemDef& def, ItemStack& stack);
    bool cast_spell(Entity e, const ItemDef& def, ItemStack& stack);
    bool throw_item(Entity e, const ItemDef& def, float power, const std::string& hand);
    bool consume(Entity e, const ItemDef& def, ItemStack& stack);
    void aim_of(Entity e, Vec3& origin, Vec3& dir) const;
    float hurt(Entity e, const Damage& d, float amount);
    void push(Entity e, Vec3 impulse, float stagger);
    float formula(const std::string& text, const std::function<bool(const std::string&, double&)>& vars, float fallback);
    void tick_damage(double now, float dt);
    void tick_effects(double now, float dt);
    void tick_survival(Entity e, Survival& sv, double now, float dt);
    float armored(Health& hp, const Damage& d, float amount, const std::function<bool(const std::string&, double&)>& vars);
    Active* active(Entity e, const std::string& name);
    const Active* active(Entity e, const std::string& name) const;
    float effect_product(Entity e, float EffectDef::*field) const;
    float effect_sum(Entity e, float EffectDef::*field) const;
    void die(Entity e, Entity attacker);
    float size_of(Entity e) const;
    float facing_area(Entity body, Vec3 dir) const;
    void char_it(Entity e, float dt, Entity skip);

    Scene* m_scene = nullptr;
    ScriptPhysics* m_physics = nullptr;
    double m_now = 0.0;
    std::unordered_map<std::uint64_t, State> m_state;
    std::vector<Fire> m_fires;
    std::vector<std::pair<Entity, double>> m_temporary;
    std::vector<Entity> m_doomed;
    std::vector<Blast> m_blasts;
    std::vector<Dot> m_dots;
    std::vector<Active> m_effects;
    std::unordered_map<std::uint64_t, Carry> m_carry;
    std::unordered_map<std::uint64_t, Zone> m_zones;
    std::vector<Projectile> m_projectiles;
    std::vector<Tracer> m_tracers;
    std::vector<std::pair<Entity, double>> m_respawns;
    std::unordered_map<std::uint64_t, double> m_next_wet;
    struct Slosh {
        Vec3 normal{0.0f, 1.0f, 0.0f};
        Vec3 speed;
        Vec3 last_pos;
        Vec3 last_vel;
        bool ready = false;
    };
    std::unordered_map<std::uint64_t, Slosh> m_slosh;
    Ballistics m_ballistics = Ballistics::cone;
    Vec3 m_gravity{0.0f, -9.81f, 0.0f};
    std::map<std::string, EffectDef> m_defs;
    mutable std::map<std::string, std::shared_ptr<const ai::Expression>> m_formulas;
    std::mt19937 m_rng{12345u};
};

} // namespace orchard

#endif // ORCHARD_GAMEPLAY_HPP
