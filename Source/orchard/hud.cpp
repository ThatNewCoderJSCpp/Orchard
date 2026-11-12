#include <orchard/hud.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace orchard {

void hud_anchor(const std::string& a, float sw, float sh, float w, float h, float ox, float oy, float& x, float& y) {
    x = (sw - w) * 0.5f;
    y = (sh - h) * 0.5f;
    if (a.find("left") != std::string::npos) x = 12.0f;
    if (a.find("right") != std::string::npos) x = sw - w - 12.0f;
    if (a.rfind("top", 0) == 0) y = 12.0f;
    if (a.rfind("bottom", 0) == 0) y = sh - h - 12.0f;
    x += ox;
    y += oy;
}

std::string amount_text(const ItemDef* def, float amount) {
    char buf[32];
    if (def && def->continuous) {
        std::snprintf(buf, sizeof(buf), amount < 10.0f ? "%.2f" : "%.1f", static_cast<double>(amount));
        return std::string(buf) + (def->unit.empty() ? "" : " " + def->unit);
    }
    std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(std::round(amount * 100.0f) / 100.0f));
    return buf;
}

HudLayout hotbar_layout(const ItemCatalog& cat, const Inventory& inv, float sw, float sh) {
    HudLayout out;
    if (!inv.enabled || !inv.hotbar || inv.hotbar_slots <= 0) return out;
    const int n = inv.hotbar_slots;
    const float c = std::max(8.0f, inv.hotbar_cell), p = std::max(0.0f, inv.hotbar_padding);
    const float w = inv.hotbar_vertical ? c + p * 2.0f : n * c + (n + 1) * p;
    const float h = inv.hotbar_vertical ? n * c + (n + 1) * p : c + p * 2.0f;
    float x = 0.0f, y = 0.0f;
    hud_anchor(inv.hotbar_anchor, sw, sh, w, h, inv.hotbar_x, inv.hotbar_y, x, y);
    HudRect panel;
    panel.x = x;
    panel.y = y;
    panel.w = w;
    panel.h = h;
    panel.kind = "hotbar";
    out.panels.push_back(panel);
    for (int i = 0; i < n; ++i) {
        HudRect r;
        r.x = inv.hotbar_vertical ? x + p : x + p + i * (c + p);
        r.y = inv.hotbar_vertical ? y + p + i * (c + p) : y + p;
        r.w = r.h = c;
        r.kind = "cell";
        r.index = i;
        r.selected = i == inv.selected;
        const bool own = inv.hotbar_own && inv.style == InventoryStyle::slots;
        r.list = own ? "hotbar" : inv.style == InventoryStyle::slots ? "items" : "bind";
        out.cells.push_back(r);
        const ItemStack* s = nullptr;
        if (own && i < static_cast<int>(inv.hotbar_items.size())) s = &inv.hotbar_items[static_cast<std::size_t>(i)];
        else if (!own && inv.style == InventoryStyle::slots && i < static_cast<int>(inv.items.size())) s = &inv.items[static_cast<std::size_t>(i)];
        if (s && !s->empty()) {
            HudRect it = r;
            it.kind = "item";
            it.label = s->item;
            out.items.push_back(it);
        } else if (!own && inv.style != InventoryStyle::slots && i < static_cast<int>(inv.binds.size()) && !inv.binds[static_cast<std::size_t>(i)].empty()) {
            HudRect it = r;
            it.kind = "bind";
            it.label = inv.binds[static_cast<std::size_t>(i)];
            out.items.push_back(it);
        }
        static_cast<void>(cat);
    }
    return out;
}

