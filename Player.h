/*
 * Player.h
 *
 *  Created on: 12/01/2026
 *      Author: dongmo
 */

#ifndef PLAYER_H_
#define PLAYER_H_

#include <string>

using namespace std;

#include "Board.h"


class Move {
public:
	Move(char d, double v) :
		direction(d), value(v) {}
	char direction;
	double value;

	bool operator<(const Move &other) const { return value < other.value; }
};

class Player {
protected:
	string name;
	
public:
	Player() { name = ""; }

	string getPlayerName() { return name; }

	virtual char getMove(const Board &board) = 0;

	virtual ~Player() {}
};

#endif /* PLAYER_H_ */
