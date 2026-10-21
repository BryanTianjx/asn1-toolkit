#!/usr/bin/env python3
"""Small parse/analyze/emit probes; no claim of wire interoperability."""
import argparse
import json
import pathlib
import subprocess

CASES = {
    "union-intersection": (True, "A ::= INTEGER ((0..10 | 20..30) ^ (5..25))"),
    "unresolved-bound": (False, "A ::= INTEGER (unknown..10)"),
    "negative-size": (False, "A ::= OCTET STRING (SIZE(-1..4))"),
    "empty-intersection": (False, "A ::= INTEGER ((0..1) ^ (3..4))"),
    "permitted-alphabet": (False, 'A ::= IA5String (FROM ("A".."Z"))'),
    "with-components": (False, "A ::= SEQUENCE { x INTEGER } (WITH COMPONENTS { x PRESENT })"),
    "alias-constraint": (False, "Base ::= INTEGER (0..255) A ::= Base (0..7)"),
    "components-of": (False, "Base ::= SEQUENCE { x INTEGER (0..7) } A ::= SEQUENCE { COMPONENTS OF Base, y BOOLEAN }"),
    "parameterized": (True, "Box {T} ::= SEQUENCE { item T } A ::= Box {INTEGER (0..7)}"),
    "extension-group": (True, "A ::= SEQUENCE { x INTEGER (0..7), ..., [[ y BOOLEAN ]] }"),
    "numbered-extension-group": (False, "A ::= SEQUENCE { x INTEGER (0..7), ..., [[ 1: y BOOLEAN ]] }"),
    "builtin-associated": (True, "A ::= EXTERNAL B ::= EMBEDDED PDV C ::= INSTANCE OF TYPE-IDENTIFIER"),
    "wide-integer": (True, "A ::= INTEGER (0..184467440737095516160)"),
    "oid-iri": (False, "A ::= OID-IRI"),
    "relative-oid-iri": (False, "A ::= RELATIVE-OID-IRI"),
    "time": (False, "A ::= TIME"),
    "date": (False, "A ::= DATE"),
    "time-of-day": (False, "A ::= TIME-OF-DAY"),
    "date-time": (False, "A ::= DATE-TIME"),
    "duration": (False, "A ::= DURATION"),
    "real-unrepresented-profile": (False, "A ::= REAL (WITH COMPONENTS { base (10) })"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    results = []
    for name, (accepted, declaration) in CASES.items():
        source = args.output / (name + ".asn")
        source.write_text("M DEFINITIONS AUTOMATIC TAGS ::= BEGIN " + declaration + " END\n", encoding="utf-8")
        destination = args.output / (name + "-generated")
        proc = subprocess.run([args.compiler, "--no-emit-cache", "--codec", "uper", "--emit-dir",
                               str(destination), str(source)], capture_output=True, text=True)
        results.append({"name": name, "schema": source.read_text(encoding="utf-8"),
                        "expected_accept": accepted, "exit": proc.returncode, "diagnostic": proc.stderr})
        if (proc.returncode == 0) != accepted:
            raise RuntimeError(f"{name}: unexpected result {proc.returncode}: {proc.stderr}")
        print(f"PASS {name}: {'emitted' if accepted else 'rejected'}", flush=True)
    (args.output / "results.json").write_text(json.dumps(results, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"PASS {len(results)} capability probes (parse/analyze/emit only)")


if __name__ == "__main__":
    main()
