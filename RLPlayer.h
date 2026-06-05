/*
 * RLPlayer.h
 *
 *  Created on: 10/04/2026
 *      Author: jaber
 *
 * Reinforcement Learning player for 2048 using N-Tuple Network with
 * Temporal Difference (TD-0) afterstate learning.
 *
 * Architecture:
 *   - Value function V(s) approximated by a 6-tuple network
 *   - Each tuple maps 6 board cells to a weight lookup table (16^6 entries)
 *   - 8 board symmetries (rotations + reflections) for generalisation
 *   - Afterstate learning: V(s) estimates expected future reward from
 *     the board state AFTER the player's move but BEFORE the random tile
 *
 * Training:
 *   - Self-play using TD(0) updates on afterstates
 *   - Update rule: V(s_t) += alpha * [r_{t+1} + V(s_{t+1}) - V(s_t)]
 *   - Terminal update: V(s_final) += alpha * [0 - V(s_final)]
 *   - Weights saved to "rl_weights.bin" after training and reloaded on
 *     subsequent runs to avoid retraining
 *
 * References:
 *   Szubert & Jaskowski (2014) "Temporal Difference Learning of N-Tuple
 *   Networks for the Game 2048"
 */

#ifndef RLPLAYER_H_
#define RLPLAYER_H_

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include "Board.h"
#include "Player.h"

using namespace std;

class RLPlayer : public Player {
private:
    // --- N-tuple network configuration ---
    static const int TILE_STATES   = 16;      // tile indices 0-15
    static const int TUPLE_LEN     = 6;       // 6-tuple patterns
    static const int NUM_PATTERNS  = 4;
    static const int NUM_SYMMETRIES = 8;

    // 16^6 = 16,777,216 entries per pattern lookup table
    static const int LUT_SIZE = 16 * 16 * 16 * 16 * 16 * 16;

    int patterns[NUM_PATTERNS][TUPLE_LEN][2];
    vector<float> lut[NUM_PATTERNS];
    float alpha;
    int trainGames;

    // =================================================================
    // Pattern initialisation -- 4 distinct 6-tuple shapes
    //
    // These patterns are chosen to capture spatial relationships across
    // the board. With 8 symmetries each, they cover the full board well.
    //
    // Pattern 0:  X X X X    (top row + 2 cells below-left)
    //             X X . .
    //             . . . .
    //             . . . .
    //
    // Pattern 1:  X X X .    (2x3 rectangular block)
    //             X X X .
    //             . . . .
    //             . . . .
    //
    // Pattern 2:  X X . .    (3x2 rectangular block)
    //             X X . .
    //             X X . .
    //             . . . .
    //
    // Pattern 3:  X X . .    (diagonal staircase)
    //             . X X .
    //             . . X X
    //             . . . .
    // =================================================================
    void initPatterns() {
        // Pattern 0: top row (4 cells) + 2 cells below
        int p0[][2] = {{0,0},{0,1},{0,2},{0,3},{1,0},{1,1}};
        // Pattern 1: 2x3 block top-left
        int p1[][2] = {{0,0},{0,1},{0,2},{1,0},{1,1},{1,2}};
        // Pattern 2: 3x2 block left
        int p2[][2] = {{0,0},{0,1},{1,0},{1,1},{2,0},{2,1}};
        // Pattern 3: diagonal staircase
        int p3[][2] = {{0,0},{0,1},{1,1},{1,2},{2,2},{2,3}};

        int (*all[])[2] = {p0, p1, p2, p3};
        for (int p = 0; p < NUM_PATTERNS; p++)
            for (int t = 0; t < TUPLE_LEN; t++) {
                patterns[p][t][0] = all[p][t][0];
                patterns[p][t][1] = all[p][t][1];
            }
    }

    // =================================================================
    // Tile value -> index:  0->0, 2->1, 4->2, ..., 32768->15
    // =================================================================
    int tileIdx(int val) const {
        if (val == 0) return 0;
        int idx = 0;
        while (val > 1) { val >>= 1; idx++; }
        return (idx < TILE_STATES) ? idx : TILE_STATES - 1;
    }

