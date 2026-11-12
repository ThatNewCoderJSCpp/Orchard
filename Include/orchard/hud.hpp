#ifndef ORCHARD_HUD_HPP
#define ORCHARD_HUD_HPP

#include "items.hpp"

#include <string>
#include <vector>

namespace orchard {

struct HudRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    std::string kind;
    std::string list;
    int index = -1;
    int gx = -1;
    int gy = -1;
    std::string label;
    bool other = false;
    bool selected = false;

    bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

struct HudLayout {
    std::vector<HudRect> panels;
    std::vector<HudRect> cells;
    std::vector<HudRect> items;
    std::vector<HudRect> texts;
};

void hud_anchor(const std::string& anchor, float screen_w, float screen_h, float w, float h, float ox, float oy, float& x, float& y);
HudLayout hotbar_layout(const ItemCatalog& cat, const Inventory& inv, float screen_w, float screen_h);
HudLayout inventory_layout(const ItemCatalog& cat, const Inventory& inv, float screen_w, float screen_h, bool containers, const Inventory* other, const std::string& other_title);
const HudRect* hud_cell_at(const HudLayout& layout, float x, float y);
const HudRect* hud_item_at(const HudLayout& layout, float x, float y);
std::string amount_text(const ItemDef* def, float amount);

} // namespace orchard

#endif // ORCHARD_HUD_HPP
