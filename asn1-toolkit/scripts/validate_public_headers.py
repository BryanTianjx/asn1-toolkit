"""Compile each public C++ header alone under C++17 (no runtime linking)."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="c++")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    headers = sorted((root / "include" / "asn1").rglob("*.hpp"))
    with tempfile.TemporaryDirectory(prefix="asn1-public-headers-") as directory:
        source = Path(directory) / "header.cpp"
        for header in headers:
            name = header.relative_to(root / "include").as_posix()
            source.write_text(f"#include <{name}>\n", encoding="utf-8")
            subprocess.run([args.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-pedantic-errors", "-fsyntax-only",
                            "-I", str(root / "include"), str(source)], check=True)
    print(f"Public C++17 headers: {len(headers)}/{len(headers)} passed")


if __name__ == "__main__":
    main()
