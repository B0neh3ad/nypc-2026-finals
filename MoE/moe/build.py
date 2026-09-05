"""후보 파라미터로 봇을 컴파일한다. -DNAME=v 주입 + 해시 캐시."""
import hashlib
import os
import subprocess

from . import util

BUILD_MODE = os.environ.get("MOE_BUILD_MODE", "judge")
ARCH = {"judge": ["-march=znver4", "-mtune=znver4"],
        "native": ["-march=native", "-mtune=native"],
        "static": ["-march=znver4", "-mtune=znver4", "-static"]}
COMMON = ["-std=gnu++20", "-O2", "-DONLINE_JUDGE", "-DNYPC", "-w"]
BOOST_INC = "/opt/boost/gcc/include"
BOOST_LIB = "/opt/boost/gcc/lib"


def cache_dir():
    return util.ensure_dir(util.work("bin"))


def _key(src, defines, mode):
    h = hashlib.md5()
    h.update(open(src, "rb").read())
    h.update(("|" + " ".join(defines) + "|" + mode).encode())
    return h.hexdigest()[:16]


def compile_bot(src, defines=(), mode=None, force=False):
    """컴파일된 실행파일 경로를 돌려준다. 같은 (소스, 정의) 조합은 재사용."""
    mode = mode or BUILD_MODE
    defines = list(defines)
    out = os.path.join(cache_dir(), "%s-%s" % (os.path.basename(src).split(".")[0], _key(src, defines, mode)))
    if os.path.exists(out) and not force:
        return out
    ext = os.path.splitext(src)[1]
    cxx = "gcc" if ext == ".c" else "g++"
    cmd = [cxx] + COMMON + ARCH[mode] + defines + ["-o", out + ".tmp", src]
    if os.path.isdir(BOOST_INC):
        cmd += ["-I" + BOOST_INC, "-L" + BOOST_LIB]
    if ext == ".c":
        cmd = [c for c in cmd if c != "-std=gnu++20"] + ["-lm"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("컴파일 실패: %s\n%s" % (src, r.stderr[-2000:]))
    os.replace(out + ".tmp", out)
    os.chmod(out, 0o755)
    return out


def command_for(src, defines=()):
    """봇 소스를 심판기에 넘길 실행 명령 문자열로."""
    ext = os.path.splitext(src)[1].lower()
    if ext == ".py":
        return "%s %s" % (os.environ.get("MOE_BOT_PYTHON", "python3"), src)
    if ext in (".c", ".cc", ".cxx", ".cpp"):
        return compile_bot(src, defines)
    if os.access(src, os.X_OK):
        return src
    raise RuntimeError("어떻게 실행할지 모르는 봇: %s" % src)
