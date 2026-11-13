#include <xylem/xylem.hpp>
#include <orchard/effects.hpp>
#include <orchard/gameplay.hpp>
#include <orchard/units.hpp>
#include <orchard/ai/world.hpp>
#include <orchard/graph.hpp>

#include <cmath>
#include <filesystem>
#include <set>
#include <functional>
#include <memory>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <string>

namespace {

int failures = 0;
int checks = 0;

void check(bool ok, const std::string& what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << what << "\n";
    }
}

bool near(float a, float b, float eps) {
    return std::fabs(a - b) < eps;
}

std::string str(float v) {
    return std::to_string(v);
}

orchard::Entity ground(orchard::Scene& s, float width) {
    const orchard::Entity g = s.create("Ground");
    s.transform(g)->position = {0.0f, -0.5f, 0.0f};
    s.add_body(g).type = orchard::BodyType::fixed;
    s.add_collider(g)->size = {width, 1.0f, width};
    return g;
}

void run(xylem::World& w, float seconds) {
    for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i) w.step(1.0f / 60.0f);
}

void test_falling_3d() {
    orchard::Scene s(orchard::Dimension::three_d);
    ground(s, 20.0f);
    const orchard::Entity box = s.create("Box");
    s.transform(box)->position = {0.0f, 5.0f, 0.0f};
    s.add_body(box);
    s.add_collider(box);
    const orchard::Entity floating = s.create("Floating");
    s.transform(floating)->position = {5.0f, 5.0f, 0.0f};
    s.add_body(floating).set(orchard::body_gravity, false);
    s.add_collider(floating);
    xylem::World w(orchard::Dimension::three_d);
    w.build(s);
    check(w.body_count() == 3 && w.collider_count() == 3, "3d: three bodies built");
    run(w, 3.0);
    const float y = s.transform(box)->position.y;
    check(near(y, 0.5f, 0.05f), "3d: the box lands on the ground (y = " + str(y) + ")");
    check(near(s.transform(floating)->position.y, 5.0f, 0.001f), "3d: no gravity, no fall");
    check(w.touching(box, s.find("Ground")), "3d: the box touches the ground");
    check(!w.touching(floating, s.find("Ground")), "3d: the floating box doesn't");
}

void test_falling_2d() {
    orchard::Scene s(orchard::Dimension::two_d);
    ground(s, 20.0f);
    const orchard::Entity ball = s.create("Ball");
    s.transform(ball)->position = {0.0f, 4.0f, 0.0f};
    s.add_body(ball);
    s.add_collider(ball, orchard::ColliderShape::sphere);
    const orchard::Entity crate = s.create("Crate");
    s.transform(crate)->position = {4.0f, 0.5f, 0.0f};
    s.add_body(crate).set(orchard::body_lock_rotation_z, true);
    s.add_collider(crate);
    xylem::World w(orchard::Dimension::two_d);
    w.build(s);
    w.set_spin(crate, {0.0f, 0.0f, 90.0f});
    run(w, 3.0);
    const float y = s.transform(ball)->position.y;
    check(near(y, 0.5f, 0.05f), "2d: the ball lands on the ground (y = " + str(y) + ")");
    const orchard::Vec3 r = orchard::to_euler(s.transform(crate)->rotation);
    check(near(r.z, 0.0f, 0.01f), "2d: lock rotation keeps the crate upright (z = " + str(r.z) + ")");
    check(near(s.transform(ball)->position.z, 0.0f, 1e-5f), "2d: z stays put");
}

void test_fluids_2d() {
    for (orchard::FluidLevel level : {orchard::FluidLevel::still, orchard::FluidLevel::waves, orchard::FluidLevel::flowing, orchard::FluidLevel::realistic, orchard::FluidLevel::blocky}) {
        orchard::Scene s(orchard::Dimension::two_d);
        ground(s, 20.0f);
        const orchard::Entity pool = s.create("Pool");
        s.transform(pool)->position = {0.0f, 1.5f, 0.0f};
        orchard::Fluid& f = s.add_fluid(pool);
        f.level = level;
        f.size = {6.0f, 3.0f, 1.0f};
        f.fill = 0.6f;
        const orchard::Entity crate = s.create("Crate");
        s.transform(crate)->position = {0.0f, 2.5f, 0.0f};
        s.add_body(s.find("Crate"));
        orchard::Collider* c = s.add_collider(crate);
        c->density = 400.0f;
        xylem::World w(orchard::Dimension::two_d);
        w.build(s);
        const std::string name = orchard::to_string(level);
        check(w.fluid_count() == 1, "2d " + name + ": one fluid volume");
        run(w, 2.0);
        std::vector<xylem::FluidDraw> draws;
        w.fluid_draws(draws);
        check(draws.size() == 1 && draws[0].flat, "2d " + name + ": the fluid can be drawn");
        if (level != orchard::FluidLevel::still) check(!draws.empty() && draws[0].image_width > 0 && draws[0].image.size() == static_cast<std::size_t>(draws[0].image_width * draws[0].image_height * 4), "2d " + name + ": the fluid makes an image");
        const float y = s.transform(crate)->position.y;
        check(std::isfinite(y) && y > 0.2f && y < 4.0f, "2d " + name + ": a light crate stays near the surface (y = " + str(y) + ")");
        if (level == orchard::FluidLevel::realistic || level == orchard::FluidLevel::blocky) {
            const std::size_t poured = w.fluid_brush(xylem::FluidBrush::pour, {2.0f, 2.8f, 0.0f}, 0.3f, {0.0f, 0.0f, 0.0f});
            check(poured > 0, "2d " + name + ": pouring adds fluid");
            const std::size_t erased = w.fluid_brush(xylem::FluidBrush::erase, {0.0f, 0.5f, 0.0f}, 0.5f, {0.0f, 0.0f, 0.0f});
            check(erased > 0, "2d " + name + ": erasing removes fluid");
        }
        check(w.fluid_depth({-2.0f, 0.3f, 0.0f}) > 0.0f, "2d " + name + ": the bottom of the pool is under water");
    }
}

void test_compound_and_sensor() {
    orchard::Scene s(orchard::Dimension::three_d);
    ground(s, 20.0f);
    const orchard::Entity cart = s.create("Cart");
    s.transform(cart)->position = {0.0f, 3.0f, 0.0f};
    s.add_body(cart);
    const orchard::Entity left = s.create("Left", cart);
    s.transform(left)->position = {-1.0f, 0.0f, 0.0f};
    s.add_collider(left);
    const orchard::Entity right = s.create("Right", cart);
    s.transform(right)->position = {1.0f, 0.0f, 0.0f};
    s.add_collider(right);
    const orchard::Entity zone = s.create("Zone");
    s.transform(zone)->position = {0.0f, 1.0f, 0.0f};
    s.add_body(zone).type = orchard::BodyType::fixed;
    orchard::Collider* trigger = s.add_collider(zone);
    trigger->sensor = true;
    trigger->size = {4.0f, 2.0f, 4.0f};
    xylem::World w(orchard::Dimension::three_d);
    w.build(s);
    check(w.body_count() == 3 && w.collider_count() == 4, "a body made of its children's colliders");
    run(w, 3.0);
    check(near(s.transform(cart)->position.y, 0.5f, 0.06f), "the two-collider cart lands (y = " + str(s.transform(cart)->position.y) + ")");
    check(near(s.transform(left)->position.x, -1.0f, 1e-4f), "children keep their place in the body");
    check(w.touching(cart, zone), "the sensor sees the cart inside it");
}

void test_impulse_and_disabled() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity puck = s.create("Puck");
    orchard::Body& b = s.add_body(puck);
    b.set(orchard::body_gravity, false);
    b.angular_damping = 0.0f;
    s.add_collider(puck, orchard::ColliderShape::sphere);
    const orchard::Entity off = s.create("Off");
    s.add_body(off).enabled = false;
    s.add_collider(off);
    const orchard::Entity bare = s.create("Bare");
    s.add_body(bare);
    xylem::World w(orchard::Dimension::three_d);
    w.build(s);
    check(w.has_body(puck) && w.has_body(off) && !w.has_body(bare), "a body needs a collider");
    w.set_velocity(puck, {2.0f, 0.0f, 0.0f});
    run(w, 1.0);
    check(near(s.transform(puck)->position.x, 2.0f, 0.05f), "velocity moves the body (x = " + str(s.transform(puck)->position.x) + ")");
    check(near(w.velocity(puck).x, 2.0f, 0.01f), "velocity reads back");
    s.transform(puck)->position = {0.0f, 10.0f, 0.0f};
    w.moved(puck);
    w.step_once();
    check(near(s.transform(puck)->position.y, 10.0f, 0.01f), "teleporting a body");
    s.destroy(puck);
    run(w, 0.1);
    check(!w.has_body(puck), "destroyed entities leave the world");
}

} // namespace

void test_axes() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity g = s.create("Ground");
    s.transform(g)->position = {0.0f, 0.0f, -0.5f};
    s.add_body(g).type = orchard::BodyType::fixed;
    s.add_collider(g)->size = {20.0f, 20.0f, 1.0f};
    const orchard::Entity box = s.create("Box");
    s.transform(box)->position = {0.0f, 0.0f, 5.0f};
    s.add_body(box);
    s.add_collider(box);
    xylem::World w(orchard::Dimension::three_d, xylem::Settings::with_axes(orchard::Dimension::three_d, orchard::Axes::z_up));
    check(near(w.settings().gravity.z, -9.80665f, 1e-4f), "z-up: gravity points down z");
    w.build(s);
    run(w, 3.0);
    const orchard::Vec3 p = s.transform(box)->position;
    check(near(p.z, 0.5f, 0.05f) && near(p.y, 0.0f, 0.01f), "z-up: the box lands on a z-up floor (z = " + str(p.z) + ")");
    const xylem::Settings flat = xylem::Settings::with_axes(orchard::Dimension::two_d, orchard::Axes::z_up, orchard::YAxis2D::down);
    check(near(flat.gravity.y, 9.80665f, 1e-4f), "2d y-down: gravity points down the screen");
}

float slide(orchard::CombineMode box_mix, orchard::CombineMode project_mix) {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity g = ground(s, 60.0f);
    s.colliders(g)->front().friction = 1.0f;
    const orchard::Entity box = s.create("Box");
    s.transform(box)->position = {0.0f, 0.5f, 0.0f};
    s.add_body(box).set(orchard::body_sleep, false);
    orchard::Collider* c = s.add_collider(box);
    c->friction = 0.0f;
    c->friction_combine = box_mix;
    orchard::ProjectInfo info;
    info.friction_combine = project_mix;
    xylem::World w(orchard::Dimension::three_d, xylem::Settings::for_project(info));
    w.build(s);
    run(w, 0.2);
    w.set_velocity(box, {5.0f, 0.0f, 0.0f});
    run(w, 2.0);
    return s.transform(box)->position.x;
}

void test_friction_mix() {
    const float ice = slide(orchard::CombineMode::project_default, orchard::CombineMode::geometric_mean);
    const float grip = slide(orchard::CombineMode::maximum, orchard::CombineMode::geometric_mean);
    const float avg = slide(orchard::CombineMode::project_default, orchard::CombineMode::average);
    check(ice > 8.0f, "friction mix: geometric mean of 0 and 1 slides (" + str(ice) + ")");
    check(grip < 1.5f, "friction mix: a collider set to maximum grips (" + str(grip) + ")");
    check(avg > grip && avg < ice, "friction mix: the project's average is in between (" + str(avg) + ")");
}

float shoot(orchard::PhysicsQuality quality, bool fast_flag) {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity wall = s.create("Wall");
    s.transform(wall)->position = {5.0f, 0.0f, 0.0f};
    s.add_body(wall).type = orchard::BodyType::fixed;
    s.add_collider(wall)->size = {0.05f, 4.0f, 4.0f};
    const orchard::Entity ball = s.create("Ball");
    s.transform(ball)->position = {0.3f, 0.0f, 0.0f};
    orchard::Body& b = s.add_body(ball);
    b.set(orchard::body_gravity, false);
    b.linear_damping = 0.0f;
    b.set(orchard::body_continuous, fast_flag);
    s.add_collider(ball, orchard::ColliderShape::sphere)->size = {0.2f, 0.2f, 0.2f};
    xylem::Settings settings;
    settings.with_quality(quality);
    xylem::World w(orchard::Dimension::three_d, settings);
    w.build(s);
    w.set_velocity(ball, {300.0f, 0.0f, 0.0f});
    run(w, 0.5);
    return s.transform(ball)->position.x;
}

void test_quality() {
    const float arcade = shoot(orchard::PhysicsQuality::arcade, true);
    const float standard_plain = shoot(orchard::PhysicsQuality::standard, false);
    const float standard_fast = shoot(orchard::PhysicsQuality::standard, true);
    const float realistic = shoot(orchard::PhysicsQuality::realistic, false);
    check(arcade > 5.0f, "quality: arcade skips continuous collision, so a bullet tunnels (x = " + str(arcade) + ")");
    check(standard_plain > 5.0f, "quality: standard only sweeps bodies marked fast-moving (x = " + str(standard_plain) + ")");
    check(standard_fast < 5.0f, "quality: standard stops a fast-moving body at a thin wall (x = " + str(standard_fast) + ")");
    check(realistic < 5.0f, "quality: realistic sweeps every body (x = " + str(realistic) + ")");
    orchard::ProjectInfo info;
    info.physics = orchard::PhysicsSettings::preset(orchard::PhysicsQuality::high);
    const xylem::Settings from_project = xylem::Settings::for_project(info);
    check(from_project.physics.substeps == 2, "quality: Settings::for_project carries the project's physics block");
    orchard::Scene s(orchard::Dimension::three_d);
    ground(s, 20.0f);
    const orchard::Entity box = s.create("Box");
    s.transform(box)->position = {0.0f, 3.0f, 0.0f};
    s.add_body(box);
    s.add_collider(box);
    xylem::World w(orchard::Dimension::three_d, from_project);
    w.build(s);
    run(w, 3.0);
    check(near(s.transform(box)->position.y, 0.5f, 0.05f), "quality: high still lands boxes on the ground");
    w.set_physics(orchard::PhysicsSettings::preset(orchard::PhysicsQuality::arcade));
    check(w.settings().physics.quality == orchard::PhysicsQuality::arcade, "quality: set_physics switches live");
}

void test_layers_and_flags() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity g = ground(s, 20.0f);
    s.body(g)->collision_layer = 2;
    const orchard::Entity ghost = s.create("Ghost");
    s.transform(ghost)->position = {0.0f, 2.0f, 0.0f};
    s.add_body(ghost).collision_mask = ~2ull;
    s.add_collider(ghost);
    const orchard::Entity solid = s.create("Solid");
    s.transform(solid)->position = {3.0f, 2.0f, 0.0f};
    s.add_body(solid);
    s.add_collider(solid);
    const orchard::Entity can = s.create("Can");
    s.transform(can)->position = {-3.0f, 2.0f, 0.0f};
    s.add_body(can);
    s.add_collider(can, orchard::ColliderShape::cylinder);
    xylem::World w(orchard::Dimension::three_d);
    w.build(s);
    run(w, 2.0);
    check(s.transform(ghost)->position.y < -2.0f, "layers: a body that masks out the ground's layer falls through");
    check(near(s.transform(solid)->position.y, 0.5f, 0.05f), "layers: the plain box still lands");
    check(near(s.transform(can)->position.y, 0.5f, 0.06f), "shapes: a cylinder collider lands on its end (y = " + str(s.transform(can)->position.y) + ")");

    orchard::Body b;
    const std::uint64_t bit = xylem::flag_options().front().bit;
    const bool was = xylem::flag_on(b, bit);
    xylem::set_flag(b, bit, !was);
    check(xylem::flag_on(b, bit) != was && (b.flags_on | b.flags_off) == bit, "flags: toggling stores one override");
    xylem::set_flag(b, bit, was);
    check(b.flags_on == 0 && b.flags_off == 0, "flags: toggling back clears the override");
    b.max_speed = 5.0f;
    check(xylem::base_flags(b) != xylem::base_flags(orchard::Body{}), "flags: a speed cap turns speed clamping on");
}

void test_balls_settle() {
    struct Case {
        orchard::Vec3 push;
        orchard::Vec3 spin;
        float limit;
    };
    const Case cases[] = {{{0.05f, 0.0f, 0.02f}, {30.0f, 200.0f, 10.0f}, 30.0f}, {{0.0f, 0.0f, 0.0f}, {30.0f, 200.0f, 10.0f}, 30.0f}, {{1.0f, 0.0f, 0.3f}, {0.0f, 0.0f, 0.0f}, 60.0f}};
    for (const Case& c : cases) {
        orchard::Scene s(orchard::Dimension::three_d);
        ground(s, 400.0f);
        const orchard::Entity ball = s.create("Ball");
        s.transform(ball)->position = {0.0f, 3.0f, 0.0f};
        orchard::Body& b = s.add_body(ball);
        b.start_velocity = c.push;
        b.start_spin = c.spin;
        s.add_collider(ball, orchard::ColliderShape::sphere);
        xylem::World w(orchard::Dimension::three_d, xylem::Settings::for_project(orchard::ProjectInfo{}));
        w.build(s);
        float asleep_at = -1.0f;
        for (int i = 0; i < static_cast<int>(c.limit * 60.0f); ++i) {
            w.step(1.0f / 60.0f);
            if (!w.awake(ball)) {
                asleep_at = static_cast<float>(i) / 60.0f;
                break;
            }
        }
        check(asleep_at > 0.0f, "a ball pushed at " + str(orchard::length(c.push)) + " m/s with spin comes to rest and sleeps within " + str(c.limit) + " s");
    }
}

