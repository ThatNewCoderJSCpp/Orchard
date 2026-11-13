#ifndef ORCHARD_ITEMS_HPP
#define ORCHARD_ITEMS_HPP

#include "math.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace orchard {

enum class ItemKind { misc, gun, melee, bow, magic, throwable, ammo, potion, food, drink, medical, clothing, armor, trinket, container, holster, tool, material, key, magazine };
const char* to_string(ItemKind k);
std::optional<ItemKind> parse_item_kind(const std::string& text);
const std::vector<ItemKind>& item_kinds();

enum class ItemSize { tiny, small, medium, large, huge };
const char* to_string(ItemSize s);
std::optional<ItemSize> parse_item_size(const std::string& text);

enum class ItemLook { simple, detailed, realistic };
const char* to_string(ItemLook l);
std::optional<ItemLook> parse_item_look(const std::string& text);

enum class Ballistics { hitscan, cone, travel, drop, realistic };
const char* to_string(Ballistics b);
std::optional<Ballistics> parse_ballistics(const std::string& text);
const std::vector<Ballistics>& ballistics_modes();
const char* ballistics_label(Ballistics b);

struct ItemDef {
    std::string id;
    std::string name;
    std::string category;
    std::string shape = "box";
    std::string description;
    ItemKind kind = ItemKind::misc;
    ItemSize size = ItemSize::small;
    float weight = 0.1f;
    float volume = 0.1f;
    float max_stack = 1.0f;
    bool continuous = false;
    std::string unit;
    int grid_w = 1;
    int grid_h = 1;
    float length = 0.3f;
    Vec3 color{0.55f, 0.55f, 0.55f};
    Vec3 accent{0.25f, 0.2f, 0.15f};
    std::string texture;
    std::vector<std::string> tags;

    float damage = 0.0f;
    std::string damage_type;
    float knockback = 0.0f;
    float rate = 1.0f;
    float range = 2.0f;
    int magazine = 0;
    std::string ammo;
    float reload = 0.0f;
    float speed = 0.0f;
    float projectile_mass = 0.0f;
    float drag = 0.0f;
    float spread = 0.0f;
    float recoil = 0.0f;
    int pellets = 1;
    bool automatic = false;
    bool two_handed = false;
    float stamina = 0.0f;
    float block = 0.0f;
    float penetration = 0.0f;
    float blast_radius = 0.0f;
    float fuse = 0.0f;
    float draw_time = 0.0f;
    float mana = 0.0f;
    std::string spell;
    std::string ballistics;
    std::string effect;
    float effect_time = 0.0f;
    float effect_chance = 0.0f;

    std::string use_effect;
    float use_time = 0.0f;
    float use_strength = 1.0f;
    float heal = 0.0f;
    float food = 0.0f;
    float saturation = 0.0f;
    float water = 0.0f;
    float mana_restore = 0.0f;
    std::string cures;

    std::string wear;
    float armor = 0.0f;
    std::string armor_types;
    float insulation = 0.0f;
    float carry_bonus = 0.0f;
    int slots_w = 0;
    int slots_h = 0;
    float capacity = 0.0f;
    std::string holster_kinds;
    ItemSize holster_size = ItemSize::tiny;
    int holster_count = 0;

    std::string caliber;
    std::string also_fires;
    std::string feed;
    std::string magazines;
    std::string fits;
    float grain = 0.0f;
    float cartridge_grain = 0.0f;
    std::string variants;
    std::string default_variant;
    std::string cartridge_class;
    std::string aliases;

    std::string effects;
    std::string extra_damage;
    std::string worn_effects;
    std::string resists;
    std::string thorns;
    std::string thorns_damage;

    std::string sound_fire;
    std::string sound_reload;
    std::string sound_aim;
    std::string sound_use;

    std::string action;
    std::string gun_class;
    std::string country;
    int year = 0;
    std::string fire_modes;
    int burst = 3;
    bool slide_lock = false;
    float cycle_time = 0.0f;
    float jam_chance = 0.0f;
    std::string maker;
    std::string sound_cycle;
    std::string sound_empty;
    std::string surface_ease;
    std::string cycle;
    bool hammer = false;

    bool has(const std::string& tag) const;
    bool stackable() const { return max_stack != 1.0f; }
    bool weapon() const { return kind == ItemKind::gun || kind == ItemKind::melee || kind == ItemKind::bow || kind == ItemKind::magic || kind == ItemKind::throwable; }
    bool wearable() const { return !wear.empty(); }
};

const std::vector<ItemDef>& item_library();
const ItemDef* builtin_item(const std::string& id);
std::vector<std::string> item_categories();
bool set_item_field(ItemDef& def, const std::string& key, const std::string& value);
std::string item_field(const ItemDef& def, const std::string& key);

struct ItemStack {
    std::string item;
    float amount = 0.0f;
    int x = -1;
    int y = -1;
    bool rotated = false;
    float durability = 1.0f;
    float loaded = 0.0f;
    std::vector<ItemStack> contents;
    std::string variant;
    std::string ammo;
    std::string mag;
    std::string enchant;
    std::string action;
    std::string fire_mode;

