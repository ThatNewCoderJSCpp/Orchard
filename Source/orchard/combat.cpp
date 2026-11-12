#include <orchard/gameplay.hpp>

#include <orchard/effects.hpp>
#include <orchard/scripting.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

namespace orchard {

namespace {

constexpr float deg = 0.01745329252f;

Quat world_rotation(const Scene& s, Entity e) {
    Quat q{0.0f, 0.0f, 0.0f, 1.0f};
    for (Entity at = e; s.alive(at); at = s.parent(at))
        if (const Transform* t = s.transform(at)) q = t->rotation * q;
    return normalize(q);
}

std::vector<std::string> words(const std::string& text) {
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

bool listed(const std::string& list, const std::string& word) {
    for (const std::string& w : words(list))
        if (w == word || (w == "all" && word != "true")) return true;
    return false;
}

Vec3 perpendicular(Vec3 d) {
    const Vec3 a = std::fabs(d.y) < 0.9f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
    return normalize(cross(d, a));
}

struct Anchor {
    Vec3 at;
    Vec3 euler;
};

Anchor anchor_of(const std::string& slot) {
    if (slot == "head") return {{0.0f, 1.72f, 0.0f}, {}};
    if (slot == "eyes") return {{0.0f, 1.66f, 0.1f}, {}};
    if (slot == "face") return {{0.0f, 1.6f, 0.1f}, {}};
    if (slot == "neck") return {{0.0f, 1.5f, 0.02f}, {}};
    if (slot == "shoulders") return {{0.0f, 1.42f, 0.0f}, {}};
    if (slot == "torso") return {{0.0f, 1.18f, 0.0f}, {}};
    if (slot == "vest") return {{0.0f, 1.2f, 0.02f}, {}};
    if (slot == "over") return {{0.0f, 1.0f, 0.0f}, {}};
    if (slot == "back") return {{0.0f, 1.22f, -0.2f}, {}};
    if (slot == "hands") return {{0.25f, 0.72f, 0.0f}, {}};
    if (slot == "waist") return {{0.0f, 0.92f, 0.0f}, {}};
    if (slot == "legs") return {{0.0f, 0.55f, 0.0f}, {}};
    if (slot == "feet") return {{0.0f, 0.05f, 0.04f}, {}};
    if (slot == "ring" || slot == "ring2") return {{0.25f, 0.66f, 0.0f}, {}};
    if (slot == "hip") return {{0.21f, 0.86f, 0.03f}, {90.0f, 0.0f, 0.0f}};
    if (slot == "thigh") return {{0.17f, 0.66f, 0.06f}, {90.0f, 0.0f, 0.0f}};
    if (slot == "sling") return {{0.0f, 1.15f, -0.2f}, {0.0f, 90.0f, 50.0f}};
    if (slot == "back_slot") return {{0.0f, 1.3f, -0.22f}, {0.0f, 90.0f, -45.0f}};
    if (slot == "side") return {{-0.24f, 0.95f, 0.0f}, {}};
    if (slot == "mainhand") return {{0.27f, 1.02f, 0.32f}, {}};
    if (slot == "offhand") return {{-0.27f, 1.02f, 0.28f}, {}};
    return {{0.0f, 1.0f, 0.0f}, {}};
}

} // namespace

ItemCatalog& Gameplay::catalog() {
    static ItemCatalog none;
    return m_scene ? m_scene->items() : none;
}

const ItemDef* Gameplay::item_def(const std::string& id) const {
    return m_scene ? m_scene->items().find(id) : builtin_item(id);
}

bool Gameplay::define_item(const std::string& id, const std::string& key, const std::string& value) {
    if (!m_scene || id.empty()) return false;
    ItemDef& d = m_scene->items().define(id);
    return key.empty() || set_item_field(d, key, value);
}

Entity Gameplay::inventory_owner(Entity e) const {
    if (!m_scene) return Entity{};
    for (Entity at = e; m_scene->alive(at); at = m_scene->parent(at))
        if (m_scene->inventory(at)) return at;
    return Entity{};
}

Gameplay::Carry& Gameplay::carry(Entity e) {
    return m_carry[e.id()];
}

const Gameplay::Carry* Gameplay::carry_of(Entity e) const {
    const auto it = m_carry.find(e.id());
    return it == m_carry.end() ? nullptr : &it->second;
}

float Gameplay::give(Entity e, const std::string& item, float amount) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return 0.0f;
    Inventory& inv = *m_scene->inventory(o);
    const float added = inventory_add(m_scene->items(), inv, item, amount);
    if (added > 0.0f && inv.hotbar && !inv.hotbar_own && inv.style != InventoryStyle::slots) {
        const ItemDef* d = item_def(item);
        if (d && (d->weapon() || d->kind == ItemKind::potion || d->kind == ItemKind::medical || d->kind == ItemKind::food)) {
            if (static_cast<int>(inv.binds.size()) < inv.hotbar_slots) inv.binds.resize(static_cast<std::size_t>(inv.hotbar_slots));
            if (std::find(inv.binds.begin(), inv.binds.end(), item) == inv.binds.end())
                for (std::string& b : inv.binds)
                    if (b.empty()) {
                        b = item;
                        break;
                    }
        }
    }
    return added;
}

float Gameplay::take(Entity e, const std::string& item, float amount) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return 0.0f;
    return inventory_take(m_scene->items(), *m_scene->inventory(o), item, amount);
}

float Gameplay::count(Entity e, const std::string& item) const {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return 0.0f;
    return inventory_count(*m_scene->inventory(o), item);
}

std::vector<ItemStack>* Gameplay::list_of(Entity owner, const std::string& list) {
    if (!m_scene || !m_scene->alive(owner) || !m_scene->inventory(owner)) return nullptr;
    Inventory& inv = *m_scene->inventory(owner);
    if (list == "items") return &inv.items;
    if (list == "hotbar") return &inv.hotbar_items;
    if (list.rfind("container:", 0) == 0) {
        const auto it = inv.worn.find(list.substr(10));
        return it == inv.worn.end() ? nullptr : &it->second.contents;
    }
    return nullptr;
}

ItemStack* Gameplay::stack_at(Entity e, const std::string& list, int index) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return nullptr;
    Inventory& inv = *m_scene->inventory(o);
    if (list.rfind("worn:", 0) == 0) return &inv.worn[list.substr(5)];
    if (list.rfind("holster:", 0) == 0) return &inv.holstered[list.substr(8)];
    if (list == "mainhand" || list == "offhand") return &inv.worn[list];
    std::vector<ItemStack>* l = list_of(o, list);
    if (!l || index < 0 || index >= static_cast<int>(l->size())) return nullptr;
    return &(*l)[static_cast<std::size_t>(index)];
}

bool Gameplay::fits_list(Entity owner, const std::string& list, int index, const ItemStack& s, int x, int y) const {
    static_cast<void>(index);
    const Inventory* inv = m_scene ? m_scene->inventory(owner) : nullptr;
    if (!inv) return false;
    const ItemDef* d = item_def(s.item);
    if (!d) return false;
    if (list.rfind("worn:", 0) == 0) {
        const std::string slot = list.substr(5);
        if (slot == "mainhand" || slot == "offhand") return true;
        return d->wear == slot || ((slot == "ring" || slot == "ring2") && d->wear == "ring");
    }
    if (list.rfind("holster:", 0) == 0) {
        const std::string slot = list.substr(8);
        const std::size_t colon = slot.find(':');
        const auto it = inv->worn.find(slot.substr(0, colon));
        if (it == inv->worn.end() || it->second.empty()) return !inv->require_holsters;
        const ItemDef* h = item_def(it->second.item);
        return h && fits_holster(*h, *d);
    }
    if (list.rfind("container:", 0) == 0) {
        const auto it = inv->worn.find(list.substr(10));
        if (it == inv->worn.end() || it->second.empty()) return false;
        const ItemDef* c = item_def(it->second.item);
        if (!c || c->slots_w <= 0) return false;
        const int w = s.rotated ? d->grid_h : d->grid_w, h = s.rotated ? d->grid_w : d->grid_h;
        return x < 0 || grid_fits(m_scene->items(), it->second.contents, c->slots_w, c->slots_h, x, y, w, h);
    }
    if (inv->style == InventoryStyle::grid && list == "items") {
        const int w = s.rotated ? d->grid_h : d->grid_w, h = s.rotated ? d->grid_w : d->grid_h;
        return x < 0 || grid_fits(m_scene->items(), inv->items, inv->columns, inv->rows, x, y, w, h);
    }
    return true;
}

