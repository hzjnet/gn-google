// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/headers_map_writer.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "gn/filesystem_utils.h"
#include "gn/setup.h"
#include "gn/switches.h"
#include "gn/test_with_scheduler.h"
#include "util/test/test.h"

using HeadersMapWriterTest = TestWithScheduler;

static void WriteFile(const base::FilePath& file, const std::string& data) {
  CHECK_EQ(static_cast<int>(data.size()),
           base::WriteFile(file, data.data(), data.size()));
}

TEST_F(HeadersMapWriterTest, MapFile) {
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);

  const char kDotfileContents[] = R"(
buildconfig = "//BUILDCONFIG.gn"
)";

  const char kBuildConfigContents[] = R"(
set_default_toolchain("//toolchain:default")
)";

  const char kToolchainBuildContents[] = R"##(
toolchain("default") {
  tool("stamp") {
    command = "stamp"
  }
}
)##";

  const char kBuildGnContents[] = R"##(
source_set("b") {
  sources = [ "b.cc", "b.h" ]
}

source_set("a") {
  sources = [ "a.cc" ]
  public = [ "a.h" ]
}
)##";

  // Create a temp directory containing the build.
  base::ScopedTempDir in_temp_dir;
  ASSERT_TRUE(in_temp_dir.CreateUniqueTempDir());
  base::FilePath in_path = in_temp_dir.GetPath();

  WriteFile(in_path.Append(FILE_PATH_LITERAL("BUILD.gn")), kBuildGnContents);
  WriteFile(in_path.Append(FILE_PATH_LITERAL("BUILDCONFIG.gn")),
            kBuildConfigContents);
  WriteFile(in_path.Append(FILE_PATH_LITERAL(".gn")), kDotfileContents);

  EXPECT_TRUE(
      base::CreateDirectory(in_path.Append(FILE_PATH_LITERAL("toolchain"))));

  WriteFile(in_path.Append(FILE_PATH_LITERAL("toolchain/BUILD.gn")),
            kToolchainBuildContents);

  cmdline.AppendSwitch(switches::kRoot, FilePathToUTF8(in_path));

  // Create another temp dir for writing the generated files to.
  base::ScopedTempDir build_temp_dir;
  ASSERT_TRUE(build_temp_dir.CreateUniqueTempDir());

  // Run setup
  Setup setup;
  EXPECT_TRUE(
      setup.DoSetup(FilePathToUTF8(build_temp_dir.GetPath()), true, cmdline));

  // Do the actual load.
  ASSERT_TRUE(setup.Run());

  std::string file_name = "headers.txt";
  Err err;
  StringOutputBuffer out =
      HeadersMapWriter::RunAndGenerate(setup.builder().GetAllResolvedTargets());

  SourceFile output_file =
      setup.build_settings().build_dir().ResolveRelativeFile(
          Value(nullptr, file_name), &err);
  ASSERT_FALSE(output_file.is_null());

  base::FilePath output_path = setup.build_settings().GetFullPath(output_file);
  bool res = out.WriteToFileIfChanged(output_path, &err);
  ASSERT_TRUE(res);

  std::string generated;
  ASSERT_TRUE(base::ReadFileToString(output_path, &generated));

  // Verify that the generated file has the expected content.
  // The paths should be relative to source root without // (as per my
  // implementation). The targets should be sorted by label in the JSON output
  // because I used std::set. And the headers should be sorted because I used
  // std::map.

  std::string expected = R"##(a.h //:a
b.h //:b
)##";

  EXPECT_EQ(generated, expected) << generated << "\n" << expected;
}

TEST(HeadersMapWriterTest, GenerateFiles) {
  Label default_toolchain(SourceDir("//toolchain/"), "default");
  Label second_toolchain(SourceDir("//toolchain/"), "second");
  Label third_toolchain(SourceDir("//toolchain/"), "third");

  auto make_label = [](auto name, const Label& toolchain) {
    return Label(SourceDir("//"), name, toolchain.dir(), toolchain.name());
  };
  
  auto a = make_label("a", default_toolchain);
  auto a_second = make_label("a", second_toolchain);
  auto a_third = make_label("a", third_toolchain);
  auto b = make_label("b", default_toolchain);

  std::map<std::string_view, std::vector<const Label*>> header_to_targets = {
    {"two_labels.h", {&a, &b}},
    {"default_included.h", {&a, &a_second, &a_third}},
    {"default_not_included.h", {&a_second, &a_third}},
  };

  auto got = HeadersMapWriter::GenerateFiles(default_toolchain, header_to_targets).str();
  
  std::string expected = R"##(default_included.h //:a
default_not_included.h //:a
two_labels.h //:a //:b
)##";

  EXPECT_EQ(got, expected) << got << "\n" << expected;
}
