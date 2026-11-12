#include <orchard/items.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <set>

namespace orchard {

namespace {

std::string num(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(v));
    return buf;
}

std::vector<std::string> split_list(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : text + ",") {
        if (c == ',' || c == ';') {
            const std::size_t a = cur.find_first_not_of(" \t");
            const std::size_t b = cur.find_last_not_of(" \t");
            if (a != std::string::npos) out.push_back(cur.substr(a, b - a + 1));
            cur.clear();
        } else {
            cur += c;
        }
    }
    return out;
}

constexpr float unlimited = std::numeric_limits<float>::infinity();

} // namespace

const char* to_string(ItemKind k) {
    switch (k) {
    case ItemKind::misc: return "misc";
    case ItemKind::gun: return "gun";
    case ItemKind::melee: return "melee";
    case ItemKind::bow: return "bow";
    case ItemKind::magic: return "magic";
    case ItemKind::throwable: return "throwable";
    case ItemKind::ammo: return "ammo";
    case ItemKind::potion: return "potion";
    case ItemKind::food: return "food";
    case ItemKind::drink: return "drink";
    case ItemKind::medical: return "medical";
    case ItemKind::clothing: return "clothing";
    case ItemKind::armor: return "armor";
    case ItemKind::trinket: return "trinket";
    case ItemKind::container: return "container";
    case ItemKind::holster: return "holster";
    case ItemKind::tool: return "tool";
    case ItemKind::material: return "material";
    case ItemKind::key: return "key";
    }
    return "misc";
}

const std::vector<ItemKind>& item_kinds() {
    static const std::vector<ItemKind> all = {ItemKind::misc, ItemKind::gun, ItemKind::melee, ItemKind::bow, ItemKind::magic, ItemKind::throwable, ItemKind::ammo, ItemKind::potion, ItemKind::food, ItemKind::drink, ItemKind::medical, ItemKind::clothing, ItemKind::armor, ItemKind::trinket, ItemKind::container, ItemKind::holster, ItemKind::tool, ItemKind::material, ItemKind::key};
    return all;
}

std::optional<ItemKind> parse_item_kind(const std::string& text) {
    for (ItemKind k : item_kinds())
        if (text == to_string(k)) return k;
    return std::nullopt;
}

const char* to_string(ItemSize s) {
    switch (s) {
    case ItemSize::tiny: return "tiny";
    case ItemSize::small: return "small";
    case ItemSize::medium: return "medium";
    case ItemSize::large: return "large";
    case ItemSize::huge: return "huge";
    }
    return "small";
}

std::optional<ItemSize> parse_item_size(const std::string& text) {
    for (ItemSize s : {ItemSize::tiny, ItemSize::small, ItemSize::medium, ItemSize::large, ItemSize::huge})
        if (text == to_string(s)) return s;
    return std::nullopt;
}

const char* to_string(ItemLook l) {
    switch (l) {
    case ItemLook::simple: return "simple";
    case ItemLook::detailed: return "detailed";
    case ItemLook::realistic: return "realistic";
    }
    return "detailed";
}

std::optional<ItemLook> parse_item_look(const std::string& text) {
    for (ItemLook l : {ItemLook::simple, ItemLook::detailed, ItemLook::realistic})
        if (text == to_string(l)) return l;
    return std::nullopt;
}

const char* to_string(Ballistics b) {
    switch (b) {
    case Ballistics::hitscan: return "hitscan";
    case Ballistics::cone: return "cone";
    case Ballistics::travel: return "travel";
    case Ballistics::drop: return "drop";
    case Ballistics::realistic: return "realistic";
    }
    return "cone";
}

std::optional<Ballistics> parse_ballistics(const std::string& text) {
    for (Ballistics b : ballistics_modes())
        if (text == to_string(b)) return b;
    return std::nullopt;
}

const std::vector<Ballistics>& ballistics_modes() {
    static const std::vector<Ballistics> all = {Ballistics::hitscan, Ballistics::cone, Ballistics::travel, Ballistics::drop, Ballistics::realistic};
    return all;
}

const char* ballistics_label(Ballistics b) {
    switch (b) {
    case Ballistics::hitscan: return "Hitscan (instant, perfectly accurate)";
    case Ballistics::cone: return "Accuracy cone (instant, spread grows with stance and movement)";
    case Ballistics::travel: return "Travel time (simulated, no drop; tracers show the real path)";
    case Ballistics::drop: return "Travel time and drop (gravity)";
    case Ballistics::realistic: return "Realistic (gravity, air drag, wind, penetration)";
    }
    return "";
}

