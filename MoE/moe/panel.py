"""opponents/ 폴더 = 상대 패널. 여기 있는 봇들에 대한 평균 승률이 score 다."""
import os

from . import build, util

EXTS = (".c", ".cc", ".cpp", ".cxx", ".py")


def opponents_dir():
    return os.environ.get("MOE_OPPONENTS") or util.path("opponents")


def list_opponents(names=None):
    d = opponents_dir()
    if not os.path.isdir(d):
        raise SystemExit("opponents 폴더가 없습니다: %s" % d)
    out = []
    for fn in sorted(os.listdir(d)):
        p = os.path.join(d, fn)
        if not os.path.isfile(p):
            continue
        if os.path.splitext(fn)[1].lower() not in EXTS:
            continue
        name = os.path.splitext(fn)[0]
        if names and name not in names:
            continue
        out.append({"name": name, "src": p})
    if not out:
        raise SystemExit("opponents 폴더에 봇이 없습니다: %s (예전 우리 봇들을 넣으세요)" % d)
    return out


def prepare(names=None):
    """패널 전체를 미리 컴파일해 둔다 (병렬 팬아웃 전에 한 번)."""
    panel = list_opponents(names)
    for o in panel:
        util.log("패널 준비: %s" % o["name"])
        o["cmd"] = build.command_for(o["src"])
    return panel


def weights(panel):
    cfg = util.read_json(util.path("config", "search.json"), {}) or {}
    w = cfg.get("opponent_weights", {})
    return [float(w.get(o["name"], 1.0)) for o in panel]
