/*
 * ImprovedMonteCarloPlayer.h
 *
 *  Created on: 13/04/2026
 *      Author: Jaber
 *
 * Extends MonteCarloPlayer with a Zobrist Hash Table cache.
 *
 * KEY IDEA
 * ─────────
 * During one move decision, getMove() calls simulation() for each of the
 * (up to) 4 candidate directions.  Each simulation() call runs
 * IMP_SIMULATION_TIMES independent random playouts (expand()).
 *
 * Many of those playouts start from boards that have already been seen
 * earlier in the same move decision.  A Zobrist hash table lets us
 * skip re-computing a known result:
 *
 *   simulation(board):
 *     h = Zobrist hash of board          O(n²) XOR pass
 *     if cache[h] exists  → return it    O(1)  — avoids all playouts
 *     else run playouts, store result    normal cost
 *
 * ZOBRIST HASHING RECAP
 * ──────────────────────
 * At startup a table Z[cell][tileIndex] is filled with random 64-bit numbers.
 *   cell      = row × boardSize + col
 *   tileIndex = log₂(tileValue), or 0 for an empty cell
 *
 * hash(board) = XOR of Z[cell][tileIndex] for every cell
 *
 * Because XOR is its own inverse, swapping one tile needs only two XORs —
 * making incremental updates O(1).  For full recomputation (used here)
 * the cost is O(n²), which is negligible compared with a playout loop.
 */

#ifndef IMPROVEDMONTECARLOPLAYER_H_
#define IMPROVEDMONTECARLOPLAYER_H_

#include <array>
#include <iostream>
#include <queue>
#include <string>
#include <cstdint>
#include <random>
#include <climits>

using namespace std;

#include "Board.h"
#include "Player.h"

// ── Tuning constants (separate names to avoid collisions with MonteCarloPlayer) ─
static const int IMP_SIMULATION_TIMES = 200;
static const int IMP_MAX_DEPTH        = 10;

// ═══════════════════════════════════════════════════════════════════════════════
//  ZobristCache  — embedded hash table used only by ImprovedMonteCarloPlayer
// ═══════════════════════════════════════════════════════════════════════════════
class ZobristCache {
    // ── Zobrist random-number table dimensions ───────────────────────────────
    static const int MAX_CELLS    = 225;  // 15×15 max board
    static const int MAX_TILE_IDX = 18;   // log₂ values: 0(empty)…17(131072)

    // ── Open-addressing hash table dimensions ────────────────────────────────
    static const int TABLE_BITS   = 18;               // 2^18 = 262 144 slots
    static const int TABLE_SIZE   = 1 << TABLE_BITS;
    static const int TABLE_MASK   = TABLE_SIZE - 1;
    static const int PROBE_LIMIT  = 8;                // max linear-probe steps
    static const uint64_t EMPTY_KEY = UINT64_MAX;     // sentinel for free slot

    // ── One slot ─────────────────────────────────────────────────────────────
    struct Entry {
        uint64_t key;   // Zobrist hash; EMPTY_KEY = vacant
        double   value; // cached simulation score
    };

    uint64_t Z[MAX_CELLS][MAX_TILE_IDX]; // Zobrist random numbers  (~32 KB)
    Entry*   table;                      // heap-allocated hash table (~4 MB)

    // ── Statistics ───────────────────────────────────────────────────────────
    int hits, misses, stored;

    // ── Map tile value → compact index ───────────────────────────────────────
    static int tileToIndex(int value) {
        if (value <= 0) return 0;
        int idx = 0;
        while (value > 1) { value >>= 1; ++idx; }
        return idx;   // log₂(original value)
    }

public:
    // ── Constructor: fill Z with reproducible random 64-bit numbers ──────────
    ZobristCache() : hits(0), misses(0), stored(0) {
        table = new Entry[TABLE_SIZE];
        for (int i = 0; i < TABLE_SIZE; ++i)
            table[i] = { EMPTY_KEY, 0.0 };

        mt19937_64 rng(98765ULL);   // 64-bit Mersenne Twister, fixed seed
        for (int c = 0; c < MAX_CELLS; ++c)
            for (int t = 0; t < MAX_TILE_IDX; ++t)
                Z[c][t] = rng();
    }

    ~ZobristCache() { delete[] table; }

    // ── computeHash: XOR together Z values for every cell ───────────────────
    uint64_t computeHash(const Board& board) const {
        uint64_t h = 0;
        int n = board.getBoardSize();
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                h ^= Z[i * n + j][tileToIndex(board.getGrid(i, j))];
        return h;
    }

