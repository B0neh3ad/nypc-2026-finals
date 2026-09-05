# 리플레이 시뮬레이터 (정적 미러)

주최측 시뮬레이터를 통째로 받아둔 것입니다. 원본:
<https://d1thb30t7rs13h.cloudfront.net/>

```
simulator/
  index.html            ← /assets/ 를 ./assets/ 로 바꾼 것 (하위 경로에서도 열림)
  index.original.html   ← 받은 그대로 (손대지 않음)
  assets/               ← main JS/CSS + visionMemory 청크
```

## 왜 미러를 두나

- 대회 중 CloudFront 가 느리거나 막히면 즉시 대안이 됩니다.
- 페이지 제목이 `NEXT NATION` 입니다 — 예선과 같은 계보라는 방증이라 보관 가치가 있습니다.

## 쓰는 법

**`file://` 로 직접 열면 안 됩니다.** ES 모듈이라 브라우저가 CORS 로 막습니다.
반드시 HTTP 로 서빙하세요.

노트북에 받아서 여는 게 제일 간단합니다:

```bash
scp -r nypc-jinsu:~/shared/simulator ~/nypc-simulator
cd ~/nypc-simulator && python3 -m http.server 8000
```

그리고 <http://localhost:8000> 을 엽니다. 로그 파일 내용을 붙여넣으면 됩니다.

서버에서 바로 보고 싶으면 포트 포워딩:

```bash
ssh -L 8000:127.0.0.1:8000 nypc-jinsu 'cd ~/shared/simulator && python3 -m http.server 8000 --bind 127.0.0.1'
```

## 로그 만들기

```bash
cd ~/shared/tools
./versus.sh mybot sample --seed 42 -l /tmp/log.txt
scp nypc-jinsu:/tmp/log.txt ~/Downloads/
```

`match.sh` 는 기본적으로 로그를 지웁니다(수천 판이면 수백 MB). 남기려면
`KEEP_LOGS=1` 을 주세요 — 다만 임시 폴더는 종료 시 삭제되므로 한 판씩 볼 때는
`versus.sh` 를 쓰는 편이 낫습니다.

## 주의

폰트는 CDN(jsdelivr, Google Fonts)에서 받습니다. 오프라인이면 글꼴만 기본체로
떨어지고 기능은 그대로 동작합니다.

에셋 파일명에 해시가 박혀 있습니다(`main-D_BckSFJ.js`). 주최측이 시뮬레이터를
업데이트하면 이 미러는 낡은 채로 남습니다. 이상하면 원본 URL 과 비교하세요.