    // =================================================================
    // 8 symmetry transformations on a 4x4 grid coordinate
    // =================================================================
    void transformCoord(int r, int c, int sym, int &tr, int &tc) const {
        switch (sym) {
            case 0: tr = r;     tc = c;     break;
            case 1: tr = c;     tc = 3 - r; break;
            case 2: tr = 3 - r; tc = 3 - c; break;
            case 3: tr = 3 - c; tc = r;     break;
            case 4: tr = r;     tc = 3 - c; break;
            case 5: tr = c;     tc = r;     break;
            case 6: tr = 3 - r; tc = c;     break;
            case 7: tr = 3 - c; tc = 3 - r; break;
        }
    }

    // =================================================================
    // Compute LUT index for a pattern under a given symmetry
    // =================================================================
    int patternIndex(int grid[4][4], int pat, int sym) const {
        int idx = 0;
        for (int t = 0; t < TUPLE_LEN; t++) {
            int tr, tc;
            transformCoord(patterns[pat][t][0], patterns[pat][t][1], sym, tr, tc);
            idx = idx * TILE_STATES + tileIdx(grid[tr][tc]);
        }
        return idx;
    }

    // =================================================================
    // V(s) = sum of weights across all patterns and symmetries
    // =================================================================
    float evalGrid(int grid[4][4]) const {
        float v = 0.0f;
        for (int s = 0; s < NUM_SYMMETRIES; s++)
            for (int p = 0; p < NUM_PATTERNS; p++)
                v += lut[p][patternIndex(grid, p, s)];
        return v;
    }

    // =================================================================
    // TD weight update: each active weight += alpha * delta
    // =================================================================
    void updateGrid(int grid[4][4], float delta) {
        for (int s = 0; s < NUM_SYMMETRIES; s++)
            for (int p = 0; p < NUM_PATTERNS; p++)
                lut[p][patternIndex(grid, p, s)] += alpha * delta;
    }

    // =================================================================
    // Extract a 4x4 grid from a Board object (used during actual play)
    // =================================================================
    void boardToGrid(const Board &b, int grid[4][4]) const {
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                grid[r][c] = b.getGrid(r, c);
    }

    bool gridsEqual(int a[4][4], int b[4][4]) const {
        return memcmp(a, b, sizeof(int) * 16) == 0;
    }

    // =================================================================
    // Training via self-play on lightweight TrainBoard
    // =================================================================
    void train() {
        cout << "Training RL player with " << trainGames
             << " self-play games (this is a one-time cost)..." << endl;

        double totalScore = 0.0;
        int maxTileSeen = 0;

        for (int g = 0; g < trainGames; g++) {
            Board board(4, true);   // quiet=true suppresses per-game console output

            int prevAfter[4][4];
            bool hasPrev = false;
            float prevV = 0.0f;

            while (board.canMove() && !board.hasWon()) {
                int origCells[4][4];
                boardToGrid(board, origCells);
                int origScore = board.getCurrentScore();

                float bestVal = -1e18f;
                int bestDir = -1;
                int bestAfter[4][4];
                float bestReward = 0.0f;
                float bestV = 0.0f;

                for (int d = 0; d < 4; d++) {
                    Board tmp(board);

                    if (!tmp.makeMove(d)) continue;

                    int tmpGrid[4][4];
                    boardToGrid(tmp, tmpGrid);

                    float reward = (float)(tmp.getCurrentScore() - origScore);
                    float v = evalGrid(tmpGrid);
                    float val = reward + v;

                    if (val > bestVal) {
                        bestVal = val;
                        bestDir = d;
                        memcpy(bestAfter, tmpGrid, sizeof(bestAfter));
                        bestReward = reward;
                        bestV = v;
                    }
                }

                if (bestDir < 0) break;

                // TD(0) update for previous afterstate
                if (hasPrev) {
                    float tdError = bestReward + bestV - prevV;
                    updateGrid(prevAfter, tdError);
                }

                memcpy(prevAfter, bestAfter, sizeof(prevAfter));
                prevV = bestV;
                hasPrev = true;

                board.makeMove(bestDir);
                board.addRandomTile();
            }

            // Terminal update: target = 0
            if (hasPrev) {
                updateGrid(prevAfter, -prevV);
            }

            totalScore += board.getCurrentScore();
            int mt = board.maxTile();
            if (mt > maxTileSeen) maxTileSeen = mt;

            if ((g + 1) % 10000 == 0) {
                cout << "  Games: " << (g + 1)
                     << "  |  Avg score: " << (totalScore / (g + 1))
                     << "  |  Max tile seen: " << maxTileSeen << endl;
            }
        }

        cout << "Training complete.  Avg score: "
             << (totalScore / trainGames)
             << "  |  Max tile: " << maxTileSeen << endl;
    }

