# 검증 요약과 로컬 증거

Git에는 검토한 Markdown 보고서만 보관한다. UART/API 캡처·빌드/테스트 로그·측정 JSON·프리뷰 PNG 등은 `.gitignore`로 제외하고 로컬에 남긴다. 실행 중 `active/`와 원본 `raw/`는 Markdown을 포함해 전부 제외한다.

각 보고서의 명령·실행 당시 결과·미검증 범위는 공개 기록이다. 보고서에 적힌 로그/이미지/JSON 경로는 **로컬 증거 경로**이며 GitHub의 새 clone에는 없을 수 있다. 원래 기기/API 실행 결과는 재생성된 합성 fixture로 대신했다고 주장하지 않는다. 보고서 날짜와 커밋 기준을 확인한다.

재생성 가능한 Host 검증:

```powershell
python -m pytest host/tests -q
python tools/run_native_tests.py
python tools/render_previews.py
pio run -e reterminal_e1001
```

실기 수집은 [운용 절차](../docs/18_DEVICE_OPERATIONS_RUNBOOK.md)를 따른다. 실제 키·토큰·비밀번호·원본 캡처를 커밋하지 않는다. 공개할 추가 증거가 필요하면 마스킹·검토한 최소 내용만 Markdown 보고에 옮긴다.

대표 제품 화면 `docs/images/tab-*.png`와 기기 빌드에 필요한 `src/ui/fonts/*.h`, 테스트 fixture/golden, 공식 참조 소스/라이선스는 보존한다. 이 파일들은 임시 생성 이미지와 역할이 다르다.

`.gitignore`와 `git rm --cached`는 **현재 추적 상태만** 바꾼다. 과거 커밋의 파일은 Git history에 남는다. 공개 전 실제 비밀이 과거에 들어간 사실이 확인되면 키 교체와 별도 이력 정리가 필요하다. 이번 작업에서 이력 재작성/강제 push는 하지 않는다.
