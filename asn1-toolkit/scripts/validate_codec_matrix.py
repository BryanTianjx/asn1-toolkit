"""7 fixtures x 12 actual product selectors, separately emitted and syntax checked.

JERI is the project's X.697 instruction entry, not an independent transfer syntax.
No `per` alias is counted. Run under GCC/Clang or MSVC, using its native executable.
"""
import argparse
import pathlib
import subprocess

FIXTURES = ["telecom/tlv_oer_golden.asn", "telecom/tlv_golden.asn", "cer/cer_golden.asn",
            "telecom/rrc_slice/rrc_slice.asn", "telecom/lpp_14_3_0/LPP-PDU-Definitions.asn",
            "telecom/rrc_8_6_0/EUTRA-RRC-Definitions.asn", "telecom/s1ap_14_4_0/s1ap_14_4_0.asn"]
CODECS = "ber der cer uper aper oer coer xer cxer exer jer jeri".split()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--cxx", default="c++")
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    msvc = pathlib.Path(args.cxx).stem.lower() in {"cl", "clang-cl"}
    for index, fixture in enumerate(FIXTURES):
        for codec in CODECS:
            dest = args.output.resolve() / str(index) / codec
            subprocess.run([args.compiler, "--no-emit-cache", "--codec", codec,
                            "--emit-dir", str(dest), str(root / "tests/fixtures" / fixture)], check=True)
            flags = (["/std:c++17", "/Zs", "/EHsc", "/I" + str(root / "include"), "/I" + str(dest)]
                     if msvc else ["-std=c++17", "-fsyntax-only", "-I" + str(root / "include"), "-I" + str(dest)])
            subprocess.run([args.cxx, *flags, str(dest / "generated.cpp")], check=True)
            print(f"PASS {index * 12 + CODECS.index(codec) + 1}/84 {fixture} {codec}", flush=True)
    print("PASS 84/84 actual product selectors; syntax checking does not prove interoperability")

if __name__ == "__main__":
    main()
