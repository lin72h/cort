#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def load_results(path: Path) -> tuple[str | None, dict[str, dict]]:
    data = load_json(path)
    probe = data.get("probe")
    results = data.get("results")
    if not isinstance(results, list):
        raise ValueError(f"{path} does not contain a top-level results array")
    by_name: dict[str, dict] = {}
    for entry in results:
        name = entry.get("name")
        if not isinstance(name, str) or not name:
            raise ValueError(f"{path} contains a result without a valid name")
        if name in by_name:
            raise ValueError(f"{path} contains duplicate result name {name!r}")
        by_name[name] = entry
    return probe if isinstance(probe, str) else None, by_name


def render_report(actual_path: Path, expected_path: Path) -> tuple[str, int, int]:
    expected = load_json(expected_path)
    expected_probe = expected.get("probe")
    expected_cases = expected.get("cases")
    if not isinstance(expected_cases, list):
        raise ValueError(f"{expected_path} does not contain a top-level cases array")

    actual_probe, actual_results = load_results(actual_path)

    blockers = 0
    warnings = 0
    lines: list[str] = []
    lines.append("# Subset 0 Runtime Ownership Report")
    lines.append("")
    lines.append(f"- actual JSON: `{actual_path}`")
    lines.append(f"- expected manifest: `{expected_path}`")
    if expected_probe is not None:
        lines.append(f"- expected probe: `{expected_probe}`")
    if actual_probe is not None:
        lines.append(f"- actual probe: `{actual_probe}`")
    lines.append("")

    if expected_probe is not None and actual_probe is not None and expected_probe != actual_probe:
        warnings += 1
        lines.append(f"Probe name mismatch: expected `{expected_probe}`, actual `{actual_probe}`.")
        lines.append("")

    lines.append("| Case | Status | Notes |")
    lines.append("| --- | --- | --- |")

    expected_names: set[str] = set()
    for case in expected_cases:
        name = case.get("name")
        if not isinstance(name, str) or not name:
            raise ValueError(f"{expected_path} contains a case without a valid name")
        expected_names.add(name)
        classification = case.get("classification")
        fx_match_level = case.get("fx_match_level")
        blocking = bool(case.get("blocking"))

        actual = actual_results.get(name)
        status = "match"
        notes: list[str] = []

        if actual is None:
            notes.append("missing from actual results")
            if blocking:
                status = "BLOCKER"
                blockers += 1
            else:
                status = "warning"
                warnings += 1
            lines.append(f"| `{name}` | {status} | {'; '.join(notes)} |")
            continue

        actual_classification = actual.get("classification")
        actual_match_level = actual.get("fx_match_level")
        success = actual.get("success")

        if actual_classification != classification:
            notes.append(f"classification differs: expected {classification!r}, actual {actual_classification!r}")
            if status == "match":
                status = "warning"
                warnings += 1

        if actual_match_level != fx_match_level:
            notes.append(f"fx_match_level differs: expected {fx_match_level!r}, actual {actual_match_level!r}")
            if status == "match":
                status = "warning"
                warnings += 1

        if success is not True:
            notes.append(f"success is {success!r}")
            if blocking:
                if status != "BLOCKER":
                    status = "BLOCKER"
                    blockers += 1
            elif status != "warning":
                status = "warning"
                warnings += 1

        if status == "match" and not notes:
            notes.append("present with expected classification and success")

        lines.append(f"| `{name}` | {status} | {'; '.join(notes)} |")

    extra_names = sorted(set(actual_results) - expected_names)
    for name in extra_names:
        warnings += 1
        lines.append(f"| `{name}` | warning | extra case present in actual results |")

    lines.append("")
    lines.append(f"- blockers: {blockers}")
    lines.append(f"- warnings: {warnings}")
    if blockers == 0:
        lines.append("- verdict: no blocking issues found against the current Subset 0 runtime-ownership manifest")
    else:
        lines.append("- verdict: blocking issues found; required runtime-ownership rows need review before advancing")
    lines.append("")
    return "\n".join(lines), blockers, warnings


def main() -> int:
    parser = argparse.ArgumentParser(description="Report MX runtime-ownership results against the expected Subset 0 case manifest.")
    parser.add_argument("--json", required=True, help="Path to the runtime-ownership JSON artifact")
    parser.add_argument("--expected", required=True, help="Path to the expected-case manifest JSON")
    parser.add_argument("--json-label", help="Optional label to render instead of the runtime-ownership JSON path")
    parser.add_argument("--expected-label", help="Optional label to render instead of the expected manifest path")
    parser.add_argument("--output", help="Optional path for the markdown report")
    args = parser.parse_args()

    actual_path = Path(args.json)
    expected_path = Path(args.expected)
    report, blockers, _warnings = render_report(actual_path, expected_path)
    if args.json_label:
        report = report.replace(f"`{actual_path}`", f"`{args.json_label}`", 1)
    if args.expected_label:
        report = report.replace(f"`{expected_path}`", f"`{args.expected_label}`", 1)

    if args.output:
        output_path = Path(args.output)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(report, encoding="utf-8")
    else:
        sys.stdout.write(report)

    return 1 if blockers else 0


if __name__ == "__main__":
    sys.exit(main())