bool Gameplay::move_item(Entity from, const std::string& from_list, int from_index, Entity to, const std::string& to_list, int to_index, float amount, int x, int y, bool rotate) {
    const Entity fo = inventory_owner(from), to_owner = inventory_owner(to);
    if (!m_scene || !m_scene->alive(fo) || !m_scene->alive(to_owner)) return false;
    ItemStack* src = stack_at(fo, from_list, from_index);
    if (!src || src->empty()) return false;
    const ItemDef* def = item_def(src->item);
    if (!def) return false;
    Inventory& dst_inv = *m_scene->inventory(to_owner);
    const float cap = stack_cap(dst_inv, *def);
    if (amount < 0.0f || amount > src->amount) amount = src->amount;
    if (amount <= 0.0f) return false;
    ItemStack moving = *src;
    moving.amount = amount;
    if (rotate) moving.rotated = !moving.rotated;
    if (amount < src->amount) moving.contents.clear();
    const bool same_place = fo == to_owner && from_list == to_list && from_index == to_index;
    if (same_place && x < 0 && !rotate) return false;
    if (to_list.rfind("worn:", 0) == 0 || to_list.rfind("holster:", 0) == 0 || to_list == "mainhand" || to_list == "offhand") {
        const std::string dest = to_list == "mainhand" || to_list == "offhand" ? "worn:" + to_list : to_list;
        if (!fits_list(to_owner, dest, 0, moving, -1, -1)) return false;
        ItemStack* dst = stack_at(to_owner, dest, 0);
        if (!dst) return false;
        if (!dst->empty()) {
            if (dst->item == moving.item && dst->contents.empty() && moving.contents.empty() && dst->amount < cap) {
                const float put = std::min(cap - dst->amount, amount);
                dst->amount += put;
                src->amount -= put;
                if (src->amount <= 1e-6f) src->clear();
                return true;
            }
            if (amount < src->amount) return false;
            ItemStack back = *dst;
            if (!fits_list(fo, from_list, from_index, back, src->x, src->y)) return false;
            back.x = src->x;
            back.y = src->y;
            *dst = moving;
            dst->x = dst->y = -1;
            *src = back;
            return true;
        }
        if (moving.amount > cap) moving.amount = cap;
        *dst = moving;
        dst->x = dst->y = -1;
        src->amount -= moving.amount;
        if (src->amount <= 1e-6f) src->clear();
        if (m_scene->inventory(fo)->style != InventoryStyle::slots && from_list == "items") {
            std::vector<ItemStack>& l = m_scene->inventory(fo)->items;
            l.erase(std::remove_if(l.begin(), l.end(), [](const ItemStack& s) { return s.empty(); }), l.end());
        }
        return true;
    }
    std::vector<ItemStack>* dst_list = list_of(to_owner, to_list);
    if (!dst_list) return false;
    const bool slotted = (to_list == "items" && dst_inv.style == InventoryStyle::slots) || to_list == "hotbar";
    const bool gridded = (to_list == "items" && dst_inv.style == InventoryStyle::grid) || to_list.rfind("container:", 0) == 0;
    if (to_list == "items" && dst_inv.style == InventoryStyle::slots) dst_list->resize(static_cast<std::size_t>(std::max(0, dst_inv.columns * dst_inv.rows)));
    if (to_list == "hotbar") dst_list->resize(static_cast<std::size_t>(std::max(0, dst_inv.hotbar_slots)));
    if (fo != to_owner || from_list != to_list) {
        if (dst_inv.max_weight > 0.0f && to_list == "items" && inventory_weight(m_scene->items(), dst_inv) + stack_weight(m_scene->items(), moving) > dst_inv.max_weight + 1e-4f) return false;
        if (dst_inv.style == InventoryStyle::volume && dst_inv.max_volume > 0.0f && to_list == "items" && inventory_volume(m_scene->items(), dst_inv) + stack_volume(m_scene->items(), moving) > dst_inv.max_volume + 1e-4f) return false;
    }
    if (slotted) {
        if (to_index < 0 || to_index >= static_cast<int>(dst_list->size())) {
            to_index = -1;
            for (std::size_t i = 0; i < dst_list->size(); ++i)
                if ((*dst_list)[i].empty()) {
                    to_index = static_cast<int>(i);
                    break;
                }
            if (to_index < 0) return false;
        }
        ItemStack& dst = (*dst_list)[static_cast<std::size_t>(to_index)];
        if (&dst == src) return false;
        if (dst.empty()) {
            moving.amount = std::min(moving.amount, cap);
            moving.x = moving.y = -1;
            dst = moving;
            src->amount -= moving.amount;
        } else if (dst.item == moving.item && dst.contents.empty() && moving.contents.empty()) {
            const float put = std::min(cap - dst.amount, amount);
            if (put <= 0.0f) return false;
            dst.amount += put;
            src->amount -= put;
        } else {
            if (amount < src->amount) return false;
            ItemStack back = dst;
            if (!fits_list(fo, from_list, from_index, back, src->x, src->y)) return false;
            back.x = src->x;
            back.y = src->y;
            dst = moving;
            dst.x = dst.y = -1;
            *src = back;
            return true;
        }
        if (src->amount <= 1e-6f) src->clear();
        if (m_scene->inventory(fo)->style != InventoryStyle::slots && from_list == "items") {
            std::vector<ItemStack>& l = m_scene->inventory(fo)->items;
            l.erase(std::remove_if(l.begin(), l.end(), [](const ItemStack& s) { return s.empty(); }), l.end());
        }
        return true;
    }
    if (gridded) {
        int cols = dst_inv.columns, rows = dst_inv.rows;
        if (to_list.rfind("container:", 0) == 0) {
            const ItemStack& c = dst_inv.worn[to_list.substr(10)];
            const ItemDef* cd = item_def(c.item);
            if (!cd || cd->slots_w <= 0) return false;
            cols = cd->slots_w;
            rows = cd->slots_h;
        }
        const int w = moving.rotated ? def->grid_h : def->grid_w, h = moving.rotated ? def->grid_w : def->grid_h;
        const int ignore = same_place || (fo == to_owner && from_list == to_list) ? from_index : -1;
        if (x >= 0 && y >= 0) {
            for (std::size_t i = 0; i < dst_list->size(); ++i) {
                ItemStack& other = (*dst_list)[i];
                if (static_cast<int>(i) == ignore || other.empty() || other.item != moving.item || !other.contents.empty()) continue;
                if (x >= other.x && x < other.x + stack_w(m_scene->items(), other) && y >= other.y && y < other.y + stack_h(m_scene->items(), other)) {
                    const float put = std::min(cap - other.amount, amount);
                    if (put <= 0.0f) return false;
                    other.amount += put;
                    src->amount -= put;
                    if (src->amount <= 1e-6f) src->clear();
                    dst_list->erase(std::remove_if(dst_list->begin(), dst_list->end(), [](const ItemStack& s) { return s.empty(); }), dst_list->end());
                    if (std::vector<ItemStack>* fl = list_of(fo, from_list); fl && fl != dst_list && m_scene->inventory(fo)->style != InventoryStyle::slots) fl->erase(std::remove_if(fl->begin(), fl->end(), [](const ItemStack& s) { return s.empty(); }), fl->end());
                    return true;
                }
            }
            if (!grid_fits(m_scene->items(), *dst_list, cols, rows, x, y, w, h, ignore)) return false;
        } else {
            bool rot = false;
            std::vector<ItemStack> others = *dst_list;
            if (ignore >= 0 && ignore < static_cast<int>(others.size())) others[static_cast<std::size_t>(ignore)].clear();
            if (!grid_place(m_scene->items(), others, cols, rows, w, h, x, y, rot)) return false;
            if (rot) moving.rotated = !moving.rotated;
        }
        moving.amount = std::min(moving.amount, cap);
        moving.x = x;
        moving.y = y;
        if (ignore >= 0 && from_list == to_list && fo == to_owner && moving.amount >= src->amount) {
            src->x = x;
            src->y = y;
            src->rotated = moving.rotated;
            return true;
        }
        src->amount -= moving.amount;
        dst_list->push_back(moving);
    } else {
        bool merged = false;
        for (ItemStack& other : *dst_list) {
            if (&other == src || other.empty() || other.item != moving.item || !other.contents.empty() || !moving.contents.empty()) continue;
            const float put = std::min(cap - other.amount, moving.amount);
            if (put <= 0.0f) continue;
            other.amount += put;
            src->amount -= put;
            moving.amount -= put;
            merged = true;
            if (moving.amount <= 1e-6f) break;
        }
        if (moving.amount > 1e-6f && !(fo == to_owner && from_list == to_list)) {
            moving.amount = std::min(moving.amount, cap);
            moving.x = moving.y = -1;
            src->amount -= moving.amount;
            dst_list->push_back(moving);
        } else if (!merged) {
            return false;
        }
    }
    if (src->amount <= 1e-6f) src->clear();
    for (std::vector<ItemStack>* l : {list_of(fo, from_list), dst_list}) {
        if (!l) continue;
        const bool keep = (l == &m_scene->inventory(fo)->items && m_scene->inventory(fo)->style == InventoryStyle::slots) || l == &m_scene->inventory(fo)->hotbar_items || (l == &dst_inv.items && dst_inv.style == InventoryStyle::slots) || l == &dst_inv.hotbar_items;
        if (!keep) l->erase(std::remove_if(l->begin(), l->end(), [](const ItemStack& s) { return s.empty(); }), l->end());
    }
    return true;
}

