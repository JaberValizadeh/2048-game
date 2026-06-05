/*
 * GeneticPlayer.h
 *
 *  Created on: 18/04/2026
 *      Author: jaber
 *
 * Genetic Algorithm player for 2048.
 *
 * Approach:
 *   The player plays with a 1-ply greedy lookahead that scores each
 *   reachable afterstate with a linear combination of heuristic features:
 *       score(s) = w0 * log(emptyCount + 1)
 *                + w1 * cornerBonus(s)         (log2 max tile if in a corner)
 *                + w2 * monotonicity(s)
 *                + w3 * smoothness(s)
 *                + w4 * log2 maxTile(s)
 *
 *   The weight vector w = (w0..w4) is evolved by a genetic algorithm:
 *     - Population of candidate weight vectors
 *     - Fitness  = average final score over a few self-play games
 *     - Selection = tournament, Crossover = uniform, Mutation = Gaussian
 *     - Elitism preserves the top individuals each generation
 *
 *   After training the best weights are saved to "ga_weights.bin" so
 *   subsequent runs skip training.
 */

#ifndef GENETICPLAYER_H_
#define GENETICPLAYER_H_

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

#include "Board.h"
#include "Player.h"

using namespace std;

class GeneticPlayer : public Player {
private:
    static const int NUM_WEIGHTS = 5;

    vector<double> weights;
    mt19937 rng;

    struct Individual {
        vector<double> genes;
        double fitness = 0.0;
        bool operator<(const Individual &o) const { return fitness < o.fitness; }
    };

    // ---------- feature extraction ----------
    double featureEmpty(const Board &b) const {
        return log((double)(b.countEmptyCells() + 1));
    }

    double featureCorner(const Board &b) const {
        int n = b.getBoardSize();
        int maxT = b.maxTile();
        if (maxT <= 0) return 0.0;
        bool inCorner = (b.getGrid(0, 0) == maxT)
                     || (b.getGrid(0, n - 1) == maxT)
                     || (b.getGrid(n - 1, 0) == maxT)
                     || (b.getGrid(n - 1, n - 1) == maxT);
        return inCorner ? log2((double)maxT) : 0.0;
    }

    double featureMonotonicity(const Board &b) const {
        int n = b.getBoardSize();
        auto v = [](int x) { return x > 0 ? log2((double)x) : 0.0; };
        double total = 0.0;
        for (int i = 0; i < n; i++) {
            double inc = 0, dec = 0;
            for (int j = 0; j + 1 < n; j++) {
                double a = v(b.getGrid(i, j));
                double c = v(b.getGrid(i, j + 1));
                if (a > c) inc += a - c; else dec += c - a;
            }
            total -= min(inc, dec);
        }
        for (int j = 0; j < n; j++) {
            double inc = 0, dec = 0;
            for (int i = 0; i + 1 < n; i++) {
                double a = v(b.getGrid(i, j));
                double c = v(b.getGrid(i + 1, j));
                if (a > c) inc += a - c; else dec += c - a;
            }
            total -= min(inc, dec);
        }
        return total;
    }

