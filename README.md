# Simple EC(S)
Template-based Entity-Component-(System) - Single file EC.hpp

## Benefits:
- Data-oriented design: component data stored contiguously in memory for cache-efficient access.
- Flexible: supports arbitrary component types with minimal boilerplate.
- Multi-world support: independent World instances with isolated component pools.
- Type-safe, zero-overhead abstractions: compile-time type mapping without runtime reflection.
- No archetypes, focus on being as cache friendly as possible.

## Usage:
1. Create a ecs::World instance.
2. Manage entities with create_entity(world) and destroy_entity(world, entity).
3. Attach components: add_component<ComponentType>(world, entity, componentData).
4. Access components: auto* ptr = get_component<ComponentType>(world, entity).
5. Iterate entities with specific components:
    `query<CompA, CompB>(world, [](Entity e, CompA* a, CompB* b) { ... });`

## Note:
- There is no built-in system abstraction. Write systems as free functions or lambdas that call query<...>().

Example
```cpp
#include <iostream>

struct Position { float x, y; };
struct Velocity { float vx, vy; };

int main() {
    World world;

    // Demo entities
    Entity e1 = create_entity(world);
    add_component<Position>(world, e1, {0, 0});
    add_component<Velocity>(world, e1, {1, 1});

    Entity e2 = create_entity(world);
    add_component<Position>(world, e2, {5, 5});

    // Movement system
    query<Position, Velocity>(world,
        [](Entity e, Position* p, Velocity* v) {
            p->x += v->vx;
            p->y += v->vy;
            std::cout << "Entity " << e
                      << " -> (" << p->x << ", " << p->y << ")\n";
        }
    );

    return 0;
}
```
