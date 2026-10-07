#pragma once

// Chunk coordinate -> pool slot.
//
// A hash table with open addressing: entries live in one flat array, and a key
// that collides with another simply takes the next free entry ("linear
// probing"). Erasing uses backward-shift deletion, which moves later entries of
// the same probe run back into the hole instead of leaving a "deleted" marker.
// There is no allocation per entry and no tombstones, so it stays fast for the
// hundreds of thousands of lookups a large world makes per generation.
//
// Each coordinate must fit in 21 bits (+-1,048,575 chunks) so a key packs into
// 64 bits. ChunkWorld owns one of these; BitLife.h's test twin uses
// std::unordered_map instead, for clarity.

#include <cstdint>
#include <vector>

namespace gol3d {

class ChunkMap {
public:
    static constexpr uint32_t NONE = 0xFFFFFFFFu; // "not present"; equals NO_CHUNK
    static constexpr int COORD_BITS = 21;         // bits per axis in a packed key
    // The largest |coordinate|: one less than 2^20, so v + 2^20 is in 1 .. 2^21 - 1.
    static constexpr int COORD_LIMIT = (1 << (COORD_BITS - 1)) - 1;

    // Whether a chunk coordinate can be stored at all.
    static bool inRange(int x, int y, int z) {
        auto fits = [](int v) { return v >= -COORD_LIMIT && v <= COORD_LIMIT; };
        return fits(x) && fits(y) && fits(z);
    }

    // The slot stored for the coordinate, or NONE.
    uint32_t find(int x, int y, int z) const {
        if (entries_.empty() || !inRange(x, y, z)) return NONE;
        uint64_t key = pack(x, y, z);
        for (size_t i = homeIndex(key);; i = nextIndex(i)) {
            const Entry& entry = entries_[i];
            if (entry.slot == NONE) return NONE; // reached the end of the probe run
            if (entry.key == key) return entry.slot;
        }
    }

    // The coordinate must be in range and not present yet.
    void insert(int x, int y, int z, uint32_t slot) {
        // Keep the table at most 3/4 full so probe runs stay short. (This also
        // guarantees an empty entry exists, which ends every probe loop.)
        if ((count_ + 1) * 4 > entries_.size() * 3) grow();
        place(pack(x, y, z), slot);
        ++count_;
    }

    // Does nothing when the coordinate is not present.
    void erase(int x, int y, int z) {
        if (entries_.empty() || !inRange(x, y, z)) return;
        uint64_t key = pack(x, y, z);
        size_t hole = homeIndex(key);
        while (entries_[hole].slot != NONE && entries_[hole].key != key) {
            hole = nextIndex(hole);
        }
        if (entries_[hole].slot == NONE) return; // not present

        // Walk the rest of the probe run (Knuth, TAOCP vol. 3, 6.4 Algorithm R).
        // The entry at `candidate` may move back into the hole only if its home
        // index is outside (hole, candidate], counting cyclically; otherwise a
        // lookup starting at its home would stop at the hole and miss it.
        for (size_t candidate = nextIndex(hole); entries_[candidate].slot != NONE;
             candidate = nextIndex(candidate)) {
            size_t home = homeIndex(entries_[candidate].key);
            bool homeOutsideGap = hole <= candidate ? (home <= hole || home > candidate)
                                                    : (home <= hole && home > candidate);
            if (homeOutsideGap) {
                entries_[hole] = entries_[candidate];
                hole = candidate;
            }
        }
        entries_[hole] = Entry{};
        --count_;
    }

    void clear() {
        entries_.assign(entries_.size(), Entry{});
        count_ = 0;
    }

    size_t size() const { return count_; }

private:
    struct Entry {
        uint64_t key = 0;
        uint32_t slot = NONE; // NONE marks an empty entry
    };

    // COORD_BITS per axis, x lowest, each offset by 2^20 so negative
    // coordinates become positive.
    static uint64_t pack(int x, int y, int z) {
        constexpr uint64_t FIELD_MASK = (uint64_t{1} << COORD_BITS) - 1;
        auto field = [](int v) { return static_cast<uint64_t>(v + COORD_LIMIT + 1) & FIELD_MASK; };
        return field(x) | field(y) << COORD_BITS | field(z) << (2 * COORD_BITS);
    }

    // The first half of MurmurHash3's 64-bit finalizer (fmix64): mixes the high
    // key bits (y and z) into the low ones that pick the table index.
    static size_t hash(uint64_t key) {
        key ^= key >> 33;
        key *= 0xff51afd7ed558ccdull;
        key ^= key >> 33;
        return static_cast<size_t>(key);
    }

    size_t homeIndex(uint64_t key) const { return hash(key) & mask_; }
    size_t nextIndex(size_t i) const { return (i + 1) & mask_; }

    void place(uint64_t key, uint32_t slot) {
        size_t i = homeIndex(key);
        while (entries_[i].slot != NONE) {
            i = nextIndex(i);
        }
        entries_[i] = {key, slot};
    }

    // Doubles the table and re-inserts every entry. The size stays a power of
    // two, so `hash & mask_` is the hash modulo the size.
    void grow() {
        constexpr size_t INITIAL_ENTRIES = 1024;
        std::vector<Entry> old;
        old.swap(entries_);
        entries_.assign(old.empty() ? INITIAL_ENTRIES : old.size() * 2, Entry{});
        mask_ = entries_.size() - 1;
        for (const Entry& entry : old) {
            if (entry.slot != NONE) place(entry.key, entry.slot);
        }
    }

    std::vector<Entry> entries_;
    size_t mask_ = 0;  // entries_.size() - 1
    size_t count_ = 0; // entries in use
};

} // namespace gol3d