Entity Gameplay::spawn_item(const std::string& item, float amount, Vec3 at, const std::string& look) {
    const ItemDef* d = item_def(item);
    if (!m_scene || !d || amount <= 0.0f) return Entity{};
    const bool flat = m_scene->dimension() == Dimension::two_d;
    const Entity e = m_scene->create(d->name);
    if (Transform* t = m_scene->transform(e)) t->position = at;
    Body& b = m_scene->add_body(e);
    b.type = BodyType::dynamic;
    const float len = std::max(0.06f, std::min(d->length, 2.5f));
    Collider* c = m_scene->add_collider(e);
    const bool long_item = d->length > 0.25f && (d->weapon() || d->kind == ItemKind::tool);
    c->size = long_item ? Vec3{0.08f, 0.08f, len} : Vec3{std::max(0.06f, std::cbrt(std::max(1e-4f, d->volume * 0.001f)) * 1.2f), std::max(0.06f, std::cbrt(std::max(1e-4f, d->volume * 0.001f)) * 1.2f), std::max(0.06f, std::cbrt(std::max(1e-4f, d->volume * 0.001f)) * 1.2f)};
    if (flat) c->size.z = 1.0f;
    c->mass = std::max(0.02f, d->weight * amount);
    c->lock(collider_lock_mass, true);
    c->friction = 0.6f;
    Pickup& p = m_scene->add_pickup(e);
    p.stack.item = item;
    p.stack.amount = amount;
    if (d->magazine > 0 && (d->kind == ItemKind::gun || d->kind == ItemKind::bow)) p.stack.loaded = static_cast<float>(d->magazine);
    p.look = look;
    m_scene->add_model(e).source = "Assets/Models/Items/" + item + "-" + look + ".glb";
    m_scene->add_tag(e, "pickup");
    if (look == "realistic" && (d->kind == ItemKind::potion || d->kind == ItemKind::drink)) {
        const Entity liquid = m_scene->create("Liquid", e);
        m_scene->add_model(liquid).source = "Assets/Models/Items/" + item + "-liquid.glb";
    }
    if (m_physics) m_physics->added(e);
    return e;
}

Entity Gameplay::drop_item(Entity e, const std::string& list, int index, float amount) {
    const Entity o = inventory_owner(e);
    ItemStack* s = stack_at(o, list, index);
    if (!s || s->empty()) return Entity{};
    if (amount < 0.0f || amount > s->amount) amount = s->amount;
    Vec3 origin, dir;
    aim_of(o, origin, dir);
    const Vec3 at = position(o) + Vec3{dir.x, 0.0f, dir.z} * 0.7f + Vec3{0.0f, 1.0f, 0.0f};
    ItemStack copy = *s;
    const Entity dropped = spawn_item(s->item, amount, at);
    if (!m_scene->alive(dropped)) return dropped;
    if (amount >= s->amount) m_scene->pickup(dropped)->stack = copy;
    m_scene->pickup(dropped)->stack.amount = amount;
    m_scene->pickup(dropped)->stack.x = m_scene->pickup(dropped)->stack.y = -1;
    s->amount -= amount;
    if (s->amount <= 1e-6f) s->clear();
    if (std::vector<ItemStack>* l = list_of(o, list); l && (list != "items" || m_scene->inventory(o)->style != InventoryStyle::slots) && list != "hotbar")
        l->erase(std::remove_if(l->begin(), l->end(), [](const ItemStack& st) { return st.empty(); }), l->end());
    if (m_physics && m_physics->has_body(dropped)) m_physics->set_velocity(dropped, dir * 2.0f + Vec3{0.0f, 1.0f, 0.0f});
    return dropped;
}

bool Gameplay::pick_up(Entity e, Entity pickup) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o) || !m_scene->alive(pickup)) return false;
    Pickup* p = m_scene->pickup(pickup);
    if (!p || p->stack.empty()) return false;
    Inventory& inv = *m_scene->inventory(o);
    float added = 0.0f;
    if (p->stack.contents.empty() && p->stack.durability == 1.0f && p->stack.loaded == 0.0f) {
        added = give(o, p->stack.item, p->stack.amount);
    } else {
        ItemStack unique = p->stack;
        const ItemDef* d = item_def(unique.item);
        if (!d) return false;
        if (inv.style == InventoryStyle::slots) {
            inv.items.resize(static_cast<std::size_t>(std::max(0, inv.columns * inv.rows)));
            for (ItemStack& s : inv.items)
                if (s.empty()) {
                    s = unique;
                    added = unique.amount;
                    break;
                }
        } else if (inv.style == InventoryStyle::grid) {
            int x = 0, y = 0;
            bool rot = false;
            if (grid_place(m_scene->items(), inv.items, inv.columns, inv.rows, d->grid_w, d->grid_h, x, y, rot)) {
                unique.x = x;
                unique.y = y;
                unique.rotated = rot;
                inv.items.push_back(unique);
                added = unique.amount;
            }
        } else if (inv.max_weight <= 0.0f || inventory_weight(m_scene->items(), inv) + stack_weight(m_scene->items(), unique) <= inv.max_weight) {
            inv.items.push_back(unique);
            added = unique.amount;
        }
    }
    if (added <= 0.0f) return false;
    p->stack.amount -= added;
    if (p->stack.amount > 1e-5f) return true;
    if (p->respawn > 0.0f) {
        m_respawns.emplace_back(pickup, m_now + p->respawn);
        if (Model* m = m_scene->model(pickup)) m->visible = false;
        p->stack.amount = 0.0f;
        p->stack.contents.clear();
        return true;
    }
    m_doomed.push_back(pickup);
    return true;
}

Entity Gameplay::nearest_pickup(Entity e, float radius) const {
    if (!m_scene) return Entity{};
    const Vec3 at = position(e);
    Entity best;
    float best_d = radius;
    for (Entity p : m_scene->entities()) {
        const Pickup* pk = m_scene->pickup(p);
        if (!pk || pk->stack.empty()) continue;
        const float d = length(position(p) - at);
        if (d <= best_d) {
            best_d = d;
            best = p;
        }
    }
    return best;
}

ItemStack* Gameplay::held(Entity e, const std::string& hand) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return nullptr;
    Inventory& inv = *m_scene->inventory(o);
    auto it = inv.worn.find(hand);
    if (it != inv.worn.end() && !it->second.empty()) return &it->second;
    if (hand != "mainhand" || !inv.hotbar || inv.selected < 0) return nullptr;
    if (inv.hotbar_own && inv.style == InventoryStyle::slots) {
        if (inv.selected < static_cast<int>(inv.hotbar_items.size()) && !inv.hotbar_items[static_cast<std::size_t>(inv.selected)].empty()) return &inv.hotbar_items[static_cast<std::size_t>(inv.selected)];
        return nullptr;
    }
    if (inv.style == InventoryStyle::slots && inv.selected < static_cast<int>(inv.items.size()) && inv.selected < inv.hotbar_slots && !inv.items[static_cast<std::size_t>(inv.selected)].empty()) return &inv.items[static_cast<std::size_t>(inv.selected)];
    return nullptr;
}

const ItemDef* Gameplay::held_def(Entity e, const std::string& hand) {
    ItemStack* s = held(e, hand);
    return s ? item_def(s->item) : nullptr;
}

bool Gameplay::hold(Entity e, const std::string& hand, const std::string& list, int index) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o) || (hand != "mainhand" && hand != "offhand")) return false;
    ItemStack* src = stack_at(o, list, index);
    if (!src || src->empty()) return false;
    Inventory& inv = *m_scene->inventory(o);
    if (!inv.worn[hand].empty() && !holster(o, hand)) return false;
    src = stack_at(o, list, index);
    if (!src || src->empty()) return false;
    const ItemDef* d = item_def(src->item);
    float amount = src->amount;
    if (d && d->weapon() && d->kind != ItemKind::throwable) amount = std::min(amount, 1.0f);
    if (!move_item(o, list, index, o, "worn:" + hand, 0, amount)) return false;
    if (d && d->two_handed && hand == "mainhand" && !inv.worn["offhand"].empty()) holster(o, "offhand");
    refresh_visuals(o);
    return true;
}

std::vector<std::string> Gameplay::holster_slots(Entity e) const {
    std::vector<std::string> out;
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return out;
    const Inventory& inv = *m_scene->inventory(o);
    for (const auto& [slot, s] : inv.worn) {
        if (s.empty() || slot == "mainhand" || slot == "offhand") continue;
        const ItemDef* d = item_def(s.item);
        if (!d || d->holster_count <= 0) continue;
        for (int i = 0; i < d->holster_count; ++i) out.push_back(slot + ":" + std::to_string(i));
    }
    if (!inv.require_holsters)
        for (int i = 0; i < 4; ++i) out.push_back("body:" + std::to_string(i));
    return out;
}

