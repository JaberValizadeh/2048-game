/*
 * MarkovPlayer.h
 *
 *  Created on: 03/04/2026
 *      Author: jaber
 *
 * Implements a Markov Decision Process (MDP) player using Expectimax search.
 *
 * MDP formulation for 2048:
 *   State  S     : current board configuration
 *   Actions A    : {left, right, up, down}
 *   Transition   : deterministic tile shift + stochastic new tile
 *                  (value 2 w.p. 0.9, value 4 w.p. 0.1, uniform over empty cells)
 *   Reward       : score gained from tile merges
 *
 * Solution — Expectimax search alternating two node types:
 *   MAX node    (player turn)  : choose action maximising expected value
 *   CHANCE node (env turn)     : average V(s') over all possible tile placements,
 *                                weighted by P(2)=0.9 and P(4)=0.1
 *
 * Leaf nodes are evaluated with a heuristic combining:
 *   - current score
 *   - empty-cell count  (log-scaled, preserves future move options)
 *   - corner bonus      (reward for keeping the max tile in a corner)
 *   - edge bonus        (reward for max tile on any edge, weaker than corner)
 *   - monotonicity      (tiles decrease from one corner along both axes)
 *   - smoothness        (penalise large differences between adjacent tiles)
 *   - merge potential   (reward adjacent equal tiles that can merge next turn)
 *
 */

#ifndef MARKOVPLAYER_H_
#define MARKOVPLAYER_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <vector>

using namespace std;

#include "Board.h"
#include "Player.h"

// Default Expectimax search depth (each level = one player move OR one tile placement).
const int MDP_DEPTH = 4;

// Cap on empty cells enumerated at a CHANCE node.
const int MDP_MAX_CHANCE_CELLS = 6;

class MarkovPlayer : public Player {
private:

    // Shared RNG used to shuffle empty cells in the chance node, eliminating the
    // systematic top-left bias that occurs when simply taking the first N cells.
    mutable mt19937 rng{42};

    bool unchanged(const Board& before, const Board& after) const {
        int n = before.getBoardSize();
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                if (before.getGrid(i, j) != after.getGrid(i, j))
                    return false;
        return true;
    }

    // Heuristic evaluation of a board state (used at leaf nodes of the search tree).

