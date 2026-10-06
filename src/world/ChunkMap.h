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
// 64 bits.

#include <cstdint>
#include <vector>

namespace gol3d {

class ChunkMap {
public:
    static constexpr uint32_t NONE = 0xFFFFFFFFu;
    static constexpr int COORD_LIMIT = (1 << 20) - 1;

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
        // Keep the table at most 3/4 full so probe runs stay short.
        if ((count_ + 1) * 4 > entries_.size() * 3) grow();
        place(pack(x, y, z), slot);
        ++count_;
    }

    void erase(int x, int y, int z) {
        if (entries_.empty() || !inRange(x, y, z)) return;
        uint64_t key = pack(x, y, z);
        size_t i = homeIndex(key);
        while (entries_[i].slot != NONE && entries_[i].key != key) {
            i = nextIndex(i);
        }
        if (entries_[i].slot == NONE) return; // not present

        // Walk the rest of the probe run. An entry may move back into the hole
        // only if its home index is outside (hole, j], counting cyclically;
        // otherwise a lookup starting at its home would no longer pass over it.
        size_t hole = i;
        for (size_t j = nextIndex(hole); entries_[j].slot != NONE; j = nextIndex(j)) {
            size_t home = homeIndex(entries_[j].key);
            bool canMoveIntoHole =
                hole <= j ? (home <= hole || home > j) : (home <= hole && home > j);
            if (canMoveIntoHole) {
                entries_[hole] = entries_[j];
                hole = j;
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

    // 21 bits per axis, offset so negative coordinates become positive.
    static uint64_t pack(int x, int y, int z) {
        auto field = [](int v) { return static_cast<uint64_t>(v + COORD_LIMIT + 1) & 0x1FFFFF; };
        return field(x) | field(y) << 21 | field(z) << 42;
    }

    // MurmurHash3's 64-bit finalizer: spreads every key bit over the whole hash.
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

    // Doubles the table (its size is always a power of two) and re-inserts.
    void grow() {
        std::vector<Entry> old;
        old.swap(entries_);
        entries_.assign(old.empty() ? 1024 : old.size() * 2, Entry{});
        mask_ = entries_.size() - 1;
        for (const Entry& entry : old) {
            if (entry.slot != NONE) place(entry.key, entry.slot);
        }
    }

    std::vector<Entry> entries_;
    size_t mask_ = 0;
    size_t count_ = 0;
};

} // namespace gol3d
