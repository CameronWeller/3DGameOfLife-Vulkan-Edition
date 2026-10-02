#pragma once

#include <cstdint>
#include <vector>

namespace VulkanHIP {

// Chunk coordinate -> pool slot. Open addressing with linear probing and
// backward-shift deletion: no allocation per entry and no tombstones, so it
// stays fast for the hundreds of thousands of lookups a large world makes per
// generation. Coordinates must fit in 21 bits each (+-1,048,575 chunks).
class ChunkMap {
public:
    static constexpr uint32_t NONE = 0xFFFFFFFFu;
    static constexpr int COORD_LIMIT = (1 << 20) - 1;

    static bool inRange(int x, int y, int z) {
        return x >= -COORD_LIMIT && x <= COORD_LIMIT && y >= -COORD_LIMIT && y <= COORD_LIMIT && z >= -COORD_LIMIT &&
               z <= COORD_LIMIT;
    }

    uint32_t find(int x, int y, int z) const {
        if (entries.empty() || !inRange(x, y, z)) return NONE;
        uint64_t key = pack(x, y, z);
        for (size_t i = hash(key) & mask;; i = (i + 1) & mask) {
            const Entry& e = entries[i];
            if (e.slot == NONE) return NONE;
            if (e.key == key) return e.slot;
        }
    }

    // The coordinate must be in range and not present yet.
    void insert(int x, int y, int z, uint32_t slot) {
        if ((count + 1) * 4 > entries.size() * 3) grow();
        place(pack(x, y, z), slot);
        ++count;
    }

    void erase(int x, int y, int z) {
        if (entries.empty() || !inRange(x, y, z)) return;
        uint64_t key = pack(x, y, z);
        size_t i = hash(key) & mask;
        while (entries[i].slot != NONE && entries[i].key != key) i = (i + 1) & mask;
        if (entries[i].slot == NONE) return;
        // Shift later entries of the probe run back into the hole.
        size_t hole = i;
        for (size_t j = (hole + 1) & mask; entries[j].slot != NONE; j = (j + 1) & mask) {
            size_t home = hash(entries[j].key) & mask;
            bool movable = hole <= j ? (home <= hole || home > j) : (home <= hole && home > j);
            if (movable) {
                entries[hole] = entries[j];
                hole = j;
            }
        }
        entries[hole] = Entry{};
        --count;
    }

    void clear() {
        entries.assign(entries.size(), Entry{});
        count = 0;
    }

    size_t size() const { return count; }

private:
    struct Entry {
        uint64_t key = 0;
        uint32_t slot = NONE;
    };

    static uint64_t pack(int x, int y, int z) {
        auto field = [](int v) { return static_cast<uint64_t>(v + COORD_LIMIT + 1) & 0x1FFFFF; };
        return field(x) | field(y) << 21 | field(z) << 42;
    }

    static size_t hash(uint64_t key) {
        key ^= key >> 33;
        key *= 0xff51afd7ed558ccdull;
        key ^= key >> 33;
        return static_cast<size_t>(key);
    }

    void place(uint64_t key, uint32_t slot) {
        size_t i = hash(key) & mask;
        while (entries[i].slot != NONE) i = (i + 1) & mask;
        entries[i] = {key, slot};
    }

    void grow() {
        std::vector<Entry> old;
        old.swap(entries);
        entries.assign(old.empty() ? 1024 : old.size() * 2, Entry{});
        mask = entries.size() - 1;
        for (const Entry& e : old)
            if (e.slot != NONE) place(e.key, e.slot);
    }

    std::vector<Entry> entries;
    size_t mask = 0;
    size_t count = 0;
};

} // namespace VulkanHIP
