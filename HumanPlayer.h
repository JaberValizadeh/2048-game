/*
 * HumanPlayer.h
 *
 *  Created on: 12/01/2026
 *      Author: dongmo
 */

#ifndef HUMANPLAYER_H_
#define HUMANPLAYER_H_

using namespace std;

#include "Board.h"
#include "Player.h"

class HumanPlayer: public Player {
public:
	HumanPlayer() {
		name = "Human Player";
	}
	char getMove(const Board&);
};

char HumanPlayer::getMove(const Board& board) {
	char move = '*';
	cout << "Enter move (l= Left, r= Right, u= Up, d= Down): ";
	while (true) {
		cin >> move;
		move = tolower(move);
		switch (move) {
		case 'l':
		case 'r':
		case 'u':
		case 'd':
			return move;
		case 'q':
			cout << "Thanks for playing! Final score: " << board.getScoreperStep()
					<< ".\n";
			return move;
		default:
			cout << "Invalid move! Use L/R/U/D or Q to quit.\n";
			continue;
		}
	}
	return move;
}

#endif /* HUMANPLAYER_H_ */
