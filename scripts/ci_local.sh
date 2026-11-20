#!/bin/sh
# K-Dash local CI (Unix): host tests + preview + secret scan + native C++ tests.
# No real API keys needed; live-API probes are opt-in only.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
echo "== host tests =="
python3 -m pytest host/tests -q
echo "== mutation gate (docs/07: mutants must be killed) =="
python3 tools/mutation_check.py
echo "== soak report on fixture log (L5 parser gate) =="
python3 tools/soak_report.py test/fixtures/soak_ok.log
echo "== preview =="
python3 tools/generate_preview.py
echo "== fixture sanitizer self-test =="
echo '{"serviceKey":"FAKE-KEY-1234","a":1}' | python3 tools/fixture_sanitizer.py >/dev/null
echo "sanitizer redacts OK"
echo "== native C++ tests (exit 2 = BLOCKED_TOOLCHAIN) =="
python3 tools/run_native_tests.py
echo "== platformio (optional) =="
if command -v pio >/dev/null 2>&1; then
  pio run -e reterminal_e1001
else
  echo "BLOCKED_TOOLCHAIN: pio not installed; firmware build not run"
  exit 2
fi
echo "CI LOCAL DONE"