    bool empty() const { return item.empty() || amount <= 0.0f; }
    void clear() { *this = ItemStack{}; }
};

std::string stack_text(const ItemStack& s);
std::optional<ItemStack> parse_stack(const std::string& text);
bool can_merge(const ItemStack& a, const ItemStack& b);
const std::vector<std::string>& item_field_keys();

struct BulletType {
    std::string id;
    std::string name;
    std::string description;
    float damage = 1.0f;
    float penetration = 1.0f;
    float armor = 1.0f;
    float drag = 1.0f;
    float spread = 1.0f;
    float grain = 1.0f;
    float ricochet = 1.0f;
    float speed = 1.0f;
    float stagger = 1.0f;
    int pellets = 0;
    bool breaks_up = false;
    bool tracer = false;
    std::string tip = "round";
    Vec3 tip_color{0.72f, 0.45f, 0.2f};
    std::string effects;
    std::string extra_damage;
    std::string damage_type;
    std::string classes;
};

const std::vector<BulletType>& bullet_types();

struct EffectRoll {
    std::string effect;
    float duration = 3.0f;
    float chance = 1.0f;
    float strength = 1.0f;
};

std::vector<EffectRoll> parse_effect_rolls(const std::string& text);
std::map<std::string, float> parse_amounts(const std::string& text);
std::string amounts_text(const std::map<std::string, float>& amounts);

enum class InventoryStyle { slots, grid, weight, volume, pouches };
const char* to_string(InventoryStyle s);
std::optional<InventoryStyle> parse_inventory_style(const std::string& text);
const std::vector<InventoryStyle>& inventory_styles();
const char* inventory_style_label(InventoryStyle s);

const std::vector<std::string>& inventory_presets();
const char* inventory_preset_label(const std::string& preset);
const std::vector<std::string>& hud_anchors();
const std::vector<std::string>& wear_slots();
const char* wear_slot_label(const std::string& slot);

struct Inventory {
    bool enabled = true;
    std::string preset = "minecraft";
    InventoryStyle style = InventoryStyle::slots;
    int columns = 9;
    int rows = 3;
    float max_weight = 0.0f;
    float max_volume = 0.0f;
    float stack_limit = 0.0f;
    bool whole_counts = false;
    bool count_weight = true;
    std::string open_keys = "E,Tab";
    std::string anchor = "center";
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float cell = 48.0f;
    float padding = 4.0f;
    float opacity = 0.9f;
    std::string title = "Inventory";

    bool hotbar = true;
    int hotbar_slots = 9;
    bool hotbar_own = true;
    std::string hotbar_anchor = "bottom";
    float hotbar_x = 0.0f;
    float hotbar_y = -16.0f;
    float hotbar_cell = 52.0f;
    float hotbar_padding = 4.0f;
    bool hotbar_vertical = false;
    std::string hotbar_keys = "1,2,3,4,5,6,7,8,9,0";
    bool hotbar_scroll = true;
    int selected = 0;

    bool equipment = true;
    bool require_holsters = false;
    std::string container_mode = "beside";
    std::string container_keys = "B";
    std::string interact_keys = "F";
    std::string use_keys = "Mouse1";
    std::string alt_keys = "Mouse2";
    std::string reload_keys = "R";
    std::string fire_mode_keys = "X";
    std::string drop_keys = "G";
    std::string holster_keys = "H";
    std::string ballistics;
    bool stance_accuracy = true;
    bool show_weight = true;
    std::string stack_measure = "count";
    bool worn_weight = true;
    std::string reload_mode;
    std::string actions = "left_click=pick_all, left_click+cursor=place_all, right_click=pick_half, right_click+cursor=place_one, shift+left_click=quick_move, shift+right_click=use, middle_click=none, hover=tooltip, key:G+cursor=drop_cursor, key:R+cursor=rotate, key:Q=drop_one";
    float place_amount = 1.0f;

    std::vector<ItemStack> items;
    std::vector<ItemStack> hotbar_items;
    std::vector<std::string> binds;
    std::map<std::string, ItemStack> worn;
    std::map<std::string, ItemStack> holstered;
};

bool apply_inventory_preset(Inventory& inv, const std::string& preset);
std::string inventory_action(const Inventory& inv, const std::string& event);
bool set_inventory_action(Inventory& inv, const std::string& event, const std::string& action);
const std::vector<std::string>& inventory_events();
const std::vector<std::pair<std::string, std::string>>& inventory_action_choices();
const std::vector<std::string>& inventory_settings();
bool set_inventory_field(Inventory& inv, const std::string& key, const std::string& value);
std::string inventory_field(const Inventory& inv, const std::string& key);

struct Pickup {
    ItemStack stack;
    bool auto_pickup = false;
    float respawn = 0.0f;
    std::string look = "detailed";
    std::string fill;
};