namespace {

void grid_panel(HudLayout& out, const ItemCatalog& cat, const std::vector<ItemStack>& items, const std::string& list, int cols, int rows, float x, float y, float c, float p, bool other) {
    for (int gy = 0; gy < rows; ++gy)
        for (int gx = 0; gx < cols; ++gx) {
            HudRect r;
            r.x = x + p + gx * (c + p);
            r.y = y + p + gy * (c + p);
            r.w = r.h = c;
            r.kind = "cell";
            r.list = list;
            r.gx = gx;
            r.gy = gy;
            r.index = -1;
            r.other = other;
            out.cells.push_back(r);
        }
    for (std::size_t i = 0; i < items.size(); ++i) {
        const ItemStack& s = items[i];
        if (s.empty() || s.x < 0) continue;
        HudRect r;
        r.x = x + p + s.x * (c + p);
        r.y = y + p + s.y * (c + p);
        r.w = stack_w(cat, s) * (c + p) - p;
        r.h = stack_h(cat, s) * (c + p) - p;
        r.kind = "item";
        r.list = list;
        r.index = static_cast<int>(i);
        r.label = s.item;
        r.other = other;
        out.items.push_back(r);
    }
}

void slot_panel(HudLayout& out, const std::vector<ItemStack>& items, const std::string& list, int cols, int count, float x, float y, float c, float p, bool other) {
    for (int i = 0; i < count; ++i) {
        HudRect r;
        r.x = x + p + (i % cols) * (c + p);
        r.y = y + p + (i / cols) * (c + p);
        r.w = r.h = c;
        r.kind = "cell";
        r.list = list;
        r.index = i;
        r.other = other;
        out.cells.push_back(r);
        if (i < static_cast<int>(items.size()) && !items[static_cast<std::size_t>(i)].empty()) {
            HudRect it = r;
            it.kind = "item";
            it.label = items[static_cast<std::size_t>(i)].item;
            out.items.push_back(it);
        }
    }
}

float list_panel(HudLayout& out, const ItemCatalog& cat, const std::vector<ItemStack>& items, const std::string& list, float x, float y, float w, float row, bool other, bool pouches) {
    float yy = y;
    std::vector<std::pair<std::string, std::size_t>> order;
    for (std::size_t i = 0; i < items.size(); ++i)
        if (!items[i].empty()) {
            const ItemDef* d = cat.find(items[i].item);
            order.emplace_back(pouches && d ? std::string(to_string(d->kind)) : std::string(), i);
        }
    std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::string group = "\x01";
    for (const auto& [g, i] : order) {
        if (pouches && g != group) {
            HudRect t;
            t.x = x + 6.0f;
            t.y = yy;
            t.w = w - 12.0f;
            t.h = row * 0.8f;
            t.kind = "heading";
            t.label = g;
            out.texts.push_back(t);
            yy += t.h;
            group = g;
        }
        HudRect r;
        r.x = x + 4.0f;
        r.y = yy;
        r.w = w - 8.0f;
        r.h = row - 2.0f;
        r.kind = "item";
        r.list = list;
        r.index = static_cast<int>(i);
        r.label = items[i].item;
        r.other = other;
        out.items.push_back(r);
        HudRect cell = r;
        cell.kind = "cell";
        out.cells.push_back(cell);
        yy += row;
    }
    HudRect drop;
    drop.x = x + 4.0f;
    drop.y = yy;
    drop.w = w - 8.0f;
    drop.h = row - 2.0f;
    drop.kind = "cell";
    drop.list = list;
    drop.index = -1;
    drop.other = other;
    out.cells.push_back(drop);
    return yy + row - y;
}

} // namespace

