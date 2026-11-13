#include <orchard/items.hpp>
#include <orchard/units.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <set>
#include <sstream>

namespace orchard {

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::vector<std::pair<std::string, std::string>> parse_filter(const std::string& filter) {
    std::vector<std::pair<std::string, std::string>> out;
    std::string part;
    std::istringstream in(filter);
    while (std::getline(in, part, ',')) {
        const std::size_t eq = part.find('=');
        if (eq == std::string::npos) {
            const std::string t = trim(part);
            if (!t.empty()) out.emplace_back("search", t);
            continue;
        }
        out.emplace_back(lower(trim(part.substr(0, eq))), trim(part.substr(eq + 1)));
    }
    return out;
}

std::vector<std::string> split_any(const std::string& text, char sep) {
    std::vector<std::string> out;
    std::string part;
    std::istringstream in(text);
    while (std::getline(in, part, sep)) {
        const std::string t = trim(part);
        if (!t.empty()) out.push_back(t);
    }
    return out;
}

} // namespace

const std::vector<std::pair<std::string, std::string>>& gun_actions() {
    static const std::vector<std::pair<std::string, std::string>> all = {
        {"semi", "Semi-automatic"},
        {"auto", "Automatic (select fire)"},
        {"open_bolt", "Open-bolt automatic"},
        {"burst", "Burst fire"},
        {"bolt", "Bolt action"},
        {"straight_pull", "Straight-pull bolt"},
        {"pump", "Pump action"},
        {"lever", "Lever action"},
        {"break", "Break action"},
        {"single_shot", "Single shot (falling block, rolling block)"},
        {"revolver_da", "Double-action revolver"},
        {"revolver_sa", "Single-action revolver"},
        {"muzzle", "Muzzle-loader"},
        {"belt", "Belt-fed"},
        {"rotary", "Rotary (Gatling)"},
        {"rocket", "Rocket or recoilless"},
        {"energy", "Energy weapon"},
    };
    return all;
}

const std::vector<std::pair<std::string, std::string>>& gun_classes() {
    static const std::vector<std::pair<std::string, std::string>> all = {
        {"pistol", "Pistol"},
        {"machine_pistol", "Machine pistol"},
        {"revolver", "Revolver"},
        {"derringer", "Derringer"},
        {"smg", "Submachine gun"},
        {"pdw", "Personal defense weapon"},
        {"carbine", "Carbine"},
        {"assault_rifle", "Assault rifle"},
        {"battle_rifle", "Battle rifle"},
        {"semi_rifle", "Semi-automatic rifle"},
        {"dmr", "Designated marksman rifle"},
        {"bolt_rifle", "Bolt-action rifle"},
        {"lever_rifle", "Lever-action rifle"},
        {"rimfire_rifle", "Rimfire rifle"},
        {"sniper", "Sniper rifle"},
        {"anti_materiel", "Anti-materiel rifle"},
        {"shotgun", "Shotgun"},
        {"lmg", "Light machine gun"},
        {"gpmg", "General-purpose machine gun"},
        {"hmg", "Heavy machine gun"},
        {"rotary", "Rotary machine gun"},
        {"grenade_launcher", "Grenade launcher"},
        {"rocket_launcher", "Rocket launcher"},
        {"flare", "Flare gun"},
        {"musket", "Musket or long gun (black powder)"},
        {"black_powder_pistol", "Black-powder pistol"},
        {"sci_fi", "Sci-fi"},
    };
    return all;
}

const char* gun_action_label(const std::string& action) {
    for (const auto& [id, label] : gun_actions())
        if (id == action) return label.c_str();
    return "Other";
}

const char* gun_class_label(const std::string& gun_class) {
    for (const auto& [id, label] : gun_classes())
        if (id == gun_class) return label.c_str();
    return "Other";
}

std::string gun_action_of(const ItemDef& g) {
    if (!g.action.empty()) return g.action;
    if (g.kind != ItemKind::gun) return std::string();
    if (g.feed == "cylinder") return "revolver_da";
    if (g.feed == "muzzle") return "muzzle";
    if (g.feed == "belt") return "belt";
    if (g.feed == "pool") return "energy";
    if (g.category.find("Bolt") != std::string::npos) return "bolt";
    if (g.category.find("Lever") != std::string::npos) return "lever";
    if (g.shape == "sawed_off") return "break";
    if ((g.shape == "shotgun" || g.shape == "sniper") && g.feed == "internal") return g.shape == "shotgun" ? "pump" : "bolt";
    if (g.category.find("Launchers") != std::string::npos) return "rocket";
    return g.automatic ? "auto" : "semi";
}

bool manual_action(const ItemDef& gun) {
    const std::string a = gun_action_of(gun);
    return a == "bolt" || a == "straight_pull" || a == "pump" || a == "lever" || a == "revolver_sa" || a == "muzzle" || gun.hammer;
}

int burst_size(const ItemDef& gun, const std::string& mode) {
    if (mode.rfind("burst", 0) != 0) return 0;
    if (mode.size() > 5) return std::max(1, std::atoi(mode.c_str() + 5));
    return std::max(1, gun.burst);
}

