from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, asdict
from datetime import datetime
from pathlib import Path

FALLBACK_TEXT = "ごめん、出力形式が崩れたので返答を作り直すね。もう一度だけ同じ内容を送って。"


@dataclass
class RunResult:
    template: str
    run_index: int
    exit_code: int
    classification: str
    ai_text: str
    stdout_file: str
    runtime_log_file: str | None


def extract_ai_text(output: str) -> str:
    marker = "AI:"
    idx = output.rfind(marker)
    if idx < 0:
        return ""
    return output[idx + len(marker):].strip()


def classify_ai_text(ai_text: str, template_name: str) -> str:
    if not ai_text:
        return "empty"
    if FALLBACK_TEXT in ai_text:
        return "fallback"

    lowered = ai_text.lower()
    leak_keywords = [
        "# ",
        "出力契約",
        "assistant_response",
        "tool_call",
        "response phase",
        "tool phase",
        "あなたの人格憲法",
    ]
    has_leak = any(k in lowered for k in leak_keywords)

    instruction_echo_keywords = [
        "終了してください",
        "最終応答を生成",
        "出力しないでください",
        "ブロック1つ",
        "ここで終了",
        "繰り返しは禁止",
        "ユーザーに返す",
        "再出力",
    ]
    has_instruction_echo = any(k in ai_text for k in instruction_echo_keywords)

    template_lower = template_name.lower()
    is_tool_template = "tool" in template_lower
    is_response_template = "response" in template_lower

    tool_call_pattern = re.compile(
        r"^\s*<tool_call>\s*name:\s*[^\n]+\ninput:\s*[\s\S]*?</tool_call>\s*$",
        re.IGNORECASE,
    )
    assistant_response_pattern = re.compile(
        r"^\s*<assistant_response>\s*[\s\S]*?</assistant_response>\s*$",
        re.IGNORECASE,
    )

    if is_tool_template:
        if tool_call_pattern.match(ai_text):
            return "tool_call_valid"
        if has_leak:
            return "tool_invalid_prompt_leak_or_contract_echo"
        return "tool_invalid_not_tool_call"

    if is_response_template:
        if assistant_response_pattern.match(ai_text):
            return "response_assistant_block_valid"

        if "<tool_call>" in lowered or "</tool_call>" in lowered:
            return "response_invalid_tool_call_markup"

        if has_leak:
            return "response_invalid_prompt_leak_or_contract_echo"

        if has_instruction_echo:
            return "response_invalid_instruction_echo"

        if "<" in ai_text and ">" in ai_text:
            return "response_invalid_markup_mixed"

        return "response_natural_text_valid"

    if has_leak:
        return "prompt_leak_or_contract_echo"

    if has_instruction_echo:
        return "instruction_echo"

    return "normal"


def is_success_classification(template_name: str, classification: str) -> bool:
    name = template_name.lower()
    if "tool" in name:
        return classification == "tool_call_valid"
    if "response" in name:
        return classification in {"response_assistant_block_valid", "response_natural_text_valid"}
    return classification in {"normal", "response_natural_text_valid", "tool_call_valid"}


def extract_tool_call_name(ai_text: str) -> str:
    m = re.search(r"<tool_call>\s*name:\s*([^\n]+)", ai_text, flags=re.IGNORECASE)
    if not m:
        return ""
    return m.group(1).strip()


def run_once_to_file(
    exe_path: Path,
    prompt_path: Path,
    cwd: Path,
    timeout_sec: int,
    output_file: Path,
    tool_phase_only: bool,
    response_phase_only: bool,
    model_path: str,
) -> tuple[int, bool]:
    if tool_phase_only:
        mode_arg = "--once-system-prompt-tool-phase"
    elif response_phase_only:
        mode_arg = "--once-system-prompt-response-phase"
    else:
        mode_arg = "--once-system-prompt"
    cmd = [str(exe_path), mode_arg, str(prompt_path)]

    with output_file.open("w", encoding="utf-8", errors="replace", newline="") as out:
        env = dict(**__import__("os").environ)
        if model_path.strip():
            env["LLMAPP_MODEL_PATH"] = model_path.strip()
        process = subprocess.Popen(
            cmd,
            cwd=str(cwd),
            stdout=out,
            stderr=subprocess.STDOUT,
            env=env,
            text=True,
            encoding="utf-8",
            errors="replace",
        )

        timed_out = False
        try:
            process.wait(timeout=timeout_sec)
        except subprocess.TimeoutExpired:
            timed_out = True
            process.kill()
            process.wait()
            out.write("\n[TIMEOUT]\n")

        return process.returncode if process.returncode is not None else 124, timed_out


