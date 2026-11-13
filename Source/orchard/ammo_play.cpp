#include <orchard/effects.hpp>
#include <orchard/gameplay.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

namespace orchard {

namespace {

std::vector<std::string> words(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : text + ",") {
        if (c == ',' || c == ';') {
            const std::size_t a = cur.find_first_not_of(" \t"), b = cur.find_last_not_of(" \t");
            if (a != std::string::npos) out.push_back(cur.substr(a, b - a + 1));
            cur.clear();
        } else {
            cur += c;
        }
    }
    return out;
}

std::string join_list(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return a + ", " + b;
}

bool magazine_fed(const ItemDef& d) {
    return d.feed.empty() || d.feed == "magazine" || d.feed == "belt";
}

bool one_at_a_time(const ItemDef& d) {
    return d.feed == "cylinder" || d.feed == "internal" || d.feed == "muzzle";
}

std::string carried_variant(const Inventory& inv, const std::string& ammo, const std::string& pref) {
    if (!pref.empty() && inventory_count_variant(inv, ammo, pref) >= 1.0f) return pref;
    std::string found;
    auto look = [&](const ItemStack& s) {
        if (found.empty() && s.item == ammo && !s.empty()) found = s.variant.empty() ? std::string("*none") : s.variant;
    };
    for (const ItemStack& s : inv.items) look(s);
    for (const ItemStack& s : inv.hotbar_items) look(s);
    for (const auto& [slot, w] : inv.worn)
        for (const ItemStack& s : w.contents) look(s);
    return found == "*none" ? std::string() : found;
}

std::string fmt(float v, int digits = 0) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.*f", digits, static_cast<double>(v));
    return buf;
}

} // namespace

std::string Gameplay::reload_mode(Entity e) const {
    const Entity o = inventory_owner(e);
    if (m_scene && m_scene->alive(o))
        if (const Inventory* inv = m_scene->inventory(o); inv && !inv->reload_mode.empty()) return inv->reload_mode;
    return m_scene ? m_scene->rule("reload") : std::string("keep");
}

float Gameplay::give_stack(Entity e, const ItemStack& st) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o) || st.empty()) return 0.0f;
    return inventory_add_stack(m_scene->items(), *m_scene->inventory(o), st, st.amount);
}

float Gameplay::count_variant(Entity e, const std::string& item, const std::string& variant) const {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return 0.0f;
    return inventory_count_variant(*m_scene->inventory(o), item, variant.empty() ? std::string("*") : variant);
}

float Gameplay::take_variant(Entity e, const std::string& item, const std::string& variant, float amount) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return 0.0f;
    return inventory_take_variant(m_scene->items(), *m_scene->inventory(o), item, variant.empty() ? std::string("*") : variant, amount);
}

void Gameplay::fill_for_world(ItemStack& s, const std::string& fill_in) {
    if (!m_scene) return;
    const ItemDef* d = item_def(s.item);
    if (!d) return;
    const ItemCatalog& cat = m_scene->items();
    if (d->kind == ItemKind::ammo) {
        if (s.variant.empty()) {
            const std::vector<std::string> v = ammo_variants(cat, *d);
            const std::string pref = m_scene->rule("default_variant");
            s.variant = std::find(v.begin(), v.end(), pref) != v.end() ? pref : default_variant(cat, *d);
        }
        return;
    }
    const bool gun = (d->kind == ItemKind::gun || d->kind == ItemKind::bow) && d->magazine > 0;
    const bool mag = d->kind == ItemKind::magazine;
    if (!gun && !mag) return;
    const std::string fill = fill_in.empty() ? m_scene->rule("world_fill") : fill_in;
    if (fill == "as_placed") return;
    const float cap = static_cast<float>(d->magazine);
    float rounds = cap;
    if (fill == "empty") rounds = 0.0f;
    else if (fill == "partial") rounds = std::floor(cap * std::clamp(m_scene->rule_number("fill_fraction"), 0.0f, 1.0f) + 0.5f);
    else if (fill == "random") rounds = std::floor(static_cast<float>(random(0.0, static_cast<double>(cap) + 0.999)));
    else if (fill != "full") {
        const float k = static_cast<float>(std::atof(fill.c_str()));
        if (fill.find('%') != std::string::npos) rounds = std::floor(cap * std::clamp(k / 100.0f, 0.0f, 1.0f) + 0.5f);
        else if (k > 1.0f) rounds = std::min(cap, std::floor(k));
        else rounds = std::floor(cap * std::clamp(k, 0.0f, 1.0f) + 0.5f);
    }
    if (s.ammo.empty()) s.ammo = d->ammo;
    if (s.variant.empty())
        if (const ItemDef* a = item_def(s.ammo)) {
            const std::vector<std::string> v = ammo_variants(cat, *a);
            const std::string pref = m_scene->rule("default_variant");
            s.variant = std::find(v.begin(), v.end(), pref) != v.end() ? pref : default_variant(cat, *a);
        }
    if (gun && magazine_fed(*d) && m_scene->rule_on("magazines") && s.mag.empty()) {
        const std::vector<const ItemDef*> mags = magazines_for_gun(cat, *d);
        if (!mags.empty()) s.mag = mags.front()->id;
    }
    s.loaded = rounds;
}

void Gameplay::prepare_world() {
    if (!m_scene) return;
    for (Entity e : m_scene->entities()) {
        if (Pickup* p = m_scene->pickup(e)) {
            fill_for_world(p->stack, p->fill);
            if (const ItemDef* d = item_def(p->stack.item); d && d->kind == ItemKind::magazine)
                if (Model* m = m_scene->model(e); m && m->source.rfind("Assets/Models/Items/", 0) == 0) m->source = "Assets/Models/Items/" + d->id + (p->stack.loaded > 0.0f ? "-full" : "-empty") + ".glb";
        }
        if (Inventory* inv = m_scene->inventory(e)) {
            auto fix = [&](ItemStack& s) {
                const ItemDef* d = item_def(s.item);
                if (!d || s.empty()) return;
                if (d->kind == ItemKind::ammo && s.variant.empty()) fill_for_world(s, "as_placed");
                if ((d->kind == ItemKind::gun || d->kind == ItemKind::magazine) && s.ammo.empty() && s.loaded <= 0.0f) fill_for_world(s, std::string());
            };
            for (ItemStack& s : inv->items) fix(s);
            for (ItemStack& s : inv->hotbar_items) fix(s);
            for (auto& [slot, s] : inv->worn) {
                fix(s);
                for (ItemStack& c : s.contents) fix(c);
            }
            for (auto& [slot, s] : inv->holstered) fix(s);
        }
    }
}

namespace {

struct Found {
    std::string list;
    int index = -1;
    ItemStack* stack = nullptr;
};

} // namespace

bool Gameplay::choose_ammo(Entity e, const std::string& ammo, const std::string& variant, const std::string& hand) {
    const Entity o = inventory_owner(e);
    const Entity who = m_scene && m_scene->alive(o) ? o : e;
    ItemStack* s = held(who, hand);
    const ItemDef* d = s ? item_def(s->item) : nullptr;
    const ItemDef* a = item_def(ammo);
    if (!d || !a || !ammo_fits(*d, *a)) return false;
    Carry& c = carry(who);
    c.reload_ammo = ammo;
    c.reload_variant = variant;
    if (s->loaded <= 0.0f) {
        s->ammo = ammo;
        s->variant = variant;
    }
    return true;
}

std::string Gameplay::loaded_ammo(Entity e, const std::string& hand) {
    const Entity o = inventory_owner(e);
    ItemStack* s = held(m_scene && m_scene->alive(o) ? o : e, hand);
    if (!s || s->empty()) return std::string();
    if (!s->ammo.empty()) return s->ammo;
    const ItemDef* d = item_def(s->item);
    return d ? d->ammo : std::string();
}

std::string Gameplay::loaded_variant(Entity e, const std::string& hand) {
    const Entity o = inventory_owner(e);
    ItemStack* s = held(m_scene && m_scene->alive(o) ? o : e, hand);
    if (!s || s->empty()) return std::string();
    if (!s->variant.empty()) return s->variant;
    const ItemDef* a = item_def(loaded_ammo(e, hand));
    return a && m_scene ? default_variant(m_scene->items(), *a) : std::string();
}

float Gameplay::loaded_rounds(Entity e, const std::string& hand) {
    const Entity o = inventory_owner(e);
    ItemStack* s = held(m_scene && m_scene->alive(o) ? o : e, hand);
    return s && !s->empty() ? s->loaded : 0.0f;
}