    // =================================================================
    // Save / load weights
    // =================================================================
    bool saveWeights(const string &fname) const {
        ofstream f(fname, ios::binary);
        if (!f) return false;
        for (int p = 0; p < NUM_PATTERNS; p++)
            f.write(reinterpret_cast<const char*>(lut[p].data()),
                    (long long)LUT_SIZE * sizeof(float));
        return f.good();
    }

    bool loadWeights(const string &fname) {
        ifstream f(fname, ios::binary);
        if (!f) return false;
        f.seekg(0, ios::end);
        long long expectedSize = (long long)NUM_PATTERNS * LUT_SIZE * sizeof(float);
        if (f.tellg() != expectedSize) return false;
        f.seekg(0, ios::beg);
        for (int p = 0; p < NUM_PATTERNS; p++)
            f.read(reinterpret_cast<char*>(lut[p].data()),
                   (long long)LUT_SIZE * sizeof(float));
        return f.good();
    }

public:
    // =================================================================
    // Constructor
    // =================================================================
    RLPlayer(int games = 200000, float lr = 0.0025f) : alpha(lr), trainGames(games) {
        name = "RL Player (N-Tuple TD Learning)";
        initPatterns();

        for (int p = 0; p < NUM_PATTERNS; p++)
            lut[p].assign(LUT_SIZE, 0.0f);

        if (loadWeights("rl_weights.bin")) {
            cout << "Loaded pre-trained RL weights from rl_weights.bin" << endl;
        } else {
            train();
            if (saveWeights("rl_weights.bin"))
                cout << "Weights saved to rl_weights.bin" << endl;
            else
                cout << "Warning: could not save weights." << endl;
        }
    }

    // =================================================================
    // getMove -- select action maximising reward + V(afterstate)
    // =================================================================
    char getMove(const Board &board) {
        int bs = board.getBoardSize();
        if (bs != 4)
            return board.getRandomMove();

        int origGrid[4][4];
        boardToGrid(board, origGrid);
        int origScore = board.getCurrentScore();

        char dirs[] = {'l', 'r', 'u', 'd'};
        float bestVal = -1e18f;
        char bestMove = 'l';

        cout << "RL evaluation: ";
        for (int m = 0; m < 4; m++) {
            Board tmp(board);
            tmp.makeMove(dirs[m]);

            int afterGrid[4][4];
            boardToGrid(tmp, afterGrid);

            if (gridsEqual(origGrid, afterGrid)) {
                cout << dirs[m] << "=* ";
                continue;
            }

            float reward = (float)(tmp.getCurrentScore() - origScore);
            float v = evalGrid(afterGrid);
            float val = reward + v;

            cout << dirs[m] << "=" << val << " ";

            if (val > bestVal) {
                bestVal = val;
                bestMove = dirs[m];
            }
        }
        cout << endl;

        return bestMove;
    }
};

#endif /* RLPLAYER_H_ */
