"""Compact generated UI proofs and a verified, reproducible header cache.

Requires Pillow. Dry-run by default; --apply performs the reported cleanup.
Never removes executables, backups, source, settings, or recovery journals.
"""
import argparse
import json
import shutil
import stat
import zlib
from pathlib import Path
from zipfile import ZipFile

from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--clean-headers", action="store_true")
    args = parser.parse_args()
    workspace = Path(__file__).resolve().parent
    build = workspace / "build"

    def safe(path):
        path = path.absolute()
        if not path.resolve().is_relative_to(build.resolve()):
            raise RuntimeError(f"Path outside build directory: {path}")
        for part in (path, *path.parents):
            if part.exists() and part.stat(follow_symlinks=False).st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT:
                raise RuntimeError(f"Refusing a reparse point: {part}")
            if part == workspace:
                break
        return path

    report = {"applied": args.apply, "proofs": 0, "proofBytesSaved": 0, "headerBytesSaved": 0}
    for bmp in sorted((build / "candidates").glob("*/ui-*.bmp")):
        safe(bmp)
        png = safe(bmp.with_suffix(".png"))
        with Image.open(bmp) as original:
            image = original.convert("RGBA")
        if args.apply:
            pending = safe(png.with_suffix(".pending.png"))
            image.save(pending, format="PNG", optimize=True)
            with Image.open(pending) as encoded:
                if encoded.convert("RGBA").tobytes() != image.tobytes():
                    raise RuntimeError(f"Lossless verification failed: {bmp}")
            old_size = bmp.stat().st_size + (png.stat().st_size if png.exists() else 0)
            pending.replace(png)
            bmp.unlink()
            report["proofBytesSaved"] += old_size - png.stat().st_size
        else:
            report["proofBytesSaved"] += bmp.stat().st_size
        report["proofs"] += 1

    include = build / "winrt" / "include"
    archive = build / "winrt" / "headers.zip"
    if args.clean_headers and include.is_dir():
        safe(include)
        safe(archive)
        actual = {p.relative_to(include).as_posix(): safe(p) for p in include.rglob("*") if p.is_file()}
        with ZipFile(archive) as compressed:
            if compressed.testzip() is not None:
                raise RuntimeError("Header archive failed CRC validation")
            expected = {item.filename: item for item in compressed.infolist() if not item.is_dir()}
            if actual.keys() != expected.keys():
                raise RuntimeError("Header archive does not exactly match the expanded cache")
            for name, path in actual.items():
                data = path.read_bytes()
                item = expected[name]
                if len(data) != item.file_size or zlib.crc32(data) != item.CRC:
                    raise RuntimeError(f"Modified header must be preserved: {name}")
            report["headerBytesSaved"] = sum(p.stat().st_size for p in actual.values())
        # The fully resolved target and every file above have been checked.
        # build.ps1 recreates this exact cache from headers.zip on demand.
        if args.apply:
            for directory in include.rglob("*"):
                safe(directory)
            shutil.rmtree(include)

    report["bytesSaved"] = report["proofBytesSaved"] + report["headerBytesSaved"]
    print(json.dumps(report, indent=2))
    if args.apply:
        (build / "workspace-compaction.json").write_text(json.dumps(report, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