float Gameplay::load_magazine(Entity e, const std::string& list, int index, const std::string& ammo_in, const std::string& variant_in, float amount) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return 0.0f;
    ItemStack* s = stack_at(o, list, index);
    const ItemDef* d = s ? item_def(s->item) : nullptr;
    if (!d || (d->kind != ItemKind::magazine && d->kind != ItemKind::gun)) return 0.0f;
    float need = static_cast<float>(d->magazine) - s->loaded;
    if (amount >= 0.0f) need = std::min(need, std::floor(amount));
    if (need < 1.0f) return 0.0f;
    std::string ammo = s->loaded > 0.0f ? s->ammo : ammo_in;
    std::string variant = s->loaded > 0.0f ? s->variant : variant_in;
    auto takes = [&](const ItemDef& a) { return d->kind == ItemKind::magazine ? magazine_takes(*d, a) : ammo_fits(*d, a); };
    if (ammo.empty() || count_variant(o, ammo, variant) < 1.0f) {
        if (s->loaded > 0.0f && !s->ammo.empty()) {
            ammo = s->ammo;
            variant = s->variant;
        } else {
            ammo.clear();
            const Inventory& inv = *m_scene->inventory(o);
            auto look = [&](const ItemStack& st) {
                if (!ammo.empty() || st.empty()) return;
                const ItemDef* a = item_def(st.item);
                if (a && takes(*a)) {
                    ammo = st.item;
                    variant = st.variant;
                }
            };
            for (const ItemStack& st : inv.items) look(st);
            for (const ItemStack& st : inv.hotbar_items) look(st);
            for (const auto& [slot, w] : inv.worn)
                for (const ItemStack& st : w.contents) look(st);
        }
    }
    const ItemDef* a = item_def(ammo);
    if (!a || !takes(*a)) return 0.0f;
    if (s->loaded <= 0.0f) variant = carried_variant(*m_scene->inventory(o), ammo, variant);
    const float got = std::floor(take_variant(o, ammo, variant, need));
    if (got < 1.0f) return 0.0f;
    s = stack_at(o, list, index);
    if (!s) return 0.0f;
    s->loaded += got;
    s->ammo = ammo;
    s->variant = variant.empty() ? default_variant(m_scene->items(), *a) : variant;
    Carry& c = carry(o);
    c.next_use = std::max(c.next_use, m_now + got * std::max(0.0f, m_scene->rule_number("load_time")));
    cue_sound("shell_insert", position(o), o, 0.6f);
    return got;
}

float Gameplay::unload_magazine(Entity e, const std::string& list, int index) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return 0.0f;
    ItemStack* s = stack_at(o, list, index);
    if (!s || s->empty() || s->loaded <= 0.0f || s->ammo.empty()) return 0.0f;
    ItemStack rounds;
    rounds.item = s->ammo;
    rounds.variant = s->variant;
    rounds.amount = std::floor(s->loaded);
    const float n = rounds.amount;
    s->loaded = 0.0f;
    const float back = give_stack(o, rounds);
    if (back < n) {
        const Entity drop = spawn_item(rounds.item, n - back, position(o) + Vec3{0.0f, 0.8f, 0.0f});
        if (m_scene->alive(drop)) m_scene->pickup(drop)->stack.variant = rounds.variant;
    }
    return n;
}

bool Gameplay::unload(Entity e, const std::string& hand) {
    const Entity o = inventory_owner(e);
    const Entity who = m_scene && m_scene->alive(o) ? o : e;
    ItemStack* s = held(who, hand);
    const ItemDef* d = s ? item_def(s->item) : nullptr;
    if (!d || d->magazine <= 0 || reloading(who)) return false;
    if (!s->mag.empty()) {
        ItemStack mag;
        mag.item = s->mag;
        mag.amount = 1.0f;
        mag.loaded = s->loaded;
        mag.ammo = s->ammo;
        mag.variant = s->variant;
        s->mag.clear();
        s->loaded = 0.0f;
        if (give_stack(who, mag) < 1.0f) {
            const Entity drop = spawn_item(mag.item, 1.0f, position(who) + Vec3{0.0f, 0.8f, 0.0f});
            if (m_scene->alive(drop)) m_scene->pickup(drop)->stack = mag;
        }
        cue_sound(sound_for(*d, "unload"), position(who), who, 0.8f);
        return true;
    }
    if (s->loaded <= 0.0f) return false;
    ItemStack rounds;
    rounds.item = s->ammo.empty() ? d->ammo : s->ammo;
    rounds.variant = s->variant;
    rounds.amount = std::floor(s->loaded);
    s->loaded = 0.0f;
    const float back = give_stack(who, rounds);
    if (back < rounds.amount) {
        const Entity drop = spawn_item(rounds.item, rounds.amount - back, position(who) + Vec3{0.0f, 0.8f, 0.0f});
        if (m_scene->alive(drop)) m_scene->pickup(drop)->stack.variant = rounds.variant;
    }
    cue_sound(sound_for(*d, "unload"), position(who), who, 0.8f);
    return true;
}

