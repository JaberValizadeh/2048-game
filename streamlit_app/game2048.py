"""Game state + thin wrapper around the C++ move_oracle subprocess.

Board mirrors the C++ Board.h state transitions (so the Streamlit UI can
render and step locally), but every AI move comes from the C++ players via
the move_oracle.exe subprocess — no Python reimplementations of the player
logic live in this file.
"""

from __future__ import annotations

import atexit
import copy
import os
import random
import subprocess
from dataclasses import dataclass, field
from typing import List, Optional, Tuple


DIRECTIONS = ("l", "r", "u", "d")

# Player IDs match Main.cpp's choice numbering.
PLAYER_IDS = {
    "Human": None,
    "Random Player": 2,
    "Heuristic (Best-First)": 3,
    "Monte Carlo Player": 4,
    "Markov Player": 5,
    "Smart Player": 6,
    "RL Player (N-Tuple TD)": 7,
    "Improved Monte Carlo (Zobrist)": 8,
    "Genetic Player": 9,
}

_PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
_ORACLE_PATH = os.path.join(os.path.dirname(__file__), "move_oracle.exe")


# ---------------------------------------------------------------------------
# Board (pure state, mirrors Board.h semantics)
# ---------------------------------------------------------------------------


@dataclass
class Board:
    size: int = 4
    grid: List[List[int]] = field(default_factory=list)
    score: int = 0
    steps: int = 0
    target: int = 2048
    bonus: int = 20000
    last_tile: Optional[Tuple[int, int, int]] = None

    def __post_init__(self) -> None:
        if not self.grid:
            self.grid = [[0] * self.size for _ in range(self.size)]
            self.target = 2048 * ((self.size - 3) // 3 + 1)
            self.bonus = 10000 * (3 - (self.size - 3) % 3)
            self.add_new_tile()
            self.add_new_tile()

    def clone(self) -> "Board":
        return copy.deepcopy(self)

    def empty_cells(self) -> List[Tuple[int, int]]:
        return [(i, j) for i in range(self.size) for j in range(self.size) if self.grid[i][j] == 0]

    def count_empty(self) -> int:
        return len(self.empty_cells())

    def max_tile(self) -> int:
        return max(max(row) for row in self.grid)

    def has_won(self) -> bool:
        return self.max_tile() >= self.target

    def can_move(self) -> bool:
        if self.count_empty() > 0:
            return True
        for i in range(self.size):
            for j in range(self.size):
                if j + 1 < self.size and self.grid[i][j] == self.grid[i][j + 1]:
                    return True
                if i + 1 < self.size and self.grid[i][j] == self.grid[i + 1][j]:
                    return True
        return False

    def add_new_tile(self) -> None:
        empties = self.empty_cells()
        if not empties:
            return
        x, y = random.choice(empties)
        value = 2 if random.randint(0, 9) < 9 else 4
        self.grid[x][y] = value
        self.last_tile = (x, y, value)

    def _merge(self, row: List[int]) -> List[int]:
        merged: List[int] = []
        i = 0
        while i < len(row):
            if i + 1 < len(row) and row[i] == row[i + 1]:
                merged.append(2 * row[i])
                self.score += 2 * row[i]
                i += 2
            else:
                merged.append(row[i])
                i += 1
        while len(merged) < self.size:
            merged.append(0)
        return merged

    def make_move(self, direction: str) -> bool:
        snapshot = [row[:] for row in self.grid]
        snap_score, snap_steps = self.score, self.steps
        for i in range(self.size):
            if direction == "l":
                row = [self.grid[i][j] for j in range(self.size) if self.grid[i][j] != 0]
                merged = self._merge(row)
                for j in range(self.size):
                    self.grid[i][j] = merged[j]
            elif direction == "r":
                row = [self.grid[i][j] for j in range(self.size - 1, -1, -1) if self.grid[i][j] != 0]
                merged = self._merge(row)
                for j in range(self.size):
                    self.grid[i][self.size - j - 1] = merged[j]
            elif direction == "u":
                row = [self.grid[j][i] for j in range(self.size) if self.grid[j][i] != 0]
                merged = self._merge(row)
                for j in range(self.size):
                    self.grid[j][i] = merged[j]
            elif direction == "d":
                row = [self.grid[j][i] for j in range(self.size - 1, -1, -1) if self.grid[j][i] != 0]
                merged = self._merge(row)
                for j in range(self.size):
                    self.grid[self.size - j - 1][i] = merged[j]
        self.steps += 1
        if self.grid == snapshot:
            self.score, self.steps = snap_score, snap_steps
            return False
        return True

    def score_per_step(self) -> float:
        return self.score / self.steps if self.steps > 0 else 0.0


# ---------------------------------------------------------------------------
# C++ subprocess client
# ---------------------------------------------------------------------------


class OracleError(RuntimeError):
    pass


class OracleClient:
    """Persistent subprocess that wraps the C++ player classes.

    One client = one chosen player. Re-instantiate to switch players.
    Working directory is the project root so RL/Genetic can find
    rl_weights.bin and ga_weights.bin.
    """

    def __init__(self, player_id: int, board_size: int, oracle_path: str = _ORACLE_PATH) -> None:
        if not os.path.exists(oracle_path):
            raise OracleError(
                f"move_oracle.exe not found at {oracle_path}. "
                "Build it with: g++ -std=c++14 -O2 -I.. -o move_oracle.exe move_oracle.cpp"
            )
        self.player_id = player_id
        self.board_size = board_size
        self._proc = subprocess.Popen(
            [oracle_path],
            cwd=_PROJECT_ROOT,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
        atexit.register(self.close)
        self._send(f"INIT {player_id} {board_size}")
        reply = self._read()
        if not reply.startswith("OK"):
            self.close()
            raise OracleError(f"oracle INIT failed: {reply}")

    def _send(self, line: str) -> None:
        if self._proc.stdin is None or self._proc.poll() is not None:
            raise OracleError("oracle process is not running")
        self._proc.stdin.write(line + "\n")
        self._proc.stdin.flush()

    def _read(self) -> str:
        if self._proc.stdout is None:
            raise OracleError("oracle stdout is closed")
        line = self._proc.stdout.readline()
        if not line:
            raise OracleError("oracle closed unexpectedly")
        return line.strip()

    def get_move(self, board: Board) -> str:
        cells = " ".join(str(board.grid[i][j]) for i in range(board.size) for j in range(board.size))
        self._send(f"MOVE {board.size} {cells}")
        reply = self._read()
        if not reply.startswith("OK "):
            raise OracleError(f"oracle MOVE failed: {reply}")
        return reply[3:].strip()[:1] or "q"

    def close(self) -> None:
        if self._proc.poll() is None:
            try:
                self._send("QUIT")
                self._proc.wait(timeout=2)
            except Exception:
                self._proc.kill()


# Convenience for callers that want a flat "factory" interface.
def make_player(name: str, board_size: int) -> Optional[OracleClient]:
    pid = PLAYER_IDS.get(name)
    if pid is None:
        return None
    return OracleClient(player_id=pid, board_size=board_size)
