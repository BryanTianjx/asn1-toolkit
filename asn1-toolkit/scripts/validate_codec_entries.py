"""Compare linked single-entry encodings with all three combined emit shapes."""
import pathlib
import subprocess
import sys
import tempfile

def require(condition, message):
    if not condition:
        raise RuntimeError(message)


compiler, *executables = sys.argv[1:]
single = {}
for exe in executables:
    lines = subprocess.check_output([exe], text=True).splitlines()
    output = dict(line.split(":", 1) for line in lines)
    if len(output) == 1:
        single.update(output)
    else:
        require(output == single, f"Combined encoding differs: {exe}: {output} != {single}")
require(len(single) == 12, "Expected 12 independent product entries")
require(single["jer"] == single["jeri"], "JER/JERI wire mismatch")
require(single["jer"].startswith("["), "JER ARRAY instruction was ignored")
require(single["xer"] != single["exer"], "E-XER ATTRIBUTE policy leaked or was ignored")
root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="asn1-codec-cli-") as tmp:
    dest = pathlib.Path(tmp)
    def emit(args):
        subprocess.run([compiler, "--no-emit-cache", "--emit-dir", str(dest), *args,
                        str(root / "tests/fixtures/codec_entries.asn")], check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return ((dest / "generated.hpp").read_bytes(), (dest / "generated.cpp").read_bytes())
    require(emit([]) == emit(["--codec", "uper"]), "Default codec differs from UPER")
    require(emit(["--codec", "per"]) == emit(["--codec", "uper,aper"]), "PER alias mismatch")
    require(emit(["--codec", "uper,ber,uper"]) == emit(["--codec", "ber", "--codec", "uper"]),
            "Duplicate/order/repeated flag mismatch")
print("12 entries: single/combined/inline/split wire equality and CLI mask equality passed")
