/*
 * Board.h
 *
 *  Created on: 12/01/2026
 *      Author: dongmo
 */

#ifndef BOARD_H_
#define BOARD_H_

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <list>
#include <vector>

using namespace std;

struct Tile {
	int x, y, value;
	Tile(int x=-1, int y=-1, int value=-1) :
		x(x), y(y), value(value) {}
};

class Board {
private:
	int boardSize;
	int **grid;
	int currentScore;
	int currentSteps;
	Tile newTile;
	int target = 2048;
	int bonus = 20000;

public:
	Board(int bs, bool quiet = false) {
		boardSize = bs;
		grid = new int *[boardSize];
		for (int i = 0; i < boardSize; i++)
			grid[i] = new int[boardSize];

		for (int i = 0; i < boardSize; i++)
			for (int j = 0; j < boardSize; j++) {
				grid[i][j] = 0;
			}

		currentScore = 0;
		currentSteps = 0;

		target = 2048 * ((int)((boardSize - 3) / 3) + 1);
		bonus = 10000 * ((int)(3 - (boardSize - 3) % 3));

		if (!quiet) {
			cout << "Based on the board size, the target number is " << target
				 << " and the bonus score if winning is " << bonus << ".\n"
				 << endl;
		}

		addNewTile();
		addNewTile();
	}

	virtual ~Board() {
		for (int i = 0; i < boardSize; i++)
			delete[] grid[i];

		delete[] grid;
	}

	Board(const Board &cboard) {
		boardSize = cboard.getBoardSize();

		grid = new int *[boardSize];
		for (int i = 0; i < boardSize; i++)
			grid[i] = new int[boardSize];

		for (int i = 0; i < boardSize; i++)
			for (int j = 0; j < boardSize; j++) {
				grid[i][j] = cboard.getGrid(i, j);
			}
		currentScore = cboard.currentScore;
		currentSteps = cboard.currentSteps;
		target = cboard.target;
		bonus = cboard.bonus;
	}

	int getBoardSize() const { return boardSize; }
	int getGrid(int x, int y) const { return grid[x][y]; }
	void setGrid(int x, int y, int val) { grid[x][y] = val; }
	int getTarget() const { return target; }
	int getBonus() const { return bonus; }
	int getCurrentSteps() const { return currentSteps; }
	int getCurrentScore() const { return currentScore; }
	double getScoreperStep() const {
		return currentSteps > 0 ? ((double)currentScore / (double)currentSteps)
								: 0;
	}
	Tile getNewTile() const { return newTile; }

	void addNewTile() {
		vector<int> empty;
		for (int i = 0; i < boardSize; i++) {
			for (int j = 0; j < boardSize; j++) {
				if (grid[i][j] == 0) {
					empty.push_back(i * boardSize + j);
				}
			}
		}

		if (!empty.empty()) {
			int idx = rand() % empty.size();
			newTile.x = empty[idx] / boardSize;
			newTile.y = empty[idx] % boardSize;
			newTile.value =
				(rand() % 10 < 9) ? 2 : 4; // 90% chance of 2, 10% chance of 4
			grid[newTile.x][newTile.y] = newTile.value;
		}
	}

	bool hasWon() const {
		for (int i = 0; i < boardSize; i++) {
			for (int j = 0; j < boardSize; j++) {
				if (grid[i][j] >= target)
					return true;
			}
		}
		return false;
	}

	void printBoard() const {
		cout << "     ";
		for (int j = 0; j < boardSize; j++) {
			cout << j + 1 << "     ";
		}
		cout << endl;

		cout << "   ";
		for (int j = 0; j < boardSize; j++) {
			cout << "------";
		}
		cout << endl;

		for (int i = 0; i < boardSize; i++) {
			cout << setw(2) << i + 1 << " ";

			for (int j = 0; j < boardSize; j++) {
				if (grid[i][j] == 0) {
					if (j == 0) {
						cout << "|     |";
					} else {
						cout << "     |";
					}
				} else {
					if (j == 0) {
						cout << "|" << setw(5) << grid[i][j] << "|";
					} else {
						cout << setw(5) << grid[i][j] << "|";
						;
					}
				}
			}
			cout << endl;

			cout << "   ";
			for (int j = 0; j < boardSize; j++) {
				cout << "------";
			}
			cout << endl;
		}
		cout << "Current score: " << currentScore
			 << " Total steps: " << currentSteps << "." << endl;
		cout << "Current score per step: " << getScoreperStep() << "." << endl;
	}

	// Implement the following functions to complete the functionality of the Board class as part of practical tasks. 
	// You can refer to the provided solution code for guidance.
	