bool ItemDef::has(const std::string& tag) const {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

const ItemDef* builtin_item(const std::string& id) {
    static const std::map<std::string, std::size_t> index = [] {
        std::map<std::string, std::size_t> m;
        const std::vector<ItemDef>& all = item_library();
        for (std::size_t i = 0; i < all.size(); ++i) m[all[i].id] = i;
        return m;
    }();
    const auto it = index.find(id);
    return it == index.end() ? nullptr : &item_library()[it->second];
}

std::vector<std::string> item_categories() {
    std::vector<std::string> out;
    std::set<std::string> seen;
    for (const ItemDef& d : item_library())
        if (seen.insert(d.category).second) out.push_back(d.category);
    return out;
}

bool set_item_field(ItemDef& d, const std::string& key, const std::string& value) {
    const float x = static_cast<float>(std::atof(value.c_str()));
    const int n = std::atoi(value.c_str());
    const bool on = value != "0" && value != "false" && !value.empty();
    auto vec = [&](Vec3& v) {
        float a = 0.0f, b = 0.0f, c = 0.0f;
        if (std::sscanf(value.c_str(), "%f,%f,%f", &a, &b, &c) == 3) v = Vec3{a, b, c};
    };
    if (key == "name") d.name = value;
    else if (key == "category") d.category = value;
    else if (key == "shape") d.shape = value;
    else if (key == "description") d.description = value;
    else if (key == "kind") {
        const auto k = parse_item_kind(value);
        if (!k) return false;
        d.kind = *k;
    } else if (key == "size") {
        const auto s = parse_item_size(value);
        if (!s) return false;
        d.size = *s;
    } else if (key == "weight") d.weight = std::max(0.0f, x);
    else if (key == "volume") d.volume = std::max(0.0f, x);
    else if (key == "max_stack" || key == "stack") d.max_stack = std::max(0.0f, x);
    else if (key == "continuous") d.continuous = on;
    else if (key == "unit") d.unit = value;
    else if (key == "grid_w") d.grid_w = std::max(1, n);
    else if (key == "grid_h") d.grid_h = std::max(1, n);
    else if (key == "length") d.length = std::max(0.01f, x);
    else if (key == "color") vec(d.color);
    else if (key == "accent") vec(d.accent);
    else if (key == "texture") d.texture = value;
    else if (key == "tags") d.tags = split_list(value);
    else if (key == "damage") d.damage = x;
    else if (key == "damage_type") d.damage_type = value;
    else if (key == "knockback") d.knockback = std::max(0.0f, x);
    else if (key == "rate") d.rate = std::max(0.01f, x);
    else if (key == "range") d.range = std::max(0.0f, x);
    else if (key == "magazine") d.magazine = std::max(0, n);
    else if (key == "ammo") d.ammo = value;
    else if (key == "reload") d.reload = std::max(0.0f, x);
    else if (key == "speed") d.speed = std::max(0.0f, x);
    else if (key == "projectile_mass") d.projectile_mass = std::max(0.0f, x);
    else if (key == "drag") d.drag = std::max(0.0f, x);
    else if (key == "spread") d.spread = std::max(0.0f, x);
    else if (key == "recoil") d.recoil = std::max(0.0f, x);
    else if (key == "pellets") d.pellets = std::max(1, n);
    else if (key == "automatic") d.automatic = on;
    else if (key == "two_handed") d.two_handed = on;
    else if (key == "stamina") d.stamina = std::max(0.0f, x);
    else if (key == "block") d.block = std::clamp(x, 0.0f, 1.0f);
    else if (key == "penetration") d.penetration = std::max(0.0f, x);
    else if (key == "blast_radius") d.blast_radius = std::max(0.0f, x);
    else if (key == "fuse") d.fuse = x;
    else if (key == "draw_time") d.draw_time = std::max(0.0f, x);
    else if (key == "mana") d.mana = std::max(0.0f, x);
    else if (key == "spell") d.spell = value;
    else if (key == "ballistics") d.ballistics = value;
    else if (key == "effect") d.effect = value;
    else if (key == "effect_time") d.effect_time = x;
    else if (key == "effect_chance") d.effect_chance = std::clamp(x, 0.0f, 1.0f);
    else if (key == "use_effect") d.use_effect = value;
    else if (key == "use_time") d.use_time = x;
    else if (key == "use_strength") d.use_strength = x;
    else if (key == "heal") d.heal = x;
    else if (key == "food") d.food = x;
    else if (key == "saturation") d.saturation = x;
    else if (key == "water") d.water = x;
    else if (key == "mana_restore") d.mana_restore = x;
    else if (key == "cures") d.cures = value;
    else if (key == "wear") d.wear = value;
    else if (key == "armor") d.armor = std::max(0.0f, x);
    else if (key == "armor_types") d.armor_types = value;
    else if (key == "insulation") d.insulation = x;
    else if (key == "carry_bonus") d.carry_bonus = x;
    else if (key == "slots_w") d.slots_w = std::max(0, n);
    else if (key == "slots_h") d.slots_h = std::max(0, n);
    else if (key == "capacity") d.capacity = std::max(0.0f, x);
    else if (key == "holster_kinds") d.holster_kinds = value;
    else if (key == "holster_size") {
        const auto s = parse_item_size(value);
        if (!s) return false;
        d.holster_size = *s;
    } else if (key == "holster_count") d.holster_count = std::max(0, n);
    else return false;
    return true;
}

std::string item_field(const ItemDef& d, const std::string& key) {
    auto vec = [](Vec3 v) { return num(v.x) + "," + num(v.y) + "," + num(v.z); };
    if (key == "id") return d.id;
    if (key == "name") return d.name;
    if (key == "category") return d.category;
    if (key == "shape") return d.shape;
    if (key == "description") return d.description;
    if (key == "kind") return to_string(d.kind);
    if (key == "size") return to_string(d.size);
    if (key == "weight") return num(d.weight);
    if (key == "volume") return num(d.volume);
    if (key == "max_stack" || key == "stack") return num(d.max_stack);
    if (key == "continuous") return d.continuous ? "1" : "0";
    if (key == "unit") return d.unit;
    if (key == "grid_w") return std::to_string(d.grid_w);
    if (key == "grid_h") return std::to_string(d.grid_h);
    if (key == "length") return num(d.length);
    if (key == "color") return vec(d.color);
    if (key == "accent") return vec(d.accent);
    if (key == "texture") return d.texture;
    if (key == "tags") {
        std::string out;
        for (const std::string& t : d.tags) out += (out.empty() ? "" : ",") + t;
        return out;
    }
    if (key == "damage") return num(d.damage);
    if (key == "damage_type") return d.damage_type;
    if (key == "knockback") return num(d.knockback);
    if (key == "rate") return num(d.rate);
    if (key == "range") return num(d.range);
    if (key == "magazine") return std::to_string(d.magazine);
    if (key == "ammo") return d.ammo;
    if (key == "reload") return num(d.reload);
    if (key == "speed") return num(d.speed);
    if (key == "spread") return num(d.spread);
    if (key == "pellets") return std::to_string(d.pellets);
    if (key == "automatic") return d.automatic ? "1" : "0";
    if (key == "two_handed") return d.two_handed ? "1" : "0";
    if (key == "block") return num(d.block);
    if (key == "blast_radius") return num(d.blast_radius);
    if (key == "mana") return num(d.mana);
    if (key == "spell") return d.spell;
    if (key == "ballistics") return d.ballistics;
    if (key == "effect") return d.effect;
    if (key == "use_effect") return d.use_effect;
    if (key == "heal") return num(d.heal);
    if (key == "food") return num(d.food);
    if (key == "water") return num(d.water);
    if (key == "wear") return d.wear;
    if (key == "armor") return num(d.armor);
    if (key == "insulation") return num(d.insulation);
    if (key == "capacity") return num(d.capacity);
    if (key == "slots_w") return std::to_string(d.slots_w);
    if (key == "slots_h") return std::to_string(d.slots_h);
    if (key == "holster_kinds") return d.holster_kinds;
    if (key == "holster_size") return to_string(d.holster_size);
    if (key == "holster_count") return std::to_string(d.holster_count);
    return std::string();
}

std::string stack_text(const ItemStack& s) {
    std::string out = s.item + "*" + num(s.amount);
    if (s.x >= 0 || s.y >= 0) out += "@" + std::to_string(s.x) + "," + std::to_string(s.y);
    if (s.rotated) out += "^";
    if (s.durability != 1.0f) out += "~" + num(s.durability);
    if (s.loaded != 0.0f) out += "#" + num(s.loaded);
    if (!s.contents.empty()) {
        out += "{";
        for (std::size_t i = 0; i < s.contents.size(); ++i) out += (i ? "|" : "") + stack_text(s.contents[i]);
        out += "}";
    }
    return out;
}

std::optional<ItemStack> parse_stack(const std::string& text) {
    ItemStack s;
    std::size_t i = 0;
    while (i < text.size() && text[i] != '*') s.item += text[i++];
    if (i >= text.size() || s.item.empty()) return std::nullopt;
    ++i;
    auto number = [&]() {
        const std::size_t start = i;
        while (i < text.size() && (std::isdigit(static_cast<unsigned char>(text[i])) || text[i] == '.' || text[i] == '-' || text[i] == 'e' || text[i] == 'E' || text[i] == '+')) ++i;
        return static_cast<float>(std::atof(text.substr(start, i - start).c_str()));
    };
    s.amount = number();
    while (i < text.size()) {
        const char c = text[i++];
        if (c == '@') {
            s.x = static_cast<int>(number());
            if (i < text.size() && text[i] == ',') ++i;
            s.y = static_cast<int>(number());
        } else if (c == '^') {
            s.rotated = true;
        } else if (c == '~') {
            s.durability = number();
        } else if (c == '#') {
            s.loaded = number();
        } else if (c == '{') {
            int depth = 1;
            std::string part;
            while (i < text.size() && depth > 0) {
                const char d = text[i++];
                if (d == '{') ++depth;
                if (d == '}' && --depth == 0) break;
                if (d == '|' && depth == 1) {
                    if (const auto child = parse_stack(part)) s.contents.push_back(*child);
                    part.clear();
                    continue;
                }
                part += d;
            }
            if (!part.empty())
                if (const auto child = parse_stack(part)) s.contents.push_back(*child);
        } else {
            return std::nullopt;
        }
    }
    return s;
}

const char* to_string(InventoryStyle s) {
    switch (s) {
    case InventoryStyle::slots: return "slots";
    case InventoryStyle::grid: return "grid";
    case InventoryStyle::weight: return "weight";
    case InventoryStyle::volume: return "volume";
    case InventoryStyle::pouches: return "pouches";
    }
    return "slots";
}

std::optional<InventoryStyle> parse_inventory_style(const std::string& text) {
    for (InventoryStyle s : inventory_styles())
        if (text == to_string(s)) return s;
    return std::nullopt;
}

const std::vector<InventoryStyle>& inventory_styles() {
    static const std::vector<InventoryStyle> all = {InventoryStyle::slots, InventoryStyle::grid, InventoryStyle::weight, InventoryStyle::volume, InventoryStyle::pouches};
    return all;
}

const char* inventory_style_label(InventoryStyle s) {
    switch (s) {
    case InventoryStyle::slots: return "Slots (one stack per square, like Minecraft)";
    case InventoryStyle::grid: return "Grid (items take up space and rotate, like Tarkov)";
    case InventoryStyle::weight: return "Weight list (limited by weight, like Skyrim or No Room In Hell)";
    case InventoryStyle::volume: return "Backpack (limited by volume, like Saints & Sinners or Into the Radius)";
    case InventoryStyle::pouches: return "Pouches (sorted into tabs by kind, like Zelda)";
    }
    return "";
}

const std::vector<std::string>& inventory_presets() {
    static const std::vector<std::string> all = {"minecraft", "terraria", "tarkov", "ghosts_of_tabor", "dayz", "no_room_in_hell", "saints_and_sinners", "into_the_radius", "resident_evil", "diablo", "skyrim", "zelda", "custom"};
    return all;
}

const char* inventory_preset_label(const std::string& p) {
    if (p == "minecraft") return "Minecraft (9 hotbar + 27 slots, stacks of 64)";
    if (p == "terraria") return "Terraria (50 slots, top row is the hotbar)";
    if (p == "tarkov") return "Escape from Tarkov (grid pockets, rig and backpack, holsters needed)";
    if (p == "ghosts_of_tabor") return "Ghosts of Tabor (grid containers on your body, holsters needed)";
    if (p == "dayz") return "DayZ (storage comes from what you wear)";
    if (p == "no_room_in_hell") return "No Room In Hell (weight limit, few quick slots)";
    if (p == "saints_and_sinners") return "Saints & Sinners (backpack by volume, body holsters)";
    if (p == "into_the_radius") return "Into the Radius (backpack and rig by volume, body holsters)";
    if (p == "resident_evil") return "Resident Evil (attaché case grid, no stacking of guns)";
    if (p == "diablo") return "Diablo (10 x 4 grid)";
    if (p == "skyrim") return "Skyrim (weight list, favourites bar)";
    if (p == "zelda") return "Zelda (pouches by kind, no weight)";
    return "Custom";
}

const std::vector<std::string>& hud_anchors() {
    static const std::vector<std::string> all = {"center", "top", "bottom", "left", "right", "top_left", "top_right", "bottom_left", "bottom_right"};
    return all;
}

const std::vector<std::string>& wear_slots() {
    static const std::vector<std::string> all = {"head", "eyes", "face", "neck", "shoulders", "torso", "vest", "over", "back", "hands", "waist", "legs", "feet", "ring", "ring2", "hip", "thigh", "sling", "back_slot", "side"};
    return all;
}

const char* wear_slot_label(const std::string& s) {
    if (s == "head") return "Head";
    if (s == "eyes") return "Eyes";
    if (s == "face") return "Face";
    if (s == "neck") return "Neck";
    if (s == "shoulders") return "Shoulders";
    if (s == "torso") return "Torso";
    if (s == "vest") return "Vest / rig / body armor";
    if (s == "over") return "Over everything";
    if (s == "back") return "Back (backpack)";
    if (s == "hands") return "Hands";
    if (s == "waist") return "Belt";
    if (s == "legs") return "Legs";
    if (s == "feet") return "Feet";
    if (s == "ring" || s == "ring2") return "Ring";
    if (s == "hip") return "Hip holster";
    if (s == "thigh") return "Thigh holster";
    if (s == "sling") return "Sling";
    if (s == "back_slot") return "Back scabbard";
    if (s == "side") return "Side bag";
    if (s == "mainhand") return "Main hand";
    if (s == "offhand") return "Off hand";
    return "Slot";
}

bool apply_inventory_preset(Inventory& inv, const std::string& p) {
    if (std::find(inventory_presets().begin(), inventory_presets().end(), p) == inventory_presets().end()) return false;
    inv.preset = p;
    if (p == "custom") return true;
    const std::vector<ItemStack> keep = inv.items;
    const std::vector<ItemStack> keep_bar = inv.hotbar_items;
    Inventory fresh;
    fresh.worn = inv.worn;
    fresh.holstered = inv.holstered;
    fresh.preset = p;
    inv = fresh;
    if (p == "minecraft") {
        inv.style = InventoryStyle::slots;
        inv.columns = 9;
        inv.rows = 3;
        inv.stack_limit = 64.0f;
        inv.whole_counts = true;
        inv.open_keys = "E";
        inv.hotbar_slots = 9;
        inv.hotbar_own = true;
        inv.equipment = true;
    } else if (p == "terraria") {
        inv.style = InventoryStyle::slots;
        inv.columns = 10;
        inv.rows = 5;
        inv.stack_limit = 9999.0f;
        inv.whole_counts = true;
        inv.open_keys = "Escape,I";
        inv.hotbar_slots = 10;
        inv.hotbar_own = false;
    } else if (p == "tarkov") {
        inv.style = InventoryStyle::grid;
        inv.columns = 4;
        inv.rows = 1;
        inv.title = "Pockets";
        inv.open_keys = "Tab";
        inv.hotbar_slots = 7;
        inv.hotbar_own = false;
        inv.hotbar_keys = "4,5,6,7,8,9,0";
        inv.require_holsters = true;
        inv.container_mode = "beside";
        inv.cell = 44.0f;
    } else if (p == "ghosts_of_tabor") {
        inv.style = InventoryStyle::grid;
        inv.columns = 2;
        inv.rows = 2;
        inv.title = "Pockets";
        inv.open_keys = "Tab";
        inv.hotbar = false;
        inv.require_holsters = true;
        inv.container_mode = "beside";
    } else if (p == "dayz") {
        inv.style = InventoryStyle::grid;
        inv.columns = 2;
        inv.rows = 1;
        inv.title = "Hands";
        inv.open_keys = "Tab";
        inv.hotbar_slots = 9;
        inv.hotbar_own = false;
        inv.require_holsters = false;
        inv.container_mode = "beside";
    } else if (p == "no_room_in_hell") {
        inv.style = InventoryStyle::weight;
        inv.max_weight = 25.0f;
        inv.open_keys = "I";
        inv.hotbar_slots = 6;
        inv.hotbar_own = false;
        inv.title = "Inventory";
    } else if (p == "saints_and_sinners") {
        inv.style = InventoryStyle::volume;
        inv.max_volume = 25.0f;
        inv.open_keys = "Tab";
        inv.hotbar = false;
        inv.require_holsters = true;
        inv.title = "Backpack";
    } else if (p == "into_the_radius") {
        inv.style = InventoryStyle::volume;
        inv.max_volume = 30.0f;
        inv.open_keys = "Tab";
        inv.hotbar = false;
        inv.require_holsters = true;
        inv.container_mode = "own_key";
        inv.title = "Backpack";
    } else if (p == "resident_evil") {
        inv.style = InventoryStyle::grid;
        inv.columns = 10;
        inv.rows = 6;
        inv.open_keys = "Tab,I";
        inv.hotbar_slots = 8;
        inv.hotbar_own = false;
        inv.title = "Attach\xC3\xA9 case";
    } else if (p == "diablo") {
        inv.style = InventoryStyle::grid;
        inv.columns = 10;
        inv.rows = 4;
        inv.open_keys = "I";
        inv.hotbar_slots = 4;
        inv.hotbar_own = true;
    } else if (p == "skyrim") {
        inv.style = InventoryStyle::weight;
        inv.max_weight = 300.0f;
        inv.stack_limit = 0.0f;
        inv.open_keys = "Tab,I";
        inv.hotbar_slots = 8;
        inv.hotbar_own = false;
    } else if (p == "zelda") {
        inv.style = InventoryStyle::pouches;
        inv.stack_limit = 999.0f;
        inv.whole_counts = true;
        inv.open_keys = "Tab";
        inv.hotbar_slots = 4;
        inv.hotbar_own = false;
    }
    for (const ItemStack& s : keep)
        if (!s.empty()) inv.items.push_back(s);
    for (const ItemStack& s : keep_bar)
        if (!s.empty()) inv.items.push_back(s);
    for (ItemStack& s : inv.items) {
        s.x = -1;
        s.y = -1;
    }
    return true;
}

namespace {

struct InvField {
    const char* key;
    int kind;
    float Inventory::*f;
    int Inventory::*i;
    bool Inventory::*b;
    std::string Inventory::*s;
};

const std::vector<InvField>& inv_fields() {
    static const std::vector<InvField> all = {
        {"enabled", 2, nullptr, nullptr, &Inventory::enabled, nullptr},
        {"preset", 3, nullptr, nullptr, nullptr, &Inventory::preset},
        {"style", 4, nullptr, nullptr, nullptr, nullptr},
        {"columns", 1, nullptr, &Inventory::columns, nullptr, nullptr},
        {"rows", 1, nullptr, &Inventory::rows, nullptr, nullptr},
        {"max_weight", 0, &Inventory::max_weight, nullptr, nullptr, nullptr},
        {"max_volume", 0, &Inventory::max_volume, nullptr, nullptr, nullptr},
        {"stack_limit", 0, &Inventory::stack_limit, nullptr, nullptr, nullptr},
        {"stack_measure", 3, nullptr, nullptr, nullptr, &Inventory::stack_measure},
        {"whole_counts", 2, nullptr, nullptr, &Inventory::whole_counts, nullptr},
        {"count_weight", 2, nullptr, nullptr, &Inventory::count_weight, nullptr},
        {"open_keys", 3, nullptr, nullptr, nullptr, &Inventory::open_keys},
        {"anchor", 3, nullptr, nullptr, nullptr, &Inventory::anchor},
        {"offset_x", 0, &Inventory::offset_x, nullptr, nullptr, nullptr},
        {"offset_y", 0, &Inventory::offset_y, nullptr, nullptr, nullptr},
        {"cell", 0, &Inventory::cell, nullptr, nullptr, nullptr},
        {"padding", 0, &Inventory::padding, nullptr, nullptr, nullptr},
        {"opacity", 0, &Inventory::opacity, nullptr, nullptr, nullptr},
        {"title", 3, nullptr, nullptr, nullptr, &Inventory::title},
        {"hotbar", 2, nullptr, nullptr, &Inventory::hotbar, nullptr},
        {"hotbar_slots", 1, nullptr, &Inventory::hotbar_slots, nullptr, nullptr},
        {"hotbar_own", 2, nullptr, nullptr, &Inventory::hotbar_own, nullptr},
        {"hotbar_anchor", 3, nullptr, nullptr, nullptr, &Inventory::hotbar_anchor},
        {"hotbar_x", 0, &Inventory::hotbar_x, nullptr, nullptr, nullptr},
        {"hotbar_y", 0, &Inventory::hotbar_y, nullptr, nullptr, nullptr},
        {"hotbar_cell", 0, &Inventory::hotbar_cell, nullptr, nullptr, nullptr},
        {"hotbar_padding", 0, &Inventory::hotbar_padding, nullptr, nullptr, nullptr},
        {"hotbar_vertical", 2, nullptr, nullptr, &Inventory::hotbar_vertical, nullptr},
        {"hotbar_keys", 3, nullptr, nullptr, nullptr, &Inventory::hotbar_keys},
        {"hotbar_scroll", 2, nullptr, nullptr, &Inventory::hotbar_scroll, nullptr},
        {"selected", 1, nullptr, &Inventory::selected, nullptr, nullptr},
        {"equipment", 2, nullptr, nullptr, &Inventory::equipment, nullptr},
        {"require_holsters", 2, nullptr, nullptr, &Inventory::require_holsters, nullptr},
        {"container_mode", 3, nullptr, nullptr, nullptr, &Inventory::container_mode},
        {"container_keys", 3, nullptr, nullptr, nullptr, &Inventory::container_keys},
        {"interact_keys", 3, nullptr, nullptr, nullptr, &Inventory::interact_keys},
        {"use_keys", 3, nullptr, nullptr, nullptr, &Inventory::use_keys},
        {"alt_keys", 3, nullptr, nullptr, nullptr, &Inventory::alt_keys},
        {"reload_keys", 3, nullptr, nullptr, nullptr, &Inventory::reload_keys},
        {"drop_keys", 3, nullptr, nullptr, nullptr, &Inventory::drop_keys},
        {"holster_keys", 3, nullptr, nullptr, nullptr, &Inventory::holster_keys},
        {"ballistics", 3, nullptr, nullptr, nullptr, &Inventory::ballistics},
        {"stance_accuracy", 2, nullptr, nullptr, &Inventory::stance_accuracy, nullptr},
        {"show_weight", 2, nullptr, nullptr, &Inventory::show_weight, nullptr},
    };
    return all;
}

} // namespace

const std::vector<std::string>& inventory_settings() {
    static const std::vector<std::string> all = [] {
        std::vector<std::string> v;
        for (const InvField& f : inv_fields()) v.push_back(f.key);
        return v;
    }();
    return all;
}

bool set_inventory_field(Inventory& inv, const std::string& key, const std::string& value) {
    for (const InvField& f : inv_fields()) {
        if (key != f.key) continue;
        switch (f.kind) {
        case 0: inv.*(f.f) = std::max(key == "offset_x" || key == "offset_y" || key == "hotbar_x" || key == "hotbar_y" ? -1e6f : 0.0f, static_cast<float>(std::atof(value.c_str()))); return true;
        case 1: inv.*(f.i) = std::max(0, std::atoi(value.c_str())); return true;
        case 2: inv.*(f.b) = value != "0" && value != "false" && !value.empty(); return true;
        case 3: inv.*(f.s) = value; return true;
        case 4: {
            const auto st = parse_inventory_style(value);
            if (!st) return false;
            inv.style = *st;
            return true;
        }
        }
    }
    return false;
}

std::string inventory_field(const Inventory& inv, const std::string& key) {
    for (const InvField& f : inv_fields()) {
        if (key != f.key) continue;
        switch (f.kind) {
        case 0: return num(inv.*(f.f));
        case 1: return std::to_string(inv.*(f.i));
        case 2: return inv.*(f.b) ? "1" : "0";
        case 3: return inv.*(f.s);
        case 4: return to_string(inv.style);
        }
    }
    return std::string();
}

const ItemDef* ItemCatalog::find(const std::string& id) const {
    if (const auto it = custom.find(id); it != custom.end()) return &it->second;
    return builtin_item(id);
}

ItemDef& ItemCatalog::define(const std::string& id) {
    auto it = custom.find(id);
    if (it != custom.end()) return it->second;
    ItemDef d;
    if (const ItemDef* base = builtin_item(id)) d = *base;
    d.id = id;
    if (d.name.empty()) d.name = id;
    if (d.category.empty()) d.category = "Custom";
    return custom.emplace(id, d).first->second;
}

float stack_cap(const Inventory& inv, const ItemDef& def) {
    float cap = def.max_stack <= 0.0f ? unlimited : def.max_stack;
    if (inv.stack_limit > 0.0f && def.max_stack != 1.0f) {
        cap = inv.stack_limit;
        if (inv.stack_measure == "weight") cap = def.weight > 0.0f ? inv.stack_limit / def.weight : unlimited;
        else if (inv.stack_measure == "volume") cap = def.volume > 0.0f ? inv.stack_limit / def.volume : unlimited;
    }
    if (inv.whole_counts && !def.continuous && std::isfinite(cap)) cap = std::max(1.0f, std::floor(cap));
    return std::max(0.0f, cap);
}

float stack_weight(const ItemCatalog& cat, const ItemStack& s) {
    if (s.empty()) return 0.0f;
    const ItemDef* d = cat.find(s.item);
    float w = d ? d->weight * s.amount : 0.0f;
    for (const ItemStack& c : s.contents) w += stack_weight(cat, c);
    return w;
}

float stack_volume(const ItemCatalog& cat, const ItemStack& s) {
    if (s.empty()) return 0.0f;
    const ItemDef* d = cat.find(s.item);
    return d ? d->volume * s.amount : 0.0f;
}

float inventory_weight(const ItemCatalog& cat, const Inventory& inv) {
    float w = 0.0f;
    for (const ItemStack& s : inv.items) w += stack_weight(cat, s);
    for (const ItemStack& s : inv.hotbar_items) w += stack_weight(cat, s);
    for (const auto& [slot, s] : inv.worn) w += stack_weight(cat, s);
    for (const auto& [slot, s] : inv.holstered) w += stack_weight(cat, s);
    return w;
}

float inventory_volume(const ItemCatalog& cat, const Inventory& inv) {
    float v = 0.0f;
    for (const ItemStack& s : inv.items) v += stack_volume(cat, s);
    return v;
}

int stack_w(const ItemCatalog& cat, const ItemStack& s) {
    const ItemDef* d = cat.find(s.item);
    const int w = d ? d->grid_w : 1, h = d ? d->grid_h : 1;
    return s.rotated ? h : w;
}

int stack_h(const ItemCatalog& cat, const ItemStack& s) {
    const ItemDef* d = cat.find(s.item);
    const int w = d ? d->grid_w : 1, h = d ? d->grid_h : 1;
    return s.rotated ? w : h;
}

bool grid_fits(const ItemCatalog& cat, const std::vector<ItemStack>& items, int columns, int rows, int x, int y, int w, int h, int ignore) {
    if (x < 0 || y < 0 || x + w > columns || y + h > rows) return false;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (static_cast<int>(i) == ignore || items[i].empty() || items[i].x < 0) continue;
        const int ox = items[i].x, oy = items[i].y, ow = stack_w(cat, items[i]), oh = stack_h(cat, items[i]);
        if (x < ox + ow && ox < x + w && y < oy + oh && oy < y + h) return false;
    }
    return true;
}

