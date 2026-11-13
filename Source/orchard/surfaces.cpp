#include <orchard/surfaces.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace orchard {

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string num(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(v));
    return buf;
}

SurfaceDef make(const char* id, const char* name, float walk, float ricochet, float angle, float resistance, float slows, const char* step, const char* impact, const char* hit, Vec3 color) {
    SurfaceDef s;
    s.id = id;
    s.name = name;
    s.walk = walk;
    s.ricochet = ricochet;
    s.ricochet_angle = angle;
    s.resistance = resistance;
    s.slows_bullets = slows;
    s.step_sound = step;
    s.impact = impact;
    s.impact_sound = hit;
    s.color = color;
    return s;
}

} // namespace

const std::vector<SurfaceDef>& surface_library() {
    static const std::vector<SurfaceDef> all = [] {
        std::vector<SurfaceDef> v;
        v.push_back(make("default", "Default", 1.0f, 1.0f, 15.0f, 6.0f, 0.5f, "step_concrete", "impact", "hit_concrete", {0.6f, 0.6f, 0.6f}));
        v.push_back(make("concrete", "Concrete", 1.0f, 1.3f, 18.0f, 12.0f, 0.6f, "step_concrete", "impact-concrete", "hit_concrete", {0.62f, 0.62f, 0.6f}));
        v.push_back(make("asphalt", "Asphalt", 1.0f, 1.1f, 15.0f, 12.0f, 0.6f, "step_concrete", "impact-concrete", "hit_concrete", {0.25f, 0.25f, 0.26f}));
        v.push_back(make("stone", "Stone and rock", 1.0f, 1.5f, 20.0f, 15.0f, 0.7f, "step_concrete", "impact-concrete", "hit_concrete", {0.5f, 0.49f, 0.47f}));
        v.push_back(make("brick", "Brick", 1.0f, 1.0f, 15.0f, 10.0f, 0.6f, "step_concrete", "impact-brick", "hit_concrete", {0.6f, 0.32f, 0.24f}));
        v.push_back(make("tile", "Tile", 1.0f, 1.2f, 15.0f, 4.0f, 0.4f, "step_tile", "impact-concrete", "hit_concrete", {0.85f, 0.85f, 0.82f}));
        v.push_back(make("metal", "Thick metal", 1.0f, 2.5f, 25.0f, 14.0f, 0.8f, "step_metal", "impact-metal", "hit_metal", {0.55f, 0.57f, 0.6f}));
        v.push_back(make("sheet_metal", "Sheet metal", 1.0f, 1.6f, 20.0f, 3.0f, 0.3f, "step_metal", "impact-metal", "hit_metal", {0.6f, 0.62f, 0.64f}));
        v.push_back(make("wood", "Wood", 1.0f, 0.3f, 10.0f, 2.0f, 0.35f, "step_wood", "impact-wood", "hit_wood", {0.5f, 0.34f, 0.2f}));
        v.push_back(make("drywall", "Drywall and plaster", 1.0f, 0.05f, 6.0f, 0.5f, 0.15f, "step_tile", "impact-concrete", "hit_wood", {0.9f, 0.9f, 0.88f}));
        v.push_back(make("glass", "Glass", 1.0f, 0.2f, 8.0f, 0.3f, 0.1f, "step_tile", "impact-glass", "hit_glass", {0.8f, 0.9f, 0.95f}));
        v.push_back(make("plastic", "Plastic", 1.0f, 0.4f, 10.0f, 1.0f, 0.2f, "step_tile", "impact", "hit_wood", {0.5f, 0.5f, 0.55f}));
        v.push_back(make("rubber", "Rubber", 1.0f, 0.3f, 10.0f, 2.0f, 0.3f, "step_carpet", "impact", "hit_dirt", {0.15f, 0.15f, 0.15f}));
        v.push_back(make("carpet", "Carpet", 1.0f, 0.1f, 6.0f, 9.0f, 0.6f, "step_carpet", "impact-dust", "hit_dirt", {0.5f, 0.35f, 0.3f}));
        v.push_back(make("dirt", "Dirt", 1.0f, 0.3f, 8.0f, 9.0f, 0.8f, "step_dirt", "impact-dirt", "hit_dirt", {0.42f, 0.31f, 0.2f}));
        v.push_back(make("grass", "Grass", 1.0f, 0.3f, 8.0f, 9.0f, 0.8f, "step_grass", "impact-grass", "hit_dirt", {0.3f, 0.45f, 0.2f}));
        v.push_back(make("leaves", "Leaves and forest floor", 1.0f, 0.2f, 6.0f, 9.0f, 0.8f, "step_leaves", "impact-dirt", "hit_dirt", {0.45f, 0.33f, 0.18f}));
        v.push_back(make("gravel", "Gravel", 0.95f, 0.8f, 12.0f, 10.0f, 0.8f, "step_gravel", "impact-dirt", "hit_dirt", {0.55f, 0.53f, 0.5f}));
        v.push_back(make("sand", "Sand", 0.85f, 0.1f, 5.0f, 12.0f, 0.9f, "step_sand", "impact-sand", "hit_dirt", {0.85f, 0.76f, 0.55f}));
        v.push_back(make("mud", "Mud", 0.7f, 0.02f, 4.0f, 10.0f, 0.9f, "step_mud", "impact-mud", "hit_mud", {0.3f, 0.22f, 0.14f}));
        v.push_back(make("snow", "Snow", 0.8f, 0.15f, 6.0f, 4.0f, 0.5f, "step_snow", "impact-snow", "hit_snow", {0.93f, 0.95f, 0.98f}));
        v.push_back(make("deep_snow", "Deep snow", 0.55f, 0.05f, 4.0f, 6.0f, 0.7f, "step_snow", "impact-snow", "hit_snow", {0.95f, 0.96f, 1.0f}));
        v.push_back(make("ice", "Ice", 1.0f, 1.2f, 12.0f, 5.0f, 0.4f, "step_ice", "impact-snow", "hit_glass", {0.75f, 0.88f, 0.95f}));
        v.push_back(make("water_shallow", "Shallow water", 1.0f, 0.6f, 7.0f, 0.0f, 0.0f, "step_water", "impact-water", "hit_water", {0.3f, 0.45f, 0.55f}));
        v.push_back(make("water", "Deep water", 1.0f, 0.6f, 7.0f, 0.0f, 0.0f, "step_water", "impact-water", "hit_water", {0.2f, 0.35f, 0.5f}));
        v.push_back(make("tall_grass", "Tall grass", 1.0f, 0.0f, 0.0f, 0.2f, 0.05f, "step_grass", "impact-leaves", "hit_leaves", {0.4f, 0.55f, 0.25f}));
        v.push_back(make("shrubs", "Shrubs and bushes", 1.0f, 0.0f, 0.0f, 0.3f, 0.08f, "step_leaves", "impact-leaves", "hit_leaves", {0.25f, 0.42f, 0.18f}));
        v.push_back(make("crops", "Crops (wheat, corn)", 1.0f, 0.0f, 0.0f, 0.2f, 0.05f, "step_grass", "impact-leaves", "hit_leaves", {0.75f, 0.65f, 0.3f}));
        v.push_back(make("sandbags", "Sandbags", 1.0f, 0.05f, 4.0f, 20.0f, 0.95f, "step_sand", "impact-sand", "hit_dirt", {0.6f, 0.55f, 0.4f}));
        v.push_back(make("cloth", "Cloth", 1.0f, 0.0f, 0.0f, 0.3f, 0.05f, "step_carpet", "impact-dust", "hit_dirt", {0.6f, 0.55f, 0.5f}));
        v.push_back(make("flesh", "Flesh", 1.0f, 0.0f, 0.0f, 1.0f, 0.4f, "step_dirt", "impact-flesh", "hit_flesh", {0.6f, 0.1f, 0.08f}));
        for (SurfaceDef& s : v) {
            const std::string& id = s.id;
            if (id == "tall_grass") {
                s.volume = true;
                s.wade = 0.9f;
                s.noise = 0.8f;
            } else if (id == "shrubs") {
                s.volume = true;
                s.wade = 0.75f;
                s.noise = 1.3f;
                s.stamina = 1.2f;
            } else if (id == "crops") {
                s.volume = true;
                s.wade = 0.85f;
                s.noise = 1.1f;
            } else if (id == "water_shallow") {
                s.volume = true;
                s.wade = 0.75f;
                s.stamina = 1.3f;
                s.noise = 1.5f;
                s.jump = 0.8f;
            } else if (id == "water") {
                s.volume = true;
                s.wade = 0.5f;
                s.stamina = 1.6f;
                s.noise = 1.2f;
                s.jump = 0.5f;
            } else if (id == "snow") {
                s.jump = 0.85f;
                s.stamina = 1.3f;
                s.noise = 0.7f;
                s.soft = true;
            } else if (id == "deep_snow") {
                s.jump = 0.6f;
                s.stamina = 1.8f;
                s.noise = 0.6f;
                s.soft = true;
            } else if (id == "mud") {
                s.jump = 0.7f;
                s.stamina = 1.5f;
                s.soft = true;
            } else if (id == "sand") {
                s.jump = 0.8f;
                s.stamina = 1.3f;
                s.soft = true;
            } else if (id == "ice") {
                s.slip = 0.85f;
                s.noise = 0.9f;
            } else if (id == "metal" || id == "sheet_metal") {
                s.noise = 1.4f;
            } else if (id == "gravel" || id == "leaves") {
                s.noise = 1.3f;
            } else if (id == "carpet") {
                s.noise = 0.4f;
            } else if (id == "dirt" || id == "grass") {
                s.soft = true;
                s.noise = 0.8f;
            }
        }
        return v;
    }();
    return all;
}

