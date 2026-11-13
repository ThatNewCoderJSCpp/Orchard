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
    case ItemKind::magazine: return "magazine";
    }
    return "misc";
}

const std::vector<ItemKind>& item_kinds() {
    static const std::vector<ItemKind> all = {ItemKind::misc, ItemKind::gun, ItemKind::melee, ItemKind::bow, ItemKind::magic, ItemKind::throwable, ItemKind::ammo, ItemKind::potion, ItemKind::food, ItemKind::drink, ItemKind::medical, ItemKind::clothing, ItemKind::armor, ItemKind::trinket, ItemKind::container, ItemKind::holster, ItemKind::tool, ItemKind::material, ItemKind::key, ItemKind::magazine};
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
    else if (key == "caliber") d.caliber = value;
    else if (key == "also_fires") d.also_fires = value;
    else if (key == "feed") d.feed = value;
    else if (key == "magazines") d.magazines = value;
    else if (key == "fits") d.fits = value;
    else if (key == "grain") d.grain = std::max(0.0f, x);
    else if (key == "cartridge_grain") d.cartridge_grain = std::max(0.0f, x);
    else if (key == "variants") d.variants = value;
    else if (key == "default_variant") d.default_variant = value;
    else if (key == "cartridge_class") d.cartridge_class = value;
    else if (key == "aliases") d.aliases = value;
    else if (key == "effects") d.effects = value;
    else if (key == "extra_damage") d.extra_damage = value;
    else if (key == "worn_effects") d.worn_effects = value;
    else if (key == "resists") d.resists = value;
    else if (key == "thorns") d.thorns = value;
    else if (key == "thorns_damage") d.thorns_damage = value;
    else if (key == "sound_fire") d.sound_fire = value;
    else if (key == "sound_reload") d.sound_reload = value;
    else if (key == "sound_aim") d.sound_aim = value;
    else if (key == "sound_use") d.sound_use = value;
    else if (key == "action") d.action = value;
    else if (key == "gun_class") d.gun_class = value;
    else if (key == "country") d.country = value;
    else if (key == "year") d.year = static_cast<int>(x);
    else if (key == "fire_modes") d.fire_modes = value;
    else if (key == "burst") d.burst = std::max(1, static_cast<int>(x));
    else if (key == "slide_lock") d.slide_lock = value == "1" || value == "true" || value == "yes";
    else if (key == "cycle_time") d.cycle_time = std::max(0.0f, x);
    else if (key == "jam_chance") d.jam_chance = std::clamp(x, 0.0f, 1.0f);
    else if (key == "maker") d.maker = value;
    else if (key == "sound_cycle") d.sound_cycle = value;
    else if (key == "sound_empty") d.sound_empty = value;
    else if (key == "surface_ease") d.surface_ease = value;
    else if (key == "cycle") d.cycle = value;
    else if (key == "hammer") d.hammer = value == "1" || value == "true" || value == "yes";
    else if (key == "projectile_mass") d.projectile_mass = std::max(0.0f, x);
    else if (key == "effect_time") d.effect_time = x;
    else if (key == "effect_chance") d.effect_chance = std::clamp(x, 0.0f, 1.0f);
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
    if (key == "caliber") return d.caliber;
    if (key == "also_fires") return d.also_fires;
    if (key == "feed") return d.feed;
    if (key == "magazines") return d.magazines;
    if (key == "fits") return d.fits;
    if (key == "grain") return num(d.grain);
    if (key == "cartridge_grain") return num(d.cartridge_grain);
    if (key == "variants") return d.variants;
    if (key == "default_variant") return d.default_variant;
    if (key == "cartridge_class") return d.cartridge_class;
    if (key == "aliases") return d.aliases;
    if (key == "effects") return d.effects;
    if (key == "extra_damage") return d.extra_damage;
    if (key == "worn_effects") return d.worn_effects;
    if (key == "resists") return d.resists;
    if (key == "thorns") return d.thorns;
    if (key == "thorns_damage") return d.thorns_damage;
    if (key == "sound_fire") return d.sound_fire;
    if (key == "sound_reload") return d.sound_reload;
    if (key == "sound_aim") return d.sound_aim;
    if (key == "sound_use") return d.sound_use;
    if (key == "action") return d.action;
    if (key == "gun_class") return d.gun_class;
    if (key == "country") return d.country;
    if (key == "year") return std::to_string(d.year);
    if (key == "fire_modes") return d.fire_modes;
    if (key == "burst") return std::to_string(d.burst);
    if (key == "slide_lock") return d.slide_lock ? "1" : "0";
    if (key == "cycle_time") return num(d.cycle_time);
    if (key == "jam_chance") return num(d.jam_chance);
    if (key == "maker") return d.maker;
    if (key == "sound_cycle") return d.sound_cycle;
    if (key == "sound_empty") return d.sound_empty;
    if (key == "surface_ease") return d.surface_ease;
    if (key == "cycle") return d.cycle;
    if (key == "hammer") return d.hammer ? "1" : "0";
    if (key == "recoil") return num(d.recoil);
    if (key == "drag") return num(d.drag);
    if (key == "penetration") return num(d.penetration);
    if (key == "projectile_mass") return num(d.projectile_mass);
    if (key == "stamina") return num(d.stamina);
    if (key == "fuse") return num(d.fuse);
    if (key == "draw_time") return num(d.draw_time);
    if (key == "effect_time") return num(d.effect_time);
    if (key == "effect_chance") return num(d.effect_chance);
    if (key == "use_time") return num(d.use_time);
    if (key == "use_strength") return num(d.use_strength);
    if (key == "saturation") return num(d.saturation);
    if (key == "mana_restore") return num(d.mana_restore);
    if (key == "cures") return d.cures;
    if (key == "armor_types") return d.armor_types;
    if (key == "carry_bonus") return num(d.carry_bonus);
    return std::string();
}