    double featureSmoothness(const Board &b) const {
        int n = b.getBoardSize();
        double s = 0.0;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                int vij = b.getGrid(i, j);
                if (vij == 0) continue;
                if (i + 1 < n) {
                    int vv = b.getGrid(i + 1, j);
                    if (vv != 0) s += fabs(log2((double)vij) - log2((double)vv));
                }
                if (j + 1 < n) {
                    int vv = b.getGrid(i, j + 1);
                    if (vv != 0) s += fabs(log2((double)vij) - log2((double)vv));
                }
            }
        }
        return -s;
    }

    double featureMaxTile(const Board &b) const {
        int mt = b.maxTile();
        return mt > 0 ? log2((double)mt) : 0.0;
    }

    double evalWithWeights(const Board &b, const vector<double> &w) const {
        return w[0] * featureEmpty(b)
             + w[1] * featureCorner(b)
             + w[2] * featureMonotonicity(b)
             + w[3] * featureSmoothness(b)
             + w[4] * featureMaxTile(b);
    }

    // Pick best 1-ply move given weight vector; returns direction 0..3 or -1 if no move.
    int bestMove(const Board &b, const vector<double> &w) const {
        int best = -1;
        double bestV = -1e18;
        for (int d = 0; d < 4; d++) {
            Board tmp(b);
            if (!tmp.makeMove(d)) continue;
            double r = tmp.getCurrentScore() - b.getCurrentScore();
            double v = r + evalWithWeights(tmp, w);
            if (v > bestV) { bestV = v; best = d; }
        }
        return best;
    }

    // Play one self-play game using the supplied weights; returns final score.
    double playGame(const vector<double> &w, int boardSize) const {
        Board b(boardSize, true);
        int safety = 100000;
        while (b.canMove() && safety-- > 0) {
            int d = bestMove(b, w);
            if (d < 0) break;
            b.makeMove(d);
            b.addRandomTile();
        }
        return (double)b.getCurrentScore();
    }

    double computeFitness(const vector<double> &w, int games, int boardSize) const {
        double total = 0.0;
        for (int g = 0; g < games; g++) total += playGame(w, boardSize);
        return total / games;
    }

    // ---------- GA operators ----------
    Individual randomIndividual() {
        // Initialise each gene with a magnitude roughly matching the
        // heuristic scales used by SmartPlayer; this gives the GA a
        // sensible starting region rather than tiny random values.
        uniform_real_distribution<double> u(-1.0, 1.0);
        Individual ind;
        ind.genes.resize(NUM_WEIGHTS);
        const double scale[NUM_WEIGHTS] = {300.0, 20.0, 50.0, 20.0, 50.0};
        for (int i = 0; i < NUM_WEIGHTS; i++)
            ind.genes[i] = u(rng) * scale[i];
        return ind;
    }

    Individual tournamentSelect(const vector<Individual> &pop, int tSize) {
        uniform_int_distribution<int> pick(0, (int)pop.size() - 1);
        Individual best = pop[pick(rng)];
        for (int i = 1; i < tSize; i++) {
            const Individual &c = pop[pick(rng)];
            if (c.fitness > best.fitness) best = c;
        }
        return best;
    }

    Individual crossover(const Individual &a, const Individual &b) {
        uniform_real_distribution<double> u(0.0, 1.0);
        Individual child;
        child.genes.resize(NUM_WEIGHTS);
        for (int i = 0; i < NUM_WEIGHTS; i++)
            child.genes[i] = (u(rng) < 0.5) ? a.genes[i] : b.genes[i];
        return child;
    }

    void mutate(Individual &ind, double rate, double sigma) {
        uniform_real_distribution<double> u(0.0, 1.0);
        normal_distribution<double> gauss(0.0, sigma);
        const double scale[NUM_WEIGHTS] = {50.0, 5.0, 10.0, 5.0, 10.0};
        for (int i = 0; i < NUM_WEIGHTS; i++)
            if (u(rng) < rate)
                ind.genes[i] += gauss(rng) * scale[i] / 10.0;
    }

    void runGA(int populationSize, int generations, int gamesPerEval,
               double mutationRate, double mutationSigma, int eliteCount,
               int boardSize) {
        vector<Individual> population;
        population.reserve(populationSize);
        for (int i = 0; i < populationSize; i++)
            population.push_back(randomIndividual());

        for (int i = 0; i < populationSize; i++)
            population[i].fitness =
                computeFitness(population[i].genes, gamesPerEval, boardSize);

        for (int gen = 0; gen < generations; gen++) {
            sort(population.begin(), population.end(),
                 [](const Individual &a, const Individual &b) {
                     return a.fitness > b.fitness;
                 });

            cout << "Generation " << (gen + 1) << "/" << generations
                 << "  best=" << population[0].fitness
                 << "  median=" << population[populationSize / 2].fitness
                 << "  worst=" << population.back().fitness << endl;

            vector<Individual> next;
            next.reserve(populationSize);

            for (int i = 0; i < eliteCount && i < populationSize; i++)
                next.push_back(population[i]);

            while ((int)next.size() < populationSize) {
                Individual p1 = tournamentSelect(population, 3);
                Individual p2 = tournamentSelect(population, 3);
                Individual child = crossover(p1, p2);
                mutate(child, mutationRate, mutationSigma);
                child.fitness = computeFitness(child.genes, gamesPerEval, boardSize);
                next.push_back(child);
            }

            population = std::move(next);
        }

        sort(population.begin(), population.end(),
             [](const Individual &a, const Individual &b) {
                 return a.fitness > b.fitness;
             });
        weights = population[0].genes;

        cout << "Training complete. Best fitness: " << population[0].fitness << endl;
        cout << "Weights: ";
        for (double w : weights) cout << w << " ";
        cout << endl;
    }

    // ---------- persistence ----------
    bool saveWeights(const string &fname) const {
        ofstream f(fname, ios::binary);
        if (!f) return false;
        int n = NUM_WEIGHTS;
        f.write(reinterpret_cast<const char *>(&n), sizeof(n));
        f.write(reinterpret_cast<const char *>(weights.data()),
                (long long)n * sizeof(double));
        return f.good();
    }

    bool loadWeights(const string &fname) {
        ifstream f(fname, ios::binary);
        if (!f) return false;
        int n;
        if (!f.read(reinterpret_cast<char *>(&n), sizeof(n))) return false;
        if (n != NUM_WEIGHTS) return false;
        weights.assign(NUM_WEIGHTS, 0.0);
        if (!f.read(reinterpret_cast<char *>(weights.data()),
                    (long long)n * sizeof(double)))
            return false;
        return true;
    }

