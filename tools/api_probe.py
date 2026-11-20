#!/usr/bin/env python3
"""Opt-in bounded read-only API probes. Credentials are read only from env.

Default invocation is blocked (exit 2); --dry-run describes the plan without
network or keys. --live requires selected providers and their actual daily
quota (--daily-quota kma=1000), and spends at most 1% of that declared quota
per invocation. The operator must also account for other clients' daily use.
Raw responses stay in ignored evidence/probe/raw; sanitized captures and
provider outcomes are written only after validation. No URL/key/token is logged.
"""

from __future__ import annotations

import argparse
import datetime as dt
import http.client
import json
import os
import ssl
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "host"))
from kdash import airkorea, endpoints, exim, finance, holidays, kma, policy  # noqa: E402
from kdash.models import FetchContext, Outcome  # noqa: E402
from kdash.secret_scan import find_secrets, redact_text, scrub  # noqa: E402

MAX_BYTES = 65536
KEYS = {
    "kma": ("KDASH_KMA_KEY",),
    "air": ("KDASH_AIR_KEY",),
    "exim": ("KDASH_EXIM_KEY",),
    "fsc": ("KDASH_KMA_KEY",),
    "holiday": ("KDASH_HOLIDAY_KEY",),
    "kis": ("KDASH_KIS_APP_KEY", "KDASH_KIS_APP_SECRET"),
}


@dataclass
class Probe:
    provider: str
    name: str
    url: str = field(repr=False)
    args: tuple = ()
    body: bytes | None = field(default=None, repr=False)
    headers: dict = field(default_factory=dict, repr=False)
    negative: bool = False


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None  # Credentials must never follow a provider redirect.


def fetch(probe, timeout=15):
    if not policy.host_allowed(probe.url):
        raise ValueError("unapproved endpoint")
    opener = urllib.request.build_opener(
        NoRedirect(), urllib.request.HTTPSHandler(context=ssl.create_default_context())
    )
    req = urllib.request.Request(probe.url, data=probe.body, headers=probe.headers)
    try:
        response = opener.open(req, timeout=timeout)
    except urllib.error.HTTPError as exc:
        response = exc  # Preserve provider error bodies, but never print the URL.
    with response:
        start = time.monotonic()
        deadline = start + timeout
        declared = response.headers.get("Content-Length")
        if declared is not None and (
            not declared.isdigit() or int(declared) > MAX_BYTES
        ):
            raise ValueError("invalid/oversize response length")
        parts, n = [], 0
        # read1 avoids waiting for the entire stream on an indefinitely trickling body.
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("response deadline")
            # urllib response exposes the network socket through its buffered reader.
            # Socket deadline complements the monotonic aggregate read deadline.
            raw = getattr(getattr(response, "fp", None), "raw", None)
            sock = getattr(raw, "_sock", None)
            if sock is not None:
                sock.settimeout(remaining)
            reader = getattr(response, "read1", response.read)
            chunk = reader(min(8192, MAX_BYTES + 1 - n))
            if not chunk:
                break
            parts.append(chunk)
            n += len(chunk)
            if n > MAX_BYTES:
                raise ValueError("oversize response")
        if declared is not None and n != int(declared):
            raise ValueError("incomplete response")
        return (
            int(response.code),
            response.headers.get("Content-Type", ""),
            b"".join(parts),
        )


def plan(selected, env, now, station, nx, ny, symbol, negative=False):
    out = []
    kst = dt.datetime.fromtimestamp(now, dt.timezone(dt.timedelta(hours=9)))

    def day(d):
        return int(d.strftime("%Y%m%d"))

    if "kma" in selected:
        key = env["KDASH_KMA_KEY"]
        d, t = kma.select_ultra_now_base(now)
        pd, pt = kma.select_ultra_now_base(now - 3600)
        for name, bd, bt in (("now", d, t), ("previous", pd, pt)):
            out.append(
                Probe(
                    "kma", name, endpoints.kma_ultra_url(key, bd, bt, nx, ny), (bd, bt)
                )
            )
        d, t = kma.select_village_base(now)
        for rows in (72, 1000):
            out.append(
                Probe(
                    "kma",
                    "forecast-%d" % rows,
                    endpoints.kma_village_url(key, d, t, nx, ny, rows),
                    (d, t),
                )
            )
    if "air" in selected:
        for name, value in (
            ("station", station),
            ("absent-station", "KDASH-NO-SUCH-STATION"),
        ):
            if name != "station" and not negative:
                continue
            out.append(
                Probe(
                    "air",
                    name,
                    endpoints.airkorea_candidate_url(env["KDASH_AIR_KEY"], value),
                    (value,),
                    negative=name != "station",
                )
            )
    if "exim" in selected:
        saturday = kst.date() - dt.timedelta(days=(kst.weekday() - 5) % 7 or 7)
        dates = list(
            dict.fromkeys((day(kst), day(kst - dt.timedelta(days=1)), day(saturday)))
        )
        for index, d in enumerate(dates):
            out.append(
                Probe(
                    "exim",
                    "date-%d" % index,
                    endpoints.exim_url(env["KDASH_EXIM_KEY"], d),
                    (d,),
                )
            )
        if negative:
            out.append(
                Probe(
                    "exim",
                    "wrong-key",
                    endpoints.exim_url("FAKE-INVALID-KEY", day(kst)),
                    (day(kst),),
                    negative=True,
                )
            )
    if "fsc" in selected:
        out.append(
            Probe(
                "fsc",
                "close",
                endpoints.fsc_price_url(
                    env["KDASH_KMA_KEY"], symbol, day(kst - dt.timedelta(days=14))
                ),
                (symbol,),
            )
        )
    if "kis" in selected:
        # Official read-only auth form: examples_user/kis_auth.py (KIS upstream).
        body = json.dumps(
            {
                "grant_type": "client_credentials",
                "appkey": env["KDASH_KIS_APP_KEY"],
                "appsecret": env["KDASH_KIS_APP_SECRET"],
            }
        ).encode()
        out.append(
            Probe(
                "kis",
                "token",
                endpoints.kis_token_url(),
                body=body,
                headers={"Content-Type": "application/json"},
            )
        )
        out.append(
            Probe("kis", "price", endpoints.kis_price_url(symbol, "KRX"), (symbol,))
        )
    if "holiday" in selected:
        for year in (kst.year, kst.year + 1):
            out.append(
                Probe(
                    "holiday",
                    "year-%d" % year,
                    holidays.year_url(env["KDASH_HOLIDAY_KEY"], year),
                    (year,),
                )
            )
    return out


