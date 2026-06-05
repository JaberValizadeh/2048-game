/*
 * QLearningPlayer.h
 *
 * Q-Learning agent for the 2048 game.
 *
 * Because the full board state space is enormous, this player uses a
 * compact feature-based state encoding that maps the board to a small
 * discrete state ID.  A standard tabular Q-Learning update rule is
 * applied online: every time getMove() is called the agent receives the
 * reward earned since its last move and updates Q(prevState, prevAction).
 *
 * State features (encoded into a single integer):
 *   f1 – log2 of the maximum tile on the board (0–11, capped at 2048)
 *   f2 – bucket for the number of empty cells  (0–3)
 *   f3 – bucket for board monotonicity          (0–2)
 *   f4 – bucket for the current score           (0–4)
 *   Total unique states: 12 × 4 × 3 × 5 = 720
 *
 * Hyper-parameters (can be tuned in the constructor):
 *   alpha        = 0.15   (learning rate)
 *   gamma        = 0.90   (discount factor)
 *   epsilon      = 0.50   (initial exploration rate)
 *   epsilonDecay = 0.9995 (multiplicative decay per step)
 *   minEpsilon   = 0.05   (floor for epsilon)
 */

#ifndef QLEARNINGPLAYER_H_
#define QLEARNINGPLAYER_H_

#include <cmath>
#include <map>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace std;

#include "Board.h"
#include "Player.h"

class QLearningPlayer : public Player {
private:
    // Q-table: maps (stateCode, actionIndex) -> Q-value
    map<pair<int, int>, double> qTable;

    // Hyper-parameters
    double alpha;        // learning rate
    double gamma;        // discount factor
    double epsilon;      // current exploration probability
    double epsilonDecay; // per-step multiplicative decay
    double minEpsilon;   // minimum exploration probability

    // Memory of the previous decision (needed for the TD update)
    int    prevStateCode;
    int    prevAction;
    double prevScore;
    bool   firstMove;

    // Pseudo-random number generation
    mt19937 rng;
    uniform_real_distribution<double> realDist;
    uniform_int_distribution<int>     actionDist;

    // The four possible moves, indexed 0-3
    const char ACTIONS[4] = {'l', 'r', 'u', 'd'};

    // ----------------------------------------------------------------
    // Helper: count empty (zero) cells without relying on the stub
    // ----------------------------------------------------------------
    int countEmpty(const Board &board) const {
        int n = board.getBoardSize();
        int count = 0;
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (board.getGrid(i, j) == 0)
                    count++;
        return count;
    }

    // ----------------------------------------------------------------
    // Helper: log2 level of the largest tile (capped at 11 == 2048)
    // ----------------------------------------------------------------
    int maxTileLevel(const Board &board) const {
        int n = board.getBoardSize();
        int maxVal = 1;
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (board.getGrid(i, j) > maxVal)
                    maxVal = board.getGrid(i, j);
        int level = 0;
        int v = maxVal;
        while (v > 1) { v >>= 1; level++; }
        return min(level, 11);
    }