void test_start_motion() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity shot = s.create("Shot");
    orchard::Body& b = s.add_body(shot);
    b.set(orchard::body_gravity, false);
    b.start_velocity = {0.0f, 0.0f, -10.0f};
    b.start_spin = {0.0f, 90.0f, 0.0f};
    s.add_collider(shot, orchard::ColliderShape::sphere)->size = {0.2f, 0.2f, 0.2f};
    const orchard::Entity turned = s.create("Turned");
    s.transform(turned)->position = {10.0f, 0.0f, 0.0f};
    s.transform(turned)->rotation = orchard::from_euler({0.0f, 90.0f, 0.0f});
    orchard::Body& tb = s.add_body(turned);
    tb.set(orchard::body_gravity, false);
    tb.start_velocity = {0.0f, 0.0f, -10.0f};
    tb.motion_local = true;
    s.add_collider(turned, orchard::ColliderShape::sphere)->size = {0.2f, 0.2f, 0.2f};
    const orchard::Entity rocket = s.create("Rocket");
    s.transform(rocket)->position = {-10.0f, 0.0f, 0.0f};
    orchard::Body& rb = s.add_body(rocket);
    rb.set(orchard::body_gravity, false);
    rb.acceleration = {0.0f, 2.0f, 0.0f};
    s.add_collider(rocket);
    xylem::World w(orchard::Dimension::three_d);
    w.build(s);
    check(near(w.velocity(shot).z, -10.0f, 0.01f) && near(w.spin(shot).y, 90.0f, 0.5f), "starting velocity and spin apply when the world is built");
    const orchard::Vec3 tv = w.velocity(turned);
    check(near(orchard::length(tv), 10.0f, 0.01f) && std::fabs(tv.z) < 0.5f, "a local starting velocity follows the body's rotation");
    run(w, 1.0f);
    check(near(s.transform(shot)->position.z, -10.0f, 0.3f), "the shot moved 10 m in a second (z = " + str(s.transform(shot)->position.z) + ")");
    check(near(w.velocity(rocket).y, 2.0f, 0.1f) && near(s.transform(rocket)->position.y, 1.0f, 0.1f), "constant acceleration: 2 m/s after a second, 1 m up (v = " + str(w.velocity(rocket).y) + ")");
    orchard::Scene copy = *orchard::Scene::load(s.save());
    const orchard::Body* cb = copy.body(copy.find("Turned"));
    const orchard::Body* cr = copy.body(copy.find("Rocket"));
    check(cb && cb->motion_local && near(cb->start_velocity.z, -10.0f, 0.001f) && cr && near(cr->acceleration.y, 2.0f, 0.001f), "start motion round trips through a scene file");
}

void test_mass_and_locks() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity e = s.create("Crate");
    s.add_body(e);
    orchard::Collider& c = *s.add_collider(e);
    c.size = {2.0f, 1.0f, 1.0f};
    const orchard::Vec3 one{1.0f, 1.0f, 1.0f};
    check(near(orchard::collider_mass(c, one, orchard::Dimension::three_d), 2000.0f, 0.5f), "a 2 m^3 box of water weighs 2000 kg");
    std::string why;
    check(orchard::edit_collider_mass(c, orchard::MassEdit::mass, 500.0f, one, orchard::Dimension::three_d, &why) && near(c.density, 0.25f, 0.001f), "nothing locked: setting the mass changes the density");
    c.lock(orchard::collider_lock_mass, true);
    c.mass = 500.0f;
    check(orchard::edit_collider_mass(c, orchard::MassEdit::size_x, 4.0f, one, orchard::Dimension::three_d, &why) && near(orchard::collider_mass(c, one, orchard::Dimension::three_d), 500.0f, 0.01f) && near(c.density, 0.125f, 0.001f), "mass locked: a bigger box gets less dense");
    c.lock(orchard::collider_lock_size_x, true);
    check(orchard::edit_collider_mass(c, orchard::MassEdit::density, 0.5f, one, orchard::Dimension::three_d, &why) && near(c.size.x, 4.0f, 0.001f) && near(c.size.y * c.size.z, 0.25f, 0.01f), "mass and size X locked: more density shrinks Y and Z only");
    check(!orchard::edit_collider_mass(c, orchard::MassEdit::size_x, 1.0f, one, orchard::Dimension::three_d, &why) && !why.empty(), "a locked size direction can't be changed");
    c.lock(orchard::collider_lock_density, true);
    const float volume = orchard::collider_volume(c, one, orchard::Dimension::three_d);
    check(orchard::edit_collider_mass(c, orchard::MassEdit::size_y, c.size.y * 2.0f, one, orchard::Dimension::three_d, &why) && near(orchard::collider_volume(c, one, orchard::Dimension::three_d), volume, volume * 0.02f), "mass and density locked: stretching Y squeezes Z to keep the volume");
    c.lock(orchard::collider_lock_size, true);
    check(!orchard::edit_collider_mass(c, orchard::MassEdit::mass, 900.0f, one, orchard::Dimension::three_d, &why), "everything locked: the mass can't change");
    c.name = "Lid";
    orchard::Scene copy = *orchard::Scene::load(s.save());
    const orchard::Collider& cc = copy.colliders(copy.find("Crate"))->front();
    check(cc.name == "Lid" && cc.locks == c.locks && near(cc.mass, 500.0f, 0.01f), "collider name, locks and mass round trip");

    orchard::Scene scaled(orchard::Dimension::three_d);
    ground(scaled, 20.0f);
    const orchard::Entity big = scaled.create("Big");
    scaled.transform(big)->position = {0.0f, 3.0f, 0.0f};
    scaled.transform(big)->scale = {3.0f, 3.0f, 3.0f};
    scaled.add_body(big);
    orchard::Collider& bc = *scaled.add_collider(big);
    bc.mass = 10.0f;
    bc.lock(orchard::collider_lock_mass, true);
    xylem::World w(orchard::Dimension::three_d);
    w.build(scaled);
    w.impulse(big, {0.0f, 10.0f, 0.0f});
    check(near(w.velocity(big).y, 1.0f, 0.05f), "a locked 10 kg mass stays 10 kg when the entity is scaled up (v = " + str(w.velocity(big).y) + ")");
}

void test_character() {
    orchard::Scene s(orchard::Dimension::three_d);
    ground(s, 60.0f);
    const orchard::Entity wall = s.create("Wall");
    s.transform(wall)->position = {0.0f, 1.0f, -5.0f};
    s.add_body(wall).type = orchard::BodyType::fixed;
    s.add_collider(wall)->size = {10.0f, 2.0f, 0.5f};
    const orchard::Entity crate = s.create("Crate");
    s.transform(crate)->position = {4.0f, 0.5f, 0.0f};
    s.add_body(crate);
    s.add_collider(crate)->density = 0.05f;
    const orchard::Entity player = s.create("Player");
    s.transform(player)->position = {0.0f, 0.2f, 0.0f};
    orchard::Body& b = s.add_body(player);
    b.set(orchard::body_sleep, false);
    orchard::Collider* c = s.add_collider(player, orchard::ColliderShape::capsule);
    c->size = {0.6f, 1.8f, 0.6f};
    c->offset = {0.0f, 0.9f, 0.0f};
    c->friction = 0.0f;
    xylem::World w(orchard::Dimension::three_d);
    w.build(s);
    xylem::CharacterMove idle;
    xylem::CharacterState st;
    for (int i = 0; i < 60; ++i) {
        st = w.move_character(player, idle, 1.0f / 60.0f);
        w.step(1.0f / 60.0f);
    }
    check(st.body && st.grounded && st.ground == s.find("Ground"), "the player stands on the ground");
    check(near(s.transform(player)->position.y, 0.0f, 0.05f), "its feet are on the floor (y = " + str(s.transform(player)->position.y) + ")");
    xylem::CharacterMove forward;
    forward.wish = {0.0f, 0.0f, -4.0f};
    for (int i = 0; i < 180; ++i) {
        w.move_character(player, forward, 1.0f / 60.0f);
        w.step(1.0f / 60.0f);
    }
    const orchard::Vec3 p = s.transform(player)->position;
    check(p.z > -4.8f && p.z < -3.9f && near(p.y, 0.0f, 0.1f), "walking into a wall stops at it (z = " + str(p.z) + ")");
    const orchard::Quat r = s.transform(player)->rotation;
    check(near(r.w, 1.0f, 0.001f), "the player stays upright");
    xylem::CharacterMove jump;
    jump.jump = true;
    st = w.move_character(player, jump, 1.0f / 60.0f);
    check(st.jumped, "Space on the ground jumps");
    float top = 0.0f;
    for (int i = 0; i < 90; ++i) {
        w.step(1.0f / 60.0f);
        w.move_character(player, idle, 1.0f / 60.0f);
        top = std::max(top, s.transform(player)->position.y);
    }
    check(top > 1.0f && top < 1.6f && near(s.transform(player)->position.y, 0.0f, 0.1f), "a 5 m/s jump goes about 1.3 m up and lands (top = " + str(top) + ")");
    xylem::CharacterMove right;
    right.wish = {4.0f, 0.0f, 0.0f};
    s.transform(player)->position = {0.0f, 0.0f, 0.0f};
    w.moved(player);
    const float crate_x = s.transform(crate)->position.x;
    for (int i = 0; i < 120; ++i) {
        w.move_character(player, right, 1.0f / 60.0f);
        w.step(1.0f / 60.0f);
    }
    check(s.transform(crate)->position.x > crate_x + 1.0f, "the player pushes a light crate (x = " + str(s.transform(crate)->position.x) + ")");
    orchard::Vec3 hit_point;
    float d = -1.0f;
    const orchard::Entity hit = w.raycast_ignoring(player, s.transform(player)->position + orchard::Vec3{0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 5.0f, &hit_point, nullptr, &d);
    check(hit == s.find("Ground") && near(d, 1.0f, 0.1f), "raycast_ignoring skips the player's own capsule");
}

void test_running_stays_down() {
    orchard::Scene s(orchard::Dimension::three_d);
    for (int i = 0; i < 40; ++i) {
        const orchard::Entity tile = s.create("Tile");
        s.transform(tile)->position = {-40.0f + 2.0f * static_cast<float>(i), -0.5f + ((i * 7) % 5) * 0.006f, 0.0f};
        s.add_body(tile).type = orchard::BodyType::fixed;
        s.add_collider(tile)->size = {2.0f, 1.0f, 6.0f};
    }
    const orchard::Entity bump = s.create("Bump");
    s.transform(bump)->position = {0.0f, -0.05f, 0.0f};
    s.transform(bump)->rotation = orchard::from_euler({0.0f, 0.0f, 4.0f});
    s.add_body(bump).type = orchard::BodyType::fixed;
    s.add_collider(bump)->size = {3.0f, 0.2f, 6.0f};
    const orchard::Entity player = s.create("Player");
    s.transform(player)->position = {-36.0f, 0.05f, 0.0f};
    orchard::Body& b = s.add_body(player);
    b.set(orchard::body_sleep, false);
    b.continuous = orchard::BodyContinuous::on;
    orchard::Collider* c = s.add_collider(player, orchard::ColliderShape::capsule);
    c->size = {0.6f, 1.8f, 0.6f};
    c->offset = {0.0f, 0.9f, 0.0f};
    c->friction = 0.0f;
    xylem::World w(orchard::Dimension::three_d);
    w.build(s);
    xylem::CharacterMove run;
    run.wish = {15.0f, 0.0f, 0.0f};
    float top = -1.0f;
    int airborne = 0;
    for (int i = 0; i < 60 * 4; ++i) {
        const xylem::CharacterState st = w.move_character(player, run, 1.0f / 60.0f);
        w.step(1.0f / 60.0f);
        if (i > 30 && !st.grounded) ++airborne;
        if (i > 30) top = std::max(top, s.transform(player)->position.y);
    }
    check(top < 0.25f, "running at 15 m/s over seams and a bump doesn't launch the player (highest " + str(top) + " m)");
    check(airborne < 10, "and it stays on the ground (" + str(static_cast<float>(airborne)) + " frames in the air)");
}

void test_detailed_player_walks_hills() {
    for (orchard::HumanoidKind hk : {orchard::HumanoidKind::detailed, orchard::HumanoidKind::full_body}) {
        orchard::Scene s(orchard::Dimension::three_d);
        const orchard::Entity te = s.create("Terrain");
        orchard::Terrain& t = s.add_terrain(te);
        t = orchard::terrain_preset("hills");
        orchard::TerrainHeights heights;
        orchard::generate_heights(t, heights);
        s.transform(te)->position = orchard::Vec3{0.0f, -heights.height_at(0.0f, 0.0f), 0.0f};
        const orchard::Entity p = orchard::add_humanoid(s, hk, {}, "Player");
        orchard::Body& b = *s.body(p);
        b.set(orchard::body_sleep, false);
        b.continuous = orchard::BodyContinuous::on;
        std::vector<orchard::Entity> todo{p};
        while (!todo.empty()) {
            const orchard::Entity at = todo.back();
            todo.pop_back();
            if (std::vector<orchard::Collider>* cs = s.colliders(at))
                for (orchard::Collider& c : *cs) {
                    c.friction = 0.0f;
                    c.friction_combine = orchard::CombineMode::minimum;
                }
            for (orchard::Entity ch : s.children(at)) todo.push_back(ch);
        }
        s.transform(p)->position = orchard::Vec3{0.0f, 0.3f, 0.0f};
        xylem::World w(orchard::Dimension::three_d);
        w.build(s);
        int stuck = 0;
        orchard::Vec3 prev = s.transform(p)->position;
        for (int i = 0; i < 60 * 15; ++i) {
            const float a = static_cast<float>(i) / 60.0f * 0.15f;
            xylem::CharacterMove mv;
            mv.wish = orchard::Vec3{std::cos(a), 0.0f, std::sin(a)} * 4.0f;
            mv.ride_height = 0.05f;
            w.move_character(p, mv, 1.0f / 60.0f);
            w.step(1.0f / 60.0f);
            const orchard::Vec3 at = s.transform(p)->position;
            orchard::Vec3 d = at - prev;
            d.y = 0.0f;
            if (i > 60 && orchard::length(d) * 60.0f < 2.0f) ++stuck;
            prev = at;
        }
        check(stuck < 10, std::string("a ") + orchard::humanoid_label(hk) + " player walks over hills without its feet catching (" + str(static_cast<float>(stuck)) + " slow frames)");
    }
}

void test_sprinting_over_hills() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity te = s.create("Terrain");
    orchard::Terrain& t = s.add_terrain(te);
    t = orchard::terrain_preset("hills");
    orchard::TerrainHeights heights;
    orchard::generate_heights(t, heights);
    s.transform(te)->position = orchard::Vec3{0.0f, -heights.height_at(0.0f, 0.0f), 0.0f};
    const orchard::Entity p = orchard::add_humanoid(s, orchard::HumanoidKind::detailed, {}, "Player");
    orchard::Body& b = *s.body(p);
    b.set(orchard::body_sleep, false);
    b.continuous = orchard::BodyContinuous::on;
    std::vector<orchard::Entity> todo{p};
    while (!todo.empty()) {
        const orchard::Entity at = todo.back();
        todo.pop_back();
        if (std::vector<orchard::Collider>* cs = s.colliders(at))
            for (orchard::Collider& c : *cs) {
                c.friction = 0.0f;
                c.friction_combine = orchard::CombineMode::minimum;
            }
        for (orchard::Entity ch : s.children(at)) todo.push_back(ch);
    }
    s.transform(p)->position = orchard::Vec3{0.0f, 0.3f, 0.0f};
    xylem::World w(orchard::Dimension::three_d);
    w.build(s);
    int air = 0, slow = 0, frames = 0, steps = 0;
    float lift = 0.0f;
    orchard::Vec3 prev = s.transform(p)->position;
    orchard::Vec3 window = prev;
    for (int i = 0; i < 60 * 12; ++i) {
        const float a = static_cast<float>(i) / 60.0f * 0.5f;
        xylem::CharacterMove mv;
        mv.wish = orchard::Vec3{std::cos(a), 0.0f, std::sin(a)} * 15.0f;
        mv.ride_height = 0.05f;
        const xylem::CharacterState st = w.move_character(p, mv, 1.0f / 60.0f);
        steps += w.step(1.0f / 60.0f);
        const orchard::Vec3 at = s.transform(p)->position;
        prev = at;
        if (i < 60) continue;
        ++frames;
        if (!st.grounded) ++air;
        if (i % 15 == 0) {
            orchard::Vec3 d = at - window;
            d.y = 0.0f;
            if (orchard::length(d) * 4.0f < 15.0f * 0.6f) ++slow;
            window = at;
        }
        float down = -1.0f;
        w.raycast_ignoring(p, at + orchard::Vec3{0.0f, 2.0f, 0.0f}, orchard::Vec3{0.0f, -1.0f, 0.0f}, 50.0f, nullptr, nullptr, &down);
        if (down >= 0.0f) lift = std::max(lift, down - 2.0f);
    }
    check(air < frames / 50, "sprinting over hills at 15 m/s stays on the ground (" + str(static_cast<float>(air)) + " of " + str(static_cast<float>(frames)) + " frames in the air)");
    check(slow < 2, "and keeps its speed instead of stopping and starting (" + str(static_cast<float>(slow)) + " slow quarter-seconds, " + str(static_cast<float>(steps)) + " physics steps)");
    check(lift < 0.6f, "and is never thrown off a slope like a ramp (highest " + str(lift) + " m above the ground)");
}

void test_characters_collide_without_launching() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity ground = s.create("Ground");
    s.transform(ground)->position = {0.0f, -0.5f, 0.0f};
    s.add_body(ground).type = orchard::BodyType::fixed;
    s.add_collider(ground)->size = {80.0f, 1.0f, 80.0f};
    auto person = [&s](const char* name, orchard::Vec3 at) {
        const orchard::Entity e = orchard::add_humanoid(s, orchard::HumanoidKind::detailed, {}, name);
        orchard::Body& b = *s.body(e);
        b.set(orchard::body_sleep, false);
        b.continuous = orchard::BodyContinuous::on;
        std::vector<orchard::Entity> todo{e};
        while (!todo.empty()) {
            const orchard::Entity at2 = todo.back();
            todo.pop_back();
            if (std::vector<orchard::Collider>* cs = s.colliders(at2))
                for (orchard::Collider& c : *cs) {
                    c.friction = 0.0f;
                    c.friction_combine = orchard::CombineMode::minimum;
                }
            for (orchard::Entity ch : s.children(at2)) todo.push_back(ch);
        }
        s.transform(e)->position = at;
        return e;
    };
    for (float offset : {0.0f, 0.25f}) {
        const orchard::Entity player = person("Player", {-10.0f, 0.1f, 0.0f});
        const orchard::Entity runner = person("Runner", {10.0f, 0.1f, offset});
        xylem::World w(orchard::Dimension::three_d);
        w.build(s);
        float top = 0.0f, fastest = 0.0f;
        for (int i = 0; i < 60 * 3; ++i) {
            const orchard::Vec3 pp = s.transform(player)->position, rp = s.transform(runner)->position;
            orchard::Vec3 toward = rp - pp;
            toward.y = 0.0f;
            const orchard::Vec3 dir = orchard::length(toward) > 1e-3f ? orchard::normalize(toward) : orchard::Vec3{1.0f, 0.0f, 0.0f};
            xylem::CharacterMove a;
            a.wish = dir * 15.0f;
            a.ride_height = 0.05f;
            w.move_character(player, a, 1.0f / 60.0f);
            w.walk(runner, dir * -6.0f, false, 0.0f, 1.0f / 60.0f);
            w.step(1.0f / 60.0f);
            top = std::max({top, s.transform(player)->position.y, s.transform(runner)->position.y});
            fastest = std::max({fastest, w.velocity(player).y, w.velocity(runner).y});
        }
        check(top < 0.5f, "a player sprinting into a running character isn't launched, and neither is it (highest " + str(top) + " m, fastest upward " + str(fastest) + " m/s, offset " + str(offset) + ")");
        s.destroy(player);
        s.destroy(runner);
    }
}

