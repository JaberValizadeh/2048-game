/*
 * MonteCarloPlayer.h
 *
 *  Created on: 02 Apr 2026
 *      Author: Jaber
 */

#ifndef MONTECARLOPLAYER_H_D
#define MONTECARLOPLAYER_H_D

#include <array>
#include <iostream>
#include <queue>
#include <string>

using namespace std;

#include "Board.h"
#include "Player.h"

const int SIMULATION_TIMES = 200;
const int maxDepth = 10;

class MonteCarloPlayer : public Player {
private:
	double expand(Board board);
	double simulation(const Board &board);
	bool unchanged(const Board &before, const Board &after) const {
		int n = before.getBoardSize();
		for (int i = 0; i < n; ++i)
			for (int j = 0; j < n; ++j)
				if (before.getGrid(i, j) != after.getGrid(i, j))
					return false;
		return true;
	}

public:
	MonteCarloPlayer() { name = "Monte Carlo Player"; }

	char getMove(const Board &);
};

char MonteCarloPlayer::getMove(const Board &board) {
	array<char, 4> moves = {'l', 'r', 'u', 'd'};
	priority_queue<Move> moveQueue;
	cout << "Simulation outcome: ";
	for (int i = 0; i < moves.size(); i++) {
		Board tempBoard(board);
		tempBoard.makeMove(moves[i]);
		tempBoard.addNewTile();
		if (unchanged(board, tempBoard))
			cout << moves[i] << "= " << '*' << " ";
		else {
			double value = simulation(tempBoard);
			moveQueue.push(Move(moves[i], value));
			cout << moves[i] << "= " << value << " ";
		}
	}
	cout << endl;

	if (moveQueue.empty())
		return board.getRandomMove();
	else {
		return moveQueue.top().direction;
	}
}

double MonteCarloPlayer::simulation(const Board &board) {
	double totalScore = 0.0;
	for (int s = 0; s < SIMULATION_TIMES; ++s) {
		totalScore += expand(board);
	}
	return totalScore / (double)SIMULATION_TIMES;
}

double MonteCarloPlayer::expand(Board board) {
	int depth = maxDepth;
	while (depth-- >=0 ) {
		if (board.hasWon()) {
			return (double)(board.getCurrentScore() + board.getBonus());


	//		return (double)(board.getCurrentScore() + board.getBonus()) /(double)board.getCurrentSteps();
		}

		if (!board.canMove()) {
			return board.getCurrentScore();
//			return board.getScoreperStep();
		}

		char randomMove = board.getRandomMove();
		board.makeMove(randomMove);
		board.addNewTile();
	}
	return board.getCurrentScore();
//	return board.getScoreperStep();
}

#endif /* MONTECARLOPLAYER_H_D */
