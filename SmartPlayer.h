/*
 * SmartPlayer.h
 *
 *  Created on: 06 Apr 2026
 *      Author: jaber
 *
 * Smart player for 2048 with an improved heuristic value function.
 *
 * Value function (heuristic estimate):
 *   h = w_empty   * log(emptyCount + 1)
 *     + w_corner  * maxTile          (if max tile sits in any corner)
 *     + w_mono    * monotonicity      (penalises non-monotone rows/cols using
 *                                      log2 tile values, so large tiles count more)
 *     + w_smooth  * smoothness        (rewards adjacent tiles that are close in value)
 */

#ifndef SMARTPLAYER_CPP_
#define SMARTPLAYER_CPP_

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <random>

#include "Board.h"
#include "Player.h"

using namespace std;

class SmartPlayer : public Player {
public:
    SmartPlayer() { name = "Smart Player"; }

    char getMove(const Board &board) {
        array<char, 4> moves1 = {'l', 'r', 'u', 'd'};
        array<char, 4> moves2 = {'l', 'r', 'u', 'd'};

        priority_queue<Move> moveQueue;

        cout << "Simulation outcomes: ";
        for (int i = 0; i < (int)moves1.size(); i++) {
            Board tempBoard1(board);
            tempBoard1.makeMove(moves1[i]);

            if (unchanged(board, tempBoard1)) {
                cout << moves1[i] << "= " << '*' << " ";
                continue;
            }

            double maxValue = -std::numeric_limits<double>::infinity();

            for (int j = 0; j < (int)moves2.size(); j++) {
                Board tempBoard2(tempBoard1);
                tempBoard2.makeMove(moves2[j]);
                if (unchanged(tempBoard1, tempBoard2))
                    continue;

                double v = (tempBoard2.getCurrentScore()-tempBoard1.getCurrentScore())+evaluate(tempBoard2);
                if (v > maxValue)
                    maxValue = v;
            }

            double value = (tempBoard1.getCurrentScore() - board.getCurrentScore()) + evaluate(tempBoard1) + 0.5*maxValue;
            moveQueue.push(Move(moves1[i], value));
            cout << moves1[i] << "= " << value << " ";
        }

        cout << endl;

        if (moveQueue.empty())
            return board.getRandomMove();
        else {
            return moveQueue.top().direction;
        }
    }

private:
    bool unchanged(const Board &a, const Board &b) const {
        int n = a.getBoardSize();
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                if (a.getGrid(i, j) != b.getGrid(i, j))
                    return false;
        return true;
    }

    // Returns the maximum tile value on the board.
    int getMaxTile(const Board &board) const {
        int n = board.getBoardSize();
        int maxTile = 0;
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                if (board.getGrid(i, j) > maxTile)
                    maxTile = board.getGrid(i, j);
        return maxTile;
    }

    double evaluate(const Board &board) const {
        // Feature 1: empty cells (more is better)
        int e = board.countEmptyCells();
        double empty = log((double)(e + 1));  // log(emptyCount+1) so log(1)=0 for full board

        // Feature 2: max tile in corner bonus
        int maxTile = getMaxTile(board); 
        double cornerBonus = isMaxInCorner(board, maxTile) ? 1.0 : 0.0;

        // Feature 3: monotonicity (higher = more monotone is better)
        double monotonicity = computeMonotonicity(board);

        // Feature 4: smoothness (negative: less negative = smoother is better)
        double smoothness = computeSmoothness(board);

        // Weights (tuned heuristically)
        const double w_empty  = 270.0;
        const double w_corner = 10.0;
        const double w_monot  = 47.0;
        const double w_smooth = 10.0;

        double score = 0.0;
        score += w_empty  * empty;
        score += w_corner * cornerBonus * (double)maxTile;
        score += w_monot  * monotonicity;
        score += w_smooth * smoothness;

        return score;
    }

    double computeMonotonicity(const Board &b) const {
        int n = b.getBoardSize();
        double total = 0.0;

        auto val = [](int x) -> double {
            return x > 0 ? std::log2((double)x) : 0.0;
        };

        for (int i = 0; i < n; ++i) {
            double inc = 0.0, dec = 0.0;
            for (int j = 0; j < n - 1; ++j) {
                double a = val(b.getGrid(i, j));
                double c = val(b.getGrid(i, j + 1));
                if (a > c)
                    inc += a - c;
                else
                    dec += c - a;
            }
            total -= std::min(inc, dec);
        }

        for (int j = 0; j < n; ++j) {
            double inc = 0.0, dec = 0.0;
            for (int i = 0; i < n - 1; ++i) {
                double a = val(b.getGrid(i, j));
                double c = val(b.getGrid(i + 1, j));
                if (a > c)
                    inc += a - c;
                else
                    dec += c - a;
            }
            total -= std::min(inc, dec);
        }

        return total;  // <= 0; less negative (closer to 0) is better
    }

    // Smoothness: negative sum of log2 differences between adjacent tiles.
    // Less negative = smoother is better.
    double computeSmoothness(const Board &b) const {
        int n = b.getBoardSize();
        double smooth = 0.0;

        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                int v = b.getGrid(i, j);
                if (v == 0)
                    continue;
                if (i + 1 < n) {
                    int v2 = b.getGrid(i + 1, j);
                    if (v2 != 0)
                        smooth += std::abs((double)std::log2(v) -
                                           (double)std::log2(v2));
                }
                if (j + 1 < n) {
                    int v2 = b.getGrid(i, j + 1);
                    if (v2 != 0)
                        smooth += std::abs((double)std::log2(v) -
                                           (double)std::log2(v2));
                }
            }
        }

        // The comment already said "return negative" but the original code returned +smooth.
        return -smooth;
    }

    bool isMaxInCorner(const Board &b, int maxTile) const {
        int n = b.getBoardSize();
        if (b.getGrid(0, 0) == maxTile)
            return true;
        if (b.getGrid(0, n - 1) == maxTile)
            return true;
        if (b.getGrid(n - 1, 0) == maxTile)
            return true;
        if (b.getGrid(n - 1, n - 1) == maxTile)
            return true;
        return false;
    }
};

#endif // SMARTPLAYER_CPP_


