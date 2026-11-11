#include <xylem/xylem.hpp>
#include <orchard/effects.hpp>
#include <orchard/gameplay.hpp>
#include <orchard/ai/world.hpp>
#include <orchard/graph.hpp>

#include <cmath>
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

int main() {
    test_running_stays_down();
    test_detailed_player_walks_hills();
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
