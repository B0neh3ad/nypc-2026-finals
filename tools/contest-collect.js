// contest-collect.js — 중간평가 결과·로그 URL을 긁어 매니페스트 JSON을 만든다.
//
// 쓰는 법:
//   1. 브라우저에서 https://contest.nypc.co.kr/problems/1 로그인
//   2. 오른쪽 패널 '중간 평가' 탭 → 보고 싶은 라운드 행 클릭 (상세로 진입)
//   3. F12 콘솔에 이 파일 전체를 붙여넣고 실행
//   4. 매니페스트 JSON 파일이 자동으로 다운로드됨
//   5. 서버로 올려서 다운로드:
//        scp ~/Downloads/nypc-round-N.json nypc-jinsu:/tmp/
//        ssh nypc-jinsu '~/shared/tools/contest-fetch.py /tmp/nypc-round-N.json'
//
// 왜 이렇게 나눠 놨나: 세션 쿠키가 HttpOnly 라 서버에서 API 를 직접 못 부릅니다.
// 그리고 로그인 정보는 공유 폴더에 두지 않는 게 규칙입니다(docs/COLLAB.md).
// 프리사인드 S3 URL 은 발급 후 **7일** 유효하므로, URL 만 넘기면 충분합니다.

(async () => {
  const findTable = (header) =>
    [...document.querySelectorAll('table')].find((t) =>
      [...t.querySelectorAll('th')].some((h) => h.innerText.trim().startsWith(header)));

  const battleTbl = findTable('대전 ID');
  if (!battleTbl) {
    alert('대전 ID 테이블이 없습니다.\n중간 평가 탭에서 라운드 행을 클릭해 상세로 들어간 뒤 다시 실행하세요.');
    return;
  }

  const battles = [...battleTbl.querySelectorAll('tbody tr')]
    .map((r) => {
      const c = [...r.querySelectorAll('td')].map((x) => x.innerText.trim());
      return { battle_id: c[0], opponent: c[1], opp_perf: c[2], my_record: c[3] };
    })
    .filter((b) => /^\d+$/.test(b.battle_id));

  const heading =
    [...document.querySelectorAll('h1,h2,h3,h4')]
      .map((h) => h.innerText.trim())
      .find((t) => t.includes('중간평가')) || '';
  const round = (heading.match(/#(\d+)/) || [])[1] || 'x';

  console.log(`라운드 #${round} · 배틀 ${battles.length}개 · 로그 URL 요청 중...`);

  for (const b of battles) {
    // /log/1, /log/2 ... 를 순회한다. 한 대전에 여러 판이 있을 수 있다.
    b.logs = [];
    for (let g = 1; g <= 10; g++) {
      const res = await fetch(`/api/v2/battle/${b.battle_id}/log/${g}`, {
        credentials: 'include',
      });
      if (!res.ok) break;
      const j = await res.json();
      b.logs.push({ game: g, url: j.url, pgn: j.pgn || null });
    }
    console.log(`  ${b.battle_id}  vs ${b.opponent}  로그 ${b.logs.length}개`);
  }

  const roundTbl = findTable('라운드 시각');
  const manifest = {
    round: Number(round) || round,
    heading,
    fetched_at: new Date().toISOString(),
    rounds_overview: roundTbl
      ? [...roundTbl.querySelectorAll('tbody tr')].map((r) =>
          [...r.querySelectorAll('td')].map((x) => x.innerText.trim()))
      : [],
    submissions: (() => {
      const t = findTable('제출 시각');
      return t
        ? [...t.querySelectorAll('tbody tr')]
            .map((r) => [...r.querySelectorAll('td')].map((x) => x.innerText.trim()))
            .filter((r) => r.length > 1)
        : [];
    })(),
    battles,
  };

  const blob = new Blob([JSON.stringify(manifest, null, 2)], { type: 'application/json' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = `nypc-round-${round}.json`;
  a.click();
  console.log(`완료: nypc-round-${round}.json (배틀 ${battles.length}개)`);
})();
