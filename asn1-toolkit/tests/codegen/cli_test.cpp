/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/codegen/cli_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-12
**   $Author: tina.cao
***************************************************************************/
/**  File Description:
**
**   End-to-end tests for the asn1cxx CLI. Spawns the built asn1cxx binary
**   as a subprocess and checks exit codes, stderr messages, and emitted
**   files. (Kept as a subprocess rather than including main.cpp so the
**   heavy compiler headers stay out of the test translation unit.)
**
** Specification: asn1cxx CLI contract (tools/asn1cxx/main.cpp).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

struct CliResult {
  int code;
  std::string out;
  std::string err;
};

// Run the asn1cxx binary with args, capturing stdout/stderr via temp files.
// Temp file names include the pid so parallel ctest invocations of this binary
// (one per test, via --gtest_filter) never share files.
CliResult run_asn1cxx(const std::vector<std::string>& args) {
  namespace fs = std::filesystem;
  const fs::path bin = ASN1CXX_BIN;
  const std::string tag =
      "asn1cxx_cli_" + std::to_string(static_cast<long long>(::getpid())) + "_";
  const fs::path out_f = fs::temp_directory_path() / (tag + "out.txt");
  const fs::path err_f = fs::temp_directory_path() / (tag + "err.txt");

  std::vector<std::string> storage;
  storage.reserve(args.size() + 3);
  storage.push_back(bin.string());
  for (const auto& a : args) storage.push_back(a);

  std::vector<char*> argv;
  argv.reserve(storage.size() + 1);
  for (auto& s : storage) argv.push_back(const_cast<char*>(s.c_str()));
  argv.push_back(nullptr);

  const pid_t pid = fork();
  if (pid == 0) {
    // Child: redirect stdout/stderr to temp files, then exec.
    FILE* fo = std::fopen(out_f.c_str(), "w");
    FILE* fe = std::fopen(err_f.c_str(), "w");
    if (fo) dup2(fileno(fo), STDOUT_FILENO);
    if (fe) dup2(fileno(fe), STDERR_FILENO);
    execv(bin.c_str(), argv.data());
    _exit(127);  // exec failed
  }
  int status = 0;
  waitpid(pid, &status, 0);
  const int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

  auto read_all = [](const fs::path& p) {
    std::string s;
    if (FILE* f = std::fopen(p.c_str(), "r")) {
      char buf[4096];
      std::size_t n;
      while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
      std::fclose(f);
    }
    return s;
  };
  CliResult r{code, read_all(out_f), read_all(err_f)};
  std::error_code ec;
  fs::remove(out_f, ec);
  fs::remove(err_f, ec);
  return r;
}

std::string example(const char* name) {
  return std::string(ASN1_EXAMPLES_DIR) + "/" + name;
}

}  // namespace

TEST(Cli, NoArgsFailsWithUsage) {
  const auto r = run_asn1cxx({});
  EXPECT_NE(r.code, 0);
  EXPECT_NE(r.err.find("Usage:"), std::string::npos);
}

TEST(Cli, HelpSucceeds) {
  const auto r = run_asn1cxx({"--help"});
  EXPECT_EQ(r.code, 0);
  EXPECT_NE(r.out.find("Usage:"), std::string::npos);
}

TEST(Cli, VersionSucceeds) {
  const auto r = run_asn1cxx({"--version"});
  EXPECT_EQ(r.code, 0);
  EXPECT_NE(r.out.find("asn1cxx"), std::string::npos);
}

TEST(Cli, DumpIrOnExampleSucceeds) {
  const auto r = run_asn1cxx({"--dump-ir", example("person.asn")});
  EXPECT_EQ(r.code, 0);
}

TEST(Cli, DumpAstOnExampleSucceeds) {
  const auto r = run_asn1cxx({"--dump-ast", example("person.asn")});
  EXPECT_EQ(r.code, 0);
}

TEST(Cli, UnknownOptionFails) {
  const auto r = run_asn1cxx({"--bogus", example("person.asn")});
  EXPECT_NE(r.code, 0);
  EXPECT_NE(r.err.find("unknown option"), std::string::npos);
}

TEST(Cli, UnknownCodecFails) {
  const auto r = run_asn1cxx(
      {"--emit-dir", "/tmp/asn1cxx_cli_ignore", "--codec", "nope",
       example("person.asn")});
  EXPECT_NE(r.code, 0);
  EXPECT_NE(r.err.find("unknown codec"), std::string::npos);
}

TEST(Cli, MissingInputFileFails) {
  const auto r = run_asn1cxx({"--dump-ir", "/no/such/file.asn"});
  EXPECT_NE(r.code, 0);
}

TEST(Cli, EmitProducesFiles) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / "asn1cxx_cli_emit_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  ASSERT_FALSE(ec);

  const auto r = run_asn1cxx(
      {"--emit-dir", dir.string(), "--codec", "uper", "--namespace", "cli_gen",
       example("person.asn")});
  EXPECT_EQ(r.code, 0);
  EXPECT_TRUE(fs::exists(dir / "generated.hpp"));
  EXPECT_TRUE(fs::exists(dir / "generated.cpp"));

  fs::remove_all(dir, ec);
}

// --- Multi-codec combination (comma-separated / repeated --codec flags) ---

std::string read_text(const std::string& path) {
  std::string s;
  if (std::FILE* f = std::fopen(path.c_str(), "r")) {
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    std::fclose(f);
  }
  return s;
}

// Emits into a per-test dir and returns the generated.hpp text.
std::string emit_hpp(const std::vector<std::string>& args, int* code) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() /
                       ("asn1cxx_cli_codec_" +
                        std::to_string(static_cast<long long>(::getpid())));
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);

  std::vector<std::string> full = args;
  full.insert(full.begin(), "--emit-dir");
  full.insert(full.begin() + 1, dir.string());
  const auto r = run_asn1cxx(full);
  *code = r.code;
  const std::string hpp = read_text((dir / "generated.hpp").string());
  fs::remove_all(dir, ec);
  return hpp;
}

TEST(Cli, CodecComboUperBerEmitsBothRuntimes) {
  int code = -1;
  const std::string hpp =
      emit_hpp({"--codec", "uper,ber", example("person.asn")}, &code);
  EXPECT_EQ(code, 0);
  EXPECT_NE(hpp.find("asn1/runtime/uper.hpp"), std::string::npos);
  EXPECT_NE(hpp.find("asn1/runtime/ber/tlv.hpp"), std::string::npos);
  // APER was NOT requested and must not leak into the emitted header.
  EXPECT_EQ(hpp.find("asn1/runtime/aper.hpp"), std::string::npos);
}

TEST(Cli, DefaultCodecIsUperWhenFlagAbsent) {
  int code = -1;
  const std::string hpp = emit_hpp({example("person.asn")}, &code);
  EXPECT_EQ(code, 0);
  EXPECT_NE(hpp.find("asn1/runtime/uper.hpp"), std::string::npos);
  EXPECT_EQ(hpp.find("asn1/runtime/ber/tlv.hpp"), std::string::npos);
}

TEST(Cli, RepeatedCodecFlagsAccumulate) {
  int code = -1;
  const std::string hpp = emit_hpp(
      {"--codec", "uper", "--codec", "ber", example("person.asn")}, &code);
  EXPECT_EQ(code, 0);
  EXPECT_NE(hpp.find("asn1/runtime/uper.hpp"), std::string::npos);
  EXPECT_NE(hpp.find("asn1/runtime/ber/tlv.hpp"), std::string::npos);
}

TEST(Cli, EmptyCodecNameInListFails) {
  const auto r = run_asn1cxx(
      {"--emit-dir", "/tmp/asn1cxx_cli_ignore", "--codec", "uper,",
       example("person.asn")});
  EXPECT_NE(r.code, 0);
  EXPECT_NE(r.err.find("empty codec name"), std::string::npos);
}

TEST(Cli, UnknownCodecInsideListFails) {
  const auto r = run_asn1cxx(
      {"--emit-dir", "/tmp/asn1cxx_cli_ignore", "--codec", "uper,nope",
       example("person.asn")});
  EXPECT_NE(r.code, 0);
  EXPECT_NE(r.err.find("unknown codec"), std::string::npos);
}

// --- Out-of-line emit (default): declarations in .hpp, definitions in .cpp ---

// Emits into a per-test dir and returns (hpp, cpp) text.
std::pair<std::string, std::string> emit_hpp_cpp(const std::vector<std::string>& args,
                                                 int* code) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() /
                       ("asn1cxx_cli_ool_" +
                        std::to_string(static_cast<long long>(::getpid())));
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);

  std::vector<std::string> full = args;
  full.insert(full.begin(), "--emit-dir");
  full.insert(full.begin() + 1, dir.string());
  const auto r = run_asn1cxx(full);
  *code = r.code;
  const std::string hpp = read_text((dir / "generated.hpp").string());
  const std::string cpp = read_text((dir / "generated.cpp").string());
  fs::remove_all(dir, ec);
  return {hpp, cpp};
}

TEST(Cli, DefaultEmitIsOutOfLine) {
  int code = -1;
  const auto [hpp, cpp] = emit_hpp_cpp(
      {"--codec", "uper", "--namespace", "ool_gen", example("person.asn")}, &code);
  EXPECT_EQ(code, 0);
  // Header: declarations only (no `inline` definitions, no runtime call bodies).
  EXPECT_NE(hpp.find("encode_uper(asn1::BitWriter& w, const Person& value);"),
            std::string::npos);
  EXPECT_EQ(hpp.find("inline asn1::Result"), std::string::npos);
  EXPECT_EQ(hpp.find("asn1::uper::encode_integer"), std::string::npos);
  // Companion .cpp: the definitions, wrapped in the generated namespace.
  EXPECT_NE(cpp.find("#include \"generated.hpp\""), std::string::npos);
  EXPECT_NE(cpp.find("namespace ool_gen {"), std::string::npos);
  EXPECT_NE(cpp.find("asn1::Result<void> encode_uper(asn1::BitWriter& w, "
                     "const Person& value) {"),
            std::string::npos);
  EXPECT_NE(cpp.find("asn1::uper::encode_integer"), std::string::npos);
  EXPECT_NE(cpp.find("}  // namespace ool_gen"), std::string::npos);
}

TEST(Cli, EmitInlineRestoresHeaderDefinitions) {
  int code = -1;
  const auto [hpp, cpp] = emit_hpp_cpp(
      {"--codec", "uper", "--emit-inline", "--namespace", "inl_gen",
       example("person.asn")},
      &code);
  EXPECT_EQ(code, 0);
  EXPECT_NE(hpp.find("inline asn1::Result<void> encode_uper(asn1::BitWriter& w, "
                     "const Person& value) {"),
            std::string::npos);
  EXPECT_NE(hpp.find("asn1::uper::encode_integer"), std::string::npos);
  // Companion TU stays a stub in inline mode.
  EXPECT_NE(cpp.find("companion TU"), std::string::npos);
  EXPECT_EQ(cpp.find("asn1::uper::encode_integer"), std::string::npos);
}
