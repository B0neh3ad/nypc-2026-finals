"""공통 유틸 — 경로 해석, JSON I/O, 원자적 append, 로깅."""
import fcntl
import json
import os
import subprocess
import sys
import time

MOE_ROOT = os.environ.get("MOE_ROOT") or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _first_existing(*cands):
    for c in cands:
        if c and os.path.exists(c):
            return c
    return None


def shared_root():
    """팀 공유 폴더. 클러스터에는 없을 수 있으므로 번들 경로를 먼저 본다."""
    return (os.environ.get("NYPC_SHARED")
            or _first_existing(os.path.join(MOE_ROOT, "bundle"), "/srv/nypc", os.path.expanduser("~/shared"))
            or "/srv/nypc")


def referee_path():
    p = os.environ.get("MOE_REFEREE")
    if p:
        return p
    return _first_existing(
        os.path.join(MOE_ROOT, "bundle", "testing-tool.py"),
        os.path.join(shared_root(), "problem", "nation-fr-providing", "testing-tool.py"),
    ) or os.path.join(shared_root(), "problem", "nation-fr-providing", "testing-tool.py")


def build_sh():
    p = os.environ.get("MOE_BUILD_SH")
    if p:
        return p
    return _first_existing(os.path.join(shared_root(), "tools", "build.sh"))


def path(*parts):
    return os.path.join(MOE_ROOT, *parts)


def work(*parts):
    p = os.path.join(os.environ.get("MOE_WORK") or path("work"), *parts)
    return p


def ensure_dir(p):
    os.makedirs(p, exist_ok=True)
    return p


def read_json(p, default=None):
    if not os.path.exists(p):
        return default
    with open(p) as f:
        return json.load(f)


def write_json(p, obj):
    ensure_dir(os.path.dirname(p))
    tmp = p + ".tmp%d" % os.getpid()
    with open(tmp, "w") as f:
        json.dump(obj, f, indent=2, ensure_ascii=False, sort_keys=False)
        f.write("\n")
    os.replace(tmp, p)


def append_jsonl(p, obj):
    """여러 프로세스(=슬럼 태스크)가 같은 파일에 써도 줄이 섞이지 않게 flock."""
    ensure_dir(os.path.dirname(p))
    line = json.dumps(obj, ensure_ascii=False, sort_keys=True) + "\n"
    with open(p, "a") as f:
        fcntl.flock(f.fileno(), fcntl.LOCK_EX)
        f.write(line)
        f.flush()
        os.fsync(f.fileno())
        fcntl.flock(f.fileno(), fcntl.LOCK_UN)


def read_jsonl(p):
    out = []
    if not os.path.exists(p):
        return out
    with open(p) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                out.append(json.loads(line))
            except json.JSONDecodeError:
                continue          # 동시 쓰기 중 잘린 줄은 버린다
    return out


_T0 = time.time()


def log(msg):
    sys.stderr.write("[%7.1fs] %s\n" % (time.time() - _T0, msg))
    sys.stderr.flush()


def run(cmd, timeout=None, env=None, cwd=None):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout,
                          env=env, cwd=cwd)
