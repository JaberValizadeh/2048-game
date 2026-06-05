// Move oracle: a persistent subprocess that wraps the C++ player classes
// from this project so the Streamlit UI can call into the real player code
// instead of a Python reimplementation.
//
// Protocol (line-based, plain text):
//   INIT <player_id> <board_size>
//       -> "OK" on success, "ERR <msg>" on failure. Constructs the chosen
//          Player; for RL/Genetic this may load weight files (slow first time).
//   MOVE <board_size> <cell_0> <cell_1> ... <cell_{n*n-1}>
//       -> "OK <dir>" where <dir> is one of l|r|u|d|q. Reads the board state
//          row-major and asks the current player for its move.
//   QUIT  -> exits.
//
// Notes:
//   * All player "cout" debug noise is redirected to stderr so stdout carries
//     only protocol responses.
//   * Score/steps are intentionally not tracked here. Player heuristics use
//     deltas in score across the four candidate moves, which cancel out when
//     the baseline score is zero — so the relative move choice is preserved.

#include <ctime>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

#include "Board.h"
#include "BestFirstPlayer.h"
#include "GeneticPlayer.h"
#include "ImprovedMonteCarloPlayer.h"
#include "MarkovPlayer.h"
#include "MonteCarloPlayer.h"
#include "Player.h"
#include "RLPlayer.h"
#include "RandomPlayer.h"
#include "SmartPlayer.h"

static Player *makePlayer(int id) {
    switch (id) {
        case 2: return new RandomPlayer();
        case 3: return new BestFirstPlayer();
        case 4: return new MonteCarloPlayer();
        case 5: return new MarkovPlayer();
        case 6: return new SmartPlayer();
        case 7: return new RLPlayer();
        case 8: return new ImprovedMonteCarloPlayer();
        case 9: return new GeneticPlayer();
        default: return nullptr;
    }
}

int main() {
    srand((unsigned)time(0));

    // Re-route cout to cerr so player debug prints don't pollute the protocol.
    streambuf *origCout = cout.rdbuf();
    cout.rdbuf(cerr.rdbuf());

    Player *player = nullptr;
    string line;

    auto reply = [&](const string &s) {
        cout.rdbuf(origCout);
        cout << s << "\n";
        cout.flush();
        cout.rdbuf(cerr.rdbuf());
    };

    while (getline(cin, line)) {
        istringstream iss(line);
        string cmd;
        if (!(iss >> cmd)) continue;

        if (cmd == "INIT") {
            int pid = 0, bs = 4;
            iss >> pid >> bs;
            if (player) { delete player; player = nullptr; }
            player = makePlayer(pid);
            reply(player ? "OK" : "ERR unknown_player");
        } else if (cmd == "MOVE") {
            int bs = 4;
            iss >> bs;
            Board board(bs, true);      // quiet=true; constructor will plant two
                                        // random tiles that we immediately overwrite
            for (int i = 0; i < bs; i++) {
                for (int j = 0; j < bs; j++) {
                    int v = 0;
                    iss >> v;
                    board.setGrid(i, j, v);
                }
            }
            char mv = player ? player->getMove(board) : 'q';
            string out = "OK ";
            out.push_back(mv);
            reply(out);
        } else if (cmd == "QUIT") {
            break;
        } else {
            reply("ERR unknown_cmd");
        }
    }

    delete player;
    return 0;
}
