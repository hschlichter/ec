#ifndef EC_HPP
#define EC_HPP

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <typeinfo>
#include <array>
#include <utility>

namespace ec {

using Entity = std::uint32_t;
enum class Status { OK = 0, ERROR = 1 };

struct PoolBase {
    virtual ~PoolBase() = default;
    virtual auto ensureSize(std::size_t n) -> void = 0;
};

template<typename T>
struct Pool : PoolBase {
    std::vector<T>    data;
    std::vector<char> mask;

    auto ensureSize(std::size_t n) -> void override {
        if (data.size() < n) {
            data.resize(n);
            mask.resize(n, 0);
        }
    }
};

struct World {
    // Alive flags per entity
    std::vector<char> alive;
    Entity nextEntity = 0;

    // Type-erased component pools
    std::vector<std::unique_ptr<PoolBase>> pools;

    // Map: type hash -> compact ID
    std::unordered_map<std::size_t, std::size_t> typeMap;
    std::size_t typeCounter = 0;

    World(std::size_t entityCap = 16, std::size_t compCap = 8) {
        alive.reserve(entityCap);
        pools.reserve(compCap);
        typeMap.reserve(compCap);
    }

    // Get or assign an ID for component type T
    template<typename T>
    auto getTypeId() -> std::size_t {
        static_assert(std::is_standard_layout_v<T>, "Component must be standard-layout");

        const auto hash = typeid(T).hash_code();
        auto it = typeMap.find(hash);
        if (it != typeMap.end()) {
            return it->second;
        }

        const auto id = typeCounter++;
        typeMap.emplace(hash, id);

        if (id >= pools.size()) {
            pools.resize(id + 1);
        }

        return id;
    }

    // Ensure storage for entity e
    auto ensureEntity(std::size_t e) -> void {
        if (e >= alive.size()) {
            alive.resize(e + 1, 0);
            for (auto &bp : pools) {
                if (bp) {
                    bp->ensureSize(e + 1);
                }
            }
        }
    }

    // Ensure pool for typeId exists and sized
    template<typename T>
    auto ensurePool(std::size_t typeId) -> Pool<T>& {
        if (!pools[typeId]) {
            pools[typeId] = std::make_unique<Pool<T>>();
        }

        auto &pl = *static_cast<Pool<T>*>(pools[typeId].get());
        pl.ensureSize(alive.size());

        return pl;
    }
};

inline auto create_entity(World &w) -> Entity {
    const auto id = w.nextEntity++;
    w.ensureEntity(id);
    w.alive[id] = 1;
    return id;
}

inline auto destroy_entity(World &w, Entity e) -> void {
    if (e < w.alive.size()) {
        w.alive[e] = 0;
    }
}

// Add a component: validates, finds pool, stores data
template<typename T>
inline auto add_component(World &w, Entity e, const T &comp) -> Status {
    // 1. Validate entity
    if (e >= w.alive.size() || !w.alive[e]) {
        return Status::ERROR;
    }

    // 2. Get component-type ID
    const auto tid  = w.getTypeId<T>();

    // 3. Ensure pool exists and is sized
    auto &pool = w.ensurePool<T>(tid);

    // 4. Assign data and mark
    pool.data[e] = comp;
    pool.mask[e] = 1;

    return Status::OK;
}

// Get a component pointer or nullptr
template<typename T>
inline auto get_component(World &w, Entity e) -> T* {
    const auto tid = w.getTypeId<T>();
    if (tid >= w.pools.size() || !w.pools[tid]) {
        return nullptr;
    }

    auto &pool = *static_cast<Pool<T>*>(w.pools[tid].get());
    if (e >= pool.data.size() || !pool.mask[e]) {
        return nullptr;
    }

    return &pool.data[e];
}

// Remove a component
template<typename T>
inline auto remove_component(World &w, Entity e) -> Status {
    const auto tid = w.getTypeId<T>();
    if (tid >= w.pools.size() || !w.pools[tid]) {
        return Status::ERROR;
    }

    auto &pool = *static_cast<Pool<T>*>(w.pools[tid].get());
    if (e < pool.mask.size()) {
        pool.mask[e] = 0;
    }

    return Status::OK;
}

template<typename... Ts, typename Func, std::size_t... I>
inline auto invoke_query(Entity e, Func f, void* ptrs[],  std::index_sequence<I...>) -> void {
    f(e, static_cast<Ts*>(ptrs[I])...);
}

template<typename... Ts, typename Func>
inline auto query(World &w, Func f) -> void {
    // Collect type IDs for Ts
    std::array<std::size_t, sizeof...(Ts)> types = { w.getTypeId<Ts>()... };

    // Iterate all entities
    for (Entity e = 0; e < w.nextEntity; ++e) {
        if (e >= w.alive.size() || !w.alive[e]) {
            continue;
        }

        bool match = true;
        void* ptrs[sizeof...(Ts)];
        std::size_t idx = 0;

        // Check each component mask and record data ptr
        (([&]() {
            auto &pl = *static_cast<Pool<Ts>*>(w.pools[types[idx]].get());
            if (e >= pl.mask.size() || !pl.mask[e]) {
                match = false;
            } else {
                ptrs[idx] = &pl.data[e];
            }
            ++idx;
        }()), ...);

        if (!match) {
            continue;
        }

        // Invoke user function with unpacked ptrs
        invoke_query<Ts...>(e, f, ptrs, std::index_sequence_for<Ts...>{});
    }
}

} // namespace ec

#endif // EC_HPP

