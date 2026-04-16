#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


BLOCKING_CLASSIFICATIONS = {"required", "required-but-error-text-flexible"}
BOOLEAN_SEMANTIC_FIELDS = {"pointer_identity_same"}
NOTE_FIELDS = {
    "type_id",
    "retain_count_before",
    "retain_count_after_retain",
    "retain_count_after_release",
    "retain_callback_count",
    "release_callback_count",
}


def load_results(path: Path) -> dict[str, dict]:
    data = json.loads(path.read_text(encoding="utf-8"))
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
    return by_name


def is_blocking(classification: str | None) -> bool:
    return classification in BLOCKING_CLASSIFICATIONS


def render_report(
    fx_results: dict[str, dict],
    mx_results: dict[str, dict],
    fx_label: str,
    mx_label: str,
) -> tuple[str, int, int]:
    blockers = 0
    warnings = 0
    lines: list[str] = []
    lines.append("# Subset 0 FX vs MX Comparison")
    lines.append("")
    lines.append(f"- FX input: `{fx_label}`")
    lines.append(f"- MX input: `{mx_label}`")
    lines.append("")

    all_names = sorted(set(fx_results) | set(mx_results))
    if not all_names:
        lines.append("No cases were found in either input.")
        return "\n".join(lines) + "\n", blockers, warnings

    lines.append("| Case | Status | Notes |")
    lines.append("| --- | --- | --- |")

    for name in all_names:
        fx_case = fx_results.get(name)
        mx_case = mx_results.get(name)
        notes: list[str] = []
        status = "match"

        if fx_case is None or mx_case is None:
            present = fx_case if fx_case is not None else mx_case
            present_side = "FX" if fx_case is not None else "MX"
            missing_side = "MX" if fx_case is not None else "FX"
            classification = present.get("classification") if isinstance(present, dict) else None
            notes.append(f"missing on {missing_side}; present on {present_side} as {classification or 'unknown'}")
            if is_blocking(classification):
                status = "BLOCKER"
                blockers += 1
            else:
                status = "warning"
                warnings += 1
            lines.append(f"| `{name}` | {status} | {'; '.join(notes)} |")
            continue

        fx_class = fx_case.get("classification")
        mx_class = mx_case.get("classification")
        blocking_case = is_blocking(fx_class) or is_blocking(mx_class)

        if fx_class != mx_class:
            notes.append(f"classification differs: FX={fx_class!r} MX={mx_class!r}")
            if status != "BLOCKER":
                status = "warning"
                warnings += 1

        fx_success = fx_case.get("success")
        mx_success = mx_case.get("success")
        if fx_success != mx_success:
            notes.append(f"success differs: FX={fx_success!r} MX={mx_success!r}")
            if blocking_case:
                status = "BLOCKER"
                blockers += 1
            elif status != "warning":
                status = "warning"
                warnings += 1

        for field in sorted(BOOLEAN_SEMANTIC_FIELDS):
            fx_value = fx_case.get(field)
            mx_value = mx_case.get(field)
            if fx_value is None or mx_value is None:
                continue
            if fx_value != mx_value:
                notes.append(f"{field} differs: FX={fx_value!r} MX={mx_value!r}")
                if blocking_case:
                    if status != "BLOCKER":
                        status = "BLOCKER"
                        blockers += 1
                elif status != "warning":
                    status = "warning"
                    warnings += 1

        for field in sorted(NOTE_FIELDS):
            fx_value = fx_case.get(field)
            mx_value = mx_case.get(field)
            if fx_value is None or mx_value is None:
                continue
            if fx_value != mx_value:
                notes.append(f"{field} differs: FX={fx_value!r} MX={mx_value!r} (diagnostic)")

        if status == "match" and not notes:
            notes.append("semantically aligned on shared blocking fields")

        lines.append(f"| `{name}` | {status} | {'; '.join(notes)} |")

    lines.append("")
    lines.append(f"- blockers: {blockers}")
    lines.append(f"- warnings: {warnings}")
    if blockers == 0:
        lines.append("- verdict: no blocking mismatches found in the shared comparison surface")
    else:
        lines.append("- verdict: blocking mismatches found; do not treat FX as semantically aligned for the affected required rows")
    lines.append("")
    return "\n".join(lines), blockers, warnings


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare FX and MX Subset 0 JSON results.")
    parser.add_argument("--fx-json", required=True, help="Path to the FX JSON artifact")
    parser.add_argument("--mx-json", required=True, help="Path to the MX JSON artifact")
    parser.add_argument("--fx-label", help="Optional label to render instead of the FX JSON path")
    parser.add_argument("--mx-label", help="Optional label to render instead of the MX JSON path")
    parser.add_argument("--output", help="Optional path for the markdown report")
    args = parser.parse_args()

    fx_path = Path(args.fx_json)
    mx_path = Path(args.mx_json)

    fx_results = load_results(fx_path)
    mx_results = load_results(mx_path)
    report, blockers, _warnings = render_report(
        fx_results,
        mx_results,
        args.fx_label or str(fx_path),
        args.mx_label or str(mx_path),
    )

    if args.output:
        output_path = Path(args.output)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(report, encoding="utf-8")
    else:
        sys.stdout.write(report)

    return 1 if blockers else 0


if __name__ == "__main__":
    sys.exit(main())
