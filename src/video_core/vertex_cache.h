
#pragma once

#include <algorithm>
#include <array>
#include "common/common_types.h"
#include "video_core/shader/shader.h"

// This little module emulates PICA's vertex shader's post transform vertex cache.
// TODO: when parallizing the shader units, this should be refactored into a
// 'multiton' aka 4 singletons with it's own size.
namespace VertexCache {

namespace {
constexpr size_t cache_max_size = 256 * 256;
constexpr size_t index_table_max_size = cache_max_size / 8;
// testing has proven that this size produces no misses at all in most games.
// in total the cache size is 128Kb
constexpr size_t cache_size = 1028;

// this is used to find if a vertex is in the cache
static std::array<u8, index_table_max_size> index_table = {};

#ifdef _DEBUG
static std::array<u8, index_table_max_size> seen_table = {};
u32 cache_misses = 0;
#endif

inline bool table_lookup(std::array<u8, index_table_max_size>& lookup, u32 position) {
    return (lookup[position / 8] & (0x01 << (position % 8))) != 0;
}

const u8 set_true = 0xFF;
const u8 set_false = 0;
inline void table_set(std::array<u8, index_table_max_size>& lookup, u32 position, u8 value) {
    const u8 mask = 0x01 << (position % 8);
    const u32 pos = position / 8;
    lookup[pos] = lookup[pos] ^ ((lookup[pos] ^ value) & mask);
}
// le cache
static std::array<Pica::Shader::OutputVertex, cache_size> cache;

// stores positions that are cached;
static std::array<u16, cache_size> remapper = {};

// used for clearing the cache;
u32 min_index = cache_size;
u32 max_index = 0;
} // Anonymous namespace

inline bool contains(u32 position) {
#ifdef _DEBUG
    if (table_lookup(seen_table, position) != table_lookup(index_table, position))
        cache_misses++;
#endif
    return table_lookup(index_table, position);
}

inline Pica::Shader::OutputVertex& obtain(u32 position) {
    return cache[position % cache_size];
}

inline void store(u32 position, Pica::Shader::OutputVertex& vertex) {
    const u32 remap_index = position % cache_size;
    table_set(index_table, remapper[remap_index], set_false);
    remapper[remap_index] = position;
    table_set(index_table, position, set_true);
    cache[remap_index] = vertex;
    max_index = std::max(max_index, remap_index);
    min_index = std::min(min_index, remap_index);
#ifdef _DEBUG
    table_set(seen_table, position, set_true);
#endif
}

void clear() {
    max_index++;
    auto result = std::minmax_element(remapper.begin() + min_index, remapper.begin() + max_index);
    const u32 min = *result.first / 8;
    const u32 max = *result.second / 8;
    std::fill(index_table.begin() + min, index_table.begin() + max + 1, 0);
    max_index = 0;
    min_index = cache_size;
#ifdef _DEBUG
    seen_table.fill(0);
    if (cache_misses > 0)
        LOG_TRACE(HW_GPU, "The vertex cache had %d misses", cache_misses);
    cache_misses = 0;
#endif
}

} // VertexCache
