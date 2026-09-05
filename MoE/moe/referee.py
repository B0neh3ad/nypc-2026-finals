"""심판기(testing-tool.py) 한 판 실행 + 결과 파싱."""
import os
import re
import subprocess
import tempfile

from . import util

RESULT_RE = re.compile(r"^RESULT\s+(LEFT_WIN|RIGHT_WIN|DRAW)\s+(\S+)", re.M)
END_TURN_RE = re.compile(r"^END TURN (\d+)", re.M)
MAX_TURN = 400


def run_game(left_cmd, right_cmd, seed, np_, kp, timeout=180, keep_log=None, python=None):
    """한 판 돌리고 {'winner': 'LEFT'|'RIGHT'|'DRAW'|'ERR', 'reason': ...} 반환."""
    tool = util.referee_path()
    py = python or os.environ.get("MOE_REFEREE_PYTHON") or "python3"
    log = keep_log
    tmp = None
    if log is None:
        tmp = tempfile.NamedTemporaryFile(prefix="moe-game-", suffix=".log", delete=False)
        tmp.close()
        log = tmp.name
    cmd = [py, tool, "-a", left_cmd, "-b", right_cmd,
           "--seed", str(seed), "--NP", str(np_), "--KP", str(kp), "-l", log]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        blob = r.stdout + "\n" + r.stderr
        m = RESULT_RE.search(blob)
        if not m and os.path.exists(log):
            with open(log, errors="replace") as f:
                m = RESULT_RE.search(f.read()[-4000:])
        if not m:
            return {"winner": "ERR", "reason": "NO_RESULT", "stderr": blob[-400:]}
        side, reason = m.group(1), m.group(2)
        winner = {"LEFT_WIN": "LEFT", "RIGHT_WIN": "RIGHT", "DRAW": "DRAW"}[side]
        # 몇 일차에 끝났는지 — 승패가 포화된 패널에서 유일하게 남는 미세 신호다.
        end_turn = MAX_TURN
        if os.path.exists(log):
            with open(log, errors="replace") as f:
                f.seek(max(0, os.path.getsize(log) - 8000))
                tail = f.read()
            hits = END_TURN_RE.findall(tail)
            if hits:
                end_turn = int(hits[-1])
        return {"winner": winner, "reason": reason, "end_turn": end_turn}
    except subprocess.TimeoutExpired:
        return {"winner": "ERR", "reason": "HARNESS_TIMEOUT"}
    finally:
        if tmp is not None:
            try:
                os.unlink(tmp.name)
            except OSError:
                pass