std::vector<std::string> fire_modes_of(const ItemDef& gun) {
    std::vector<std::string> out = split_any(gun.fire_modes, ',');
    if (out.empty()) out.push_back(gun.automatic ? "auto" : "semi");
    return out;
}

std::string gun_era(int year) {
    if (year <= 0) return "unknown";
    if (year < 1800) return "pre_1800";
    if (year < 1900) return "1800s";
    if (year < 1946) return "world_wars";
    if (year < 1990) return "cold_war";
    if (year < 2010) return "modern";
    if (year < 2100) return "current";
    return "future";
}

const std::vector<std::pair<std::string, std::string>>& gun_filter_keys() {
    static const std::vector<std::pair<std::string, std::string>> all = {
        {"class", "type (pistol, rifle, SMG...)"},
        {"action", "action"},
        {"ammo", "ammo it fires"},
        {"cartridge", "kind of cartridge (rimfire, handgun, rifle...)"},
        {"feed", "how it's fed"},
        {"fire", "fire modes"},
        {"country", "country"},
        {"era", "era"},
        {"maker", "maker"},
        {"capacity", "how many rounds it holds"},
        {"slide_lock", "whether it locks open when empty"},
    };
    return all;
}

std::string gun_filter_value(const ItemCatalog& cat, const ItemDef& g, const std::string& key) {
    if (key == "class") return g.gun_class;
    if (key == "action") return gun_action_of(g);
    if (key == "ammo") return g.ammo;
    if (key == "cartridge") {
        const ItemDef* a = cat.find(g.ammo);
        return a ? a->cartridge_class : std::string();
    }
    if (key == "feed") return g.feed;
    if (key == "fire") return g.fire_modes.empty() ? (g.automatic ? "auto" : "semi") : g.fire_modes;
    if (key == "country") return g.country;
    if (key == "era") return gun_era(g.year);
    if (key == "maker") return g.maker;
    if (key == "capacity") {
        const int m = g.magazine;
        if (m <= 1) return "1";
        if (m <= 6) return "2-6";
        if (m <= 10) return "7-10";
        if (m <= 20) return "11-20";
        if (m <= 40) return "21-40";
        if (m <= 100) return "41-100";
        return "100+";
    }
    if (key == "slide_lock") return g.slide_lock ? "yes" : "no";
    if (key == "category") return g.category;
    if (key == "id") return g.id;
    if (key == "name") return g.name;
    return item_field(g, key);
}

bool gun_matches(const ItemCatalog& cat, const ItemDef& g, const std::string& filter) {
    if (g.kind != ItemKind::gun) return false;
    for (const auto& [key, want_all] : parse_filter(filter)) {
        if (want_all.empty() || want_all == "any") continue;
        bool any = false;
        for (const std::string& want : split_any(want_all, '|')) {
            const std::string w = lower(want);
            if (key == "search") {
                const std::string hay = lower(g.name + " " + g.id + " " + g.maker + " " + g.country + " " + g.ammo + " " + g.gun_class + " " + g.action + " " + g.description);
                bool all_words = true;
                for (const std::string& word : split_any(w, ' '))
                    if (hay.find(word) == std::string::npos) all_words = false;
                any = any || all_words;
            } else if (key == "ammo") {
                const ItemDef* a = cat.find(want);
                if (a && a->kind == ItemKind::ammo) any = any || ammo_fits(g, *a);
                else any = any || lower(g.ammo) == w || lower(g.caliber) == w;
            } else if (key == "fire") {
                for (const std::string& m : fire_modes_of(g))
                    if (lower(m) == w) any = true;
            } else if (key == "min_year") {
                any = any || (g.year > 0 && g.year >= std::atoi(want.c_str()));
            } else if (key == "max_year") {
                any = any || (g.year > 0 && g.year <= std::atoi(want.c_str()));
            } else if (key == "min_capacity") {
                any = any || g.magazine >= std::atoi(want.c_str());
            } else if (key == "max_weight") {
                any = any || g.weight <= static_cast<float>(std::atof(want.c_str()));
            } else {
                any = any || lower(gun_filter_value(cat, g, key)) == w;
            }
        }
        if (!any) return false;
    }
    return true;
}

std::vector<const ItemDef*> find_guns(const ItemCatalog& cat, const std::string& filter) {
    std::vector<const ItemDef*> out;
    std::set<std::string> seen;
    for (const auto& [id, d] : cat.custom)
        if (gun_matches(cat, d, filter)) {
            out.push_back(&d);
            seen.insert(id);
        }
    for (const ItemDef& d : item_library())
        if (!seen.count(d.id) && gun_matches(cat, d, filter)) out.push_back(cat.find(d.id) ? cat.find(d.id) : &d);
    std::sort(out.begin(), out.end(), [](const ItemDef* a, const ItemDef* b) { return a->name < b->name; });
    return out;
}