bool grid_place(const ItemCatalog& cat, const std::vector<ItemStack>& items, int columns, int rows, int w, int h, int& x, int& y, bool& rotated) {
    for (int pass = 0; pass < 2; ++pass) {
        const int pw = pass == 0 ? w : h, ph = pass == 0 ? h : w;
        if (pass == 1 && w == h) break;
        for (int yy = 0; yy + ph <= rows; ++yy)
            for (int xx = 0; xx + pw <= columns; ++xx)
                if (grid_fits(cat, items, columns, rows, xx, yy, pw, ph)) {
                    x = xx;
                    y = yy;
                    rotated = pass == 1;
                    return true;
                }
    }
    return false;
}

namespace {

float whole(const Inventory& inv, const ItemDef& def, float amount) {
    return def.continuous || !inv.whole_counts ? amount : std::floor(amount + 1e-4f);
}

float merge_into(std::vector<ItemStack>& list, const std::string& item, float amount, float cap) {
    float added = 0.0f;
    for (ItemStack& s : list) {
        if (amount - added <= 0.0f) break;
        if (s.item != item || s.empty() || !s.contents.empty()) continue;
        const float room = cap - s.amount;
        if (room <= 0.0f) continue;
        const float put = std::min(room, amount - added);
        s.amount += put;
        added += put;
    }
    return added;
}

float room_left(const ItemCatalog& cat, const Inventory& inv, const ItemDef& def) {
    float room = unlimited;
    if (inv.max_weight > 0.0f && def.weight > 0.0f) room = std::min(room, (inv.max_weight - inventory_weight(cat, inv)) / def.weight);
    if ((inv.style == InventoryStyle::volume || inv.max_volume > 0.0f) && def.volume > 0.0f && inv.max_volume > 0.0f) room = std::min(room, (inv.max_volume - inventory_volume(cat, inv)) / def.volume);
    return std::max(0.0f, room);
}

} // namespace

