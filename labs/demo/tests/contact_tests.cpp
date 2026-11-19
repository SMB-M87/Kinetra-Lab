// Contact-response tests for the Demo's physics use: head-on, rotated, off-center,
// resting and thin-wall contacts, isolated from steering, navigation and rendering.
#include <kinetra/physics.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

namespace {
using namespace kinetra;

/// Collides two equal elastic bodies head-on at `speed` for `duration` seconds in
/// steps of `dt`, then checks reversal, momentum, energy and separation.
void check_head_on(float dt, float speed = 2, float duration = 2) {
    CAPTURE(dt);
    CAPTURE(speed);
    World world;
    Body a;
    a.position = {-2, 0};
    a.velocity = {speed, 0};
    a.restitution = 1;
    a.friction = 0;
    Body b = a;
    b.position = {2, 0};
    b.velocity = {-speed, 0};
    world.add(a);
    world.add(b);
    for (int tick = 0; tick < int(std::lround(duration / dt)); ++tick)
        world.step(dt);
    const auto &left = world.body(0);
    const auto &right = world.body(1);
    CHECK_MESSAGE((left.velocity.x < 0 && right.velocity.x > 0), "Head-on contact must reverse velocities");
    CHECK_MESSAGE(length(left.velocity + right.velocity) < 0.001f, "Contact lost linear momentum");
    CHECK_MESSAGE(std::abs(dot(left.velocity, left.velocity) + dot(right.velocity, right.velocity) -
                           2 * speed * speed) < 0.01f,
                  "Elastic contact lost kinetic energy");
    CHECK_MESSAGE(left.position.x < right.position.x, "Bodies passed through one another");
}

} // namespace

TEST_CASE("slow elastic head-on contacts reverse and conserve momentum and energy") {
    check_head_on(1.f / 60);
    check_head_on(1.f / 120);
}

TEST_CASE("fast elastic head-on contacts reverse within one step without passing through") {
    check_head_on(1.f / 60, 600, 1.f / 60);
    check_head_on(1.f / 120, 600, 1.f / 120);
}

TEST_CASE("a common rigid transform preserves contact geometry for every shape pair") {
    for (const auto first : {Shape::circle, Shape::box})
        for (const auto second : {Shape::circle, Shape::box}) {
            Body a, b;
            a.shape = first;
            b.shape = second;
            a.half_extents = b.half_extents = {0.5f, 0.7f};
            b.position = {0.8f, 0.1f};
            const auto original = collide(a, b);
            CAPTURE(first);
            CAPTURE(second);
            REQUIRE_MESSAGE(original.has_value(), "Reference shapes must overlap");
            for (const float angle : {0.37f, 1.2f, -2.1f}) {
                Body transformed_a = a, transformed_b = b;
                const Vec2 translation{3, -4};
                transformed_a.position = rotated(a.position, angle) + translation;
                transformed_b.position = rotated(b.position, angle) + translation;
                transformed_a.angle += angle;
                transformed_b.angle += angle;
                const auto contact = collide(transformed_a, transformed_b);
                CAPTURE(angle);
                REQUIRE_MESSAGE(contact.has_value(), "Rotation lost a contact");
                CHECK_MESSAGE(std::abs(contact->penetration - original->penetration) < 0.0001f,
                              "Rotation changed penetration");
                CHECK_MESSAGE(length(contact->normal - rotated(original->normal, angle)) < 0.0001f,
                              "Contact normal did not rotate with shapes");
                CHECK_MESSAGE(length(contact->point - (rotated(original->point, angle) + translation)) <
                                  0.0001f,
                              "Contact point did not transform with shapes");
            }
        }
}

TEST_CASE("an off-center impact rotates the box and conserves linear and angular momentum") {
    Body box, ball;
    box.shape = Shape::box;
    box.half_extents = {0.5f, 1};
    box.friction = 0;
    ball.position = {-0.9f, 0.6f};
    ball.velocity = {3, 0};
    ball.friction = 0;
    const auto contact = collide(box, ball);
    REQUIRE_MESSAGE(contact.has_value(), "Off-center impact must produce contact");
    const Vec2 momentum = box.velocity * box.mass + ball.velocity * ball.mass;
    const float angular_momentum =
        cross(box.position, box.velocity * box.mass) + cross(ball.position, ball.velocity * ball.mass);
    // Suppress position correction to isolate the impulse's conservation laws.
    auto impulse_contact = *contact;
    impulse_contact.penetration = 0;
    resolve_contact(box, ball, impulse_contact);
    CHECK_MESSAGE(box.angular_velocity < -0.01f, "Off-center impact must rotate the box clockwise");
    CHECK_MESSAGE(length(box.velocity * box.mass + ball.velocity * ball.mass - momentum) < 0.0001f,
                  "Off-center contact lost linear momentum");
    const float box_inertia =
        box.mass * (box.half_extents.x * box.half_extents.x + box.half_extents.y * box.half_extents.y) / 3;
    const float ball_inertia = ball.mass * ball.radius * ball.radius / 2;
    const float after = cross(box.position, box.velocity * box.mass) +
                        cross(ball.position, ball.velocity * ball.mass) + box_inertia * box.angular_velocity +
                        ball_inertia * ball.angular_velocity;
    CHECK_MESSAGE(std::abs(after - angular_momentum) < 0.0001f, "Contact lost angular momentum");
}

// Regression for the previously observed thin-wall tunneling.
TEST_CASE("fast convex bodies do not tunnel through a thin wall") {
    for (int variant = 0; variant < 4; ++variant) {
        CAPTURE(variant);
        World world;
        Body wall;
        wall.shape = Shape::box;
        wall.motion = Motion::fixed;
        wall.half_extents = {0.05f, 2};
        world.add(wall);
        Body body;
        if (variant >= 2)
            body = regular_polygon(variant == 2 ? 3 : 6, 0.1f);
        else
            body.shape = variant == 0 ? Shape::circle : Shape::box;
        body.radius = 0.1f;
        body.half_extents = {0.1f, 0.1f};
        body.position = {-2, 0};
        body.velocity = {600, 0};
        world.add(body);
        world.step(1.f / 60);
        CHECK_MESSAGE(world.body(1).position.x <= -0.1499f,
                      "Fast convex body tunneled through the thin wall");
        CHECK_MESSAGE(world.body(1).velocity.x <= 0, "Fast wall contact did not stop or reverse approach");
    }
}

TEST_CASE("a body resting on a fixed wall settles without penetrating or moving the wall") {
    World world;
    Body wall;
    wall.shape = Shape::box;
    wall.motion = Motion::fixed;
    wall.position = {0, -1};
    wall.half_extents = {5, 0.5f};
    world.add(wall);
    Body agent;
    agent.position = {0, 2};
    agent.restitution = 0;
    world.add(agent);
    world.gravity = {0, -9.81f};
    float maximum_penetration = 0;
    for (int tick = 0; tick < 600; ++tick) {
        world.step(1.f / 120);
        if (const auto contact = collide(world.body(0), world.body(1)))
            maximum_penetration = std::max(maximum_penetration, contact->penetration);
    }
    CHECK_MESSAGE(maximum_penetration < 0.01f, "Resting contact penetrates the wall");
    CHECK_MESSAGE(std::abs(world.body(1).position.y) < 0.01f, "Body failed to settle on wall");
    CHECK_MESSAGE(length(world.body(1).velocity) < 0.05f, "Resting contact remains unstable");
    CHECK_MESSAGE(world.body(0).position.y == -1, "Fixed wall moved");
}