void test_triangle_prism_lands() {
    for (orchard::Dimension dim : {orchard::Dimension::three_d, orchard::Dimension::two_d}) {
        orchard::Scene s(dim);
        const orchard::Entity floor_e = s.create("Floor");
        s.transform(floor_e)->position = {0.0f, -0.5f, 0.0f};
        s.add_body(floor_e).type = orchard::BodyType::fixed;
        s.add_collider(floor_e)->size = {20.0f, 1.0f, 20.0f};
        const orchard::Entity t = s.create("Triangle");
        s.transform(t)->position = {0.0f, 2.0f, 0.0f};
        s.add_body(t);
        orchard::Collider* c = s.add_collider(t, orchard::ColliderShape::triangle);
        c->triangle = orchard::TriangleRule::right;
        c->triangle_sides = {0.0f, 1.0f, 2.0f};
        orchard::fit_triangle_size(*c);
        xylem::World w(dim);
        w.build(s);
        for (int i = 0; i < 60 * 4; ++i) w.step(1.0f / 60.0f);
        const float y = s.transform(t)->position.y;
        const float v = orchard::length(w.velocity(t));
        check(y > 0.1f && y < 1.0f && v < 0.05f, std::string("a triangle prism lands on the floor and stops (") + (dim == orchard::Dimension::two_d ? "2D" : "3D") + ", height " + str(y) + ", speed " + str(v) + ")");
    }
}

void test_flattened_play() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity g = s.create("Ground");
    s.add_body(g).type = orchard::BodyType::fixed;
    s.add_collider(g)->size = {40.0f, 1.0f, 40.0f};
    s.transform(g)->position = {0.0f, -0.5f, 0.0f};
    const orchard::Entity pool = s.create("Pool");
    orchard::Fluid& w = s.add_fluid(pool);
    w.size = {6.0f, 2.0f, 6.0f};
    w.fill = 0.6f;
    s.transform(pool)->position = {8.0f, 1.0f, 0.0f};
    s.add_body(pool).type = orchard::BodyType::fixed;
    const float t = 0.2f;
    const orchard::Vec3 walls[5][2] = {{{6.0f + 2.0f * t, t, 6.0f + 2.0f * t}, {0.0f, -1.0f - t * 0.5f, 0.0f}}, {{t, 2.0f, 6.0f}, {3.0f + t * 0.5f, 0.0f, 0.0f}}, {{t, 2.0f, 6.0f}, {-3.0f - t * 0.5f, 0.0f, 0.0f}}, {{6.0f + 2.0f * t, 2.0f, t}, {0.0f, 0.0f, 3.0f + t * 0.5f}}, {{6.0f + 2.0f * t, 2.0f, t}, {0.0f, 0.0f, -3.0f - t * 0.5f}}};
    for (const auto& wl : walls) {
        orchard::Collider* c = s.add_collider(pool);
        c->size = wl[0];
        c->offset = wl[1];
    }
    const orchard::Entity cube = s.create("Cube");
    s.add_body(cube);
    s.add_collider(cube)->size = {1.0f, 1.0f, 1.0f};
    s.transform(cube)->position = {-4.0f, 6.0f, 0.0f};
    const orchard::Entity ball = s.create("Ball");
    s.add_body(ball);
    orchard::Collider* bc = s.add_collider(ball, orchard::ColliderShape::sphere);
    bc->density = 0.5f;
    s.transform(ball)->position = {8.0f, 4.0f, 0.5f};
    const orchard::Entity cone = s.create("Cone");
    s.add_body(cone);
    s.add_collider(cone, orchard::ColliderShape::cone)->size = {1.0f, 1.0f, 1.0f};
    s.transform(cone)->position = {-10.0f, 2.0f, 0.0f};
    const orchard::Entity p = orchard::add_humanoid(s, orchard::HumanoidKind::detailed, {}, "Player");
    orchard::Body& b = *s.body(p);
    b.set(orchard::body_lock_rotation_x, true);
    b.set(orchard::body_lock_rotation_y, true);
    b.set(orchard::body_lock_rotation_z, true);
    b.set(orchard::body_sleep, false);
    s.transform(p)->position = {0.0f, 0.2f, 0.0f};
    s.transform(p)->rotation = orchard::Quat{0.0f, 1.0f, 0.0f, 0.0f};
    xylem::World world(orchard::Dimension::two_d, xylem::Settings::flattened(xylem::Settings{}));
    world.build(s);
    check(world.fluid_count() == 1, "2D play of a 3D scene keeps its water");
    float cube_v = 0.0f;
    for (int i = 0; i < 60 * 4; ++i) {
        xylem::CharacterMove mv;
        mv.ride_height = 0.05f;
        const float at = static_cast<float>(i) / 60.0f;
        mv.wish = orchard::Vec3{at > 1.0f && at < 2.0f ? 3.0f : 0.0f, 0.0f, 0.0f};
        world.move_character(p, mv, 1.0f / 60.0f);
        world.step(1.0f / 60.0f);
        if (i == 50) cube_v = world.velocity(cube).y;
    }
    std::vector<xylem::FluidDraw> draws;
    world.fluid_draws(draws);
    check(draws.size() == 1 && draws[0].flat, "2D play draws the water flat");
    check(cube_v < -7.0f, "a cube in 2D play falls freely (" + str(cube_v) + " m/s after 0.83 s)");
    const float by = s.transform(ball)->position.y;
    check(by > 0.6f && by < 1.6f, "a half-density ball floats in the pool in 2D play, so the pool's front and back walls don't fill it (y = " + str(by) + ")");
    const orchard::Vec3 up = orchard::rotate(s.transform(p)->rotation, orchard::Vec3{0.0f, 1.0f, 0.0f});
    const float px = s.transform(p)->position.x;
    check(up.y > 0.99f && px > 2.0f && px < 4.0f, "a player turned to face the view stays upright and walks along X in 2D play (x = " + str(px) + ")");
    check(s.transform(cone)->position.y < 1.0f && s.transform(cone)->position.y > 0.2f, "a cone lands on its base in 2D play (y = " + str(s.transform(cone)->position.y) + ")");
    const xylem::FlatOutline front = xylem::flat_outline(s, pool, (*s.colliders(pool))[3]);
    const xylem::FlatOutline side = xylem::flat_outline(s, pool, (*s.colliders(pool))[1]);
    check(!front.in_plane && side.in_plane, "the pool's front wall is left out of 2D play and its side walls kept");
}

orchard::Entity add_box(orchard::Scene& s, const char* name, orchard::Vec3 at, bool dynamic = true) {
    const orchard::Entity e = s.create(name);
    s.transform(e)->position = at;
    s.add_body(e).type = dynamic ? orchard::BodyType::dynamic : orchard::BodyType::fixed;
    s.add_collider(e)->size = {1.0f, 1.0f, 1.0f};
    return e;
}

void test_gameplay_health() {
    orchard::Scene s(orchard::Dimension::three_d);
    add_box(s, "Ground", {0.0f, -0.5f, 0.0f}, false);
    (*s.colliders(s.find("Ground")))[0].size = {40.0f, 1.0f, 40.0f};
    const orchard::Entity a = add_box(s, "A", {0.0f, 0.5f, 0.0f});
    s.add_health(a);
    xylem::World w(orchard::Dimension::three_d);
    w.build(s);
    orchard::Gameplay g;
    g.start(&s, &w);
    check(g.damage(a, 30.0f) == 70.0f, "damage takes health away (" + str(g.health(a)) + ")");
    check(g.heal(a, 50.0f) == 100.0f, "healing stops at max health");
    g.set_invulnerable(a, true);
    g.damage(a, 50.0f);
    check(g.health(a) == 100.0f, "invulnerable things take no damage");
    g.set_invulnerable(a, false);
    g.tick(0.016, 0.016);
    s.health(a)->on_death = orchard::DeathAction::respawn;
    s.health(a)->respawn_delay = 0.5f;
    s.transform(a)->position = {5.0f, 3.0f, 0.0f};
    w.moved(a);
    g.damage(a, 500.0f);
    check(g.dead(a) && g.health(a) == 0.0f, "too much damage kills");
    g.tick(0.016, 0.032);
    check(g.died(a), "died() is true on the tick after death");
    double t = 0.032;
    for (int i = 0; i < 60; ++i) {
        t += 1.0 / 60.0;
        w.step(1.0f / 60.0f);
        g.tick(1.0 / 60.0, t);
    }
    check(!g.dead(a) && g.health(a) == 100.0f, "a respawning thing comes back with full health");
    check(std::fabs(s.transform(a)->position.x) < 0.5f, "and comes back where it started (x = " + str(s.transform(a)->position.x) + ")");
    const orchard::Entity b = add_box(s, "B", {3.0f, 0.5f, 0.0f});
    check(g.set_health(b, 40.0f) && s.health(b) && g.health(b) == 40.0f, "set_health gives health to a thing that had none");
    s.add_tag(b, "enemy");
    check(g.nearest_tagged("enemy", orchard::Vec3{}) == b, "nearest_tagged finds a tagged thing");
}

void test_gameplay_explosions() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity ground = add_box(s, "Ground", {0.0f, -0.5f, 0.0f}, false);
    (*s.colliders(ground))[0].size = {60.0f, 1.0f, 60.0f};
    const orchard::Entity near = add_box(s, "Near", {2.0f, 0.5f, 0.0f});
    (*s.colliders(near))[0].density = 0.05f;
    s.add_health(near);
    const orchard::Entity barrel = add_box(s, "Barrel", {-3.0f, 0.5f, 0.0f});
    orchard::Health& bh = s.add_health(barrel);
    bh.value = bh.max = 10.0f;
    bh.on_death = orchard::DeathAction::explode;
    bh.explode_radius = 5.0f;
    bh.explode_damage = 80.0f;
    const orchard::Entity far = add_box(s, "Far", {-7.0f, 0.5f, 0.0f});
    s.add_health(far);
    xylem::World w(orchard::Dimension::three_d);
    w.build(s);
    orchard::Gameplay g;
    g.start(&s, &w);
    const int hit = g.explode({0.0f, 0.5f, 0.0f}, 4.0f, 20000.0f, 60.0f);
    check(hit >= 2, "an explosion reaches the things around it (" + str(static_cast<float>(hit)) + ")");
    check(g.health(near) < 100.0f && g.health(near) > 0.0f, "an explosion hurts less with distance (" + str(g.health(near)) + ")");
    check(w.velocity(near).x > 1.0f, "an explosion pushes things away (" + str(w.velocity(near).x) + " m/s)");
    double t = 0.0;
    for (int i = 0; i < 30; ++i) {
        t += 1.0 / 60.0;
        w.step(1.0f / 60.0f);
        g.tick(1.0 / 60.0, t);
    }
    check(!s.alive(barrel), "an explosive barrel goes off when its health runs out");
    check(g.health(far) < 100.0f, "and its blast hurts things the first explosion didn't reach (" + str(g.health(far)) + ")");
    bool effects = false;
    for (orchard::Entity e : s.entities())
        if (s.emitter(e)) effects = true;
    check(effects, "explosions leave particle effects in the scene");
    int marks = 0;
    for (orchard::Entity e : s.entities())
        if (s.name(e) == "Scorch" && s.emitter(e) && s.emitter(e)->flat) {
            ++marks;
            const float y = orchard::mul_point(s.world_matrix(e), orchard::Vec3{}).y;
            check(std::fabs(y - 0.02f) < 0.05f, "a scorch mark lies on the ground (y = " + str(y) + ")");
        }
    check(marks >= 2, "each explosion leaves a scorch mark on the ground (" + str(static_cast<float>(marks)) + ")");
}

void test_gameplay_fire() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity crate = add_box(s, "Crate", {0.0f, 0.5f, 0.0f}, false);
    orchard::Health& h = s.add_health(crate);
    h.flammable = true;
    h.burn_damage = 10.0f;
    xylem::World w(orchard::Dimension::three_d);
    w.build(s);
    orchard::Gameplay g;
    g.start(&s, &w);
    g.ignite(crate, 3.0f);
    double t = 0.0;
    for (int i = 0; i < 60; ++i) {
        t += 1.0 / 60.0;
        g.tick(1.0 / 60.0, t);
    }
    check(g.burning(crate) && g.health(crate) < 95.0f && g.health(crate) > 80.0f, "burning hurts over time (" + str(g.health(crate)) + " after 1 s)");
    g.extinguish(crate);
    const float after = g.health(crate);
    for (int i = 0; i < 60; ++i) {
        t += 1.0 / 60.0;
        g.tick(1.0 / 60.0, t);
    }
    check(!g.burning(crate) && g.health(crate) == after, "extinguish puts the fire out");
    const orchard::Material* m = s.material(crate);
    check(m && m->color.x < 0.75f, "burning chars things darker (" + str(m ? m->color.x : 1.0f) + ")");
}

void test_effects() {
    for (const std::string& name : orchard::effect_names()) {
        orchard::Scene s(orchard::Dimension::three_d);
        const orchard::Entity fx = orchard::add_effect(s, name);
        int emitters = 0;
        for (orchard::Entity e : s.subtree(fx))
            if (s.emitter(e)) ++emitters;
        check(s.alive(fx) && emitters > 0, "effect " + name + " adds particle emitters (" + str(static_cast<float>(emitters)) + ")");
        std::string error;
        const std::optional<orchard::Scene> back = orchard::Scene::load(s.save(), &error);
        check(back && back->entities().size() == s.entities().size(), "effect " + name + " saves and loads: " + error);
    }
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity e = s.create("Barrel");
    orchard::Health& h = s.add_health(e);
    h.value = 42.0f;
    h.flammable = true;
    h.on_death = orchard::DeathAction::explode;
    s.add_tag(e, "barrel");
    s.add_tag(e, "red");
    std::string error;
    const std::optional<orchard::Scene> back = orchard::Scene::load(s.save(), &error);
    const orchard::Entity be = back ? back->find("Barrel") : orchard::Entity{};
    const orchard::Health* bh = back && back->alive(be) ? back->health(be) : nullptr;
    check(bh && bh->value == 42.0f && bh->flammable && bh->on_death == orchard::DeathAction::explode, "health saves and loads");
    check(back && back->has_tag(be, "barrel") && back->has_tag(be, "red"), "tags save and load");
}

void test_sprinting() {
    for (float speed : {15.0f, 30.0f, 45.0f}) {
        orchard::Scene s(orchard::Dimension::three_d);
        for (int i = 0; i < 40; ++i) {
            const orchard::Entity tile = s.create("Tile");
            s.transform(tile)->position = {-40.0f + 4.0f * static_cast<float>(i), -0.5f, 0.0f};
            s.add_body(tile).type = orchard::BodyType::fixed;
            s.add_collider(tile)->size = {4.0f, 1.0f, 8.0f};
        }
        const orchard::Entity p = orchard::add_humanoid(s, orchard::HumanoidKind::detailed, {}, "Player");
        orchard::Body& b = *s.body(p);
        b.set(orchard::body_sleep, false);
        b.continuous = orchard::BodyContinuous::on;
        s.transform(p)->position = {-38.0f, 0.1f, 0.0f};
        xylem::World w(orchard::Dimension::three_d);
        w.build(s);
        float lo = 1e9f, hi = -1e9f;
        int air = 0;
        for (int i = 0; i < 60 * 2; ++i) {
            xylem::CharacterMove mv;
            mv.ride_height = 0.05f;
            mv.wish = {speed, 0.0f, 0.0f};
            const xylem::CharacterState st = w.move_character(p, mv, 1.0f / 60.0f);
            w.step(1.0f / 60.0f);
            if (i > 40) {
                lo = std::min(lo, s.transform(p)->position.y);
                hi = std::max(hi, s.transform(p)->position.y);
                if (!st.grounded) ++air;
            }
        }
        check(hi - lo < 0.05f && air == 0, "sprinting at " + str(speed) + " m/s stays smooth on the ground (bob " + str(hi - lo) + " m, " + str(static_cast<float>(air)) + " frames in the air)");
    }
}

void test_render_smoothing() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity e = s.create("Ball");
    s.transform(e)->position = {0.0f, 10.0f, 0.0f};
    s.add_body(e);
    s.add_collider(e, orchard::ColliderShape::sphere);
    xylem::World w(orchard::Dimension::three_d);
    w.build(s);
    w.set_velocity(e, {12.0f, 0.0f, 0.0f});
    w.step(1.0f / 60.0f);
    const float x1 = s.transform(e)->position.x;
    w.step(0.5f / 60.0f);
    orchard::Vec3 pos;
    orchard::Quat rot;
    check(w.render_pose(e, pos, rot), "a moving body has a smoothed pose to draw");
    const float x0 = x1 - 12.0f / 60.0f;
    check(std::fabs(w.interpolation() - 0.5f) < 0.01f && pos.x > x0 + 0.05f && pos.x < x1 - 0.05f, "the drawn pose sits between physics steps (" + str(pos.x) + " between " + str(x0) + " and " + str(x1) + ")");
    w.step(0.5f / 60.0f);
    w.render_pose(e, pos, rot);
    check(std::fabs(pos.x - x1) < 0.02f, "and moves on smoothly after the next step (" + str(pos.x) + ")");
}

struct AiScene {
    orchard::Scene s{orchard::Dimension::three_d};
    std::unique_ptr<xylem::World> w;
    orchard::Gameplay g;
    orchard::ai::AiWorld ai;
    double t = 0.0;

    explicit AiScene(orchard::Dimension dim = orchard::Dimension::three_d) : s(dim) {
        const orchard::Entity ground = s.create("Ground");
        s.transform(ground)->position = {0.0f, -0.5f, 0.0f};
        s.add_body(ground).type = orchard::BodyType::fixed;
        s.add_collider(ground)->size = {80.0f, 1.0f, dim == orchard::Dimension::two_d ? 1.0f : 80.0f};
    }