std::vector<std::string> gun_filter_values(const ItemCatalog& cat, const std::string& key, const std::string& filter) {
    std::map<std::string, int> counts;
    for (const ItemDef* g : find_guns(cat, filter)) {
        if (key == "fire") {
            for (const std::string& m : fire_modes_of(*g)) ++counts[m];
            continue;
        }
        const std::string v = gun_filter_value(cat, *g, key);
        if (!v.empty()) ++counts[v];
    }
    std::vector<std::string> out;
    for (const auto& [v, n] : counts) out.push_back(v);
    if (key == "class" || key == "action") {
        const auto& order = key == "class" ? gun_classes() : gun_actions();
        std::stable_sort(out.begin(), out.end(), [&](const std::string& a, const std::string& b) {
            auto at = [&](const std::string& v) {
                for (std::size_t i = 0; i < order.size(); ++i)
                    if (order[i].first == v) return i;
                return order.size();
            };
            return at(a) < at(b);
        });
    }
    if (key == "era") {
        static const std::vector<std::string> order = {"pre_1800", "1800s", "world_wars", "cold_war", "modern", "current", "future", "unknown"};
        std::stable_sort(out.begin(), out.end(), [&](const std::string& a, const std::string& b) { return std::find(order.begin(), order.end(), a) < std::find(order.begin(), order.end(), b); });
    }
    if (key == "capacity") {
        static const std::vector<std::string> order = {"1", "2-6", "7-10", "11-20", "21-40", "41-100", "100+"};
        std::stable_sort(out.begin(), out.end(), [&](const std::string& a, const std::string& b) { return std::find(order.begin(), order.end(), a) < std::find(order.begin(), order.end(), b); });
    }
    return out;
}

std::string weight_text(float kg, const std::string& unit) {
    char buf[64];
    const std::string u = unit.empty() ? std::string("kg") : unit;
    if (u == "lb" || u == "oz") {
        const double lb = kg / 0.45359237;
        if (lb < 1.0 || u == "oz") std::snprintf(buf, sizeof(buf), "%.1f oz", lb * 16.0);
        else std::snprintf(buf, sizeof(buf), "%.2f lb", lb);
        return buf;
    }
    if (u == "st") {
        std::snprintf(buf, sizeof(buf), "%.2f st", kg / 6.35029318);
        return buf;
    }
    if (u == "t") {
        std::snprintf(buf, sizeof(buf), "%.3f t", kg / 1000.0);
        return buf;
    }
    if (u == "g" || kg < 1.0f) {
        std::snprintf(buf, sizeof(buf), kg * 1000.0f < 10.0f ? "%.1f g" : "%.0f g", static_cast<double>(kg) * 1000.0);
        return buf;
    }
    std::snprintf(buf, sizeof(buf), "%.2f kg", static_cast<double>(kg));
    return buf;
}

std::string gun_info(const ItemCatalog& cat, const std::string& id) {
    const ItemDef* g = cat.find(id);
    if (!g || g->kind != ItemKind::gun) return std::string();
    std::string out = g->name + "\n";
    out += std::string(gun_class_label(g->gun_class)) + ", " + gun_action_label(g->action) + "\n";
    if (const ItemDef* a = cat.find(g->ammo)) {
        out += "Fires " + a->name;
        std::string more;
        for (const ItemDef* o : ammo_for_gun(cat, *g))
            if (o != a) more += (more.empty() ? "" : ", ") + o->name;
        if (!more.empty()) out += " (also " + more + ")";
        out += "\n";
    }
    char buf[200];
    std::snprintf(buf, sizeof(buf), "Holds %d (%s)%s\n", g->magazine, g->feed.empty() ? "magazine" : g->feed.c_str(), g->slide_lock ? ", locks open when empty" : "");
    out += buf;
    std::string modes;
    for (const std::string& m : fire_modes_of(*g)) modes += (modes.empty() ? "" : ", ") + m;
    std::snprintf(buf, sizeof(buf), "Fire: %s, %.0f rounds a minute\n", modes.c_str(), static_cast<double>(g->rate * 60.0f));
    out += buf;
    std::snprintf(buf, sizeof(buf), "Damage %.0f, muzzle speed %.0f m/s, %s, %.2f m long\n", static_cast<double>(g->damage), static_cast<double>(g->speed), weight_text(g->weight).c_str(), static_cast<double>(g->length));
    out += buf;
    std::string from;
    if (!g->maker.empty()) from += g->maker;
    if (!g->country.empty()) from += (from.empty() ? "" : ", ") + g->country;
    if (g->year > 0) from += (from.empty() ? "" : ", ") + std::to_string(g->year);
    if (!from.empty()) out += from + "\n";
    std::string mags;
    int n = 0;
    for (const ItemDef* m : magazines_for_gun(cat, *g))
        if (n++ < 4) mags += (mags.empty() ? "" : ", ") + m->name;
    if (!mags.empty()) out += "Magazines: " + mags + (n > 4 ? " and more" : "") + "\n";
    return out;
}

} // namespace orchard