const SurfaceDef* builtin_surface(const std::string& id) {
    for (const SurfaceDef& s : surface_library())
        if (s.id == id) return &s;
    return nullptr;
}

std::vector<std::string> surface_ids() {
    std::vector<std::string> out;
    for (const SurfaceDef& s : surface_library()) out.push_back(s.id);
    return out;
}

std::string guess_surface(const std::string& text_in) {
    const std::string t = lower(text_in);
    if (t.empty()) return std::string();
    struct Word {
        const char* word;
        const char* surface;
    };
    static const Word words[] = {
        {"deep_snow", "deep_snow"}, {"deep snow", "deep_snow"}, {"snow", "snow"}, {"ice", "ice"}, {"frost", "snow"},
        {"water", "water_shallow"}, {"lake", "water"}, {"river", "water_shallow"}, {"ocean", "water"}, {"pool", "water_shallow"},
        {"mud", "mud"}, {"swamp", "mud"}, {"bog", "mud"}, {"sandbag", "sandbags"}, {"sand", "sand"}, {"dune", "sand"}, {"beach", "sand"},
        {"gravel", "gravel"}, {"pebble", "gravel"}, {"tall_grass", "tall_grass"}, {"tall grass", "tall_grass"}, {"wheat", "crops"}, {"corn", "crops"}, {"crop", "crops"},
        {"bush", "shrubs"}, {"shrub", "shrubs"}, {"hedge", "shrubs"}, {"fern", "shrubs"}, {"foliage", "shrubs"},
        {"leaf", "leaves"}, {"leaves", "leaves"}, {"forest", "leaves"}, {"moss", "grass"}, {"grass", "grass"}, {"lawn", "grass"}, {"meadow", "grass"},
        {"dirt", "dirt"}, {"soil", "dirt"}, {"earth", "dirt"}, {"ground", "dirt"}, {"clay", "dirt"},
        {"asphalt", "asphalt"}, {"road", "asphalt"}, {"tarmac", "asphalt"}, {"concrete", "concrete"}, {"cement", "concrete"}, {"pavement", "concrete"},
        {"brick", "brick"}, {"tile", "tile"}, {"marble", "stone"}, {"granite", "stone"}, {"cobble", "stone"}, {"rock", "stone"}, {"stone", "stone"}, {"cliff", "stone"}, {"boulder", "stone"},
        {"carpet", "carpet"}, {"sheet", "sheet_metal"}, {"tin", "sheet_metal"}, {"corrugated", "sheet_metal"}, {"car", "sheet_metal"}, {"vehicle", "sheet_metal"},
        {"steel", "metal"}, {"iron", "metal"}, {"metal", "metal"}, {"chrome", "metal"}, {"copper", "metal"}, {"gold", "metal"}, {"armor", "metal"}, {"tank", "metal"}, {"barrel", "sheet_metal"},
        {"wood", "wood"}, {"plank", "wood"}, {"crate", "wood"}, {"door", "wood"}, {"tree", "wood"}, {"pine", "wood"}, {"oak", "wood"}, {"log", "wood"}, {"fence", "wood"},
        {"drywall", "drywall"}, {"plaster", "drywall"}, {"wall", "drywall"},
        {"window", "glass"}, {"glass", "glass"}, {"crystal", "glass"}, {"diamond", "glass"}, {"mirror", "glass"},
        {"plastic", "plastic"}, {"rubber", "rubber"}, {"tire", "rubber"}, {"carpet", "carpet"}, {"rug", "carpet"}, {"cloth", "cloth"}, {"fabric", "cloth"}, {"curtain", "cloth"},
    };
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : t + " ") {
        if (std::isalnum(static_cast<unsigned char>(c))) cur += c;
        else if (!cur.empty()) {
            tokens.push_back(cur);
            cur.clear();
        }
    }
    std::string joined;
    for (const std::string& tok : tokens) joined += (joined.empty() ? "" : " ") + tok;
    for (const Word& w : words) {
        const std::string word = w.word;
        if (word.find(' ') != std::string::npos || word.find('_') != std::string::npos) {
            std::string spaced = word;
            std::replace(spaced.begin(), spaced.end(), '_', ' ');
            if (joined.find(spaced) != std::string::npos) return w.surface;
            continue;
        }
        for (const std::string& tok : tokens)
            if (tok == word || tok == word + "s" || tok == word + "es" || (word.size() >= 4 && tok.rfind(word, 0) == 0)) return w.surface;
    }
    return std::string();
}

