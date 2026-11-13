#include <orchard/items.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <set>

namespace orchard {

namespace {

using K = ItemKind;
using S = ItemSize;

const Vec3 steel{0.62f, 0.63f, 0.66f};
const Vec3 gunmetal{0.16f, 0.17f, 0.18f};
const Vec3 black{0.07f, 0.07f, 0.08f};
const Vec3 wood{0.45f, 0.29f, 0.16f};
const Vec3 dark_wood{0.28f, 0.17f, 0.09f};
const Vec3 leather{0.4f, 0.24f, 0.13f};
const Vec3 bronze{0.72f, 0.5f, 0.22f};
const Vec3 iron{0.4f, 0.4f, 0.42f};
const Vec3 olive{0.33f, 0.36f, 0.2f};
const Vec3 tan{0.71f, 0.6f, 0.42f};
const Vec3 cloth{0.5f, 0.45f, 0.38f};
const Vec3 red{0.7f, 0.1f, 0.08f};
const Vec3 gold{0.85f, 0.68f, 0.2f};
const Vec3 glass{0.75f, 0.85f, 0.9f};
const Vec3 polymer{0.12f, 0.12f, 0.12f};

struct Library {
    std::vector<ItemDef> v;

    ItemDef& add(const std::string& id, const std::string& name, const std::string& category, K kind, const std::string& shape) {
        ItemDef d;
        d.id = id;
        d.name = name;
        d.category = category;
        d.kind = kind;
        d.shape = shape;
        v.push_back(d);
        return v.back();
    }

    ItemDef& gun(const std::string& id, const std::string& name, const std::string& cat, const std::string& shape, float dmg, float rate, int mag, const std::string& ammo, float speed, float spread, float weight, float length, bool automatic, S size, int gw, int gh) {
        ItemDef& d = add(id, name, cat, K::gun, shape);
        d.damage = dmg;
        d.damage_type = "projectile";
        d.rate = rate;
        d.magazine = mag;
        d.ammo = ammo;
        d.speed = speed;
        d.spread = spread;
        d.weight = weight;
        d.length = length;
        d.automatic = automatic;
        d.size = size;
        d.grid_w = gw;
        d.grid_h = gh;
        d.range = speed * 0.6f;
        d.reload = size >= S::large ? 2.6f : size == S::medium ? 2.2f : 1.6f;
        d.projectile_mass = 0.008f;
        d.drag = 0.0012f;
        d.recoil = dmg * 0.04f;
        d.knockback = 0.0f;
        d.color = gunmetal;
        d.accent = black;
        d.volume = length * 0.6f;
        d.two_handed = size >= S::medium;
        return d;
    }

    ItemDef& melee(const std::string& id, const std::string& name, const std::string& cat, const std::string& shape, float dmg, const std::string& type, float rate, float reach, float weight, float length, S size, std::initializer_list<const char*> tags) {
        ItemDef& d = add(id, name, cat, K::melee, shape);
        d.damage = dmg;
        d.damage_type = type;
        d.rate = rate;
        d.range = reach;
        d.weight = weight;
        d.length = length;
        d.size = size;
        d.stamina = 6.0f + weight * 6.0f;
        d.knockback = 20.0f + weight * 40.0f;
        d.block = 0.3f;
        d.color = steel;
        d.accent = leather;
        d.volume = length * 0.3f;
        d.two_handed = size >= S::large;
        d.grid_w = length >= 1.2f ? 5 : length >= 0.8f ? 4 : length >= 0.45f ? 3 : length >= 0.25f ? 2 : 1;
        d.grid_h = 1;
        for (const char* t : tags) d.tags.push_back(t);
        return d;
    }

    ItemDef& bow(const std::string& id, const std::string& name, const std::string& cat, const std::string& shape, float dmg, float speed, float draw, const std::string& ammo, float weight, float length, S size) {
        ItemDef& d = add(id, name, cat, K::bow, shape);
        d.damage = dmg;
        d.damage_type = "pierce";
        d.speed = speed;
        d.draw_time = draw;
        d.ammo = ammo;
        d.weight = weight;
        d.length = length;
        d.size = size;
        d.rate = 1.0f / std::max(0.2f, draw + 0.4f);
        d.magazine = 1;
        d.reload = draw;
        d.projectile_mass = 0.025f;
        d.drag = 0.002f;
        d.spread = 0.8f;
        d.range = speed * 1.5f;
        d.ballistics = "drop";
        d.color = wood;
        d.accent = leather;
        d.two_handed = true;
        d.grid_w = length >= 1.4f ? 5 : 4;
        d.grid_h = 1;
        d.volume = length * 0.4f;
        return d;
    }

    ItemDef& magic(const std::string& id, const std::string& name, const std::string& cat, const std::string& shape, const std::string& spell, float dmg, const std::string& type, float mana, float rate, const std::string& effect, float effect_time, Vec3 color, float weight) {
        ItemDef& d = add(id, name, cat, K::magic, shape);
        d.spell = spell;
        d.damage = dmg;
        d.damage_type = type;
        d.mana = mana;
        d.rate = rate;
        d.effect = effect;
        d.effect_time = effect_time;
        d.effect_chance = effect.empty() ? 0.0f : 1.0f;
        d.color = color;
        d.accent = wood;
        d.weight = weight;
        d.speed = spell == "projectile" ? 35.0f : 0.0f;
        d.range = spell == "area" ? 6.0f : 40.0f;
        d.blast_radius = spell == "area" ? 6.0f : 0.0f;
        d.size = shape == "staff" ? S::large : S::small;
        d.length = shape == "staff" ? 1.7f : shape == "wand" ? 0.35f : 0.3f;
        d.grid_w = shape == "staff" ? 5 : shape == "wand" ? 2 : 2;
        d.grid_h = shape == "tome" || shape == "orb" ? 2 : 1;
        d.two_handed = shape == "staff";
        return d;
    }

    ItemDef& throwable(const std::string& id, const std::string& name, const std::string& cat, const std::string& shape, float dmg, const std::string& type, float radius, float fuse, float speed, float weight, int stack) {
        ItemDef& d = add(id, name, cat, K::throwable, shape);
        d.damage = dmg;
        d.damage_type = type;
        d.blast_radius = radius;
        d.fuse = fuse;
        d.speed = speed;
        d.weight = weight;
        d.max_stack = static_cast<float>(stack);
        d.size = S::small;
        d.rate = 1.0f;
        d.projectile_mass = weight;
        d.drag = 0.01f;
        d.ballistics = "drop";
        d.color = olive;
        d.volume = 0.3f;
        return d;
    }

    ItemDef& ammo(const std::string& id, const std::string& name, const std::string& cat, float weight_each, int stack, Vec3 color) {
        ItemDef& d = add(id, name, cat, K::ammo, "ammo");
        d.weight = weight_each;
        d.volume = weight_each * 0.4f;
        d.max_stack = static_cast<float>(stack);
        d.size = S::tiny;
        d.unit = "rounds";
        d.color = color;
        return d;
    }

    ItemDef& potion(const std::string& id, const std::string& name, const std::string& effect, float time, float strength, float heal, Vec3 color, const std::string& cures = std::string()) {
        ItemDef& d = add(id, name, "Potions", K::potion, "potion");
        d.use_effect = effect;
        d.use_time = time;
        d.use_strength = strength;
        d.heal = heal;
        d.color = color;
        d.accent = glass;
        d.weight = 0.35f;
        d.volume = 0.3f;
        d.max_stack = 5.0f;
        d.size = S::small;
        d.cures = cures;
        return d;
    }

    ItemDef& food(const std::string& id, const std::string& name, const std::string& shape, float hunger, float sat, float water, float weight, int stack, Vec3 color) {
        ItemDef& d = add(id, name, "Food", K::food, shape);
        d.food = hunger;
        d.saturation = sat;
        d.water = water;
        d.weight = weight;
        d.volume = weight * 1.2f;
        d.max_stack = static_cast<float>(stack);
        d.size = S::small;
        d.color = color;
        return d;
    }

    ItemDef& drink(const std::string& id, const std::string& name, const std::string& shape, float water, float weight, Vec3 color) {
        ItemDef& d = add(id, name, "Drinks", K::drink, shape);
        d.water = water;
        d.weight = weight;
        d.volume = weight;
        d.max_stack = 4.0f;
        d.size = S::small;
        d.color = color;
        return d;
    }

    ItemDef& medical(const std::string& id, const std::string& name, const std::string& shape, float heal, const std::string& cures, const std::string& effect, float time, float weight, int stack) {
        ItemDef& d = add(id, name, "Medical", K::medical, shape);
        d.heal = heal;
        d.cures = cures;
        d.use_effect = effect;
        d.use_time = time;
        d.weight = weight;
        d.volume = weight * 1.5f;
        d.max_stack = static_cast<float>(stack);
        d.size = S::small;
        d.color = Vec3{0.85f, 0.85f, 0.85f};
        d.accent = red;
        return d;
    }

    ItemDef& wear(const std::string& id, const std::string& name, const std::string& cat, const std::string& slot, const std::string& shape, float armor, float insulation, float weight, Vec3 color, K kind = K::clothing) {
        ItemDef& d = add(id, name, cat, kind, shape);
        d.wear = slot;
        d.armor = armor;
        d.insulation = insulation;
        d.weight = weight;
        d.volume = weight * 1.5f;
        d.color = color;
        d.size = slot == "torso" || slot == "vest" || slot == "legs" ? S::medium : S::small;
        if (kind == K::armor) d.armor_types = "physical,melee,blunt,slash,pierce,projectile,explosion";
        d.grid_w = d.size == S::medium ? 2 : 2;
        d.grid_h = d.size == S::medium ? 2 : 1;
        return d;
    }

    ItemDef& container(const std::string& id, const std::string& name, const std::string& cat, const std::string& slot, const std::string& shape, int w, int h, float capacity, float weight, Vec3 color) {
        ItemDef& d = add(id, name, cat, K::container, shape);
        d.wear = slot;
        d.slots_w = w;
        d.slots_h = h;
        d.capacity = capacity;
        d.weight = weight;
        d.volume = capacity * 0.25f + 1.0f;
        d.color = color;
        d.size = capacity > 30.0f ? S::large : capacity > 8.0f ? S::medium : S::small;
        d.grid_w = std::max(1, std::min(w, 5));
        d.grid_h = std::max(1, std::min(h, 5));
        return d;
    }