float container_add(const ItemCatalog& cat, ItemStack& container, const ItemDef& cdef, const std::string& item, float amount) {
    const ItemDef* def = cat.find(item);
    if (!def || amount <= 0.0f) return 0.0f;
    if (!cdef.holster_kinds.empty() && cdef.kind == ItemKind::container) {
        bool ok = false;
        for (const std::string& k : split_list(cdef.holster_kinds)) ok = ok || k == to_string(def->kind) || k == def->id || k == def->shape || def->has(k);
        if (!ok) return 0.0f;
    }
    Inventory rules;
    rules.whole_counts = !def->continuous;
    const float cap = stack_cap(rules, *def);
    if (cdef.capacity > 0.0f && def->volume > 0.0f) {
        float used = 0.0f;
        for (const ItemStack& s : container.contents) used += stack_volume(cat, s);
        amount = std::min(amount, std::max(0.0f, (cdef.capacity - used) / def->volume));
        if (rules.whole_counts) amount = std::floor(amount + 1e-4f);
    }
    if (amount <= 0.0f) return 0.0f;
    float added = merge_into(container.contents, item, amount, cap);
    const int cols = std::max(1, cdef.slots_w), rows = std::max(1, cdef.slots_h);
    while (amount - added > 1e-5f) {
        const float put = std::min(cap, amount - added);
        if (put <= 0.0f) break;
        ItemStack s;
        s.item = item;
        s.amount = rules.whole_counts ? std::floor(put + 1e-4f) : put;
        if (s.amount <= 0.0f) break;
        int x = 0, y = 0;
        bool rot = false;
        if (!grid_place(cat, container.contents, cols, rows, def->grid_w, def->grid_h, x, y, rot)) break;
        s.x = x;
        s.y = y;
        s.rotated = rot;
        container.contents.push_back(s);
        added += s.amount;
    }
    container.contents.erase(std::remove_if(container.contents.begin(), container.contents.end(), [](const ItemStack& s) { return s.empty(); }), container.contents.end());
    return added;
}