def clean_body(raw, credentials):
    text = raw.decode("utf-8-sig", errors="strict")
    # Replace exact known values even when providers echo them under arbitrary names.
    for value in credentials:
        for form in (value, urllib.parse.quote(value, safe="")):
            if form:
                text = text.replace(form, "***REDACTED***")
    try:
        clean = json.dumps(
            scrub(json.loads(text)), ensure_ascii=False, separators=(",", ":")
        )
        ext = "json"
    except json.JSONDecodeError:
        clean, ext = (
            redact_text(text),
            "xml" if text.lstrip().startswith("<") else "txt",
        )
    if find_secrets(clean):
        raise ValueError("capture did not pass secret scan")
    return clean, ext


def outcome(probe, status, body, ctx):
    if probe.provider == "kis":
        if probe.name == "token":
            return finance.kis_token(status, body, ctx)[1]
        return finance.parse_kis(status, body, probe.args[0], "KRX", ctx)["outcome"]
    if status != 200:
        return Outcome.AuthError if status in (401, 403) else Outcome.ProviderError
    if probe.provider == "kma":
        parser = (
            kma.parse_village
            if probe.name.startswith("forecast")
            else kma.parse_ultra_now
        )
        return parser(body, *probe.args, ctx)["outcome"]
    if probe.provider == "air":
        return airkorea.parse(body, *probe.args, ctx)["outcome"]
    if probe.provider == "exim":
        return exim.parse(body, *probe.args, ctx)["outcome"]
    if probe.provider == "holiday":
        return holidays.parse(body, *probe.args, ctx)["outcome"]
    return finance.parse_fsc(body, probe.args[0], "KRX", ctx)["outcome"]


def reserve_budget(root, probes, quotas, now):
    """Reserve calls before I/O; repeated/parallel runs share a daily cap.

    A crash may spend fewer calls than reserved, deliberately failing safe.
    A stale lock from a killed process needs operator inspection; no auto-delete.
    """
    directory = root / "evidence" / "probe" / "raw"
    directory.mkdir(parents=True, exist_ok=True)
    lock = directory / ".budget-lock"
    fd = os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    os.close(fd)
    try:
        ledger = directory / ".daily-budget.json"
        saved = (
            json.loads(ledger.read_text(encoding="utf-8")) if ledger.exists() else {}
        )
        day = dt.datetime.fromtimestamp(
            now, dt.timezone(dt.timedelta(hours=9))
        ).strftime("%Y-%m-%d")
        counts = saved.get("calls", {}) if saved.get("day") == day else {}
        for provider in {p.provider for p in probes}:
            calls = sum(p.provider == provider for p in probes)
            previous = counts.get(provider, 0)
            if (
                not isinstance(previous, int)
                or previous < 0
                or previous + calls > quotas.get(provider, 0) // 100
            ):
                raise ValueError("daily probe budget exhausted")
            counts[provider] = previous + calls
        temp = directory / ".daily-budget.new"
        temp.write_text(json.dumps({"day": day, "calls": counts}), encoding="utf-8")
        os.replace(temp, ledger)
    finally:
        lock.unlink()


