#include <cmath>
#include <iostream>
#include <kinetra/physics.h>
#include <stdexcept>

namespace {
using namespace kinetra;
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

// Isolate contact response from steering, navigation and rendering.
void head_on(float dt, float speed = 2, float duration = 2) {
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
    require(left.velocity.x < 0 && right.velocity.x > 0, "Head-on contact must reverse velocities");
    require(length(left.velocity + right.velocity) < 0.001f, "Contact lost linear momentum");
    require(std::abs(dot(left.velocity, left.velocity) + dot(right.velocity, right.velocity) -
                     2 * speed * speed) < 0.01f,
            "Elastic contact lost kinetic energy");
    require(left.position.x < right.position.x, "Bodies passed through one another");
}

// A common rigid transform must preserve contact geometry for every supported pair.
void rotated_contacts() {
    for (const auto first : {Shape::circle, Shape::box})
        for (const auto second : {Shape::circle, Shape::box}) {
            Body a, b;
            a.shape = first;
            b.shape = second;
            a.half_extents = b.half_extents = {0.5f, 0.7f};
            b.position = {0.8f, 0.1f};
            const auto original = collide(a, b);
            require(original.has_value(), "Reference shapes must overlap");
            for (const float angle : {0.37f, 1.2f, -2.1f}) {
                Body transformed_a = a, transformed_b = b;
                const Vec2 translation{3, -4};
                transformed_a.position = rotated(a.position, angle) + translation;
                transformed_b.position = rotated(b.position, angle) + translation;
                transformed_a.angle += angle;
                transformed_b.angle += angle;
                const auto contact = collide(transformed_a, transformed_b);
                require(contact.has_value(), "Rotation lost a contact");
                require(std::abs(contact->penetration - original->penetration) < 0.0001f,
                        "Rotation changed penetration");
                require(length(contact->normal - rotated(original->normal, angle)) < 0.0001f,
                        "Contact normal did not rotate with shapes");
                require(length(contact->point - (rotated(original->point, angle) + translation)) < 0.0001f,
                        "Contact point did not transform with shapes");
            }
        }
}

void off_center_contact() {
    Body box, ball;
    box.shape = Shape::box;
    box.half_extents = {0.5f, 1};
    box.friction = 0;
    ball.position = {-0.9f, 0.6f};
    ball.velocity = {3, 0};
    ball.friction = 0;
    const auto contact = collide(box, ball);
    require(contact.has_value(), "Off-center impact must produce contact");
    const Vec2 momentum = box.velocity * box.mass + ball.velocity * ball.mass;
    const float angular_momentum =
        cross(box.position, box.velocity * box.mass) + cross(ball.position, ball.velocity * ball.mass);
    // Suppress position correction to isolate the impulse's conservation laws.
    auto impulse_contact = *contact;
    impulse_contact.penetration = 0;
    resolve_contact(box, ball, impulse_contact);
    require(box.angular_velocity < -0.01f, "Off-center impact must rotate the box clockwise");
    require(length(box.velocity * box.mass + ball.velocity * ball.mass - momentum) < 0.0001f,
            "Off-center contact lost linear momentum");
    const float box_inertia =
        box.mass * (box.half_extents.x * box.half_extents.x + box.half_extents.y * box.half_extents.y) / 3;
    const float ball_inertia = ball.mass * ball.radius * ball.radius / 2;
    const float after = cross(box.position, box.velocity * box.mass) +
                        cross(ball.position, ball.velocity * ball.mass) + box_inertia * box.angular_velocity +
                        ball_inertia * ball.angular_velocity;
    require(std::abs(after - angular_momentum) < 0.0001f, "Contact lost angular momentum");
}

// Regression for the previously observed thin-wall tunneling.
void fast_wall_contact() {
    for (int variant = 0; variant < 4; ++variant) {
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
        require(world.body(1).position.x <= -0.1499f, "Fast convex body tunneled through the thin wall");
        require(world.body(1).velocity.x <= 0, "Fast wall contact did not stop or reverse approach");
    }
}

void resting_wall() {
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
    require(maximum_penetration < 0.01f, "Resting contact penetrates the wall");
    require(std::abs(world.body(1).position.y) < 0.01f, "Body failed to settle on wall");
    require(length(world.body(1).velocity) < 0.05f, "Resting contact remains unstable");
    require(world.body(0).position.y == -1, "Fixed wall moved");
}
} // namespace

int main() {
    try {
        head_on(1.f / 60);
        head_on(1.f / 120);
        head_on(1.f / 60, 600, 1.f / 60);
        head_on(1.f / 120, 600, 1.f / 120);
        rotated_contacts();
        off_center_contact();
        resting_wall();
        fast_wall_contact();
        std::cout << "Demo contact baseline passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
