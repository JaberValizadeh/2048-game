"""Streamlit UI that drives the C++ 2048 players via move_oracle.exe."""

from __future__ import annotations

import time

import streamlit as st

from game2048 import PLAYER_IDS, Board, OracleClient, OracleError, make_player


TILE_COLORS = {
    0: ("#cdc1b4", "#776e65"),
    2: ("#eee4da", "#776e65"),
    4: ("#ede0c8", "#776e65"),
    8: ("#f2b179", "#f9f6f2"),
    16: ("#f59563", "#f9f6f2"),
    32: ("#f67c5f", "#f9f6f2"),
    64: ("#f65e3b", "#f9f6f2"),
    128: ("#edcf72", "#f9f6f2"),
    256: ("#edcc61", "#f9f6f2"),
    512: ("#edc850", "#f9f6f2"),
    1024: ("#edc53f", "#f9f6f2"),
    2048: ("#edc22e", "#f9f6f2"),
}
HIGH_TILE = ("#3c3a32", "#f9f6f2")
DIRECTION_MAP = {"⬅ Left": "l", "➡ Right": "r", "⬆ Up": "u", "⬇ Down": "d"}


# ---------------------------------------------------------------------------
# Session state
# ---------------------------------------------------------------------------


def init_state(board_size: int, player_name: str) -> None:
    # Tear down any existing oracle before creating a new board / player.
    old = st.session_state.get("player")
    if isinstance(old, OracleClient):
        old.close()

    st.session_state.board = Board(size=board_size)
    player, msg = None, ""
    if PLAYER_IDS.get(player_name) is not None:
        try:
            player = make_player(player_name, board_size)
        except OracleError as exc:
            msg = f"⚠ {player_name} unavailable: {exc}. Falling back to Human controls."
    st.session_state.player = player
    st.session_state.player_name = player_name
    st.session_state.board_size = board_size
    st.session_state.game_over = False
    st.session_state.won = False
    st.session_state.auto_play = False
    st.session_state.message = msg


def ensure_state(board_size: int, player_name: str) -> None:
    if "board" not in st.session_state:
        init_state(board_size, player_name)


# ---------------------------------------------------------------------------
# Rendering
# ---------------------------------------------------------------------------


def render_board(board: Board) -> str:
    cell_px = max(48, 320 // board.size)
    font_px = max(14, cell_px // 2 - 4)
    rows_html = []
    for row in board.grid:
        cells_html = []
        for value in row:
            bg, fg = TILE_COLORS.get(value, HIGH_TILE)
            label = "" if value == 0 else str(value)
            cells_html.append(
                f"<div style='width:{cell_px}px;height:{cell_px}px;"
                f"background:{bg};color:{fg};border-radius:6px;"
                f"display:flex;align-items:center;justify-content:center;"
                f"font-weight:700;font-size:{font_px}px;font-family:sans-serif;'>"
                f"{label}</div>"
            )
        rows_html.append(
            f"<div style='display:flex;gap:8px;'>{''.join(cells_html)}</div>"
        )
    return (
        "<div style='display:inline-flex;flex-direction:column;gap:8px;"
        "padding:12px;background:#bbada0;border-radius:8px;'>"
        + "".join(rows_html)
        + "</div>"
    )


def apply_move(direction: str) -> None:
    board: Board = st.session_state.board
    if st.session_state.game_over:
        return
    if direction not in DIRECTIONS:
        # 'q' or unrecognised — treat as concede
        st.session_state.game_over = True
        st.session_state.auto_play = False
        st.session_state.message = "Player conceded."
        return
    changed = board.make_move(direction)
    if changed:
        board.add_new_tile()
        if board.has_won() and not st.session_state.won:
            st.session_state.won = True
            st.session_state.message = (
                f"🎉 Reached {board.target}! Bonus +{board.bonus}. Keep going for a higher tile."
            )
            board.score += board.bonus
    if not board.can_move():
        st.session_state.game_over = True
        st.session_state.auto_play = False
        st.session_state.message = "Game over — no moves left."


DIRECTIONS = ("l", "r", "u", "d")


# ---------------------------------------------------------------------------
# Sidebar / controls
# ---------------------------------------------------------------------------


st.set_page_config(page_title="2048 — DSA26 Assignment", page_icon="🎮", layout="centered")
st.title("🎮 2048 — Streamlit edition")
st.caption("Streamlit UI driving the C++ players from Game2048BaseCode (move_oracle subprocess).")

with st.sidebar:
    st.header("Game setup")
    board_size = st.slider("Board size", min_value=3, max_value=8, value=4)
    player_name = st.selectbox("Player", list(PLAYER_IDS.keys()), index=0)
    if st.button("🔄 New game", use_container_width=True):
        init_state(board_size, player_name)
        st.rerun()

    ensure_state(board_size, player_name)

    if (
        st.session_state.board_size != board_size
        or st.session_state.player_name != player_name
    ):
        init_state(board_size, player_name)
        st.rerun()

    st.divider()
    st.markdown(
        f"**Target:** {st.session_state.board.target}  \n"
        f"**Win bonus:** {st.session_state.board.bonus}"
    )
    if player_name == "RL Player (N-Tuple TD)" and board_size != 4:
        st.warning("RL Player is trained for 4×4 only. It will play randomly on other sizes.")

# ---------------------------------------------------------------------------
# Main panel
# ---------------------------------------------------------------------------

board: Board = st.session_state.board

metric_cols = st.columns(4)
metric_cols[0].metric("Score", board.score)
metric_cols[1].metric("Steps", board.steps)
metric_cols[2].metric("Score / step", f"{board.score_per_step():.2f}")
metric_cols[3].metric("Max tile", board.max_tile())

st.markdown(render_board(board), unsafe_allow_html=True)

if st.session_state.message:
    if st.session_state.game_over:
        st.error(st.session_state.message)
    else:
        st.success(st.session_state.message)

st.markdown("---")

if st.session_state.player is None:
    st.subheader("Your move")
    btn_cols = st.columns(4)
    pressed = None
    for col, (label, direction) in zip(btn_cols, DIRECTION_MAP.items()):
        if col.button(label, use_container_width=True, disabled=st.session_state.game_over):
            pressed = direction
    if pressed:
        apply_move(pressed)
        st.rerun()
    st.caption("Tip: use the four buttons to slide tiles. Same-value tiles merge.")
else:
    st.subheader(f"AI: {st.session_state.player_name}  (C++)")
    ctrl_cols = st.columns(3)
    step_clicked = ctrl_cols[0].button(
        "▶ Step once", use_container_width=True, disabled=st.session_state.game_over
    )
    if ctrl_cols[1].button(
        "⏯ Toggle auto-play",
        use_container_width=True,
        disabled=st.session_state.game_over,
    ):
        st.session_state.auto_play = not st.session_state.auto_play
        st.rerun()
    delay_ms = ctrl_cols[2].slider("Delay (ms)", 0, 1000, 120, step=20)

    if step_clicked:
        try:
            move = st.session_state.player.get_move(board)
            apply_move(move)
        except OracleError as exc:
            st.session_state.message = f"Oracle error: {exc}"
            st.session_state.auto_play = False
        st.rerun()

    if st.session_state.auto_play and not st.session_state.game_over:
        try:
            move = st.session_state.player.get_move(board)
            apply_move(move)
        except OracleError as exc:
            st.session_state.message = f"Oracle error: {exc}"
            st.session_state.auto_play = False
        if delay_ms:
            time.sleep(delay_ms / 1000)
        st.rerun()