bool Gameplay::holster(Entity e, const std::string& hand) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return false;
    Inventory& inv = *m_scene->inventory(o);
    ItemStack& s = inv.worn[hand];
    if (s.empty()) {
        if (hand == "mainhand" && inv.hotbar && inv.selected >= 0) {
            inv.selected = -1;
            refresh_visuals(o);
            return true;
        }
        return false;
    }
    const ItemDef* d = item_def(s.item);
    if (!d) return false;
    for (const std::string& slot : holster_slots(o)) {
        if (!inv.holstered[slot].empty()) continue;
        if (slot.rfind("body:", 0) != 0 && !fits_list(o, "holster:" + slot, 0, s, -1, -1)) continue;
        if (slot.rfind("body:", 0) == 0 && (d->size >= ItemSize::large && inv.require_holsters)) continue;
        if (slot.rfind("body:", 0) == 0 && !d->weapon()) continue;
        inv.holstered[slot] = s;
        s.clear();
        refresh_visuals(o);
        return true;
    }
    if (inv.require_holsters && d->weapon() && d->size >= ItemSize::small) return false;
    const ItemStack copy = s;
    s.clear();
    const float back = give(o, copy.item, copy.amount);
    if (back + 1e-5f < copy.amount) {
        s = copy;
        s.amount = copy.amount - back;
        return false;
    }
    refresh_visuals(o);
    return true;
}

bool Gameplay::draw(Entity e, const std::string& slot, const std::string& hand) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return false;
    Inventory& inv = *m_scene->inventory(o);
    auto it = inv.holstered.find(slot);
    if (it == inv.holstered.end() || it->second.empty()) return false;
    if (!inv.worn[hand].empty() && !holster(o, hand)) return false;
    inv.worn[hand] = it->second;
    it->second.clear();
    if (hand == "mainhand") inv.selected = -1;
    refresh_visuals(o);
    return true;
}

bool Gameplay::equip(Entity e, const std::string& list, int index) {
    const Entity o = inventory_owner(e);
    ItemStack* s = stack_at(o, list, index);
    if (!s || s->empty()) return false;
    const ItemDef* d = item_def(s->item);
    if (!d) return false;
    if (d->wearable() && d->kind != ItemKind::tool) {
        std::string slot = d->wear;
        Inventory& inv = *m_scene->inventory(o);
        if (slot == "ring" && !inv.worn["ring"].empty()) slot = "ring2";
        if (!inv.worn[slot].empty() && !unequip(o, slot)) return false;
        s = stack_at(o, list, index);
        if (!s || s->empty()) return false;
        const bool ok = move_item(o, list, index, o, "worn:" + slot, 0, 1.0f);
        refresh_visuals(o);
        return ok;
    }
    if (d->has("offhand")) return hold(o, "offhand", list, index);
    return hold(o, "mainhand", list, index);
}

bool Gameplay::unequip(Entity e, const std::string& slot) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return false;
    Inventory& inv = *m_scene->inventory(o);
    auto it = inv.worn.find(slot);
    if (it == inv.worn.end() || it->second.empty()) return false;
    const ItemStack copy = it->second;
    it->second.clear();
    bool ok = false;
    if (copy.contents.empty()) {
        ok = give(o, copy.item, copy.amount) + 1e-5f >= copy.amount;
    } else {
        const ItemDef* d = item_def(copy.item);
        if (inv.style == InventoryStyle::grid && d) {
            int x = 0, y = 0;
            bool rot = false;
            if (grid_place(m_scene->items(), inv.items, inv.columns, inv.rows, d->grid_w, d->grid_h, x, y, rot)) {
                ItemStack placed = copy;
                placed.x = x;
                placed.y = y;
                placed.rotated = rot;
                inv.items.push_back(placed);
                ok = true;
            }
        } else if (inv.style == InventoryStyle::slots) {
            inv.items.resize(static_cast<std::size_t>(std::max(0, inv.columns * inv.rows)));
            for (ItemStack& s : inv.items)
                if (s.empty()) {
                    s = copy;
                    ok = true;
                    break;
                }
        } else {
            inv.items.push_back(copy);
            ok = true;
        }
    }
    if (!ok) {
        it->second = copy;
        return false;
    }
    refresh_visuals(o);
    return true;
}

bool Gameplay::select_slot(Entity e, int index) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return false;
    Inventory& inv = *m_scene->inventory(o);
    if (!inv.hotbar || index < 0 || index >= inv.hotbar_slots) return false;
    if (!inv.worn["mainhand"].empty() && !holster(o, "mainhand")) return false;
    if (!inv.hotbar_own && inv.style != InventoryStyle::slots) {
        if (index >= static_cast<int>(inv.binds.size()) || inv.binds[static_cast<std::size_t>(index)].empty()) return false;
        const std::string want = inv.binds[static_cast<std::size_t>(index)];
        for (const auto& [slot, s] : inv.holstered)
            if (s.item == want && !s.empty()) {
                const std::string name = slot;
                return draw(o, name, "mainhand");
            }
        for (std::size_t i = 0; i < inv.items.size(); ++i)
            if (inv.items[i].item == want && !inv.items[i].empty()) return hold(o, "mainhand", "items", static_cast<int>(i));
        return false;
    }
    inv.selected = index;
    carry(o).drawing = false;
    refresh_visuals(o);
    return true;
}

bool Gameplay::scroll_slot(Entity e, int delta) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return false;
    Inventory& inv = *m_scene->inventory(o);
    if (!inv.hotbar || !inv.hotbar_scroll || inv.hotbar_slots <= 0) return false;
    const int n = inv.hotbar_slots;
    const int cur = inv.selected < 0 ? 0 : inv.selected;
    return select_slot(o, ((cur + delta) % n + n) % n);
}

bool Gameplay::use_item(Entity e, const std::string& list, int index) {
    const Entity o = inventory_owner(e);
    ItemStack* s = stack_at(o, list, index);
    if (!s || s->empty()) return false;
    const ItemDef* d = item_def(s->item);
    if (!d) return false;
    switch (d->kind) {
    case ItemKind::potion:
    case ItemKind::food:
    case ItemKind::drink:
    case ItemKind::medical: {
        const bool ok = consume(o, *d, *s);
        if (s->amount <= 1e-6f) {
            s->clear();
            if (std::vector<ItemStack>* l = list_of(o, list); l && list != "hotbar" && !(list == "items" && m_scene->inventory(o)->style == InventoryStyle::slots))
                l->erase(std::remove_if(l->begin(), l->end(), [](const ItemStack& st) { return st.empty(); }), l->end());
        }
        return ok;
    }
    default: return equip(o, list, index);
    }
}

bool Gameplay::consume(Entity e, const ItemDef& d, ItemStack& s) {
    const Entity h = holder(e);
    const bool continuous = d.continuous;
    const float part = continuous ? std::min(s.amount, 0.25f) : 1.0f;
    const float k = continuous ? part : 1.0f;
    if (d.heal > 0.0f) heal(m_scene->alive(h) ? h : e, d.heal * k);
    if (d.food != 0.0f || d.saturation != 0.0f) eat(e, d.food * k, d.saturation * k);
    if (d.water != 0.0f) drink(e, d.water * k);
    if (d.mana_restore != 0.0f) add_stat(e, "mana", d.mana_restore * k);
    if (d.has("stamina")) set_stat(e, "stamina", stat(e, "max_stamina"));
    for (const std::string& c : words(d.cures)) remove_effect(m_scene->alive(h) ? h : e, c);
    if (!d.use_effect.empty() && (d.effect_chance <= 0.0f || random(0.0, 1.0) < d.effect_chance)) apply_effect(m_scene->alive(h) ? h : e, d.use_effect, d.use_time, d.use_strength);
    s.amount -= part;
    if (s.amount <= 1e-6f) s.amount = 0.0f;
    return true;
}

float Gameplay::worn_armor(Entity e, const std::string& type) const {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return 0.0f;
    float a = 0.0f;
    for (const auto& [slot, s] : m_scene->inventory(o)->worn) {
        if (s.empty() || slot == "mainhand" || slot == "offhand") continue;
        const ItemDef* d = item_def(s.item);
        if (!d || d->armor <= 0.0f) continue;
        if (d->armor_types.empty() ? armor_blocks(type) : listed(d->armor_types, type)) a += d->armor * std::max(0.1f, s.durability);
    }
    return a;
}

float Gameplay::worn_insulation(Entity e) const {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return 0.0f;
    float v = 0.0f;
    for (const auto& [slot, s] : m_scene->inventory(o)->worn) {
        if (s.empty() || slot == "mainhand" || slot == "offhand") continue;
        if (const ItemDef* d = item_def(s.item)) v += d->insulation;
    }
    return v;
}

float Gameplay::worn_carry_bonus(Entity e) const {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return 0.0f;
    float v = 0.0f;
    for (const auto& [slot, s] : m_scene->inventory(o)->worn) {
        if (s.empty() || slot == "mainhand" || slot == "offhand") continue;
        if (const ItemDef* d = item_def(s.item)) v += d->carry_bonus;
    }
    return v;
}

void Gameplay::set_aim(Entity e, Vec3 origin, Vec3 direction) {
    Carry& c = carry(e);
    c.aim_set = true;
    c.aim_origin = origin;
    if (length(direction) > 1e-5f) c.aim_dir = normalize(direction);
}