    orchard::Entity wall(orchard::Vec3 at, orchard::Vec3 size) {
        const orchard::Entity e = s.create("Wall");
        s.transform(e)->position = at;
        s.add_body(e).type = orchard::BodyType::fixed;
        s.add_collider(e)->size = size;
        return e;
    }

    orchard::Entity body(const char* name, orchard::Vec3 at, float height = 1.8f) {
        const orchard::Entity e = s.create(name);
        s.transform(e)->position = at;
        orchard::Body& b = s.add_body(e);
        b.set(orchard::body_lock_rotation_x, true);
        b.set(orchard::body_lock_rotation_y, true);
        b.set(orchard::body_lock_rotation_z, true);
        b.set(orchard::body_sleep, false);
        orchard::Collider* c = s.add_collider(e, orchard::ColliderShape::capsule);
        c->size = {0.6f, height, 0.6f};
        c->offset = {0.0f, height * 0.5f, 0.0f};
        c->friction = 0.0f;
        return e;
    }

    orchard::Entity player(orchard::Vec3 at) {
        const orchard::Entity p = body("Player", at);
        s.add_tag(p, "player");
        s.add_health(p);
        return p;
    }

    orchard::Entity agent(const char* brain, orchard::Vec3 at) {
        const orchard::Entity e = body(brain, at);
        s.add_agent(e) = *orchard::Agent::preset(brain);
        s.add_health(e);
        return e;
    }

    void start() {
        w = std::make_unique<xylem::World>(s.dimension());
        w->build(s);
        g.start(&s, w.get());
        ai.start(&s, w.get(), &g);
        ai.random().seed(12345);
    }

    void run(double seconds, const std::function<void()>& each = {}) {
        const int n = static_cast<int>(seconds * 60.0);
        for (int i = 0; i < n; ++i) {
            t += 1.0 / 60.0;
            ai.tick(1.0 / 60.0, t);
            w->step(1.0f / 60.0f);
            g.tick(1.0 / 60.0, t);
            if (each) each();
        }
    }

    orchard::Vec3 pos(orchard::Entity e) const { return s.transform(e)->position; }
};

float flat_distance(orchard::Vec3 a, orchard::Vec3 b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
}

void test_ai_zombie_chases_and_bites() {
    AiScene a;
    const orchard::Entity p = a.player({0.0f, 0.05f, 0.0f});
    const orchard::Entity z = a.agent("runner", {0.0f, 0.05f, 10.0f});
    a.s.transform(z)->rotation = orchard::from_euler({0.0f, 180.0f, 0.0f});
    a.start();
    const float before = flat_distance(a.pos(p), a.pos(z));
    a.run(3.0);
    const float after = flat_distance(a.pos(p), a.pos(z));
    check(after < 3.0f && after < before, "a runner zombie sees the player and runs to them (" + str(before) + " m -> " + str(after) + " m)");
    a.run(3.0);
    check(a.g.health(p) < 100.0f, "and bites them (" + str(a.g.health(p)) + " health left)");
    check(a.ai.state(z) == "combat" && a.ai.target(z) == p, "its state is combat with the player as its target (" + a.ai.state(z) + ")");
    bool spotted = false;
    for (std::string e = a.ai.next_event(z); !e.empty(); e = a.ai.next_event(z)) spotted = spotted || e == "spotted";
    check(spotted, "it reports a spotted event");
}

void test_ai_heavy_hit_does_not_launch() {
    AiScene a;
    const orchard::Entity p = a.player({0.0f, 0.05f, 0.0f});
    const orchard::Entity h = a.agent("hunter", {0.0f, 0.05f, 6.0f});
    a.s.transform(h)->rotation = orchard::from_euler({0.0f, 180.0f, 0.0f});
    a.s.health(p)->on_death = orchard::DeathAction::nothing;
    a.start();
    float top = 0.0f;
    a.run(6.0, [&] { top = std::max(top, a.pos(p).y); });
    check(a.g.health(p) < 100.0f, "a hunter reaches the player and hits them (" + str(a.g.health(p)) + " health left)");
    check(top < 1.0f, "and its huge damage doesn't fling the player into the air (highest " + str(top) + " m)");
}

void test_grenade_does_not_throw_people() {
    AiScene a;
    const orchard::Entity p = a.player({0.0f, 0.05f, 0.0f});
    orchard::Collider& pc = (*a.s.colliders(p))[0];
    pc.mass = 80.0f;
    pc.lock(orchard::collider_lock_mass, true);
    a.s.health(p)->on_death = orchard::DeathAction::nothing;
    const orchard::Entity crate = a.s.create("Crate");
    a.s.transform(crate)->position = {-1.5f, 0.5f, 0.0f};
    a.s.add_body(crate);
    orchard::Collider* cc = a.s.add_collider(crate);
    cc->size = {0.6f, 0.6f, 0.6f};
    cc->mass = 10.0f;
    cc->lock(orchard::collider_lock_mass, true);
    a.start();
    a.run(0.2);
    a.g.explode({-1.0f, 0.1f, 0.0f}, 4.5f, 15000.0f, 90.0f);
    const float kick = orchard::length(a.w->velocity(p));
    const float toss = orchard::length(a.w->velocity(crate));
    float top = 0.0f;
    a.run(2.0, [&] { top = std::max(top, a.pos(p).y); });
    check(a.g.health(p) < 100.0f, "a grenade going off a metre away hurts the player (" + str(a.g.health(p)) + " health left)");
    check(kick < 3.0f, "but only staggers them, like a real blast would (" + str(kick) + " m/s)");
    check(top < 0.3f, "and doesn't throw them into the air (highest " + str(top) + " m)");
    check(toss > kick * 2.0f, "while a light crate next to it gets blown away (" + str(toss) + " m/s)");
}

void test_ai_knockback_is_separate_from_damage() {
    auto shove = [](float damage, float knockback, float& lost) {
        AiScene a;
        const orchard::Entity p = a.player({0.0f, 0.05f, 0.0f});
        orchard::Collider& pc = (*a.s.colliders(p))[0];
        pc.mass = 80.0f;
        pc.lock(orchard::collider_lock_mass, true);
        a.s.health(p)->on_death = orchard::DeathAction::nothing;
        a.s.health(p)->max = a.s.health(p)->value = 100000.0f;
        const orchard::Entity h = a.agent("brawler", {0.0f, 0.05f, 4.0f});
        a.s.transform(h)->rotation = orchard::from_euler({0.0f, 180.0f, 0.0f});
        a.s.agent(h)->attack_damage = damage;
        a.s.agent(h)->attack_knockback = knockback;
        a.start();
        float fastest = 0.0f;
        a.run(4.0, [&] {
            const orchard::Vec3 v = a.w->velocity(p);
            fastest = std::max(fastest, std::sqrt(v.x * v.x + v.z * v.z));
        });
        lost = 100000.0f - a.g.health(p);
        return fastest;
    };
    float lost_soft = 0.0f, lost_hard = 0.0f;
    const float soft = shove(1000.0f, 0.0f, lost_soft);
    const float hard = shove(1.0f, 400.0f, lost_hard);
    check(lost_soft >= 1000.0f && lost_hard >= 1.0f, "the brawler lands its hits in both cases (" + str(lost_soft) + ", " + str(lost_hard) + " damage)");
    check(soft < 1.0f, "a hit that does lots of damage but has no knockback doesn't shove the player (" + str(soft) + " m/s)");
    check(hard > 3.0f, "a weak hit with lots of knockback does (" + str(hard) + " m/s)");
}

struct Arena {
    orchard::Scene s{orchard::Dimension::three_d};
    std::unique_ptr<xylem::World> w;
    orchard::Gameplay g;
    double t = 0.0;

    Arena() {
        const orchard::Entity ground = s.create("Ground");
        s.transform(ground)->position = {0.0f, -0.5f, 0.0f};
        s.add_body(ground).type = orchard::BodyType::fixed;
        s.add_collider(ground)->size = {60.0f, 1.0f, 60.0f};
    }

    orchard::Entity person(const char* name, orchard::Vec3 at) {
        const orchard::Entity e = s.create(name);
        s.transform(e)->position = at;
        orchard::Body& b = s.add_body(e);
        b.set(orchard::body_lock_rotation_x, true);
        b.set(orchard::body_lock_rotation_z, true);
        b.set(orchard::body_sleep, false);
        orchard::Collider* c = s.add_collider(e, orchard::ColliderShape::capsule);
        c->size = {0.5f, 1.8f, 0.5f};
        c->offset = {0.0f, 0.9f, 0.0f};
        c->mass = 80.0f;
        c->lock(orchard::collider_lock_mass, true);
        s.add_health(e);
        return e;
    }

    void start() {
        w = std::make_unique<xylem::World>(s.dimension());
        w->build(s);
        g.start(&s, w.get());
    }

    void run(double seconds, const std::function<void()>& each = {}) {
        const int n = static_cast<int>(seconds * 60.0 + 0.5);
        for (int i = 0; i < n; ++i) {
            t += 1.0 / 60.0;
            w->step(1.0f / 60.0f);
            g.tick(1.0 / 60.0, t);
            if (each) each();
        }
    }
};

void test_damage_types_and_resistances() {
    Arena a;
    const orchard::Entity p = a.person("P", {0.0f, 0.05f, 0.0f});
    orchard::Health& h = *a.s.health(p);
    h.armor = 100.0f;
    h.resist["fire"] = 0.5f;
    h.resist["poison"] = 1.0f;
    h.resist["cold"] = -0.5f;
    a.start();
    orchard::Damage d;
    d.amount = 20.0f;
    d.type = "melee";
    check(near(a.g.apply_damage(p, d), 90.0f, 0.01f), "armor 100 halves a melee hit (" + str(a.g.health(p)) + ")");
    d.type = "fire";
    check(near(a.g.apply_damage(p, d), 80.0f, 0.01f), "fire resistance 0.5 halves fire and armor doesn't block it (" + str(a.g.health(p)) + ")");
    d.type = "cold";
    check(near(a.g.apply_damage(p, d), 50.0f, 0.01f), "a negative resistance is a weakness (" + str(a.g.health(p)) + ")");
    d.type = "poison";
    check(near(a.g.apply_damage(p, d), 50.0f, 0.01f), "resistance 1 is immunity");
    check(!a.g.apply_effect(p, "poison", 5.0f), "and blocks effects of that kind too");
    d.type = "true";
    d.amount = 10.0f;
    check(near(a.g.apply_damage(p, d), 40.0f, 0.01f), "true damage ignores armor and resistance");
    check(a.g.last_damage_type(p) == "true", "the last damage type is remembered");
    h.damage_formula = "amount * (1 + is_fire) + 2 * has_slowed";
    a.g.apply_effect(p, "slowed", 5.0f);
    d.type = "fire";
    d.amount = 10.0f;
    check(near(a.g.apply_damage(p, d), 28.0f, 0.01f), "damage formulas can read the type and effects (" + str(a.g.health(p)) + ")");
    const orchard::Health copy = h;
    std::string err;
    const std::optional<orchard::Scene> loaded = orchard::Scene::load(a.s.save(), &err);
    check(loaded.has_value(), "a scene with resistances loads: " + err);
    const orchard::Scene& back = loaded ? *loaded : a.s;
    const orchard::Health* bh = back.health(back.find("P"));
    check(bh && bh->armor == 100.0f && bh->resist == copy.resist && bh->damage_formula == copy.damage_formula && bh->poise == copy.poise, "armor, resistances and formulas save and load");
}

void test_passive_damage() {
    Arena a;
    const orchard::Entity p = a.person("P", {0.0f, 0.05f, 0.0f});
    a.start();
    orchard::Damage d;
    d.amount = 5.0f;
    d.type = "poison";
    d.duration = 2.0f;
    d.interval = 0.5f;
    a.g.apply_damage(p, d);
    check(a.g.damage_over_time_count(p) == 1, "passive damage is tracked");
    a.run(2.5);
    check(near(a.g.health(p), 80.0f, 0.01f), "pulsed passive damage hits every interval for its duration (" + str(a.g.health(p)) + ")");
    check(a.g.damage_over_time_count(p) == 0, "and then stops");
    orchard::Damage burn;
    burn.amount = 4.0f;
    burn.type = "fire";
    burn.duration = -1.0f;
    a.g.apply_damage(p, burn);
    a.run(2.0);
    check(near(a.g.health(p), 72.0f, 0.2f), "smooth passive damage is per second (" + str(a.g.health(p)) + ")");
    a.run(3.0);
    check(a.g.damage_over_time_count(p) == 1 && a.g.health(p) < 61.0f, "forever lasts until stopped (" + str(a.g.health(p)) + ")");
    check(a.g.stop_damage(p, "fire") && a.g.damage_over_time_count(p) == 0, "stopping passive damage by type");
    a.g.apply_damage(p, burn);
    a.g.apply_damage(p, burn);
    check(a.g.damage_over_time_count(p) == 1, "the same passive damage refreshes instead of stacking");
    burn.stack = true;
    a.g.apply_damage(p, burn);
    check(a.g.damage_over_time_count(p) == 2, "unless asked to stack");
    a.g.stop_damage(p, "");

    Arena b;
    const orchard::Entity q = b.person("Q", {0.0f, 0.05f, 0.0f});
    b.s.health(q)->poise = 100.0f;
    b.start();
    b.run(0.3);
    orchard::Damage shove;
    shove.amount = 0.0f;
    shove.duration = 1.0f;
    shove.interval = 0.25f;
    shove.knockback = {40.0f, 0.0f, 0.0f};
    shove.push = orchard::PushRepeat::every_pulse;
    b.g.apply_damage(q, shove);
    int kicks = 0;
    float last = 0.0f;
    b.run(1.2, [&] {
        const float v = b.w->velocity(q).x;
        if (v > last + 0.3f) ++kicks;
        last = v;
    });
    check(kicks >= 3 && kicks <= 5, "a push can repeat every pulse (" + str(static_cast<float>(kicks)) + " kicks)");
    Arena c;
    const orchard::Entity r = c.person("R", {0.0f, 0.05f, 0.0f});
    c.start();
    c.run(0.3);
    orchard::Damage once = shove;
    once.push = orchard::PushRepeat::once;
    c.g.apply_damage(r, once);
    int c_kicks = 0;
    float c_last = 0.0f;
    c.run(1.2, [&] {
        const float v = c.w->velocity(r).x;
        if (v > c_last + 0.3f) ++c_kicks;
        c_last = v;
    });
    check(c_kicks == 1, "or happen only once (" + str(static_cast<float>(c_kicks)) + " kicks)");
}

void test_stagger_and_effects() {
    Arena a;
    const orchard::Entity p = a.person("P", {0.0f, 0.05f, 0.0f});
    const orchard::Entity tank = a.person("Tank", {5.0f, 0.05f, 0.0f});
    a.s.health(tank)->poise = 50.0f;
    a.start();
    a.run(0.3);
    orchard::Damage punch;
    punch.amount = 5.0f;
    punch.type = "melee";
    punch.knockback = {320.0f, 0.0f, 0.0f};
    a.g.apply_damage(p, punch);
    a.g.apply_damage(tank, punch);
    check(a.g.staggered(p) > 0.3f && a.g.stunned(p), "a hard shove staggers someone (" + str(a.g.staggered(p)) + " s)");
    check(a.g.move_scale(p) < 0.5f, "and slows them while staggered");
    check(a.g.staggered(tank) == 0.0f, "but not someone with lots of poise");
    a.run(1.5);
    check(a.g.staggered(p) == 0.0f && !a.g.stunned(p), "stagger wears off");
    check(a.g.apply_effect(p, "poison", 4.0f) && a.g.apply_effect(p, "poison", 4.0f), "effects can be applied twice");
    check(a.g.effect_stacks(p, "poison") == 2, "and stack up to their limit");
    const float before = a.g.health(p);
    a.run(0.95);
    check(near(before - a.g.health(p), 4.0f, 0.01f), "stacked poison hurts more (" + str(before - a.g.health(p)) + ")");
    check(a.g.apply_effect(p, "slowed", -1.0f) && a.g.effect_time_left(p, "slowed") < 0.0f, "effects can last forever");
    check(near(a.g.move_scale(p), 0.6f, 0.01f), "slowed slows (" + str(a.g.move_scale(p)) + ")");
    check(a.g.apply_effect(p, "haste", 5.0f, 2.0f) && near(a.g.move_scale(p), 0.6f * 1.6f, 0.01f), "strength scales an effect");
    a.run(6.0);
    check(a.g.effect_stacks(p, "slowed") == 1 && a.g.effect_stacks(p, "haste") == 0 && a.g.effect_stacks(p, "poison") == 0, "timed effects end and forever ones stay");
    check(a.g.remove_effect(p, "slowed") && a.g.effects(p).empty(), "effects can be removed");
    check(a.g.set_effect_value("venom", "damage", "3") && a.g.set_effect_value("venom", "interval", "0.5") && a.g.set_effect_value("venom", "particle", "electric"), "new effects can be defined");
    const float pre = a.g.health(p);
    a.g.apply_effect(p, "venom", 1.0f);
    a.run(1.2);
    check(near(pre - a.g.health(p), 6.0f, 0.01f), "custom effects hurt on their own interval (" + str(pre - a.g.health(p)) + ")");
    a.g.apply_effect(p, "burning", 2.0f);
    check(a.g.burning(p) && a.g.effect_stacks(p, "burning") == 1, "burning is an effect too");
    a.g.remove_effect(p, "burning");
    check(!a.g.burning(p), "and putting it out removes it");
    a.s.health(p)->effect_formula = "duration * 0.5 * (1 - is_stunned)";
    a.g.apply_effect(p, "frozen", 4.0f);
    check(near(a.g.effect_time_left(p, "frozen"), 2.0f, 0.01f), "effect formulas change how long effects last");
    check(!a.g.apply_effect(p, "stunned", 4.0f), "and can block them");
    a.g.apply_effect(p, "strength", 10.0f);
    orchard::Damage hit;
    hit.amount = 10.0f;
    hit.type = "melee";
    hit.attacker = p;
    const float tank_before = a.g.health(tank);
    a.g.apply_damage(tank, hit);
    check(near(tank_before - a.g.health(tank), 15.0f, 0.01f), "an attacker's strength effect raises their damage");
}