bool Gameplay::reload(Entity e, const std::string& hand) {
    const Entity o = inventory_owner(e);
    const Entity who = m_scene && m_scene->alive(o) ? o : e;
    if (!m_scene) return false;
    ItemStack* s = held(who, hand);
    const ItemDef* d = s ? item_def(s->item) : nullptr;
    if (!d || d->magazine <= 0 || reloading(who) || cycling(who)) return false;
    if (d->kind == ItemKind::gun && (s->action == "jammed" || ((s->action == "spent" || s->action == "empty" || s->action == "open") && s->loaded >= 1.0f))) return cycle_weapon(who, hand);
    const ItemCatalog& cat = m_scene->items();
    Carry& c = carry(who);
    const std::string mode = reload_mode(who);
    if (d->kind != ItemKind::gun) {
        const float need = static_cast<float>(d->magazine) - s->loaded;
        if (need < 1.0f) return false;
        const float got = d->ammo.empty() ? need : take(who, d->ammo, need);
        if (got < 1.0f && !d->ammo.empty()) return false;
        s->loaded += std::floor(got);
        c.reload_start = m_now;
        c.reload_until = m_now + std::max(0.0f, d->reload);
        cue_sound(sound_for(*d, "reload"), position(who), who);
        return true;
    }
    Inventory& inv = *m_scene->inventory(who);
    const std::string act = gun_action_of(*d);
    const std::string before = s->action;
    auto sequence = [&](double T) {
        const double t0 = m_now;
        c.sound_queue.erase(std::remove_if(c.sound_queue.begin(), c.sound_queue.end(), [](const std::pair<double, std::string>& q) { return q.second.rfind('@', 0) != 0; }), c.sound_queue.end());
        if (!d->sound_reload.empty()) {
            queue_sound(c, t0, d->sound_reload);
            return;
        }
        if (d->feed == "belt") {
            queue_sound(c, t0 + T * 0.08, "feed_cover_open");
            queue_sound(c, t0 + T * 0.35, "belt_in");
            queue_sound(c, t0 + T * 0.6, "feed_cover_close");
            queue_sound(c, t0 + T * 0.85, "charging_handle");
            return;
        }
        queue_sound(c, t0 + T * 0.12, "mag_out");
        queue_sound(c, t0 + T * 0.55, "mag_in");
        if (before == "open") queue_sound(c, t0 + T * 0.84, "slide_release");
        else if (before == "empty" || before == "spent" || before == "jammed") {
            if (needs_working(*d) && cycle_kind_for(*d) != "hammer") {
                queue_sound(c, t0 + T * 0.74, act == "pump" ? "pump_back" : act == "lever" ? "lever_open" : "bolt_open");
                queue_sound(c, t0 + T * 0.9, act == "pump" ? "pump_forward" : act == "lever" ? "lever_close" : "bolt_close");
            } else {
                queue_sound(c, t0 + T * 0.86, "slide_rack");
            }
        }
    };
    const float rack_time = before == "empty" || before == "spent" || before == "jammed" ? 0.35f : before == "open" ? 0.1f : 0.0f;
    std::vector<Found> stacks;
    auto gather_all = [&]() {
        stacks.clear();
        auto gather = [&](const std::string& list, std::vector<ItemStack>& v) {
            for (std::size_t i = 0; i < v.size(); ++i)
                if (!v[i].empty()) stacks.push_back({list, static_cast<int>(i), &v[i]});
        };
        gather("items", inv.items);
        gather("hotbar", inv.hotbar_items);
        for (auto& [slot, w] : inv.worn)
            if (!w.empty() && slot != "mainhand" && slot != "offhand") gather("container:" + slot, w.contents);
    };
    gather_all();
    const std::vector<const ItemDef*> mag_defs = magazines_for_gun(cat, *d);
    const bool use_mags = m_scene->rule_on("magazines") && magazine_fed(*d) && mode != "pool" && mode != "top_up" && !mag_defs.empty();
    const std::string want_ammo = !c.reload_ammo.empty() ? c.reload_ammo : !s->ammo.empty() ? s->ammo : d->ammo;
    const std::string want_variant = !c.reload_ammo.empty() ? c.reload_variant : s->variant;
    if (use_mags) {
        auto find_best = [&]() {
            Found best;
            float best_score = -1.0f;
            for (const Found& f : stacks) {
                const ItemDef* md = item_def(f.stack->item);
                if (!md || !magazine_fits(*d, *md) || f.stack->loaded < 1.0f) continue;
                float score = f.stack->loaded;
                if (f.stack->ammo == want_ammo) score += 1000.0f;
                if (f.stack->ammo == want_ammo && f.stack->variant == want_variant) score += 1000.0f;
                if (score > best_score) {
                    best_score = score;
                    best = f;
                }
            }
            return best;
        };
        Found best = find_best();
        float extra = 0.0f;
        if (!best.stack && m_scene->rule_on("auto_fill")) {
            std::vector<std::pair<std::string, int>> empties;
            for (const Found& f : stacks)
                if (const ItemDef* md = item_def(f.stack->item); md && magazine_fits(*d, *md)) empties.emplace_back(f.list, f.index);
            for (const auto& [list, index] : empties) {
                const float got = load_magazine(who, list, index, want_ammo, want_variant);
                if (got >= 1.0f) {
                    extra = got * std::max(0.0f, m_scene->rule_number("load_time"));
                    break;
                }
            }
            gather_all();
            best = find_best();
            s = held(who, hand);
            if (!best.stack && !s->mag.empty() && s->loaded < static_cast<float>(d->magazine)) {
                const std::string ammo = s->loaded > 0.0f && !s->ammo.empty() ? s->ammo : want_ammo.empty() ? d->ammo : want_ammo;
                const std::string variant = s->loaded > 0.0f ? s->variant : carried_variant(inv, ammo, want_variant);
                const float got = std::floor(take_variant(who, ammo, variant, static_cast<float>(d->magazine) - s->loaded));
                if (got >= 1.0f) {
                    s = held(who, hand);
                    s->loaded += got;
                    s->ammo = ammo;
                    s->variant = variant;
                    s->action.clear();
                    c.reload_start = m_now;
                    c.reload_until = m_now + std::max(0.0f, d->reload) + got * std::max(0.0f, m_scene->rule_number("load_time")) + rack_time;
                    c.reload_style = d->feed == "belt" ? "belt" : "mag";
                    sequence(c.reload_until - c.reload_start);
                    return true;
                }
            }
        }
        bool any_mag = !s->mag.empty();
        for (const Found& f : stacks)
            if (const ItemDef* md = item_def(f.stack->item); md && magazine_fits(*d, *md)) any_mag = true;
        if (!best.stack && any_mag) {
            if (!m_quiet_reload) cue_sound("dry_fire", position(who), who, 0.6f);
            return false;
        }
        if (best.stack) {
            ItemStack fresh = *best.stack;
            fresh.amount = 1.0f;
            best.stack->amount -= 1.0f;
            if (best.stack->amount <= 1e-6f) best.stack->clear();
            if (inv.style != InventoryStyle::slots) {
                inv.items.erase(std::remove_if(inv.items.begin(), inv.items.end(), [](const ItemStack& x) { return x.empty(); }), inv.items.end());
                for (auto& [slot, w] : inv.worn) w.contents.erase(std::remove_if(w.contents.begin(), w.contents.end(), [](const ItemStack& x) { return x.empty(); }), w.contents.end());
            }
            stacks.clear();
            s = held(who, hand);
            const bool tactical = s->loaded >= 1.0f;
            const float chamber = m_scene->rule_on("chamber") && tactical && !s->mag.empty() ? 1.0f : 0.0f;
            if (!s->mag.empty()) {
                ItemStack old;
                old.item = s->mag;
                old.amount = 1.0f;
                old.loaded = std::max(0.0f, s->loaded - chamber);
                old.ammo = s->ammo;
                old.variant = s->variant;
                if (mode == "keep") {
                    if (give_stack(who, old) < 1.0f) {
                        const Entity drop = spawn_item(old.item, 1.0f, position(who) + Vec3{0.0f, 0.6f, 0.0f});
                        if (m_scene->alive(drop)) m_scene->pickup(drop)->stack = old;
                    }
                } else if (mode == "drop") {
                    const Entity drop = spawn_item(old.item, 1.0f, position(who) + Vec3{0.0f, 0.6f, 0.0f});
                    if (m_scene->alive(drop)) {
                        m_scene->pickup(drop)->stack = old;
                        if (Model* m = m_scene->model(drop)) m->source = "Assets/Models/Items/" + old.item + (old.loaded > 0.0f ? "-full" : "-empty") + ".glb";
                    }
                }
                s = held(who, hand);
            } else if (s->loaded >= 1.0f) {
                ItemStack rounds;
                rounds.item = s->ammo.empty() ? d->ammo : s->ammo;
                rounds.variant = s->variant;
                rounds.amount = std::floor(s->loaded - chamber);
                if (rounds.amount >= 1.0f) give_stack(who, rounds);
                s = held(who, hand);
            }
            s->mag = fresh.item;
            s->loaded = fresh.loaded + chamber;
            s->ammo = fresh.ammo.empty() ? d->ammo : fresh.ammo;
            s->variant = fresh.variant;
            s->action.clear();
            c.reload_start = m_now;
            c.reload_until = m_now + std::max(0.0f, d->reload) * (tactical ? 0.85f : 1.0f) + extra + rack_time;
            c.reload_rounds = 0;
            c.reload_style = d->feed == "belt" ? "belt" : "mag";
            sequence(c.reload_until - c.reload_start);
            return true;
        }
        if (s->mag.empty()) s->mag = mag_defs.front()->id;
    }
    gather_all();
    const float need = static_cast<float>(d->magazine) - s->loaded;
    if (need < 1.0f) return false;
    std::string ammo, variant;
    auto pick = [&](bool exact, bool same_id) {
        for (const Found& f : stacks) {
            const ItemDef* a = item_def(f.stack->item);
            if (!a || !ammo_fits(*d, *a)) continue;
            if (same_id && f.stack->item != want_ammo) continue;
            if (exact && f.stack->variant != want_variant) continue;
            ammo = f.stack->item;
            variant = f.stack->variant;
            return true;
        }
        return false;
    };
    if (s->loaded >= 1.0f && !s->ammo.empty()) {
        ammo = s->ammo;
        variant = s->variant;
        if (count_variant(who, ammo, variant) < 1.0f) {
            if (c.reload_ammo.empty() || !pick(true, true)) {
                if (!m_quiet_reload) cue_sound("dry_fire", position(who), who, 0.6f);
                return false;
            }
        }
    } else if (!pick(true, true) && !pick(false, true) && !pick(false, false)) {
        if (!m_quiet_reload) cue_sound("dry_fire", position(who), who, 0.6f);
        return false;
    }
    if (s->loaded >= 1.0f && (ammo != s->ammo || variant != s->variant)) {
        ItemStack rounds;
        rounds.item = s->ammo.empty() ? d->ammo : s->ammo;
        rounds.variant = s->variant;
        rounds.amount = std::floor(s->loaded);
        s->loaded = 0.0f;
        give_stack(who, rounds);
        s = held(who, hand);
    }
    s->ammo = ammo;
    s->variant = variant;
    const float room = static_cast<float>(d->magazine) - s->loaded;
    if (one_at_a_time(*d) || mode == "top_up") {
        c.reload_rounds = static_cast<int>(room);
        c.round_time = std::max(0.05f, d->reload / std::max(1.0f, static_cast<float>(d->magazine)) * (d->feed == "muzzle" ? static_cast<float>(d->magazine) : 1.0f));
        c.reload_hand = hand;
        c.reload_ammo = ammo;
        c.reload_variant = variant;
        c.reload_start = m_now;
        const bool hammer_gun = needs_working(*d) && cycle_kind_for(*d) == "hammer";
        const bool needs_cycle = hammer_gun || ((act == "pump" || act == "lever") && (before == "spent" || before == "empty" || s->loaded < 1.0f));
        const float lead = act == "revolver_da" || act == "break" ? 0.45f : act == "bolt" || act == "straight_pull" ? 0.2f : 0.0f;
        c.next_round = m_now + c.round_time + lead;
        c.reload_until = m_now + c.round_time * room + lead;
        c.reload_style = std::string(act == "revolver_da" || act == "revolver_sa" ? "cylinder" : act == "break" ? "break" : act == "muzzle" ? "muzzle" : act == "bolt" || act == "straight_pull" ? "bolt" : "tube") + (needs_cycle ? "+cycle" : "");
        if (hammer_gun) s->action = "spent";
        else if (act != "pump" && act != "lever") s->action.clear();
        cue_sound(sound_for(*d, "reload_start"), position(who), who, 0.7f);
        if (act == "revolver_da") queue_sound(c, m_now + 0.3, "ejector");
        if (act == "break") queue_sound(c, m_now + 0.3, "shell_out");
        if (act == "muzzle") {
            queue_sound(c, m_now + c.round_time * 0.15, "powder_pour");
            queue_sound(c, m_now + c.round_time * 0.45, "ramrod");
            queue_sound(c, m_now + c.round_time * 0.6, "ramrod");
        }
        return true;
    }
    const float got = std::floor(take_variant(who, ammo, variant, room));
    if (got < 1.0f) return false;
    s = held(who, hand);
    s->loaded += got;
    s->action.clear();
    c.reload_start = m_now;
    c.reload_until = m_now + std::max(0.0f, d->reload) + rack_time;
    c.reload_rounds = 0;
    c.reload_style = d->feed == "belt" ? "belt" : "mag";
    sequence(c.reload_until - c.reload_start);
    return true;
}