HudLayout inventory_layout(const ItemCatalog& cat, const Inventory& inv, float sw, float sh, bool containers, const Inventory* other, const std::string& other_title) {
    HudLayout out;
    if (!inv.enabled) return out;
    const float c = std::max(8.0f, inv.cell), p = std::max(0.0f, inv.padding);
    const float title = 22.0f;
    float main_w = 0.0f, main_h = 0.0f;
    const bool listy = inv.style == InventoryStyle::weight || inv.style == InventoryStyle::volume || inv.style == InventoryStyle::pouches;
    if (listy) {
        main_w = std::max(260.0f, c * 6.0f);
        std::size_t n = 0;
        for (const ItemStack& s : inv.items) n += s.empty() ? 0 : 1;
        main_h = title + (static_cast<float>(n) + 2.0f + (inv.style == InventoryStyle::pouches ? 6.0f : 0.0f)) * c * 0.6f + 8.0f;
    } else {
        main_w = inv.columns * c + (inv.columns + 1) * p;
        main_h = title + inv.rows * c + (inv.rows + 1) * p;
        if (inv.style == InventoryStyle::slots && inv.hotbar && inv.hotbar_own) main_h += c + p * 3.0f;
    }
    const float equip_w = inv.equipment ? c * 3.0f + p * 4.0f : 0.0f;
    std::vector<std::pair<std::string, const ItemStack*>> bags;
    if (containers)
        for (const auto& [slot, s] : inv.worn) {
            if (s.empty() || slot == "mainhand" || slot == "offhand") continue;
            const ItemDef* d = cat.find(s.item);
            if (d && d->slots_w > 0 && d->slots_h > 0) bags.emplace_back(slot, &s);
        }
    float bags_w = 0.0f, bags_h = 0.0f;
    for (const auto& [slot, s] : bags) {
        const ItemDef* d = cat.find(s->item);
        bags_w = std::max(bags_w, d->slots_w * c + (d->slots_w + 1) * p);
        bags_h += title + d->slots_h * c + (d->slots_h + 1) * p + p;
    }
    const float total_w = equip_w + (equip_w > 0.0f ? 12.0f : 0.0f) + main_w + (bags.empty() ? 0.0f : 12.0f + bags_w);
    const float total_h = std::max({main_h, bags_h, inv.equipment ? title + 9.0f * (c * 0.8f + p) : 0.0f});
    float other_w = 0.0f;
    if (other) {
        const bool olisty = other->style == InventoryStyle::weight || other->style == InventoryStyle::volume || other->style == InventoryStyle::pouches;
        other_w = (olisty ? std::max(260.0f, c * 6.0f) : other->columns * c + (other->columns + 1) * p) + 16.0f;
    }
    // Too big for the screen (a small window, or a chest open beside it): shrink the cells to fit.
    const float need_w = total_w + other_w, room_w = sw - 16.0f, room_h = sh - 16.0f;
    if ((need_w > room_w || total_h > room_h) && c > 16.5f) {
        const float k = std::min(room_w / std::max(1.0f, need_w), room_h / std::max(1.0f, total_h)) * 0.98f;
        Inventory smaller = inv;
        smaller.cell = std::max(16.0f, c * k);
        smaller.padding = std::max(1.0f, p * k);
        return inventory_layout(cat, smaller, sw, sh, containers, other, other_title);
    }
    float x0 = 0.0f, y0 = 0.0f;
    hud_anchor(inv.anchor, sw, sh, total_w + other_w, total_h, inv.offset_x, inv.offset_y, x0, y0);
    float x = x0;
    if (inv.equipment) {
        HudRect panel;
        panel.x = x;
        panel.y = y0;
        panel.w = equip_w;
        panel.h = total_h;
        panel.kind = "panel";
        panel.label = "Equipment";
        out.panels.push_back(panel);
        const float ec = c * 0.8f;
        const std::vector<std::string> left = {"head", "eyes", "face", "neck", "shoulders", "torso", "vest", "back", "over"};
        const std::vector<std::string> right = {"mainhand", "offhand", "hands", "waist", "legs", "feet", "ring", "ring2", "side"};
        const std::vector<std::string> third = {"hip", "thigh", "sling", "back_slot"};
        auto column = [&](const std::vector<std::string>& slots, float cx) {
            for (std::size_t i = 0; i < slots.size(); ++i) {
                HudRect r;
                r.x = cx;
                r.y = y0 + title + p + static_cast<float>(i) * (ec + p);
                r.w = r.h = ec;
                r.kind = "cell";
                r.list = "worn:" + slots[i];
                r.label = slots[i];
                out.cells.push_back(r);
                const auto it = inv.worn.find(slots[i]);
                if (it != inv.worn.end() && !it->second.empty()) {
                    HudRect item = r;
                    item.kind = "item";
                    item.label = it->second.item;
                    out.items.push_back(item);
                }
            }
        };
        column(left, x + p);
        column(right, x + p * 2.0f + ec);
        column(third, x + p * 3.0f + ec * 2.0f);
        float hy = y0 + title + p + 4.0f * (ec + p) + p;
        for (const auto& [slot, s] : inv.holstered) {
            if (hy + ec > y0 + total_h) break;
            HudRect r;
            r.x = x + p * 3.0f + ec * 2.0f;
            r.y = hy;
            r.w = r.h = ec;
            r.kind = "cell";
            r.list = "holster:" + slot;
            r.label = "holster";
            out.cells.push_back(r);
            if (!s.empty()) {
                HudRect item = r;
                item.kind = "item";
                item.label = s.item;
                out.items.push_back(item);
            }
            hy += ec + p;
        }
        x += equip_w + 12.0f;
    }
    HudRect main;
    main.x = x;
    main.y = y0;
    main.w = main_w;
    main.h = listy ? total_h : main_h;
    main.kind = "panel";
    main.label = inv.title;
    out.panels.push_back(main);
    if (listy) {
        list_panel(out, cat, inv.items, "items", x, y0 + title, main_w, c * 0.6f, false, inv.style == InventoryStyle::pouches);
    } else if (inv.style == InventoryStyle::grid) {
        grid_panel(out, cat, inv.items, "items", inv.columns, inv.rows, x, y0 + title, c, p, false);
    } else {
        slot_panel(out, inv.items, "items", inv.columns, inv.columns * inv.rows, x, y0 + title, c, p, false);
        if (inv.hotbar && inv.hotbar_own) slot_panel(out, inv.hotbar_items, "hotbar", inv.hotbar_slots, inv.hotbar_slots, x, y0 + title + inv.rows * (c + p) + p * 2.0f, c, p, false);
    }
    x += main_w + 12.0f;
    float by = y0;
    for (const auto& [slot, s] : bags) {
        const ItemDef* d = cat.find(s->item);
        HudRect panel;
        panel.x = x;
        panel.y = by;
        panel.w = d->slots_w * c + (d->slots_w + 1) * p;
        panel.h = title + d->slots_h * c + (d->slots_h + 1) * p;
        panel.kind = "panel";
        panel.label = d->name;
        out.panels.push_back(panel);
        grid_panel(out, cat, s->contents, "container:" + slot, d->slots_w, d->slots_h, x, by + title, c, p, false);
        by += panel.h + p;
    }
    if (other) {
        const bool olisty = other->style == InventoryStyle::weight || other->style == InventoryStyle::volume || other->style == InventoryStyle::pouches;
        const float ow = olisty ? std::max(260.0f, c * 6.0f) : other->columns * c + (other->columns + 1) * p;
        const float oh = olisty ? total_h : title + other->rows * c + (other->rows + 1) * p;
        float ox = x0 + total_w + 16.0f;
        if (ox + ow > sw) ox = std::max(4.0f, x0 - ow - 16.0f);
        HudRect panel;
        panel.x = ox;
        panel.y = y0;
        panel.w = ow;
        panel.h = oh;
        panel.kind = "panel";
        panel.label = other_title;
        panel.other = true;
        out.panels.push_back(panel);
        if (olisty) list_panel(out, cat, other->items, "items", ox, y0 + title, ow, c * 0.6f, true, other->style == InventoryStyle::pouches);
        else if (other->style == InventoryStyle::grid) grid_panel(out, cat, other->items, "items", other->columns, other->rows, ox, y0 + title, c, p, true);
        else slot_panel(out, other->items, "items", other->columns, other->columns * other->rows, ox, y0 + title, c, p, true);
    }
    return out;
}

const HudRect* hud_cell_at(const HudLayout& layout, float x, float y) {
    for (const HudRect& r : layout.cells)
        if (r.contains(x, y)) return &r;
    return nullptr;
}

const HudRect* hud_item_at(const HudLayout& layout, float x, float y) {
    for (auto it = layout.items.rbegin(); it != layout.items.rend(); ++it)
        if (it->contains(x, y)) return &*it;
    return nullptr;
}

} // namespace orchard
