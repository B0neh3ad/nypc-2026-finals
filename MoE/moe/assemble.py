"""MoE 조립 — 버킷별 최적 파라미터 5벌을 한 바이너리 안에 넣는다.

탐색 중에는 `-DNAME=v` 로 버킷마다 따로 컴파일하지만, 제출은 바이너리 하나다.
그래서 최종본은 N 을 읽는 순간 버킷을 골라 파라미터 테이블을 갈아끼우는 형태여야 한다.

방법: 버킷마다 값이 다른 파라미터만 골라
    #define NAME 123      ->    #define NAME (g_moe->NAME)
로 바꾸고, 테이블 5벌과 `moe_set_map(N)` 을 튜닝 블록 앞에 끼워 넣는다. 매크로라
사용처 코드는 한 줄도 안 고쳐도 된다.

단, `#if NAME` 처럼 전처리기에서 쓰이는 이름은 이렇게 바꿀 수 없다. 그런 이름은
컴파일러가 잡아 주므로, **컴파일 -> 실패한 이름을 고정(pin) -> 재시도** 를 반복해
자동으로 걸러낸다. 고정된 이름은 버킷별로 다르게 갈 수 없으니 기준 버킷 값으로 통일하고
그 사실을 보고한다.
"""
import os
import re
import subprocess

from . import buckets, util
from .build import ARCH, BOOST_INC, BOOST_LIB, COMMON

HEADER_TMPL = """
/* ==== MoE (map-size mixture of experts) — MoE/moe/assemble.py 가 생성 ==== */
typedef struct {{
{fields}
}} MoeParams;

static const MoeParams MOE_TABLE[{nb}] = {{
{table}
}};

static const MoeParams *g_moe = &MOE_TABLE[{default_bucket}];

static void moe_set_map(int map_N) {{
  static const int MOE_N_HI[{nb}] = {{{hi_list}}};
  int b = {nb} - 1;
  for (int i = 0; i < {nb}; ++i) {{
    if (map_N <= MOE_N_HI[i]) {{ b = i; break; }}
  }}
  g_moe = &MOE_TABLE[b];
}}
/* ==== end MoE ==== */
"""


def _define_span(text, name):
    """`#define NAME value` 한 줄을 찾아 (start, end, value) 로."""
    m = re.search(r"^[ \t]*#define[ \t]+%s[ \t]+([^\n]*)$" % re.escape(name), text, re.M)
    return m


def build_source(src, per_bucket, pinned=(), default_bucket=2, hook_regex=None):
    """per_bucket: [{name: value}, ...] 버킷 순서대로. 반환 (새 소스, 가변 이름 목록)."""
    text = open(src, errors="replace").read()
    bks = buckets.load_buckets()
    names = sorted({n for d in per_bucket for n in d})
    varying = [n for n in names
               if n not in pinned
               and len({int(d.get(n)) for d in per_bucket if n in d}) > 1
               and _define_span(text, n)]

    # 값이 하나로 모이는(=버킷 간 동일) 파라미터는 그냥 그 값으로 덮어쓴다.
    for n in names:
        m = _define_span(text, n)
        if not m or n in varying:
            continue
        vals = [int(d[n]) for d in per_bucket if n in d]
        val = vals[default_bucket] if len(vals) > default_bucket else vals[0]
        text = text[:m.start()] + "#define %s %d" % (n, val) + text[m.end():]

    # 가변 파라미터는 테이블 참조로.
    for n in varying:
        m = _define_span(text, n)
        text = text[:m.start()] + "#define %s (g_moe->%s)" % (n, n) + text[m.end():]

    fields = "\n".join("  int %s;" % n for n in varying) or "  int _moe_dummy;"
    rows = []
    for b, d in enumerate(per_bucket):
        vals = ", ".join(".%s = %d" % (n, int(d[n])) for n in varying) or "._moe_dummy = 0"
        rows.append("  { %s },   /* %s */" % (vals, bks[b]["name"]))
    header = HEADER_TMPL.format(
        fields=fields, table="\n".join(rows), nb=len(per_bucket),
        default_bucket=default_bucket,
        hi_list=", ".join(str(b["n_hi"]) for b in bks[:len(per_bucket)]))

    # 튜닝 블록 시작 직전에 삽입 (모든 사용처보다 앞).
    anchor = text.find("/* ============================ TUNING PARAMETERS")
    if anchor < 0:
        anchor = 0
    text = text[:anchor] + header + text[anchor:]

    # N 을 읽은 직후에 moe_set_map 호출을 끼운다.
    hook = hook_regex or r"M->N\s*=\s*atoi\([^)]*\);"
    m = re.search(hook, text)
    if m:
        text = text[:m.end()] + "\n  moe_set_map(M->N);" + text[m.end():]
        hooked = True
    else:
        hooked = False
    return text, varying, hooked


def assemble(src, per_bucket, out_path, default_bucket=2, hook_regex=None, max_rounds=6):
    """컴파일이 통과할 때까지 문제되는 이름을 pin 하며 재시도."""
    pinned = set()
    for rnd in range(max_rounds):
        text, varying, hooked = build_source(src, per_bucket, pinned, default_bucket, hook_regex)
        util.ensure_dir(os.path.dirname(out_path))
        with open(out_path, "w") as f:
            f.write(text)
        cxx = "gcc" if out_path.endswith(".c") else "g++"
        cmd = [cxx] + [c for c in COMMON if not (cxx == "gcc" and c == "-std=gnu++20")] \
            + ARCH["judge"] + ["-fsyntax-only", out_path]
        if os.path.isdir(BOOST_INC):
            cmd += ["-I" + BOOST_INC, "-L" + BOOST_LIB]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode == 0:
            return {"ok": True, "out": out_path, "varying": varying,
                    "pinned": sorted(pinned), "hooked": hooked, "rounds": rnd + 1}
        bad = set()
        for n in varying:
            if re.search(r"\b%s\b" % re.escape(n), r.stderr):
                bad.add(n)
        if not bad:
            return {"ok": False, "out": out_path, "error": r.stderr[-3000:],
                    "varying": varying, "pinned": sorted(pinned), "hooked": hooked}
        util.log("조립 %d회차: 전처리기에서 쓰여 고정하는 이름 %d개 — %s"
                 % (rnd + 1, len(bad), ", ".join(sorted(bad))[:200]))
        pinned |= bad
    return {"ok": False, "out": out_path, "error": "고정 반복 한도 초과",
            "pinned": sorted(pinned)}