void Gameplay::tick_reloads() {
    if (!m_scene) return;
    for (Entity e : m_scene->entities()) {
        if (!m_scene->inventory(e)) continue;
        auto it = m_carry.find(e.id());
        if (it == m_carry.end()) continue;
        Carry& c = it->second;
        const bool now_reloading = c.reload_until > m_now;
        if (c.was_reloading && !now_reloading && c.reload_rounds <= 0) {
            if (const ItemDef* d = held_def(e)) {
                if (c.reload_style != "mag" && c.reload_style != "belt") cue_sound(sound_for(*d, "reload_end"), position(e), e, 0.8f);
                if (c.reload_style.size() > 6 && c.reload_style.compare(c.reload_style.size() - 6, 6, "+cycle") == 0)
                    if (ItemStack* hs = held(e, c.reload_hand); hs && hs->loaded >= 1.0f) {
                        hs->action = "spent";
                        if (!by_hand(*d)) cycle_weapon(e, c.reload_hand);
                    }
            }
            c.reload_style.clear();
        }
        c.was_reloading = now_reloading;
        if (c.reload_rounds <= 0 || m_now < c.next_round) continue;
        ItemStack* s = held(e, c.reload_hand);
        const ItemDef* d = s ? item_def(s->item) : nullptr;
        if (!d || s->loaded >= static_cast<float>(d->magazine)) {
            c.reload_rounds = 0;
            c.reload_until = m_now;
            continue;
        }
        const float got = take_variant(e, c.reload_ammo, c.reload_variant, 1.0f);
        if (got < 1.0f) {
            c.reload_rounds = 0;
            c.reload_until = m_now;
            continue;
        }
        s = held(e, c.reload_hand);
        if (!s) continue;
        s->loaded += 1.0f;
        s->ammo = c.reload_ammo;
        s->variant = c.reload_variant;
        --c.reload_rounds;
        c.next_round += c.round_time;
        cue_sound(sound_for(*d, "round_in"), position(e), e, 0.6f);
        if (c.reload_rounds <= 0) {
            c.reload_until = m_now;
            cue_sound(sound_for(*d, "reload_end"), position(e), e, 0.8f);
            c.was_reloading = false;
            if (c.reload_style.size() > 6 && c.reload_style.compare(c.reload_style.size() - 6, 6, "+cycle") == 0) {
                s->action = "spent";
                if (!by_hand(*d)) cycle_weapon(e, c.reload_hand);
            }
            c.reload_style.clear();
        }
    }
}

bool Gameplay::finish_reload(Entity who) {
    auto it = m_carry.find(who.id());
    if (it == m_carry.end() || it->second.reload_rounds <= 0) return false;
    it->second.reload_rounds = 0;
    it->second.reload_until = m_now;
    return true;
}

std::string Gameplay::stack_enchant_effects(const ItemStack& s) const {
    std::string out;
    for (const std::string& w : words(s.enchant))
        if (w.find('=') == std::string::npos) out = join_list(out, w);
    return out;
}

std::string Gameplay::stack_enchant_damage(const ItemStack& s) const {
    std::string out;
    for (const std::string& w : words(s.enchant))
        if (w.find('=') != std::string::npos) out = join_list(out, w);
    return out;
}

void Gameplay::apply_hit_extras(Entity target, Entity attacker, const std::string& effects, const std::string& extra_damage, float scale) {
    if (!m_scene || !m_scene->alive(target)) return;
    const Entity h = holder(target);
    for (const auto& [type, amount] : parse_amounts(extra_damage)) {
        if (amount <= 0.0f) continue;
        Damage d;
        d.amount = amount * scale;
        d.type = type;
        d.attacker = attacker;
        d.key = "extra";
        apply_damage(target, d);
        if (type == "fire" && m_scene->alive(h))
            if (const Health* hp = m_scene->health(h); hp && hp->flammable) ignite(h, 3.0f);
    }
    if (!m_scene->alive(h)) return;
    for (const EffectRoll& r : parse_effect_rolls(effects))
        if (random(0.0, 1.0) < r.chance) apply_effect(h, r.effect, r.duration, r.strength, attacker);
}

float Gameplay::worn_resist(Entity e, const std::string& type) const {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return 0.0f;
    const Inventory* inv = m_scene->inventory(o);
    if (!inv) return 0.0f;
    float total = 0.0f;
    for (const auto& [slot, s] : inv->worn) {
        if (s.empty() || slot == "mainhand" || slot == "offhand") continue;
        const ItemDef* d = item_def(s.item);
        if (!d) continue;
        const std::map<std::string, float> r = parse_amounts(d->resists);
        if (const auto it = r.find(type); it != r.end()) total += it->second;
        else if (const auto all = r.find("all"); all != r.end() && type != "true") total += all->second;
    }
    return total;
}

ItemStack* Gameplay::cursor(Entity e) {
    const Entity o = inventory_owner(e);
    if (!m_scene || !m_scene->alive(o)) return nullptr;
    Carry& c = carry(o);
    if (c.cursor.empty()) c.cursor.resize(1);
    return &c.cursor[0];
}

void Gameplay::set_open_inventory(Entity viewer, Entity other) {
    const Entity o = inventory_owner(viewer);
    if (!m_scene || !m_scene->alive(o)) return;
    carry(o).open_other = other;
}

bool Gameplay::return_cursor(Entity e) {
    const Entity o = inventory_owner(e);
    ItemStack* cur = cursor(o);
    if (!cur || cur->empty()) return false;
    ItemStack st = *cur;
    st.x = st.y = -1;
    const float back = give_stack(o, st);
    cur = cursor(o);
    cur->amount -= back;
    if (cur->amount > 1e-6f) drop_item(o, "cursor", 0, -1.0f);
    cur = cursor(o);
    if (cur && cur->amount <= 1e-6f) cur->clear();
    return true;
}

void Gameplay::push_item_event(const ItemEvent& ev) {
    if (m_item_events.size() < 256) m_item_events.push_back(ev);
}

bool Gameplay::next_item_event(Entity e, ItemEvent& out) {
    const Entity o = inventory_owner(e);
    for (auto it = m_item_events.begin(); it != m_item_events.end(); ++it)
        if (!m_scene || !m_scene->alive(e) || it->owner == o || it->owner == e) {
            out = *it;
            m_item_events.erase(it);
            return true;
        }
    return false;
}

