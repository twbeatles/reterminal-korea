"""Result-code mapping + body sniffing mirror (src/providers/ResultCodes.*)."""

from __future__ import annotations

import json
import re

from .models import Outcome

_AUTH = {"20", "21", "30", "31", "32", "33"}


def data_go_kr(code) -> Outcome:
    if not code:
        return Outcome.ProviderError
    if code in ("00", "0"):
        return Outcome.Ok
    if code == "03":
        return Outcome.NoData
    if code == "22":
        return Outcome.QuotaExceeded
    if code in _AUTH:
        return Outcome.AuthError
    return Outcome.ProviderError


def exim(result: int) -> Outcome:
    return {1: Outcome.Ok, 3: Outcome.AuthError, 4: Outcome.QuotaExceeded}.get(
        result, Outcome.ProviderError
    )


def kis_token_invalid(http_status: int, msg_cd) -> bool:
    return http_status == 401 or msg_cd in ("EGW00123", "EGW00121")


def kis(http_status: int, rt_cd, msg_cd) -> Outcome:
    if kis_token_invalid(http_status, msg_cd):
        return Outcome.AuthError
    if http_status == 429 or msg_cd in ("EGW00201", "EGW00133"):
        return Outcome.RateLimited
    if http_status == 403:
        return Outcome.AuthError
    if http_status != 200:
        return Outcome.ProviderError
    return Outcome.Ok if rt_cd == "0" else Outcome.ProviderError


def looks_like_xml(body: str) -> bool:
    return body.lstrip("﻿").lstrip().startswith("<")


def xml_tag_text(body: str, tag: str):
    m = re.search(r"<%s>([^<]*)</" % re.escape(tag), body)
    return m.group(1).strip() if m else None


def text(v):
    """JSON string or number -> text (None for null/other), like the adapter."""
    if v is None or isinstance(v, bool):
        return None
    if isinstance(v, str):
        return v
    if isinstance(v, int):
        return str(v)
    if isinstance(v, float):
        return "%.6g" % v
    return None


def load_data_go_kr(body):
    """-> (code, is_xml, root|None). root: the 'response' object."""
    if not body:
        return None, True, None
    if looks_like_xml(body):
        return (
            (
                xml_tag_text(body, "returnReasonCode")
                or xml_tag_text(body, "resultCode")
            ),
            True,
            None,
        )
    try:
        doc = json.loads(body)
    except ValueError:
        return None, False, None
    if not isinstance(doc, dict):
        return None, False, None
    root = doc.get("response", doc)
    header = root.get("header") or {}
    return text(header.get("resultCode")), False, root


def items_of(node) -> list:
    """items.item may be a list or a single object; '' when empty."""
    if isinstance(node, list):
        return [x for x in node if isinstance(x, dict)][:96]
    if isinstance(node, dict):
        return [node]
    return []