const std::vector<std::string>& surface_field_keys() {
    static const std::vector<std::string> all = {"name", "walk", "wade", "jump", "stamina", "slip", "ricochet", "ricochet_angle", "resistance", "slows_bullets", "noise", "volume", "soft", "step_sound", "impact", "impact_sound", "color"};
    return all;
}

bool parse_speed_change(const std::string& text_in, float& multiplier, float& minus) {
    std::string t;
    for (char c : text_in)
        if (!std::isspace(static_cast<unsigned char>(c))) t += c;
    t = lower(t);
    multiplier = 1.0f;
    minus = 0.0f;
    if (t.empty() || t == "none" || t == "normal" || t == "off") return true;
    const bool percent = !t.empty() && t.back() == '%';
    if (percent) t.pop_back();
    bool times = false;
    if (!t.empty() && (t[0] == 'x' || t[0] == '*')) {
        times = true;
        t.erase(0, 1);
    }
    for (const char* suffix : {"m/s", "mps", "ms"})
        if (t.size() > std::string(suffix).size() && t.compare(t.size() - std::string(suffix).size(), std::string::npos, suffix) == 0) t.erase(t.size() - std::string(suffix).size());
    char* end = nullptr;
    const float v = std::strtof(t.c_str(), &end);
    if (!end || *end != '\0') return false;
    if (percent) {
        multiplier = std::max(0.0f, v / 100.0f);
        return true;
    }
    if (v < 0.0f && !times) {
        minus = -v;
        return true;
    }
    multiplier = std::max(0.0f, v);
    return true;
}