bool Gameplay::inventory_action(Entity viewer, Entity who_in, const std::string& list, int index, int x, int y, const std::string& action, const std::string& event) {
    const Entity owner = inventory_owner(viewer);
    if (!m_scene || !m_scene->alive(owner)) return false;
    const Entity who = m_scene->alive(inventory_owner(who_in)) ? inventory_owner(who_in) : owner;
    Inventory& inv = *m_scene->inventory(owner);
    ItemStack* cur = cursor(owner);
    ItemStack* src = list.empty() ? nullptr : stack_at(who, list, list.rfind("worn:", 0) == 0 || list.rfind("holster:", 0) == 0 ? 0 : index);
    ItemEvent ev;
    ev.owner = owner;
    ev.event = event;
    ev.action = action;
    ev.item = src && !src->empty() ? src->item : cur && !cur->empty() ? cur->item : std::string();
    ev.list = list;
    ev.index = index;
    push_item_event(ev);
    const int at = list.rfind("worn:", 0) == 0 || list.rfind("holster:", 0) == 0 ? 0 : index;
    auto half_of = [&](const ItemStack& s) {
        const ItemDef* d = item_def(s.item);
        if (d && d->continuous) return s.amount * 0.5f;
        return std::ceil(s.amount * 0.5f);
    };
    auto put = [&](float amount) -> bool {
        if (!cur || cur->empty()) return false;
        const ItemDef* cd = item_def(cur->item);
        const ItemDef* td = src && !src->empty() ? item_def(src->item) : nullptr;
        if (cd && td && cd->kind == ItemKind::magazine && td->kind == ItemKind::gun && magazine_fits(*td, *cd) && who == owner) {
            ItemStack mag = *cur;
            mag.amount = 1.0f;
            ItemStack old;
            if (!src->mag.empty()) {
                old.item = src->mag;
                old.amount = 1.0f;
                old.loaded = src->loaded;
                old.ammo = src->ammo;
                old.variant = src->variant;
            }
            src->mag = mag.item;
            src->loaded = mag.loaded;
            src->ammo = mag.ammo.empty() ? td->ammo : mag.ammo;
            src->variant = mag.variant;
            cur->amount -= 1.0f;
            if (cur->amount <= 1e-6f) cur->clear();
            if (!old.empty()) {
                if (cur->empty()) *cur = old;
                else give_stack(owner, old);
            }
            cue_sound("mag_in", position(owner), owner, 0.7f);
            return true;
        }
        if (cd && td && cd->kind == ItemKind::ammo && (td->kind == ItemKind::magazine || td->kind == ItemKind::gun)) {
            const bool takes = td->kind == ItemKind::magazine ? magazine_takes(*td, *cd) : ammo_fits(*td, *cd);
            const bool same = src->loaded <= 0.0f || (src->ammo == cur->item && src->variant == cur->variant);
            if (takes && same) {
                const float room = static_cast<float>(td->magazine) - src->loaded;
                const float n = std::floor(std::min({room, cur->amount, amount < 0.0f ? cur->amount : amount}));
                if (n < 1.0f) return false;
                src->loaded += n;
                src->ammo = cur->item;
                src->variant = cur->variant;
                cur->amount -= n;
                if (cur->amount <= 1e-6f) cur->clear();
                carry(owner).next_use = std::max(carry(owner).next_use, m_now + n * std::max(0.0f, m_scene->rule_number("load_time")));
                cue_sound("shell_insert", position(owner), owner, 0.6f);
                return true;
            }
        }
        if (list.empty()) return false;
        return move_item(owner, "cursor", 0, who, list, at, amount, x, y, false);
    };
    auto pick = [&](float amount) -> bool {
        if (!src || src->empty()) return false;
        if (cur && !cur->empty() && !can_merge(*cur, *src)) return put(-1.0f);
        return move_item(who, list, at, owner, "cursor", 0, amount);
    };
    if (action.empty() || action == "none" || action == "tooltip" || action == "inspect") return action == "inspect";
    if (action == "pick_all") return pick(-1.0f);
    if (action == "pick_half") return src && !src->empty() && pick(half_of(*src));
    if (action == "pick_one") return pick(1.0f);
    if (action == "pick_amount") return pick(std::max(0.0f, inv.place_amount));
    if (action == "place_all") return src && !src->empty() && cur && !cur->empty() && !can_merge(*cur, *src) && item_def(cur->item) && item_def(src->item) && !(item_def(cur->item)->kind == ItemKind::magazine || item_def(cur->item)->kind == ItemKind::ammo) ? move_item(owner, "cursor", 0, who, list, at, -1.0f, x, y, false) : put(-1.0f);
    if (action == "place_one") return put(1.0f);
    if (action == "place_half") return cur && !cur->empty() && put(half_of(*cur));
    if (action == "place_amount") return put(std::max(0.0f, inv.place_amount));
    if (action == "drop_cursor") return cur && !cur->empty() && m_scene->alive(drop_item(owner, "cursor", 0, -1.0f));
    if (action == "drop_one") return src && !src->empty() && m_scene->alive(drop_item(who, list, at, 1.0f));
    if (action == "drop_stack") return src && !src->empty() && m_scene->alive(drop_item(who, list, at, -1.0f));
    if (action == "rotate") {
        if (!cur || cur->empty()) return false;
        cur->rotated = !cur->rotated;
        return true;
    }
    if (action == "split") return src && !src->empty() && src->amount >= 2.0f && move_item(who, list, at, who, list, -1, half_of(*src));
    if (action == "quick_move") {
        if (!src || src->empty()) return false;
        Entity to = who != owner ? owner : carry(owner).open_other;
        if (m_scene->alive(to) && m_scene->inventory(to)) return move_item(who, list, at, to, "items", -1, -1.0f);
        if (list == "hotbar") return move_item(who, list, at, who, "items", -1, -1.0f);
        if (list == "items" && inv.hotbar && inv.hotbar_own && inv.style == InventoryStyle::slots) return move_item(who, list, at, who, "hotbar", -1, -1.0f);
        for (const auto& [slot, w] : inv.worn)
            if (!w.empty() && list != "container:" + slot)
                if (const ItemDef* cd = item_def(w.item); cd && cd->slots_w > 0 && move_item(who, list, at, who, "container:" + slot, -1, -1.0f)) return true;
        return false;
    }
    if (action == "use") return who == owner && src && !src->empty() && use_item(owner, list, at);
    if (action == "equip") {
        if (who != owner || !src || src->empty()) return false;
        if (list.rfind("worn:", 0) == 0) return unequip(owner, list.substr(5));
        return equip(owner, list, at);
    }
    if (action == "hold") return who == owner && src && !src->empty() && hold(owner, "mainhand", list, at);
    if (action == "load") {
        if (who != owner || !src || src->empty()) return false;
        const ItemDef* d = item_def(src->item);
        if (!d) return false;
        if (cur && !cur->empty()) return put(-1.0f);
        if (d->kind == ItemKind::magazine) {
            if (src->loaded >= static_cast<float>(d->magazine)) return unload_magazine(owner, list, at) > 0.0f;
            return load_magazine(owner, list, at) > 0.0f || unload_magazine(owner, list, at) > 0.0f;
        }
        if (d->kind == ItemKind::gun) {
            if (!src->mag.empty()) {
                ItemStack mag;
                mag.item = src->mag;
                mag.amount = 1.0f;
                mag.loaded = src->loaded;
                mag.ammo = src->ammo;
                mag.variant = src->variant;
                src->mag.clear();
                src->loaded = 0.0f;
                if (cur) *cur = mag;
                return true;
            }
            return src->loaded > 0.0f ? unload_magazine(owner, list, at) > 0.0f : load_magazine(owner, list, at) > 0.0f;
        }
        return false;
    }
    return false;
}

void Gameplay::cue_sound(const std::string& name, Vec3 at, Entity follow, float volume, float pitch, float loudness) {
    if (!m_scene || name.empty() || !m_scene->rule_on("sounds")) return;
    SoundCue c;
    c.clip = name.find('/') != std::string::npos || name.find('.') != std::string::npos ? name : "Assets/Sounds/Items/" + name + ".wav";
    c.at = at;
    c.follow = follow;
    c.volume = volume;
    c.pitch = pitch * static_cast<float>(random(0.94, 1.06));
    c.loudness = loudness;
    c.range = loudness > 0.0f ? std::max(30.0f, loudness * 2.0f) : 25.0f;
    if (m_sound_cues.size() < 512) m_sound_cues.push_back(c);
}

std::vector<SoundCue> Gameplay::take_sound_cues() {
    std::vector<SoundCue> out;
    out.swap(m_sound_cues);
    return out;
}

