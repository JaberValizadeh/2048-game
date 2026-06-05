/*
 * Main.cpp
 *
 *  Created on: 12/01/2026
 *      Author: dongmo
 */

#include <ctime>
#include <iostream>

using namespace std;

#include "Board.h"
#include "BestFirstPlayer.h"
#include "Game.h"
#include "HumanPlayer.h"
#include "MonteCarloPlayer.h"
#include "ImprovedMonteCarloPlayer.h"
#include "SmartPlayer.h"
#include "MarkovPlayer.h"
#include "RLPlayer.h"
#include "GeneticPlayer.h"
#include "Player.h"
#include "RandomPlayer.h"

int main() {
	srand(time(0));
	int boardSize = 4;
	cout << "Input the size of board (>= 3):" << endl;
	cin >> boardSize;
	if (boardSize < 3)
		boardSize = 3;
	else if (boardSize > 15)
		boardSize = 15;

	Board *board = new Board(boardSize);

	cout << "Empty cells after initialization: " << board->countEmptyCells() << endl;

	cout << "Choose a player to play: " << endl;
	cout << "1. Human Player" << endl;
	cout << "2. Random Player" << endl;
	cout << "3. Heuristic Player" << endl;
	cout << "4. Monte Carlo Player" << endl;
	cout << "5. Markov Player" << endl;
	cout << "6. Smart Player" << endl;
	cout << "7. RL Player (N-Tuple TD Learning)" << endl;
	cout << "8. Improved Monte Carlo Player (Zobrist Hash Cache)" << endl;
	cout << "9. Genetic Player (GA-evolved heuristic weights)" << endl;
	cout << "Others: Quit" << endl;

	int choice;
	cin >> choice;

	Player *p;

	if (choice == 1) {
		p = new HumanPlayer;
	} else if (choice == 2) {
		p = new RandomPlayer;
	} else if (choice == 3) {
		p = new BestFirstPlayer;
	} else if (choice == 4) {
		p = new MonteCarloPlayer;
	} else if (choice == 5) {
		p = new MarkovPlayer;
	} else if (choice == 6) {
		p = new SmartPlayer;
	} else if (choice == 7) {
		p = new RLPlayer;
	} else if (choice == 8) {
		p = new ImprovedMonteCarloPlayer;
	} else if (choice == 9) {
		p = new GeneticPlayer;
	} else {
		return 0;
	}

	Game game(board, p);
	game.play();

	// Note: Game::~Game() already deletes board and player

	//	system("pause");
	return 0;
}