public:
    GeneticPlayer(int populationSize = 20, int generations = 15,
                  int gamesPerEval = 2, double mutationRate = 0.2,
                  double mutationSigma = 5.0, int eliteCount = 2,
                  int trainBoardSize = 4)
        : rng((unsigned)time(nullptr) ^ 0x9e3779b9u) {
        name = "Genetic Player";
        weights.assign(NUM_WEIGHTS, 0.0);

        if (loadWeights("ga_weights.bin")) {
            cout << "Loaded GA weights from ga_weights.bin: ";
            for (double w : weights) cout << w << " ";
            cout << endl;
        } else {
            cout << "Training Genetic Player"
                 << " (population=" << populationSize
                 << ", generations=" << generations
                 << ", games/eval=" << gamesPerEval << ")..." << endl;
            runGA(populationSize, generations, gamesPerEval, mutationRate,
                  mutationSigma, eliteCount, trainBoardSize);
            if (saveWeights("ga_weights.bin"))
                cout << "GA weights saved to ga_weights.bin" << endl;
            else
                cout << "Warning: could not save GA weights." << endl;
        }
    }

    char getMove(const Board &board) override {
        const char dirs[] = {'l', 'r', 'u', 'd'};
        int bestDir = -1;
        double bestVal = -1e18;

        cout << "GA evaluation: ";
        for (int d = 0; d < 4; d++) {
            Board tmp(board);
            if (!tmp.makeMove(d)) { cout << dirs[d] << "=* "; continue; }
            double r = tmp.getCurrentScore() - board.getCurrentScore();
            double v = r + evalWithWeights(tmp, weights);
            cout << dirs[d] << "=" << v << " ";
            if (v > bestVal) { bestVal = v; bestDir = d; }
        }
        cout << endl;

        if (bestDir < 0) return board.getRandomMove();
        return dirs[bestDir];
    }
};

#endif /* GENETICPLAYER_H_ */