void test_survival() {
    Arena a;
    const orchard::Entity p = a.person("P", {0.0f, 0.05f, 0.0f});
    orchard::Survival& sv = a.s.add_survival(p);
    sv.hunger_on = true;
    sv.thirst_on = true;
    sv.temperature_on = true;
    a.start();
    orchard::Exertion e = a.g.exert(p, 4.0f, 9.0f, true, true, false, 0.5f);
    check(e.sprinting && e.speed == 9.0f, "sprinting is allowed with stamina");
    check(near(a.g.stat(p, "stamina"), 92.5f, 0.01f), "and uses it (" + str(a.g.stat(p, "stamina")) + ")");
    for (int i = 0; i < 20; ++i) e = a.g.exert(p, 4.0f, 9.0f, true, true, false, 0.5f);
    check(!e.sprinting && e.speed == 4.0f && a.g.stat(p, "stamina") == 0.0f, "running out of stamina stops sprinting");
    a.g.set_stat(p, "stamina", 10.0f);
    e = a.g.exert(p, 4.0f, 9.0f, true, true, false, 0.0f);
    check(!e.sprinting, "sprinting needs a little stamina to start again");
    e = a.g.exert(p, 4.0f, 9.0f, false, true, true, 0.1f);
    check(e.can_jump, "jumping is allowed with enough stamina");
    a.g.jumped(p);
    check(near(a.g.stat(p, "stamina"), 0.0f, 0.01f), "and each jump costs stamina");
    check(!a.g.exert(p, 4.0f, 9.0f, false, true, true, 0.1f).can_jump, "too tired to jump");
    a.run(2.0);
    check(a.g.stat(p, "stamina") > 15.0f, "stamina comes back after a short rest (" + str(a.g.stat(p, "stamina")) + ")");
    a.g.apply_effect(p, "frozen", 5.0f);
    e = a.g.exert(p, 4.0f, 9.0f, false, true, true, 0.1f);
    check(near(e.speed, 1.6f, 0.01f) && near(e.jump, 0.5f, 0.01f), "effects slow walking and jumping");
    a.g.remove_effect(p, "frozen");

    a.g.set_stat(p, "saturation", 1.0f);
    a.g.set_stat(p, "hunger", 1.0f);
    a.g.set_stat(p, "idle_exhaustion", 8.0f);
    a.run(0.6);
    check(a.g.stat(p, "saturation") == 0.0f && a.g.stat(p, "hunger") <= 1.0f, "saturation is used up before food");
    a.run(1.0);
    check(a.g.stat(p, "hunger") == 0.0f, "then food runs out");
    float hp = a.g.health(p);
    a.run(2.0);
    check(near(hp - a.g.health(p), 2.0f, 0.1f) && a.g.last_damage_type(p) == "starve", "starving hurts (" + str(hp - a.g.health(p)) + ")");
    a.g.set_stat(p, "idle_exhaustion", 0.0f);
    check(a.g.eat(p, 20.0f, 5.0f) == 20.0f && a.g.stat(p, "saturation") == 5.0f, "eating fills food and saturation");
    hp = a.g.health(p);
    a.run(2.0);
    check(a.g.health(p) > hp + 1.5f && a.g.stat(p, "saturation") < 5.0f, "being fed heals and uses up saturation (" + str(a.g.stat(p, "saturation")) + ")");

    a.g.set_stat(p, "thirst", 0.0f);
    hp = a.g.health(p);
    a.run(1.0);
    check(a.g.last_damage_type(p) == "thirst" || a.g.health(p) < hp + 1.0f, "no water hurts");
    check(a.g.drink(p, 50.0f) == 50.0f, "drinking refills water");
    a.g.set_stat(p, "ambient", -20.0f);
    a.run(4.0);
    check(a.g.stat(p, "temperature") < 36.5f, "cold air cools the body (" + str(a.g.stat(p, "temperature")) + ")");
    a.g.apply_effect(p, "warm", 30.0f, 3.0f);
    const float cold = a.g.stat(p, "temperature");
    a.run(4.0);
    check(a.g.stat(p, "temperature") > cold, "warming effects heat it back up");
    a.g.set_stat(p, "ambient", 20.0f);

    sv.temperature_on = false;
    sv.hunger_on = false;
    sv.thirst_on = false;
    sv.max_stamina = 150.0f;
    std::string err;
    const std::optional<orchard::Scene> loaded = orchard::Scene::load(a.s.save(), &err);
    check(loaded.has_value(), "a scene with survival stats loads: " + err);
    const orchard::Scene& back = loaded ? *loaded : a.s;
    const orchard::Survival* bs = back.survival(back.find("P"));
    check(bs && bs->max_stamina == 150.0f && !bs->hunger_on && bs->breath_on, "survival stats save and load");
    check(!a.g.set_stat(p, "nonsense", 1.0f), "unknown stats are refused");
}

void test_drowning() {
    orchard::Scene s(orchard::Dimension::three_d);
    ground(s, 20.0f);
    const orchard::Entity pool = s.create("Pool");
    s.transform(pool)->position = {0.0f, 1.5f, 0.0f};
    orchard::Fluid& f = s.add_fluid(pool);
    f.level = orchard::FluidLevel::still;
    f.size = {6.0f, 3.0f, 6.0f};
    f.fill = 1.0f;
    const orchard::Entity diver = s.create("Diver");
    s.transform(diver)->position = {0.0f, 0.1f, 0.0f};
    s.add_health(diver);
    orchard::Survival& sv = s.add_survival(diver);
    sv.max_breath = sv.breath = 2.0f;
    const orchard::Entity walker = s.create("Walker");
    s.transform(walker)->position = {12.0f, 0.1f, 0.0f};
    s.add_health(walker);
    s.add_survival(walker).max_breath = 2.0f;
    xylem::World w(orchard::Dimension::three_d);
    w.build(s);
    orchard::Gameplay g;
    g.start(&s, &w);
    check(g.underwater(diver) && !g.underwater(walker), "a head under water is noticed");
    double t = 0.0;
    for (int i = 0; i < 240; ++i) {
        t += 1.0 / 60.0;
        w.step(1.0f / 60.0f);
        g.tick(1.0 / 60.0, t);
    }
    check(g.stat(diver, "breath") == 0.0f && g.health(diver) < 85.0f && g.last_damage_type(diver) == "drown", "holding breath too long drowns (" + str(g.health(diver)) + ")");
    check(g.health(walker) == 100.0f && g.stat(walker, "breath") == 2.0f, "breathing air is fine");
}

void test_ai_attack_settings() {
    AiScene a;
    const orchard::Entity p = a.player({0.0f, 0.05f, 0.0f});
    a.s.health(p)->on_death = orchard::DeathAction::nothing;
    a.s.health(p)->max = a.s.health(p)->value = 100000.0f;
    const orchard::Entity h = a.agent("brawler", {0.0f, 0.05f, 4.0f});
    a.s.transform(h)->rotation = orchard::from_euler({0.0f, 180.0f, 0.0f});
    orchard::Agent& ag = *a.s.agent(h);
    ag.attack_damage = 10.0f;
    ag.damage_type = "cold";
    ag.damage_formula = "damage * 3";
    ag.hit_effect = "chilled";
    ag.hit_effect_chance = 1.0f;
    a.start();
    a.run(4.0);
    const float lost = 100000.0f - a.g.health(p);
    check(lost >= 30.0f && std::fmod(lost, 30.0f) < 0.01f, "agents use their damage formula (" + str(lost) + ")");
    check(a.g.last_damage_type(p) == "cold", "and their damage type");
    check(a.g.effect_stacks(p, "chilled") == 1, "and can cause effects on hit");
    a.g.stagger(h, 3.0f);
    const float now = a.g.health(p);
    a.run(1.5);
    check(a.g.health(p) == now, "a staggered agent can't attack");
    std::string err;
    const std::optional<orchard::Scene> loaded = orchard::Scene::load(a.s.save(), &err);
    check(loaded.has_value(), "agents with attack settings load: " + err);
    const orchard::Scene& back = loaded ? *loaded : a.s;
    const orchard::Agent* bag = back.agent(back.find("brawler"));
    check(bag && bag->damage_type == "cold" && bag->hit_effect == "chilled" && bag->damage_formula == "damage * 3", "attack settings save and load");
}

void test_armor_and_shields() {
    Arena a;
    const orchard::Entity p = a.person("P", {0.0f, 0.05f, 0.0f});
    orchard::Health& h = *a.s.health(p);
    h.armor = 30.0f;
    a.start();
    orchard::Damage d;
    d.amount = 50.0f;
    d.type = "melee";
    auto hit = [&](float amount, const std::string& type) {
        a.g.set_health(p, 100.0f);
        d.amount = amount;
        d.type = type;
        return 100.0f - a.g.apply_damage(p, d);
    };
    h.armor_mode = "flat";
    check(near(hit(50.0f, "melee"), 20.0f, 0.01f), "flat armor takes its value off each hit");
    check(near(hit(20.0f, "melee"), 2.0f, 0.01f), "but always lets a little through (" + str(hit(20.0f, "melee")) + ")");
    h.armor_mode = "percent";
    check(near(hit(50.0f, "melee"), 35.0f, 0.01f), "percent armor blocks that share");
    h.armor = 200.0f;
    check(near(hit(50.0f, "melee"), 5.0f, 0.01f), "up to its cap");
    h.armor = 30.0f;
    h.armor_mode = "diminishing";
    h.armor_constant = 30.0f;
    check(near(hit(50.0f, "melee"), 25.0f, 0.01f), "diminishing armor halves damage at its constant");
    h.armor_mode = "formula";
    h.armor_formula = "max(amount - armor * 0.5, amount * 0.2) * (1 + is_projectile)";
    check(near(hit(50.0f, "melee"), 35.0f, 0.01f), "armor can be your own formula");
    h.armor_types = "melee,projectile";
    check(near(hit(50.0f, "projectile"), 70.0f, 0.01f), "which can read the damage type");
    check(near(hit(50.0f, "fire"), 50.0f, 0.01f), "armor only blocks the types it's set to");
    h.armor_types = "all";
    check(near(hit(50.0f, "fire"), 35.0f, 0.01f) && near(hit(50.0f, "true"), 50.0f, 0.01f), "all means everything except true damage");
    h.armor_mode = "none";
    check(near(hit(50.0f, "melee"), 50.0f, 0.01f), "armor can do nothing");
    h.armor_mode = "flat";
    h.armor_wear = 0.5f;
    hit(50.0f, "melee");
    check(near(h.armor, 15.0f, 0.01f), "armor can wear down as it blocks (" + str(h.armor) + ")");
    h.armor = 0.0f;
    h.armor_wear = 0.0f;
    check(a.g.set_health_setting(p, "armor_mode", "percent") && h.armor_mode == "percent" && !a.g.set_health_setting(p, "armor_mode", "magic"), "scripts can change how armor works");
    check(a.g.set_health_setting(p, "armor", "12") && a.g.stat(p, "armor") == 12.0f && a.g.health_setting(p, "armor_mode") == "percent", "and read it back");
    h.armor = 0.0f;

    h.max_shield = 40.0f;
    h.shield = 40.0f;
    check(near(hit(30.0f, "melee"), 0.0f, 0.01f) && near(h.shield, 10.0f, 0.01f), "a shield soaks up hits before health");
    check(near(hit(30.0f, "melee"), 20.0f, 0.01f) && h.shield == 0.0f, "until it runs out");
    h.shield = 40.0f;
    h.shield_absorb = 0.5f;
    check(near(hit(30.0f, "melee"), 15.0f, 0.01f) && near(h.shield, 25.0f, 0.01f), "a shield can take only part of each hit");
    h.shield_types = "projectile";
    check(near(hit(30.0f, "melee"), 30.0f, 0.01f), "and only some damage types");
    h.shield_types.clear();
    h.shield = 40.0f;
    h.shield_formula = "amount * shield_fraction";
    check(near(hit(20.0f, "melee"), 0.0f, 0.01f) && near(hit(20.0f, "melee"), 10.0f, 0.01f), "shields can be your own formula (weaker as they drain)");
    check(near(hit(10.0f, "true"), 10.0f, 0.01f), "true damage goes straight through shields");
    h.shield = 0.0f;
    h.shield_regen = 10.0f;
    h.shield_delay = 1.0f;
    hit(1.0f, "melee");
    a.run(0.9);
    check(h.shield == 0.0f, "shields wait before recharging");
    a.run(1.2);
    check(h.shield > 9.0f && h.shield < 12.0f, "then recharge (" + str(h.shield) + ")");
    std::string err;
    const std::optional<orchard::Scene> loaded = orchard::Scene::load(a.s.save(), &err);
    const orchard::Health* bh = loaded ? loaded->health(loaded->find("P")) : nullptr;
    check(bh && bh->armor_mode == "percent" && bh->max_shield == 40.0f && bh->shield_formula == h.shield_formula && bh->shield_types.empty() && bh->armor_types == "all" && bh->shield_regen == 10.0f, "armor and shield settings save and load");
}

void test_encumbrance() {
    Arena a;
    const orchard::Entity p = a.person("P", {0.0f, 0.05f, 0.0f});
    a.s.add_survival(p).weight_on = true;
    a.s.survival(p)->carry_limit = 40.0f;
    const orchard::Entity pack = a.s.create("Pack", p);
    a.s.add_tag(pack, "item");
    orchard::Collider* c = a.s.add_collider(pack);
    c->size = {0.2f, 0.2f, 0.2f};
    c->mass = 10.0f;
    c->lock(orchard::collider_lock_mass, true);
    a.start();
    check(near(a.g.carried_weight(p), 10.0f, 0.01f), "things tagged item count as carried (" + str(a.g.carried_weight(p)) + " kg)");
    check(a.g.move_scale(p) == 1.0f, "a light load doesn't slow you");
    a.g.set_stat(p, "carried", 20.0f);
    check(near(a.g.encumbrance(p), 0.75f, 0.01f) && near(a.g.move_scale(p), 0.75f, 0.01f), "a heavy load slows you (" + str(a.g.move_scale(p)) + ")");
    orchard::Exertion e = a.g.exert(p, 4.0f, 9.0f, true, true, true, 0.1f);
    check(e.sprinting && near(e.speed, 6.75f, 0.01f) && e.can_jump, "you can still sprint and jump under your limit");
    a.g.set_stat(p, "carried", 40.0f);
    e = a.g.exert(p, 4.0f, 9.0f, true, true, true, 0.1f);
    check(!e.sprinting && !e.can_jump && e.speed > 0.0f, "over your limit you can't sprint or jump but can still walk (" + str(e.speed) + ")");
    a.g.set_stat(p, "carried", 60.0f);
    check(a.g.move_scale(p) == 0.0f, "far over it you can't move");
    a.g.set_stat(p, "carried", 20.0f);
    const float before = a.g.stat(p, "stamina");
    a.g.exert(p, 4.0f, 9.0f, true, true, false, 1.0f);
    check(near(before - a.g.stat(p, "stamina"), 15.0f * 1.75f, 0.01f), "carrying more uses more stamina (" + str(before - a.g.stat(p, "stamina")) + ")");
    a.s.survival(p)->weight_on = false;
    check(a.g.move_scale(p) == 1.0f && a.g.encumbrance(p) == 0.0f, "weight is optional (" + str(a.g.move_scale(p)) + ", " + str(a.g.encumbrance(p)) + ")");
    check(near(a.g.stat(p, "weight"), 30.0f, 0.01f), "the weight stat is what you carry");
}

void test_units() {
    check(near(static_cast<float>(*orchard::convert_units(1.0, "lb", "kg")), 0.45359237f, 1e-6f), "pounds to kilograms");
    check(near(static_cast<float>(*orchard::convert_units(100.0, "C", "F")), 212.0f, 1e-4f), "Celsius to Fahrenheit");
    check(near(static_cast<float>(*orchard::convert_units(0.0, "C", "K")), 273.15f, 1e-4f), "Celsius to kelvin");
    check(near(static_cast<float>(*orchard::convert_units(36.0, "km/h", "m/s")), 10.0f, 1e-4f), "km/h to m/s");
    check(near(static_cast<float>(*orchard::convert_units(1.0, "mi", "ft")), 5280.0f, 1e-2f), "miles to feet");
    check(near(static_cast<float>(*orchard::convert_units(1.0, "lbf*s", "N*s")), 4.448222f, 1e-4f), "pound-force seconds to newton seconds");
    check(!orchard::convert_units(1.0, "kg", "m").has_value(), "units of different kinds don't convert");
    check(near(static_cast<float>(orchard::from_si(5.0, orchard::UnitKind::temperature_change, "F")), 9.0f, 1e-4f), "a temperature change converts without the offset");
    check(near(static_cast<float>(orchard::from_si(9.0, orchard::UnitKind::per_degree, "F")), 5.0f, 1e-4f), "damage per degree converts the other way");
}

void test_ai_walks_around_walls() {
    AiScene a;
    a.wall({0.0f, 1.5f, 5.0f}, {12.0f, 3.0f, 0.5f});
    const orchard::Entity z = a.agent("custom", {0.0f, 0.05f, 0.0f});
    a.start();
    a.ai.move_to(z, {0.0f, 0.0f, 10.0f}, true);
    a.run(12.0);
    const orchard::Vec3 at = a.pos(z);
    check(flat_distance(at, {0.0f, 0.0f, 10.0f}) < 1.2f, "an agent walks around a wall to reach a point (ended at " + str(at.x) + ", " + str(at.z) + ")");
    check(a.ai.arrived(z), "and says it arrived");
    const orchard::ai::NavPath path = a.ai.find_path({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 10.0f});
    check(path.found && path.length > 12.0f, "a found path goes around the wall (" + str(path.length) + " m)");
}

void test_ai_guard_shoots() {
    AiScene a;
    const orchard::Entity p = a.player({0.0f, 0.05f, 0.0f});
    const orchard::Entity guard = a.agent("guard", {0.0f, 0.05f, 12.0f});
    a.s.transform(guard)->rotation = orchard::from_euler({0.0f, 180.0f, 0.0f});
    a.start();
    a.run(6.0);
    check(a.g.health(p) < 100.0f, "a guard spots and shoots the player (" + str(a.g.health(p)) + " health left)");
    bool shot = false;
    for (std::string e = a.ai.next_event(guard); !e.empty(); e = a.ai.next_event(guard)) shot = shot || e == "shoot";
    check(shot, "and reports shoot events");
}

