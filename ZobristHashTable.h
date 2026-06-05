#ifndef ZOBRISTHASHTABLE_H_
#define ZOBRISTHASHTABLE_H_

#include <cstdint>
#include <random>
#include <vector>
#include <iostream>
#include "Board.h"

class ZobristHashTable {
private:

    struct Entry {
        uint64_t key = 0;
        double value = 0.0;
        bool occupied = false;
    };

    static constexpr int MAX_CELLS = 225;
    static constexpr int MAX_TILE_IDX = 18;
    static constexpr int TABLE_BITS = 18;
    static constexpr int TABLE_SIZE = 1 << TABLE_BITS;
    static constexpr int TABLE_MASK = TABLE_SIZE - 1;
    static constexpr int PROBE_LIMIT = 8;

    std::vector<Entry> table;
    uint64_t Z[MAX_CELLS][MAX_TILE_IDX];

    int hits = 0;
    int misses = 0;
    int stored = 0;

    static inline int tileToIndex(int value) {
        if (value <= 0) return 0;
        return 63 - __builtin_clzll(value); // faster than loop
    }

public:

    ZobristHashTable()
        : table(TABLE_SIZE) {

        std::mt19937_64 rng(12345ULL);

        for (auto& row : Z)
            for (auto& x : row)
                x = rng();
    }

    ZobristHashTable(const ZobristHashTable&) = delete;
    ZobristHashTable& operator=(const ZobristHashTable&) = delete;

    uint64_t computeHash(const Board& board) const {
        uint64_t h = 0;
        int n = board.getBoardSize();

        for (int i = 0; i < n; ++i) {
            int base = i * n;
            for (int j = 0; j < n; ++j) {
                int cell = base + j;
                int tile = tileToIndex(board.getGrid(i, j));
                h ^= Z[cell][tile];
            }
        }
        return h;
    }

    bool lookup(uint64_t key, double& value) {
        int base = key & TABLE_MASK;

        for (int i = 0; i < PROBE_LIMIT; ++i) {
            int pos = (base + i) & TABLE_MASK;

            if (!table[pos].occupied) break;

            if (table[pos].key == key) {
                value = table[pos].value;
                ++hits;
                return true;
            }
        }

        ++misses;
        return false;
    }

    void store(uint64_t key, double value) {
        int base = key & TABLE_MASK;

        for (int i = 0; i < PROBE_LIMIT; ++i) {
            int pos = (base + i) & TABLE_MASK;

            if (!table[pos].occupied || table[pos].key == key) {
                table[pos] = { key, value, true };
                ++stored;
                return;
            }
        }

        table[base] = { key, value, true };
        ++stored;
    }

    void clear() {
        for (auto& e : table)
            e.occupied = false;

        hits = misses = stored = 0;
    }

    void printStats() const {
        int total = hits + misses;
        double rate = total ? (100.0 * hits / total) : 0.0;

        std::cout << "[Zobrist] hits=" << hits
            << " misses=" << misses
            << " stored=" << stored
            << " hit-rate=" << rate << "%\n";
    }
};

#endif