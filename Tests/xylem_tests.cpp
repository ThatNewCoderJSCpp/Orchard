#include <xylem/xylem.hpp>

#include <cmath>
#include <cstdlib>
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

int main() {
    test_falling_3d();
    test_falling_2d();
    test_compound_and_sensor();
    test_impulse_and_disabled();
    test_axes();
    test_layers_and_flags();
    test_friction_mix();
    test_quality();
    std::cout << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
