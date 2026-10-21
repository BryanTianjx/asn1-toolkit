"""Verify cache invalidation by rebuilding an isolated compiler with a changed identity.

Use a separate compiler-only build directory, never one currently being built.
Only its generated private header is changed; it is restored and rebuilt in finally.
"""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--config", default="Release")
    args = parser.parse_args()
    build = args.build.resolve()
    root = Path(__file__).resolve().parents[1]
    header = build / "generated/generator_revision.hpp"
    tool = build / ("asn1cxx.exe" if sys.platform == "win32" else "asn1cxx")
    if not tool.exists() and sys.platform == "win32":
        tool = build / args.config / "asn1cxx.exe"
    def rebuild():
        subprocess.run(["cmake", "--build", str(build), "--target", "asn1cxx",
                        "--config", args.config, "--parallel", "1"], check=True)
    rebuild()
    original = header.read_bytes()
    with tempfile.TemporaryDirectory(prefix="asn1-generator-identity-") as tmp:
        out = Path(tmp)
        def emit():
            result = subprocess.run([str(tool), "--codec", "uper", "--emit-dir", str(out),
                                     str(root / "examples/person.asn")], check=True, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            key = (out / ".asn1cxx-cache/default.cache").read_text().split()[0]
            return key, result.stderr
        try:
            first, _ = emit()
            same, note = emit()
            if first != same or "1/1 units hit" not in note:
                raise RuntimeError("Identical input did not hit cache")
            identity = hashlib.sha256(original + b"test revision").hexdigest().encode()
            changed, count = re.subn(rb'"[0-9a-f]{64}"', b'"' + identity + b'"', original)
            if count != 1 or changed == original:
                raise RuntimeError("Expected exactly one private identity macro")
            header.write_bytes(changed)
            rebuild()
            second, note = emit()
            if first == second or "0/1 units hit" not in note:
                raise RuntimeError("Changed generator identity reused stale cache")
        finally:
            header.write_bytes(original)
            rebuild()
    print("PASS actual generator identity change forces cache miss for identical ASN.1")


if __name__ == "__main__":
    main()