const std::vector<std::string>& item_field_keys() {
    static const std::vector<std::string> all = {"name", "category", "shape", "description", "kind", "size", "weight", "volume", "max_stack", "continuous", "unit", "grid_w", "grid_h", "length", "color", "accent", "texture", "tags", "damage", "damage_type", "knockback", "rate", "range", "magazine", "ammo", "reload", "speed", "spread", "pellets", "automatic", "two_handed", "block", "blast_radius", "mana", "spell", "ballistics", "effect", "use_effect", "heal", "food", "water", "wear", "armor", "insulation", "capacity", "slots_w", "slots_h", "holster_kinds", "holster_size", "holster_count",
        "caliber", "also_fires", "feed", "magazines", "fits", "grain", "cartridge_grain", "variants", "default_variant", "cartridge_class", "aliases", "effects", "extra_damage", "worn_effects", "resists", "thorns", "thorns_damage", "sound_fire", "sound_reload", "sound_aim", "sound_use", "action", "gun_class", "country", "year", "fire_modes", "burst", "slide_lock", "cycle_time", "jam_chance", "maker", "sound_cycle", "sound_empty", "surface_ease", "cycle", "hammer",
        "recoil", "drag", "penetration", "projectile_mass", "stamina", "fuse", "draw_time", "effect_time", "effect_chance", "use_time", "use_strength", "saturation", "mana_restore", "cures", "armor_types", "carry_bonus"};
    return all;
}