    ItemDef& holster(const std::string& id, const std::string& name, const std::string& slot, const std::string& shape, const std::string& kinds, S size, int count, float weight, Vec3 color) {
        ItemDef& d = add(id, name, "Holsters & Slings", K::holster, shape);
        d.wear = slot;
        d.holster_kinds = kinds;
        d.holster_size = size;
        d.holster_count = count;
        d.weight = weight;
        d.volume = weight * 2.0f;
        d.color = color;
        d.size = S::small;
        return d;
    }
};

void guns(Library& L) {
    auto pistol = [&](const char* id, const char* name, float dmg, int mag, const char* ammo, float speed, float weight, Vec3 c = gunmetal) {
        ItemDef& d = L.gun(id, name, "Guns/Pistols", "pistol", dmg, 3.5f, mag, ammo, speed, 1.6f, weight, 0.2f, false, S::small, 2, 1);
        d.color = c;
        d.drag = 0.0028f;
        return std::ref(d);
    };
    pistol("glock17", "Glock 17", 24.0f, 17, "9mm", 375.0f, 0.71f, polymer);
    pistol("beretta_m9", "Beretta M9", 24.0f, 15, "9mm", 381.0f, 0.95f, black);
    pistol("m1911", "M1911", 30.0f, 7, "45acp", 253.0f, 1.1f, steel);
    pistol("desert_eagle", "Desert Eagle", 48.0f, 7, "50ae", 470.0f, 2.0f, steel).get().recoil = 4.0f;
    pistol("makarov", "Makarov PM", 20.0f, 8, "9x18", 315.0f, 0.73f);
    pistol("luger_p08", "Luger P08", 23.0f, 8, "9mm", 350.0f, 0.87f, steel);
    pistol("fn57", "FN Five-seveN", 22.0f, 20, "57x28", 650.0f, 0.61f, tan);
    pistol("p226", "SIG P226", 24.0f, 15, "9mm", 360.0f, 0.96f, black);
    pistol("usp45", "HK USP .45", 30.0f, 12, "45acp", 260.0f, 0.75f, polymer);
    pistol("g18", "Glock 18 (auto)", 22.0f, 33, "9mm", 375.0f, 0.75f, polymer).get().automatic = true;
    L.v.back().rate = 20.0f;
    auto revolver = [&](const char* id, const char* name, float dmg, int mag, const char* ammo, float speed, float weight, Vec3 c) {
        ItemDef& d = L.gun(id, name, "Guns/Revolvers", "revolver", dmg, 2.0f, mag, ammo, speed, 1.2f, weight, 0.28f, false, S::small, 2, 1);
        d.color = c;
        d.accent = wood;
        d.reload = 3.2f;
        d.drag = 0.0025f;
    };
    revolver("colt_saa", "Colt Single Action Army", 34.0f, 6, "45colt", 280.0f, 1.05f, steel);
    revolver("python357", "Colt Python .357", 38.0f, 6, "357", 440.0f, 1.2f, steel);
    revolver("sw500", "S&W Model 500", 60.0f, 5, "500sw", 600.0f, 2.1f, steel);
    revolver("nagant_m1895", "Nagant M1895", 22.0f, 7, "762nagant", 272.0f, 0.8f, gunmetal);
    revolver("webley", "Webley Mk VI", 30.0f, 6, "455webley", 190.0f, 1.1f, gunmetal);

    auto smg = [&](const char* id, const char* name, float dmg, float rate, int mag, const char* ammo, float speed, float weight, Vec3 c = black) {
        ItemDef& d = L.gun(id, name, "Guns/Submachine guns", "smg", dmg, rate, mag, ammo, speed, 2.2f, weight, 0.55f, true, S::medium, 3, 2);
        d.color = c;
        d.drag = 0.0026f;
    };
    smg("mp5", "HK MP5", 23.0f, 13.3f, 30, "9mm", 400.0f, 2.5f);
    smg("uzi", "Uzi", 22.0f, 10.0f, 32, "9mm", 400.0f, 3.5f);
    smg("mp7", "HK MP7", 20.0f, 15.8f, 40, "46x30", 735.0f, 1.9f);
    smg("thompson", "Thompson M1928", 29.0f, 12.0f, 30, "45acp", 285.0f, 4.9f, wood);
    smg("p90", "FN P90", 21.0f, 15.0f, 50, "57x28", 715.0f, 2.6f, polymer);
    smg("vector", "Kriss Vector", 28.0f, 20.0f, 25, "45acp", 290.0f, 2.7f);
    smg("mp40", "MP 40", 22.0f, 9.2f, 32, "9mm", 380.0f, 4.0f, gunmetal);
    smg("ppsh41", "PPSh-41", 20.0f, 16.7f, 71, "762x25", 488.0f, 3.6f, wood);
    smg("ump45", "HK UMP45", 28.0f, 10.0f, 25, "45acp", 285.0f, 2.3f);
    smg("sten", "Sten Mk II", 22.0f, 9.0f, 32, "9mm", 365.0f, 3.2f, gunmetal);

    auto rifle = [&](const char* id, const char* name, const char* cat, const char* shape, float dmg, float rate, int mag, const char* ammo, float speed, float spread, float weight, float len, bool autom, Vec3 c, Vec3 a) {
        ItemDef& d = L.gun(id, name, cat, shape, dmg, rate, mag, ammo, speed, spread, weight, len, autom, S::large, len > 1.0f ? 5 : 4, 2);
        d.color = c;
        d.accent = a;
        d.drag = 0.0012f;
        d.projectile_mass = 0.008f;
        d.penetration = speed / 100.0f;
        return std::ref(d);
    };
    rifle("ak47", "AK-47", "Guns/Assault rifles", "rifle", 36.0f, 10.0f, 30, "762x39", 715.0f, 0.9f, 4.3f, 0.88f, true, gunmetal, wood);
    rifle("akm", "AKM", "Guns/Assault rifles", "rifle", 36.0f, 10.0f, 30, "762x39", 715.0f, 0.85f, 3.1f, 0.88f, true, gunmetal, wood);
    rifle("ak74", "AK-74", "Guns/Assault rifles", "rifle", 30.0f, 10.0f, 30, "545x39", 900.0f, 0.7f, 3.3f, 0.94f, true, gunmetal, polymer);
    rifle("m4a1", "M4A1 Carbine", "Guns/Assault rifles", "carbine", 30.0f, 13.3f, 30, "556", 910.0f, 0.6f, 2.9f, 0.84f, true, black, black);
    rifle("m16a2", "M16A2", "Guns/Assault rifles", "rifle", 32.0f, 13.3f, 30, "556", 948.0f, 0.5f, 3.6f, 1.0f, false, black, black);
    rifle("fal", "FN FAL", "Guns/Battle rifles", "rifle", 45.0f, 11.0f, 20, "762x51", 840.0f, 0.6f, 4.3f, 1.09f, true, black, black);
    rifle("g3", "HK G3", "Guns/Battle rifles", "rifle", 45.0f, 10.0f, 20, "762x51", 800.0f, 0.6f, 4.4f, 1.03f, true, black, black);
    rifle("scar_l", "FN SCAR-L", "Guns/Assault rifles", "carbine", 30.0f, 10.8f, 30, "556", 870.0f, 0.55f, 3.3f, 0.89f, true, tan, black);
    rifle("scar_h", "FN SCAR-H", "Guns/Battle rifles", "carbine", 45.0f, 10.0f, 20, "762x51", 714.0f, 0.6f, 3.6f, 0.97f, true, tan, black);
    rifle("aug", "Steyr AUG", "Guns/Assault rifles", "bullpup", 30.0f, 11.3f, 30, "556", 970.0f, 0.5f, 3.6f, 0.79f, true, olive, black);
    rifle("famas", "FAMAS", "Guns/Assault rifles", "bullpup", 29.0f, 15.8f, 25, "556", 960.0f, 0.65f, 3.6f, 0.76f, true, black, black);
    rifle("g36", "HK G36", "Guns/Assault rifles", "rifle", 30.0f, 12.5f, 30, "556", 920.0f, 0.55f, 3.6f, 1.0f, true, polymer, polymer);
    rifle("sks", "SKS", "Guns/Semi-auto rifles", "rifle", 36.0f, 3.0f, 10, "762x39", 735.0f, 0.5f, 3.9f, 1.02f, false, gunmetal, wood);
    rifle("m1_garand", "M1 Garand", "Guns/Semi-auto rifles", "rifle", 50.0f, 3.0f, 8, "3006", 853.0f, 0.4f, 4.4f, 1.1f, false, gunmetal, wood);
    rifle("m14", "M14", "Guns/Battle rifles", "rifle", 46.0f, 12.0f, 20, "762x51", 850.0f, 0.5f, 4.1f, 1.12f, true, gunmetal, wood);
    rifle("lee_enfield", "Lee-Enfield No. 4", "Guns/Bolt-action rifles", "rifle", 55.0f, 0.8f, 10, "303", 744.0f, 0.3f, 4.1f, 1.13f, false, gunmetal, wood);
    rifle("mosin", "Mosin-Nagant M91/30", "Guns/Bolt-action rifles", "rifle", 60.0f, 0.7f, 5, "762x54r", 865.0f, 0.3f, 4.0f, 1.23f, false, gunmetal, wood);
    rifle("kar98k", "Karabiner 98k", "Guns/Bolt-action rifles", "rifle", 60.0f, 0.75f, 5, "792x57", 760.0f, 0.3f, 3.9f, 1.11f, false, gunmetal, wood);
    rifle("springfield", "M1903 Springfield", "Guns/Bolt-action rifles", "rifle", 58.0f, 0.75f, 5, "3006", 853.0f, 0.3f, 3.9f, 1.1f, false, gunmetal, wood);
    rifle("win1873", "Winchester 1873", "Guns/Lever-action rifles", "rifle", 34.0f, 1.5f, 14, "4440", 380.0f, 0.6f, 3.7f, 1.0f, false, steel, wood);
    rifle("marlin336", "Marlin 336", "Guns/Lever-action rifles", "rifle", 45.0f, 1.5f, 6, "3030", 720.0f, 0.5f, 3.4f, 0.97f, false, steel, wood);
    rifle("ruger1022", "Ruger 10/22", "Guns/Semi-auto rifles", "rifle", 12.0f, 4.0f, 10, "22lr", 380.0f, 0.5f, 2.3f, 0.94f, false, black, wood);

    auto sniper = [&](const char* id, const char* name, float dmg, int mag, const char* ammo, float speed, float weight, float len, float drag, bool semi, Vec3 c) {
        ItemDef& d = rifle(id, name, "Guns/Sniper rifles", "sniper", dmg, semi ? 2.0f : 0.7f, mag, ammo, speed, 0.08f, weight, len, false, c, black).get();
        d.drag = drag;
        d.recoil = dmg * 0.05f;
    };
    sniper("m24", "M24 SWS", 75.0f, 5, "762x51", 790.0f, 5.4f, 1.09f, 0.0010f, false, olive);
    sniper("rem700", "Remington 700", 75.0f, 4, "308", 830.0f, 3.6f, 1.06f, 0.0010f, false, black);
    sniper("barrett_m82", "Barrett M82", 150.0f, 10, "50bmg", 853.0f, 14.0f, 1.45f, 0.0004f, true, gunmetal);
    sniper("svd", "SVD Dragunov", 70.0f, 10, "762x54r", 830.0f, 4.3f, 1.22f, 0.0011f, true, wood);
    sniper("awm", "AWM .338", 110.0f, 5, "338lm", 936.0f, 6.9f, 1.23f, 0.0006f, false, olive);
    sniper("intervention", "CheyTac M200", 130.0f, 7, "408chey", 900.0f, 14.0f, 1.4f, 0.0004f, false, black);

    auto shotgun = [&](const char* id, const char* name, float dmg, float rate, int mag, int pellets, float spread, float weight, float len, bool autom, Vec3 c, Vec3 a) {
        ItemDef& d = L.gun(id, name, "Guns/Shotguns", len < 0.6f ? "sawed_off" : "shotgun", dmg, rate, mag, "12g_buck", 400.0f, spread, weight, len, autom, len < 0.6f ? S::medium : S::large, len < 0.6f ? 3 : 5, 1);
        d.pellets = pellets;
        d.color = c;
        d.accent = a;
        d.drag = 0.006f;
        d.projectile_mass = 0.0035f;
        d.reload = 0.5f * static_cast<float>(mag);
        d.range = 40.0f;
    };
    shotgun("rem870", "Remington 870", 12.0f, 1.2f, 6, 9, 4.0f, 3.6f, 1.06f, false, black, wood);
    shotgun("moss500", "Mossberg 500", 12.0f, 1.2f, 6, 9, 4.2f, 3.4f, 1.0f, false, black, polymer);
    shotgun("double_barrel", "Double-barrel shotgun", 13.0f, 3.0f, 2, 9, 4.5f, 3.2f, 1.1f, false, steel, wood);
    shotgun("sawed_off", "Sawed-off shotgun", 13.0f, 3.0f, 2, 9, 9.0f, 2.2f, 0.5f, false, steel, wood);
    shotgun("spas12", "SPAS-12", 12.0f, 4.0f, 8, 9, 4.5f, 4.4f, 1.04f, false, black, black);
    shotgun("aa12", "AA-12", 11.0f, 5.0f, 20, 9, 5.0f, 5.2f, 0.97f, true, black, black);
    shotgun("benelli_m4", "Benelli M4", 12.0f, 4.0f, 7, 9, 4.0f, 3.8f, 1.01f, false, black, black);
    shotgun("ks23", "KS-23", 20.0f, 1.0f, 3, 6, 5.0f, 3.8f, 1.04f, false, gunmetal, wood);

    auto mg = [&](const char* id, const char* name, float dmg, float rate, int mag, const char* ammo, float speed, float weight, float len, Vec3 c) {
        ItemDef& d = L.gun(id, name, "Guns/Machine guns", "lmg", dmg, rate, mag, ammo, speed, 1.4f, weight, len, true, S::huge, 5, 2);
        d.color = c;
        d.reload = 5.5f;
        d.drag = 0.0011f;
    };
    mg("m249", "M249 SAW", 30.0f, 13.3f, 200, "556", 915.0f, 7.5f, 1.04f, black);
    mg("pkm", "PKM", 45.0f, 11.0f, 100, "762x54r", 825.0f, 7.5f, 1.17f, gunmetal);
    mg("m60", "M60", 45.0f, 9.5f, 100, "762x51", 853.0f, 10.5f, 1.1f, gunmetal);
    mg("mg42", "MG 42", 45.0f, 20.0f, 50, "792x57", 755.0f, 11.6f, 1.22f, gunmetal);
    mg("rpk", "RPK", 36.0f, 10.0f, 75, "762x39", 745.0f, 4.8f, 1.04f, wood);
    mg("minigun", "M134 Minigun", 40.0f, 50.0f, 1500, "762x51", 853.0f, 39.0f, 0.8f, steel);

    auto launcher = [&](const char* id, const char* name, float dmg, float radius, const char* ammo, float speed, float weight, float len, int mag) {
        ItemDef& d = L.gun(id, name, "Guns/Launchers", "launcher", dmg, 0.5f, mag, ammo, speed, 0.8f, weight, len, false, S::huge, 5, 2);
        d.damage_type = "explosion";
        d.blast_radius = radius;
        d.projectile_mass = 1.0f;
        d.drag = 0.004f;
        d.color = olive;
        d.reload = 4.0f;
        d.ballistics = "drop";
    };
    launcher("rpg7", "RPG-7", 200.0f, 6.0f, "rpg_rocket", 115.0f, 7.0f, 0.95f, 1);
    launcher("m72_law", "M72 LAW", 180.0f, 5.0f, "law_rocket", 145.0f, 2.5f, 0.88f, 1);
    launcher("m79", "M79 Grenade Launcher", 120.0f, 4.0f, "40mm", 76.0f, 2.7f, 0.73f, 1);
    launcher("m32", "M32 MGL", 120.0f, 4.0f, "40mm", 76.0f, 5.3f, 0.81f, 6);
    launcher("flare_gun", "Flare gun", 15.0f, 0.5f, "flare", 70.0f, 0.6f, 0.3f, 1);
    L.v.back().damage_type = "fire";
    L.v.back().effect = "burning";
    L.v.back().effect_time = 6.0f;
    L.v.back().effect_chance = 1.0f;
    L.v.back().size = S::small;
    L.v.back().grid_w = 2;
    L.v.back().grid_h = 1;

    auto old = [&](const char* id, const char* name, const char* shape, float dmg, float reload, const char* ammo, float speed, float spread, float weight, float len, int pellets) {
        ItemDef& d = L.gun(id, name, "Guns/Black powder", shape, dmg, 1.0f / reload, 1, ammo, speed, spread, weight, len, false, len < 0.5f ? S::small : S::large, len < 0.5f ? 2 : 5, 1);
        d.reload = reload;
        d.pellets = pellets;
        d.projectile_mass = 0.03f;
        d.drag = 0.004f;
        d.color = iron;
        d.accent = wood;
        d.ballistics = "drop";
    };
    old("brown_bess", "Brown Bess musket", "musket", 70.0f, 15.0f, "musket_ball", 300.0f, 2.5f, 4.7f, 1.4f, 1);
    old("charleville", "Charleville musket", "musket", 70.0f, 15.0f, "musket_ball", 320.0f, 2.3f, 4.5f, 1.5f, 1);
    old("blunderbuss", "Blunderbuss", "blunderbuss", 12.0f, 18.0f, "musket_shot", 250.0f, 9.0f, 3.5f, 0.8f, 10);
    old("flintlock_pistol", "Flintlock pistol", "flintlock", 50.0f, 12.0f, "musket_ball", 250.0f, 3.0f, 1.2f, 0.4f, 1);
    old("arquebus", "Arquebus", "musket", 65.0f, 20.0f, "musket_ball", 280.0f, 3.5f, 5.0f, 1.3f, 1);
    old("kentucky_rifle", "Kentucky long rifle", "musket", 65.0f, 20.0f, "musket_ball", 400.0f, 0.8f, 4.0f, 1.5f, 1);

    auto scifi = [&](const char* id, const char* name, const char* shape, float dmg, float rate, int mag, const char* ammo, float speed, float spread, float weight, bool autom, const char* type, Vec3 c) {
        ItemDef& d = L.gun(id, name, "Guns/Sci-fi", shape, dmg, rate, mag, ammo, speed, spread, weight, shape == std::string("pistol") ? 0.25f : 0.9f, autom, shape == std::string("pistol") ? S::small : S::large, shape == std::string("pistol") ? 2 : 4, shape == std::string("pistol") ? 1 : 2);
        d.damage_type = type;
        d.color = c;
        d.accent = Vec3{0.2f, 0.8f, 1.0f};
        d.drag = 0.0f;
        d.projectile_mass = 0.001f;
        d.ballistics = speed > 5000.0f ? "hitscan" : "travel";
    };
    scifi("laser_pistol", "Laser pistol", "pistol", 22.0f, 4.0f, 30, "energy_cell", 1e6f, 0.3f, 0.9f, false, "fire", Vec3{0.85f, 0.85f, 0.9f});
    scifi("laser_rifle", "Laser rifle", "rifle", 30.0f, 8.0f, 60, "energy_cell", 1e6f, 0.2f, 3.0f, true, "fire", Vec3{0.85f, 0.85f, 0.9f});
    scifi("plasma_rifle", "Plasma rifle", "rifle", 45.0f, 6.0f, 40, "plasma_cell", 120.0f, 0.6f, 4.0f, true, "fire", Vec3{0.3f, 0.35f, 0.4f});
    scifi("railgun", "Railgun", "sniper", 160.0f, 0.5f, 5, "rail_slug", 6000.0f, 0.02f, 9.0f, false, "projectile", Vec3{0.2f, 0.22f, 0.25f});
    scifi("tesla_gun", "Tesla gun", "rifle", 18.0f, 10.0f, 50, "energy_cell", 1e6f, 1.0f, 5.0f, true, "shock", Vec3{0.6f, 0.4f, 0.2f});
    L.v.back().effect = "shocked";
    L.v.back().effect_time = 1.5f;
    L.v.back().effect_chance = 0.5f;
}

void ammunition(Library& L) {
    const Vec3 brass{0.78f, 0.6f, 0.25f};
    struct A {
        const char* id;
        const char* name;
        float w;
        int stack;
    };
    const A pistol[] = {{"9mm", "9x19mm Parabellum", 0.012f, 60}, {"45acp", ".45 ACP", 0.021f, 50}, {"50ae", ".50 AE", 0.033f, 30}, {"9x18", "9x18mm Makarov", 0.01f, 60}, {"57x28", "5.7x28mm", 0.006f, 80}, {"46x30", "4.6x30mm", 0.006f, 80}, {"357", ".357 Magnum", 0.016f, 50}, {"45colt", ".45 Colt", 0.02f, 50}, {"500sw", ".500 S&W Magnum", 0.04f, 25}, {"762nagant", "7.62x38mmR Nagant", 0.012f, 50}, {"455webley", ".455 Webley", 0.02f, 50}, {"762x25", "7.62x25mm Tokarev", 0.011f, 60}, {"22lr", ".22 Long Rifle", 0.0033f, 100}};
    for (const A& a : pistol) L.ammo(a.id, a.name, "Ammo/Pistol", a.w, a.stack, brass);
    const A rifle[] = {{"556", "5.56x45mm NATO", 0.012f, 60}, {"545x39", "5.45x39mm", 0.011f, 60}, {"762x39", "7.62x39mm", 0.016f, 60}, {"762x51", "7.62x51mm NATO", 0.025f, 40}, {"308", ".308 Winchester", 0.025f, 40}, {"762x54r", "7.62x54mmR", 0.022f, 40}, {"3006", ".30-06 Springfield", 0.027f, 40}, {"303", ".303 British", 0.025f, 40}, {"792x57", "7.92x57mm Mauser", 0.024f, 40}, {"4440", ".44-40 Winchester", 0.018f, 50}, {"3030", ".30-30 Winchester", 0.02f, 40}, {"338lm", ".338 Lapua Magnum", 0.046f, 20}, {"50bmg", ".50 BMG", 0.115f, 10}, {"408chey", ".408 CheyTac", 0.07f, 10}};
    for (const A& a : rifle) L.ammo(a.id, a.name, "Ammo/Rifle", a.w, a.stack, brass);
    L.ammo("12g_buck", "12 gauge 00 buckshot", "Ammo/Shotgun", 0.045f, 25, red);
    L.ammo("12g_slug", "12 gauge slug", "Ammo/Shotgun", 0.045f, 25, Vec3{0.1f, 0.3f, 0.7f});
    L.ammo("12g_dragon", "12 gauge dragon's breath", "Ammo/Shotgun", 0.045f, 25, Vec3{0.9f, 0.5f, 0.1f});
    L.ammo("rpg_rocket", "PG-7V rocket", "Ammo/Explosive", 2.6f, 2, olive).size = S::medium;
    L.ammo("law_rocket", "M72 rocket", "Ammo/Explosive", 1.0f, 1, olive);
    L.ammo("40mm", "40mm grenade", "Ammo/Explosive", 0.23f, 12, olive);
    L.ammo("flare", "Signal flare", "Ammo/Explosive", 0.1f, 10, red);
    L.ammo("musket_ball", "Musket ball and powder", "Ammo/Black powder", 0.035f, 30, iron);
    L.ammo("musket_shot", "Lead shot and powder", "Ammo/Black powder", 0.05f, 20, iron);
    L.ammo("energy_cell", "Energy cell", "Ammo/Sci-fi", 0.1f, 10, Vec3{0.2f, 0.8f, 1.0f}).max_stack = 0.0f;
    L.v.back().continuous = true;
    L.v.back().unit = "%";
    L.ammo("plasma_cell", "Plasma cell", "Ammo/Sci-fi", 0.15f, 10, Vec3{0.3f, 1.0f, 0.4f});
    L.ammo("rail_slug", "Rail slug", "Ammo/Sci-fi", 0.05f, 20, steel);
    struct Arrow {
        const char* id;
        const char* name;
        float dmg;
        const char* effect;
    };
    const Arrow arrows[] = {{"arrow", "Wooden arrow", 1.0f, ""}, {"arrow_broadhead", "Broadhead arrow", 1.3f, "bleeding"}, {"arrow_bodkin", "Bodkin arrow", 1.1f, ""}, {"arrow_fire", "Fire arrow", 1.0f, "burning"}, {"arrow_poison", "Poison arrow", 0.9f, "poison"}, {"arrow_carbon", "Carbon arrow", 1.2f, ""}};
    for (const Arrow& a : arrows) {
        ItemDef& d = L.ammo(a.id, a.name, "Ammo/Arrows & bolts", 0.03f, 30, wood);
        d.shape = "arrow";
        d.damage = a.dmg;
        d.effect = a.effect;
        d.effect_time = 6.0f;
        d.effect_chance = *a.effect ? 1.0f : 0.0f;
        d.unit = "";
    }
    L.ammo("bolt", "Crossbow bolt", "Ammo/Arrows & bolts", 0.04f, 30, wood).shape = "arrow";
    L.ammo("bolt_heavy", "Heavy crossbow bolt", "Ammo/Arrows & bolts", 0.07f, 20, iron).shape = "arrow";
    L.ammo("sling_stone", "Sling stone", "Ammo/Primitive", 0.1f, 20, Vec3{0.5f, 0.48f, 0.45f}).shape = "rock";
    L.ammo("lead_bullet", "Lead sling bullet (glans)", "Ammo/Primitive", 0.05f, 30, iron).shape = "rock";
    L.ammo("steel_ball", "Steel ball", "Ammo/Primitive", 0.008f, 60, steel).shape = "rock";
    L.ammo("dart", "Blowgun dart", "Ammo/Primitive", 0.005f, 30, wood).shape = "arrow";
}

void melee_weapons(Library& L) {
    auto blade = [&](const char* id, const char* name, const char* cat, const char* shape, float dmg, const char* type, float rate, float reach, float weight, float len, S size, std::initializer_list<const char*> tags, Vec3 c = steel, Vec3 a = leather) {
        ItemDef& d = L.melee(id, name, cat, shape, dmg, type, rate, reach, weight, len, size, tags);
        d.color = c;
        d.accent = a;
        return std::ref(d);
    };
    const char* R = "Melee/Roman";
    blade("gladius", "Gladius", R, "sword", 34.0f, "pierce", 1.6f, 1.0f, 1.0f, 0.68f, S::medium, {"sharp", "roman", "one_handed"});
    blade("spatha", "Spatha", R, "sword", 38.0f, "slash", 1.3f, 1.25f, 1.3f, 0.9f, S::medium, {"sharp", "roman"});
    blade("pugio", "Pugio", R, "dagger", 20.0f, "pierce", 2.4f, 0.6f, 0.4f, 0.3f, S::small, {"sharp", "roman"});
    blade("hasta", "Hasta", R, "spear", 40.0f, "pierce", 1.0f, 2.3f, 2.0f, 2.1f, S::huge, {"sharp", "roman", "reach"}, iron, wood);
    blade("falx", "Falx", R, "falx", 46.0f, "slash", 0.9f, 1.4f, 2.0f, 1.2f, S::large, {"sharp", "roman"}, iron, wood);
    ItemDef& scutum = blade("scutum", "Scutum", R, "shield_tower", 8.0f, "blunt", 0.8f, 0.8f, 7.5f, 1.05f, S::huge, {"shield", "roman", "offhand"}, red, bronze).get();
    scutum.block = 0.9f;
    ItemDef& parma = blade("parma", "Parma", R, "shield_round", 6.0f, "blunt", 1.0f, 0.7f, 4.0f, 0.9f, S::large, {"shield", "roman", "offhand"}, bronze, wood).get();
    parma.block = 0.75f;
    const char* G = "Melee/Greek";
    blade("xiphos", "Xiphos", G, "sword", 32.0f, "slash", 1.5f, 1.0f, 0.9f, 0.6f, S::medium, {"sharp", "greek"}, bronze);
    blade("kopis", "Kopis", G, "falchion", 40.0f, "slash", 1.2f, 1.1f, 1.2f, 0.65f, S::medium, {"sharp", "greek"});
    blade("dory", "Dory", G, "spear", 42.0f, "pierce", 1.0f, 2.6f, 2.0f, 2.5f, S::huge, {"sharp", "greek", "reach"}, bronze, wood);
    blade("sarissa", "Sarissa", G, "pike", 45.0f, "pierce", 0.6f, 5.5f, 6.0f, 5.5f, S::huge, {"sharp", "greek", "reach"}, iron, wood);
    ItemDef& aspis = blade("aspis", "Aspis (hoplon)", G, "shield_round", 7.0f, "blunt", 0.9f, 0.8f, 7.0f, 0.95f, S::large, {"shield", "greek", "offhand"}, bronze, wood).get();
    aspis.block = 0.85f;
    const char* J = "Melee/Samurai";
    blade("katana", "Katana", J, "katana", 48.0f, "slash", 1.4f, 1.3f, 1.2f, 1.0f, S::large, {"sharp", "samurai"}, steel, black);
    blade("wakizashi", "Wakizashi", J, "katana", 36.0f, "slash", 1.8f, 0.9f, 0.7f, 0.6f, S::medium, {"sharp", "samurai"}, steel, black);
    blade("tanto", "Tanto", J, "dagger", 22.0f, "pierce", 2.4f, 0.55f, 0.3f, 0.3f, S::small, {"sharp", "samurai"}, steel, black);
    blade("nodachi", "Nodachi", J, "greatsword", 62.0f, "slash", 0.8f, 1.8f, 2.5f, 1.6f, S::huge, {"sharp", "samurai"}, steel, black);
    blade("odachi", "Odachi", J, "greatsword", 66.0f, "slash", 0.7f, 2.0f, 3.0f, 1.8f, S::huge, {"sharp", "samurai"}, steel, black);
    blade("naginata", "Naginata", J, "glaive", 52.0f, "slash", 0.9f, 2.4f, 2.5f, 2.2f, S::huge, {"sharp", "samurai", "reach"}, steel, dark_wood);
    blade("yari", "Yari", J, "spear", 44.0f, "pierce", 1.0f, 2.8f, 2.2f, 2.7f, S::huge, {"sharp", "samurai", "reach"}, steel, dark_wood);
    blade("kanabo", "Kanabo", J, "club_studded", 55.0f, "blunt", 0.6f, 1.5f, 6.0f, 1.5f, S::huge, {"blunt", "samurai"}, iron, dark_wood);
    blade("tessen", "Tessen (war fan)", J, "fan", 12.0f, "blunt", 2.5f, 0.5f, 0.6f, 0.3f, S::small, {"blunt", "samurai"}, black, gold);
    blade("jutte", "Jutte", J, "jutte", 14.0f, "blunt", 2.2f, 0.6f, 0.5f, 0.45f, S::small, {"blunt", "samurai"}, iron, red);
    blade("bo", "Bo staff", J, "staff_plain", 22.0f, "blunt", 1.6f, 1.8f, 1.5f, 1.8f, S::huge, {"blunt", "samurai"}, wood, wood);
    blade("kusarigama", "Kusarigama", J, "sickle", 28.0f, "slash", 1.4f, 2.0f, 1.0f, 0.5f, S::medium, {"sharp", "samurai", "chain"}, steel, dark_wood);
    blade("sai", "Sai", J, "sai", 16.0f, "pierce", 2.4f, 0.55f, 0.6f, 0.5f, S::small, {"pierce", "samurai"}, steel, black);
    blade("kunai_melee", "Kunai", J, "dagger", 18.0f, "pierce", 2.6f, 0.45f, 0.25f, 0.25f, S::tiny, {"sharp", "samurai", "ninja"}, black, black);
    const char* M = "Melee/Medieval";
    blade("arming_sword", "Arming sword", M, "sword", 40.0f, "slash", 1.4f, 1.2f, 1.1f, 0.95f, S::medium, {"sharp", "medieval"});
    blade("longsword", "Longsword", M, "longsword", 50.0f, "slash", 1.1f, 1.5f, 1.5f, 1.2f, S::large, {"sharp", "medieval"});
    blade("claymore", "Claymore", M, "greatsword", 60.0f, "slash", 0.8f, 1.8f, 2.5f, 1.4f, S::huge, {"sharp", "medieval"});
    blade("zweihander", "Zweihänder", M, "greatsword", 66.0f, "slash", 0.7f, 2.0f, 3.2f, 1.8f, S::huge, {"sharp", "medieval"});
    blade("falchion", "Falchion", M, "falchion", 44.0f, "slash", 1.3f, 1.1f, 1.3f, 0.85f, S::medium, {"sharp", "medieval"});
    blade("messer", "Messer", M, "falchion", 42.0f, "slash", 1.3f, 1.1f, 1.2f, 0.9f, S::medium, {"sharp", "medieval"});
    blade("rapier", "Rapier", M, "rapier", 34.0f, "pierce", 1.9f, 1.4f, 1.0f, 1.1f, S::large, {"sharp", "renaissance"});
    blade("estoc", "Estoc", M, "longsword", 44.0f, "pierce", 1.1f, 1.6f, 1.6f, 1.3f, S::large, {"pierce", "medieval"});
    blade("dagger", "Dagger", M, "dagger", 20.0f, "pierce", 2.4f, 0.6f, 0.4f, 0.35f, S::small, {"sharp", "medieval"});
    blade("rondel", "Rondel dagger", M, "dagger", 24.0f, "pierce", 2.2f, 0.6f, 0.45f, 0.4f, S::small, {"pierce", "medieval"});
    blade("battle_axe", "Battle axe", M, "axe", 48.0f, "slash", 1.0f, 1.2f, 2.0f, 0.9f, S::large, {"sharp", "medieval", "axe"}, steel, wood);
    blade("bearded_axe", "Bearded axe", M, "axe", 44.0f, "slash", 1.1f, 1.1f, 1.4f, 0.8f, S::medium, {"sharp", "viking", "axe"}, iron, wood);
    blade("dane_axe", "Dane axe", M, "greataxe", 62.0f, "slash", 0.75f, 1.8f, 2.4f, 1.6f, S::huge, {"sharp", "viking", "axe"}, iron, wood);
    blade("mace", "Mace", M, "mace", 42.0f, "blunt", 1.1f, 0.9f, 1.8f, 0.7f, S::medium, {"blunt", "medieval"}, iron, leather);
    blade("flanged_mace", "Flanged mace", M, "mace", 46.0f, "blunt", 1.0f, 0.9f, 2.0f, 0.75f, S::medium, {"blunt", "medieval"}, steel, leather);
    blade("morning_star", "Morning star", M, "morning_star", 48.0f, "blunt", 0.95f, 1.0f, 2.3f, 0.85f, S::medium, {"blunt", "spiked", "medieval"}, iron, wood);
    blade("flail", "Flail", M, "flail", 50.0f, "blunt", 0.85f, 1.3f, 2.4f, 0.9f, S::medium, {"blunt", "chain", "medieval"}, iron, wood);
    blade("warhammer", "Warhammer", M, "warhammer", 50.0f, "blunt", 0.9f, 1.0f, 2.0f, 0.75f, S::medium, {"blunt", "pierce", "medieval"}, steel, wood);
    blade("lucerne", "Lucerne hammer", M, "polehammer", 58.0f, "blunt", 0.7f, 2.2f, 3.2f, 2.0f, S::huge, {"blunt", "medieval", "reach"}, steel, wood);
    blade("halberd", "Halberd", M, "halberd", 60.0f, "slash", 0.7f, 2.3f, 3.2f, 2.0f, S::huge, {"sharp", "medieval", "reach"}, steel, wood);
    blade("glaive", "Glaive", M, "glaive", 54.0f, "slash", 0.8f, 2.3f, 2.8f, 2.1f, S::huge, {"sharp", "medieval", "reach"}, steel, wood);
    blade("bill", "Bill", M, "glaive", 52.0f, "slash", 0.8f, 2.3f, 2.8f, 2.1f, S::huge, {"sharp", "medieval", "reach"}, iron, wood);
    blade("pike", "Pike", M, "pike", 48.0f, "pierce", 0.6f, 4.8f, 5.0f, 4.8f, S::huge, {"pierce", "medieval", "reach"}, steel, wood);
    blade("poleaxe", "Poleaxe", M, "halberd", 58.0f, "slash", 0.75f, 1.9f, 2.8f, 1.8f, S::huge, {"sharp", "medieval"}, steel, wood);
    blade("war_scythe", "War scythe", M, "glaive", 50.0f, "slash", 0.8f, 2.2f, 2.5f, 2.0f, S::huge, {"sharp", "medieval"}, iron, wood);
    blade("lance", "Lance", M, "pike", 70.0f, "pierce", 0.4f, 3.5f, 6.0f, 3.5f, S::huge, {"pierce", "medieval", "mounted"}, wood, red);
    ItemDef& kite = blade("kite_shield", "Kite shield", M, "shield_kite", 7.0f, "blunt", 0.9f, 0.8f, 5.5f, 1.1f, S::huge, {"shield", "medieval", "offhand"}, Vec3{0.2f, 0.25f, 0.55f}, iron).get();
    kite.block = 0.85f;
    ItemDef& heater = blade("heater_shield", "Heater shield", M, "shield_kite", 6.0f, "blunt", 1.0f, 0.7f, 4.0f, 0.8f, S::large, {"shield", "medieval", "offhand"}, red, iron).get();
    heater.block = 0.8f;
    ItemDef& buckler = blade("buckler", "Buckler", M, "shield_round", 10.0f, "blunt", 1.8f, 0.5f, 1.2f, 0.35f, S::small, {"shield", "medieval", "offhand"}, steel, leather).get();
    buckler.block = 0.55f;
    const char* V = "Melee/Viking & Norse";
    blade("seax", "Seax", V, "dagger", 26.0f, "slash", 2.0f, 0.7f, 0.6f, 0.45f, S::small, {"sharp", "viking"}, iron, leather);
    blade("viking_sword", "Viking sword", V, "sword", 42.0f, "slash", 1.3f, 1.15f, 1.2f, 0.9f, S::medium, {"sharp", "viking"}, iron, leather);
    blade("skeggox", "Skeggox", V, "axe", 40.0f, "slash", 1.2f, 1.0f, 1.0f, 0.7f, S::medium, {"sharp", "viking", "axe"}, iron, wood);
    ItemDef& round = blade("viking_round_shield", "Round shield", V, "shield_round", 7.0f, "blunt", 1.0f, 0.75f, 5.0f, 0.85f, S::large, {"shield", "viking", "offhand"}, wood, iron).get();
    round.block = 0.8f;
    const char* W = "Melee/World";
    blade("scimitar", "Scimitar", W, "scimitar", 42.0f, "slash", 1.4f, 1.1f, 1.1f, 0.85f, S::medium, {"sharp", "middle_east"});
    blade("shamshir", "Shamshir", W, "scimitar", 44.0f, "slash", 1.35f, 1.15f, 1.1f, 0.9f, S::medium, {"sharp", "persian"});
    blade("khopesh", "Khopesh", W, "khopesh", 40.0f, "slash", 1.2f, 1.0f, 1.3f, 0.6f, S::medium, {"sharp", "egyptian"}, bronze);
    blade("tulwar", "Tulwar", W, "scimitar", 42.0f, "slash", 1.3f, 1.1f, 1.2f, 0.85f, S::medium, {"sharp", "indian"});
    blade("talwar", "Talwar", W, "scimitar", 42.0f, "slash", 1.3f, 1.1f, 1.2f, 0.85f, S::medium, {"sharp", "indian"});
    blade("dao", "Dao", W, "falchion", 42.0f, "slash", 1.3f, 1.1f, 1.1f, 0.85f, S::medium, {"sharp", "chinese"});
    blade("jian", "Jian", W, "sword", 38.0f, "pierce", 1.6f, 1.15f, 0.9f, 0.9f, S::medium, {"sharp", "chinese"});
    blade("guandao", "Guandao", W, "glaive", 58.0f, "slash", 0.75f, 2.2f, 3.0f, 2.0f, S::huge, {"sharp", "chinese", "reach"}, steel, red);
    blade("kris", "Kris", W, "kris", 22.0f, "pierce", 2.2f, 0.6f, 0.4f, 0.4f, S::small, {"sharp", "indonesian"});
    blade("kampilan", "Kampilan", W, "sword", 46.0f, "slash", 1.1f, 1.3f, 1.4f, 1.0f, S::large, {"sharp", "filipino"});
    blade("katar", "Katar", W, "katar", 26.0f, "pierce", 2.2f, 0.55f, 0.6f, 0.4f, S::small, {"pierce", "indian"});
    blade("macuahuitl", "Macuahuitl", W, "club_studded", 46.0f, "slash", 1.0f, 1.1f, 1.8f, 0.9f, S::large, {"sharp", "blunt", "aztec"}, dark_wood, black);
    blade("tomahawk_hist", "Tomahawk", W, "hatchet", 34.0f, "slash", 1.6f, 0.75f, 0.6f, 0.5f, S::small, {"sharp", "axe"}, iron, wood);
    blade("kukri", "Kukri", W, "kukri", 36.0f, "slash", 1.8f, 0.75f, 0.7f, 0.45f, S::small, {"sharp", "nepalese"}, steel, wood);
    blade("bolo", "Bolo knife", W, "machete", 34.0f, "slash", 1.7f, 0.8f, 0.7f, 0.5f, S::small, {"sharp", "filipino"}, steel, wood);
    blade("war_club", "War club", W, "club", 36.0f, "blunt", 1.2f, 0.9f, 1.5f, 0.7f, S::medium, {"blunt", "tribal"}, dark_wood, dark_wood);

    const char* MS = "Melee/Modern sharp";
    blade("combat_knife", "Combat knife", MS, "knife", 24.0f, "slash", 2.4f, 0.6f, 0.3f, 0.3f, S::small, {"sharp", "modern"}, gunmetal, black);
    blade("bayonet", "M9 bayonet", MS, "knife", 24.0f, "pierce", 2.2f, 0.6f, 0.35f, 0.3f, S::small, {"sharp", "modern"}, gunmetal, black);
    blade("machete", "Machete", MS, "machete", 34.0f, "slash", 1.7f, 0.85f, 0.6f, 0.6f, S::medium, {"sharp", "modern"}, steel, black);
    blade("karambit", "Karambit", MS, "karambit", 20.0f, "slash", 2.8f, 0.5f, 0.15f, 0.2f, S::tiny, {"sharp", "modern"}, black, black);
    blade("tactical_tomahawk", "Tactical tomahawk", MS, "hatchet", 36.0f, "slash", 1.6f, 0.75f, 0.7f, 0.4f, S::small, {"sharp", "modern", "axe"}, black, black);
    blade("fire_axe", "Fire axe", MS, "axe", 46.0f, "slash", 1.0f, 1.2f, 2.7f, 0.9f, S::large, {"sharp", "modern", "axe"}, red, Vec3{0.85f, 0.75f, 0.2f});
    blade("hatchet", "Hatchet", MS, "hatchet", 30.0f, "slash", 1.6f, 0.7f, 0.7f, 0.35f, S::small, {"sharp", "modern", "axe"}, steel, wood);
    blade("box_cutter", "Box cutter", MS, "knife", 10.0f, "slash", 3.0f, 0.45f, 0.08f, 0.15f, S::tiny, {"sharp", "improvised"}, Vec3{0.9f, 0.8f, 0.1f}, black);
    blade("kitchen_knife", "Kitchen knife", MS, "knife", 18.0f, "slash", 2.4f, 0.55f, 0.15f, 0.3f, S::small, {"sharp", "improvised"}, steel, black);
    blade("cleaver", "Meat cleaver", MS, "cleaver", 28.0f, "slash", 1.8f, 0.6f, 0.6f, 0.3f, S::small, {"sharp", "improvised"}, steel, wood);
    blade("butterfly_knife", "Butterfly knife", MS, "knife", 16.0f, "slash", 2.8f, 0.5f, 0.15f, 0.22f, S::tiny, {"sharp", "modern"}, steel, steel);
    blade("switchblade", "Switchblade", MS, "knife", 16.0f, "pierce", 2.8f, 0.5f, 0.12f, 0.22f, S::tiny, {"sharp", "modern"}, steel, black);
    blade("survival_knife", "Survival knife", MS, "knife", 22.0f, "slash", 2.2f, 0.6f, 0.4f, 0.32f, S::small, {"sharp", "modern"}, steel, olive);
    blade("chainsaw", "Chainsaw", MS, "chainsaw", 12.0f, "slash", 12.0f, 0.9f, 5.5f, 0.8f, S::large, {"sharp", "powered"}, Vec3{0.9f, 0.45f, 0.1f}, black);
    L.v.back().effect = "bleeding";
    L.v.back().effect_time = 6.0f;
    L.v.back().effect_chance = 0.5f;
    blade("katana_replica", "Display katana", MS, "katana", 30.0f, "slash", 1.4f, 1.2f, 1.0f, 1.0f, S::large, {"sharp", "improvised"}, steel, black);
    blade("sickle", "Sickle", MS, "sickle", 24.0f, "slash", 1.8f, 0.6f, 0.4f, 0.45f, S::small, {"sharp", "farm"}, iron, wood);
    blade("scythe", "Scythe", MS, "scythe", 40.0f, "slash", 0.8f, 1.9f, 2.5f, 1.7f, S::huge, {"sharp", "farm"}, iron, wood);
    blade("pitchfork", "Pitchfork", MS, "pitchfork", 30.0f, "pierce", 1.0f, 1.8f, 2.0f, 1.6f, S::huge, {"pierce", "farm"}, iron, wood);
    blade("pickaxe", "Pickaxe", MS, "pickaxe", 40.0f, "pierce", 0.9f, 1.0f, 2.7f, 0.9f, S::large, {"pierce", "tool"}, iron, wood);
    blade("broken_bottle", "Broken bottle", MS, "bottle", 12.0f, "slash", 2.4f, 0.45f, 0.3f, 0.25f, S::small, {"sharp", "improvised"}, Vec3{0.25f, 0.5f, 0.25f}, glass);
    L.v.back().effect = "bleeding";
    L.v.back().effect_time = 4.0f;
    L.v.back().effect_chance = 0.4f;

    const char* MB = "Melee/Modern blunt";
    blade("baseball_bat", "Baseball bat", MB, "bat", 34.0f, "blunt", 1.3f, 1.0f, 0.9f, 0.85f, S::medium, {"blunt", "sports"}, wood, wood);
    blade("aluminum_bat", "Aluminium bat", MB, "bat", 32.0f, "blunt", 1.4f, 1.0f, 0.8f, 0.85f, S::medium, {"blunt", "sports"}, steel, black);
    blade("spiked_bat", "Nail bat", MB, "bat", 40.0f, "blunt", 1.2f, 1.0f, 1.0f, 0.85f, S::medium, {"blunt", "spiked", "improvised"}, wood, iron);
    L.v.back().effect = "bleeding";
    L.v.back().effect_time = 5.0f;
    L.v.back().effect_chance = 0.4f;
    blade("crowbar", "Crowbar", MB, "crowbar", 32.0f, "blunt", 1.3f, 0.8f, 1.6f, 0.6f, S::medium, {"blunt", "tool"}, red, red);
    blade("lead_pipe", "Lead pipe", MB, "pipe", 32.0f, "blunt", 1.2f, 0.8f, 1.8f, 0.6f, S::medium, {"blunt", "improvised"}, iron, iron);
    blade("pipe_wrench", "Pipe wrench", MB, "wrench", 34.0f, "blunt", 1.2f, 0.7f, 1.6f, 0.45f, S::medium, {"blunt", "tool"}, red, iron);
    blade("sledgehammer", "Sledgehammer", MB, "sledgehammer", 58.0f, "blunt", 0.6f, 1.1f, 4.5f, 0.9f, S::large, {"blunt", "tool"}, iron, wood);
    blade("claw_hammer", "Claw hammer", MB, "hammer", 26.0f, "blunt", 1.8f, 0.6f, 0.6f, 0.33f, S::small, {"blunt", "tool"}, steel, wood);
    blade("frying_pan", "Frying pan", MB, "pan", 26.0f, "blunt", 1.4f, 0.65f, 1.4f, 0.45f, S::medium, {"blunt", "improvised"}, black, black);
    blade("golf_club", "Golf club", MB, "golf_club", 28.0f, "blunt", 1.4f, 1.1f, 0.4f, 1.0f, S::large, {"blunt", "sports"}, steel, black);
    blade("hockey_stick", "Hockey stick", MB, "hockey_stick", 24.0f, "blunt", 1.4f, 1.2f, 0.6f, 1.5f, S::large, {"blunt", "sports"}, wood, black);
    blade("cricket_bat", "Cricket bat", MB, "cricket_bat", 30.0f, "blunt", 1.3f, 0.95f, 1.2f, 0.95f, S::medium, {"blunt", "sports"}, tan, black);
    blade("police_baton", "Police baton", MB, "baton", 22.0f, "blunt", 2.0f, 0.7f, 0.5f, 0.6f, S::small, {"blunt", "police"}, black, black);
    blade("tonfa", "Tonfa", MB, "tonfa", 22.0f, "blunt", 2.0f, 0.65f, 0.6f, 0.55f, S::small, {"blunt", "police"}, black, black);
    blade("tire_iron", "Tire iron", MB, "crowbar", 28.0f, "blunt", 1.5f, 0.7f, 1.2f, 0.5f, S::medium, {"blunt", "tool"}, gunmetal, gunmetal);
    blade("shovel", "Shovel", MB, "shovel", 34.0f, "blunt", 1.0f, 1.3f, 1.8f, 1.2f, S::large, {"blunt", "sharp", "tool"}, iron, wood);
    blade("entrenching_tool", "Entrenching tool", MB, "shovel", 32.0f, "slash", 1.4f, 0.8f, 1.0f, 0.6f, S::medium, {"blunt", "sharp", "military"}, olive, wood);
    blade("brass_knuckles", "Brass knuckles", MB, "knuckles", 16.0f, "blunt", 2.8f, 0.45f, 0.25f, 0.12f, S::tiny, {"blunt", "fist"}, bronze, bronze);
    blade("rolling_pin", "Rolling pin", MB, "baton", 18.0f, "blunt", 1.8f, 0.6f, 0.7f, 0.45f, S::small, {"blunt", "improvised"}, wood, wood);
    blade("fire_extinguisher", "Fire extinguisher", MB, "extinguisher", 30.0f, "blunt", 0.9f, 0.6f, 5.0f, 0.5f, S::medium, {"blunt", "improvised"}, red, black);
    blade("nightstick", "Nightstick", MB, "baton", 20.0f, "blunt", 2.0f, 0.7f, 0.5f, 0.6f, S::small, {"blunt", "police"}, dark_wood, dark_wood);
    blade("rebar", "Rebar", MB, "pipe", 28.0f, "blunt", 1.2f, 0.9f, 1.4f, 0.9f, S::medium, {"blunt", "improvised"}, Vec3{0.45f, 0.3f, 0.2f}, Vec3{0.45f, 0.3f, 0.2f});
    blade("plank", "Plank", MB, "plank", 20.0f, "blunt", 1.3f, 1.0f, 1.2f, 1.0f, S::medium, {"blunt", "improvised"}, wood, wood);
    blade("guitar", "Electric guitar", MB, "guitar", 26.0f, "blunt", 1.0f, 1.0f, 3.5f, 1.0f, S::large, {"blunt", "improvised"}, red, black);
    ItemDef& riot = blade("riot_shield", "Riot shield", MB, "shield_tower", 8.0f, "blunt", 1.0f, 0.7f, 4.0f, 0.9f, S::huge, {"shield", "police", "offhand"}, Vec3{0.75f, 0.8f, 0.85f}, black).get();
    riot.block = 0.9f;
    ItemDef& ballistic_shield = blade("ballistic_shield", "Ballistic shield", MB, "shield_tower", 8.0f, "blunt", 0.8f, 0.7f, 10.0f, 0.9f, S::huge, {"shield", "military", "offhand"}, black, black).get();
    ballistic_shield.block = 0.98f;

    const char* F = "Melee/Fantasy";
    blade("greatsword", "Greatsword", F, "greatsword", 64.0f, "slash", 0.7f, 1.9f, 3.0f, 1.6f, S::huge, {"sharp", "fantasy"});
    blade("great_axe", "Great axe", F, "greataxe", 66.0f, "slash", 0.65f, 1.7f, 3.5f, 1.5f, S::huge, {"sharp", "fantasy", "axe"});
    blade("war_maul", "War maul", F, "sledgehammer", 70.0f, "blunt", 0.55f, 1.5f, 5.0f, 1.3f, S::huge, {"blunt", "fantasy"});
    blade("reaper_scythe", "Reaper's scythe", F, "scythe", 56.0f, "slash", 0.8f, 2.0f, 2.5f, 1.9f, S::huge, {"sharp", "fantasy"}, black, black);
    blade("elven_blade", "Elven blade", F, "sword", 44.0f, "slash", 1.6f, 1.2f, 0.9f, 0.9f, S::medium, {"sharp", "fantasy"}, Vec3{0.8f, 0.85f, 0.9f}, gold);
    blade("dwarven_axe", "Dwarven axe", F, "axe", 52.0f, "slash", 1.0f, 1.0f, 2.2f, 0.8f, S::medium, {"sharp", "fantasy", "axe"}, iron, gold);
    blade("frost_brand", "Frostbrand", F, "longsword", 46.0f, "cold", 1.2f, 1.4f, 1.4f, 1.1f, S::large, {"sharp", "fantasy", "enchanted"}, Vec3{0.6f, 0.8f, 1.0f}, steel);
    L.v.back().effect = "chilled";
    L.v.back().effect_time = 5.0f;
    L.v.back().effect_chance = 1.0f;
    blade("flame_tongue", "Flametongue", F, "sword", 44.0f, "fire", 1.3f, 1.2f, 1.2f, 0.95f, S::medium, {"sharp", "fantasy", "enchanted"}, Vec3{0.9f, 0.4f, 0.1f}, black);
    L.v.back().effect = "burning";
    L.v.back().effect_time = 4.0f;
    L.v.back().effect_chance = 0.5f;
    blade("vampire_dagger", "Vampiric dagger", F, "dagger", 22.0f, "magic", 2.4f, 0.6f, 0.4f, 0.35f, S::small, {"sharp", "fantasy", "enchanted"}, Vec3{0.5f, 0.05f, 0.1f}, black);
    blade("fists", "Fists", "Melee/Unarmed", "fist", 8.0f, "blunt", 2.5f, 0.6f, 0.0f, 0.1f, S::tiny, {"blunt", "fist"}, Vec3{0.85f, 0.7f, 0.6f}, Vec3{0.85f, 0.7f, 0.6f});
}

void ranged_primitive(Library& L) {
    L.bow("shortbow", "Shortbow", "Bows/Bows", "bow", 30.0f, 50.0f, 0.6f, "arrow", 0.6f, 1.1f, S::large);
    L.bow("longbow", "English longbow", "Bows/Bows", "longbow", 45.0f, 60.0f, 1.0f, "arrow", 1.0f, 1.9f, S::huge);
    L.bow("recurve", "Recurve bow", "Bows/Bows", "bow", 38.0f, 62.0f, 0.8f, "arrow", 1.2f, 1.5f, S::large);
    L.bow("composite_bow", "Mongol composite bow", "Bows/Bows", "bow", 42.0f, 70.0f, 0.8f, "arrow", 0.9f, 1.2f, S::large);
    L.bow("compound_bow", "Compound bow", "Bows/Bows", "compound_bow", 48.0f, 95.0f, 0.7f, "arrow_carbon", 1.9f, 0.95f, S::large).color = black;
    L.bow("yumi", "Yumi", "Bows/Bows", "longbow", 44.0f, 58.0f, 1.0f, "arrow", 0.7f, 2.2f, S::huge);
    L.bow("crossbow", "Crossbow", "Bows/Crossbows", "crossbow", 55.0f, 80.0f, 2.5f, "bolt", 3.5f, 0.8f, S::large);
    L.bow("arbalest", "Arbalest", "Bows/Crossbows", "crossbow", 80.0f, 95.0f, 6.0f, "bolt_heavy", 8.0f, 1.0f, S::huge);
    L.bow("hand_crossbow", "Hand crossbow", "Bows/Crossbows", "hand_crossbow", 28.0f, 55.0f, 1.5f, "bolt", 1.2f, 0.4f, S::small).two_handed = false;
    L.bow("repeating_crossbow", "Chu-ko-nu", "Bows/Crossbows", "crossbow", 22.0f, 45.0f, 0.4f, "bolt", 2.5f, 0.75f, S::large).magazine = 10;
    L.bow("sling", "Sling", "Bows/Slings", "sling", 18.0f, 30.0f, 0.8f, "sling_stone", 0.1f, 0.6f, S::small).damage_type = "blunt";
    L.v.back().two_handed = false;
    L.bow("balearic_sling", "Balearic sling", "Bows/Slings", "sling", 24.0f, 40.0f, 1.0f, "lead_bullet", 0.1f, 0.9f, S::small).damage_type = "blunt";
    L.bow("slingshot", "Slingshot", "Bows/Slings", "slingshot", 10.0f, 60.0f, 0.4f, "steel_ball", 0.2f, 0.2f, S::small).damage_type = "blunt";
    L.bow("blowgun", "Blowgun", "Bows/Slings", "blowgun", 6.0f, 40.0f, 0.3f, "dart", 0.3f, 1.2f, S::medium).effect = "poison";
    L.v.back().effect_time = 8.0f;
    L.v.back().effect_chance = 1.0f;
    L.v.back().damage_type = "pierce";
}

void throwables(Library& L) {
    L.throwable("frag_grenade", "Frag grenade (M67)", "Throwables/Grenades", "grenade", 120.0f, "explosion", 5.0f, 4.0f, 15.0f, 0.4f, 4);
    L.throwable("stick_grenade", "Stick grenade", "Throwables/Grenades", "stick_grenade", 110.0f, "explosion", 4.5f, 4.5f, 17.0f, 0.6f, 4).color = dark_wood;
    L.throwable("impact_grenade", "Impact grenade", "Throwables/Grenades", "grenade", 90.0f, "explosion", 4.0f, 0.0f, 15.0f, 0.35f, 4);
    L.throwable("sticky_bomb", "Sticky bomb", "Throwables/Grenades", "box", 140.0f, "explosion", 5.0f, 3.0f, 12.0f, 0.8f, 3).tags.push_back("sticky");
    L.throwable("dynamite", "Dynamite", "Throwables/Explosives", "dynamite", 150.0f, "explosion", 6.0f, 5.0f, 14.0f, 0.2f, 6).color = red;
    L.throwable("pipe_bomb", "Pipe bomb", "Throwables/Explosives", "pipe", 100.0f, "explosion", 4.5f, 4.0f, 13.0f, 0.6f, 3).color = iron;
    L.throwable("c4", "C4 charge", "Throwables/Explosives", "box", 250.0f, "explosion", 8.0f, -1.0f, 8.0f, 0.6f, 3).tags.push_back("remote");
    L.throwable("molotov", "Molotov cocktail", "Throwables/Incendiary", "bottle", 8.0f, "fire", 3.5f, 0.0f, 13.0f, 0.7f, 3).effect = "burning";
    L.v.back().effect_time = 8.0f;
    L.v.back().effect_chance = 1.0f;
    L.v.back().color = Vec3{0.45f, 0.6f, 0.3f};
    L.throwable("thermite", "Thermite grenade", "Throwables/Incendiary", "grenade", 30.0f, "fire", 2.0f, 2.0f, 13.0f, 0.5f, 3).effect = "burning";
    L.v.back().effect_time = 10.0f;
    L.v.back().effect_chance = 1.0f;
    L.throwable("smoke_grenade", "Smoke grenade", "Throwables/Utility", "can", 0.0f, "physical", 6.0f, 1.5f, 14.0f, 0.5f, 4).tags.push_back("smoke");
    L.throwable("flashbang", "Flashbang", "Throwables/Utility", "can", 1.0f, "physical", 6.0f, 1.5f, 15.0f, 0.4f, 4).effect = "stunned";
    L.v.back().effect_time = 3.0f;
    L.v.back().effect_chance = 1.0f;
    L.throwable("tear_gas", "Tear gas", "Throwables/Utility", "can", 2.0f, "poison", 6.0f, 1.5f, 14.0f, 0.5f, 4).effect = "weakness";
    L.v.back().effect_time = 10.0f;
    L.v.back().effect_chance = 1.0f;
    L.throwable("decoy", "Decoy", "Throwables/Utility", "can", 0.0f, "physical", 0.0f, 2.0f, 14.0f, 0.3f, 4).tags.push_back("noise");
    L.throwable("throwing_knife", "Throwing knife", "Throwables/Blades", "knife", 26.0f, "pierce", 0.0f, 0.0f, 22.0f, 0.15f, 10).color = steel;
    L.throwable("throwing_axe", "Throwing axe (francisca)", "Throwables/Blades", "hatchet", 38.0f, "slash", 0.0f, 0.0f, 16.0f, 0.6f, 4).color = iron;
    L.throwable("shuriken", "Shuriken", "Throwables/Blades", "shuriken", 14.0f, "slash", 0.0f, 0.0f, 25.0f, 0.05f, 20).color = black;
    L.throwable("kunai", "Kunai", "Throwables/Blades", "dagger", 18.0f, "pierce", 0.0f, 0.0f, 22.0f, 0.12f, 10).color = black;
    L.throwable("javelin", "Javelin", "Throwables/Spears", "spear", 45.0f, "pierce", 0.0f, 0.0f, 25.0f, 0.8f, 3).color = wood;
    L.v.back().size = S::large;
    L.v.back().grid_w = 5;
    L.throwable("pilum", "Pilum", "Throwables/Spears", "spear", 50.0f, "pierce", 0.0f, 0.0f, 22.0f, 2.0f, 2).color = iron;
    L.v.back().size = S::large;
    L.v.back().grid_w = 5;
    L.v.back().tags.push_back("roman");
    L.throwable("plumbata", "Plumbata", "Throwables/Spears", "arrow", 22.0f, "pierce", 0.0f, 0.0f, 25.0f, 0.3f, 6).tags.push_back("roman");
    L.throwable("rock", "Rock", "Throwables/Improvised", "rock", 12.0f, "blunt", 0.0f, 0.0f, 18.0f, 0.5f, 5).color = Vec3{0.5f, 0.48f, 0.45f};
    L.throwable("brick", "Brick", "Throwables/Improvised", "box", 16.0f, "blunt", 0.0f, 0.0f, 15.0f, 2.0f, 3).color = Vec3{0.65f, 0.3f, 0.2f};
    L.throwable("bottle_throw", "Glass bottle", "Throwables/Improvised", "bottle", 8.0f, "blunt", 0.0f, 0.0f, 15.0f, 0.4f, 3).color = Vec3{0.25f, 0.5f, 0.25f};
    L.throwable("bola", "Bola", "Throwables/Improvised", "bola", 6.0f, "blunt", 0.0f, 0.0f, 18.0f, 0.4f, 3).effect = "slowed";
    L.v.back().effect_time = 4.0f;
    L.v.back().effect_chance = 1.0f;
}

void magic_items(Library& L) {
    const Vec3 fire{0.95f, 0.4f, 0.1f};
    const Vec3 frost{0.55f, 0.8f, 1.0f};
    const Vec3 storm{0.6f, 0.6f, 1.0f};
    const Vec3 life{0.4f, 0.95f, 0.45f};
    const Vec3 dark{0.35f, 0.1f, 0.45f};
    L.magic("staff_fire", "Staff of fire", "Magic/Staves", "staff", "projectile", 40.0f, "fire", 15.0f, 1.2f, "burning", 5.0f, fire, 2.0f).blast_radius = 2.5f;
    L.magic("staff_frost", "Staff of frost", "Magic/Staves", "staff", "projectile", 30.0f, "cold", 14.0f, 1.3f, "frozen", 3.0f, frost, 2.0f);
    L.magic("staff_lightning", "Staff of storms", "Magic/Staves", "staff", "bolt", 35.0f, "shock", 18.0f, 1.0f, "shocked", 2.0f, storm, 2.0f);
    L.magic("staff_healing", "Staff of healing", "Magic/Staves", "staff", "self", -40.0f, "magic", 25.0f, 0.5f, "regeneration", 8.0f, life, 1.8f);
    L.magic("staff_earth", "Staff of earth", "Magic/Staves", "staff", "area", 30.0f, "physical", 30.0f, 0.4f, "slowed", 4.0f, Vec3{0.5f, 0.4f, 0.25f}, 2.5f);
    L.magic("staff_necro", "Necromancer's staff", "Magic/Staves", "staff", "bolt", 30.0f, "magic", 20.0f, 1.0f, "weakness", 10.0f, dark, 2.0f);
    L.magic("wand_sparks", "Wand of sparks", "Magic/Wands", "wand", "bolt", 12.0f, "shock", 4.0f, 3.0f, "", 0.0f, storm, 0.2f);
    L.magic("wand_frost", "Wand of frost", "Magic/Wands", "wand", "projectile", 14.0f, "cold", 5.0f, 2.5f, "chilled", 4.0f, frost, 0.2f);
    L.magic("wand_fire", "Wand of embers", "Magic/Wands", "wand", "projectile", 16.0f, "fire", 6.0f, 2.5f, "burning", 2.0f, fire, 0.2f);
    L.magic("wand_poison", "Wand of venom", "Magic/Wands", "wand", "projectile", 8.0f, "poison", 6.0f, 2.0f, "poison", 8.0f, Vec3{0.4f, 0.8f, 0.2f}, 0.2f);
    L.magic("wand_slow", "Wand of slowness", "Magic/Wands", "wand", "bolt", 0.0f, "magic", 8.0f, 1.0f, "slowed", 6.0f, dark, 0.2f);
    L.magic("tome_fireball", "Tome of fireball", "Magic/Tomes", "tome", "projectile", 60.0f, "fire", 30.0f, 0.6f, "burning", 5.0f, fire, 1.5f).blast_radius = 4.0f;
    L.magic("tome_chain_lightning", "Tome of chain lightning", "Magic/Tomes", "tome", "bolt", 45.0f, "shock", 30.0f, 0.6f, "shocked", 3.0f, storm, 1.5f).tags.push_back("chain");
    L.magic("tome_frost_nova", "Tome of frost nova", "Magic/Tomes", "tome", "area", 25.0f, "cold", 35.0f, 0.4f, "frozen", 4.0f, frost, 1.5f);
    L.magic("tome_poison_cloud", "Tome of poison cloud", "Magic/Tomes", "tome", "area", 5.0f, "poison", 30.0f, 0.4f, "poison", 10.0f, Vec3{0.4f, 0.8f, 0.2f}, 1.5f);
    L.magic("tome_haste", "Tome of haste", "Magic/Tomes", "tome", "self", 0.0f, "magic", 25.0f, 0.3f, "haste", 15.0f, Vec3{0.9f, 0.9f, 0.3f}, 1.5f);
    L.magic("tome_shield", "Tome of warding", "Magic/Tomes", "tome", "self", 0.0f, "magic", 25.0f, 0.3f, "resistance", 20.0f, Vec3{0.8f, 0.8f, 0.95f}, 1.5f);
    L.magic("tome_strength", "Tome of might", "Magic/Tomes", "tome", "self", 0.0f, "magic", 25.0f, 0.3f, "strength", 20.0f, red, 1.5f);
    L.magic("orb_storm", "Storm orb", "Magic/Orbs", "orb", "bolt", 28.0f, "shock", 12.0f, 1.5f, "shocked", 2.0f, storm, 1.0f);
    L.magic("orb_void", "Void orb", "Magic/Orbs", "orb", "projectile", 40.0f, "magic", 18.0f, 1.0f, "vulnerable", 6.0f, dark, 1.0f);
    L.magic("orb_life", "Life orb", "Magic/Orbs", "orb", "self", -25.0f, "magic", 15.0f, 0.8f, "", 0.0f, life, 1.0f);
    auto scroll = [&](const char* id, const char* name, const char* spell, float dmg, const char* type, const char* effect, float t, Vec3 c) {
        ItemDef& d = L.magic(id, name, "Magic/Scrolls", "scroll", spell, dmg, type, 0.0f, 1.0f, effect, t, c, 0.05f);
        d.max_stack = 10.0f;
        d.tags.push_back("consumed");
        d.size = S::tiny;
    };
    scroll("scroll_fireball", "Scroll of fireball", "projectile", 60.0f, "fire", "burning", 5.0f, fire);
    scroll("scroll_lightning", "Scroll of lightning", "bolt", 50.0f, "shock", "shocked", 3.0f, storm);
    scroll("scroll_frost", "Scroll of frost nova", "area", 25.0f, "cold", "frozen", 4.0f, frost);
    scroll("scroll_heal", "Scroll of healing", "self", -50.0f, "magic", "regeneration", 6.0f, life);
    scroll("scroll_haste", "Scroll of haste", "self", 0.0f, "magic", "haste", 20.0f, Vec3{0.9f, 0.9f, 0.3f});
}

void consumables(Library& L) {
    L.potion("potion_health_small", "Small health potion", "", 0.0f, 1.0f, 25.0f, Vec3{0.85f, 0.1f, 0.15f});
    L.potion("potion_health", "Health potion", "", 0.0f, 1.0f, 50.0f, Vec3{0.85f, 0.1f, 0.15f});
    L.potion("potion_health_large", "Large health potion", "", 0.0f, 1.0f, 100.0f, Vec3{0.85f, 0.1f, 0.15f});
    L.potion("potion_regeneration", "Potion of regeneration", "regeneration", 20.0f, 1.0f, 0.0f, Vec3{0.95f, 0.4f, 0.6f});
    L.potion("potion_mana", "Mana potion", "", 0.0f, 1.0f, 0.0f, Vec3{0.15f, 0.3f, 0.9f}).mana_restore = 50.0f;
    L.potion("potion_stamina", "Stamina potion", "", 0.0f, 1.0f, 0.0f, Vec3{0.2f, 0.8f, 0.3f}).tags.push_back("stamina");
    L.potion("potion_strength", "Potion of strength", "strength", 60.0f, 1.0f, 0.0f, Vec3{0.6f, 0.15f, 0.05f});
    L.potion("potion_swiftness", "Potion of swiftness", "haste", 60.0f, 1.0f, 0.0f, Vec3{0.5f, 0.85f, 0.95f});
    L.potion("potion_resistance", "Potion of resistance", "resistance", 60.0f, 1.0f, 0.0f, Vec3{0.6f, 0.6f, 0.65f});
    L.potion("potion_fire_resist", "Potion of fire resistance", "fire_resistance", 120.0f, 1.0f, 0.0f, Vec3{0.95f, 0.55f, 0.1f});
    L.potion("potion_warmth", "Potion of warmth", "warm", 120.0f, 1.0f, 0.0f, Vec3{0.95f, 0.6f, 0.2f});
    L.potion("antidote", "Antidote", "", 0.0f, 1.0f, 5.0f, Vec3{0.5f, 0.9f, 0.5f}, "poison,food_poisoning");
    L.potion("elixir", "Elixir", "regeneration", 30.0f, 2.0f, 100.0f, gold, "poison,bleeding,burning,frozen");
    L.potion("potion_poison", "Vial of poison", "poison", 15.0f, 2.0f, 0.0f, Vec3{0.3f, 0.7f, 0.1f}).tags.push_back("throwable");
    L.potion("potion_weakness", "Potion of weakness", "weakness", 30.0f, 1.0f, 0.0f, Vec3{0.4f, 0.4f, 0.4f}).tags.push_back("throwable");
    L.potion("potion_frost", "Potion of frost", "frozen", 6.0f, 1.0f, 0.0f, Vec3{0.6f, 0.85f, 1.0f}).tags.push_back("throwable");

    L.food("apple", "Apple", "apple", 4.0f, 2.4f, 0.0f, 0.18f, 16, Vec3{0.8f, 0.1f, 0.1f});
    L.food("bread", "Bread", "bread", 5.0f, 6.0f, 0.0f, 0.4f, 16, Vec3{0.8f, 0.6f, 0.3f});
    L.food("cheese", "Cheese", "wedge", 4.0f, 4.0f, 0.0f, 0.3f, 16, Vec3{0.95f, 0.8f, 0.3f});
    L.food("cooked_meat", "Cooked meat", "meat", 8.0f, 12.8f, 0.0f, 0.3f, 16, Vec3{0.5f, 0.28f, 0.15f});
    L.food("raw_meat", "Raw meat", "meat", 3.0f, 1.8f, 0.0f, 0.3f, 16, Vec3{0.85f, 0.3f, 0.3f}).use_effect = "food_poisoning";
    L.v.back().use_time = 15.0f;
    L.v.back().effect_chance = 0.3f;
    L.food("canned_beans", "Canned beans", "can", 6.0f, 6.0f, 0.0f, 0.45f, 8, Vec3{0.7f, 0.7f, 0.72f});
    L.food("mre", "MRE", "box", 10.0f, 12.0f, 0.0f, 0.65f, 4, olive);
    L.food("chocolate", "Chocolate bar", "bar", 3.0f, 2.0f, 0.0f, 0.1f, 16, Vec3{0.35f, 0.2f, 0.1f});
    L.food("energy_bar", "Energy bar", "bar", 4.0f, 4.0f, 0.0f, 0.08f, 16, Vec3{0.8f, 0.6f, 0.2f}).tags.push_back("stamina");
    L.food("berries", "Berries", "berries", 2.0f, 1.0f, 0.5f, 0.1f, 32, Vec3{0.4f, 0.1f, 0.5f});
    L.food("cooked_fish", "Cooked fish", "fish", 6.0f, 9.6f, 0.0f, 0.3f, 16, Vec3{0.75f, 0.55f, 0.3f});
    L.food("soup", "Soup", "bowl", 6.0f, 7.2f, 5.0f, 0.5f, 1, Vec3{0.7f, 0.4f, 0.2f});
    L.food("rice", "Rice ball", "rice", 4.0f, 3.0f, 0.0f, 0.15f, 16, Vec3{0.95f, 0.95f, 0.9f});
    L.food("hardtack", "Hardtack", "bar", 3.0f, 5.0f, -2.0f, 0.1f, 32, Vec3{0.85f, 0.75f, 0.55f});
    L.food("carrot", "Carrot", "carrot", 3.0f, 3.6f, 0.0f, 0.07f, 32, Vec3{0.95f, 0.5f, 0.1f});
    L.food("golden_apple", "Golden apple", "apple", 4.0f, 9.6f, 0.0f, 0.2f, 16, gold).use_effect = "regeneration";
    L.v.back().use_time = 5.0f;
    L.v.back().use_strength = 2.0f;

    ItemDef& water = L.drink("water", "Water", "bottle", 100.0f, 1.0f, Vec3{0.6f, 0.8f, 0.95f});
    water.continuous = true;
    water.unit = "L";
    water.max_stack = 1.5f;
    water.water = 100.0f;
    ItemDef& dirty = L.drink("dirty_water", "Dirty water", "bottle", 100.0f, 1.0f, Vec3{0.5f, 0.45f, 0.3f});
    dirty.continuous = true;
    dirty.unit = "L";
    dirty.max_stack = 1.5f;
    dirty.use_effect = "food_poisoning";
    dirty.use_time = 20.0f;
    dirty.effect_chance = 0.5f;
    L.drink("soda", "Soda", "can", 30.0f, 0.36f, red).tags.push_back("stamina");
    L.drink("coffee", "Coffee", "cup", 15.0f, 0.3f, Vec3{0.3f, 0.2f, 0.1f}).tags.push_back("stamina");
    L.drink("energy_drink", "Energy drink", "can", 25.0f, 0.5f, Vec3{0.2f, 0.9f, 0.3f}).use_effect = "haste";
    L.v.back().use_time = 20.0f;
    L.drink("milk", "Milk", "carton", 35.0f, 1.0f, Vec3{0.95f, 0.95f, 0.95f}).cures = "poison";
    L.drink("juice", "Juice", "carton", 35.0f, 1.0f, Vec3{0.95f, 0.6f, 0.1f});
    L.drink("tea", "Hot tea", "cup", 20.0f, 0.3f, Vec3{0.6f, 0.4f, 0.2f}).use_effect = "warm";
    L.v.back().use_time = 60.0f;
    L.drink("wine", "Wine", "bottle", 15.0f, 0.75f, Vec3{0.45f, 0.05f, 0.1f}).use_effect = "warm";
    L.v.back().use_time = 30.0f;
    L.drink("beer", "Beer", "bottle", 20.0f, 0.5f, Vec3{0.75f, 0.55f, 0.15f});
    ItemDef& canteen = L.drink("canteen", "Canteen", "canteen", 0.0f, 0.4f, olive);
    canteen.kind = K::container;
    canteen.capacity = 1.0f;
    canteen.holster_kinds = "water,dirty_water";
    canteen.max_stack = 1.0f;

    L.medical("bandage", "Bandage", "bandage", 5.0f, "bleeding", "", 0.0f, 0.05f, 10);
    L.medical("tourniquet", "Tourniquet", "bandage", 0.0f, "bleeding", "slowed", 30.0f, 0.1f, 4);
    L.medical("splint", "Splint", "plank", 5.0f, "slowed", "", 0.0f, 0.2f, 4);
    L.medical("painkillers", "Painkillers", "pills", 15.0f, "", "resistance", 30.0f, 0.05f, 10);
    L.medical("morphine", "Morphine", "syringe", 35.0f, "", "resistance", 60.0f, 0.03f, 4);
    L.medical("medkit", "Medkit", "medkit", 60.0f, "bleeding,poison", "", 0.0f, 1.0f, 2);
    L.medical("ifak", "IFAK", "pouch", 40.0f, "bleeding", "", 0.0f, 0.5f, 2).color = olive;
    L.medical("antibiotics", "Antibiotics", "pills", 5.0f, "food_poisoning", "", 0.0f, 0.05f, 10);
    L.medical("adrenaline", "Adrenaline shot", "syringe", 10.0f, "stunned,staggered", "haste", 20.0f, 0.03f, 4);
    L.medical("stim", "Combat stim", "syringe", 20.0f, "", "strength", 30.0f, 0.03f, 4);
    L.medical("burn_cream", "Burn cream", "tube", 10.0f, "burning", "", 0.0f, 0.1f, 4);
    L.medical("herbs", "Healing herbs", "berries", 8.0f, "", "regeneration", 10.0f, 0.05f, 16).color = Vec3{0.3f, 0.6f, 0.2f};
}

void wearables(Library& L) {
    using KA = ItemKind;
    const char* C = "Clothing/Modern";
    L.wear("tshirt", "T-shirt", C, "torso", "shirt", 0.0f, 1.0f, 0.2f, Vec3{0.85f, 0.85f, 0.85f});
    L.wear("shirt", "Button-up shirt", C, "torso", "shirt", 0.0f, 2.0f, 0.3f, Vec3{0.5f, 0.6f, 0.75f});
    L.wear("hoodie", "Hoodie", C, "torso", "jacket", 0.0f, 5.0f, 0.6f, Vec3{0.3f, 0.3f, 0.35f});
    L.wear("jacket", "Leather jacket", C, "torso", "jacket", 2.0f, 7.0f, 1.5f, black);
    L.wear("winter_coat", "Winter coat", C, "torso", "coat", 2.0f, 15.0f, 2.0f, Vec3{0.2f, 0.25f, 0.4f});
    L.wear("raincoat", "Raincoat", C, "torso", "coat", 0.0f, 4.0f, 0.8f, Vec3{0.9f, 0.8f, 0.1f}).tags.push_back("waterproof");
    L.wear("jeans", "Jeans", C, "legs", "pants", 1.0f, 3.0f, 0.7f, Vec3{0.2f, 0.3f, 0.5f});
    L.wear("cargo_pants", "Cargo pants", C, "legs", "pants", 1.0f, 3.0f, 0.8f, olive).carry_bonus = 3.0f;
    L.wear("shorts", "Shorts", C, "legs", "pants", 0.0f, 0.5f, 0.3f, tan);
    L.wear("boots", "Boots", C, "feet", "boots", 2.0f, 2.0f, 1.4f, leather);
    L.wear("sneakers", "Sneakers", C, "feet", "boots", 0.0f, 1.0f, 0.7f, Vec3{0.9f, 0.9f, 0.9f});
    L.wear("winter_boots", "Winter boots", C, "feet", "boots", 2.0f, 6.0f, 1.8f, Vec3{0.3f, 0.25f, 0.2f});
    L.wear("gloves", "Gloves", C, "hands", "gloves", 1.0f, 1.0f, 0.2f, black);
    L.wear("winter_gloves", "Winter gloves", C, "hands", "gloves", 0.0f, 4.0f, 0.3f, Vec3{0.2f, 0.2f, 0.25f});
    L.wear("beanie", "Beanie", C, "head", "beanie", 0.0f, 3.0f, 0.1f, red);
    L.wear("cap", "Baseball cap", C, "head", "cap", 0.0f, 0.5f, 0.1f, Vec3{0.15f, 0.25f, 0.5f});
    L.wear("bandana", "Bandana", C, "face", "mask", 0.0f, 0.5f, 0.05f, red);
    L.wear("sunglasses", "Sunglasses", C, "eyes", "glasses", 0.0f, 0.0f, 0.03f, black);
    L.wear("gas_mask", "Gas mask", C, "face", "gas_mask", 0.0f, 1.0f, 0.7f, black).tags.push_back("gas_proof");
    L.wear("balaclava", "Balaclava", C, "face", "mask", 0.0f, 2.0f, 0.1f, black);
    L.wear("scarf", "Scarf", C, "neck", "scarf", 0.0f, 3.0f, 0.2f, Vec3{0.6f, 0.1f, 0.1f});
    L.wear("ghillie", "Ghillie suit", C, "over", "ghillie", 0.0f, 4.0f, 2.5f, Vec3{0.3f, 0.35f, 0.15f}).tags.push_back("camouflage");
    const char* H = "Clothing/Historical";
    L.wear("roman_tunic", "Roman tunic", H, "torso", "tunic", 0.0f, 2.0f, 0.5f, red).tags.push_back("roman");
    L.wear("toga", "Toga", H, "over", "toga", 0.0f, 3.0f, 2.0f, Vec3{0.92f, 0.9f, 0.85f}).tags.push_back("roman");
    L.wear("caligae", "Caligae", H, "feet", "sandals", 1.0f, 0.5f, 0.9f, leather).tags.push_back("roman");
    L.wear("kimono", "Kimono", H, "torso", "robe", 0.0f, 3.0f, 1.0f, Vec3{0.2f, 0.2f, 0.45f}).tags.push_back("samurai");
    L.wear("hakama", "Hakama", H, "legs", "pants", 0.0f, 2.0f, 0.8f, black).tags.push_back("samurai");
    L.wear("cloak", "Cloak", H, "back", "cloak", 0.0f, 8.0f, 1.5f, Vec3{0.25f, 0.2f, 0.15f});
    L.wear("wizard_robe", "Wizard robe", H, "torso", "robe", 0.0f, 4.0f, 1.2f, Vec3{0.25f, 0.2f, 0.6f}).tags.push_back("magic");
    L.wear("wizard_hat", "Wizard hat", H, "head", "wizard_hat", 0.0f, 1.0f, 0.2f, Vec3{0.25f, 0.2f, 0.6f}).tags.push_back("magic");

    const char* AM = "Armor/Modern";
    L.wear("kevlar_vest", "Soft armor vest (IIIA)", AM, "vest", "vest", 30.0f, 2.0f, 2.5f, Vec3{0.15f, 0.15f, 0.15f}, KA::armor).armor_types = "projectile,slash,pierce";
    L.wear("plate_carrier", "Plate carrier (III)", AM, "vest", "plate_carrier", 55.0f, 2.0f, 7.0f, olive, KA::armor);
    L.v.back().slots_w = 4;
    L.v.back().slots_h = 2;
    L.wear("plate_carrier_iv", "Plate carrier (IV)", AM, "vest", "plate_carrier", 70.0f, 2.0f, 9.5f, tan, KA::armor);
    L.v.back().slots_w = 4;
    L.v.back().slots_h = 2;
    L.wear("combat_helmet", "Combat helmet", AM, "head", "helmet", 30.0f, 1.0f, 1.4f, olive, KA::armor);
    L.wear("ballistic_helmet", "Ballistic helmet (IIIA)", AM, "head", "helmet", 40.0f, 1.0f, 1.6f, black, KA::armor);
    L.wear("riot_helmet", "Riot helmet", AM, "head", "helmet_visor", 25.0f, 1.0f, 1.5f, Vec3{0.2f, 0.2f, 0.25f}, KA::armor);
    L.wear("knee_pads", "Knee pads", AM, "legs", "pads", 8.0f, 0.5f, 0.6f, black, KA::armor);
    L.wear("bomb_suit", "Bomb suit", AM, "over", "bomb_suit", 80.0f, 10.0f, 36.0f, olive, KA::armor).armor_types = "explosion,projectile,fire";
    const char* AR = "Armor/Roman";
    L.wear("galea", "Galea", AR, "head", "galea", 25.0f, 1.0f, 2.0f, iron, KA::armor).tags.push_back("roman");
    L.wear("lorica_segmentata", "Lorica segmentata", AR, "vest", "segmentata", 45.0f, 1.0f, 9.0f, iron, KA::armor).tags.push_back("roman");
    L.wear("lorica_hamata", "Lorica hamata", AR, "vest", "mail", 35.0f, 1.0f, 12.0f, iron, KA::armor).tags.push_back("roman");
    L.wear("lorica_squamata", "Lorica squamata", AR, "vest", "scale", 38.0f, 1.0f, 11.0f, bronze, KA::armor).tags.push_back("roman");
    L.wear("balteus", "Balteus", AR, "waist", "belt", 2.0f, 0.0f, 0.8f, leather, KA::armor).holster_kinds = "sword,dagger";
    L.v.back().holster_size = S::medium;
    L.v.back().holster_count = 2;
    L.wear("greaves_roman", "Ocrea (greaves)", AR, "legs", "greaves", 12.0f, 0.0f, 1.5f, bronze, KA::armor).tags.push_back("roman");
    const char* AJ = "Armor/Samurai";
    L.wear("kabuto", "Kabuto", AJ, "head", "kabuto", 28.0f, 1.0f, 2.5f, black, KA::armor).tags.push_back("samurai");
    L.wear("menpo", "Menpo", AJ, "face", "menpo", 10.0f, 0.5f, 0.8f, red, KA::armor).tags.push_back("samurai");
    L.wear("do", "Do (cuirass)", AJ, "vest", "do", 42.0f, 1.0f, 8.0f, black, KA::armor).tags.push_back("samurai");
    L.wear("kote", "Kote (sleeves)", AJ, "hands", "gloves", 12.0f, 0.5f, 1.2f, black, KA::armor).tags.push_back("samurai");
    L.wear("suneate", "Suneate (shin guards)", AJ, "legs", "greaves", 12.0f, 0.0f, 1.2f, black, KA::armor).tags.push_back("samurai");
    L.wear("sode", "Sode (shoulder guards)", AJ, "shoulders", "pauldrons", 10.0f, 0.0f, 1.4f, black, KA::armor).tags.push_back("samurai");
    const char* AD = "Armor/Medieval";
    L.wear("gambeson", "Gambeson", AD, "torso", "gambeson", 15.0f, 6.0f, 3.5f, Vec3{0.8f, 0.75f, 0.6f}, KA::armor);
    L.wear("chainmail", "Chainmail hauberk", AD, "vest", "mail", 35.0f, 0.5f, 12.0f, iron, KA::armor);
    L.wear("plate_cuirass", "Plate cuirass", AD, "vest", "cuirass", 50.0f, 0.5f, 10.0f, steel, KA::armor);
    L.wear("leather_armor", "Leather armor", AD, "vest", "vest", 18.0f, 2.0f, 5.0f, leather, KA::armor);
    L.wear("great_helm", "Great helm", AD, "head", "great_helm", 35.0f, 1.0f, 3.0f, steel, KA::armor);
    L.wear("bascinet", "Bascinet", AD, "head", "bascinet", 32.0f, 1.0f, 2.5f, steel, KA::armor);
    L.wear("kettle_hat", "Kettle hat", AD, "head", "kettle_hat", 22.0f, 1.0f, 1.8f, iron, KA::armor);
    L.wear("gauntlets", "Gauntlets", AD, "hands", "gauntlets", 12.0f, 0.5f, 1.5f, steel, KA::armor);
    L.wear("greaves", "Greaves", AD, "legs", "greaves", 15.0f, 0.0f, 2.5f, steel, KA::armor);
    L.wear("sabatons", "Sabatons", AD, "feet", "boots", 10.0f, 0.5f, 2.0f, steel, KA::armor);
    L.wear("pauldrons", "Pauldrons", AD, "shoulders", "pauldrons", 12.0f, 0.0f, 2.0f, steel, KA::armor);

    const char* T = "Trinkets";
    auto trinket = [&](const char* id, const char* name, const char* shape, const char* effect, float insulation, Vec3 c) {
        ItemDef& d = L.wear(id, name, T, shape == std::string("ring") ? "ring" : "neck", shape, 0.0f, insulation, 0.02f, c, KA::trinket);
        d.effect = effect;
        d.effect_time = -1.0f;
        d.size = S::tiny;
        d.grid_w = 1;
        d.grid_h = 1;
        d.max_stack = 1.0f;
    };
    trinket("ring_strength", "Ring of strength", "ring", "strength", 0.0f, gold);
    trinket("ring_haste", "Ring of swiftness", "ring", "haste", 0.0f, Vec3{0.6f, 0.85f, 0.95f});
    trinket("ring_regen", "Ring of regeneration", "ring", "regeneration", 0.0f, Vec3{0.4f, 0.9f, 0.4f});
    trinket("ring_warding", "Ring of warding", "ring", "resistance", 0.0f, Vec3{0.7f, 0.7f, 0.75f});
    trinket("amulet_warmth", "Amulet of warmth", "amulet", "", 15.0f, Vec3{0.95f, 0.5f, 0.1f});
    trinket("amulet_frost", "Frost amulet", "amulet", "", -10.0f, Vec3{0.6f, 0.85f, 1.0f});
    trinket("lucky_charm", "Lucky charm", "amulet", "", 0.0f, Vec3{0.3f, 0.7f, 0.3f});
    trinket("wedding_ring", "Wedding ring", "ring", "", 0.0f, gold);
    trinket("dog_tags", "Dog tags", "amulet", "", 0.0f, steel);
    trinket("talisman", "Talisman of purity", "amulet", "", 0.0f, Vec3{0.9f, 0.9f, 0.95f});
    L.v.back().tags.push_back("poison_ward");
    trinket("pocket_watch", "Pocket watch", "amulet", "", 0.0f, gold);
}

void storage(Library& L) {
    const char* B = "Containers/Backpacks";
    L.container("daypack", "Daypack", B, "back", "backpack", 4, 4, 20.0f, 0.6f, Vec3{0.2f, 0.35f, 0.55f});
    L.container("hiking_pack", "Hiking backpack", B, "back", "backpack_big", 5, 6, 45.0f, 1.8f, Vec3{0.75f, 0.35f, 0.1f});
    L.container("rucksack", "Military rucksack", B, "back", "backpack_big", 6, 7, 60.0f, 2.5f, olive);
    L.container("tactical_pack", "Tactical backpack", B, "back", "backpack", 5, 5, 35.0f, 1.4f, tan);
    L.container("school_bag", "School bag", B, "back", "backpack", 4, 3, 15.0f, 0.5f, red);
    L.container("roman_sarcina", "Sarcina (marching pack)", B, "back", "sarcina", 4, 4, 18.0f, 3.0f, leather).tags.push_back("roman");
    L.container("frame_pack", "Frame pack", B, "back", "backpack_big", 6, 6, 55.0f, 3.0f, Vec3{0.3f, 0.3f, 0.3f});
    const char* R = "Containers/Rigs & belts";
    L.container("chest_rig", "Chest rig", R, "vest", "rig", 4, 2, 6.0f, 0.8f, olive);
    L.container("battle_belt", "Battle belt", R, "waist", "belt", 4, 1, 3.0f, 0.7f, black).holster_kinds = "pistol,revolver,knife,dagger";
    L.v.back().holster_size = S::small;
    L.v.back().holster_count = 2;
    L.container("utility_belt", "Utility belt", R, "waist", "belt", 3, 1, 2.0f, 0.5f, leather).holster_kinds = "tool,knife,hatchet,hammer";
    L.v.back().holster_size = S::small;
    L.v.back().holster_count = 2;
    L.container("fanny_pack", "Fanny pack", R, "waist", "pouch", 2, 2, 2.0f, 0.2f, Vec3{0.6f, 0.2f, 0.7f});
    const char* P = "Containers/Pouches & bags";
    L.container("belt_pouch", "Belt pouch", P, "", "pouch", 2, 2, 1.0f, 0.15f, leather);
    L.container("ammo_pouch", "Ammo pouch", P, "", "pouch", 2, 1, 1.0f, 0.15f, olive).holster_kinds = "ammo";
    L.container("med_pouch", "Medical pouch", P, "", "pouch", 2, 2, 1.0f, 0.15f, red).holster_kinds = "medical";
    L.container("satchel", "Satchel", P, "side", "satchel", 3, 3, 8.0f, 0.6f, leather);
    L.container("duffel_bag", "Duffel bag", P, "", "duffel", 6, 4, 60.0f, 1.2f, black);
    L.container("ammo_box", "Ammo can", P, "", "ammo_box", 3, 2, 8.0f, 2.0f, olive).holster_kinds = "ammo";
    L.container("wallet", "Wallet", P, "", "wallet", 2, 1, 0.2f, 0.1f, leather);
    L.container("keyring", "Keyring", P, "", "keyring", 2, 1, 0.1f, 0.05f, steel).holster_kinds = "key";
    L.container("quiver", "Quiver", "Containers/Pouches & bags", "back", "quiver", 1, 4, 3.0f, 0.4f, leather).holster_kinds = "ammo";
    L.container("lunchbox", "Lunchbox", P, "", "box", 2, 2, 1.5f, 0.3f, red).holster_kinds = "food,drink";

    using SZ = ItemSize;
    L.holster("hip_holster", "Hip holster", "hip", "holster", "pistol,revolver,flintlock,hand_crossbow", SZ::small, 1, 0.3f, black);
    L.holster("thigh_holster", "Thigh holster", "thigh", "holster", "pistol,revolver", SZ::small, 1, 0.4f, black);
    L.holster("shoulder_holster", "Shoulder holster", "shoulders", "holster", "pistol,revolver", SZ::small, 2, 0.5f, leather);
    L.holster("rifle_sling", "Rifle sling", "sling", "sling", "gun,bow", SZ::huge, 1, 0.2f, olive);
    L.holster("two_point_sling", "Two-point sling", "sling", "sling", "gun", SZ::huge, 1, 0.2f, black);
    L.holster("scabbard", "Sword scabbard", "hip", "scabbard", "sword,longsword,falchion,scimitar,katana,rapier,khopesh,machete", SZ::large, 1, 0.6f, leather);
    L.holster("saya", "Saya (katana scabbard)", "hip", "scabbard", "katana", SZ::large, 2, 0.4f, black);
    L.holster("back_scabbard", "Back scabbard", "back_slot", "scabbard", "melee", SZ::huge, 1, 0.8f, leather);
    L.holster("knife_sheath", "Knife sheath", "hip", "sheath", "knife,dagger,karambit,kukri,kris", SZ::small, 1, 0.1f, black);
    L.holster("axe_loop", "Axe loop", "hip", "loop", "axe,hatchet,hammer,crowbar,mace,warhammer", SZ::medium, 1, 0.1f, leather);
    L.holster("bow_holder", "Bow holder", "back_slot", "sling", "bow", SZ::huge, 1, 0.3f, leather);
    L.holster("shield_strap", "Shield strap", "back_slot", "sling", "shield", SZ::huge, 1, 0.3f, leather);
    L.holster("staff_strap", "Staff strap", "back_slot", "sling", "magic", SZ::huge, 1, 0.2f, leather);
    L.holster("tool_belt", "Tool belt", "waist", "belt", "tool,melee", SZ::medium, 3, 0.8f, tan);
}

void misc_items(Library& L) {
    auto tool = [&](const char* id, const char* name, const char* shape, float weight, Vec3 c, const char* cat = "Tools") {
        ItemDef& d = L.add(id, name, cat, K::tool, shape);
        d.weight = weight;
        d.volume = weight * 1.5f;
        d.color = c;
        d.size = weight > 2.0f ? S::medium : S::small;
        return std::ref(d);
    };
    tool("flashlight", "Flashlight", "flashlight", 0.3f, black);
    tool("lighter", "Lighter", "lighter", 0.03f, red);
    tool("matches", "Matches", "box", 0.02f, Vec3{0.8f, 0.7f, 0.4f}).get().max_stack = 50.0f;
    tool("rope", "Rope", "rope", 1.0f, tan).get().continuous = true;
    L.v.back().unit = "m";
    L.v.back().max_stack = 30.0f;
    L.v.back().weight = 0.06f;
    tool("lockpick", "Lockpick", "lockpick", 0.01f, steel).get().max_stack = 10.0f;
    tool("compass", "Compass", "compass", 0.1f, bronze);
    tool("map", "Map", "scroll", 0.05f, Vec3{0.85f, 0.8f, 0.6f});
    tool("binoculars", "Binoculars", "binoculars", 0.8f, black);
    tool("radio", "Radio", "radio", 0.5f, olive);
    tool("repair_kit", "Repair kit", "box", 1.0f, Vec3{0.6f, 0.5f, 0.2f});
    tool("whetstone", "Whetstone", "bar", 0.2f, Vec3{0.4f, 0.4f, 0.45f});
    tool("fishing_rod", "Fishing rod", "rod", 0.5f, wood).get().size = S::large;
    tool("toolbox", "Toolbox", "box", 4.0f, red);
    tool("torch", "Torch", "torch", 0.6f, wood);
    tool("lantern", "Lantern", "lantern", 1.0f, iron);
    tool("night_vision", "Night vision goggles", "binoculars", 0.6f, olive).get().wear = "eyes";
    tool("geiger", "Geiger counter", "radio", 0.8f, Vec3{0.85f, 0.75f, 0.2f});
    auto key = [&](const char* id, const char* name, Vec3 c) {
        ItemDef& d = L.add(id, name, "Keys", K::key, "key");
        d.weight = 0.01f;
        d.volume = 0.01f;
        d.color = c;
        d.size = S::tiny;
    };
    key("key_brass", "Brass key", bronze);
    key("key_iron", "Iron key", iron);
    key("keycard_red", "Red keycard", red);
    key("keycard_blue", "Blue keycard", Vec3{0.1f, 0.3f, 0.9f});
    key("keycard_green", "Green keycard", Vec3{0.1f, 0.7f, 0.2f});
    auto material = [&](const char* id, const char* name, const char* shape, float weight, bool continuous, const char* unit, float stack, Vec3 c) {
        ItemDef& d = L.add(id, name, "Materials", K::material, shape);
        d.weight = weight;
        d.volume = weight;
        d.continuous = continuous;
        d.unit = unit;
        d.max_stack = stack;
        d.color = c;
        d.size = S::small;
    };
    material("gunpowder", "Gunpowder", "pouch", 1.0f, true, "kg", 2.0f, black);
    material("scrap_metal", "Scrap metal", "scrap", 0.5f, false, "", 20.0f, iron);
    material("wood_log", "Wood", "log", 2.0f, false, "", 10.0f, wood);
    material("cloth", "Cloth", "cloth", 0.1f, false, "", 20.0f, cloth);
    material("fuel", "Fuel", "jerrycan", 0.75f, true, "L", 20.0f, red);
    material("oil", "Oil", "can", 0.9f, true, "L", 5.0f, Vec3{0.2f, 0.15f, 0.05f});
    material("thread", "Thread", "spool", 0.02f, false, "", 50.0f, Vec3{0.9f, 0.9f, 0.85f});
    material("nails", "Nails", "box", 0.005f, false, "", 200.0f, steel);
    material("duct_tape", "Duct tape", "tape", 0.2f, true, "m", 50.0f, Vec3{0.6f, 0.6f, 0.62f});
    L.v.back().weight = 0.004f;
    material("gold_coin", "Gold coin", "coin", 0.008f, false, "", 1000.0f, gold);
    material("silver_coin", "Silver coin", "coin", 0.006f, false, "", 1000.0f, Vec3{0.8f, 0.8f, 0.82f});
    material("cash", "Cash", "cash", 0.001f, true, "$", 0.0f, Vec3{0.4f, 0.65f, 0.35f});
    material("iron_ingot", "Iron ingot", "ingot", 1.0f, false, "", 64.0f, iron);
    material("gem", "Gem", "gem", 0.01f, false, "", 64.0f, Vec3{0.2f, 0.8f, 0.6f});
    material("herb_bundle", "Herbs", "berries", 0.05f, false, "", 64.0f, Vec3{0.3f, 0.6f, 0.2f});
    material("sand", "Sand", "pouch", 1.6f, true, "kg", 25.0f, Vec3{0.85f, 0.75f, 0.5f});
}

struct Cartridge {
    const char* id;
    const char* name;
    const char* category;
    const char* klass;
    const char* caliber;
    float grain;
    float round_grain;
    float speed;
    const char* aliases;
    bool replaces;
};

const Cartridge cartridge_table[] = {
#include "cartridges.inc"
};


#include "gun_table.inc"

Vec3 named_color(const std::string& n) {
    if (n == "polymer") return polymer;
    if (n == "wood") return wood;
    if (n == "steel") return steel;
    if (n == "tan") return tan;
    if (n == "olive") return olive;
    if (n == "gunmetal") return gunmetal;
    if (n == "iron") return iron;
    if (n == "brass") return Vec3{0.8f, 0.62f, 0.28f};
    return black;
}

std::string category_for_class(const std::string& c, const std::string& action) {
    if (c == "pistol") return "Guns/Pistols";
    if (c == "machine_pistol") return "Guns/Machine pistols";
    if (c == "revolver") return "Guns/Revolvers";
    if (c == "derringer") return "Guns/Derringers";
    if (c == "smg") return "Guns/Submachine guns";
    if (c == "pdw") return "Guns/PDWs";
    if (c == "carbine") return "Guns/Carbines";
    if (c == "assault_rifle") return "Guns/Assault rifles";
    if (c == "battle_rifle") return "Guns/Battle rifles";
    if (c == "semi_rifle") return "Guns/Semi-auto rifles";
    if (c == "dmr") return "Guns/Marksman rifles";
    if (c == "bolt_rifle") return action == "break" || action == "single_shot" ? "Guns/Single-shot and double rifles" : "Guns/Bolt-action rifles";
    if (c == "lever_rifle") return "Guns/Lever-action rifles";
    if (c == "rimfire_rifle") return "Guns/Rimfire rifles";
    if (c == "sniper") return "Guns/Sniper rifles";
    if (c == "anti_materiel") return "Guns/Anti-materiel rifles";
    if (c == "shotgun") return "Guns/Shotguns";
    if (c == "lmg" || c == "gpmg" || c == "hmg" || c == "rotary") return "Guns/Machine guns";
    if (c == "grenade_launcher" || c == "rocket_launcher" || c == "flare") return "Guns/Launchers";
    if (c == "musket" || c == "black_powder_pistol") return "Guns/Black powder";
    if (c == "sci_fi") return "Guns/Sci-fi";
    return "Guns/Other";
}

float cycle_time_for(const std::string& action) {
    if (action == "bolt") return 0.9f;
    if (action == "straight_pull") return 0.55f;
    if (action == "pump") return 0.45f;
    if (action == "lever") return 0.5f;
    if (action == "revolver_sa") return 0.35f;
    return 0.0f;
}

void gun_table(Library& L) {
    for (const GunMeta& m : gun_meta_rows)
        for (ItemDef& d : L.v)
            if (d.id == m.id) {
                d.gun_class = m.gun_class;
                d.action = m.action;
                d.country = m.country;
                d.year = m.year;
                d.maker = m.maker;
                d.fire_modes = m.modes;
                d.slide_lock = m.lock;
                d.cycle_time = cycle_time_for(d.action);
                if (d.action == "rocket") d.feed = "internal";
                if (d.id == "m16a2") d.automatic = false;
                if (d.id == "m32") d.feed = "cylinder";
            }
    static const std::set<std::string> bullpups = {"tavor", "x95", "steyr_aug_a3", "famas_g2", "f2000", "qbz95", "sa80", "l85a3", "ots14_groza", "fs2000", "desert_tech_mdr", "ksg", "ut15", "dp12", "desert_tech_srs", "kel_tec_rfb", "dsr1", "pdw_57", "type05"};
    for (const GunRow& r : gun_rows) {
        bool exists = false;
        for (const ItemDef& d : L.v)
            if (d.id == r.id) exists = true;
        if (exists) continue;
        const ItemDef* a = nullptr;
        for (const ItemDef& d : L.v)
            if (d.id == r.ammo) a = &d;
        const std::string klass = r.gun_class, action = r.action;
        const std::string ammo_class = a ? a->cartridge_class : std::string();
        const bool handgun = klass == "pistol" || klass == "machine_pistol" || klass == "revolver" || klass == "derringer" || klass == "black_powder_pistol";
        const bool long_gun = !handgun;
        const bool launcher = klass == "grenade_launcher" || klass == "rocket_launcher" || klass == "flare";
        const bool energy = action == "energy";
        float speed = a && a->speed > 0.0f ? a->speed : 350.0f;
        if (ammo_class == "handgun" && long_gun) speed *= 1.1f;
        if ((ammo_class == "rifle") && r.length < 0.8f && !handgun) speed *= 0.92f;
        if (energy) speed = r.id == std::string("gauss_rifle") ? 6000.0f : r.id == std::string("plasma_pistol") ? 120.0f : 1e6f;
        if (launcher) speed = a && a->speed > 0.0f ? a->speed : 80.0f;
        const float grain = a && a->grain > 0.0f ? a->grain : (a ? a->projectile_mass * grains_per_kg : 120.0f);
        const float mass = std::max(0.0005f, grain / grains_per_kg);
        const bool shot = ammo_class == "shotgun" || std::string(r.ammo) == "12g_buck";
        int pellets = 1;
        float energy_j = 0.5f * mass * speed * speed;
        if (shot) {
            pellets = std::string(r.ammo) == "ammo_410_bore" ? 5 : 9;
            energy_j /= static_cast<float>(pellets);
        }
        float dmg = 1.9f * std::pow(std::max(1.0f, energy_j), 0.4f);
        if (shot) dmg *= 0.65f;
        if (energy) dmg = klass == "sci_fi" && r.mag <= 10 ? 120.0f : 26.0f;
        std::string shape;
        if (klass == "pistol" || klass == "machine_pistol") shape = "pistol";
        else if (klass == "revolver" || klass == "derringer") shape = "revolver";
        else if (klass == "smg" || klass == "pdw") shape = bullpups.count(r.id) ? "bullpup" : "smg";
        else if (klass == "carbine") shape = "carbine";
        else if (klass == "sniper" || klass == "anti_materiel") shape = bullpups.count(r.id) ? "bullpup" : "sniper";
        else if (klass == "shotgun") shape = bullpups.count(r.id) ? "bullpup" : r.length < 0.6f ? "sawed_off" : "shotgun";
        else if (klass == "lmg" || klass == "gpmg" || klass == "hmg" || klass == "rotary") shape = "lmg";
        else if (launcher) shape = "launcher";
        else if (klass == "musket") shape = "musket";
        else if (klass == "black_powder_pistol") shape = "flintlock";
        else shape = bullpups.count(r.id) ? "bullpup" : "rifle";
        float rate = r.rpm > 0.0f ? r.rpm / 60.0f : 3.0f;
        if (r.rpm <= 0.0f) {
            if (handgun) rate = 3.5f;
            if (klass == "dmr" || klass == "sniper") rate = 2.5f;
            if (action == "bolt") rate = 0.75f;
            if (action == "straight_pull") rate = 1.0f;
            if (action == "lever") rate = 1.5f;
            if (action == "pump") rate = 1.2f;
            if (action == "break") rate = 3.0f;
            if (action == "single_shot") rate = 0.4f;
            if (action == "revolver_da") rate = 2.0f;
            if (action == "revolver_sa") rate = 1.2f;
            if (action == "rocket") rate = 0.5f;
            if (action == "muzzle") rate = 1.0f / 15.0f;
            if (energy) rate = 4.0f;
        }
        float spread = 0.6f;
        if (handgun) spread = klass == "revolver" ? 1.2f : 1.6f;
        if (klass == "smg" || klass == "pdw") spread = 2.2f;
        if (klass == "carbine") spread = 0.7f;
        if (klass == "dmr") spread = 0.25f;
        if (klass == "bolt_rifle" || klass == "lever_rifle" || klass == "rimfire_rifle") spread = 0.35f;
        if (klass == "sniper") spread = 0.08f;
        if (klass == "anti_materiel") spread = 0.1f;
        if (shot) spread = r.length < 0.6f ? 7.0f : 4.0f;
        if (klass == "lmg" || klass == "gpmg" || klass == "hmg" || klass == "rotary") spread = 1.4f;
        if (launcher) spread = 0.8f;
        if (klass == "musket") spread = 2.5f;
        if (klass == "black_powder_pistol") spread = 3.0f;
        ItemSize size = ItemSize::large;
        int gw = r.length > 1.0f ? 5 : 4, gh = 2;
        if (handgun) {
            size = ItemSize::small;
            gw = 2;
            gh = 1;
        } else if (klass == "smg" || klass == "pdw") {
            size = ItemSize::medium;
            gw = 3;
            gh = 2;
        } else if (klass == "sniper" || klass == "anti_materiel" || klass == "lmg" || klass == "gpmg" || klass == "hmg" || klass == "rotary" || launcher) {
            size = ItemSize::huge;
            gw = 5;
            gh = 2;
        } else if (klass == "musket" || klass == "shotgun") {
            gh = 1;
            gw = r.length < 0.6f ? 3 : 5;
            if (r.length < 0.6f) size = ItemSize::medium;
        }
        if (klass == "flare" || (klass == "grenade_launcher" && r.length < 0.45f)) {
            size = ItemSize::small;
            gw = 2;
            gh = 1;
        }
        const std::vector<std::string> modes = [&] {
            std::vector<std::string> out;
            std::string part;
            for (const char* c = r.modes;; ++c) {
                if (*c == ',' || *c == '\0') {
                    if (!part.empty()) out.push_back(part);
                    part.clear();
                    if (*c == '\0') break;
                } else part += *c;
            }
            return out;
        }();
        const bool autom = std::find(modes.begin(), modes.end(), "auto") != modes.end();
        ItemDef& d = L.gun(r.id, r.name, category_for_class(klass, action), shape, std::round(dmg), rate, r.mag, r.ammo, std::round(speed), spread, r.weight, r.length, autom, size, gw, gh);
        d.gun_class = klass;
        d.action = action;
        d.country = r.country;
        d.year = r.year;
        d.maker = r.maker;
        d.fire_modes = r.modes;
        d.slide_lock = r.lock;
        d.feed = r.feed;
        d.burst = r.id == std::string("an94") ? 2 : 3;
        d.cycle_time = cycle_time_for(action);
        d.color = named_color(r.color);
        d.accent = named_color(r.accent);
        d.pellets = pellets;
        d.projectile_mass = mass;
        d.drag = shot ? 0.006f : ammo_class == "handgun" ? 0.0027f : grain > 400.0f ? 0.0006f : 0.0012f;
        d.penetration = handgun || shot ? 0.0f : speed / 100.0f;
        d.recoil = dmg * (klass == "sniper" || klass == "anti_materiel" ? 0.05f : 0.04f);
        d.range = shot ? 40.0f : speed * 0.6f;
        if (d.feed.empty()) {
            if (action == "revolver_da" || action == "revolver_sa") d.feed = "cylinder";
            else if (action == "muzzle") d.feed = "muzzle";
            else if (action == "belt" || action == "rotary") d.feed = "belt";
            else if (energy) d.feed = "pool";
            else if (action == "bolt" || action == "straight_pull" || action == "pump" || action == "lever" || action == "break" || action == "single_shot" || action == "rocket") d.feed = "internal";
            else d.feed = "magazine";
        }
        const float per_round = action == "lever" ? 0.5f : action == "pump" ? 0.5f : action == "break" ? 0.9f : action == "revolver_sa" ? 0.6f : action == "revolver_da" ? 0.35f : 0.45f;
        if (d.feed == "internal" || d.feed == "cylinder") d.reload = std::max(1.0f, per_round * static_cast<float>(std::max(1, r.mag)));
        else if (d.feed == "belt") d.reload = 5.5f;
        else if (d.feed == "muzzle") d.reload = handgun ? 12.0f : 18.0f;
        else d.reload = handgun ? 1.6f : size == ItemSize::medium ? 2.2f : 2.6f;
        if (action == "break" && r.mag <= 2) d.reload = 2.2f;
        if (launcher) {
            d.damage_type = std::string(r.ammo) == "flare" ? "fire" : "explosion";
            d.damage = std::string(r.ammo) == "flare" ? 15.0f : std::string(r.ammo) == "40mm" ? 120.0f : 190.0f;
            d.blast_radius = std::string(r.ammo) == "flare" ? 0.5f : std::string(r.ammo) == "40mm" ? 4.0f : 5.5f;
            d.projectile_mass = 1.0f;
            d.drag = 0.004f;
            d.ballistics = "drop";
            d.penetration = 0.0f;
            d.reload = action == "belt" ? 6.0f : 4.0f;
            if (std::string(r.ammo) == "flare") {
                d.effect = "burning";
                d.effect_time = 6.0f;
                d.effect_chance = 1.0f;
            }
        }
        if (klass == "musket" || klass == "black_powder_pistol") {
            d.projectile_mass = std::max(d.projectile_mass, 0.02f);
            d.drag = 0.004f;
            d.ballistics = "drop";
            if (std::string(r.ammo) == "musket_shot") d.pellets = 10;
        }
        if (energy) {
            d.damage_type = r.id == std::string("needler") ? "projectile" : "fire";
            d.projectile_mass = 0.001f;
            d.drag = 0.0f;
            d.ballistics = speed > 5000.0f ? "hitscan" : "travel";
            d.accent = Vec3{0.2f, 0.8f, 1.0f};
        }
        if (klass == "sniper" || klass == "anti_materiel" || klass == "dmr") d.tags.push_back("scoped");
        if (std::string(r.id).find("sd") != std::string::npos || std::string(r.id) == "pb_6p9" || std::string(r.id) == "vss" || std::string(r.id) == "as_val") d.tags.push_back("suppressed");
    }
}

void gun_hammers(Library& L) {
    static const std::set<std::string> hammers = {"double_barrel", "sawed_off", "stoeger_coach", "lupara", "single_barrel", "h_and_r_pardner", "sharps_1874", "rolling_block", "trapdoor", "snider_enfield", "thompson_encore", "h_and_r_handi", "remington_derringer", "philadelphia_deringer", "bond_arms", "cobra_derringer", "flare_gun", "flare_pistol_hebel"};
    for (ItemDef& d : L.v)
        if (d.kind == ItemKind::gun && hammers.count(d.id)) d.hammer = true;
}

void cartridges(Library& L) {
    const Vec3 brass{0.78f, 0.6f, 0.25f};
    const Vec3 steel_case{0.42f, 0.45f, 0.38f};
    const Vec3 hull{0.7f, 0.12f, 0.1f};
    const Vec3 lead{0.48f, 0.48f, 0.5f};
    const Vec3 paper{0.82f, 0.76f, 0.6f};
    for (const Cartridge& c : cartridge_table) {
        ItemDef* d = nullptr;
        for (ItemDef& x : L.v)
            if (x.id == c.id) d = &x;
        const std::string klass = c.klass;
        if (!d) {
            int stack = klass == "rimfire" ? 100 : klass == "handgun" ? 50 : klass == "shotgun" ? 25 : klass == "airgun" ? 500 : klass == "blackpowder" ? 30 : 40;
            if (klass == "specialty") stack = 10;
            if (c.grain > 350.0f && klass == "rifle") stack = 20;
            d = &L.ammo(c.id, c.name, c.category, c.round_grain / grains_per_kg, stack, brass);
        }
        d->name = c.name;
        d->category = c.category;
        d->caliber = c.caliber;
        d->grain = c.grain;
        d->cartridge_grain = c.round_grain;
        d->speed = c.speed;
        d->cartridge_class = klass;
        d->aliases = c.aliases;
        d->weight = c.round_grain / grains_per_kg;
        d->volume = d->weight * 0.4f;
        d->projectile_mass = c.grain / grains_per_kg;
        d->shape = klass == "shotgun" ? "shell" : klass == "airgun" ? "pellet" : "round";
        d->length = klass == "airgun" ? 0.006f : std::clamp(0.012f + std::cbrt(c.round_grain) * 0.0045f, 0.01f, 0.2f);
        const std::string name = c.name;
        if (klass == "shotgun") d->color = hull;
        else if (klass == "airgun") d->color = lead;
        else if (klass == "blackpowder" && name.find("Musket") != std::string::npos) d->color = paper;
        else if (name.find("39mm") != std::string::npos || name.find("Makarov") != std::string::npos || name.find("54mmR") != std::string::npos || name.find("108mm") != std::string::npos || name.find("114mm") != std::string::npos) d->color = steel_case;
        else d->color = brass;
        if (klass == "specialty") d->size = ItemSize::small;
        d->default_variant = klass == "shotgun" ? "buckshot" : klass == "airgun" ? "domed" : "fmj";
        if (d->description.empty() || c.replaces) {
            char buf[160];
            std::snprintf(buf, sizeof(buf), "%.0f gr bullet at about %.0f m/s.", static_cast<double>(c.grain), static_cast<double>(c.speed));
            d->description = buf;
            if (klass == "shotgun") {
                std::snprintf(buf, sizeof(buf), "%.0f gr of shot at about %.0f m/s.", static_cast<double>(c.grain), static_cast<double>(c.speed));
                d->description = buf;
            }
        }
    }
    for (const char* id : {"338lm", "455webley"})
        for (ItemDef& x : L.v)
            if (x.id == id) {
                const bool rifle = std::string(id) == "338lm";
                x.category = rifle ? "Ammo/Rifle, 8mm to .378" : "Ammo/Handgun";
                x.cartridge_class = rifle ? "rifle" : "handgun";
                x.caliber = id;
                x.grain = rifle ? 250.0f : 265.0f;
                x.cartridge_grain = rifle ? 710.0f : 400.0f;
                x.speed = rifle ? 905.0f : 190.0f;
                x.projectile_mass = x.grain / grains_per_kg;
                x.weight = x.cartridge_grain / grains_per_kg;
                x.default_variant = "fmj";
            }
    for (ItemDef& x : L.v) {
        if (x.id == "12g_buck" || x.id == "12g_slug" || x.id == "12g_dragon") {
            x.caliber = "12g";
            x.category = "Ammo/Shotgun";
            x.cartridge_class = "shotgun";
            x.variants = x.id == "12g_buck" ? "buckshot" : x.id == "12g_slug" ? "slug" : "dragons_breath";
            x.default_variant = x.variants;
            x.grain = 492.0f;
            x.cartridge_grain = 713.0f;
        }
    }
}

std::string feed_for(const ItemDef& g) {
    static const std::set<std::string> detachable = {"awm", "intervention", "svd", "barrett_m82", "aa12"};
    if (detachable.count(g.id)) return "magazine";
    const std::string& c = g.category;
    if (c == "Guns/Revolvers") return "cylinder";
    if (c == "Guns/Bolt-action rifles" || c == "Guns/Lever-action rifles" || c == "Guns/Shotguns" || c == "Guns/Sniper rifles" || c == "Guns/Launchers") return "internal";
    if (c == "Guns/Black powder") return "muzzle";
    if (c == "Guns/Sci-fi") return "pool";
    if (g.id == "m1_garand" || g.id == "sks") return "internal";
    if (g.id == "m249" || g.id == "pkm" || g.id == "m60" || g.id == "mg42") return "belt";
    return "magazine";
}

void calibers_and_magazines(Library& L) {
    std::map<std::string, std::string> caliber_of;
    for (const ItemDef& d : L.v)
        if (d.kind == K::ammo) caliber_of[d.id] = d.caliber.empty() ? d.id : d.caliber;
    std::map<std::pair<std::string, int>, std::vector<std::size_t>> groups;
    for (std::size_t i = 0; i < L.v.size(); ++i) {
        ItemDef& g = L.v[i];
        if (g.kind != K::gun) continue;
        if (g.caliber.empty()) g.caliber = caliber_of.count(g.ammo) ? caliber_of[g.ammo] : g.ammo;
        if (g.feed.empty()) g.feed = feed_for(g);
        if (g.caliber == "357") g.also_fires = "ammo_38_special,ammo_38_long_colt,ammo_38_short_colt";
        if (g.caliber == "556") g.also_fires = "ammo_223_remington,ammo_223_wylde";
        if (g.caliber == "308") g.also_fires = "762x51";
        if (g.caliber == "762x51") g.also_fires = "308";
        if (g.caliber == "45colt") g.also_fires = "ammo_454_casull";
        if (g.id == "colt_saa" || g.id == "win1873") g.also_fires += std::string(g.also_fires.empty() ? "" : ",") + "4440";
        if (g.feed == "magazine" || g.feed == "belt") groups[{g.caliber, g.magazine}].push_back(i);
    }
    std::vector<ItemDef> mags;
    for (const auto& [key, guns] : groups) {
        const ItemDef& first = L.v[guns.front()];
        ItemDef m;
        const bool belt = first.feed == "belt";
        m.id = "mag_" + key.first + "_" + std::to_string(key.second);
        std::string ammo_name = first.ammo;
        for (const ItemDef& a : L.v)
            if (a.id == first.ammo) ammo_name = a.name.substr(0, a.name.find(" ("));
        if (guns.size() == 1) m.name = first.name + (belt ? " ammo box" : " magazine");
        else m.name = std::to_string(key.second) + "-round " + ammo_name + (belt ? " box" : " magazine");
        m.kind = K::magazine;
        m.category = first.category == "Guns/Pistols" || first.category == "Guns/Submachine guns" ? "Magazines/Pistol and SMG" : belt ? "Magazines/Machine gun boxes" : "Magazines/Rifle";
        m.magazine = key.second;
        m.caliber = key.first;
        m.ammo = first.ammo;
        for (std::size_t gi : guns) m.fits += (m.fits.empty() ? "" : ",") + L.v[gi].id;
        const bool drum = key.second >= 50 && !belt;
        m.shape = belt ? "ammo_box" : drum ? "drum" : first.category == "Guns/Pistols" ? "mag_pistol" : "mag_rifle";
        m.size = belt || drum ? ItemSize::medium : ItemSize::small;
        m.grid_w = belt || drum ? 2 : 1;
        m.grid_h = belt || drum ? 2 : first.category == "Guns/Pistols" ? 1 : 2;
        m.weight = belt ? 0.5f : drum ? 0.7f : first.category == "Guns/Pistols" ? 0.07f : 0.12f;
        m.volume = m.weight * 0.6f + 0.05f;
        m.length = first.category == "Guns/Pistols" ? 0.12f : 0.2f;
        m.color = Vec3{0.12f, 0.12f, 0.13f};
        m.accent = Vec3{0.78f, 0.6f, 0.25f};
        m.description = "Holds " + std::to_string(key.second) + " rounds of " + ammo_name + ".";
        m.unit = "rounds";
        mags.push_back(m);
        for (std::size_t gi : guns) L.v[gi].magazines += (L.v[gi].magazines.empty() ? "" : ",") + m.id;
    }
    for (ItemDef& m : mags) L.v.push_back(m);
}

void enchanted_items(Library& L) {
    const Vec3 steel{0.75f, 0.76f, 0.78f};
    ItemDef& ember = L.melee("ember_blade", "Ember blade", "Melee/Enchanted", "sword", 34.0f, "slash", 1.4f, 1.6f, 1.3f, 0.95f, ItemSize::large, {"sharp", "magic"});
    ember.color = Vec3{0.95f, 0.45f, 0.15f};
    ember.extra_damage = "fire=10";
    ember.effects = "burning:4:0.5";
    ItemDef& storm = L.melee("storm_hammer", "Storm hammer", "Melee/Enchanted", "hammer", 40.0f, "blunt", 0.9f, 1.4f, 4.0f, 0.9f, ItemSize::large, {"blunt", "magic"});
    storm.color = Vec3{0.45f, 0.6f, 0.95f};
    storm.extra_damage = "shock=14";
    storm.effects = "shocked:1.5:0.6, stunned:0.6:0.25";
    ItemDef& venom = L.melee("venom_dagger", "Venom dagger", "Melee/Enchanted", "dagger", 18.0f, "pierce", 2.6f, 1.0f, 0.3f, 0.3f, ItemSize::small, {"sharp", "magic"});
    venom.color = Vec3{0.35f, 0.85f, 0.25f};
    venom.extra_damage = "acid=4";
    venom.effects = "poison:8:0.8";
    ItemDef& frost = L.melee("frostbrand", "Frostbrand", "Melee/Enchanted", "longsword", 38.0f, "slash", 1.1f, 1.8f, 1.6f, 1.15f, ItemSize::large, {"sharp", "magic"});
    frost.color = Vec3{0.7f, 0.9f, 1.0f};
    frost.extra_damage = "cold=10";
    frost.effects = "chilled:5:0.8, frozen:1.2:0.15";
    ItemDef& dragon = L.gun("dragonfire_rifle", "Dragonfire rifle", "Guns/Enchanted", "rifle", 36.0f, 8.0f, 30, "762x39", 715.0f, 0.9f, 4.0f, 0.9f, true, ItemSize::large, 4, 2);
    dragon.color = Vec3{0.45f, 0.12f, 0.08f};
    dragon.accent = Vec3{0.85f, 0.6f, 0.2f};
    dragon.extra_damage = "fire=8";
    dragon.effects = "burning:3:0.35";
    ItemDef& tesla = L.gun("arc_pistol", "Arc pistol", "Guns/Enchanted", "pistol", 22.0f, 3.5f, 17, "9mm", 375.0f, 1.6f, 0.8f, 0.2f, false, ItemSize::small, 2, 1);
    tesla.color = Vec3{0.3f, 0.4f, 0.7f};
    tesla.extra_damage = "shock=10";
    tesla.effects = "shocked:1:0.5";
    auto armor = [&](const char* id, const char* name, const char* slot, float a, float warm, float weight, Vec3 col) -> ItemDef& {
        return L.wear(id, name, "Armor/Enchanted", slot, slot, a, warm, weight, col, K::armor);
    };
    ItemDef& cloak = L.wear("fireward_cloak", "Fireward cloak", "Clothing/Enchanted", "over", "cloak", 0.0f, 4.0f, 1.2f, Vec3{0.6f, 0.15f, 0.1f});
    cloak.resists = "fire=0.6";
    cloak.worn_effects = "warm:-1:1:0.5";
    ItemDef& thorn = armor("thornmail", "Thornmail", "torso", 28.0f, 2.0f, 9.0f, Vec3{0.35f, 0.38f, 0.3f});
    thorn.thorns_damage = "pierce=6";
    thorn.thorns = "bleeding:3:0.5";
    ItemDef& frostplate = armor("frost_plate", "Frost plate", "torso", 40.0f, 8.0f, 14.0f, Vec3{0.75f, 0.88f, 0.95f});
    frostplate.resists = "cold=0.75, fire=-0.25";
    frostplate.thorns = "chilled:3:0.6";
    ItemDef& storm_helm = armor("storm_helm", "Storm helm", "head", 14.0f, 1.0f, 2.0f, steel);
    storm_helm.resists = "shock=0.8";
    storm_helm.thorns_damage = "shock=5";
    ItemDef& acid_boots = armor("acidwalker_boots", "Acidwalker boots", "feet", 6.0f, 2.0f, 1.6f, Vec3{0.3f, 0.5f, 0.25f});
    acid_boots.resists = "acid=0.8, poison=0.5";
    ItemDef& ring = L.wear("ring_vigor", "Ring of vigor", "Trinkets", "ring", "ring", 0.0f, 0.0f, 0.01f, Vec3{0.9f, 0.75f, 0.2f}, K::trinket);
    ring.worn_effects = "regeneration:-1:1:0.5, haste:-1:1:0.3";
    ItemDef& amulet = L.wear("amulet_warding", "Amulet of warding", "Trinkets", "neck", "amulet", 0.0f, 0.0f, 0.05f, Vec3{0.5f, 0.3f, 0.9f}, K::trinket);
    amulet.resists = "magic=0.5, shock=0.25, fire=0.25, cold=0.25";
}

void finish(std::vector<ItemDef>& v) {
    for (ItemDef& d : v) {
        if (d.grid_w == 1 && d.grid_h == 1 && d.kind != ItemKind::ammo && d.kind != ItemKind::magazine) {
            switch (d.size) {
            case ItemSize::tiny:
            case ItemSize::small: break;
            case ItemSize::medium: d.grid_w = 2; d.grid_h = 2; break;
            case ItemSize::large: d.grid_w = 3; d.grid_h = 2; break;
            case ItemSize::huge: d.grid_w = 4; d.grid_h = 3; break;
            }
        }
        if (d.weapon() && d.max_stack == 1.0f && d.kind != ItemKind::throwable) d.max_stack = 1.0f;
        if (d.description.empty()) {
            if (d.kind == ItemKind::gun) {
                std::string ammo = d.ammo;
                for (const ItemDef& a : v)
                    if (a.id == d.ammo) ammo = a.name;
                d.description = std::to_string(d.magazine) + " rounds of " + ammo + (d.automatic ? ", automatic" : "");
            }
            else if (d.kind == ItemKind::melee) d.description = d.damage_type + " weapon";
            else if (d.kind == ItemKind::container) d.description = std::to_string(d.slots_w) + "x" + std::to_string(d.slots_h) + " storage";
        }
    }
}

} // namespace

const std::vector<ItemDef>& item_library() {
    static const std::vector<ItemDef> all = [] {
        Library L;
        guns(L);
        ammunition(L);
        melee_weapons(L);
        ranged_primitive(L);
        throwables(L);
        magic_items(L);
        consumables(L);
        wearables(L);
        storage(L);
        misc_items(L);
        enchanted_items(L);
        cartridges(L);
        gun_table(L);
        gun_hammers(L);
        calibers_and_magazines(L);
        finish(L.v);
        return L.v;
    }();
    return all;
}

} // namespace orchard