def main() -> int:
    parser = argparse.ArgumentParser(description="Run one-shot stability tests for prompt templates.")
    parser.add_argument("--repeats", type=int, default=3, help="Run count per template (default: 3)")
    parser.add_argument("--timeout", type=int, default=300, help="Timeout seconds per run (default: 300)")
    parser.add_argument(
        "--response-template",
        type=str,
        default="Prompt_operational_response_qwen.md",
        help="Response template filename under promptTest/",
    )
    parser.add_argument(
        "--tool-template",
        type=str,
        default="Prompt_operational_tool_qwen.md",
        help="Tool template filename under promptTest/",
    )
    parser.add_argument(
        "--expected-tool-name",
        type=str,
        default="get_current_time",
        help="Expected tool name for tool template validation (default: get_current_time)",
    )
    parser.add_argument(
        "--model-path",
        type=str,
        default="",
        help="Optional model path passed via LLMAPP_MODEL_PATH",
    )
    args = parser.parse_args()

    workspace = Path(__file__).resolve().parents[1]
    prompt_dir = workspace / "promptTest"
    exe_path = workspace / "LLMapp" / "x64" / "Debug" / "LLMapp.exe"
    cwd = exe_path.parent

    templates = [
        prompt_dir / args.response_template,
        prompt_dir / args.tool_template,
    ]

    if not exe_path.exists():
        print(f"ERROR: executable not found: {exe_path}")
        return 1

    for path in templates:
        if not path.exists():
            print(f"ERROR: template not found: {path}")
            return 1

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_dir = prompt_dir / "oneshot_runs" / timestamp
    out_dir.mkdir(parents=True, exist_ok=True)

    results: list[RunResult] = []

    print(f"[runner] output dir: {out_dir}")
    print(f"[runner] executable: {exe_path}")
    if args.model_path:
        print(f"[runner] model path override: {args.model_path}")

    for template in templates:
        template_name = template.stem
        print(f"\n[runner] template: {template_name}")

        for i in range(1, args.repeats + 1):
            print(f"  - run {i}/{args.repeats} ...", end="", flush=True)

            stdout_file = out_dir / f"{template_name}_run{i:02d}.txt"
            tool_phase_only = "tool" in template_name.lower()
            response_phase_only = "response" in template_name.lower()
            exit_code, timed_out = run_once_to_file(
                exe_path,
                template,
                cwd,
                args.timeout,
                stdout_file,
                tool_phase_only,
                response_phase_only,
                args.model_path,
            )

            merged = stdout_file.read_text(encoding="utf-8", errors="replace")
            if timed_out:
                exit_code = 124

            ai_text = extract_ai_text(merged)
            classification = classify_ai_text(ai_text, template_name)
            if classification == "tool_call_valid" and "tool" in template_name.lower():
                tool_name = extract_tool_call_name(ai_text)
                if tool_name and tool_name != args.expected_tool_name:
                    classification = f"tool_call_unexpected_name({tool_name})"

            runtime_src = prompt_dir / "_oneshot_runtime.log"
            runtime_dst = out_dir / f"{template_name}_run{i:02d}_runtime.log"
            runtime_log_file: str | None = None
            if runtime_src.exists():
                shutil.copyfile(runtime_src, runtime_dst)
                runtime_log_file = str(runtime_dst)

            results.append(
                RunResult(
                    template=template_name,
                    run_index=i,
                    exit_code=exit_code,
                    classification=classification,
                    ai_text=ai_text,
                    stdout_file=str(stdout_file),
                    runtime_log_file=runtime_log_file,
                )
            )

            print(f" done ({classification}, exit={exit_code})")

    summary = defaultdict(lambda: {
        "total": 0,
        "success": 0,
        "classifications": Counter(),
        "exit_codes": Counter(),
        "samples": [],
    })
    for r in results:
        s = summary[r.template]
        s["total"] += 1
        if is_success_classification(r.template, r.classification):
            s["success"] += 1
        s["classifications"][r.classification] += 1
        s["exit_codes"][str(r.exit_code)] += 1
        if len(s["samples"]) < 3:
            s["samples"].append(r.ai_text[:160])

    summary_json_path = out_dir / "summary.json"
    summary_md_path = out_dir / "summary.md"

    serializable_summary = {
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "repeats": args.repeats,
        "executable": str(exe_path),
        "model_path_override": args.model_path,
        "templates": {k: {
            "total": v["total"],
            "success": v["success"],
            "success_rate": round((v["success"] / v["total"] * 100.0), 1) if v["total"] else 0.0,
            "classifications": dict(v["classifications"]),
            "exit_codes": dict(v["exit_codes"]),
            "samples": v["samples"],
        } for k, v in summary.items()},
        "results": [asdict(r) for r in results],
    }

    summary_json_path.write_text(json.dumps(serializable_summary, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# One-shot Stability Summary",
        "",
        f"- Generated: {serializable_summary['generated_at']}",
        f"- Repeats per template: {args.repeats}",
        f"- Executable: {exe_path}",
        f"- Model path override: {args.model_path if args.model_path else '(default)'}",
        f"- Expected tool name: {args.expected_tool_name}",
        "",
    ]

    for template_name, data in serializable_summary["templates"].items():
        lines.append(f"## {template_name}")
        lines.append(f"- total: {data['total']}")
        lines.append(f"- success: {data['success']} ({data['success_rate']}%)")
        lines.append(f"- classifications: {data['classifications']}")
        lines.append(f"- exit_codes: {data['exit_codes']}")
        lines.append("- samples:")
        for sample in data["samples"]:
            safe_sample = re.sub(r"\s+", " ", sample).strip()
            lines.append(f"  - {safe_sample}")
        lines.append("")

    summary_md_path.write_text("\n".join(lines), encoding="utf-8")

    print("\n[runner] completed")
    print(f"[runner] summary: {summary_md_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
