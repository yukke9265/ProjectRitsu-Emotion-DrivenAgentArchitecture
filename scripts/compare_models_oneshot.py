from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path


def run_runner(
    python_exe: str,
    runner_path: Path,
    workspace: Path,
    model_path: Path,
    repeats: int,
    timeout: int,
    response_template: str,
    tool_template: str,
    expected_tool_name: str,
) -> Path:
    cmd = [
        python_exe,
        str(runner_path),
        "--repeats",
        str(repeats),
        "--timeout",
        str(timeout),
        "--response-template",
        response_template,
        "--tool-template",
        tool_template,
        "--expected-tool-name",
        expected_tool_name,
        "--model-path",
        str(model_path),
    ]

    completed = subprocess.run(
        cmd,
        cwd=str(workspace),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )

    output = completed.stdout + ("\n" + completed.stderr if completed.stderr else "")
    if completed.returncode != 0:
        raise RuntimeError(f"runner failed for {model_path.name}:\n{output}")

    m = re.search(r"\[runner\] output dir: (.+)", output)
    if not m:
        raise RuntimeError(f"cannot detect output dir for {model_path.name}:\n{output}")

    return Path(m.group(1).strip())


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare one-shot responses across models")
    parser.add_argument("--repeats", type=int, default=1, help="Runs per template per model")
    parser.add_argument("--timeout", type=int, default=60, help="Timeout per run")
    parser.add_argument("--response-template", type=str, default="Template_response_minimal_v4.md")
    parser.add_argument("--tool-template", type=str, default="Template_tool_minimal_v3.md")
    parser.add_argument("--expected-tool-name", type=str, default="get_current_time")
    parser.add_argument("--models", type=str, nargs="*", default=[])
    args = parser.parse_args()

    workspace = Path(__file__).resolve().parents[1]
    models_dir = workspace / "models"
    runner_path = workspace / "scripts" / "oneshot_stability_runner.py"
    python_exe = sys.executable

    if args.models:
        model_paths = [Path(m) for m in args.models]
    else:
        model_paths = sorted(models_dir.glob("*.gguf"))

    if not model_paths:
        print("ERROR: no models found")
        return 1

    compare_rows: list[dict] = []

    print(f"[compare] models: {len(model_paths)}")
    for model in model_paths:
        print(f"[compare] running: {model.name}")
        out_dir = run_runner(
            python_exe,
            runner_path,
            workspace,
            model,
            args.repeats,
            args.timeout,
            args.response_template,
            args.tool_template,
            args.expected_tool_name,
        )

        summary_path = out_dir / "summary.json"
        data = json.loads(summary_path.read_text(encoding="utf-8"))

        templates = data.get("templates", {})
        response_key = Path(args.response_template).stem
        tool_key = Path(args.tool_template).stem
        response_info = templates.get(response_key, {})
        tool_info = templates.get(tool_key, {})

        compare_rows.append(
            {
                "model": str(model),
                "summary_dir": str(out_dir),
                "response_success_rate": response_info.get("success_rate"),
                "response_classifications": response_info.get("classifications"),
                "response_sample": (response_info.get("samples") or [""])[0],
                "tool_success_rate": tool_info.get("success_rate"),
                "tool_classifications": tool_info.get("classifications"),
                "tool_sample": (tool_info.get("samples") or [""])[0],
            }
        )

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_base = workspace / "promptTest" / f"model_compare_{timestamp}"
    out_json = out_base.with_suffix(".json")
    out_md = out_base.with_suffix(".md")

    out_json.write_text(json.dumps(compare_rows, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# Model One-shot Comparison",
        "",
        f"- Generated: {datetime.now().isoformat(timespec='seconds')}",
        f"- Repeats: {args.repeats}",
        f"- Response template: {args.response_template}",
        f"- Tool template: {args.tool_template}",
        f"- Expected tool: {args.expected_tool_name}",
        "",
        "| Model | Response success | Tool success |",
        "|---|---:|---:|",
    ]

    for row in compare_rows:
        lines.append(
            f"| {Path(row['model']).name} | {row['response_success_rate']}% | {row['tool_success_rate']}% |"
        )

    lines.append("\n## Details\n")
    for row in compare_rows:
        lines.append(f"### {Path(row['model']).name}")
        lines.append(f"- summary: {row['summary_dir']}")
        lines.append(f"- response_classifications: {row['response_classifications']}")
        lines.append(f"- tool_classifications: {row['tool_classifications']}")
        lines.append(f"- response_sample: {re.sub(r'\\s+', ' ', row['response_sample']).strip()}")
        lines.append(f"- tool_sample: {re.sub(r'\\s+', ' ', row['tool_sample']).strip()}")
        lines.append("")

    out_md.write_text("\n".join(lines), encoding="utf-8")

    print(f"[compare] done: {out_md}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