std::string speed_change_text(float multiplier, float minus) {
    if (minus > 0.0f) return "-" + num(minus);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g%%", static_cast<double>(std::round(multiplier * 1000.0f) / 10.0f));
    return buf;
}

float ease_for(const std::string& ease_list, const std::string& surface) {
    if (ease_list.empty()) return 0.0f;
    std::istringstream in(ease_list);
    std::string part;
    float best = 0.0f;
    while (std::getline(in, part, ',')) {
        const std::size_t eq = part.find('=');
        std::string key = part.substr(0, eq);
        key.erase(std::remove_if(key.begin(), key.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)); }), key.end());
        const float v = eq == std::string::npos ? 1.0f : static_cast<float>(std::atof(part.c_str() + eq + 1));
        if (key == surface || key == "all" || (key == "snow" && surface == "deep_snow") || (key == "water" && surface == "water_shallow")) best = std::max(best, std::clamp(v, 0.0f, 1.0f));
    }
    return best;
}

bool set_surface_field(SurfaceDef& s, const std::string& key, const std::string& value) {
    const float x = static_cast<float>(std::atof(value.c_str()));
    const bool on = value == "1" || value == "true" || value == "yes" || value == "on";
    if (key == "name") s.name = value;
    else if (key == "walk") {
        float m = 1.0f, minus = 0.0f;
        if (!parse_speed_change(value, m, minus)) return false;
        s.walk = m;
        s.walk_minus = minus;
    } else if (key == "wade") {
        float m = 1.0f, minus = 0.0f;
        if (!parse_speed_change(value, m, minus)) return false;
        s.wade = m;
        if (minus > 0.0f) s.walk_minus = minus;
    } else if (key == "jump") s.jump = std::max(0.0f, x);
    else if (key == "stamina") s.stamina = std::max(0.0f, x);
    else if (key == "slip") s.slip = std::clamp(x, 0.0f, 1.0f);
    else if (key == "ricochet") s.ricochet = std::max(0.0f, x);
    else if (key == "ricochet_angle") s.ricochet_angle = std::clamp(x, 0.0f, 90.0f);
    else if (key == "resistance") s.resistance = std::max(0.0f, x);
    else if (key == "slows_bullets") s.slows_bullets = std::clamp(x, 0.0f, 1.0f);
    else if (key == "noise") s.noise = std::max(0.0f, x);
    else if (key == "volume") s.volume = on;
    else if (key == "soft") s.soft = on;
    else if (key == "step_sound") s.step_sound = value;
    else if (key == "impact") s.impact = value;
    else if (key == "impact_sound") s.impact_sound = value;
    else if (key == "color") {
        float r = 0.0f, g = 0.0f, b = 0.0f;
        if (std::sscanf(value.c_str(), "%f,%f,%f", &r, &g, &b) != 3) return false;
        s.color = {r, g, b};
    } else return false;
    return true;
}

