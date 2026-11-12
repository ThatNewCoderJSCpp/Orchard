#ifndef ORCHARD_UNITS_HPP
#define ORCHARD_UNITS_HPP

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace orchard {

enum class UnitKind { weight, temperature, temperature_change, per_degree, speed, distance, impulse };

struct UnitDef {
    const char* name;
    const char* label;
    double scale;
    double offset;
};

inline const std::vector<UnitKind>& unit_kinds() {
    static const std::vector<UnitKind> all = {UnitKind::weight, UnitKind::temperature, UnitKind::temperature_change, UnitKind::per_degree, UnitKind::speed, UnitKind::distance, UnitKind::impulse};
    return all;
}

inline const char* to_string(UnitKind k) {
    switch (k) {
    case UnitKind::weight: return "weight";
    case UnitKind::temperature: return "temperature";
    case UnitKind::temperature_change: return "temperature_change";
    case UnitKind::per_degree: return "per_degree";
    case UnitKind::speed: return "speed";
    case UnitKind::distance: return "distance";
    case UnitKind::impulse: return "impulse";
    }
    return "weight";
}

inline const std::vector<UnitDef>& units_of(UnitKind k) {
    static const std::vector<UnitDef> weight = {{"kg", "kilograms (kg)", 1.0, 0.0}, {"g", "grams (g)", 0.001, 0.0}, {"lb", "pounds (lb)", 0.45359237, 0.0}, {"oz", "ounces (oz)", 0.028349523125, 0.0}, {"st", "stone (st)", 6.35029318, 0.0}, {"t", "tonnes (t)", 1000.0, 0.0}};
    static const std::vector<UnitDef> temperature = {{"C", "Celsius (C)", 1.0, 0.0}, {"F", "Fahrenheit (F)", 5.0 / 9.0, -32.0 * 5.0 / 9.0}, {"K", "Kelvin (K)", 1.0, -273.15}};
    static const std::vector<UnitDef> change = {{"C", "Celsius (C)", 1.0, 0.0}, {"F", "Fahrenheit (F)", 5.0 / 9.0, 0.0}, {"K", "Kelvin (K)", 1.0, 0.0}};
    static const std::vector<UnitDef> per_degree = {{"C", "per degree Celsius", 1.0, 0.0}, {"F", "per degree Fahrenheit", 9.0 / 5.0, 0.0}, {"K", "per kelvin", 1.0, 0.0}};
    static const std::vector<UnitDef> speed = {{"m/s", "metres per second (m/s)", 1.0, 0.0}, {"km/h", "kilometres per hour (km/h)", 1.0 / 3.6, 0.0}, {"mph", "miles per hour (mph)", 0.44704, 0.0}, {"ft/s", "feet per second (ft/s)", 0.3048, 0.0}, {"kn", "knots (kn)", 1852.0 / 3600.0, 0.0}};
    static const std::vector<UnitDef> distance = {{"m", "metres (m)", 1.0, 0.0}, {"cm", "centimetres (cm)", 0.01, 0.0}, {"mm", "millimetres (mm)", 0.001, 0.0}, {"km", "kilometres (km)", 1000.0, 0.0}, {"in", "inches (in)", 0.0254, 0.0}, {"ft", "feet (ft)", 0.3048, 0.0}, {"yd", "yards (yd)", 0.9144, 0.0}, {"mi", "miles (mi)", 1609.344, 0.0}};
    static const std::vector<UnitDef> impulse = {{"N*s", "newton seconds (N·s)", 1.0, 0.0}, {"lbf*s", "pound-force seconds (lbf·s)", 4.4482216152605, 0.0}, {"lb*ft/s", "pound feet per second (lb·ft/s)", 0.138254954376, 0.0}};
    switch (k) {
    case UnitKind::weight: return weight;
    case UnitKind::temperature: return temperature;
    case UnitKind::temperature_change: return change;
    case UnitKind::per_degree: return per_degree;
    case UnitKind::speed: return speed;
    case UnitKind::distance: return distance;
    case UnitKind::impulse: return impulse;
    }
    return weight;
}

inline const UnitDef* find_unit(UnitKind k, const std::string& name) {
    std::string n = name;
    for (char& c : n)
        if (c == '\xB7') c = '*';
    if (n == "N\xC2*s" || n == "Ns" || n == "kg*m/s") n = "N*s";
    for (const UnitDef& u : units_of(k))
        if (n == u.name) return &u;
    return nullptr;
}

inline const char* unit_short(UnitKind k, const std::string& name) {
    const UnitDef* u = find_unit(k, name);
    if (!u) u = &units_of(k).front();
    if (k == UnitKind::impulse) return u->name[0] == 'N' ? "N·s" : u->name[1] == 'b' && u->name[2] == 'f' ? "lbf·s" : "lb·ft/s";
    if (k == UnitKind::temperature || k == UnitKind::temperature_change) return u->name[0] == 'F' ? "°F" : u->name[0] == 'K' ? "K" : "°C";
    if (k == UnitKind::per_degree) return u->name[0] == 'F' ? "per °F" : u->name[0] == 'K' ? "per K" : "per °C";
    return u->name;
}

inline double to_si(double value, UnitKind k, const std::string& unit) {
    const UnitDef* u = find_unit(k, unit);
    return u ? value * u->scale + u->offset : value;
}

inline double from_si(double value, UnitKind k, const std::string& unit) {
    const UnitDef* u = find_unit(k, unit);
    return u ? (value - u->offset) / u->scale : value;
}

inline std::optional<std::pair<UnitKind, const UnitDef*>> any_unit(const std::string& name) {
    for (UnitKind k : {UnitKind::weight, UnitKind::temperature, UnitKind::speed, UnitKind::distance, UnitKind::impulse})
        if (const UnitDef* u = find_unit(k, name)) return std::make_pair(k, u);
    return std::nullopt;
}

inline std::optional<double> convert_units(double value, const std::string& from, const std::string& to) {
    const auto a = any_unit(from), b = any_unit(to);
    if (!a || !b || a->first != b->first) return std::nullopt;
    return from_si(to_si(value, a->first, from), b->first, to);
}

} // namespace orchard

#endif // ORCHARD_UNITS_HPP