    double evaluate(const Board& board) const {
        int n = board.getBoardSize();

        double score = (double)board.getCurrentScore();

        int emptyCnt = board.countEmptyCells();
        double emptyBonus = (emptyCnt > 0) ? log2((double)(emptyCnt + 1)) * 512.0 : 0.0;

        int maxTile = 0, maxI = 0, maxJ = 0;
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (board.getGrid(i, j) > maxTile) {
                    maxTile = board.getGrid(i, j);
                    maxI = i; maxJ = j;
                }

        double cornerBonus = 0.0;
        if ((maxI == 0 || maxI == n - 1) && (maxJ == 0 || maxJ == n - 1))
            cornerBonus = (double)maxTile * 5.0;

        double edgeBonus = 0.0;
        if (cornerBonus == 0.0 &&
            (maxI == 0 || maxI == n - 1 || maxJ == 0 || maxJ == n - 1))
            edgeBonus = (double)maxTile * 1.5;

        double monotonicity = 0.0;
        for (int i = 0; i < n; i++) {
            double left = 0.0, right = 0.0;
            for (int j = 1; j < n; j++) {
                double cur  = board.getGrid(i, j)   > 0 ? log2(board.getGrid(i, j))   : 0.0;
                double prev = board.getGrid(i, j-1) > 0 ? log2(board.getGrid(i, j-1)) : 0.0;
                if (prev > cur) left  += prev - cur;
                else            right += cur  - prev;
            }
            monotonicity -= min(left, right);

            double up = 0.0, down = 0.0;
            for (int j = 1; j < n; j++) {
                double cur  = board.getGrid(j,   i) > 0 ? log2(board.getGrid(j,   i)) : 0.0;
                double prev = board.getGrid(j-1, i) > 0 ? log2(board.getGrid(j-1, i)) : 0.0;
                if (prev > cur) up   += prev - cur;
                else            down += cur  - prev;
            }
            monotonicity -= min(up, down);
        }

        double smoothness = 0.0;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (board.getGrid(i, j) == 0) continue;
                double val = log2(board.getGrid(i, j));
                if (j + 1 < n && board.getGrid(i, j+1) > 0)
                    smoothness -= fabs(val - log2(board.getGrid(i, j+1)));
                if (i + 1 < n && board.getGrid(i+1, j) > 0)
                    smoothness -= fabs(val - log2(board.getGrid(i+1, j)));
            }
        }

        double mergePotential = 0.0;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                int v = board.getGrid(i, j);
                if (v == 0) continue;
                if (j + 1 < n && board.getGrid(i, j+1) == v)
                    mergePotential += log2((double)v);
                if (i + 1 < n && board.getGrid(i+1, j) == v)
                    mergePotential += log2((double)v);
            }
        }

        return score
             + 130.0 * emptyBonus       // rescaled from 270×512 → keeps other terms relevant
             + 10.0  * cornerBonus      // multiplier ×5 baked into cornerBonus already
             + 5.0   * edgeBonus        // new term
             + 110.0 * monotonicity     // raised from 47 — ordering is a primary driver
             + 15.0  * smoothness       // raised from 10
             + 80.0  * mergePotential;  // new term — reward imminent merges
    }

    // Core Expectimax recursion.
    //   isChance == false  →  MAX node   (player selects best action)
    //   isChance == true   →  CHANCE node (environment places a random tile)
    double expectimax(const Board& board, int depth, bool isChance) const {

        if (board.hasWon())
            return (double)(board.getCurrentScore() + board.getBonus());
        if (!board.canMove())
            return (double)board.getCurrentScore();
        if (depth == 0)
            return evaluate(board);

        if (!isChance) {
            double best = -numeric_limits<double>::infinity();
            array<char, 4> moves = {'l', 'r', 'u', 'd'};
            for (char mv : moves) {
                Board next(board);
                next.makeMove(mv);
                if (unchanged(board, next)) continue;   // invalid move — skip
                double val = expectimax(next, depth - 1, true);
                if (val > best) best = val;
            }
            return (best == -numeric_limits<double>::infinity()) ? evaluate(board) : best;

        } else {
            // ---- CHANCE node: enumerate possible tile placements ----
            // The environment picks one empty cell uniformly at random and
            // places a 2 (prob 0.9) or a 4 (prob 0.1).
            //
            // E[V | state s] = (1/|empties|) * sum_c [ 0.9*V(s,c,2) + 0.1*V(s,c,4) ]
            //
            // When |empties| > MDP_MAX_CHANCE_CELLS we sample a random subset.
            // Shuffling before truncation avoids the original top-left positional
            // bias (which arose from always taking cells in row-major order).

            int n = board.getBoardSize();
            vector<pair<int,int>> empties;
            for (int i = 0; i < n; i++)
                for (int j = 0; j < n; j++)
                    if (board.getGrid(i, j) == 0)
                        empties.push_back({i, j});

            if (empties.empty())
                return evaluate(board);

            // Random shuffle before capping — uniform coverage of the board
            if ((int)empties.size() > MDP_MAX_CHANCE_CELLS) {
                shuffle(empties.begin(), empties.end(), rng);
                empties.resize(MDP_MAX_CHANCE_CELLS);
            }

            double total = 0.0;
            for (auto& cell : empties) {
                // Tile = 2  (probability 0.9)
                Board b2(board);
                b2.setGrid(cell.first, cell.second, 2);
                double v2 = expectimax(b2, depth - 1, false);

                // Tile = 4  (probability 0.1)
                Board b4(board);
                b4.setGrid(cell.first, cell.second, 4);
                double v4 = expectimax(b4, depth - 1, false);

                total += 0.9 * v2 + 0.1 * v4;
            }

            return total / (double)empties.size();
        }
    }

public:
    MarkovPlayer() { name = "Markov Player"; }

    char getMove(const Board& board) {
        array<char, 4> moves = {'l', 'r', 'u', 'd'};
        priority_queue<Move> moveQueue;

        // Adaptive depth: invest extra search when the board is nearly full.
        // A near-full board has fewer branching options at chance nodes, so the
        // deeper search is cheap AND most critical (wrong moves are unrecoverable).
        int empties = board.countEmptyCells();
        int searchDepth = MDP_DEPTH;
        if (empties <= 4) searchDepth += 1;

        cout << "MDP evaluation (depth=" << searchDepth << "): ";
        for (char mv : moves) {
            Board next(board);
            next.makeMove(mv);
            if (unchanged(board, next)) {
                cout << mv << "=* ";
                continue;
            }
            // After the player's move comes a CHANCE node (tile placement)
            double val = expectimax(next, searchDepth - 1, true);
            moveQueue.push(Move(mv, val));
            cout << mv << "=" << val << " ";
        }
        cout << endl;

        if (moveQueue.empty())
            return board.getRandomMove();
        return moveQueue.top().direction;
    }
};

#endif /* MARKOVPLAYER_H_ */
