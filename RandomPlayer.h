/*
 * RandomPlayer.h
 *
 *  Created on: 18/03/2026
 *      Author: jaber
 */

#ifndef RANDOMPLAYER_H_
#define RANDOMPLAYER_H_

#include "Board.h"
#include "Player.h"

class RandomPlayer : public Player {
public:
	RandomPlayer() {
		name = "Random Player";
	}
	char getMove(const Board& board);
};

char RandomPlayer::getMove(const Board& board) {
	const char moves[] = {'l', 'r', 'u', 'd'};
	return moves[rand() % 4];
}

#endif /* RANDOMPLAYER_H_ */