std::string stack_text(const ItemStack& s) {
    std::string out = s.item + "*" + num(s.amount);
    if (s.x >= 0 || s.y >= 0) out += "@" + std::to_string(s.x) + "," + std::to_string(s.y);
    if (s.rotated) out += "^";
    if (s.durability != 1.0f) out += "~" + num(s.durability);
    if (s.loaded != 0.0f) out += "#" + num(s.loaded);
    if (!s.variant.empty()) out += "!" + s.variant;
    if (!s.ammo.empty()) out += "&" + s.ammo;
    if (!s.mag.empty()) out += "%" + s.mag;
    if (!s.enchant.empty()) out += "$" + s.enchant;
    if (!s.action.empty()) out += "<" + s.action;
    if (!s.fire_mode.empty()) out += ">" + s.fire_mode;
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
        } else if (c == '!' || c == '&' || c == '%' || c == '$' || c == '<' || c == '>') {
            std::string word;
            while (i < text.size() && std::string("@^~#{}|!&%$<>").find(text[i]) == std::string::npos) word += text[i++];
            (c == '!' ? s.variant : c == '&' ? s.ammo : c == '%' ? s.mag : c == '$' ? s.enchant : c == '<' ? s.action : s.fire_mode) = word;
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

bool can_merge(const ItemStack& a, const ItemStack& b) {
    return a.item == b.item && a.variant == b.variant && a.enchant == b.enchant && a.ammo == b.ammo && a.mag == b.mag && a.contents.empty() && b.contents.empty() && a.loaded == b.loaded && a.durability == b.durability && a.action == b.action && a.fire_mode == b.fire_mode;
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
        {"fire_mode_keys", 3, nullptr, nullptr, nullptr, &Inventory::fire_mode_keys},
        {"drop_keys", 3, nullptr, nullptr, nullptr, &Inventory::drop_keys},
        {"holster_keys", 3, nullptr, nullptr, nullptr, &Inventory::holster_keys},
        {"ballistics", 3, nullptr, nullptr, nullptr, &Inventory::ballistics},
        {"stance_accuracy", 2, nullptr, nullptr, &Inventory::stance_accuracy, nullptr},
        {"show_weight", 2, nullptr, nullptr, &Inventory::show_weight, nullptr},
        {"worn_weight", 2, nullptr, nullptr, &Inventory::worn_weight, nullptr},
        {"reload_mode", 3, nullptr, nullptr, nullptr, &Inventory::reload_mode},
        {"actions", 3, nullptr, nullptr, nullptr, &Inventory::actions},
        {"place_amount", 0, &Inventory::place_amount, nullptr, nullptr, nullptr},
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
    if (key.rfind("action:", 0) == 0) return set_inventory_action(inv, key.substr(7), value);
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
    if (key.rfind("action:", 0) == 0) return inventory_action(inv, key.substr(7));
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
    float w = 0.0f;
    if (d && d->kind == ItemKind::ammo && (d->cartridge_grain > 0.0f || d->grain > 0.0f)) w = round_weight(cat, *d, s.variant) * s.amount;
    else if (d) w = d->weight * s.amount;
    if (s.loaded > 0.0f && !s.ammo.empty())
        if (const ItemDef* a = cat.find(s.ammo); a && a->kind == ItemKind::ammo) w += round_weight(cat, *a, s.variant) * s.loaded;
    if (!s.mag.empty())
        if (const ItemDef* m = cat.find(s.mag)) w += m->weight;
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
    for (const auto& [slot, s] : inv.worn) {
        if (inv.worn_weight || slot == "mainhand" || slot == "offhand") {
            w += stack_weight(cat, s);
        } else {
            for (const ItemStack& c : s.contents) w += stack_weight(cat, c);
        }
    }
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

float merge_into(std::vector<ItemStack>& list, const ItemStack& proto, float amount, float cap) {
    float added = 0.0f;
    for (ItemStack& s : list) {
        if (amount - added <= 0.0f) break;
        if (s.empty() || !can_merge(s, proto)) continue;
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
    ItemStack proto;
    proto.item = item;
    return container_add_stack(cat, container, cdef, proto, amount);
}

float container_add_stack(const ItemCatalog& cat, ItemStack& container, const ItemDef& cdef, const ItemStack& base, float amount) {
    const std::string& item = base.item;
    ItemStack proto = base;
    proto.amount = 0.0f;
    proto.x = proto.y = -1;
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
    float added = merge_into(container.contents, proto, amount, cap);
    const int cols = std::max(1, cdef.slots_w), rows = std::max(1, cdef.slots_h);
    while (amount - added > 1e-5f) {
        const float put = std::min(cap, amount - added);
        if (put <= 0.0f) break;
        ItemStack s = proto;
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
    ItemStack proto;
    proto.item = item;
    return inventory_add_stack(cat, inv, proto, amount, hotbar_first);
}

float inventory_add_stack(const ItemCatalog& cat, Inventory& inv, const ItemStack& base, float amount, bool hotbar_first) {
    const std::string& item = base.item;
    ItemStack proto = base;
    proto.amount = 0.0f;
    proto.x = proto.y = -1;
    proto.rotated = false;
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
    if (bar && hotbar_first) added += merge_into(inv.hotbar_items, proto, amount - added, cap);
    added += merge_into(inv.items, proto, amount - added, cap);
    if (bar && !hotbar_first) added += merge_into(inv.hotbar_items, proto, amount - added, cap);
    auto fill_slots = [&](std::vector<ItemStack>& list) {
        for (ItemStack& s : list) {
            if (amount - added <= 1e-5f) break;
            if (!s.empty()) continue;
            s = proto;
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
            ItemStack s = proto;
            s.amount = std::min(cap, amount - added);
            s.x = x;
            s.y = y;
            s.rotated = rot;
            inv.items.push_back(s);
            added += s.amount;
        }
    } else {
        while (amount - added > 1e-5f) {
            ItemStack s = proto;
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
            added += container_add_stack(cat, it->second, *cdef, proto, amount - added);
            if (amount - added <= 1e-5f) break;
        }
    }
    return added;
}

float inventory_take(const ItemCatalog& cat, Inventory& inv, const std::string& item, float amount) {
    return inventory_take_variant(cat, inv, item, "*", amount);
}

float inventory_take_variant(const ItemCatalog& cat, Inventory& inv, const std::string& item, const std::string& variant, float amount) {
    static_cast<void>(cat);
    float taken = 0.0f;
    const bool any = variant == "*";
    auto take_from = [&](std::vector<ItemStack>& list, bool keep_slots) {
        for (ItemStack& s : list) {
            if (amount - taken <= 1e-6f) break;
            if (s.item != item || s.empty() || (!any && s.variant != variant)) continue;
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
        if (s.item == item && (any || s.variant == variant) && amount - taken > 1e-6f) {
            const float t = std::min(s.amount, amount - taken);
            s.amount -= t;
            taken += t;
            if (s.amount <= 1e-6f) s.clear();
        }
    return taken;
}

float inventory_count(const Inventory& inv, const std::string& item) {
    return inventory_count_variant(inv, item, "*");
}

float inventory_count_variant(const Inventory& inv, const std::string& item, const std::string& variant) {
    float n = 0.0f;
    auto count = [&](const ItemStack& s, auto& self) -> void {
        if (s.item == item && !s.empty() && (variant == "*" || s.variant == variant)) n += s.amount;
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


const std::vector<BulletType>& bullet_types() {
    static const std::vector<BulletType> all = [] {
        std::vector<BulletType> v;
        auto add = [&](const char* id, const char* name, const char* classes, const char* desc) -> BulletType& {
            BulletType b;
            b.id = id;
            b.name = name;
            b.classes = classes;
            b.description = desc;
            v.push_back(b);
            return v.back();
        };
        const char* bullets = "rimfire,handgun,rifle,blackpowder,specialty";
        BulletType& fmj = add("fmj", "FMJ (full metal jacket)", bullets, "Lead core in a copper jacket. The standard round: goes deep, doesn't expand, ricochets easily.");
        fmj.tip = "round";
        fmj.tip_color = {0.78f, 0.48f, 0.25f};
        BulletType& ap = add("ap", "AP (armor piercing)", "handgun,rifle,specialty", "Hardened steel or tungsten core. Punches through armor and cover, but does less harm to soft targets.");
        ap.damage = 0.9f;
        ap.penetration = 2.2f;
        ap.armor = 0.35f;
        ap.grain = 0.92f;
        ap.ricochet = 0.6f;
        ap.tip = "pointed";
        ap.tip_color = {0.08f, 0.08f, 0.08f};
        BulletType& hp = add("hp", "HP (hollow point)", "rimfire,handgun,rifle", "A hollow in the nose makes it mushroom on impact: more damage and bleeding, but armor stops it easily and it barely penetrates.");
        hp.damage = 1.3f;
        hp.penetration = 0.35f;
        hp.armor = 1.7f;
        hp.ricochet = 0.4f;
        hp.tip = "hollow";
        hp.tip_color = {0.8f, 0.5f, 0.28f};
        hp.effects = "bleeding:6:0.4";
        BulletType& sp = add("sp", "SP (soft point)", "handgun,rifle,blackpowder", "Exposed lead tip that expands more slowly than a hollow point. The usual hunting bullet.");
        sp.damage = 1.18f;
        sp.penetration = 0.6f;
        sp.armor = 1.4f;
        sp.ricochet = 0.5f;
        sp.tip = "soft";
        sp.tip_color = {0.55f, 0.55f, 0.58f};
        sp.effects = "bleeding:5:0.25";
        BulletType& bt = add("bt", "BT (boat tail)", "rifle", "Tapered base cuts drag, so it keeps its speed and drops less at long range.");
        bt.drag = 0.7f;
        bt.speed = 1.02f;
        bt.tip = "boat";
        bt.tip_color = {0.8f, 0.52f, 0.28f};
        BulletType& otm = add("otm", "OTM (open tip match)", "rifle,handgun", "Match-grade bullet with a small opening at the tip. Very accurate and low drag; expands a little.");
        otm.drag = 0.75f;
        otm.spread = 0.55f;
        otm.damage = 1.08f;
        otm.penetration = 0.9f;
        otm.armor = 1.1f;
        otm.tip = "open";
        otm.tip_color = {0.7f, 0.45f, 0.25f};
        BulletType& wc = add("wadcutter", "Wadcutter", "handgun,airgun,rimfire", "Flat-nosed target bullet that punches clean holes. Slow, high drag and short ranged, poor against armor.");
        wc.drag = 1.8f;
        wc.speed = 0.85f;
        wc.damage = 0.95f;
        wc.penetration = 0.5f;
        wc.armor = 1.5f;
        wc.ricochet = 0.3f;
        wc.tip = "flat";
        wc.tip_color = {0.5f, 0.5f, 0.52f};
        BulletType& fr = add("frangible", "Frangible", "handgun,rifle", "Compressed powdered metal that turns to dust on hard surfaces: no ricochets, no going through walls, weak against armor.");
        fr.damage = 1.1f;
        fr.penetration = 0.15f;
        fr.armor = 2.0f;
        fr.ricochet = 0.0f;
        fr.grain = 0.8f;
        fr.breaks_up = true;
        fr.tip = "round";
        fr.tip_color = {0.45f, 0.47f, 0.5f};
        BulletType& tr = add("tracer", "Tracer", "rifle,specialty", "Burning compound in the base leaves a bright line so you can see where the shots go.");
        tr.tracer = true;
        tr.tip = "pointed";
        tr.tip_color = {0.85f, 0.15f, 0.12f};
        BulletType& inc = add("incendiary", "Incendiary", "rifle,specialty,shotgun", "Sets what it hits on fire.");
        inc.extra_damage = "fire=6";
        inc.effects = "burning:4:0.6";
        inc.tip = "pointed";
        inc.tip_color = {0.15f, 0.4f, 0.75f};
        BulletType& api = add("api", "API (armor piercing incendiary)", "rifle,specialty", "Hard core and an incendiary charge: goes through armor and starts fires.");
        api.damage = 0.95f;
        api.penetration = 2.0f;
        api.armor = 0.45f;
        api.extra_damage = "fire=5";
        api.effects = "burning:3:0.4";
        api.tip = "pointed";
        api.tip_color = {0.55f, 0.6f, 0.8f};
        BulletType& buck = add("buckshot", "Buckshot", "shotgun", "A handful of big lead balls. Devastating up close.");
        buck.pellets = 9;
        buck.tip = "shell";
        buck.tip_color = {0.75f, 0.1f, 0.1f};
        BulletType& bird = add("birdshot", "Birdshot", "shotgun", "Hundreds of tiny pellets: wide spread, weak past a few metres.");
        bird.pellets = 24;
        bird.damage = 0.35f;
        bird.spread = 1.6f;
        bird.penetration = 0.2f;
        bird.armor = 2.5f;
        bird.tip = "shell";
        bird.tip_color = {0.15f, 0.55f, 0.2f};
        BulletType& slug = add("slug", "Slug", "shotgun", "One heavy solid projectile. Accurate out to 100 m and hits very hard.");
        slug.pellets = 1;
        slug.damage = 6.0f;
        slug.spread = 0.25f;
        slug.penetration = 1.2f;
        slug.tip = "shell";
        slug.tip_color = {0.1f, 0.25f, 0.7f};
        slug.stagger = 2.0f;
        BulletType& flech = add("flechette", "Flechette", "shotgun", "Small steel darts that fly flat and pierce.");
        flech.pellets = 12;
        flech.damage = 0.6f;
        flech.penetration = 1.8f;
        flech.armor = 0.6f;
        flech.drag = 0.6f;
        flech.tip = "shell";
        flech.tip_color = {0.6f, 0.62f, 0.66f};
        BulletType& db = add("dragons_breath", "Dragon's breath", "shotgun", "Magnesium pellets that burst into a cone of flame.");
        db.pellets = 10;
        db.damage = 0.3f;
        db.damage_type = "fire";
        db.effects = "burning:5:0.8";
        db.tip = "shell";
        db.tip_color = {0.95f, 0.55f, 0.1f};
        BulletType& bean = add("beanbag", "Beanbag (less lethal)", "shotgun", "A fabric bag of shot. Knocks people down rather than killing them.");
        bean.pellets = 1;
        bean.damage = 0.8f;
        bean.damage_type = "blunt";
        bean.penetration = 0.0f;
        bean.stagger = 4.0f;
        bean.effects = "stunned:1.5:0.5";
        bean.tip = "shell";
        bean.tip_color = {0.85f, 0.8f, 0.2f};
        BulletType& rb = add("rubber", "Rubber", "handgun,shotgun,specialty", "Less-lethal rubber bullet or baton round.");
        rb.damage = 0.25f;
        rb.damage_type = "blunt";
        rb.penetration = 0.0f;
        rb.stagger = 3.0f;
        rb.ricochet = 2.0f;
        rb.tip = "round";
        rb.tip_color = {0.15f, 0.15f, 0.15f};
        BulletType& dome = add("domed", "Domed pellet", "airgun", "The all-round airgun pellet: accurate and steady in wind.");
        dome.tip = "round";
        dome.tip_color = {0.55f, 0.55f, 0.58f};
        BulletType& point = add("pointed", "Pointed pellet", "airgun", "Pointed airgun pellet that penetrates a little more.");
        point.penetration = 1.3f;
        point.tip = "pointed";
        point.tip_color = {0.55f, 0.55f, 0.58f};
        BulletType& ahp = add("hollow_pellet", "Hollow point pellet", "airgun", "Airgun pellet that opens up on impact.");
        ahp.damage = 1.2f;
        ahp.penetration = 0.6f;
        ahp.tip = "hollow";
        ahp.tip_color = {0.55f, 0.55f, 0.58f};
        BulletType& shock = add("shock", "Shock (magic)", "rimfire,handgun,rifle,shotgun,specialty,airgun,blackpowder", "Charged with lightning: arcs into what it hits.");
        shock.extra_damage = "shock=8";
        shock.effects = "shocked:1.5:0.6";
        shock.tip_color = {0.5f, 0.75f, 1.0f};
        shock.tip = "pointed";
        BulletType& acid = add("acid", "Acid (magic)", "rimfire,handgun,rifle,shotgun,specialty,airgun,blackpowder", "Hollow tip filled with acid that keeps eating after the hit.");
        acid.extra_damage = "acid=4";
        acid.effects = "poison:6:0.7";
        acid.tip = "hollow";
        acid.tip_color = {0.4f, 0.9f, 0.2f};
        BulletType& frost = add("frost", "Frost (magic)", "rimfire,handgun,rifle,shotgun,specialty,airgun,blackpowder", "Enchanted to freeze: slows and chills.");
        frost.extra_damage = "cold=6";
        frost.effects = "chilled:5:0.8, frozen:1.5:0.15";
        frost.tip = "pointed";
        frost.tip_color = {0.6f, 0.9f, 1.0f};
        BulletType& holy = add("blessed", "Blessed silver (magic)", "handgun,rifle,shotgun,blackpowder", "Silver bullet with a blessing: deals extra magic damage.");
        holy.extra_damage = "magic=10";
        holy.tip = "round";
        holy.tip_color = {0.85f, 0.87f, 0.9f};
        return v;
    }();
    return all;
}

const BulletType* ItemCatalog::bullet(const std::string& id) const {
    if (const auto it = bullets.find(id); it != bullets.end()) return &it->second;
    for (const BulletType& b : bullet_types())
        if (b.id == id) return &b;
    return nullptr;
}

BulletType& ItemCatalog::define_bullet(const std::string& id) {
    auto it = bullets.find(id);
    if (it != bullets.end()) return it->second;
    BulletType b;
    for (const BulletType& base : bullet_types())
        if (base.id == id) b = base;
    b.id = id;
    if (b.name.empty()) b.name = id;
    return bullets.emplace(id, b).first->second;
}

const std::vector<std::string>& bullet_field_keys() {
    static const std::vector<std::string> all = {"name", "description", "damage", "penetration", "armor", "drag", "spread", "grain", "ricochet", "speed", "stagger", "pellets", "breaks_up", "tracer", "tip", "tip_color", "effects", "extra_damage", "damage_type", "classes"};
    return all;
}

bool set_bullet_field(BulletType& b, const std::string& key, const std::string& value) {
    const float x = static_cast<float>(std::atof(value.c_str()));
    const bool on = value != "0" && value != "false" && !value.empty();
    if (key == "name") b.name = value;
    else if (key == "description") b.description = value;
    else if (key == "damage") b.damage = std::max(0.0f, x);
    else if (key == "penetration") b.penetration = std::max(0.0f, x);
    else if (key == "armor") b.armor = std::max(0.0f, x);
    else if (key == "drag") b.drag = std::max(0.0f, x);
    else if (key == "spread") b.spread = std::max(0.0f, x);
    else if (key == "grain") b.grain = std::max(0.01f, x);
    else if (key == "ricochet") b.ricochet = std::max(0.0f, x);
    else if (key == "speed") b.speed = std::max(0.01f, x);
    else if (key == "stagger") b.stagger = std::max(0.0f, x);
    else if (key == "pellets") b.pellets = std::max(0, std::atoi(value.c_str()));
    else if (key == "breaks_up") b.breaks_up = on;
    else if (key == "tracer") b.tracer = on;
    else if (key == "tip") b.tip = value;
    else if (key == "tip_color") {
        float r = 0.0f, g = 0.0f, bl = 0.0f;
        if (std::sscanf(value.c_str(), "%f,%f,%f", &r, &g, &bl) != 3) return false;
        b.tip_color = {r, g, bl};
    } else if (key == "effects") b.effects = value;
    else if (key == "extra_damage") b.extra_damage = value;
    else if (key == "damage_type") b.damage_type = value;
    else if (key == "classes") b.classes = value;
    else return false;
    return true;
}

std::string bullet_field(const BulletType& b, const std::string& key) {
    if (key == "id") return b.id;
    if (key == "name") return b.name;
    if (key == "description") return b.description;
    if (key == "damage") return num(b.damage);
    if (key == "penetration") return num(b.penetration);
    if (key == "armor") return num(b.armor);
    if (key == "drag") return num(b.drag);
    if (key == "spread") return num(b.spread);
    if (key == "grain") return num(b.grain);
    if (key == "ricochet") return num(b.ricochet);
    if (key == "speed") return num(b.speed);
    if (key == "stagger") return num(b.stagger);
    if (key == "pellets") return std::to_string(b.pellets);
    if (key == "breaks_up") return b.breaks_up ? "1" : "0";
    if (key == "tracer") return b.tracer ? "1" : "0";
    if (key == "tip") return b.tip;
    if (key == "tip_color") return num(b.tip_color.x) + "," + num(b.tip_color.y) + "," + num(b.tip_color.z);
    if (key == "effects") return b.effects;
    if (key == "extra_damage") return b.extra_damage;
    if (key == "damage_type") return b.damage_type;
    if (key == "classes") return b.classes;
    return std::string();
}

std::vector<EffectRoll> parse_effect_rolls(const std::string& text) {
    std::vector<EffectRoll> out;
    for (const std::string& part : split_list(text)) {
        EffectRoll r;
        std::vector<std::string> bits;
        std::string cur;
        for (char c : part + ":") {
            if (c == ':') {
                bits.push_back(cur);
                cur.clear();
            } else if (c != ' ') {
                cur += c;
            }
        }
        if (bits.empty() || bits[0].empty()) continue;
        r.effect = bits[0];
        if (bits.size() > 1 && !bits[1].empty()) r.duration = static_cast<float>(std::atof(bits[1].c_str()));
        if (bits.size() > 2 && !bits[2].empty()) r.chance = std::clamp(static_cast<float>(std::atof(bits[2].c_str())), 0.0f, 1.0f);
        if (bits.size() > 3 && !bits[3].empty()) r.strength = static_cast<float>(std::atof(bits[3].c_str()));
        out.push_back(r);
    }
    return out;
}

std::map<std::string, float> parse_amounts(const std::string& text) {
    std::map<std::string, float> out;
    for (const std::string& part : split_list(text)) {
        const std::size_t eq = part.find('=');
        if (eq == std::string::npos) continue;
        std::string k = part.substr(0, eq);
        k.erase(std::remove(k.begin(), k.end(), ' '), k.end());
        if (!k.empty()) out[k] += static_cast<float>(std::atof(part.substr(eq + 1).c_str()));
    }
    return out;
}

std::string amounts_text(const std::map<std::string, float>& amounts) {
    std::string out;
    for (const auto& [k, v] : amounts) out += (out.empty() ? "" : ", ") + k + "=" + num(v);
    return out;
}

namespace {

bool listed(const std::string& list, const std::string& want) {
    if (want.empty()) return false;
    for (const std::string& x : split_list(list))
        if (x == want) return true;
    return false;
}

} // namespace

bool ammo_fits(const ItemDef& gun, const ItemDef& ammo) {
    if (ammo.kind != ItemKind::ammo) return false;
    if (!gun.ammo.empty() && gun.ammo == ammo.id) return true;
    const std::string& want = gun.caliber.empty() ? gun.ammo : gun.caliber;
    const std::string& have = ammo.caliber.empty() ? ammo.id : ammo.caliber;
    if (!want.empty() && want == have) return true;
    return listed(gun.also_fires, have) || listed(gun.also_fires, ammo.id);
}

bool magazine_fits(const ItemDef& gun, const ItemDef& magazine) {
    if (magazine.kind != ItemKind::magazine) return false;
    if (listed(gun.magazines, magazine.id) || listed(magazine.fits, gun.id)) return true;
    return gun.magazines.empty() && magazine.fits.empty() && !gun.caliber.empty() && gun.caliber == magazine.caliber && magazine.magazine == gun.magazine;
}

bool magazine_takes(const ItemDef& magazine, const ItemDef& ammo) {
    if (ammo.kind != ItemKind::ammo) return false;
    const std::string& have = ammo.caliber.empty() ? ammo.id : ammo.caliber;
    return have == magazine.caliber || ammo.id == magazine.ammo || listed(magazine.also_fires, have);
}

std::vector<std::string> ammo_variants(const ItemCatalog& cat, const ItemDef& ammo) {
    std::vector<std::string> out;
    if (!ammo.variants.empty()) {
        for (const std::string& v : split_list(ammo.variants))
            if (cat.bullet(v)) out.push_back(v);
        return out;
    }
    if (ammo.cartridge_class.empty()) return out;
    for (const BulletType& b : bullet_types())
        if (listed(b.classes, ammo.cartridge_class)) out.push_back(b.id);
    for (const auto& [id, b] : cat.bullets)
        if (listed(b.classes, ammo.cartridge_class) && std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
    return out;
}

std::string default_variant(const ItemCatalog& cat, const ItemDef& ammo) {
    if (!ammo.default_variant.empty()) return ammo.default_variant;
    const std::vector<std::string> v = ammo_variants(cat, ammo);
    return v.empty() ? std::string() : v.front();
}

float bullet_grain(const ItemCatalog& cat, const ItemDef& ammo, const std::string& variant) {
    const BulletType* b = variant.empty() ? nullptr : cat.bullet(variant);
    const float g = ammo.grain > 0.0f ? ammo.grain : ammo.projectile_mass * grains_per_kg;
    return g * (b ? b->grain : 1.0f);
}

float round_weight(const ItemCatalog& cat, const ItemDef& ammo, const std::string& variant) {
    if (ammo.cartridge_grain <= 0.0f && ammo.grain <= 0.0f) return ammo.weight;
    const float bullet = bullet_grain(cat, ammo, variant);
    const float base_bullet = ammo.grain > 0.0f ? ammo.grain : bullet;
    const float whole = ammo.cartridge_grain > 0.0f ? ammo.cartridge_grain - base_bullet + bullet : bullet;
    return std::max(0.0f, whole) / grains_per_kg;
}

std::vector<const ItemDef*> guns_for_ammo(const ItemCatalog& cat, const ItemDef& ammo) {
    std::vector<const ItemDef*> out;
    std::set<std::string> seen;
    auto look = [&](const ItemDef& d) {
        if ((d.kind == ItemKind::gun || d.kind == ItemKind::bow) && ammo_fits(d, ammo) && seen.insert(d.id).second) out.push_back(cat.find(d.id));
    };
    for (const auto& [id, d] : cat.custom) look(d);
    for (const ItemDef& d : item_library()) look(d);
    return out;
}

std::vector<const ItemDef*> ammo_for_gun(const ItemCatalog& cat, const ItemDef& gun) {
    std::vector<const ItemDef*> out;
    std::set<std::string> seen;
    auto look = [&](const ItemDef& d) {
        if (d.kind == ItemKind::ammo && ammo_fits(gun, d) && seen.insert(d.id).second) out.push_back(cat.find(d.id));
    };
    for (const auto& [id, d] : cat.custom) look(d);
    for (const ItemDef& d : item_library()) look(d);
    return out;
}

std::vector<const ItemDef*> magazines_for_gun(const ItemCatalog& cat, const ItemDef& gun) {
    std::vector<const ItemDef*> out;
    std::set<std::string> seen;
    auto look = [&](const ItemDef& d) {
        if (magazine_fits(gun, d) && seen.insert(d.id).second) out.push_back(cat.find(d.id));
    };
    for (const auto& [id, d] : cat.custom) look(d);
    for (const ItemDef& d : item_library()) look(d);
    return out;
}

std::string ammo_info(const ItemCatalog& cat, const std::string& id) {
    const ItemDef* a = cat.find(id);
    if (!a) return std::string();
    std::string out = a->name + "\n";
    char buf[256];
    if (a->kind == ItemKind::ammo) {
        if (!a->cartridge_class.empty()) out += "Type: " + a->cartridge_class + "\n";
        if (!a->aliases.empty()) out += "Also called: " + a->aliases + "\n";
        const float g = bullet_grain(cat, *a, std::string());
        if (g > 0.0f) {
            std::snprintf(buf, sizeof(buf), "Bullet: %.0f gr   Round: %s\n", static_cast<double>(g), weight_text(round_weight(cat, *a, std::string())).c_str());
            out += buf;
        }
        if (a->speed > 0.0f) {
            std::snprintf(buf, sizeof(buf), "Muzzle speed: about %.0f m/s\n", static_cast<double>(a->speed));
            out += buf;
        }
        std::string guns;
        for (const ItemDef* gd : guns_for_ammo(cat, *a)) guns += (guns.empty() ? "" : ", ") + gd->name;
        out += "Fits: " + (guns.empty() ? std::string("no gun in the library yet (make one with define_item and caliber ") + (a->caliber.empty() ? a->id : a->caliber) + ")" : guns) + "\n";
        std::string mags;
        for (const ItemDef& m : item_library())
            if (m.kind == ItemKind::magazine && magazine_takes(m, *a)) mags += (mags.empty() ? "" : ", ") + m.name;
        if (!mags.empty()) out += "Magazines: " + mags + "\n";
        out += "Bullet types:\n";
        for (const std::string& v : ammo_variants(cat, *a))
            if (const BulletType* b = cat.bullet(v)) {
                std::snprintf(buf, sizeof(buf), "  %s: damage x%.2f, penetration x%.2f, vs armor x%.2f, drag x%.2f, %.0f gr", b->name.c_str(), static_cast<double>(b->damage), static_cast<double>(b->penetration), static_cast<double>(1.0f / std::max(0.05f, b->armor)), static_cast<double>(b->drag), static_cast<double>(bullet_grain(cat, *a, v)));
                out += buf;
                if (!b->extra_damage.empty()) out += ", +" + b->extra_damage;
                if (!b->effects.empty()) out += ", effects " + b->effects;
                out += "\n";
            }
        if (!a->effects.empty()) out += "Effects: " + a->effects + "\n";
        if (!a->extra_damage.empty()) out += "Extra damage: " + a->extra_damage + "\n";
    } else if (a->kind == ItemKind::gun || a->kind == ItemKind::bow) {
        std::string ammo;
        for (const ItemDef* d : ammo_for_gun(cat, *a)) ammo += (ammo.empty() ? "" : ", ") + d->name;
        out += "Fires: " + (ammo.empty() ? std::string("nothing in the library") : ammo) + "\n";
        std::string mags;
        for (const ItemDef* d : magazines_for_gun(cat, *a)) mags += (mags.empty() ? "" : ", ") + d->name;
        if (!mags.empty()) out += "Magazines: " + mags + "\n";
        out += "Feed: " + (a->feed.empty() ? std::string("magazine") : a->feed) + "\n";
        if (!a->effects.empty()) out += "Effects: " + a->effects + "\n";
        if (!a->extra_damage.empty()) out += "Extra damage: " + a->extra_damage + "\n";
    } else if (a->kind == ItemKind::magazine) {
        std::snprintf(buf, sizeof(buf), "Holds %d rounds\n", a->magazine);
        out += buf;
        std::string guns;
        for (const ItemDef& d : item_library())
            if (d.kind == ItemKind::gun && magazine_fits(d, *a)) guns += (guns.empty() ? "" : ", ") + d.name;
        out += "Fits: " + guns + "\n";
    }
    return out;
}

std::string item_display_name(const ItemCatalog& cat, const ItemStack& s) {
    const ItemDef* d = cat.find(s.item);
    std::string name = d ? d->name : s.item;
    if (!s.variant.empty() && d && d->kind == ItemKind::ammo)
        if (const BulletType* b = cat.bullet(s.variant)) {
            std::string shortname = b->name.substr(0, b->name.find(" ("));
            name += " " + shortname;
        }
    if (!s.enchant.empty()) name += " (enchanted)";
    return name;
}

const std::vector<GameRule>& game_rules() {
    static const std::vector<GameRule> all = {
        {"reload", "keep", "select", "When you reload", "keep=Keep the old magazine (with what's left in it)|drop=Drop the old magazine on the ground|discard=Throw the old magazine away (rounds and all)|pool=No magazines: rounds come from a shared ammo pool|top_up=No magazines: top up round by round from the pool", "Tarkov keeps partly used magazines; most shooters use a shared pool. Guns that load one round at a time (revolvers, bolt actions, pump shotguns) always load from the pool.", "Ammo and magazines"},
        {"magazines", "1", "bool", "Guns use magazine items", "", "Off: guns with magazines reload straight from loose rounds.", "Ammo and magazines"},
        {"auto_fill", "1", "bool", "Reloading fills an empty magazine from loose rounds if there's no loaded one", "", "", "Ammo and magazines"},
        {"chamber", "1", "bool", "Reloading a gun that still has a round keeps one in the chamber (+1)", "", "", "Ammo and magazines"},
        {"auto_reload", "1", "bool", "Pulling the trigger on an empty gun starts a reload", "", "", "Ammo and magazines"},
        {"load_time", "0.25", "float", "Seconds to push one round into a magazine", "", "", "Ammo and magazines"},
        {"world_fill", "full", "select", "Guns and magazines in the world start", "full=Full|empty=Empty|partial=Partly full|random=A random amount|as_placed=As placed in the editor", "Each pickup can choose its own in the inspector.", "Ammo and magazines"},
        {"fill_fraction", "0.5", "float", "Partly full means (0 to 1)", "", "", "Ammo and magazines"},
        {"default_variant", "fmj", "string", "Bullet type when none is given", "", "Used when a round's own list doesn't name one.", "Ammo and magazines"},
        {"look_info", "1", "bool", "Show details about what you look at", "", "Like Jade or WAILA in Minecraft.", "Look-at panel"},
        {"look_range", "8", "float", "How far away it works (m)", "", "", "Look-at panel"},
        {"look_anchor", "top", "select", "Where it shows", "top=Top|top-left=Top left|top-right=Top right|center=Under the crosshair|bottom=Bottom", "", "Look-at panel"},
        {"look_lines", "{name}\n{health_bar}\n{health} / {max_health} health\n{item}\n{effects}\n{source}", "text", "What it shows (one line each)", "", "Names in braces are filled in: {name} {source} {category} {health} {max_health} {health_bar} {shield} {shield_bar} {armor} {effects} {item} {amount} {loaded} {ammo} {distance} {stamina} {hunger} {thirst} {tags} {info} {stat:name} {inventory}. Lines whose names are all empty are skipped. Scripts add lines with set_look_info.", "Look-at panel"},
        {"look_items", "1", "bool", "Also for items on the ground", "", "", "Look-at panel"},
        {"look_everything", "0", "bool", "Also for things without health or items (walls, props)", "", "", "Look-at panel"},
        {"plates", "1", "bool", "Name plates above characters", "", "", "Name plates"},
        {"plate_lines", "{name}\n{health_bar}", "text", "What plates show", "", "Same names as the look-at panel.", "Name plates"},
        {"plate_range", "25", "float", "Shown within (m)", "", "", "Name plates"},
        {"plate_when", "always", "select", "Shown", "always=Always|hurt=Only once hurt|looked=Only when looked at|hurt_or_looked=When hurt or looked at", "", "Name plates"},
        {"plate_height", "0.35", "float", "Height above the head (m)", "", "", "Name plates"},
        {"plate_players", "0", "bool", "Plates on the player too", "", "", "Name plates"},
        {"plate_items", "0", "bool", "Plates on items on the ground", "", "", "Name plates"},
        {"worn_weight", "1", "bool", "Worn clothes and armor count towards carry weight", "", "Each inventory can turn it off too. Off: only what's carried in the inventory and bags matters.", "Weight"},
        {"held_weight", "1", "bool", "What's in the hands counts towards carry weight", "", "", "Weight"},
        {"sounds", "1", "bool", "Weapons and items make sounds", "", "Made for you in Assets/Sounds/Items the first time they're needed; replace them with your own.", "Animation and sound"},
        {"animations", "1", "bool", "Held items animate (recoil, reloading, aiming, swinging)", "", "", "Animation and sound"},
        {"gun_loudness", "60", "float", "How far AI hears gunshots (m)", "", "", "Animation and sound"},
        {"manual_cycle", "0", "bool", "Work actions and hammers by hand", "", "Off: bolt, pump and lever actions are worked and hammers are cocked by themselves after each shot. On: pull the trigger again or press R to do it. Each gun can choose for itself with its cycle setting.", "Gun handling"},
        {"revolver_hammer", "0", "bool", "Cock double-action revolvers before each shot", "", "Slower, but a cocked hammer is more accurate. Single-action revolvers, muzzle-loaders and other hammer guns always need cocking.", "Gun handling"},
        {"burst5", "0", "bool", "Automatic guns also get a 5-round burst mode", "", "", "Gun handling"},
        {"cycle_speed", "100", "float", "How fast actions are worked (percent)", "", "", "Gun handling"},
        {"dry_click", "realistic", "select", "Pulling the trigger on an empty gun", "realistic=Clicks, unless the slide or bolt is locked open|always=Always clicks|never=Never clicks", "", "Gun handling"},
        {"casings", "1", "bool", "Guns throw out spent casings (with sounds for what they land on)", "", "", "Gun handling"},
        {"jams", "0", "bool", "Guns can jam", "", "Worn guns jam more. Press R or pull the trigger to clear a jam (you lose the stuck round).", "Gun handling"},
        {"jam_rate", "1", "float", "How often guns jam (1 = normal)", "", "", "Gun handling"},
        {"surface_movement", "1", "bool", "Ground and plants change how fast you move", "", "Snow, mud, sand, water, tall grass and bushes slow you down; ice is slippery.", "Ground and surfaces"},
        {"surface_strength", "100", "float", "How strong that is (percent: 100 = as set, 0 = none, 200 = twice as much)", "", "", "Ground and surfaces"},
        {"ground_snow", "", "string", "Walking in snow (empty: 80%)", "", "A percentage (80%), a multiplier (0.8), metres a second taken off (-1.2), or empty for the surface's own setting. Scripts can also set any character's speed with set_ground_speed.", "Ground and surfaces"},
        {"ground_deep_snow", "", "string", "Deep snow (empty: 55%)", "", "", "Ground and surfaces"},
        {"ground_mud", "", "string", "Mud (empty: 70%)", "", "", "Ground and surfaces"},
        {"ground_sand", "", "string", "Sand (empty: 85%)", "", "", "Ground and surfaces"},
        {"ground_gravel", "", "string", "Gravel (empty: 95%)", "", "", "Ground and surfaces"},
        {"ground_water_shallow", "", "string", "Wading in shallow water (empty: 75%)", "", "", "Ground and surfaces"},
        {"ground_water", "", "string", "Wading in deep water (empty: 50%)", "", "", "Ground and surfaces"},
        {"ground_tall_grass", "", "string", "Tall grass (empty: 90%)", "", "", "Ground and surfaces"},
        {"ground_shrubs", "", "string", "Shrubs and bushes (empty: 75%)", "", "", "Ground and surfaces"},
        {"ground_crops", "", "string", "Crops (empty: 85%)", "", "", "Ground and surfaces"},
        {"surface_bullets", "1", "bool", "What bullets hit matters (ricochets off metal and stone, through wood and glass, stopped by sand and dirt, slowed by water)", "", "", "Ground and surfaces"},
        {"footsteps", "1", "bool", "Footstep sounds for what you walk on", "", "", "Ground and surfaces"},
        {"footstep_noise", "8", "float", "How far AI hears footsteps (m, for normal walking)", "", "Crouching is quieter; sprinting, gravel, metal and leaves are louder.", "Ground and surfaces"},
        {"weather_ground", "1", "bool", "Weather changes the ground (snowfall makes snow, heavy rain turns dirt to mud)", "", "", "Ground and surfaces"},
        {"blood", "1", "bool", "Hits on characters show a red puff", "", "", "Ground and surfaces"},
    };
    return all;
}

const GameRule* game_rule(const std::string& key) {
    for (const GameRule& r : game_rules())
        if (r.key == key) return &r;
    return nullptr;
}

const std::vector<std::string>& inventory_events() {
    static const std::vector<std::string> all = {"left_click", "left_click+cursor", "right_click", "right_click+cursor", "middle_click", "middle_click+cursor", "shift+left_click", "shift+right_click", "ctrl+left_click", "ctrl+right_click", "double_click", "hover", "hover+cursor", "key:G+cursor", "key:R+cursor", "key:Q", "key:Q+cursor"};
    return all;
}

const std::vector<std::pair<std::string, std::string>>& inventory_action_choices() {
    static const std::vector<std::pair<std::string, std::string>> all = {
        {"none", "Nothing"},
        {"pick_all", "Pick up the whole stack"},
        {"pick_half", "Pick up half"},
        {"pick_one", "Pick up one"},
        {"pick_amount", "Pick up the set amount"},
        {"place_all", "Put down everything held (or swap)"},
        {"place_one", "Put down one"},
        {"place_half", "Put down half"},
        {"place_amount", "Put down the set amount"},
        {"drop_cursor", "Drop what's held out of the inventory"},
        {"drop_one", "Drop one of the item"},
        {"drop_stack", "Drop the whole stack"},
        {"quick_move", "Send to the other side (chest or bags)"},
        {"use", "Use, eat, drink or read"},
        {"equip", "Wear, or take off"},
        {"hold", "Hold in the main hand"},
        {"load", "Load rounds into a magazine, or unload it"},
        {"rotate", "Rotate what's held"},
        {"split", "Split the stack in two"},
        {"inspect", "Show its details"},
        {"tooltip", "Show the tooltip"},
    };
    return all;
}

std::string inventory_action(const Inventory& inv, const std::string& event) {
    for (const std::string& part : split_list(inv.actions)) {
        const std::size_t eq = part.find('=');
        if (eq == std::string::npos) continue;
        std::string k = part.substr(0, eq), v = part.substr(eq + 1);
        k.erase(std::remove(k.begin(), k.end(), ' '), k.end());
        v.erase(std::remove(v.begin(), v.end(), ' '), v.end());
        if (k == event) return v;
    }
    return std::string();
}

bool set_inventory_action(Inventory& inv, const std::string& event, const std::string& action) {
    if (event.empty() || event.find_first_of(",=") != std::string::npos || action.find_first_of(",=") != std::string::npos) return false;
    std::string out;
    bool done = false;
    for (const std::string& part : split_list(inv.actions)) {
        const std::size_t eq = part.find('=');
        if (eq == std::string::npos) continue;
        std::string k = part.substr(0, eq);
        k.erase(std::remove(k.begin(), k.end(), ' '), k.end());
        if (k == event) {
            if (!action.empty()) out += (out.empty() ? "" : ", ") + event + "=" + action;
            done = true;
        } else {
            out += (out.empty() ? "" : ", ") + part;
        }
    }
    if (!done && !action.empty()) out += (out.empty() ? "" : ", ") + event + "=" + action;
    inv.actions = out;
    return true;
}

} // namespace orchard