    // ── lookup: linear probe for key, write value on hit ────────────────────
    bool lookup(uint64_t key, double& value) {
        int base = (int)(key & TABLE_MASK);
        for (int i = 0; i < PROBE_LIMIT; ++i) {
            int pos = (base + i) & TABLE_MASK;
            if (table[pos].key == EMPTY_KEY) break;     // gap → not present
            if (table[pos].key == key) {
                value = table[pos].value;
                ++hits;
                return true;
            }
        }
        ++misses;
        return false;
    }

    // ── store: insert with linear probing; evict base slot on overflow ───────
    void store(uint64_t key, double value) {
        int base = (int)(key & TABLE_MASK);
        for (int i = 0; i < PROBE_LIMIT; ++i) {
            int pos = (base + i) & TABLE_MASK;
            if (table[pos].key == EMPTY_KEY || table[pos].key == key) {
                table[pos] = { key, value };
                ++stored;
                return;
            }
        }
        table[base] = { key, value };   // evict on overflow
        ++stored;
    }

    // ── clear: wipe all slots and reset stats ─────────────────────────────
    void clear() {
        for (int i = 0; i < TABLE_SIZE; ++i)
            table[i] = { EMPTY_KEY, 0.0 };
        hits = misses = stored = 0;
    }

    // ── printStats ────────────────────────────────────────────────────────
    void printStats() const {
        int total = hits + misses;
        double rate = (total > 0) ? (100.0 * hits / total) : 0.0;
        cout << "[ZobristCache] hits=" << hits
             << "  misses=" << misses
             << "  stored=" << stored
             << "  hit-rate=" << rate << "%" << endl;
    }
};


// ═══════════════════════════════════════════════════════════════════════════════
//  ImprovedMonteCarloPlayer
// ═══════════════════════════════════════════════════════════════════════════════
class ImprovedMonteCarloPlayer : public Player {
private:
    ZobristCache cache;   // one cache per player, persists across turns

    // ── unchanged: returns true if the move had no effect ─────────────────
    bool unchanged(const Board& before, const Board& after) const {
        int n = before.getBoardSize();
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                if (before.getGrid(i, j) != after.getGrid(i, j))
                    return false;
        return true;
    }

    // ── expand: one random playout from board, returns terminal score ──────
    double expand(Board board) {
        int depth = IMP_MAX_DEPTH;
        while (depth-- >= 0) {
            if (board.hasWon())
                return (double)(board.getCurrentScore() + board.getBonus());
            if (!board.canMove())
                return (double)board.getCurrentScore();

            board.makeMove(board.getRandomMove());
            board.addNewTile();
        }
        return (double)board.getCurrentScore();
    }

    // ── simulation: average over IMP_SIMULATION_TIMES playouts ────────────
    //   Cache protocol:
    //     1. Compute Zobrist hash of the board.
    //     2. HIT  → return cached average immediately (0 playouts needed).
    //     3. MISS → run playouts, store result, return average.
    double simulation(const Board& board) {
        // Step 1 — hash
        uint64_t h = cache.computeHash(board);

        // Step 2 — lookup
        double cached;
        if (cache.lookup(h, cached))
            return cached;          // cache HIT: skip all playouts

        // Step 3 — miss: run IMP_SIMULATION_TIMES random playouts
        double total = 0.0;
        for (int s = 0; s < IMP_SIMULATION_TIMES; ++s)
            total += expand(board);
        double avg = total / (double)IMP_SIMULATION_TIMES;

        // Step 4 — store for future hits
        cache.store(h, avg);
        return avg;
    }

public:
    ImprovedMonteCarloPlayer() { name = "Improved Monte Carlo Player (Zobrist)"; }

    // ── getMove: evaluate all 4 directions, pick the best ─────────────────
    char getMove(const Board& board) {
        // Clear the cache at the start of each move so that scores from
        // the previous board state never pollute the current decision.
        cache.clear();

        array<char, 4> moves = {'l', 'r', 'u', 'd'};
        priority_queue<Move> moveQueue;

        cout << "Simulation outcome: ";
        for (int i = 0; i < (int)moves.size(); ++i) {
            Board tempBoard(board);
            tempBoard.makeMove(moves[i]);
            tempBoard.addNewTile();

            if (unchanged(board, tempBoard)) {
                cout << moves[i] << "=* ";      // illegal move
            } else {
                double value = simulation(tempBoard);
                moveQueue.push(Move(moves[i], value));
                cout << moves[i] << "=" << value << " ";
            }
        }
        cout << endl;

        // Show cache efficiency for this move
        cache.printStats();

        return moveQueue.empty() ? board.getRandomMove()
                                 : moveQueue.top().direction;
    }
};

#endif /* IMPROVEDMONTECARLOPLAYER_H_ */