std::string Gameplay::sound_for(const ItemDef& d, const std::string& what) const {
    if (what == "fire" && !d.sound_fire.empty()) return d.sound_fire;
    if ((what == "reload" || what == "reload_start") && !d.sound_reload.empty()) return d.sound_reload;
    if (what == "aim" && !d.sound_aim.empty()) return d.sound_aim;
    if (what == "use" && !d.sound_use.empty()) return d.sound_use;
    const std::string& c = d.category;
    const std::string& sh = d.shape;
    if (what == "fire") {
        if (d.kind == ItemKind::bow) return sh.find("crossbow") != std::string::npos ? "crossbow" : "bow_release";
        if (d.kind == ItemKind::magic) return "spell";
        if (c == "Guns/Sci-fi") return "laser";
        if (c == "Guns/Black powder") return "musket";
        if (c == "Guns/Launchers") return "launcher";
        if (d.has("suppressed")) return "gunshot_suppressed";
        const ItemDef* a = item_def(d.ammo);
        if (a && a->cartridge_class == "rimfire") return "gunshot_rimfire";
        if (d.gun_class == "anti_materiel" || d.gun_class == "hmg" || (a && a->grain >= 600.0f && a->cartridge_class != "shotgun")) return "gunshot_heavy";
        if (d.gun_class == "lmg" || d.gun_class == "gpmg" || d.gun_class == "rotary") return "gunshot_mg";
        if ((sh == "revolver" || d.gun_class == "revolver") && d.damage >= 40.0f) return "gunshot_magnum";
        if (sh == "shotgun" || sh == "sawed_off") return "gunshot_shotgun";
        if (sh == "sniper") return "gunshot_sniper";
        if (sh == "smg") return "gunshot_smg";
        if (sh == "pistol" || sh == "revolver") return "gunshot_pistol";
        return "gunshot_rifle";
    }
    const std::string act = gun_action_of(d);
    if (what == "reload") return magazine_fed(d) ? "mag_out" : "shell_insert";
    if (what == "reload_start") {
        if (d.feed == "cylinder" || act == "revolver_da" || act == "revolver_sa") return "cylinder_open";
        if (act == "break") return "break_open";
        if (act == "muzzle") return "powder_pour";
        if (act == "pump" || act == "lever" || act == "rocket") return std::string();
        return "bolt_open";
    }
    if (what == "round_in") {
        if (act == "muzzle") return "ramrod";
        if (d.feed == "cylinder" || sh == "shotgun" || sh == "sawed_off" || act == "break" || act == "pump") return "shell_insert";
        return "round_in";
    }
    if (what == "reload_end") {
        if (magazine_fed(d)) return "mag_in";
        if (d.feed == "cylinder" || act == "revolver_da" || act == "revolver_sa") return "cylinder_close";
        if (act == "break") return "break_close";
        if (act == "muzzle") return "ramrod";
        if (act == "pump" || act == "lever" || act == "rocket") return std::string();
        return "bolt_close";
    }
    if (what == "unload") return "mag_out";
    if (what == "aim") return "aim";
    if (what == "swing") return d.damage_type == "blunt" ? "swing_heavy" : "swing";
    if (what == "use") {
        if (d.kind == ItemKind::drink || d.kind == ItemKind::potion) return "drink";
        if (d.kind == ItemKind::food) return "eat";
        if (d.kind == ItemKind::medical) return "bandage";
        return "equip";
    }
    if (what == "draw") return d.kind == ItemKind::bow ? "bow_draw" : "equip";
    if (what == "throw") return "throw";
    if (what == "empty") return d.sound_empty.empty() ? std::string("dry_fire") : d.sound_empty;
    return std::string();
}

void Gameplay::tick_animations(float dt) {
    if (!m_scene) return;
    const bool on = m_scene->rule_on("animations");
    for (auto& [id, c] : m_carry) {
        c.kick = std::max(0.0f, c.kick - dt * (2.0f + c.kick * 8.0f));
        c.swing = std::max(0.0f, c.swing - dt / 0.32f);
        const float want_aim = c.aiming ? 1.0f : 0.0f;
        c.aim_blend += (want_aim - c.aim_blend) * std::min(1.0f, dt * 12.0f);
        if (!on || !m_scene->alive(c.hand_visual)) continue;
        Transform* t = m_scene->transform(c.hand_visual);
        if (!t) continue;
        float dip = 0.0f, roll = 0.0f, pitch = 0.0f, slide = 0.0f, inward = 0.0f;
        const float pi = 3.14159265f;
        if (c.reload_until > m_now && c.reload_until > c.reload_start) {
            const float p = std::clamp(static_cast<float>((m_now - c.reload_start) / (c.reload_until - c.reload_start)), 0.0f, 1.0f);
            const float wave = std::sin(p * pi);
            const float hold = std::min(1.0f, std::min(p, 1.0f - p) * 8.0f);
            const std::string style = c.reload_style.substr(0, c.reload_style.find('+'));
            if (style == "cylinder") {
                roll = hold * 70.0f;
                pitch = p < 0.25f ? std::sin(p / 0.25f * pi) * 55.0f : 0.0f;
                inward = hold * 0.08f;
            } else if (style == "break") {
                pitch = -hold * 38.0f;
                dip = hold * 0.06f;
                inward = hold * 0.05f;
            } else if (style == "bolt" || style == "tube") {
                roll = hold * (style == "bolt" ? 35.0f : 55.0f);
                dip = hold * 0.04f;
                inward = hold * 0.05f;
                const float per = c.round_time > 0.0f ? std::fmod(static_cast<float>(m_now - c.reload_start), c.round_time) / c.round_time : 0.0f;
                slide = std::sin(per * pi) * 0.012f * hold;
            } else if (style == "muzzle") {
                pitch = hold * 65.0f;
                dip = hold * 0.1f;
                inward = hold * 0.06f;
                slide = std::fabs(std::sin(p * pi * 6.0f)) * 0.02f * hold;
            } else if (style == "belt") {
                roll = hold * 25.0f;
                pitch = -hold * 10.0f;
                dip = wave * 0.08f;
            } else {
                dip = wave * 0.16f;
                roll = wave * 40.0f;
            }
        }
        if (!c.cycle_kind.empty() && c.cycle_until > m_now && c.cycle_until > c.cycle_start) {
            const float p = std::clamp(static_cast<float>((m_now - c.cycle_start) / (c.cycle_until - c.cycle_start)), 0.0f, 1.0f);
            const float wave = std::sin(p * pi);
            const std::string& k = c.cycle_kind;
            if (k == "bolt" || k == "straight_pull") {
                roll += wave * (k == "bolt" ? 22.0f : 10.0f);
                slide += std::sin(p * pi) * 0.05f;
                dip += wave * 0.02f;
            } else if (k == "pump") {
                slide += std::sin(p * pi) * 0.08f;
                pitch -= wave * 6.0f;
            } else if (k == "lever") {
                pitch -= wave * 20.0f;
                dip += wave * 0.035f;
            } else if (k == "hammer") {
                roll += wave * 12.0f;
                pitch += wave * 6.0f;
            } else if (k == "rack" || k == "release" || k == "charge") {
                roll += wave * (k == "release" ? 15.0f : 35.0f);
                inward += wave * 0.06f;
                pitch += wave * 8.0f;
                slide += wave * 0.02f;
            } else if (k == "clear_jam") {
                roll += wave * 55.0f;
                inward += wave * 0.07f;
                dip += wave * 0.06f;
                slide += std::fabs(std::sin(p * pi * 3.0f)) * 0.03f;
            }
        }
        const float sw = c.swing > 0.0f ? std::sin((1.0f - c.swing) * pi) : 0.0f;
        const float draw = c.drawing ? std::min(1.0f, c.draw / 0.8f) : 0.0f;
        Vec3 pos = c.hand_pos;
        pos.x *= 1.0f - c.aim_blend * 0.85f - inward * 4.0f;
        pos.y += c.aim_blend * 0.12f - dip;
        pos.z += c.kick * 0.06f + draw * 0.12f - sw * 0.25f + slide;
        t->position = pos;
        const Quat extra = from_euler(Vec3{c.kick * 14.0f - sw * 70.0f * c.swing_dir + pitch, sw * 25.0f, roll});
        t->rotation = normalize(c.hand_rot * extra);
    }
}

Entity Gameplay::looked_at(Entity viewer, float range, bool items, bool everything) {
    if (!m_scene || !m_physics || !m_scene->alive(viewer)) return Entity{};
    Vec3 origin, dir;
    aim_of(viewer, origin, dir);
    Vec3 p, n;
    float dist = 0.0f;
    const Entity hit = ray_skip(origin, dir, std::max(0.5f, range), viewer, &p, &n, &dist);
    if (!m_scene->alive(hit)) return Entity{};
    for (Entity at = hit; m_scene->alive(at); at = m_scene->parent(at)) {
        if (m_scene->has_tag(at, "worn-visual")) continue;
        if (m_scene->pickup(at)) return items ? at : Entity{};
        if (m_scene->health(at) || m_scene->agent(at) || m_scene->inventory(at)) return at;
    }
    const Entity owner = m_scene->body_owner(hit);
    if (m_scene->alive(owner) && (m_scene->health(owner) || m_scene->inventory(owner))) return owner;
    return everything ? hit : Entity{};
}

void Gameplay::set_look_info(Entity e, const std::string& text) {
    if (text.empty()) m_look_info.erase(e.id());
    else m_look_info[e.id()] = text;
}

std::string Gameplay::look_info(Entity e) const {
    const auto it = m_look_info.find(e.id());
    return it == m_look_info.end() ? std::string() : it->second;
}