std::string surface_field(const SurfaceDef& s, const std::string& key) {
    if (key == "id") return s.id;
    if (key == "name") return s.name;
    if (key == "walk") return speed_change_text(s.walk, s.walk_minus);
    if (key == "wade") return speed_change_text(s.wade, 0.0f);
    if (key == "jump") return num(s.jump);
    if (key == "stamina") return num(s.stamina);
    if (key == "slip") return num(s.slip);
    if (key == "ricochet") return num(s.ricochet);
    if (key == "ricochet_angle") return num(s.ricochet_angle);
    if (key == "resistance") return num(s.resistance);
    if (key == "slows_bullets") return num(s.slows_bullets);
    if (key == "noise") return num(s.noise);
    if (key == "volume") return s.volume ? "1" : "0";
    if (key == "soft") return s.soft ? "1" : "0";
    if (key == "step_sound") return s.step_sound;
    if (key == "impact") return s.impact;
    if (key == "impact_sound") return s.impact_sound;
    if (key == "color") return num(s.color.x) + "," + num(s.color.y) + "," + num(s.color.z);
    return std::string();
}

const SurfaceDef* SurfaceCatalog::find(const std::string& id) const {
    auto it = custom.find(id);
    if (it != custom.end()) return &it->second;
    return builtin_surface(id);
}

SurfaceDef& SurfaceCatalog::define(const std::string& id) {
    auto it = custom.find(id);
    if (it != custom.end()) return it->second;
    SurfaceDef s;
    if (const SurfaceDef* b = builtin_surface(id)) s = *b;
    else {
        s.id = id;
        s.name = id;
    }
    return custom.emplace(id, s).first->second;
}

} // namespace orchard