void Gameplay::aim_of(Entity e, Vec3& origin, Vec3& dir) const {
    const Carry* c = carry_of(e);
    if (c && c->aim_set) {
        origin = c->aim_origin;
        dir = c->aim_dir;
        return;
    }
    const bool flat = m_scene && m_scene->dimension() == Dimension::two_d;
    const Entity s = survivor(e);
    const float head = m_scene && m_scene->alive(s) ? m_scene->survival(s)->head_height * 0.9f : 1.45f;
    origin = position(e) + Vec3{0.0f, head, 0.0f};
    dir = m_scene ? rotate(world_rotation(*m_scene, e), Vec3{0.0f, 0.0f, 1.0f}) : Vec3{0.0f, 0.0f, 1.0f};
    if (flat) {
        dir = Vec3{dir.z >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f};
        if (const Carry* cc = carry_of(e); cc && cc->aim_set) dir = cc->aim_dir;
    }
    if (length(dir) < 1e-5f) dir = Vec3{0.0f, 0.0f, 1.0f};
    dir = normalize(dir);
}

bool Gameplay::set_stance(Entity e, const std::string& s) {
    if (s != "stand" && s != "crouch" && s != "prone") return false;
    carry(e).stance = s;
    return true;
}

std::string Gameplay::stance(Entity e) const {
    const Carry* c = carry_of(e);
    return c ? c->stance : std::string("stand");
}

void Gameplay::set_aiming(Entity e, bool on) {
    carry(e).aiming = on;
}

void Gameplay::set_blocking(Entity e, bool on) {
    carry(e).blocking = on;
}

float Gameplay::spread_now(Entity e) const {
    const Carry* c = carry_of(e);
    const Entity o = inventory_owner(e);
    const bool stance_on = !m_scene || !m_scene->alive(o) || m_scene->inventory(o)->stance_accuracy;
    float k = 1.0f;
    if (c && stance_on) {
        if (c->stance == "crouch") k *= 0.7f;
        if (c->stance == "prone") k *= 0.45f;
        if (c->aiming) k *= 0.5f;
        k *= 1.0f + std::min(c->speed, 8.0f) * 0.2f;
    }
    return k * (1.0f + (c ? c->bloom : 0.0f));
}

Ballistics Gameplay::ballistics_for(Entity e, const ItemDef& w) const {
    Ballistics chosen = m_ballistics;
    const Entity o = inventory_owner(e);
    if (m_scene && m_scene->alive(o))
        if (const auto b = parse_ballistics(m_scene->inventory(o)->ballistics)) chosen = *b;
    const std::optional<Ballistics> own = parse_ballistics(w.ballistics);
    if (own && *own == Ballistics::hitscan) return Ballistics::hitscan;
    if (own && (w.kind == ItemKind::throwable || w.kind == ItemKind::bow || w.blast_radius > 0.0f || w.kind == ItemKind::magic)) {
        if (chosen <= Ballistics::cone) return *own;
        return std::max(chosen, *own);
    }
    if (w.kind == ItemKind::magic && chosen <= Ballistics::cone) return Ballistics::travel;
    return chosen;
}

Entity Gameplay::ray_skip(Vec3 origin, Vec3 dir, float max_distance, Entity skip, Vec3* point, Vec3* normal, float* distance) const {
    if (!m_physics || !m_scene) return Entity{};
    Vec3 start = origin;
    float left = max_distance, travelled = 0.0f;
    const Entity skip_owner = holder(skip);
    for (int i = 0; i < 32 && left > 0.0f; ++i) {
        Vec3 p, n;
        float d = -1.0f;
        const Entity hit = m_physics->raycast(start, dir, left, &p, &n, &d);
        if (!m_scene->alive(hit) || d < 0.0f) return Entity{};
        Entity owner = m_scene->body_owner(hit);
        if (!m_scene->alive(owner)) owner = hit;
        bool mine = false;
        for (Entity at = hit; m_scene->alive(at); at = m_scene->parent(at))
            if (at == skip || (m_scene->alive(skip_owner) && at == skip_owner)) mine = true;
        if (mine || m_scene->pickup(owner)) {
            // Starting inside the shooter (the eye is inside its capsule) the ray hits right away, so step further.
            const float step = d < 0.005f ? 0.08f : 0.02f;
            start = p + dir * step;
            left -= d + step;
            travelled += d + step;
            continue;
        }
        if (point) *point = p;
        if (normal) *normal = n;
        if (distance) *distance = travelled + d;
        return hit;
    }
    return Entity{};
}

int Gameplay::fire(Entity shooter, Vec3 origin, Vec3 direction, const ItemDef& w, float power) {
    if (!m_scene) return 0;
    const bool flat = m_scene->dimension() == Dimension::two_d;
    const Ballistics mode = ballistics_for(shooter, w);
    const float spread = (mode == Ballistics::hitscan ? 0.0f : w.spread) * spread_now(shooter);
    direction = normalize(direction);
    int shots = 0;
    const int pellets = std::max(1, w.pellets);
    for (int i = 0; i < pellets; ++i) {
        Vec3 d = direction;
        if (spread > 0.0f) {
            const float a = static_cast<float>(random(0.0, 1.0)) * 6.2831853f;
            const float r = std::sqrt(static_cast<float>(random(0.0, 1.0))) * spread * deg;
            if (flat) {
                const float ang = static_cast<float>(random(-1.0, 1.0)) * spread * deg;
                d = Vec3{d.x * std::cos(ang) - d.y * std::sin(ang), d.x * std::sin(ang) + d.y * std::cos(ang), 0.0f};
            } else {
                const Vec3 u = perpendicular(d), v = cross(d, u);
                d = normalize(d + (u * std::cos(a) + v * std::sin(a)) * std::tan(r));
            }
        }
        const float speed = std::max(1.0f, w.speed * std::max(0.05f, power));
        const float dmg = w.damage * (w.kind == ItemKind::bow ? std::max(0.2f, power) : 1.0f);
        if (mode == Ballistics::hitscan || mode == Ballistics::cone) {
            Vec3 p, n;
            float dist = 0.0f;
            const float reach = std::max(10.0f, w.range > 0.0f ? w.range * 2.0f : 500.0f);
            const Entity hit = ray_skip(origin, d, reach, shooter, &p, &n, &dist);
            Projectile pr;
            pr.owner = shooter;
            pr.item = w.id;
            pr.damage = dmg;
            pr.damage_type = w.damage_type.empty() ? "projectile" : w.damage_type;
            pr.mass = std::max(0.0f, w.projectile_mass);
            pr.velocity = d * speed;
            pr.speed0 = speed;
            pr.effect = w.effect;
            pr.effect_time = w.effect_time;
            pr.effect_chance = w.effect_chance;
            pr.blast_radius = w.blast_radius;
            Tracer t;
            t.from = origin + d * 0.3f;
            t.to = m_scene->alive(hit) ? p : origin + d * reach;
            if (w.damage_type == "fire" || w.damage_type == "shock") t.color = w.damage_type == "fire" ? Vec3{1.0f, 0.3f, 0.2f} : Vec3{0.5f, 0.7f, 1.0f};
            m_tracers.push_back(t);
            if (m_scene->alive(hit)) {
                pr.position = p;
                hit_with(pr, hit, p, n);
            }
        } else {
            Projectile pr;
            pr.owner = shooter;
            pr.item = w.id;
            pr.position = origin + d * 0.25f;
            pr.start = pr.position;
            pr.velocity = d * speed;
            if (mode == Ballistics::realistic && m_physics) {
                const Entity body = m_scene->body_owner(shooter);
                if (m_scene->alive(body) && m_physics->has_body(body)) pr.velocity = pr.velocity + m_physics->velocity(body);
            }
            pr.speed0 = speed;
            pr.mass = std::max(0.001f, w.projectile_mass);
            pr.drag = mode == Ballistics::realistic ? w.drag : 0.0f;
            pr.gravity = mode >= Ballistics::drop && w.kind != ItemKind::magic;
            pr.wind = mode == Ballistics::realistic;
            pr.damage = dmg;
            pr.damage_type = w.damage_type.empty() ? "projectile" : w.damage_type;
            pr.penetration = mode == Ballistics::realistic ? w.penetration : 0.0f;
            pr.blast_radius = w.blast_radius;
            pr.fuse = w.kind == ItemKind::throwable ? w.fuse : 0.0f;
            pr.thrown = w.kind == ItemKind::throwable;
            pr.magic = w.kind == ItemKind::magic;
            pr.effect = w.effect;
            pr.effect_time = w.effect_time;
            pr.effect_chance = w.effect_chance;
            pr.life = pr.thrown ? std::max(6.0f, w.fuse + 2.0f) : 8.0f;
            pr.trail.push_back(pr.position);
            m_projectiles.push_back(pr);
        }
        ++shots;
    }
    if (const Entity o = inventory_owner(shooter); m_scene->alive(o)) carry(o).bloom = std::min(4.0f, carry(o).bloom + w.recoil * 0.12f);
    return shots;
}