std::string Gameplay::describe(Entity e, const std::string& lines, Entity viewer) {
    if (!m_scene || !m_scene->alive(e)) return std::string();
    const ItemCatalog& cat = m_scene->items();
    const Pickup* pk = m_scene->pickup(e);
    const ItemDef* item = pk ? item_def(pk->stack.item) : nullptr;
    const Entity h = m_scene->health(e) ? e : holder(e);
    const Health* hp = m_scene->alive(h) ? m_scene->health(h) : nullptr;
    const Entity o = inventory_owner(e);
    auto value = [&](const std::string& key) -> std::string {
        if (key == "name") return pk ? item_display_name(cat, pk->stack) : m_scene->name(e);
        if (key == "source") {
            if (item) return cat.custom.count(item->id) ? "Made for this game" : "Orchard";
            if (m_scene->agent(e)) return "Character";
            return std::string();
        }
        if (key == "category") return item ? item->category : std::string();
        if (key == "health") return hp ? fmt(hp->value) : std::string();
        if (key == "max_health") return hp ? fmt(hp->max) : std::string();
        if (key == "shield") return hp && hp->max_shield > 0.0f ? fmt(hp->shield) : std::string();
        if (key == "armor") {
            if (!hp) return std::string();
            const float a = hp->armor + worn_armor(h, "physical");
            return a > 0.0f ? fmt(a) + " armor" : std::string();
        }
        if (key == "effects") {
            std::string out;
            for (const std::string& ef : effects(h)) out = join_list(out, ef);
            return out;
        }
        if (key == "item") {
            if (pk) return pk->stack.amount != 1.0f ? "x" + fmt(pk->stack.amount, item && item->continuous ? 2 : 0) : std::string();
            if (m_scene->alive(o))
                if (const ItemStack* s = held(o); s && !s->empty()) return "Holding " + item_display_name(cat, *s);
            return std::string();
        }
        if (key == "amount") return pk ? fmt(pk->stack.amount, item && item->continuous ? 2 : 0) : std::string();
        if (key == "loaded") {
            const ItemStack* s = pk ? &pk->stack : nullptr;
            if (!s && m_scene->alive(o)) s = held(o);
            const ItemDef* d = s ? item_def(s->item) : nullptr;
            if (!d || d->magazine <= 0) return std::string();
            return fmt(s->loaded) + " / " + std::to_string(d->magazine) + " rounds";
        }
        if (key == "ammo") {
            const ItemStack* s = pk ? &pk->stack : nullptr;
            if (!s && m_scene->alive(o)) s = held(o);
            if (!s || s->ammo.empty()) return std::string();
            ItemStack r;
            r.item = s->ammo;
            r.variant = s->variant;
            return item_display_name(cat, r);
        }
        if (key == "distance") return m_scene->alive(viewer) ? fmt(distance(viewer, e), 1) + " m" : std::string();
        if (key == "stamina") return m_scene->survival(h) ? fmt(stat(h, "stamina")) : std::string();
        if (key == "hunger") return m_scene->survival(h) ? fmt(stat(h, "hunger")) : std::string();
        if (key == "thirst") return m_scene->survival(h) ? fmt(stat(h, "thirst")) : std::string();
        if (key == "tags") {
            std::string out;
            for (const std::string& t : m_scene->tags(e))
                if (t != "pickup" && t != "worn-visual") out = join_list(out, t);
            return out;
        }
        if (key == "info") return look_info(e);
        if (key == "inventory") {
            const Inventory* inv = m_scene->inventory(e);
            if (!inv) return std::string();
            int n = 0;
            for (const ItemStack& s : inv->items) n += s.empty() ? 0 : 1;
            return std::to_string(n) + " stacks inside";
        }
        if (key.rfind("stat:", 0) == 0) return m_scene->survival(h) ? fmt(stat(h, key.substr(5)), 1) : std::string();
        return std::string();
    };
    std::string out;
    std::string line;
    for (char ch : lines + "\n") {
        if (ch != '\n') {
            line += ch;
            continue;
        }
        if (line == "{health_bar}" || line == "{shield_bar}" || line.rfind("{bar:", 0) == 0) {
            float v = 0.0f, mx = 0.0f;
            std::string kind = "health";
            if (line == "{health_bar}" && hp) {
                v = hp->value;
                mx = hp->max;
            } else if (line == "{shield_bar}" && hp) {
                v = hp->shield;
                mx = hp->max_shield;
                kind = "shield";
            } else if (line.rfind("{bar:", 0) == 0 && m_scene->survival(h)) {
                kind = line.substr(5, line.size() - 6);
                v = stat(h, kind);
                mx = stat(h, "max_" + kind);
            }
            if (mx > 0.0f) out += "#bar " + kind + " " + fmt(v, 2) + " " + fmt(mx, 2) + "\n";
            line.clear();
            continue;
        }
        std::string expanded;
        bool had = false, filled = false;
        for (std::size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '{') {
                const std::size_t end = line.find('}', i);
                if (end != std::string::npos) {
                    const std::string v = value(line.substr(i + 1, end - i - 1));
                    had = true;
                    filled = filled || !v.empty();
                    expanded += v;
                    i = end;
                    continue;
                }
            }
            expanded += line[i];
        }
        if ((!had || filled) && !expanded.empty()) out += expanded + "\n";
        line.clear();
    }
    if (!out.empty()) out.pop_back();
    return out;
}

} // namespace orchard

