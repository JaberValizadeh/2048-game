/*
 * BestFirstPlayer.h
 *
 *  Created on: 27 Mar. 2026
 *      Author: jaber
 */

#ifndef BESTFIRSTPLAYER_H_
#define BESTFIRSTPLAYER_H_

#include <queue>
#include "Board.h"
#include "Player.h"

using namespace std;

class BestFirstPlayer : public Player {
public:
	BestFirstPlayer() {
		name = "Best First Player";
	}

	char getMove(const Board& board) {
		if (!board.canMove())
			return 'q';

		array<char, 4> moves = {'l', 'r', 'u', 'd'};
	    priority_queue<Move> moveQueue;

	    for (int i = 0; i < moves.size(); i++) {
			Board tempBoard(board);
			tempBoard.makeMove(moves[i]);

			bool changed = false;
			int bs = board.getBoardSize();
			for (int i = 0; i < bs && !changed; i++)
				for (int j = 0; j < bs && !changed; j++)
					if (board.getGrid(i, j) != tempBoard.getGrid(i, j))
						changed = true;

			if (changed) {
				double value = board.getCurrentScore() + 5*tempBoard.countEmptyCells(); // heuristic: current score + 10 * number of empty cells
				moveQueue.push(Move(moves[i], value));
				cout << moves[i] << "= " << value << " ";
			}
		}
		cout << endl;

		if (moveQueue.empty()) {
			return board.getRandomMove();
		} else {
			return moveQueue.top().direction;
		}
	}
};

#endif /* BESTFIRSTPLAYER_H_ */