void Gameplay::hit_with(Projectile& p, Entity hit, Vec3 point, Vec3 normal) {
    if (p.done || !m_scene) return;
    Entity owner = m_scene->body_owner(hit);
    if (!m_scene->alive(owner)) owner = hit;
    const Vec3 dir = length(p.velocity) > 1e-5f ? normalize(p.velocity) : Vec3{0.0f, 0.0f, 1.0f};
    const float speed = length(p.velocity);
    if (p.thrown && (p.fuse > 0.0f || p.fuse < 0.0f)) {
        const ItemDef* d = item_def(p.item);
        if (d && d->has("sticky")) {
            p.velocity = Vec3{};
            p.position = point;
            p.sticks = true;
            return;
        }
        const Vec3 v = p.velocity - normal * (2.0f * dot(p.velocity, normal));
        p.velocity = v * 0.35f;
        p.position = point + normal * 0.03f;
        return;
    }
    if (p.blast_radius > 0.0f || (p.thrown && p.fuse == 0.0f && p.blast_radius > 0.0f)) {
        detonate(p, point + normal * 0.05f);
        return;
    }
    float dmg = p.damage;
    if (p.drag > 0.0f && p.speed0 > 0.0f) dmg *= std::clamp((speed * speed) / (p.speed0 * p.speed0), 0.05f, 1.0f);
    const ItemDef* ammo = p.ammo.empty() ? nullptr : item_def(p.ammo);
    if (ammo && ammo->damage > 0.0f) dmg *= ammo->damage;
    Damage d;
    d.amount = dmg;
    d.type = p.damage_type;
    d.attacker = p.owner;
    d.knockback = dir * (p.mass * speed);
    apply_damage(owner, d);
    std::string effect = p.effect;
    float chance = p.effect_chance, etime = p.effect_time;
    if (ammo && !ammo->effect.empty()) {
        effect = ammo->effect;
        chance = ammo->effect_chance;
        etime = ammo->effect_time;
    }
    if (!effect.empty() && m_scene->alive(holder(owner)) && random(0.0, 1.0) < chance) apply_effect(holder(owner), effect, etime, 1.0f, p.owner);
    if (p.magic || p.damage_type == "fire") {
        if (const Health* hp = m_scene->alive(holder(owner)) ? m_scene->health(holder(owner)) : nullptr; hp && hp->flammable && p.damage_type == "fire") ignite(holder(owner), 6.0f);
    }
    if (!p.magic) {
        const Entity fx = add_effect(*m_scene, "impact", Entity{}, 0.35f, m_now);
        if (m_scene->alive(fx)) {
            if (Transform* t = m_scene->transform(fx)) t->position = point;
            m_temporary.emplace_back(fx, m_now + 0.6);
        }
    }
    if (p.thrown) {
        const Entity back = spawn_item(p.item, 1.0f, point + normal * 0.15f);
        static_cast<void>(back);
        p.done = true;
        return;
    }
    const bool soft = m_scene->alive(holder(owner));
    if (p.penetration > 0.5f && soft) {
        p.penetration -= 1.0f;
        p.velocity = p.velocity * 0.6f;
        p.position = point + dir * 0.6f;
        p.owner = owner;
        return;
    }
    if (p.drag > 0.0f && !soft && dot(dir, normal) > -0.25f && speed > 50.0f) {
        p.velocity = (p.velocity - normal * (2.0f * dot(p.velocity, normal))) * 0.5f;
        p.position = point + normal * 0.02f;
        p.damage *= 0.5f;
        return;
    }
    p.done = true;
}

void Gameplay::detonate(Projectile& p, Vec3 at) {
    p.done = true;
    if (!m_scene) return;
    const ItemDef* d = item_def(p.item);
    const float radius = std::max(0.5f, p.blast_radius);
    if (d && d->has("smoke")) {
        const Entity fx = effect("smoke-grenade", at, 1.0f);
        if (m_scene->alive(fx)) m_temporary.emplace_back(fx, m_now + 20.0);
        return;
    }
    if (p.damage_type == "fire" && d && d->kind == ItemKind::throwable) {
        const Entity fx = effect("fire", at, radius / 2.0f);
        if (m_scene->alive(fx)) m_temporary.emplace_back(fx, m_now + std::max(3.0f, p.effect_time));
        if (m_physics)
            for (Entity hit : m_physics->overlap_sphere(at, radius)) {
                const Entity h = holder(hit);
                if (m_scene->alive(h)) {
                    ignite(h, std::max(2.0f, p.effect_time));
                    Damage burn;
                    burn.type = "fire";
                    burn.amount = p.damage;
                    burn.attacker = p.owner;
                    apply_damage(h, burn);
                }
            }
        return;
    }
    if (p.damage > 0.0f || p.magic) explode(at, radius, radius * 3000.0f * (p.damage > 0.0f ? 1.0f : 0.1f), p.damage, p.owner, !p.magic || p.damage > 30.0f);
    if (!p.effect.empty() && m_physics)
        for (Entity hit : m_physics->overlap_sphere(at, radius)) {
            const Entity h = holder(hit);
            if (!m_scene->alive(h) || (p.effect_chance < 1.0f && random(0.0, 1.0) > p.effect_chance)) continue;
            Vec3 pt;
            float dist = 0.0f;
            const Vec3 to = position(h) + Vec3{0.0f, 1.0f, 0.0f} - at;
            if (length(to) > 0.2f && ray_skip(at, normalize(to), length(to) - 0.2f, h, &pt, nullptr, &dist).valid()) continue;
            apply_effect(h, p.effect, p.effect_time, 1.0f, p.owner);
        }
    if (p.magic) {
        const Entity fx = effect(p.damage_type == "cold" ? "steam-vent" : p.damage_type == "shock" ? "electric" : "explosion-small", at, 0.6f);
        if (m_scene->alive(fx)) m_temporary.emplace_back(fx, m_now + 1.5);
    }
}

void Gameplay::tick_projectiles(float dt) {
    if (!m_scene) return;
    for (Tracer& t : m_tracers) t.age += dt;
    m_tracers.erase(std::remove_if(m_tracers.begin(), m_tracers.end(), [](const Tracer& t) { return t.age >= t.life; }), m_tracers.end());
    const bool flat = m_scene->dimension() == Dimension::two_d;
    for (std::size_t i = 0; i < m_projectiles.size(); ++i) {
        Projectile p = m_projectiles[i];
        p.age += dt;
        if (p.thrown && p.fuse != 0.0f && p.age >= (p.fuse < 0.0f ? 5.0f : p.fuse)) {
            detonate(p, p.position);
            m_projectiles[i] = p;
            continue;
        }
        if (p.age > p.life || length(p.position - p.start) > 4000.0f) {
            p.done = true;
            m_projectiles[i] = p;
            continue;
        }
        if (p.sticks) {
            m_projectiles[i] = p;
            continue;
        }
        const int steps = std::clamp(static_cast<int>(std::ceil(length(p.velocity) * dt / 40.0f)), 1, 16);
        const float h = dt / static_cast<float>(steps);
        for (int s = 0; s < steps && !p.done; ++s) {
            Vec3 air = p.velocity;
            if (p.wind) air = air - climate_at(p.position).wind;
            Vec3 a{};
            if (p.gravity) a = a + m_gravity;
            if (p.drag > 0.0f) a = a - air * (p.drag * length(air));
            p.velocity = p.velocity + a * h;
            if (flat) {
                p.velocity.z = 0.0f;
                p.position.z = 0.0f;
            }
            const Vec3 step = p.velocity * h;
            const float len = length(step);
            if (len < 1e-6f) {
                if (p.thrown && p.fuse == 0.0f) {
                    detonate(p, p.position);
                }
                break;
            }
            Vec3 point, normal;
            float dist = -1.0f;
            const Entity hit = ray_skip(p.position, step * (1.0f / len), len, p.age < 0.05f ? p.owner : (p.thrown ? p.owner : p.owner), &point, &normal, &dist);
            if (m_scene->alive(hit)) {
                hit_with(p, hit, point, normal);
                if (p.thrown && !p.done && length(p.velocity) < 0.6f) p.velocity = Vec3{};
            } else {
                p.position = p.position + step;
            }
        }
        if (p.trail.empty() || length(p.trail.back() - p.position) > 0.5f) {
            p.trail.push_back(p.position);
            if (p.trail.size() > 12) p.trail.erase(p.trail.begin());
        }
        m_projectiles[i] = p;
    }
    m_projectiles.erase(std::remove_if(m_projectiles.begin(), m_projectiles.end(), [](const Projectile& p) { return p.done; }), m_projectiles.end());
}

