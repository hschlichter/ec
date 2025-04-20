#include "catch2/catch_test_macros.hpp"
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include "ec.hpp"
#include <chrono>
#include <random>
#include <numeric>
#include <algorithm>

using namespace ec;

struct Position { float x, y; };
struct Velocity { float vx, vy; };
struct Health { int hp; };

TEST_CASE("Entity creation and destruction", "[entity]") {
    World world;

    // Create entities
    Entity e1 = create_entity(world);
    Entity e2 = create_entity(world);

    REQUIRE(e1 == 0);
    REQUIRE(e2 == 1);
    
    // Ensure alive
    REQUIRE(get_component<Position>(world, e1) == nullptr);
    
    destroy_entity(world, e1);
    // After destruction, cannot add components
    REQUIRE(add_component<Position>(world, e1, {1,2}) == Status::ERROR);
}

TEST_CASE("Component add/get/remove", "[component]") {
    World world;
    Entity e = create_entity(world);

    // Initially no component
    REQUIRE(get_component<Position>(world, e) == nullptr);

    // Add component
    REQUIRE(add_component<Position>(world, e, {3.5f, -2.0f}) == Status::OK);
    Position *p = get_component<Position>(world, e);
    REQUIRE(p != nullptr);
    REQUIRE(p->x == Catch::Approx(3.5f));
    REQUIRE(p->y == Catch::Approx(-2.0f));

    // Remove component
    REQUIRE(remove_component<Position>(world, e) == Status::OK);
    REQUIRE(get_component<Position>(world, e) == nullptr);
}

TEST_CASE("Multiple component types", "[component][multi]") {
    World world;
    Entity e = create_entity(world);

    REQUIRE(add_component<Position>(world, e, {1,1}) == Status::OK);
    REQUIRE(add_component<Velocity>(world, e, {0.1f, 0.2f}) == Status::OK);
    REQUIRE(add_component<Health>(world, e, {100}) == Status::OK);

    auto *p = get_component<Position>(world, e);
    auto *v = get_component<Velocity>(world, e);
    auto *h = get_component<Health>(world, e);
    REQUIRE(p != nullptr);
    REQUIRE(v != nullptr);
    REQUIRE(h != nullptr);
    REQUIRE(h->hp == 100);
}

TEST_CASE("Query with single component", "[query][single]") {
    World world;
    Entity e1 = create_entity(world);
    Entity e2 = create_entity(world);

    add_component<Position>(world, e1, {0,0});
    add_component<Position>(world, e2, {5,5});
    add_component<Velocity>(world, e2, {1,1});

    std::vector<Entity> results;
    query<Position>(world, [&](Entity e, Position*) {
        results.push_back(e);
    });

    // Both entities have Position
    REQUIRE(results.size() == 2);
    REQUIRE((results[0] == e1 && results[1] == e2));
}

TEST_CASE("Query with multiple components", "[query][multi]") {
    World world;
    Entity e1 = create_entity(world);
    Entity e2 = create_entity(world);

    add_component<Position>(world, e1, {0,0});
    add_component<Velocity>(world, e1, {1,1});
    add_component<Position>(world, e2, {5,5});

    std::vector<Entity> results;
    query<Position, Velocity>(world, [&](Entity e, Position*, Velocity*) {
        results.push_back(e);
    });

    // Only e1 has both Position and Velocity
    REQUIRE(results.size() == 1);
    REQUIRE(results[0] == e1);
}

TEST_CASE("Multiple worlds isolation", "[world]") {
    World w1;
    World w2;
    Entity a = create_entity(w1);
    Entity b = create_entity(w2);

    REQUIRE(a == 0);
    REQUIRE(b == 0);
    
    add_component<Health>(w1, a, {50});
    REQUIRE(get_component<Health>(w1, a)->hp == 50);
    REQUIRE(get_component<Health>(w2, b) == nullptr);
}

TEST_CASE("Cache-friendly access pattern", "[performance]") {
    using Clock = std::chrono::high_resolution_clock;

    World world;
    const size_t N = 200000;

    // Create a large number of entities with Position
    for (size_t i = 0; i < N; ++i) {
        auto e = create_entity(world);
        add_component<Position>(world, e, { static_cast<float>(i), static_cast<float>(i) });
    }

    // Measure contiguous access via query
    auto start = Clock::now();
    size_t sum1 = 0;
    query<Position>(world, [&](Entity, Position *p) {
        sum1 += static_cast<size_t>(p->x);
    });
    auto mid = Clock::now();

    // Prepare random access indices
    std::vector<size_t> idx(N);
    std::iota(idx.begin(), idx.end(), 0);
    std::mt19937_64 rng(123);
    std::shuffle(idx.begin(), idx.end(), rng);

    // Measure random access
    size_t sum2 = 0;
    for (auto e : idx) {
        auto p = get_component<Position>(world, e);
        sum2 += static_cast<size_t>(p->x);
    }
    auto end = Clock::now();

    auto durCont = std::chrono::duration<double, std::micro>(mid - start).count();
    auto durRand = std::chrono::duration<double, std::micro>(end - mid).count();

    // Contiguous (cache-friendly) should be faster than randomized access
    REQUIRE(sum1 == sum2);
    REQUIRE(durCont < durRand);
}
