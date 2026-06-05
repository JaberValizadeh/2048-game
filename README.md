# 2048 Game — C++ AI Players + Streamlit Web UI

A 2048 game engine written in C++ with nine playable AI strategies, plus a Streamlit web interface that drives the real C++ players via a subprocess oracle.

## Players

| # | Name | Strategy |
|---|------|----------|
| 1 | Human | Keyboard input (`w/a/s/d` or arrow keys) |
| 2 | Random | Picks a random valid move |
| 3 | Heuristic | Greedy best-first using a hand-crafted evaluation |
| 4 | Monte Carlo | Simulates random rollouts from each candidate move |
| 5 | Markov | Transition-probability-based policy |
| 6 | Smart | 2-ply expectimax-style search over `evaluate()` |
| 7 | RL | N-tuple TD-learning network (`rl_weights.bin`) |
| 8 | Improved Monte Carlo | Monte Carlo + Zobrist hash table cache |
| 9 | Genetic | GA-evolved heuristic weights (`ga_weights.bin`) |

Board size is configurable (3–15); the target tile scales with size (2048 on 4×4, higher on larger boards).

## Project structure

```
├── Board.h                  # Full game model (state, rules, scoring)
├── Player.h                 # Abstract player base class
├── *Player.h                # One header per AI strategy
├── Game.h                   # Play loop
├── Main.cpp                 # Console entry point
├── ga_weights.bin           # Pre-trained Genetic player weights
└── streamlit_app/
    ├── app.py               # Streamlit UI
    ├── game2048.py          # Python board mirror + OracleClient
    ├── move_oracle.cpp      # C++ subprocess (line protocol over stdin/stdout)
    └── requirements.txt
```

> `rl_weights.bin` (~256 MB) is excluded from this repo due to GitHub's file size limit. The RL player will train from scratch on first use if the file is missing.

## Requirements

- C++ compiler supporting C++14 (`g++` or `clang++`)
- Python 3.9+
- `streamlit >= 1.30`

## Console game

```bash
# Build
g++ -std=c++14 -O2 -o Game2048 Main.cpp

# Run (prompts for board size, then player choice)
./Game2048
```

## Streamlit web app

```bash
cd streamlit_app

# Build the oracle (required on first run, and after any C++ header change)
g++ -std=c++14 -O2 -I.. -o move_oracle.exe move_oracle.cpp

# Install Python dependencies
pip install -r requirements.txt

# Launch
python -m streamlit run app.py
```

Open [http://localhost:8501](http://localhost:8501). Use the sidebar to choose board size and player. Human mode uses on-screen buttons; AI players show a step/auto-play control.

### Oracle protocol

`move_oracle.exe` is a persistent subprocess. The Python layer talks to it over stdin/stdout:

```
INIT <player_id> <board_size>   →  OK  |  ERR <msg>
MOVE <size> <cell0 cell1 ...>   →  OK <l|r|u|d>
QUIT
```

## Architecture notes

- The Python `Board` class in `game2048.py` mirrors `Board.h` semantics exactly — rule changes in one must be reflected in the other.
- Player IDs must stay consistent across `Main.cpp`, `move_oracle.cpp`, and `game2048.py`.
- RL and Genetic players load their `.bin` weight files from the **project root** (the directory containing `Main.cpp`), so the oracle always launches with `cwd` set there.