float inventory_add(const ItemCatalog& cat, Inventory& inv, const std::string& item, float amount, bool hotbar_first) {
    const ItemDef* def = cat.find(item);
    if (!def || amount <= 0.0f) return 0.0f;
    amount = std::min(whole(inv, *def, amount), room_left(cat, inv, *def));
    amount = whole(inv, *def, amount);
    if (amount <= 0.0f) return 0.0f;
    const float cap = stack_cap(inv, *def);
    if (cap <= 0.0f) return 0.0f;
    float added = 0.0f;
    const bool bar = inv.hotbar && inv.hotbar_own && inv.style == InventoryStyle::slots;
    if (bar && static_cast<int>(inv.hotbar_items.size()) < inv.hotbar_slots) inv.hotbar_items.resize(static_cast<std::size_t>(std::max(0, inv.hotbar_slots)));
    if (inv.style == InventoryStyle::slots) inv.items.resize(static_cast<std::size_t>(std::max(0, inv.columns * inv.rows)));
    if (bar && hotbar_first) added += merge_into(inv.hotbar_items, item, amount - added, cap);
    added += merge_into(inv.items, item, amount - added, cap);
    if (bar && !hotbar_first) added += merge_into(inv.hotbar_items, item, amount - added, cap);
    auto fill_slots = [&](std::vector<ItemStack>& list) {
        for (ItemStack& s : list) {
            if (amount - added <= 1e-5f) break;
            if (!s.empty()) continue;
            s = ItemStack{};
            s.item = item;
            s.amount = std::min(cap, amount - added);
            added += s.amount;
        }
    };
    if (inv.style == InventoryStyle::slots) {
        if (bar && hotbar_first) fill_slots(inv.hotbar_items);
        fill_slots(inv.items);
        if (bar && !hotbar_first) fill_slots(inv.hotbar_items);
    } else if (inv.style == InventoryStyle::grid) {
        while (amount - added > 1e-5f) {
            int x = 0, y = 0;
            bool rot = false;
            if (!grid_place(cat, inv.items, inv.columns, inv.rows, def->grid_w, def->grid_h, x, y, rot)) break;
            ItemStack s;
            s.item = item;
            s.amount = std::min(cap, amount - added);
            s.x = x;
            s.y = y;
            s.rotated = rot;
            inv.items.push_back(s);
            added += s.amount;
        }
    } else {
        while (amount - added > 1e-5f) {
            ItemStack s;
            s.item = item;
            s.amount = std::min(cap, amount - added);
            if (s.amount <= 0.0f) break;
            inv.items.push_back(s);
            added += s.amount;
            if (!std::isfinite(cap)) break;
        }
    }
    if (amount - added > 1e-5f) {
        for (const char* slot : {"vest", "waist", "back", "side", "torso", "legs"}) {
            const auto it = inv.worn.find(slot);
            if (it == inv.worn.end() || it->second.empty()) continue;
            const ItemDef* cdef = cat.find(it->second.item);
            if (!cdef || (cdef->slots_w <= 0 && cdef->capacity <= 0.0f)) continue;
            added += container_add(cat, it->second, *cdef, item, amount - added);
            if (amount - added <= 1e-5f) break;
        }
    }
    return added;
}