void test_ai_stealth_suspicion() {
    AiScene a;
    const orchard::Entity p = a.player({0.0f, 0.05f, 0.0f});
    const orchard::Entity guard = a.agent("stealth-guard", {0.0f, 0.05f, 20.0f});
    a.s.transform(guard)->rotation = orchard::from_euler({0.0f, 180.0f, 0.0f});
    a.s.agent(guard)->walk_speed = 0.0f;
    a.start();
    a.run(0.3);
    const float early = a.ai.awareness(guard, p);
    check(early > 0.0f && early < 1.0f, "a stealth guard's awareness builds up instead of spotting at once (" + str(early) + ")");
    a.run(6.0);
    check(a.ai.awareness(guard, p) >= 1.0f, "and then spots the player");
    check(a.ai.team_alert("enemies") == orchard::ai::AlertPhase::alert, "spotting raises the team's alarm");
    a.ai.set_visibility(p, 0.0f);
    AiScene b;
    const orchard::Entity p2 = b.player({0.0f, 0.05f, 0.0f});
    const orchard::Entity g2 = b.agent("stealth-guard", {0.0f, 0.05f, 10.0f});
    b.s.transform(g2)->rotation = orchard::from_euler({0.0f, 180.0f, 0.0f});
    b.start();
    b.ai.set_visibility(p2, 0.0f);
    b.run(3.0);
    check(b.ai.awareness(g2, p2) == 0.0f, "an invisible player isn't noticed");
}

void test_ai_hears_noise() {
    AiScene a;
    const orchard::Entity guard = a.agent("guard", {0.0f, 0.05f, 0.0f});
    a.start();
    a.run(0.5);
    a.ai.make_noise({8.0f, 0.0f, 3.0f}, 25.0f, orchard::Entity{}, "glass");
    a.run(6.0);
    check(flat_distance(a.pos(guard), {8.0f, 0.0f, 3.0f}) < 3.0f, "a guard goes to check out a noise (" + str(flat_distance(a.pos(guard), {8.0f, 0.0f, 3.0f})) + " m away)");
}

void test_ai_prey_flees() {
    AiScene a;
    const orchard::Entity p = a.player({0.0f, 0.05f, 0.0f});
    const orchard::Entity deer = a.agent("prey", {0.0f, 0.05f, 6.0f});
    a.start();
    a.w->set_velocity(p, {0.0f, 0.0f, 4.0f});
    const float before = flat_distance(a.pos(p), a.pos(deer));
    a.run(3.0, [&] { a.w->set_velocity(p, {0.0f, a.w->velocity(p).y, 4.0f}); });
    const float after = flat_distance(a.pos(p), a.pos(deer));
    check(after > before + 3.0f, "a prey animal runs from a player charging at it (" + str(before) + " m -> " + str(after) + " m)");
}

void test_ai_flat_2d() {
    AiScene a(orchard::Dimension::two_d);
    const orchard::Entity z = a.agent("custom", {0.0f, 0.05f, 0.0f});
    a.wall({6.0f, 0.4f, 0.0f}, {1.0f, 0.8f, 1.0f});
    a.start();
    a.ai.move_to(z, {12.0f, 0.0f, 0.0f}, true);
    a.run(8.0);
    check(std::fabs(a.pos(z).x - 12.0f) < 1.2f, "a 2D agent walks (and hops a low wall) along X (x = " + str(a.pos(z).x) + ")");
}

void test_ai_script_action_and_extras() {
    AiScene a;
    const orchard::Entity e = a.agent("custom", {0.0f, 0.05f, 0.0f});
    a.start();
    std::string err;
    check(a.ai.set_behavior(e, "sequence\n  dance fast\n  set done=yes\n", &err), "a custom behavior tree loads: " + err);
    a.run(0.2);
    check(a.ai.pending_action(e) == "dance" && a.ai.pending_argument(e, 0) == "fast", "an action the AI doesn't know is handed to scripts (" + a.ai.pending_action(e) + ")");
    a.ai.finish_action(e, true);
    a.run(0.2);
    check(a.ai.blackboard(e) && a.ai.blackboard(e)->text("done") == "yes", "finishing it lets the tree carry on");
    check(!a.ai.set_behavior(e, "sequence\n", &err), "a broken behavior is refused (" + err + ")");
    {
        orchard::graph::Graph brain(orchard::graph::GraphKind::brain);
        const int root = brain.add_node("bt_root", 0, 0);
        const int seq = brain.add_node("bt_sequence", 300, 0);
        const int ask = brain.add_node("bt_ask", 600, 0);
        const int note = brain.add_node("bt_set", 600, 120);
        brain.connect(root, "start", seq, "in");
        brain.connect(seq, "children", ask, "in");
        brain.connect(seq, "children", note, "in");
        brain.set_value(ask, "action", "open_door");
        brain.set_value(ask, "arguments", "fast");
        brain.set_value(note, "note", "door");
        brain.set_value(note, "value", "open");
        const std::optional<std::string> tree = orchard::graph::brain_file_behavior(brain.save(), &err);
        check(tree && a.ai.define_behavior("Behaviors/door.brain", *tree, &err) && a.ai.set_behavior(e, "Behaviors/door.brain", &err), "a brain graph drives an agent: " + err);
        a.run(0.2);
        check(a.ai.pending_action(e) == "open_door" && a.ai.pending_argument(e, 0) == "fast", "the brain graph's Ask My Script To Do node reaches the script (" + a.ai.pending_action(e) + ")");
        a.ai.finish_action(e, true);
        a.run(0.2);
        check(a.ai.blackboard(e) && a.ai.blackboard(e)->text("door") == "open", "and the graph carries on after it");
    }
    orchard::ai::VoxelWorld& vw = a.ai.voxels();
    vw.fill({-5, -1, -5}, {20, -1, 5}, vw.type_id("solid"));
    vw.fill({6, -1, -5}, {7, -1, 5}, vw.type_id("lava"));
    orchard::ai::VoxelAgent va;
    const orchard::ai::VoxelPath none = vw.pathfinder().find({0, 0, 0}, {10, 0, 0}, va, false);
    va.jump_across = 3;
    const orchard::ai::VoxelPath jump = vw.pathfinder().find({0, 0, 0}, {10, 0, 0}, va, false);
    check(!none.found && jump.found, "voxel paths won't walk through lava, but jump over it when they can");
    const int game = a.ai.new_chess();
    orchard::ai::Chess* c = a.ai.chess(game);
    check(c && c->play("f3") && c->play("e5") && c->play("g4"), "a chess game plays moves");
    const orchard::ai::Chess::SearchInfo best = c->search_level(6);
    check(best.found && best.move.uci() == "d8h4", "the chess AI finds a mate in one (" + best.move.uci() + ")");
    orchard::ai::GoapPlanner& planner = a.ai.planner("test");
    planner.add("get_axe", "", "has_axe", 2.0f);
    planner.add("chop", "has_axe", "has_wood", 4.0f);
    planner.add("buy_wood", "has_money", "has_wood", 1.0f);
    const orchard::ai::GoapPlan plan = planner.plan(orchard::ai::parse_state("has_money=0"), orchard::ai::parse_state("has_wood"));
    check(plan.found && plan.actions.size() == 2 && plan.actions[0] == "get_axe", "GOAP plans get_axe then chop");
}

void test_ai_soldiers_plan() {
    AiScene a;
    const orchard::Entity p = a.player({0.0f, 0.05f, 0.0f});
    a.wall({3.0f, 0.5f, 10.0f}, {2.0f, 1.0f, 0.4f});
    a.wall({-3.0f, 0.5f, 10.0f}, {2.0f, 1.0f, 0.4f});
    std::vector<orchard::Entity> squad;
    for (int i = 0; i < 3; ++i) {
        const orchard::Entity s = a.agent("soldier", {-2.0f + 2.0f * static_cast<float>(i), 0.05f, 14.0f});
        a.s.transform(s)->rotation = orchard::from_euler({0.0f, 180.0f, 0.0f});
        squad.push_back(s);
    }
    a.start();
    a.run(8.0);
    int plans = 0, shots = 0;
    for (orchard::Entity s : squad)
        for (std::string e = a.ai.next_event(s); !e.empty(); e = a.ai.next_event(s)) {
            if (e.rfind("plan:", 0) == 0) ++plans;
            if (e == "shoot") ++shots;
        }
    check(plans >= 3 && shots > 0, "a squad of soldiers plans its moves and fires (" + str(static_cast<float>(plans)) + " plan steps, " + str(static_cast<float>(shots)) + " shots)");
    check(a.g.health(p) < 100.0f, "and hurts the player");
}

void test_ai_horde_flow() {
    AiScene a;
    const orchard::Entity p = a.player({0.0f, 0.05f, 0.0f});
    std::vector<orchard::Entity> horde;
    for (int i = 0; i < 10; ++i) {
        const orchard::Entity z = a.agent("runner", {-9.0f + 2.0f * static_cast<float>(i), 0.05f, 11.0f});
        a.s.transform(z)->rotation = orchard::from_euler({0.0f, 180.0f, 0.0f});
        horde.push_back(z);
    }
    a.start();
    std::set<std::uint64_t> reached;
    a.run(6.0, [&] {
        for (orchard::Entity z : horde)
            if (flat_distance(a.pos(z), a.pos(p)) < 3.0f) reached.insert(z.id());
    });
    check(reached.size() >= 7, "a horde of 10 runners closes in on the player (" + str(static_cast<float>(reached.size())) + " got within 3 m)");
    check(a.g.health(p) <= 50.0f, "and overwhelms them (" + str(a.g.health(p)) + " health left)");
}

void test_ai_director() {
    orchard::ai::Director d;
    for (int i = 0; i < 600 && d.phase() == orchard::ai::Director::Phase::build_up; ++i) {
        d.report_damage(5.0f);
        d.report_nearby(3, 2.0f);
        d.tick(1.0f / 30.0f);
    }
    check(d.phase() == orchard::ai::Director::Phase::peak, "the director hits a peak when the player is under pressure");
    for (int i = 0; i < 30 * 12; ++i) d.tick(1.0f / 30.0f);
    check(d.phase() == orchard::ai::Director::Phase::relax && !d.wants_spawn(0), "then relaxes and stops spawning");
    for (int i = 0; i < 30 * 6; ++i) {
        d.report_seen(true, 4.0f);
        d.tick(1.0f / 30.0f);
    }
    check(d.wants_backoff(), "a hunter seen up close too long is told to back off");
}

void test_inventories() {
    orchard::ItemCatalog cat;
    check(orchard::item_library().size() > 300, "the item library has hundreds of items (" + str(static_cast<float>(orchard::item_library().size())) + ")");
    std::set<std::string> ids;
    bool unique = true;
    for (const orchard::ItemDef& d : orchard::item_library()) unique = unique && ids.insert(d.id).second && !d.name.empty() && !d.category.empty();
    check(unique, "every library item has its own id, a name and a category");
    for (const std::string& p : orchard::inventory_presets()) {
        orchard::Inventory inv;
        check(orchard::apply_inventory_preset(inv, p), "the " + p + " preset applies");
    }
    orchard::Inventory mc;
    orchard::apply_inventory_preset(mc, "minecraft");
    check(orchard::inventory_add(cat, mc, "apple", 100.0f) == 100.0f, "100 apples fit in a Minecraft inventory");
    int stacks = 0;
    for (const orchard::ItemStack& s : mc.items) stacks += s.item == "apple" ? 1 : 0;
    for (const orchard::ItemStack& s : mc.hotbar_items) stacks += s.item == "apple" ? 1 : 0;
    const orchard::ItemDef* apple = cat.find("apple");
    check(apple && stacks >= 2, "they split into stacks (" + str(static_cast<float>(stacks)) + ")");
    mc.stack_limit = 0.0f;
    check(apple && orchard::stack_cap(mc, *apple) == static_cast<float>(apple->max_stack), "a stack limit of 0 uses the item's own");
    mc.stack_limit = 5.0f;
    check(apple && orchard::stack_cap(mc, *apple) == 5.0f, "a stack limit caps every stack");
    orchard::set_inventory_field(mc, "stack_limit", "-3");
    check(mc.stack_limit == 0.0f, "stack limits can't go negative");
    check(orchard::inventory_take(cat, mc, "apple", 30.0f) == 30.0f && orchard::inventory_count(mc, "apple") == 70.0f, "taking apples leaves the rest");
    orchard::Inventory dz;
    orchard::apply_inventory_preset(dz, "dayz");
    dz.stack_measure = "volume";
    dz.whole_counts = false;
    dz.stack_limit = 2.5f;
    const float poured = orchard::inventory_add(cat, dz, "water", 1.75f);
    check(near(poured, 1.75f, 1e-4f) && near(orchard::inventory_count(dz, "water"), 1.75f, 1e-4f), "liquids are carried as fractions (" + str(poured) + " L)");
    orchard::Inventory tk;
    orchard::apply_inventory_preset(tk, "tarkov");
    tk.columns = 4;
    tk.rows = 4;
    tk.max_weight = 3.0f;
    check(orchard::inventory_add(cat, tk, "glock17", 1.0f) == 1.0f, "a pistol fits in a grid inventory");
    check(!tk.items.empty() && tk.items[0].x >= 0 && tk.items[0].y >= 0, "and gets a place on the grid");
    int placed = 0;
    for (int i = 0; i < 20; ++i) placed += orchard::inventory_add(cat, tk, "glock17", 1.0f) > 0.0f ? 1 : 0;
    check(placed < 20, "a full grid or a weight limit stops more going in (" + str(static_cast<float>(placed)) + " more fit)");
    check(orchard::inventory_weight(cat, tk) <= tk.max_weight + 1e-3f, "the weight limit is respected (" + str(orchard::inventory_weight(cat, tk)) + " kg)");
    orchard::ItemStack st;
    st.item = "rucksack";
    st.amount = 1.0f;
    st.x = 2;
    st.y = 1;
    st.loaded = 3.0f;
    orchard::ItemStack inner;
    inner.item = "bandage";
    inner.amount = 4.0f;
    st.contents.push_back(inner);
    const std::optional<orchard::ItemStack> back = orchard::parse_stack(orchard::stack_text(st));
    check(back && back->item == "rucksack" && back->x == 2 && back->y == 1 && back->contents.size() == 1 && back->contents[0].amount == 4.0f, "stacks with contents survive being written out");
}

void test_holsters_and_weapons(bool flat) {
    orchard::Scene s(flat ? orchard::Dimension::two_d : orchard::Dimension::three_d);
    const orchard::Entity ground = s.create("Ground");
    s.transform(ground)->position = {0.0f, -0.5f, 0.0f};
    s.add_body(ground).type = orchard::BodyType::fixed;
    s.add_collider(ground)->size = {60.0f, 1.0f, 60.0f};
    auto person = [&](const char* name, orchard::Vec3 at) {
        const orchard::Entity e = s.create(name);
        s.transform(e)->position = at;
        orchard::Body& b = s.add_body(e);
        b.set(orchard::body_lock_rotation_x, true);
        b.set(orchard::body_lock_rotation_y, flat);
        b.set(orchard::body_lock_rotation_z, true);
        orchard::Collider* c = s.add_collider(e, orchard::ColliderShape::capsule);
        c->size = {0.5f, 1.8f, 0.5f};
        c->offset = {0.0f, 0.9f, 0.0f};
        s.add_health(e);
        return e;
    };
    const orchard::Entity p = person("P", {0.0f, 0.05f, 0.0f});
    const orchard::Entity target = person("T", flat ? orchard::Vec3{6.0f, 0.05f, 0.0f} : orchard::Vec3{0.0f, 0.05f, -6.0f});
    orchard::Inventory& inv = s.add_inventory(p);
    orchard::apply_inventory_preset(inv, "tarkov");
    inv.require_holsters = true;
    xylem::World w(s.dimension());
    w.build(s);
    orchard::Gameplay g;
    g.start(&s, &w);
    g.give(p, "glock17", 1.0f);
    g.give(p, "9mm", 34.0f);
    check(g.count(p, "9mm") == 34.0f, "ammo goes into the inventory");
    int gun_index = -1;
    for (std::size_t i = 0; i < inv.items.size(); ++i)
        if (inv.items[i].item == "glock17") gun_index = static_cast<int>(i);
    check(gun_index >= 0 && g.hold(p, "mainhand", "items", gun_index), "the pistol goes into the hand");
    check(g.held_def(p) && g.held_def(p)->id == "glock17", "and it's what the player holds");
    check(!g.holster(p), "with holsters required, a pistol can't be put away without one");
    g.give(p, "hip_holster", 1.0f);
    for (std::size_t i = 0; i < inv.items.size(); ++i)
        if (inv.items[i].item == "hip_holster") g.equip(p, "items", static_cast<int>(i));
    check(g.holster(p), "wearing a hip holster lets it be put away");
    check(!g.held(p) || g.held(p)->empty(), "and the hand is empty afterwards");
    const std::vector<std::string> slots = g.holster_slots(p);
    check(!slots.empty() && g.draw(p, slots.front()), "drawing takes it back out");
    g.reload(p);
    double t = 0.0;
    for (int i = 0; i < 180; ++i) {
        t += 1.0 / 60.0;
        w.step(1.0f / 60.0f);
        g.tick(1.0 / 60.0, t);
    }
    check(g.held(p) && g.held(p)->loaded > 0.0f, "reloading fills the magazine from the ammo carried (" + str(g.held(p) ? g.held(p)->loaded : 0.0f) + ")");
    const float before = g.health(target);
    const orchard::Vec3 eye = g.position(p) + orchard::Vec3{0.0f, flat ? 0.9f : 1.5f, 0.0f};
    const orchard::Vec3 to = g.position(target) + orchard::Vec3{0.0f, 0.9f, 0.0f};
    g.set_aim(p, eye, orchard::normalize(to - eye));
    for (const orchard::Ballistics mode : {orchard::Ballistics::hitscan, orchard::Ballistics::travel, orchard::Ballistics::realistic}) {
        g.set_ballistics(mode);
        g.set_health(target, 100.0f);
        const orchard::ItemDef& gun = *g.item_def("glock17");
        orchard::ItemDef exact = gun;
        exact.spread = 0.0f;
        g.fire(p, eye, orchard::normalize(to - eye), exact, 1.0f);
        for (int i = 0; i < 60; ++i) {
            t += 1.0 / 60.0;
            w.step(1.0f / 60.0f);
            g.tick(1.0 / 60.0, t);
        }
        check(g.health(target) < 100.0f, std::string(flat ? "2D " : "3D ") + orchard::to_string(mode) + " ballistics hit a target 6 m away (" + str(g.health(target)) + ")");
    }
    static_cast<void>(before);
    std::string list = "worn:mainhand";
    const orchard::Entity dropped = g.drop_item(p, list, 0);
    check(s.alive(dropped) && s.pickup(dropped), "dropping makes a pickup in the world");
    check(g.pick_up(p, dropped) && g.count(p, "glock17") >= 1.0f, "and it can be picked up again");
}