bool Gameplay::melee_attack(Entity e, const ItemDef& w, ItemStack& stack) {
    if (!m_scene || !m_physics) return false;
    Carry& c = carry(e);
    if (m_now < c.next_use || stunned(e)) return false;
    if (w.stamina > 0.0f && !use_stamina(e, w.stamina)) return false;
    c.next_use = m_now + 1.0 / std::max(0.05f, w.rate);
    Vec3 origin, dir;
    aim_of(e, origin, dir);
    const float reach = std::max(0.3f, w.range);
    std::set<std::uint64_t> struck;
    auto strike = [&](Entity hit) {
        const Entity h = holder(hit);
        Entity target = m_scene->alive(h) ? h : m_scene->body_owner(hit);
        if (!m_scene->alive(target)) target = hit;
        if (target == e || holder(e) == target || !struck.insert(target.id()).second) return;
        for (Entity at = hit; m_scene->alive(at); at = m_scene->parent(at))
            if (at == e) return;
        if (m_scene->pickup(target)) return;
        Damage d;
        d.amount = w.damage * (c.aiming ? 1.25f : 1.0f);
        d.type = w.damage_type.empty() ? "melee" : w.damage_type;
        d.attacker = e;
        d.knockback = dir * w.knockback + Vec3{0.0f, w.knockback * 0.1f, 0.0f};
        apply_damage(target, d);
        if (!w.effect.empty() && m_scene->alive(h) && random(0.0, 1.0) < w.effect_chance) apply_effect(h, w.effect, w.effect_time, 1.0f, e);
        if (w.damage_type == "fire" && m_scene->alive(h) && m_scene->health(h)->flammable) ignite(h, 4.0f);
    };
    Vec3 p;
    float dist = 0.0f;
    const Entity first = ray_skip(origin, dir, reach, e, &p, nullptr, &dist);
    if (m_scene->alive(first)) strike(first);
    for (Entity hit : m_physics->overlap_sphere(origin + dir * (reach * 0.55f), reach * 0.55f)) {
        const Vec3 to = position(hit) - origin;
        const float l = length(Vec3{to.x, 0.0f, to.z});
        if (l > 1e-3f && dot(Vec3{to.x, 0.0f, to.z} * (1.0f / l), normalize(Vec3{dir.x, 0.0f, dir.z} + Vec3{0.0f, 0.0f, 0.0f})) < 0.35f) continue;
        strike(hit);
    }
    stack.durability = std::max(0.0f, stack.durability - (struck.empty() ? 0.0f : 0.002f));
    return true;
}

bool Gameplay::cast_spell(Entity e, const ItemDef& w, ItemStack& stack) {
    Carry& c = carry(e);
    if (m_now < c.next_use || stunned(e)) return false;
    const Entity sv = survivor(e);
    if (w.mana > 0.0f && m_scene->alive(sv) && m_scene->survival(sv)->mana_on) {
        Survival& s = *m_scene->survival(sv);
        if (s.mana < w.mana) return false;
        s.mana -= w.mana;
    }
    c.next_use = m_now + 1.0 / std::max(0.05f, w.rate);
    Vec3 origin, dir;
    aim_of(e, origin, dir);
    const Entity self = m_scene->alive(holder(e)) ? holder(e) : e;
    if (w.spell == "self") {
        if (w.damage < 0.0f) heal(self, -w.damage);
        if (!w.effect.empty()) apply_effect(self, w.effect, w.effect_time);
    } else if (w.spell == "area") {
        Vec3 at = position(e);
        if (w.shape == "staff") {
            Vec3 p;
            float d = 0.0f;
            at = ray_skip(origin, dir, w.range, e, &p, nullptr, &d).valid() ? p : origin + dir * w.range;
        }
        Projectile pr;
        pr.owner = e;
        pr.item = w.id;
        pr.damage = w.damage;
        pr.damage_type = w.damage_type;
        pr.blast_radius = std::max(1.0f, w.blast_radius);
        pr.magic = true;
        pr.effect = w.effect;
        pr.effect_time = w.effect_time;
        pr.effect_chance = 1.0f;
        detonate(pr, at);
        if (m_physics && w.damage > 0.0f)
            for (Entity hit : m_physics->overlap_sphere(at, pr.blast_radius)) {
                const Entity h = holder(hit);
                if (!m_scene->alive(h) || h == self) continue;
                Damage d;
                d.amount = w.damage;
                d.type = w.damage_type;
                d.attacker = e;
                apply_damage(h, d);
            }
    } else if (w.spell == "bolt") {
        std::vector<Entity> chain;
        Vec3 from = origin, aim = dir;
        Entity skip = e;
        const int jumps = w.has("chain") ? 3 : 1;
        for (int j = 0; j < jumps; ++j) {
            Vec3 p, n;
            float d = 0.0f;
            const Entity hit = ray_skip(from, aim, w.range, skip, &p, &n, &d);
            Tracer t;
            t.from = from;
            t.to = hit.valid() ? p : from + aim * w.range;
            t.life = 0.15f;
            t.color = w.damage_type == "shock" ? Vec3{0.6f, 0.75f, 1.0f} : w.color;
            m_tracers.push_back(t);
            if (!hit.valid()) break;
            const Entity h = holder(hit);
            Damage dm;
            dm.amount = w.damage;
            dm.type = w.damage_type;
            dm.attacker = e;
            apply_damage(m_scene->alive(h) ? h : hit, dm);
            if (m_scene->alive(h) && !w.effect.empty()) apply_effect(h, w.effect, w.effect_time, 1.0f, e);
            if (!m_scene->alive(h)) break;
            chain.push_back(h);
            Entity next;
            float best = 8.0f;
            for (Entity other : m_scene->entities()) {
                if (!m_scene->health(other) || other == self || std::find(chain.begin(), chain.end(), other) != chain.end() || dead(other)) continue;
                const float dd = length(position(other) - position(h));
                if (dd < best) {
                    best = dd;
                    next = other;
                }
            }
            if (!next.valid()) break;
            from = p;
            aim = normalize(position(next) + Vec3{0.0f, 1.0f, 0.0f} - p);
            skip = h;
        }
    } else {
        fire(e, origin, dir, w, 1.0f);
    }
    if (w.has("consumed")) stack.amount = std::max(0.0f, stack.amount - 1.0f);
    return true;
}

bool Gameplay::throw_item(Entity e, const ItemDef& w, float power, const std::string& hand) {
    Carry& c = carry(e);
    if (m_now < c.next_use) return false;
    c.next_use = m_now + 0.6;
    Vec3 origin, dir;
    aim_of(e, origin, dir);
    if (m_scene->dimension() != Dimension::two_d) dir = normalize(dir + Vec3{0.0f, 0.12f, 0.0f});
    ItemDef thrown = w;
    if (thrown.ballistics.empty()) thrown.ballistics = "drop";
    thrown.spread = 0.5f;
    fire(e, origin, dir, thrown, std::clamp(power, 0.3f, 1.0f));
    ItemStack* s = held(e, hand);
    if (s) {
        s->amount -= 1.0f;
        if (s->amount <= 1e-6f) s->clear();
    }
    refresh_visuals(inventory_owner(e));
    return true;
}

bool Gameplay::use_held(Entity e, bool pressed, bool held_down, float dt, const std::string& hand) {
    const Entity o = inventory_owner(e);
    if (!m_scene) return false;
    ItemStack* s = held(m_scene->alive(o) ? o : e, hand);
    const ItemDef* d = s ? item_def(s->item) : nullptr;
    const Entity who = m_scene->alive(o) ? o : e;
    Carry& c = carry(who);
    if (!d) {
        if (!pressed) return false;
        const ItemDef* fists = builtin_item("fists");
        ItemStack bare;
        return fists && melee_attack(who, *fists, bare);
    }
    switch (d->kind) {
    case ItemKind::gun: {
        if (reloading(who) || m_now < c.next_use || stunned(who)) return false;
        if (!(pressed || (held_down && d->automatic))) return false;
        if (d->magazine > 0 && s->loaded < 1.0f) {
            if (pressed) reload(who, hand);
            return false;
        }
        Vec3 origin, dir;
        aim_of(who, origin, dir);
        ItemDef shot = *d;
        if (const ItemDef* a = item_def(d->ammo); a && a->damage > 0.0f && a->kind == ItemKind::ammo) shot.damage *= a->damage;
        fire(who, origin, dir, shot, 1.0f);
        if (d->magazine > 0) s->loaded -= 1.0f;
        s->durability = std::max(0.0f, s->durability - 0.0005f);
        c.next_use = m_now + 1.0 / std::max(0.05f, d->rate);
        return true;
    }
    case ItemKind::bow: {
        const bool crossbow = d->shape.find("crossbow") != std::string::npos;
        if (crossbow) {
            if (reloading(who) || m_now < c.next_use || !pressed) return false;
            if (s->loaded < 1.0f) {
                reload(who, hand);
                return false;
            }
            Vec3 origin, dir;
            aim_of(who, origin, dir);
            ItemDef shot = *d;
            if (const ItemDef* a = item_def(d->ammo); a && a->damage > 0.0f) shot.damage *= a->damage;
            fire(who, origin, dir, shot, 1.0f);
            s->loaded -= 1.0f;
            c.next_use = m_now + 0.2;
            return true;
        }
        if (pressed && count(who, d->ammo) + s->loaded >= 1.0f) {
            c.drawing = true;
            c.draw = 0.0f;
            return true;
        }
        if (c.drawing && held_down) c.draw += dt;
        return c.drawing;
    }
    case ItemKind::melee: return (pressed || (held_down && d->rate > 4.0f)) && melee_attack(who, *d, *s);
    case ItemKind::magic: return pressed && cast_spell(who, *d, *s);
    case ItemKind::throwable:
        if (pressed) {
            c.drawing = true;
            c.draw = 0.0f;
            return true;
        }
        if (c.drawing && held_down) c.draw += dt;
        return c.drawing;
    case ItemKind::potion:
    case ItemKind::food:
    case ItemKind::drink:
    case ItemKind::medical:
        if (!pressed) return false;
        consume(who, *d, *s);
        if (s->amount <= 1e-6f) s->clear();
        return true;
    default: return false;
    }
}