struct ItemCatalog {
    std::map<std::string, ItemDef> custom;
    std::map<std::string, BulletType> bullets;
    const ItemDef* find(const std::string& id) const;
    ItemDef& define(const std::string& id);
    const BulletType* bullet(const std::string& id) const;
    BulletType& define_bullet(const std::string& id);
};

bool set_bullet_field(BulletType& b, const std::string& key, const std::string& value);
std::string bullet_field(const BulletType& b, const std::string& key);
const std::vector<std::string>& bullet_field_keys();

constexpr float grains_per_kg = 15432.3584f;
bool ammo_fits(const ItemDef& gun, const ItemDef& ammo);
bool magazine_fits(const ItemDef& gun, const ItemDef& magazine);
bool magazine_takes(const ItemDef& magazine, const ItemDef& ammo);
std::vector<std::string> ammo_variants(const ItemCatalog& cat, const ItemDef& ammo);
std::string default_variant(const ItemCatalog& cat, const ItemDef& ammo);
float round_weight(const ItemCatalog& cat, const ItemDef& ammo, const std::string& variant);
float bullet_grain(const ItemCatalog& cat, const ItemDef& ammo, const std::string& variant);
std::vector<const ItemDef*> guns_for_ammo(const ItemCatalog& cat, const ItemDef& ammo);
std::vector<const ItemDef*> ammo_for_gun(const ItemCatalog& cat, const ItemDef& gun);
std::vector<const ItemDef*> magazines_for_gun(const ItemCatalog& cat, const ItemDef& gun);
std::string ammo_info(const ItemCatalog& cat, const std::string& id);
std::string item_display_name(const ItemCatalog& cat, const ItemStack& s);

const std::vector<std::pair<std::string, std::string>>& gun_actions();
const std::vector<std::pair<std::string, std::string>>& gun_classes();
const char* gun_action_label(const std::string& action);
const char* gun_class_label(const std::string& gun_class);
bool manual_action(const ItemDef& gun);
int burst_size(const ItemDef& gun, const std::string& mode);
std::string gun_action_of(const ItemDef& gun);
std::vector<std::string> fire_modes_of(const ItemDef& gun);
std::string gun_era(int year);
std::string gun_filter_value(const ItemCatalog& cat, const ItemDef& gun, const std::string& key);
bool gun_matches(const ItemCatalog& cat, const ItemDef& gun, const std::string& filter);
std::vector<const ItemDef*> find_guns(const ItemCatalog& cat, const std::string& filter);
std::vector<std::string> gun_filter_values(const ItemCatalog& cat, const std::string& key, const std::string& filter = std::string());
const std::vector<std::pair<std::string, std::string>>& gun_filter_keys();
std::string gun_info(const ItemCatalog& cat, const std::string& id);
std::string weight_text(float kg, const std::string& unit = std::string());

struct GameRule {
    std::string key;
    std::string fallback;
    std::string kind;
    std::string label;
    std::string options;
    std::string help;
    std::string group;
};

const std::vector<std::string>& item_sound_names();
bool write_item_sound(const std::string& name, const std::string& path);

const std::vector<GameRule>& game_rules();
const GameRule* game_rule(const std::string& key);

float stack_cap(const Inventory& inv, const ItemDef& def);
float stack_weight(const ItemCatalog& cat, const ItemStack& s);
float stack_volume(const ItemCatalog& cat, const ItemStack& s);
float inventory_weight(const ItemCatalog& cat, const Inventory& inv);
float inventory_volume(const ItemCatalog& cat, const Inventory& inv);
int stack_w(const ItemCatalog& cat, const ItemStack& s);
int stack_h(const ItemCatalog& cat, const ItemStack& s);
bool grid_fits(const ItemCatalog& cat, const std::vector<ItemStack>& items, int columns, int rows, int x, int y, int w, int h, int ignore = -1);
bool grid_place(const ItemCatalog& cat, const std::vector<ItemStack>& items, int columns, int rows, int w, int h, int& x, int& y, bool& rotated);
float inventory_add(const ItemCatalog& cat, Inventory& inv, const std::string& item, float amount, bool hotbar_first = true);
float inventory_add_stack(const ItemCatalog& cat, Inventory& inv, const ItemStack& proto, float amount, bool hotbar_first = true);
float inventory_take_variant(const ItemCatalog& cat, Inventory& inv, const std::string& item, const std::string& variant, float amount);
float inventory_count_variant(const Inventory& inv, const std::string& item, const std::string& variant);
float inventory_take(const ItemCatalog& cat, Inventory& inv, const std::string& item, float amount);
float inventory_count(const Inventory& inv, const std::string& item);
float container_add(const ItemCatalog& cat, ItemStack& container, const ItemDef& def, const std::string& item, float amount);
float container_add_stack(const ItemCatalog& cat, ItemStack& container, const ItemDef& cdef, const ItemStack& proto, float amount);
bool fits_holster(const ItemDef& holster, const ItemDef& item);

} // namespace orchard

#endif // ORCHARD_ITEMS_HPP