    // ----------------------------------------------------------------
    // Helper: monotonicity ratio in [0, 1].
    // Counts the fraction of adjacent non-zero pairs that are in
    // non-increasing order (row-wise and column-wise).
    // ----------------------------------------------------------------
    double computeMonotonicity(const Board &board) const {
        int n = board.getBoardSize();
        double mono  = 0.0;
        double total = 0.0;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n - 1; j++) {
                // Row pairs
                int a = board.getGrid(i, j);
                int b = board.getGrid(i, j + 1);
                if (a > 0 && b > 0) {
                    if (a >= b) mono++;
                    total++;
                }
                // Column pairs
                int c = board.getGrid(j,     i);
                int d = board.getGrid(j + 1, i);
                if (c > 0 && d > 0) {
                    if (c >= d) mono++;
                    total++;
                }
            }
        }
        return (total > 0) ? mono / total : 0.5;
    }

    // ----------------------------------------------------------------
    // Encode the board into a single compact state integer.
    // Layout:  stateCode = f1 + 12*(f2 + 4*(f3 + 3*f4))
    // ----------------------------------------------------------------
    int encodeState(const Board &board) const {
        // f1: max-tile level, 0-11
        int f1 = maxTileLevel(board);

        // f2: empty-cell bucket, 0-3
        int empty = countEmpty(board);
        int f2 = (empty <= 2) ? 0 :
                 (empty <= 5) ? 1 :
                 (empty <= 9) ? 2 : 3;

        // f3: monotonicity bucket, 0-2
        double mono = computeMonotonicity(board);
        int f3 = (mono < 0.40) ? 0 :
                 (mono < 0.70) ? 1 : 2;

        // f4: score bucket (log scale), 0-4
        int score = board.getCurrentScore();
        int f4 = (score <   100) ? 0 :
                 (score <   500) ? 1 :
                 (score <  2000) ? 2 :
                 (score <  8000) ? 3 : 4;

        return f1 + 12 * (f2 + 4 * (f3 + 3 * f4));
    }

    // ----------------------------------------------------------------
    // Q-table accessors
    // ----------------------------------------------------------------
    double getQ(int stateCode, int action) const {
        auto it = qTable.find({stateCode, action});
        return (it != qTable.end()) ? it->second : 0.0;
    }

    void setQ(int stateCode, int action, double value) {
        qTable[{stateCode, action}] = value;
    }

    double maxQ(int stateCode) const {
        double best = getQ(stateCode, 0);
        for (int a = 1; a < 4; a++)
            best = max(best, getQ(stateCode, a));
        return best;
    }

    // ----------------------------------------------------------------
    // Check whether a move actually changes the board (validity test)
    // ----------------------------------------------------------------
    bool isMoveValid(const Board &board, char move) const {
        Board copy(board);
        copy.makeMove(move);
        int n = board.getBoardSize();
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (board.getGrid(i, j) != copy.getGrid(i, j))
                    return true;
        return false;
    }

    // ----------------------------------------------------------------
    // Epsilon-greedy action selection
    // ----------------------------------------------------------------
    int chooseAction(const Board &board, int stateCode) {
        // Collect valid actions
        vector<int> valid;
        for (int a = 0; a < 4; a++)
            if (isMoveValid(board, ACTIONS[a]))
                valid.push_back(a);

        // If nothing is valid, pick randomly (game should be over anyway)
        if (valid.empty())
            return actionDist(rng);

        // Explore: uniform random among valid actions
        if (realDist(rng) < epsilon) {
            uniform_int_distribution<int> d(0, (int)valid.size() - 1);
            return valid[d(rng)];
        }

        // Exploit: highest Q among valid actions
        int    bestAction = valid[0];
        double bestVal    = getQ(stateCode, valid[0]);
        for (int i = 1; i < (int)valid.size(); i++) {
            double q = getQ(stateCode, valid[i]);
            if (q > bestVal) {
                bestVal    = q;
                bestAction = valid[i];
            }
        }
        return bestAction;
    }

public:
    QLearningPlayer()
        : alpha(0.15), gamma(0.90), epsilon(0.50),
          epsilonDecay(0.9995), minEpsilon(0.05),
          prevStateCode(-1), prevAction(-1), prevScore(0.0), firstMove(true),
          rng(42), realDist(0.0, 1.0), actionDist(0, 3) {
        name = "Q-Learning Player";
    }

    // ----------------------------------------------------------------
    // Main decision method called by the game loop each turn.
    //
    // Flow:
    //   1. Encode the current board state.
    //   2. Compute the reward earned since the last move.
    //   3. Apply the Q-Learning update: Q(s,a) += α[r + γ·max Q(s') - Q(s,a)]
    //   4. Choose the next action using epsilon-greedy.
    //   5. Save current state/action/score for the next call.
    //   6. Decay epsilon.
    // ----------------------------------------------------------------
    char getMove(const Board &board) override {
        int    currentState = encodeState(board);
        double currentScore = (double)board.getCurrentScore();

        // ---- Q-Learning update (skipped on the very first move) ----
        if (!firstMove && prevAction >= 0) {
            double reward   = currentScore - prevScore;
            double oldQ     = getQ(prevStateCode, prevAction);
            double maxNextQ = maxQ(currentState);
            double newQ     = oldQ + alpha * (reward + gamma * maxNextQ - oldQ);
            setQ(prevStateCode, prevAction, newQ);
        }
        firstMove = false;

        // ---- Action selection ----
        int action = chooseAction(board, currentState);

        // ---- Store decision for next update ----
        prevStateCode = currentState;
        prevAction    = action;
        prevScore     = currentScore;

        // ---- Decay exploration rate ----
        epsilon = max(minEpsilon, epsilon * epsilonDecay);

        return ACTIONS[action];
    }
};

#endif /* QLEARNINGPLAYER_H_ */