namespace orchard {

bool Gameplay::by_hand(const ItemDef& d) const {
    if (d.cycle == "manual") return true;
    if (d.cycle == "auto") return false;
    return m_scene && (m_scene->rule_on("manual_cycle") || m_scene->rule("cycle") == "manual");
}

bool Gameplay::needs_working(const ItemDef& d) const {
    if (manual_action(d)) return true;
    return gun_action_of(d) == "revolver_da" && m_scene && m_scene->rule_on("revolver_hammer");
}

std::string Gameplay::cycle_kind_for(const ItemDef& d) const {
    const std::string act = gun_action_of(d);
    if (act == "bolt" || act == "straight_pull" || act == "pump" || act == "lever") return act;
    return "hammer";
}

std::vector<std::string> Gameplay::modes_for(const ItemDef& d) const {
    std::vector<std::string> modes = fire_modes_of(d);
    if (m_scene && m_scene->rule_on("burst5") && std::find(modes.begin(), modes.end(), "auto") != modes.end() && std::find(modes.begin(), modes.end(), "burst5") == modes.end()) modes.push_back("burst5");
    return modes;
}

std::vector<std::string> Gameplay::fire_modes(Entity e, const std::string& hand) {
    const Entity o = inventory_owner(e);
    ItemStack* s = m_scene ? held(m_scene->alive(o) ? o : e, hand) : nullptr;
    const ItemDef* d = s ? item_def(s->item) : nullptr;
    if (!d || d->kind != ItemKind::gun) return {};
    return modes_for(*d);
}

bool Gameplay::cycling(Entity e) const {
    const Entity o = inventory_owner(e);
    const Carry* c = carry_of(m_scene && m_scene->alive(o) ? o : e);
    return c && !c->cycle_kind.empty() && c->cycle_until > m_now;
}

float Gameplay::action_phase(Entity e) const {
    const Entity o = inventory_owner(e);
    const Carry* c = carry_of(m_scene && m_scene->alive(o) ? o : e);
    if (!c || c->cycle_kind.empty() || c->cycle_until <= c->cycle_start) return 0.0f;
    return std::clamp(static_cast<float>((m_now - c->cycle_start) / (c->cycle_until - c->cycle_start)), 0.0f, 1.0f);
}

std::string Gameplay::action_kind(Entity e) const {
    const Entity o = inventory_owner(e);
    const Carry* c = carry_of(m_scene && m_scene->alive(o) ? o : e);
    if (!c) return std::string();
    if (!c->cycle_kind.empty() && c->cycle_until > m_now) return c->cycle_kind;
    if (c->reload_until > m_now) return "reload:" + c->reload_style;
    return std::string();
}

void Gameplay::queue_sound(Carry& c, double at, const std::string& name) {
    if (name.empty()) return;
    c.sound_queue.emplace_back(at, name);
}

void Gameplay::eject_casing(Entity who, const ItemDef& gun, const std::string& ammo) {
    if (!m_scene || !m_scene->rule_on("casings")) return;
    const ItemDef* a = item_def(ammo.empty() ? gun.ammo : ammo);
    const std::string klass = a ? a->cartridge_class : std::string();
    if (gun_action_of(gun) == "energy" || klass == "airgun" || gun.feed == "muzzle") return;
    const Vec3 at = position(who);
    const Entity fx = add_effect(*m_scene, klass == "shotgun" ? "shell-eject" : "casing-eject", Entity{}, 0.3f, m_now);
    Carry& c = carry(who);
    if (m_scene->alive(fx)) {
        if (Transform* t = m_scene->transform(fx)) {
            Vec3 o, d;
            aim_of(who, o, d);
            const Vec3 right = normalize(cross(d, Vec3{0.0f, 1.0f, 0.0f}));
            t->position = o + d * 0.35f + right * 0.12f - Vec3{0.0f, 0.1f, 0.0f};
        }
        m_temporary.emplace_back(fx, m_now + 1.5);
    }
    std::string ground = surface_at(at);
    std::string sound = klass == "shotgun" ? "casing_shell" : "casing_brass";
    if (const SurfaceDef* sd = surface_def(ground); sd && (sd->soft || sd->volume)) sound = "casing_soft";
    if (ground == "water" || ground == "water_shallow") sound = "casing_soft";
    queue_sound(c, m_now + 0.35 + random(0.0, 0.25), sound);
}

void Gameplay::start_cycle(Entity who, const std::string& kind, float seconds, const std::string& hand) {
    Carry& c = carry(who);
    const double t0 = m_now;
    const double T = std::max(0.05f, seconds);
    c.cycle_kind = kind;
    c.cycle_start = t0;
    c.cycle_until = t0 + T;
    c.cycle_hand = hand;
    const ItemDef* d = held_def(who, hand);
    if (d && !d->sound_cycle.empty()) {
        queue_sound(c, t0, d->sound_cycle);
        if (kind != "rack" && kind != "release" && kind != "hammer") queue_sound(c, t0 + T * 0.3, "@eject");
        return;
    }
    if (kind == "bolt" || kind == "straight_pull") {
        queue_sound(c, t0 + T * 0.08, "bolt_open");
        queue_sound(c, t0 + T * 0.3, "@eject");
        queue_sound(c, t0 + T * 0.68, "bolt_close");
    } else if (kind == "pump") {
        queue_sound(c, t0 + T * 0.05, "pump_back");
        queue_sound(c, t0 + T * 0.25, "@eject");
        queue_sound(c, t0 + T * 0.55, "pump_forward");
    } else if (kind == "lever") {
        queue_sound(c, t0 + T * 0.05, "lever_open");
        queue_sound(c, t0 + T * 0.28, "@eject");
        queue_sound(c, t0 + T * 0.6, "lever_close");
    } else if (kind == "hammer") {
        queue_sound(c, t0 + T * 0.25, "hammer_cock");
    } else if (kind == "rack") {
        queue_sound(c, t0 + T * 0.15, "slide_rack");
    } else if (kind == "release") {
        queue_sound(c, t0 + T * 0.1, "slide_release");
    } else if (kind == "charge") {
        queue_sound(c, t0 + T * 0.2, "charging_handle");
    } else if (kind == "clear_jam") {
        queue_sound(c, t0 + T * 0.15, "mag_out");
        queue_sound(c, t0 + T * 0.45, "slide_rack");
        queue_sound(c, t0 + T * 0.55, "@drop_round");
        queue_sound(c, t0 + T * 0.75, "mag_in");
        queue_sound(c, t0 + T * 0.9, "slide_rack");
    }
}

bool Gameplay::cycle_weapon(Entity e, const std::string& hand) {
    const Entity o = inventory_owner(e);
    const Entity who = m_scene && m_scene->alive(o) ? o : e;
    if (!m_scene || reloading(who) || cycling(who)) return false;
    ItemStack* s = held(who, hand);
    const ItemDef* d = s ? item_def(s->item) : nullptr;
    if (!d || d->kind != ItemKind::gun) return false;
    const std::string act = gun_action_of(*d);
    const std::string kind = cycle_kind_for(*d);
    const float speed = std::max(0.1f, m_scene->rule_number("cycle_speed") / 100.0f);
    const float base = kind == "hammer" ? (d->cycle_time > 0.0f && act == "revolver_sa" ? d->cycle_time : 0.35f) : d->cycle_time > 0.0f ? d->cycle_time : act == "bolt" ? 0.9f : act == "straight_pull" ? 0.55f : act == "pump" ? 0.45f : act == "lever" ? 0.5f : 0.45f;
    if (s->action == "jammed") {
        start_cycle(who, "clear_jam", 1.4f / speed, hand);
        return true;
    }
    if (s->action == "spent") {
        start_cycle(who, kind, base / speed, hand);
        return true;
    }
    if (s->action == "empty" && s->loaded >= 1.0f) {
        const bool worked = needs_working(*d);
        start_cycle(who, worked ? kind : act == "belt" || act == "rotary" ? std::string("charge") : std::string("rack"), (worked ? base : 0.45f) / speed, hand);
        return true;
    }
    if (s->action == "open" && s->loaded >= 1.0f) {
        start_cycle(who, "release", 0.25f / speed, hand);
        return true;
    }
    return false;
}

void Gameplay::tick_actions() {
    if (!m_scene) return;
    for (auto& [id, c] : m_carry) {
        const Entity who = Entity::from_id(id);
        if (!m_scene->alive(who)) continue;
        if (!c.sound_queue.empty()) {
            std::vector<std::pair<double, std::string>> keep;
            for (const auto& [at, name] : c.sound_queue) {
                if (at > m_now) {
                    keep.emplace_back(at, name);
                    continue;
                }
                if (name == "@eject") {
                    if (const ItemDef* d = held_def(who, c.cycle_hand)) {
                        ItemStack* s = held(who, c.cycle_hand);
                        eject_casing(who, *d, s ? s->ammo : std::string());
                    }
                } else if (name == "@drop_round") {
                    if (ItemStack* s = held(who, c.cycle_hand); s && s->loaded >= 1.0f) {
                        s->loaded -= 1.0f;
                        const Entity drop = spawn_item(s->ammo.empty() ? (held_def(who, c.cycle_hand) ? held_def(who, c.cycle_hand)->ammo : std::string()) : s->ammo, 1.0f, position(who) + Vec3{0.0f, 0.9f, 0.0f});
                        if (m_scene->alive(drop))
                            if (Pickup* p = m_scene->pickup(drop)) p->stack.variant = s->variant;
                    }
                } else {
                    cue_sound(name, position(who), who, 0.7f);
                }
            }
            c.sound_queue.swap(keep);
        }
        if (!c.cycle_kind.empty() && m_now >= c.cycle_until) {
            ItemStack* s = held(who, c.cycle_hand);
            const ItemDef* d = s ? item_def(s->item) : nullptr;
            if (s && d) {
                if (c.cycle_kind == "clear_jam") s->action = s->loaded >= 1.0f ? std::string() : d->slide_lock ? std::string("open") : std::string("empty");
                else s->action = s->loaded >= 1.0f ? std::string() : std::string("empty");
            }
            c.cycle_kind.clear();
        }
    }
}

std::string Gameplay::gun_state(Entity e, const std::string& hand) {
    const Entity o = inventory_owner(e);
    const Entity who = m_scene && m_scene->alive(o) ? o : e;
    ItemStack* s = m_scene ? held(who, hand) : nullptr;
    const ItemDef* d = s ? item_def(s->item) : nullptr;
    if (!d || d->kind != ItemKind::gun) return "none";
    if (reloading(who)) return "reloading";
    if (cycling(who)) return "cycling";
    if (s->action == "jammed" || s->action == "spent" || s->action == "open") return s->action;
    if (s->loaded < 1.0f) return "empty";
    if (s->action == "empty") return "unchambered";
    return "ready";
}

std::string Gameplay::fire_mode(Entity e, const std::string& hand) {
    const Entity o = inventory_owner(e);
    ItemStack* s = m_scene ? held(m_scene->alive(o) ? o : e, hand) : nullptr;
    const ItemDef* d = s ? item_def(s->item) : nullptr;
    if (!d || d->kind != ItemKind::gun) return std::string();
    const std::vector<std::string> modes = modes_for(*d);
    return !s->fire_mode.empty() && std::find(modes.begin(), modes.end(), s->fire_mode) != modes.end() ? s->fire_mode : modes.front();
}

bool Gameplay::set_fire_mode(Entity e, const std::string& mode, const std::string& hand) {
    const Entity o = inventory_owner(e);
    const Entity who = m_scene && m_scene->alive(o) ? o : e;
    ItemStack* s = m_scene ? held(who, hand) : nullptr;
    const ItemDef* d = s ? item_def(s->item) : nullptr;
    if (!d || d->kind != ItemKind::gun) return false;
    const std::vector<std::string> modes = modes_for(*d);
    if (std::find(modes.begin(), modes.end(), mode) == modes.end()) return false;
    if (s->fire_mode == mode) return true;
    s->fire_mode = mode;
    cue_sound("selector", position(who), who, 0.6f);
    return true;
}

bool Gameplay::next_fire_mode(Entity e, const std::string& hand) {
    const std::string now = fire_mode(e, hand);
    const Entity o = inventory_owner(e);
    ItemStack* s = m_scene ? held(m_scene->alive(o) ? o : e, hand) : nullptr;
    const ItemDef* d = s ? item_def(s->item) : nullptr;
    if (!d) return false;
    const std::vector<std::string> modes = modes_for(*d);
    if (modes.size() < 2) return false;
    auto it = std::find(modes.begin(), modes.end(), now);
    const std::size_t i = it == modes.end() ? 0 : static_cast<std::size_t>(it - modes.begin());
    return set_fire_mode(e, modes[(i + 1) % modes.size()], hand);
}

} // namespace orchard
