/*
 * Game.h
 *
 *  Created on: 12/01/2026
 *      Author: dongmo
 */

#ifndef GAME_H_
#define GAME_H_

#include "Board.h"
#include "Player.h"

const int NUMBEROFGAMES = 1;

class Game {
private:
	Board *board;
	Player *player;
	string moveStr(char move) {
		string res = "Unknown";
		if (move == 'l')
			res = "Left";
		if (move == 'r')
			res = "Right";
		if (move == 'u')
			res = "Up";
		if (move == 'd')
			res = "Down";
		return res;
	}

public:
	Game(Board *b, Player *p) {
		board = b;
		player = p;
	}
	
	virtual ~Game() {
		delete board;
		delete player;
	}

	void play();
};

void Game::play() {
	int won = 0;
	double averageScore = 0.0;
	board->printBoard();
	int games = NUMBEROFGAMES;
	while (!won && games > 0) {
		if (board->hasWon()) {
			cout << "Congratulations! You have reached the target number at step "
				 << board->getCurrentSteps() << "." << endl;

			cout << "You current score is " << board->getCurrentScore()
				 << " ." << endl;

			int totalScore = board->getCurrentScore() + board->getBonus();
			double scorePerStep =
				(double)totalScore / (double)board->getCurrentSteps();

			cout << "You final score per step is " << scorePerStep << endl;

			averageScore += scorePerStep;
			board = new Board(board->getBoardSize());
			games--;
			continue;
		}

		if (!board->canMove()) {
			cout << "Game Over at step " << board->getCurrentSteps()
				 << ". No more moves available.\n";
			cout << "Final Score per step: " << board->getScoreperStep() << ". "
				 << endl;

			averageScore += board->getScoreperStep();
			board = new Board(board->getBoardSize());
			games--;
			continue;
		}

		cout <<"\nGame " << (NUMBEROFGAMES - games + 1) << " Step " << board->getCurrentSteps() + 1 << " ..." << endl;
		 
		char move = player->getMove(*board);
		cout << player->getPlayerName() << " plays: " << moveStr(move)
			 << " at step " << board->getCurrentSteps() << "." << endl;

		board->makeMove(move);
		board->addNewTile();
		board->printBoard();
		Tile newTitle = board->getNewTile();
		cout << "Added tile " << newTitle.value << " at position ("
			 << newTitle.x + 1 << ", " << newTitle.y + 1 << ")" << "." << endl;
		cout << "Empty cells remaining: " << board->countEmptyCells() << endl;
	}

	averageScore /= (double)(NUMBEROFGAMES);
	cout << "Average score per step over " << NUMBEROFGAMES
		 << " games: " << averageScore << endl;
}

#endif /* GAME_H_ */