bool Gameplay::release_held(Entity e, const std::string& hand) {
    const Entity o = inventory_owner(e);
    const Entity who = m_scene && m_scene->alive(o) ? o : e;
    Carry& c = carry(who);
    if (!c.drawing) return false;
    c.drawing = false;
    ItemStack* s = held(who, hand);
    const ItemDef* d = s ? item_def(s->item) : nullptr;
    if (!d) return false;
    if (d->kind == ItemKind::throwable) return throw_item(who, *d, 0.4f + std::min(1.0f, c.draw / 0.6f) * 0.6f, hand);
    if (d->kind != ItemKind::bow) return false;
    const float power = d->draw_time > 0.0f ? std::min(1.0f, c.draw / d->draw_time) : 1.0f;
    if (power < 0.15f) return false;
    if (take(who, d->ammo, 1.0f) < 1.0f) return false;
    Vec3 origin, dir;
    aim_of(who, origin, dir);
    ItemDef shot = *d;
    if (const ItemDef* a = item_def(d->ammo); a) {
        if (a->damage > 0.0f) shot.damage *= a->damage;
        if (!a->effect.empty()) {
            shot.effect = a->effect;
            shot.effect_time = a->effect_time;
            shot.effect_chance = a->effect_chance;
        }
        if (a->effect == "burning") shot.damage_type = "fire";
    }
    fire(who, origin, dir, shot, power);
    return true;
}

bool Gameplay::reload(Entity e, const std::string& hand) {
    const Entity o = inventory_owner(e);
    const Entity who = m_scene && m_scene->alive(o) ? o : e;
    ItemStack* s = held(who, hand);
    const ItemDef* d = s ? item_def(s->item) : nullptr;
    if (!d || d->magazine <= 0 || reloading(who)) return false;
    const float need = static_cast<float>(d->magazine) - s->loaded;
    if (need < 1.0f) return false;
    const float got = d->ammo.empty() ? need : take(who, d->ammo, need);
    if (got < 1.0f && !d->ammo.empty()) return false;
    s->loaded += std::floor(got);
    carry(who).reload_until = m_now + std::max(0.0f, d->reload);
    return true;
}

bool Gameplay::reloading(Entity e) const {
    const Carry* c = carry_of(e);
    return c && c->reload_until > m_now;
}

std::vector<Entity> Gameplay::visual_parts(Entity e) const {
    const Carry* c = carry_of(e);
    return c ? c->visuals : std::vector<Entity>{};
}

void Gameplay::refresh_visuals(Entity e) {
    if (!m_scene || !m_scene->alive(e) || !m_scene->inventory(e)) return;
    Inventory& inv = *m_scene->inventory(e);
    std::vector<std::pair<std::string, const ItemStack*>> want;
    for (const auto& [slot, s] : inv.worn)
        if (!s.empty()) want.emplace_back(slot, &s);
    for (const auto& [slot, s] : inv.holstered)
        if (!s.empty()) want.emplace_back("holster:" + slot, &s);
    if (inv.worn["mainhand"].empty())
        if (ItemStack* h = held(e, "mainhand")) want.emplace_back("mainhand", h);
    std::string key;
    for (const auto& [slot, s] : want) key += slot + "=" + s->item + ";";
    Carry& c = carry(e);
    if (key == c.visual_key) return;
    c.visual_key = key;
    for (Entity v : c.visuals)
        if (m_scene->alive(v)) m_scene->destroy(v);
    c.visuals.clear();
    const float k = 1.0f;
    for (const auto& [slot, s] : want) {
        const ItemDef* d = item_def(s->item);
        if (!d) continue;
        std::string where = slot;
        if (slot.rfind("holster:", 0) == 0) {
            where = slot.substr(8);
            where = where.substr(0, where.find(':'));
            if (where == "body") where = d->size >= ItemSize::large ? "back_slot" : "hip";
        }
        const Anchor a = anchor_of(where);
        const Entity v = m_scene->create("Worn: " + d->name, e);
        if (Transform* t = m_scene->transform(v)) {
            t->position = a.at * k;
            t->rotation = from_euler(a.euler);
        }
        m_scene->add_model(v).source = "Assets/Models/Items/" + d->id + "-worn.glb";
        m_scene->add_tag(v, "worn-visual");
        if (!d->texture.empty()) m_scene->add_material(v).base_map = d->texture;
        c.visuals.push_back(v);
    }
}

void Gameplay::tick_items(double now, float dt) {
    if (!m_scene) return;
    for (auto it = m_respawns.begin(); it != m_respawns.end();) {
        if (now < it->second) {
            ++it;
            continue;
        }
        if (m_scene->alive(it->first))
            if (Pickup* p = m_scene->pickup(it->first)) {
                if (Model* m = m_scene->model(it->first)) m->visible = true;
                p->stack.amount = p->stack.amount > 0.0f ? p->stack.amount : 1.0f;
            }
        it = m_respawns.erase(it);
    }
    for (Entity e : m_scene->entities()) {
        Inventory* inv = m_scene->inventory(e);
        if (!inv) continue;
        Carry& c = carry(e);
        const Vec3 p = position(e);
        if (dt > 0.0f) c.speed = length(Vec3{p.x - c.last_pos.x, 0.0f, p.z - c.last_pos.z}) / dt * 0.3f + c.speed * 0.7f;
        c.last_pos = p;
        c.bloom = std::max(0.0f, c.bloom - dt * 2.5f);
        std::vector<std::string> effects;
        for (const auto& [slot, s] : inv->worn) {
            if (s.empty() || slot == "mainhand" || slot == "offhand") continue;
            const ItemDef* d = item_def(s.item);
            if (d && !d->effect.empty() && d->effect_time < 0.0f) effects.push_back(d->effect);
        }
        const Entity h = m_scene->alive(holder(e)) ? holder(e) : e;
        for (const std::string& ef : c.worn_effects)
            if (std::find(effects.begin(), effects.end(), ef) == effects.end()) remove_effect(h, ef);
        for (const std::string& ef : effects)
            if (effect_stacks(h, ef) == 0) apply_effect(h, ef, -1.0f);
        c.worn_effects = effects;
        if (inv->equipment) refresh_visuals(e);
    }
    tick_projectiles(dt);
    tick_slosh(dt);
}

void Gameplay::tick_slosh(float dt) {
    if (!m_scene || dt <= 0.0f) return;
    for (Entity e : m_scene->entities()) {
        if (m_scene->name(e) != "Liquid") continue;
        const Entity parent = m_scene->parent(e);
        if (!m_scene->alive(parent)) continue;
        Slosh& s = m_slosh[e.id()];
        const Vec3 p = position(parent);
        const Vec3 v = s.ready ? (p - s.last_pos) * (1.0f / dt) : Vec3{};
        const Vec3 a = s.ready ? (v - s.last_vel) * (1.0f / dt) : Vec3{};
        s.last_pos = p;
        s.last_vel = v;
        s.ready = true;
        Vec3 want = Vec3{0.0f, 9.81f, 0.0f} - a * 0.6f;
        if (length(want) < 1e-3f) want = Vec3{0.0f, 1.0f, 0.0f};
        want = normalize(want);
        if (want.y < 0.2f) want = normalize(Vec3{want.x, 0.2f, want.z});
        const Vec3 pull = (want - s.normal) * 60.0f - s.speed * 4.0f;
        s.speed = s.speed + pull * dt;
        s.normal = normalize(s.normal + s.speed * dt);
        const Vec3 up{0.0f, 1.0f, 0.0f};
        const Vec3 axis = cross(up, s.normal);
        const float sin_a = length(axis);
        const float cos_a = std::clamp(dot(up, s.normal), -1.0f, 1.0f);
        Quat world{0.0f, 0.0f, 0.0f, 1.0f};
        if (sin_a > 1e-5f) {
            const float ang = std::atan2(sin_a, cos_a);
            const Vec3 n = axis * (1.0f / sin_a);
            world = Quat{n.x * std::sin(ang * 0.5f), n.y * std::sin(ang * 0.5f), n.z * std::sin(ang * 0.5f), std::cos(ang * 0.5f)};
        }
        const Quat pr = world_rotation(*m_scene, parent);
        if (Transform* t = m_scene->transform(e)) t->rotation = normalize(Quat{-pr.x, -pr.y, -pr.z, pr.w} * world);
    }
}

} // namespace orchard
