# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A university DSA assignment: a console **2048** game in C++ with several AI "players", plus a **Streamlit** web UI (`streamlit_app/`) that reuses the *real* C++ player code (not a Python reimplementation) by talking to a small C++ subprocess.

There are two independent runnable artifacts:
- The C++ console game (`Game2048.exe` from `Main.cpp`).
- The Streamlit app (`streamlit_app/app.py`), which spawns `streamlit_app/move_oracle.exe`.

## Commands

### C++ console game
The project is an Eclipse CDT project (`.cproject`, `.project`, `build/Debug/`); `Game2048.exe` at the root is the prebuilt binary. To rebuild manually:
```bash
g++ -std=c++14 -O2 -o Game2048.exe Main.cpp
```
Run it interactively — it prompts for board size, then a player choice (1–9, see `Main.cpp`).

### Streamlit app (run from `streamlit_app/`)
```bash
python -m pip install -r requirements.txt
# Rebuild the oracle after ANY change to a *.h player or Board.h (note -I.. to find the headers one level up):
g++ -std=c++14 -O2 -I.. -o move_oracle.exe move_oracle.cpp
python -m streamlit run app.py --server.headless true   # serves http://localhost:8501
```
Health check: `curl -s http://localhost:8501/_stcore/health`.

There is no test suite, linter, or CI. `.clang-format` / `.clangd` exist for editor tooling only.

## Architecture

### C++ core (header-only players)
- **`Board.h`** — the entire game model lives here (state + rules) and is `#include`d everywhere. Note the methods marked `Task1_3`/`Task2_2`/`Task3_1` (`countEmptyCells`, `canMove`, `merge`, `makeMove`) — these are the *student-implemented* assignment tasks. There are **two** `makeMove` overloads: `makeMove(char 'l'|'r'|'u'|'d')` (the core mutator) and `makeMove(int 0..3)` (returns whether the board changed; used by training/search loops). Board geometry scales with size: `target` and `bonus` are derived from `boardSize`, so a 4×4 targets 2048 but larger boards target higher.
- **`Player.h`** — abstract base; every player implements `char getMove(const Board&)` returning `'l'/'r'/'u'/'d'`. `Move` is a `{direction, value}` pair ordered by value for use in priority queues.
- **Player implementations**, each a header subclass of `Player`, wired into the menu in `Main.cpp` by number:
  - `2 RandomPlayer`, `3 BestFirstPlayer` (greedy heuristic), `4 MonteCarloPlayer`, `5 MarkovPlayer`, `6 SmartPlayer` (2-ply expectimax-ish search over `evaluate()`), `8 ImprovedMonteCarloPlayer` (Monte Carlo + `ZobristHashTable.h` cache).
  - `7 RLPlayer` — N-tuple TD-learning value network; loads/saves **`rl_weights.bin`** (large, ~268 MB) from the **current working directory**.
  - `9 GeneticPlayer` — GA-evolved heuristic weights; loads/saves **`ga_weights.bin`** from the cwd, trains on first run if the file is missing.
  - `HumanPlayer` (`1`) reads keystrokes; not used by the oracle.
- **`Game.h`** — the play loop (`Game::play()`): print board → player move → add tile → check win/loss. `NUMBEROFGAMES` controls how many games run per session.

Because RL/Genetic players read their `.bin` weights from the cwd, **anything launching them must run from the project root** (the directory containing `rl_weights.bin` / `ga_weights.bin`).

### Streamlit layer (`streamlit_app/`)
The key design decision: **the UI does not reimplement the AI** — it shells out to the C++ players.

- **`move_oracle.cpp` → `move_oracle.exe`** — a persistent subprocess wrapping the C++ players. Line protocol over stdin/stdout:
  - `INIT <player_id> <board_size>` → `OK` / `ERR <msg>` (constructs the player; may load weights, slow first time)
  - `MOVE <board_size> <cell0 cell1 ... >` (row-major) → `OK <dir>`
  - `QUIT`
  - It redirects all player `cout` debug spew to stderr so stdout carries only protocol replies. Score/steps are deliberately not tracked here — heuristics use score *deltas* that cancel at baseline zero, preserving move choice.
- **`game2048.py`**:
  - `Board` is a Python dataclass that **mirrors `Board.h` semantics** (merge/move/scoring/target/bonus) so the UI can render and step locally without C++ for state.
  - `OracleClient` manages one subprocess = one player; it launches the oracle with `cwd=` the **project root** (so RL/Genetic find their `.bin` files) and exposes `get_move(board)`. `make_player(name, size)` is the factory; `Human` returns `None`.
  - `PLAYER_IDS` maps UI names to the same integer IDs used in `Main.cpp` — keep these in sync with the oracle's `makePlayer()` switch.
- **`app.py`** — the Streamlit UI: sidebar (board size, player), board rendering, Human button controls vs. AI "step / auto-play" controls. If the oracle is unavailable it falls back to Human controls instead of crashing.

### When changing things, watch these cross-cutting invariants
- Player numbering must stay consistent across **three** places: `Main.cpp`, `move_oracle.cpp`'s `makePlayer()`, and `game2048.py`'s `PLAYER_IDS`.
- Game rules exist in **two** implementations — `Board.h` (C++) and `Board` in `game2048.py`. A rule change in one must be mirrored in the other or the UI's local board will diverge from what the oracle's C++ board assumes.
- After editing any C++ header or `move_oracle.cpp`, the oracle must be **recompiled** for the Streamlit app to pick up the change.