def execute(probes, root, env, now, transport=fetch):
    run_id = (
        dt.datetime.fromtimestamp(now, dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        + "-"
        + os.urandom(4).hex()
    )
    rawdir = root / "evidence" / "probe" / "raw" / run_id
    fixtures = root / "test" / "fixtures"
    rawdir.mkdir(parents=True, exist_ok=False)
    fixtures.mkdir(parents=True, exist_ok=True)
    credentials = [
        v for k, v in env.items() if k in {n for ns in KEYS.values() for n in ns} and v
    ]
    issued_token, records, failed = None, [], False
    for probe in probes:
        row = {
            "provider": probe.provider,
            "scenario": probe.name,
            "http": 0,
            "outcome": "TransportError",
        }
        start = time.monotonic()
        try:
            if probe.provider == "kis" and probe.name == "price":
                if not issued_token:
                    row["outcome"] = "SkippedTokenFailure"
                    failed = True
                    records.append(row)
                    continue
                probe.headers = {
                    "authorization": "Bearer " + issued_token,
                    "appkey": env["KDASH_KIS_APP_KEY"],
                    "appsecret": env["KDASH_KIS_APP_SECRET"],
                    "tr_id": "FHKST01010100",
                    "custtype": "P",
                }
            status, content_type, raw = transport(probe)
            # Raw data is never placed in tracked fixtures, reports or stdout.
            (rawdir / (probe.provider + "-" + probe.name + ".bin")).write_bytes(raw)
            ctx = FetchContext(int(time.time()), int(time.time()), True)
            text = raw.decode("utf-8-sig")
            result = outcome(probe, status, text, ctx)
            if (
                probe.provider == "kis"
                and probe.name == "token"
                and result == Outcome.Ok
            ):
                issued_token = json.loads(text)["access_token"]
                credentials.append(issued_token)
            clean, ext = clean_body(raw, credentials)
            name = (
                "live_" + run_id + "_" + probe.provider + "_" + probe.name + "." + ext
            )
            (fixtures / name).write_text(clean + "\n", encoding="utf-8")
            media = content_type.split(";", 1)[0].strip().lower()
            safe_media = (
                media
                if media
                in (
                    "application/json",
                    "application/xml",
                    "text/xml",
                    "text/plain",
                    "text/html",
                )
                else "other"
            )
            row.update(
                http=status,
                content_type=safe_media,
                bytes=len(raw),
                outcome=result.name,
                fixture=name,
            )
            # NoData is valid capture evidence, but not successful functional validation.
            if (not probe.negative and result != Outcome.Ok) or (
                probe.negative and result == Outcome.Ok
            ):
                failed = True
        except (
            OSError,
            ValueError,
            KeyError,
            TypeError,
            AttributeError,
            http.client.HTTPException,
        ):
            failed = True  # Never include exception text: URLs can contain credentials.
        row["elapsed_ms"] = int((time.monotonic() - start) * 1000)
        records.append(row)
    report = {
        "run": run_id,
        "hardware_verified": False,
        "status": "INCOMPLETE" if failed else "CAPTURED",
        "note": "Captured cases only; midnight/release-delay/holiday assumptions require separate timed runs.",
        "records": records,
    }
    (root / "evidence" / "probe" / (run_id + ".json")).write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    return 1 if failed else 0


def main(argv=None, environ=None, root=ROOT, transport=fetch):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--live", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--provider", action="append", choices=tuple(KEYS))
    ap.add_argument("--daily-quota", action="append", default=[], metavar="PROVIDER=N")
    ap.add_argument(
        "--negative-tests",
        action="store_true",
        help="one bogus station/key case; never quota/rate flooding",
    )
    ap.add_argument("--station", default="종로구")
    ap.add_argument("--nx", type=int, default=60)
    ap.add_argument("--ny", type=int, default=127)
    ap.add_argument("--symbol", default="005930")
    args = ap.parse_args(argv)
    selected = list(dict.fromkeys(args.provider or []))
    if args.dry_run:
        print(
            "PLAN ONLY: kma current/previous + forecast 72/1000; air station; exim 3 dates; fsc close; kis token+price; holiday current/next year"
        )
        print(
            "Timed midnight/release-delay/holiday checks require separate runs; no validation claimed."
        )
        return 0
    env = os.environ if environ is None else environ
    if (
        not args.live
        or not selected
        or any(not env.get(k) for p in selected for k in KEYS[p])
    ):
        print(
            "BLOCKED_CREDENTIALS: --live, selected providers and their environment keys are required"
        )
        return 2
    try:
        if policy.validate_location(
            args.nx, args.ny, "probe", args.station
        ) != "OK" or not finance.is_valid_symbol(args.symbol):
            raise ValueError("invalid location/symbol")
        quotas = {
            k: int(v) for k, v in (part.split("=", 1) for part in args.daily_quota)
        }
        probes = plan(
            selected,
            env,
            int(time.time()),
            args.station,
            args.nx,
            args.ny,
            args.symbol,
            args.negative_tests,
        )
        if any(
            quotas.get(p, 0) // 100 < sum(x.provider == p for x in probes)
            for p in selected
        ):
            raise ValueError("declared quota insufficient")
        reserve_budget(Path(root), probes, quotas, int(time.time()))
        return execute(probes, Path(root), env, int(time.time()), transport)
    except (OSError, ValueError, KeyError, TypeError, AttributeError):
        print(
            "BLOCKED_CONFIG: check inputs, daily quotas or output permissions; no details logged"
        )
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