float inventory_take(const ItemCatalog& cat, Inventory& inv, const std::string& item, float amount) {
    static_cast<void>(cat);
    float taken = 0.0f;
    auto take_from = [&](std::vector<ItemStack>& list, bool keep_slots) {
        for (ItemStack& s : list) {
            if (amount - taken <= 1e-6f) break;
            if (s.item != item || s.empty()) continue;
            const float t = std::min(s.amount, amount - taken);
            s.amount -= t;
            taken += t;
            if (s.amount <= 1e-6f) {
                if (keep_slots) s.clear();
                else s.amount = 0.0f;
            }
        }
        if (!keep_slots) list.erase(std::remove_if(list.begin(), list.end(), [](const ItemStack& s) { return s.empty(); }), list.end());
    };
    const bool slots = inv.style == InventoryStyle::slots;
    take_from(inv.items, slots);
    take_from(inv.hotbar_items, true);
    for (auto& [slot, s] : inv.worn) take_from(s.contents, false);
    for (auto& [slot, s] : inv.holstered)
        if (s.item == item && amount - taken > 1e-6f) {
            const float t = std::min(s.amount, amount - taken);
            s.amount -= t;
            taken += t;
            if (s.amount <= 1e-6f) s.clear();
        }
    return taken;
}

float inventory_count(const Inventory& inv, const std::string& item) {
    float n = 0.0f;
    auto count = [&](const ItemStack& s, auto& self) -> void {
        if (s.item == item && !s.empty()) n += s.amount;
        for (const ItemStack& c : s.contents) self(c, self);
    };
    for (const ItemStack& s : inv.items) count(s, count);
    for (const ItemStack& s : inv.hotbar_items) count(s, count);
    for (const auto& [slot, s] : inv.worn) count(s, count);
    for (const auto& [slot, s] : inv.holstered) count(s, count);
    return n;
}

bool fits_holster(const ItemDef& holster, const ItemDef& item) {
    if (holster.holster_count <= 0 || static_cast<int>(item.size) > static_cast<int>(holster.holster_size)) return false;
    for (const std::string& k : split_list(holster.holster_kinds))
        if (k == to_string(item.kind) || k == item.shape || k == item.id || item.has(k)) return true;
    return false;
}

} // namespace orchard
