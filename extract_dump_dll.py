#!/usr/bin/env python3
"""
Split an Il2CppDumper dump.cs by DLL.

Examples:
  python extract_dump_dll.py dump.cs --dll Assembly-CSharp.dll -o split_dump
  python extract_dump_dll.py dump.cs --dll Gameplay -o split_dump
  python extract_dump_dll.py dump.cs --all -o split_dump
  python extract_dump_dll.py dump.cs --list
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Set, Tuple


DLL_LINE_RE = re.compile(r"^//\s*Dll\s*:\s*(.*?)\s*$")
INVALID_FILENAME_RE = re.compile(r'[<>:"/\\|?*\x00-\x1f]')


def detect_encoding(path: Path) -> str:
    start = path.read_bytes()[:4]
    if start.startswith(b"\xef\xbb\xbf"):
        return "utf-8-sig"
    if start.startswith(b"\xff\xfe") or start.startswith(b"\xfe\xff"):
        return "utf-16"

    for encoding in ("utf-8-sig", "utf-8", "gb18030"):
        try:
            with path.open("r", encoding=encoding, newline="") as handle:
                handle.read(8192)
            return encoding
        except UnicodeDecodeError:
            continue

    return "utf-8"


def iter_dump_blocks(path: Path, encoding: str) -> Iterable[Tuple[str, List[str]]]:
    current_dll: Optional[str] = None
    block: List[str] = []

    with path.open("r", encoding=encoding, newline="") as handle:
        for line in handle:
            match = DLL_LINE_RE.match(line)
            if match:
                if current_dll is not None:
                    yield current_dll, block
                current_dll = match.group(1).strip()
                block = [line]
                continue

            if current_dll is not None:
                block.append(line)

    if current_dll is not None:
        yield current_dll, block


def normalize_name(name: str) -> str:
    lowered = name.casefold()
    if lowered.endswith(".dll"):
        return lowered[:-4]
    return lowered


def wanted_match(dll_name: str, wanted: Set[str]) -> bool:
    normalized = normalize_name(dll_name)
    return dll_name.casefold() in wanted or normalized in wanted


def safe_filename(dll_name: str) -> str:
    cleaned = INVALID_FILENAME_RE.sub("_", dll_name).strip().rstrip(". ")
    return cleaned or "unnamed_dll"


def build_output_path(
    output_dir: Path,
    dll_name: str,
    paths_by_dll: Dict[str, Path],
    used_names: Set[str],
) -> Path:
    key = dll_name.casefold()
    if key in paths_by_dll:
        return paths_by_dll[key]

    stem = safe_filename(dll_name)
    filename = f"{stem}.cs"
    candidate_key = filename.casefold()
    index = 2
    while candidate_key in used_names:
        filename = f"{stem}_{index}.cs"
        candidate_key = filename.casefold()
        index += 1

    used_names.add(candidate_key)
    path = output_dir / filename
    paths_by_dll[key] = path
    return path


def collect_dll_names(dump_path: Path, encoding: str) -> List[str]:
    seen: Set[str] = set()
    names: List[str] = []
    for dll_name, _block in iter_dump_blocks(dump_path, encoding):
        key = dll_name.casefold()
        if key not in seen:
            seen.add(key)
            names.append(dll_name)
    return names


def write_selected(
    dump_path: Path,
    output_dir: Path,
    encoding: str,
    wanted: Optional[Set[str]],
) -> Dict[str, Tuple[Path, int]]:
    output_dir.mkdir(parents=True, exist_ok=True)

    paths_by_dll: Dict[str, Path] = {}
    used_names: Set[str] = set()
    written: Dict[str, Tuple[Path, int]] = {}
    initialized_files: Set[Path] = set()

    for dll_name, block in iter_dump_blocks(dump_path, encoding):
        if wanted is not None and not wanted_match(dll_name, wanted):
            continue

        output_path = build_output_path(output_dir, dll_name, paths_by_dll, used_names)
        mode = "a" if output_path in initialized_files else "w"
        with output_path.open(mode, encoding="utf-8", newline="") as handle:
            handle.writelines(block)

        initialized_files.add(output_path)
        old_path, old_count = written.get(dll_name.casefold(), (output_path, 0))
        written[dll_name.casefold()] = (old_path, old_count + 1)

    return written


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Extract one DLL or split all DLLs from an Il2CppDumper dump.cs.",
    )
    parser.add_argument(
        "dump",
        nargs="?",
        default="dump.cs",
        type=Path,
        help="Path to dump.cs. Default: dump.cs",
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--dll",
        action="append",
        metavar="NAME",
        help="DLL name to extract. Can be used multiple times. .dll suffix is optional.",
    )
    mode.add_argument(
        "--all",
        action="store_true",
        help="Extract every DLL into separate files.",
    )
    mode.add_argument(
        "--list",
        action="store_true",
        help="List DLL names found in the dump and exit.",
    )
    parser.add_argument(
        "-o",
        "--output",
        default="dump_split",
        type=Path,
        help="Output directory. Default: dump_split",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    dump_path: Path = args.dump

    if not dump_path.is_file():
        print(f"dump file not found: {dump_path}", file=sys.stderr)
        return 1

    encoding = detect_encoding(dump_path)

    if args.list:
        names = collect_dll_names(dump_path, encoding)
        if not names:
            print("no '// Dll :' blocks found", file=sys.stderr)
            return 1
        for name in names:
            print(name)
        return 0

    wanted = None
    if args.dll:
        wanted = {item.casefold() for item in args.dll}
        wanted.update(normalize_name(item) for item in args.dll)

    written = write_selected(dump_path, args.output, encoding, wanted)
    if not written:
        if args.dll:
            print(f"no matching DLL found: {', '.join(args.dll)}", file=sys.stderr)
        else:
            print("no '// Dll :' blocks found", file=sys.stderr)
        return 1

    total_blocks = sum(count for _path, count in written.values())
    print(f"written {len(written)} file(s), {total_blocks} block(s):")
    for _key, (path, count) in sorted(written.items(), key=lambda item: item[1][0].name.casefold()):
        print(f"  {path} ({count} block(s))")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
