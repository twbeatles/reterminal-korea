#Requires -Version 5.1
# K-Dash local CI (Windows): host tests + preview + secret scan + native C++ tests.
# No real API keys are needed; live-API probes are opt-in only.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

Write-Host "== host tests =="
python -m pytest host/tests -q
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "== mutation gate (docs/07: mutants must be killed) =="
python tools/mutation_check.py
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== soak report on fixture log (L5 parser gate) =="
python tools/soak_report.py test/fixtures/soak_ok.log
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== preview =="
python tools/generate_preview.py
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== fixture sanitizer self-test =="
'{"serviceKey":"FAKE-KEY-1234","a":1}' | python tools/fixture_sanitizer.py | Out-Null
if ($LASTEXITCODE -eq 0) { Write-Host "sanitizer redacts OK" } else { exit 1 }

Write-Host "== native C++ tests (zig/gcc/clang; exit 2 = BLOCKED_TOOLCHAIN) =="
python tools/run_native_tests.py
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== platformio firmware build (optional; needs toolchain) =="
if (Get-Command pio -ErrorAction SilentlyContinue) {
  pio run -e reterminal_e1001
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
  Write-Host "BLOCKED_TOOLCHAIN: pio not installed; firmware build not run"
  exit 2
}

Write-Host "CI LOCAL DONE"
