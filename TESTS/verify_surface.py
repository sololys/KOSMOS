"""
KOSMOS — Surface, Zero-Float, Zero-Leak, Markdown Plate, and SHA-256 Manifest Verifier.
SPDX-License-Identifier: Apache-2.0
"""

import argparse
import hashlib
import pathlib
import re
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
MANIFEST_NAME = "MANIFEST.sha256"

# Constructed by concatenation so zero literal private path tokens exist even in this file
FORBIDDEN_PARTS = [
    "/" + "home" + "/" + "sololyset",
    "AGENTS" + ".md",
    "00_" + "INTERKOGNITIV",
    "03_" + "ENGINES",
    "04_" + "REALITY_FORGE",
    "05_" + "PATENTS",
    "06_" + "RESEARCH",
    "07_" + "BOKHYLLA",
    "09_" + "SYSTEM",
    "CARBON" + "/",
    "Gemini" + "-Core",
]

FLOAT_C_SV_RE = re.compile(r"\b(float|double|shortreal|realtime)\b")
ROYAL_WE_RE = re.compile(r"\b(vi|oss|vår|vårt|våre)\b", re.IGNORECASE)
INLINE_MATH_RE = re.compile(r"(?<!\$)\$(?!\$)([^$\n]+?)\$(?!\$)")


def iter_repo_files():
    for p in sorted(REPO_ROOT.rglob("*")):
        if not p.is_file():
            continue
        rel = p.relative_to(REPO_ROOT).as_posix()
        if rel.startswith(".git/") or rel.startswith("build/") or "__pycache__" in rel:
            continue
        if rel == MANIFEST_NAME:
            continue
        yield p, rel


def strip_c_sv_comments(src: str) -> str:
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.DOTALL)
    src = re.sub(r"//[^\n]*", " ", src)
    src = re.sub(r'"(?:\\.|[^"\\])*"', '""', src)
    return src


def check_zero_float() -> None:
    for p, rel in iter_repo_files():
        if p.suffix in (".c", ".h", ".sv"):
            cleaned = strip_c_sv_comments(p.read_text(encoding="utf-8"))
            m = FLOAT_C_SV_RE.search(cleaned)
            if m:
                raise AssertionError(f"0-float violation in {rel}: token '{m.group(1)}'")


def check_zero_leaks() -> None:
    for p, rel in iter_repo_files():
        text = p.read_text(encoding="utf-8")
        for needle in FORBIDDEN_PARTS:
            if needle in text:
                raise AssertionError(f"Private path/token leak in {rel}: '{needle}'")


def check_markdown_discipline() -> None:
    for p, rel in iter_repo_files():
        if p.suffix != ".md":
            continue
        text = p.read_text(encoding="utf-8")
        lines = text.splitlines()

        if "\t" in text:
            raise AssertionError(f"Tab character found in {rel}")

        in_text_block = False
        box_widths: set[int] = set()
        prose_lines: list[str] = []
        in_fence = False

        for lineno, line in enumerate(lines, start=1):
            stripped = line.strip()
            if stripped.startswith("```"):
                if not in_fence:
                    in_fence = True
                    in_text_block = stripped == "```text"
                    box_widths = set()
                else:
                    in_fence = False
                    in_text_block = False
                continue

            if in_fence:
                if in_text_block:
                    if line.endswith(" "):
                        raise AssertionError(f"Trailing space inside ```text block at {rel}:{lineno}")
                    if (line.startswith("+") and line.endswith("+")) or (
                        line.startswith("|") and line.endswith("|")
                    ):
                        box_widths.add(len(line))
                        if len(box_widths) > 1:
                            raise AssertionError(
                                f"Ragged ASCII box width at {rel}:{lineno} (widths={sorted(box_widths)})"
                            )
                continue

            prose_lines.append(line)

            # GFM MathJax closing-$ collision check (strip inline code spans and $$...$$ first)
            no_code = re.sub(r"`[^`]*`", "", line)
            no_display = re.sub(r"\$\$[^$]+\$\$", " ", no_code)
            if not stripped.startswith("$$"):
                for m in INLINE_MATH_RE.finditer(no_display):
                    end_idx = m.end()
                    if end_idx < len(no_display) and re.match(r"[A-Za-z0-9\-]", no_display[end_idx]):
                        raise AssertionError(
                            f"cmark-gfm MathJax collision at {rel}:{lineno}: '{m.group(0)}{no_display[end_idx]}'"
                        )

            # Raw pipe inside $...$ inside Markdown table row
            if stripped.startswith("|") and "$" in no_display:
                for m in INLINE_MATH_RE.finditer(no_display):
                    seg = m.group(1)
                    if "|" in seg:
                        raise AssertionError(f"Raw '|' inside table math at {rel}:{lineno}: '${seg}$'")

        prose_joined = "\n".join(prose_lines)
        m_we = ROYAL_WE_RE.search(prose_joined)
        if m_we:
            raise AssertionError(f"Institutional plural '{m_we.group(0)}' found in {rel}")


def compute_manifest_lines() -> list[str]:
    out = []
    for p, rel in iter_repo_files():
        digest = hashlib.sha256(p.read_bytes()).hexdigest()
        out.append(f"{digest}  {rel}")
    return out


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--write-manifest", action="store_true")
    parser.add_argument("--verify-manifest", action="store_true")
    args = parser.parse_args()

    check_zero_float()
    check_zero_leaks()
    check_markdown_discipline()

    manifest_path = REPO_ROOT / MANIFEST_NAME
    expected_lines = compute_manifest_lines()
    expected_text = "\n".join(expected_lines) + "\n"

    if args.write_manifest:
        manifest_path.write_text(expected_text, encoding="utf-8")
        print(f"[PASS] Wrote {MANIFEST_NAME} ({len(expected_lines)} files sealed).")

    if args.verify_manifest:
        if not manifest_path.exists():
            raise AssertionError(f"Missing {MANIFEST_NAME}; run 'make manifest' first.")
        actual = manifest_path.read_text(encoding="utf-8")
        if actual != expected_text:
            raise AssertionError(f"{MANIFEST_NAME} out of sync with repository contents.")
        print(f"[PASS] Surface, 0-float, 0-leak, Markdown plates, and {MANIFEST_NAME} ({len(expected_lines)} files) verified.")


if __name__ == "__main__":
    try:
        main()
    except AssertionError as exc:
        print(f"[SURFACE FAIL] {exc}", file=sys.stderr)
        sys.exit(1)