void test_climate_zones() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity cold = s.create("Cold");
    s.transform(cold)->position = {10.0f, 0.0f, 0.0f};
    orchard::Climate& c = s.add_climate(cold);
    check(orchard::apply_climate_preset(c, "blizzard"), "the blizzard preset applies");
    c.global = false;
    c.size = {6.0f, 6.0f, 6.0f};
    c.blend = 1.0f;
    xylem::World w(s.dimension());
    w.build(s);
    orchard::Gameplay g;
    g.start(&s, &w);
    double t = 0.0;
    for (int i = 0; i < 10; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
    const orchard::ClimateSample in = g.climate_at({10.0f, 1.0f, 0.0f});
    const orchard::ClimateSample out = g.climate_at({-20.0f, 1.0f, 0.0f});
    check(in.temperature < out.temperature - 5.0f, "inside a blizzard zone it's colder (" + str(in.temperature) + " vs " + str(out.temperature) + ")");
    check(in.snow > 0.3f && out.snow == 0.0f, "and it only snows inside it");
    const orchard::Entity rain = g.start_weather("rain", 2.0f, 0.5f, 0.5f);
    for (int i = 0; i < 60; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
    check(g.climate_at({-20.0f, 1.0f, 0.0f}).rain > 0.3f, "start_weather makes it rain everywhere");
    for (int i = 0; i < 240; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
    check(!s.alive(rain) || g.climate_at({-20.0f, 1.0f, 0.0f}).rain < 0.05f, "and the rain ends after its duration and fade");
    check(g.set_climate(cold, "temperature", "35") && g.climate_at({10.0f, 1.0f, 0.0f}).temperature > 25.0f, "scripts can change a zone's settings");
    const std::optional<orchard::Scene> loaded = orchard::Scene::load(s.save());
    check(loaded && loaded->climate(loaded->find("Cold")) && loaded->climate(loaded->find("Cold"))->preset == "blizzard", "climate zones are saved with the scene");
}

void test_items_save() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity p = s.create("P");
    orchard::Inventory& inv = s.add_inventory(p);
    orchard::apply_inventory_preset(inv, "saints_and_sinners");
    orchard::inventory_add(s.items(), inv, "bandage", 3.0f);
    inv.worn["back"] = orchard::ItemStack{};
    inv.worn["back"].item = "daypack";
    inv.worn["back"].amount = 1.0f;
    s.items().define("my_blade").damage = 77.0f;
    s.items().define("my_blade").name = "My blade";
    const orchard::Entity drop = s.create("Drop");
    s.add_pickup(drop).stack.item = "my_blade";
    s.pickup(drop)->stack.amount = 1.0f;
    std::string err;
    const std::optional<orchard::Scene> back = orchard::Scene::load(s.save(), &err);
    check(back.has_value(), "a scene with items loads (" + err + ")");
    if (!back) return;
    const orchard::Inventory* bi = back->inventory(back->find("P"));
    check(bi && bi->preset == "saints_and_sinners" && orchard::inventory_count(*bi, "bandage") == 3.0f, "inventories and their items are saved");
    check(bi && bi->worn.count("back") && bi->worn.at("back").item == "daypack", "worn items are saved");
    check(back->items().find("my_blade") && back->items().find("my_blade")->damage == 77.0f, "custom items are saved with the scene");
    check(back->pickup(back->find("Drop")) && back->pickup(back->find("Drop"))->stack.item == "my_blade", "pickups are saved");
}

void test_guns_actions_and_surfaces() {
    orchard::ItemCatalog cat;
    int guns = 0;
    for (const orchard::ItemDef& d : orchard::item_library()) guns += d.kind == orchard::ItemKind::gun ? 1 : 0;
    check(guns > 600, "lots of guns in the library (" + str(static_cast<float>(guns)) + ")");
    bool all_have_action = true;
    for (const orchard::ItemDef& d : orchard::item_library())
        if (d.kind == orchard::ItemKind::gun && (d.action.empty() || d.gun_class.empty())) all_have_action = false;
    check(all_have_action, "every library gun has an action and a type");
    check(orchard::find_guns(cat, "class=pistol, ammo=9mm").size() > 30, "find 9mm pistols");
    check(orchard::find_guns(cat, "action=bolt, class=sniper").size() > 10, "find bolt-action sniper rifles");
    check(orchard::find_guns(cat, "action=lever").size() > 10, "find lever actions");
    check(orchard::find_guns(cat, "action=pump, class=shotgun").size() > 10, "find pump shotguns");
    const std::vector<const orchard::ItemDef*> m1 = orchard::find_guns(cat, "garand");
    check(!m1.empty(), "search by name");
    check(orchard::find_guns(cat, "era=world_wars, country=Germany").size() > 5, "find German guns from the world wars");
    check(!orchard::gun_filter_values(cat, "action").empty() && !orchard::gun_info(cat, "glock17").empty(), "filter values and gun info");
    const orchard::ItemDef* glock = cat.find("glock17");
    check(glock && glock->slide_lock && glock->action == "semi", "a Glock is semi-automatic and locks open");
    const orchard::ItemDef* r870 = cat.find("rem870");
    check(r870 && orchard::manual_action(*r870), "a Remington 870 is worked by hand");
    orchard::ItemStack st;
    st.item = "glock17";
    st.amount = 1.0f;
    st.action = "open";
    st.fire_mode = "semi";
    const std::optional<orchard::ItemStack> back = orchard::parse_stack(orchard::stack_text(st));
    check(back && back->action == "open" && back->fire_mode == "semi", "a gun keeps its slide and fire mode when saved");
    check(orchard::weight_text(0.012f, "kg") == "12 g" && orchard::weight_text(2.5f, "kg") == "2.50 kg", "weights read in normal units");
    float m = 1.0f, minus = 0.0f;
    check(orchard::parse_speed_change("80%", m, minus) && near(m, 0.8f, 1e-4f), "80% speed");
    check(orchard::parse_speed_change("-1.5", m, minus) && near(minus, 1.5f, 1e-4f) && m == 1.0f, "a plain number takes speed off");
    check(orchard::guess_surface("builtin:snow") == "snow" && orchard::guess_surface("Steel crate") == "metal" && orchard::guess_surface("Office chair").empty(), "surfaces guessed from names");

    auto setup = [&](orchard::Scene& s, const std::string& gun, const std::string& ammo) {
        const orchard::Entity p = s.create("P");
        orchard::Inventory& pi = s.add_inventory(p);
        orchard::apply_inventory_preset(pi, "tarkov");
        pi.columns = 10;
        pi.rows = 8;
        return std::make_pair(p, std::make_pair(gun, ammo));
    };
    {
        orchard::Scene s(orchard::Dimension::three_d);
        s.set_rule("reload", "pool");
        auto [p, ids] = setup(s, "rem870", "12g_buck");
        xylem::World w(s.dimension());
        w.build(s);
        orchard::Gameplay g;
        g.start(&s, &w);
        g.give(p, "rem870", 1.0f);
        g.give(p, "12g_buck", 20.0f);
        int gi = -1;
        for (std::size_t i = 0; i < s.inventory(p)->items.size(); ++i)
            if (s.inventory(p)->items[i].item == "rem870") gi = static_cast<int>(i);
        g.hold(p, "mainhand", "items", gi);
        double t = 0.0;
        auto run = [&](int frames) {
            for (int i = 0; i < frames; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
        };
        g.reload(p);
        run(60 * 6);
        check(g.loaded_rounds(p) >= 5.0f, "the pump gun loads shell by shell (" + str(g.loaded_rounds(p)) + ")");
        g.set_aim(p, {0.0f, 1.5f, 0.0f}, {0.0f, 0.0f, -1.0f});
        const float before = g.loaded_rounds(p);
        check(g.use_held(p, true, false, 1.0f / 60.0f), "the pump gun fires");
        check(g.cycling(p) && g.action_kind(p) == "pump", "then works the pump by itself");
        check(!g.use_held(p, true, false, 1.0f / 60.0f), "and can't fire while pumping");
        run(60);
        check(g.gun_state(p) == "ready" && g.loaded_rounds(p) == before - 1.0f, "ready again after pumping");
        s.set_rule("cycle", "manual");
        g.use_held(p, true, false, 1.0f / 60.0f);
        run(30);
        check(g.gun_state(p) == "spent", "with manual cycling the spent shell stays until you pump");
        g.reload(p);
        check(g.cycling(p), "R works the action");
        run(60);
        check(g.gun_state(p) == "ready", "and it's ready");
    }
    {
        orchard::Scene s(orchard::Dimension::three_d);
        s.set_rule("reload", "pool");
        auto [p, ids] = setup(s, "glock17", "9mm");
        xylem::World w(s.dimension());
        w.build(s);
        orchard::Gameplay g;
        g.start(&s, &w);
        g.give(p, "glock17", 1.0f);
        g.give(p, "9mm", 3.0f);
        int gi = -1;
        for (std::size_t i = 0; i < s.inventory(p)->items.size(); ++i)
            if (s.inventory(p)->items[i].item == "glock17") gi = static_cast<int>(i);
        g.hold(p, "mainhand", "items", gi);
        double t = 0.0;
        auto run = [&](int frames) {
            for (int i = 0; i < frames; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
        };
        g.reload(p);
        run(150);
        g.set_aim(p, {0.0f, 1.5f, 0.0f}, {0.0f, 0.0f, -1.0f});
        for (int i = 0; i < 3; ++i) {
            g.use_held(p, true, false, 1.0f / 60.0f);
            run(20);
        }
        check(g.loaded_rounds(p) == 0.0f && g.gun_state(p) == "open", "the slide locks open on the last round");
        bool clicked = false;
        g.take_sound_cues();
        g.use_held(p, true, false, 1.0f / 60.0f);
        for (const orchard::SoundCue& c : g.take_sound_cues()) clicked = clicked || c.clip.find("dry_fire") != std::string::npos;
        check(!clicked, "a locked-open gun doesn't click");
        const orchard::ItemDef* mp5 = cat.find("mp5");
        check(mp5 && orchard::fire_modes_of(*mp5).size() == 3, "an MP5 has three fire modes");
    }
    {
        orchard::Scene s(orchard::Dimension::three_d);
        const orchard::Entity floor = ground(s, 40.0f);
        (*s.colliders(floor))[0].surface = "snow";
        const orchard::Entity metal = s.create("Steel plate");
        s.transform(metal)->position = {10.0f, 0.5f, 0.0f};
        s.add_body(metal).type = orchard::BodyType::fixed;
        s.add_collider(metal)->size = {2.0f, 1.0f, 2.0f};
        const orchard::Entity walker = s.create("Walker");
        s.transform(walker)->position = {0.0f, 1.0f, 0.0f};
        s.add_survival(walker);
        xylem::World w(s.dimension());
        w.build(s);
        orchard::Gameplay g;
        g.start(&s, &w);
        double t = 0.0;
        for (int i = 0; i < 20; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
        check(g.surface_under(walker) == "snow", "standing on snow (" + g.surface_under(walker) + ")");
        check(near(g.ground_speed(walker), 0.8f, 0.01f), "snow slows you to 80% (" + str(g.ground_speed(walker)) + ")");
        s.set_rule("ground_snow", "50%");
        for (int i = 0; i < 20; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
        check(near(g.ground_speed(walker), 0.5f, 0.01f), "the rule changes it to 50%");
        s.set_rule("surface_strength", "0");
        for (int i = 0; i < 20; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
        check(near(g.ground_speed(walker), 1.0f, 0.01f), "strength 0 turns it off");
        s.set_rule("surface_strength", "100");
        g.set_ground_speed(walker, "-1");
        for (int i = 0; i < 20; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
        check(near(g.ground_minus(walker), 1.0f, 0.01f), "a script can take 1 m/s off");
        g.set_ground_speed(walker, "");
        check(g.surface_at({10.0f, 2.0f, 0.0f}) == "metal", "the steel plate is metal");
        g.define_surface("snow", "walk", "70%");
        check(g.surface_setting("snow", "walk") == "70%", "surfaces can be changed");
        const std::string saved = s.save();
        const std::optional<orchard::Scene> loaded = orchard::Scene::load(saved);
        check(loaded && loaded->surfaces().find("snow") && near(loaded->surfaces().find("snow")->walk, 0.7f, 1e-4f), "custom surfaces are saved");
        check(loaded && (*loaded->colliders(loaded->find("Ground")))[0].surface == "snow", "a collider's surface is saved");
    }
}

void test_auto_actions_hammers_and_bursts() {
    auto armed = [](orchard::Scene& s, xylem::World& w, orchard::Gameplay& g, const std::string& gun, const std::string& ammo, float rounds) {
        const orchard::Entity p = s.create("P");
        orchard::Inventory& pi = s.add_inventory(p);
        orchard::apply_inventory_preset(pi, "tarkov");
        pi.columns = 10;
        pi.rows = 8;
        w.build(s);
        g.start(&s, &w);
        g.give(p, gun, 1.0f);
        g.give(p, ammo, rounds);
        int gi = -1;
        for (std::size_t i = 0; i < s.inventory(p)->items.size(); ++i)
            if (s.inventory(p)->items[i].item == gun) gi = static_cast<int>(i);
        g.hold(p, "mainhand", "items", gi);
        g.set_aim(p, {0.0f, 1.5f, 0.0f}, {0.0f, 0.0f, -1.0f});
        return p;
    };
    {
        orchard::Scene s(orchard::Dimension::three_d);
        s.set_rule("reload", "pool");
        xylem::World w(s.dimension());
        orchard::Gameplay g;
        const orchard::Entity p = armed(s, w, g, "colt_saa", "45colt", 12.0f);
        double t = 0.0;
        auto run = [&](int frames) {
            for (int i = 0; i < frames; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
        };
        g.reload(p);
        run(60 * 8);
        check(g.loaded_rounds(p) >= 6.0f, "the single-action revolver loads (" + str(g.loaded_rounds(p)) + ")");
        run(60);
        check(g.gun_state(p) == "ready", "and its hammer is cocked after loading (" + g.gun_state(p) + ")");
        check(g.use_held(p, true, false, 1.0f / 60.0f), "it fires");
        check(g.cycling(p) && g.action_kind(p) == "hammer", "and the hammer gets cocked again by itself");
        run(60);
        check(g.gun_state(p) == "ready", "ready after cocking");
        s.set_rule("manual_cycle", "1");
        g.use_held(p, true, false, 1.0f / 60.0f);
        run(30);
        check(g.gun_state(p) == "spent", "with manual actions on, the hammer stays down");
        check(!g.use_held(p, true, false, 1.0f / 60.0f), "so pulling the trigger does nothing");
        g.reload(p);
        check(g.cycling(p) && g.action_kind(p) == "hammer", "R cocks the hammer");
        run(60);
        check(g.gun_state(p) == "ready", "then it's ready");
    }
    {
        orchard::Scene s(orchard::Dimension::three_d);
        s.set_rule("reload", "pool");
        xylem::World w(s.dimension());
        orchard::Gameplay g;
        const orchard::Entity p = armed(s, w, g, "brown_bess", "musket_ball", 5.0f);
        double t = 0.0;
        auto run = [&](int frames) {
            for (int i = 0; i < frames; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
        };
        g.reload(p);
        run(60 * 25);
        check(g.loaded_rounds(p) == 1.0f && g.gun_state(p) == "ready", "a muzzle-loader is rammed and cocked (" + g.gun_state(p) + ")");
        check(g.use_held(p, true, false, 1.0f / 60.0f), "the musket fires");
        run(60);
        check(g.loaded_rounds(p) == 0.0f && !g.use_held(p, true, false, 1.0f / 60.0f), "and has to be loaded again");
    }
    {
        orchard::Scene s(orchard::Dimension::three_d);
        s.set_rule("reload", "pool");
        xylem::World w(s.dimension());
        orchard::Gameplay g;
        const orchard::Entity p = armed(s, w, g, "double_barrel", "12g_buck", 10.0f);
        const orchard::ItemDef* d = g.held_def(p);
        check(d && d->hammer, "the coach gun has hammers");
        double t = 0.0;
        auto run = [&](int frames) {
            for (int i = 0; i < frames; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
        };
        g.reload(p);
        run(60 * 6);
        check(g.loaded_rounds(p) == 2.0f && g.gun_state(p) == "ready", "both barrels load and cock (" + str(g.loaded_rounds(p)) + ", " + g.gun_state(p) + ")");
        g.use_held(p, true, false, 1.0f / 60.0f);
        run(60);
        check(g.loaded_rounds(p) == 1.0f && g.gun_state(p) == "ready", "the second hammer is cocked automatically");
    }
    {
        orchard::Scene s(orchard::Dimension::three_d);
        s.set_rule("reload", "pool");
        xylem::World w(s.dimension());
        orchard::Gameplay g;
        const orchard::Entity p = armed(s, w, g, "ak47", "762x39", 60.0f);
        double t = 0.0;
        auto run = [&](int frames) {
            for (int i = 0; i < frames; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
        };
        g.reload(p);
        run(60 * 4);
        std::vector<std::string> modes = g.fire_modes(p);
        check(std::find(modes.begin(), modes.end(), "burst5") == modes.end(), "no 5-round burst until the rule is on");
        s.set_rule("burst5", "1");
        modes = g.fire_modes(p);
        check(std::find(modes.begin(), modes.end(), "burst5") != modes.end(), "the rule adds a 5-round burst to automatic guns");
        check(g.set_fire_mode(p, "burst5"), "and it can be picked");
        const float before = g.loaded_rounds(p);
        g.use_held(p, true, false, 1.0f / 60.0f);
        for (int i = 0; i < 120; ++i) {
            g.use_held(p, false, true, 1.0f / 60.0f);
            g.tick(1.0 / 60.0, t += 1.0 / 60.0);
        }
        check(near(before - g.loaded_rounds(p), 5.0f, 0.01f), "one pull fires five rounds (" + str(before - g.loaded_rounds(p)) + ")");
        const orchard::ItemDef* d = g.held_def(p);
        check(d && orchard::burst_size(*d, "burst5") == 5 && orchard::burst_size(*d, "burst") >= 1 && orchard::burst_size(*d, "auto") == 0, "burst sizes come from the mode");
    }
    {
        orchard::Scene s(orchard::Dimension::three_d);
        s.set_rule("reload", "pool");
        s.set_rule("manual_cycle", "1");
        xylem::World w(s.dimension());
        orchard::Gameplay g;
        const orchard::Entity p = armed(s, w, g, "rem870", "12g_buck", 10.0f);
        double t = 0.0;
        auto run = [&](int frames) {
            for (int i = 0; i < frames; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
        };
        g.reload(p);
        run(60 * 6);
        check(g.gun_state(p) == "spent", "loaded from empty with manual actions on, the pump is left for you (" + g.gun_state(p) + ")");
        g.reload(p);
        run(60);
        check(g.gun_state(p) == "ready", "R pumps it");
        g.use_held(p, true, false, 1.0f / 60.0f);
        run(30);
        check(g.gun_state(p) == "spent", "and after a shot the pump is left for you again");
        s.set_rule("manual_cycle", "0");
        g.reload(p);
        run(60);
        g.use_held(p, true, false, 1.0f / 60.0f);
        check(g.cycling(p), "turned off, the pump is worked automatically again");
    }
}

void test_ai_ground() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity floor = s.create("Ground");
    s.transform(floor)->position = {0.0f, -0.5f, 0.0f};
    s.add_body(floor).type = orchard::BodyType::fixed;
    s.add_collider(floor)->size = {200.0f, 1.0f, 200.0f};
    (*s.colliders(floor))[0].surface = "ice";
    auto agent = [&](const char* name, float z) {
        const orchard::Entity a = s.create(name);
        s.transform(a)->position = {0.0f, 0.05f, z};
        orchard::Body& b = s.add_body(a);
        b.set(orchard::body_lock_rotation_x, true);
        b.set(orchard::body_lock_rotation_z, true);
        orchard::Collider* c = s.add_collider(a, orchard::ColliderShape::capsule);
        c->size = {0.5f, 1.8f, 0.5f};
        c->offset = {0.0f, 0.9f, 0.0f};
        orchard::Agent& ag = s.add_agent(a);
        ag.brain = "wander";
        return a;
    };
    const orchard::Entity skater = agent("Skater", 0.0f);
    xylem::World w(s.dimension());
    w.build(s);
    orchard::Gameplay g;
    g.start(&s, &w);
    double t = 0.0;
    for (int i = 0; i < 20; ++i) {
        w.step(1.0f / 60.0f);
        g.tick(1.0 / 60.0, t += 1.0 / 60.0);
    }
    check(g.surface_under(skater) == "ice", "the agent stands on ice (" + g.surface_under(skater) + ")");
    check(g.ground_slip(skater) > 0.5f, "and ice is slippery for agents too (" + str(g.ground_slip(skater)) + ")");
    (*s.colliders(floor))[0].surface = "mud";
    for (int i = 0; i < 20; ++i) {
        w.step(1.0f / 60.0f);
        g.tick(1.0 / 60.0, t += 1.0 / 60.0);
    }
    check(g.ground_speed(skater) < 0.95f, "mud slows agents down (" + str(g.ground_speed(skater)) + ")");
}

void test_ammo_and_magazines() {
    orchard::ItemCatalog cat;
    int cartridges = 0;
    for (const orchard::ItemDef& d : orchard::item_library()) cartridges += d.kind == orchard::ItemKind::ammo && d.grain > 0.0f ? 1 : 0;
    check(cartridges > 290, "the cartridge list is in the library (" + str(static_cast<float>(cartridges)) + ")");
    const orchard::ItemDef* nine = cat.find("9mm");
    check(nine && nine->grain == 115.0f && nine->category == "Ammo/Handgun", "9mm is a 115 gr handgun cartridge");
    check(nine && near(orchard::round_weight(cat, *nine, "fmj") * orchard::grains_per_kg, 184.0f, 0.5f), "a 9mm round weighs its grains");
    check(nine && orchard::round_weight(cat, *nine, "ap") < orchard::round_weight(cat, *nine, "fmj"), "a lighter AP bullet makes a lighter round");
    const std::vector<std::string> v = nine ? orchard::ammo_variants(cat, *nine) : std::vector<std::string>{};
    for (const char* t : {"fmj", "ap", "hp", "sp", "otm", "wadcutter", "frangible"}) check(std::find(v.begin(), v.end(), t) != v.end(), std::string("9mm comes in ") + t);
    const orchard::ItemDef* rifle = cat.find("ammo_6_5_creedmoor");
    const std::vector<std::string> rv = rifle ? orchard::ammo_variants(cat, *rifle) : std::vector<std::string>{};
    check(std::find(rv.begin(), rv.end(), "bt") != rv.end(), "rifle rounds come in boat tail");
    const orchard::ItemDef* glock = cat.find("glock17");
    check(glock && nine && orchard::ammo_fits(*glock, *nine), "a Glock takes 9mm");
    check(glock && !orchard::magazines_for_gun(cat, *glock).empty(), "a Glock has a magazine");
    const orchard::ItemDef* python = cat.find("python357");
    const orchard::ItemDef* special = cat.find("ammo_38_special");
    check(python && special && orchard::ammo_fits(*python, *special), "a .357 revolver also fires .38 Special");
    check(!orchard::ammo_info(cat, "9mm").empty() && orchard::ammo_info(cat, "9mm").find("Glock 17") != std::string::npos, "ammo lookup lists the guns it fits");
    orchard::ItemStack st;
    st.item = "9mm";
    st.amount = 20.0f;
    st.variant = "hp";
    const std::optional<orchard::ItemStack> back = orchard::parse_stack(orchard::stack_text(st));
    check(back && back->variant == "hp", "a stack keeps its bullet type when saved");
    orchard::Inventory inv;
    orchard::apply_inventory_preset(inv, "tarkov");
    orchard::ItemStack hp = st, fmj = st;
    fmj.variant = "fmj";
    orchard::inventory_add_stack(cat, inv, hp, 20.0f);
    orchard::inventory_add_stack(cat, inv, fmj, 20.0f);
    check(orchard::inventory_count_variant(inv, "9mm", "hp") == 20.0f && orchard::inventory_count(inv, "9mm") == 40.0f, "different bullet types stack separately");

    for (const char* mode : {"keep", "drop", "discard", "pool"}) {
        orchard::Scene s(orchard::Dimension::three_d);
        s.set_rule("reload", mode);
        const orchard::Entity p = s.create("P");
        orchard::Inventory& pi = s.add_inventory(p);
        orchard::apply_inventory_preset(pi, "tarkov");
        pi.columns = 10;
        pi.rows = 8;
        xylem::World w(s.dimension());
        w.build(s);
        orchard::Gameplay g;
        g.start(&s, &w);
        g.give(p, "glock17", 1.0f);
        orchard::ItemStack rounds;
        rounds.item = "9mm";
        rounds.variant = "ap";
        rounds.amount = 50.0f;
        g.give_stack(p, rounds);
        const std::string mag = orchard::magazines_for_gun(cat, *glock).front()->id;
        g.give(p, mag, 2.0f);
        int gi = -1;
        for (std::size_t i = 0; i < s.inventory(p)->items.size(); ++i)
            if (s.inventory(p)->items[i].item == "glock17") gi = static_cast<int>(i);
        g.hold(p, "mainhand", "items", gi);
        double t = 0.0;
        auto run = [&](int frames) {
            for (int i = 0; i < frames; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
        };
        if (std::string(mode) != "pool") {
            for (std::size_t i = 0; i < s.inventory(p)->items.size(); ++i)
                if (s.inventory(p)->items[i].item == mag) {
                    g.load_magazine(p, "items", static_cast<int>(i));
                    break;
                }
        }
        run(120);
        check(g.reload(p), std::string(mode) + ": reloading works");
        run(150);
        check(g.loaded_rounds(p) >= 1.0f && g.loaded_variant(p) == "ap", std::string(mode) + ": the gun is loaded with AP (" + str(g.loaded_rounds(p)) + ")");
        g.set_aim(p, {0.0f, 1.5f, 0.0f}, {0.0f, 0.0f, -1.0f});
        g.use_held(p, true, false, 1.0f / 60.0f);
        run(30);
        const float left = g.loaded_rounds(p);
        check(g.reload(p), std::string(mode) + ": a second reload works");
        run(150);
        float mags_carried = g.count(p, mag), on_ground = 0.0f;
        for (orchard::Entity e : s.entities())
            if (const orchard::Pickup* pk = s.pickup(e); pk && pk->stack.item == mag) on_ground += 1.0f;
        if (std::string(mode) == "keep") check(mags_carried >= 1.0f && on_ground == 0.0f, "keep: the partly used magazine goes back in the inventory");
        if (std::string(mode) == "drop") check(on_ground >= 1.0f, "drop: the old magazine falls on the ground");
        if (std::string(mode) == "discard") check(on_ground == 0.0f && mags_carried == 0.0f, "discard: the old magazine is gone");
        if (std::string(mode) == "pool") check(left >= 1.0f && g.loaded_rounds(p) == static_cast<float>(glock->magazine) && g.count(p, mag) == 2.0f, "pool: rounds come straight from the inventory");
    }
    orchard::Scene ws(orchard::Dimension::three_d);
    ws.set_rule("world_fill", "empty");
    orchard::Gameplay wg;
    wg.start(&ws, nullptr);
    const orchard::Entity full = wg.spawn_item("glock17", 1.0f, {0.0f, 0.0f, 0.0f});
    check(ws.pickup(full) && ws.pickup(full)->stack.loaded == 0.0f, "guns in the world can start empty");
    ws.set_rule("world_fill", "partial");
    ws.set_rule("fill_fraction", "0.5");
    const orchard::Entity half = wg.spawn_item("glock17", 1.0f, {0.0f, 0.0f, 0.0f});
    check(ws.pickup(half) && near(ws.pickup(half)->stack.loaded, 9.0f, 0.6f), "or half full (" + str(ws.pickup(half) ? ws.pickup(half)->stack.loaded : -1.0f) + ")");
    orchard::Pickup& placed = ws.add_pickup(ws.create("Placed"));
    placed.stack.item = orchard::magazines_for_gun(cat, *glock).front()->id;
    placed.stack.amount = 1.0f;
    placed.fill = "random";
    wg.prepare_world();
    check(placed.stack.loaded >= 0.0f && placed.stack.loaded <= static_cast<float>(glock->magazine) && placed.stack.ammo == "9mm", "a placed magazine can get a random amount");
}

void test_cursor_and_actions() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity p = s.create("P");
    orchard::apply_inventory_preset(s.add_inventory(p), "minecraft");
    auto inventory = [&]() -> orchard::Inventory& { return *s.inventory(p); };
    orchard::Gameplay g;
    g.start(&s, nullptr);
    g.give(p, "apple", 10.0f);
    int at = -1;
    std::string list;
    for (const char* l : {"hotbar", "items"}) {
        std::vector<orchard::ItemStack>& v = std::string(l) == "hotbar" ? inventory().hotbar_items : inventory().items;
        for (std::size_t i = 0; i < v.size() && at < 0; ++i)
            if (v[i].item == "apple") {
                at = static_cast<int>(i);
                list = l;
            }
    }
    check(at >= 0, "the apples are somewhere");
    check(orchard::inventory_action(inventory(), "right_click") == "pick_half", "right click picks up half by default");
    check(g.inventory_action(p, p, list, at, -1, -1, "pick_half") && g.cursor(p)->amount == 5.0f, "picking up half leaves 5 on the cursor");
    check(g.inventory_action(p, p, "items", 20, -1, -1, "place_one") && inventory().items[20].amount == 1.0f && g.cursor(p)->amount == 4.0f, "placing one puts down one");
    inventory().place_amount = 3.0f;
    check(g.inventory_action(p, p, "items", 21, -1, -1, "place_amount") && inventory().items[21].amount == 3.0f, "placing a set amount uses the inventory's amount");
    check(g.inventory_action(p, p, "", -1, -1, -1, "drop_cursor") && g.cursor(p)->empty(), "G drops what's on the cursor");
    bool dropped = false;
    for (orchard::Entity e : s.entities()) dropped = dropped || (s.pickup(e) && s.pickup(e)->stack.item == "apple");
    check(dropped, "and it lands in the world");
    check(g.inventory_action(p, p, "items", 21, -1, -1, "pick_all") && g.cursor(p)->amount == 3.0f && inventory().items[21].empty(), "left click picks up the whole stack");
    check(g.return_cursor(p) && g.cursor(p)->empty() && g.count(p, "apple") == 9.0f, "closing the inventory puts the cursor back");
    orchard::ItemEvent ev;
    int events = 0;
    while (g.next_item_event(p, ev)) ++events;
    check(events >= 5, "scripts can see inventory clicks (" + std::to_string(events) + ")");
    check(orchard::set_inventory_field(inventory(), "action:middle_click", "use") && orchard::inventory_action(inventory(), "middle_click") == "use", "click actions can be changed");
}

void test_worn_magic_and_weight() {
    orchard::Scene s(orchard::Dimension::three_d);
    auto person = [&](const char* name, orchard::Vec3 at) {
        const orchard::Entity e = s.create(name);
        s.transform(e)->position = at;
        s.add_health(e);
        orchard::Inventory& inv = s.add_inventory(e);
        orchard::apply_inventory_preset(inv, "dayz");
        inv.columns = 12;
        inv.rows = 12;
        return e;
    };
    const orchard::Entity a = person("A", {0.0f, 0.0f, 0.0f});
    const orchard::Entity b = person("B", {1.0f, 0.0f, 0.0f});
    orchard::Gameplay g;
    g.start(&s, nullptr);
    g.give(b, "thornmail", 1.0f);
    g.give(b, "fireward_cloak", 1.0f);
    for (int pass = 0; pass < 2; ++pass)
        for (std::size_t i = 0; i < s.inventory(b)->items.size(); ++i)
            if (s.inventory(b)->items[i].item == "thornmail" || s.inventory(b)->items[i].item == "fireward_cloak") {
                g.equip(b, "items", static_cast<int>(i));
                break;
            }
    check(g.worn_resist(b, "fire") > 0.5f, "a fireward cloak resists fire");
    g.set_health(b, 100.0f);
    orchard::Damage burn;
    burn.amount = 20.0f;
    burn.type = "fire";
    g.apply_damage(b, burn);
    check(g.health(b) > 90.0f, "so fire hurts less (" + str(g.health(b)) + ")");
    g.set_health(a, 100.0f);
    orchard::Damage hit;
    hit.amount = 10.0f;
    hit.type = "melee";
    hit.attacker = a;
    g.apply_damage(b, hit);
    check(g.health(a) < 100.0f, "thornmail hurts whoever hits it (" + str(g.health(a)) + ")");
    double t = 0.0;
    g.give(a, "ring_vigor", 1.0f);
    for (std::size_t i = 0; i < s.inventory(a)->items.size(); ++i)
        if (s.inventory(a)->items[i].item == "ring_vigor") g.equip(a, "items", static_cast<int>(i));
    for (int i = 0; i < 5; ++i) g.tick(1.0 / 60.0, t += 1.0 / 60.0);
    check(g.effect_stacks(a, "regeneration") > 0, "a ring can keep an effect on while worn");
    const float with_worn = orchard::inventory_weight(s.items(), *s.inventory(b));
    s.inventory(b)->worn_weight = false;
    check(orchard::inventory_weight(s.items(), *s.inventory(b)) < with_worn - 5.0f, "worn armor's weight can be left out");
    orchard::ItemStack blade;
    blade.item = "gladius";
    blade.amount = 1.0f;
    blade.enchant = "fire=7, burning:3:1";
    const std::optional<orchard::ItemStack> eb = orchard::parse_stack(orchard::stack_text(blade));
    check(eb && eb->enchant == "fire=7, burning:3:1", "enchantments are saved with the stack");
    check(g.stack_enchant_damage(blade) == "fire=7" && g.stack_enchant_effects(blade) == "burning:3:1", "enchantments split into damage and effects");
    const std::string info = g.describe(b, "{name}\n{health_bar}\n{health} / {max_health}\n{item}\n{nothing}", a);
    check(info.find("B") == 0 && info.find("#bar health") != std::string::npos, "the look-at panel fills in names and bars:\n" + info);
    s.set_rule("look_range", "12");
    check(s.rule_number("look_range") == 12.0f && s.rule("plates") == "1", "game rules have defaults and can change");
    const std::optional<orchard::Scene> loaded = orchard::Scene::load(s.save());
    check(loaded && loaded->rule_number("look_range") == 12.0f, "game rules are saved with the scene");
    const std::string wav = (std::filesystem::temp_directory_path() / "orchard-shot.wav").string();
    check(orchard::write_item_sound("gunshot_rifle", wav) && std::filesystem::file_size(wav) > 10000, "gunshot sounds are made for you");
}

int main() {
    test_running_stays_down();
    test_detailed_player_walks_hills();
    test_sprinting_over_hills();
    test_characters_collide_without_launching();
    test_triangle_prism_lands();
    test_flattened_play();
    test_balls_settle();
    test_start_motion();
    test_mass_and_locks();
    test_character();
    test_falling_3d();
    test_falling_2d();
    test_fluids_2d();
    test_compound_and_sensor();
    test_impulse_and_disabled();
    test_axes();
    test_layers_and_flags();
    test_friction_mix();
    test_quality();
    test_gameplay_health();
    test_gameplay_explosions();
    test_gameplay_fire();
    test_effects();
    test_sprinting();
    test_render_smoothing();
    test_ai_zombie_chases_and_bites();
    test_ai_heavy_hit_does_not_launch();
    test_grenade_does_not_throw_people();
    test_ai_knockback_is_separate_from_damage();
    test_damage_types_and_resistances();
    test_passive_damage();
    test_stagger_and_effects();
    test_survival();
    test_drowning();
    test_ai_attack_settings();
    test_armor_and_shields();
    test_inventories();
    test_holsters_and_weapons(false);
    test_holsters_and_weapons(true);
    test_climate_zones();
    test_items_save();
    test_ammo_and_magazines();
    test_auto_actions_hammers_and_bursts();
    test_ai_ground();
    test_cursor_and_actions();
    test_worn_magic_and_weight();
    test_guns_actions_and_surfaces();
    test_encumbrance();
    test_units();
    test_ai_walks_around_walls();
    test_ai_guard_shoots();
    test_ai_stealth_suspicion();
    test_ai_hears_noise();
    test_ai_prey_flees();
    test_ai_flat_2d();
    test_ai_script_action_and_extras();
    test_ai_soldiers_plan();
    test_ai_horde_flow();
    test_ai_director();
    std::cout << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
