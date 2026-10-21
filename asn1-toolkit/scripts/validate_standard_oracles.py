#!/usr/bin/env python3
"""Independent ASN.1 differential check; optional asn1tools is development-only.

Default: dependency-free frozen vector replay. --oracle-root enables live
cross-decoding; --write-vectors explicitly refreshes external expectations.
No expected byte string is sourced from asn1-toolkit output.
"""
import argparse
import hashlib
import importlib
import json
import pathlib
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCHEMA = ROOT / "tests/fixtures/standards.asn"
VECTORS = ROOT / "tests/standards/oracle-vectors.json"
VALUE = {"count": 42, "enabled": True, "text": "ASN1"}
CODECS = {"ber": "ber", "der": "der", "uper": "uper", "aper": "per",
          "oer": "oer", "xer": "xer", "jer": "jer"}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def xml_tree(data):
    def element(e):
        return e.tag, sorted(e.attrib.items()), (e.text or "").strip(), [element(c) for c in e]
    return element(ET.fromstring(data))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", required=True)
    parser.add_argument("--oracle-root", type=pathlib.Path)
    parser.add_argument("--write-vectors", action="store_true")
    args = parser.parse_args()
    require(not args.write_vectors or args.oracle_root, "Refreshing requires an independent oracle")
    # Normalize checkout line endings so Windows autocrlf does not invalidate
    # an otherwise identical schema's provenance.
    digest = hashlib.sha256(SCHEMA.read_text(encoding="utf-8").encode("utf-8")).hexdigest()
    live = {}
    if args.oracle_root:
        sys.path.insert(0, str(args.oracle_root.resolve()))
        oracle = importlib.import_module("asn1tools")
        live = {name: oracle.compile_files(str(SCHEMA), codec) for name, codec in CODECS.items()}
        external = {name: tool.encode("Record", VALUE, check_constraints=True).hex()
                    for name, tool in live.items()}
        try:
            git_root = pathlib.Path(subprocess.check_output(
                ["git", "-C", str(args.oracle_root), "rev-parse", "--show-toplevel"],
                text=True, stderr=subprocess.DEVNULL).strip())
        except (subprocess.CalledProcessError, FileNotFoundError):
            git_root = None
        # A parent workspace commit does not identify an untracked oracle copy.
        commit = (subprocess.check_output(["git", "-C", str(args.oracle_root), "rev-parse", "HEAD"], text=True).strip()
                  if git_root is not None and git_root.resolve() == args.oracle_root.resolve() else None)
        source_hash = hashlib.sha256()
        package = pathlib.Path(oracle.__file__).resolve().parent
        require(args.oracle_root.resolve() in package.parents, "Imported oracle is outside --oracle-root")
        for source in sorted(package.rglob("*.py")):
            source_hash.update(source.relative_to(package).as_posix().encode("utf-8") + b"\0")
            source_hash.update(source.read_text(encoding="utf-8").encode("utf-8") + b"\0")
        document = {"oracle": "asn1tools", "version": oracle.__version__, "commit": commit,
                    "source_sha256": source_hash.hexdigest(),
                    "schema": "tests/fixtures/standards.asn", "sha256": digest,
                    "schema_hash_normalization": "UTF-8 LF",
                    "type": "Record", "value": VALUE,
                    "parameters": {"check_constraints": True}, "vectors": external}
        if args.write_vectors:
            VECTORS.parent.mkdir(parents=True, exist_ok=True)
            VECTORS.write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        else:
            require(document == json.loads(VECTORS.read_text(encoding="utf-8")), "Live oracle metadata/vectors changed; review before refresh")
    document = json.loads(VECTORS.read_text(encoding="utf-8"))
    require(document["sha256"] == digest and document["value"] == VALUE, "Frozen schema/value mismatch")
    expected = document["vectors"]
    actual = dict(line.split("|", 1) for line in subprocess.check_output([args.driver], text=True).splitlines())
    require(set(actual) == {"ber", "der", "cer", "uper", "aper", "oer", "coer", "xer", "cxer", "exer", "jer", "jeri"}, "Missing product entry")
    for name, ref in expected.items():
        if name == "xer":
            require(xml_tree(bytes.fromhex(actual[name])) == xml_tree(bytes.fromhex(ref)), "XER tree differs")
        elif name == "jer":
            require(json.loads(bytes.fromhex(actual[name])) == json.loads(bytes.fromhex(ref)), "JER value differs")
        else:
            require(actual[name] == ref, f"{name} reference bytes differ for the selected primitive sequence")
        subprocess.run([args.driver, name, ref], check=True)
    # Same abstract syntax in the instruction-free profile. These comparisons
    # do not certify complete COER/CXER/E-XER/JERI instruction semantics.
    for name, base in {"coer": "oer", "cxer": "xer", "exer": "xer", "jeri": "jer"}.items():
        reference = expected[base]
        if name == "cxer":
            # X.693 canonical empty-element spelling for this attribute-free
            # fixture; external XER legitimately inserts a space before />.
            reference = bytes.fromhex(reference).replace(b" />", b"/>").hex()
        subprocess.run([args.driver, name, reference], check=True)
    require(subprocess.run([args.driver, "cer", expected["ber"]], check=False).returncode != 0,
            "CER accepted definite constructed BER")
    if live:
        for name, data in actual.items():
            base = {"cer": "ber", "coer": "oer", "cxer": "xer", "exer": "xer", "jeri": "jer"}.get(name, name)
            decoded = live[base].decode("Record", bytes.fromhex(data), check_constraints=True)
            require(decoded == VALUE, f"External decoder disagrees: {name}: {decoded}")
        print(f"LIVE {document['oracle']} {document['version']} source={document['source_sha256']}: 12 external decodes, 11 reference inputs accepted (CXER empty-element spelling normalized); CER definite-form negative passed")
    else:
        print("Frozen independent vectors: 7 base entries, 4 instruction-free aliases, CER negative passed")


if __name__ == "__main__":
    main()