	void makeMove(char direction);
	vector<int> merge(list<int> row);
	int countEmptyCells() const;
	bool canMove() const;
	double heuristic(const Board& after) const;

	char getRandomMove() const {
		char moves[] = {'l', 'r', 'u', 'd'};
		return moves[rand() % 4];
	}

	// Reset the board to a fresh game state (used by training loops)
	void init() {
		for (int i = 0; i < boardSize; i++)
			for (int j = 0; j < boardSize; j++)
				grid[i][j] = 0;
		currentScore = 0;
		currentSteps = 0;
		addNewTile();
		addNewTile();
	}

	// Alias kept for callers that use the addRandomTile() name
	void addRandomTile() { addNewTile(); }

	// Return the highest tile value currently on the board
	int maxTile() const {
		int m = 0;
		for (int i = 0; i < boardSize; i++)
			for (int j = 0; j < boardSize; j++)
				if (grid[i][j] > m) m = grid[i][j];
		return m;
	}

	// Direction-integer overload: 0=left, 1=right, 2=up, 3=down.
	// Returns true if the board changed, false if the move was invalid.
	bool makeMove(int dir) {
		// Snapshot current grid so we can detect no-ops
		vector<int> snap(boardSize * boardSize);
		for (int i = 0; i < boardSize; i++)
			for (int j = 0; j < boardSize; j++)
				snap[i * boardSize + j] = grid[i][j];
		int snapScore  = currentScore;
		int snapSteps  = currentSteps;

		char dirs[] = {'l', 'r', 'u', 'd'};
		makeMove(dirs[dir]);

		for (int i = 0; i < boardSize; i++)
			for (int j = 0; j < boardSize; j++)
				if (grid[i][j] != snap[i * boardSize + j]) return true;

		// Move had no effect — restore score/step counters
		currentScore = snapScore;
		currentSteps = snapSteps;
		return false;
	}
};

  // Task1_3: Implement the countEmptyCells() function to count the number of empty cells (cells with value 0) on the board.
int Board::countEmptyCells() const {
	int count = 0;
	for (int i = 0; i < boardSize; i++)
		for (int j = 0; j < boardSize; j++)
			if (grid[i][j] == 0) count++;
	return count;
}

  // Task2_2: Implement the canMove() function to check if there are any valid moves left on the board. 
  // A move is valid if there is at least one empty cell or if there are adjacent cells with the same value that can be merged.
bool Board::canMove() const {
	if (countEmptyCells() > 0) return true;
	for (int i = 0; i < boardSize; i++) {
		for (int j = 0; j < boardSize; j++) {
			if (j + 1 < boardSize && grid[i][j] == grid[i][j + 1]) return true;
			if (i + 1 < boardSize && grid[i][j] == grid[i + 1][j]) return true;
		}
	}
	return false;
}

  // Task3_1: Implement the merge() function to merge a list of integers representing a row or column of the board according to the rules of the 2048 game. 
  // The function should return a vector of integers representing the merged row or column.
vector<int> Board::merge(list<int> row) {
	vector<int> mergedRow;
	while (!row.empty()) {
		int front = row.front();
		row.pop_front();
		if (!row.empty() && row.front() == front) {
			mergedRow.push_back(2 * front);
			currentScore += 2 * front;
			row.pop_front();
		} else {
			mergedRow.push_back(front);
		}
	}
	while ((int)mergedRow.size() < boardSize)
		mergedRow.push_back(0);
	return mergedRow;
}

void Board::makeMove(char direction) {

	for (int i = 0; i < boardSize; i++) {
		list<int> row;
		for (int j = 0; j < boardSize; j++) {
			if (direction == 'l' && grid[i][j] != 0)
				row.push_back(grid[i][j]);

			if (direction == 'r' && grid[i][j] != 0)
				row.push_front(grid[i][j]);

			if (direction == 'u' && grid[j][i] != 0)
				row.push_back(grid[j][i]);

			if (direction == 'd' && grid[j][i] != 0)
				row.push_front(grid[j][i]);
		}

		if (!row.empty()) {
			vector<int> merged = merge(row);
			for (int j = 0; j < boardSize; j++) {
				if (direction == 'l')
					grid[i][j] = merged[j];

				if (direction == 'r')
					grid[i][boardSize - j - 1] = merged[j];

				if (direction == 'u')
					grid[j][i] = merged[j];

				if (direction == 'd')
					grid[boardSize - j - 1][i] = merged[j];
			}
		}
	}

	currentSteps++;
}

#endif /* BOARD_H_ */