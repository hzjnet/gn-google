// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "gn/filesystem_utils.h"
#include "gn/functions.h"
#include "gn/input_file.h"
#include "gn/test_with_scheduler.h"
#include "gn/test_with_scope.h"
#include "util/test/test.h"

class ExpandDirectoryTest : public TestWithScheduler {
 protected:
  ExpandDirectoryTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    setup.build_settings()->SetRootPath(temp_dir_.GetPath());
  }

  base::ScopedTempDir temp_dir_;
  TestWithScope setup;
};

TEST_F(ExpandDirectoryTest, Recursive) {
  base::FilePath dir_path = temp_dir_.GetPath();
  base::FilePath file1 = dir_path.AppendASCII("file1.txt");
  base::FilePath file2 = dir_path.AppendASCII("file2.txt");
  base::FilePath sub_dir = dir_path.AppendASCII("sub");
  base::FilePath file3 = sub_dir.AppendASCII("file3.txt");

  ASSERT_TRUE(base::CreateDirectory(sub_dir));
  ASSERT_TRUE(WriteFile(file1, "content1", nullptr));
  ASSERT_TRUE(WriteFile(file2, "content2", nullptr));
  ASSERT_TRUE(WriteFile(file3, "content3", nullptr));

  FunctionCallNode function;
  Err err;
  Value result = functions::RunExpandDirectory(
      setup.scope(), &function,
      {Value(nullptr, FilePathToUTF8(dir_path)), Value(nullptr, true)}, &err);
  ASSERT_FALSE(err.has_error());

  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(result.list_value().size(), 3);
  EXPECT_EQ(result.list_value()[0].string_value(), "//file1.txt");
  EXPECT_EQ(result.list_value()[1].string_value(), "//file2.txt");
  EXPECT_EQ(result.list_value()[2].string_value(), "//sub/file3.txt");

  std::vector<base::FilePath> deps = scheduler().GetGenDependencies();
  EXPECT_TRUE(std::ranges::find(deps, temp_dir_.GetPath()) != deps.end());
  EXPECT_TRUE(std::ranges::find(deps, sub_dir) != deps.end());
}

TEST_F(ExpandDirectoryTest, NonRecursive) {
  base::FilePath dir_path = temp_dir_.GetPath();
  base::FilePath file1 = dir_path.AppendASCII("file1.txt");
  base::FilePath sub_dir = dir_path.AppendASCII("sub");
  base::FilePath file2 = sub_dir.AppendASCII("file2.txt");

  ASSERT_TRUE(base::CreateDirectory(sub_dir));
  ASSERT_TRUE(WriteFile(file1, "content1", nullptr));
  ASSERT_TRUE(WriteFile(file2, "content2", nullptr));

  FunctionCallNode function;
  Err err;
  Value result = functions::RunExpandDirectory(
      setup.scope(), &function,
      {Value(nullptr, FilePathToUTF8(dir_path)), Value(nullptr, false)}, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(result.list_value().size(), 1);
  EXPECT_EQ(result.list_value()[0].string_value(), "//file1.txt");

  std::vector<base::FilePath> deps = scheduler().GetGenDependencies();
  EXPECT_TRUE(std::ranges::find(deps, temp_dir_.GetPath()) != deps.end());
  EXPECT_TRUE(std::ranges::find(deps, sub_dir) == deps.end());
}

TEST_F(ExpandDirectoryTest, EmptyDir) {
  std::string dir_str = FilePathToUTF8(temp_dir_.GetPath());

  FunctionCallNode function;
  Err err;
  Value result = functions::RunExpandDirectory(
      setup.scope(), &function, {Value(nullptr, dir_str), Value(nullptr, true)},
      &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(result.list_value().size(), 0);
}

TEST_F(ExpandDirectoryTest, NonExistentDir) {
  base::FilePath non_existent = temp_dir_.GetPath().AppendASCII("non_existent");

  FunctionCallNode function;
  Err err;
  Value result = functions::RunExpandDirectory(
      setup.scope(), &function,
      {Value(nullptr, FilePathToUTF8(non_existent)), Value(nullptr, true)},
      &err);
  EXPECT_TRUE(err.has_error());
}

TEST_F(ExpandDirectoryTest, OutsideRoot) {
  base::FilePath outside = temp_dir_.GetPath().DirName();

  FunctionCallNode function;
  Err err;
  Value result = functions::RunExpandDirectory(
      setup.scope(), &function,
      {Value(nullptr, FilePathToUTF8(outside)), Value(nullptr, true)}, &err);
  EXPECT_TRUE(err.has_error());
}

TEST_F(ExpandDirectoryTest, Allowlist) {
  InputFile input_file(SourceFile("//BUILD.gn"));
  Location location(&input_file, 1, 1);
  Token token(location, Token::IDENTIFIER, "expand_directory");
  FunctionCallNode function;
  function.set_function(token);

  auto args = std::make_unique<ListNode>();
  args->set_begin_token(token);
  args->set_end(std::make_unique<EndNode>(token));
  function.set_args(std::move(args));

  // No allowlist
  {
    Err err;
    Value result = functions::RunExpandDirectory(
        setup.scope(), &function,
        {Value(nullptr, FilePathToUTF8(temp_dir_.GetPath())),
         Value(nullptr, true)},
        &err);
    EXPECT_FALSE(err.has_error());
  }

  // Empty allowlist
  auto allowlist_owned = std::make_unique<SourceFileSet>();
  auto allowlist = allowlist_owned.get();
  setup.build_settings()->set_expand_directory_allowlist(
      std::move(allowlist_owned));
  allowlist->insert(SourceFile("//foo.gni"));
  {
    Err err;
    Value result = functions::RunExpandDirectory(
        setup.scope(), &function,
        {Value(nullptr, FilePathToUTF8(temp_dir_.GetPath())),
         Value(nullptr, true)},
        &err);
    EXPECT_TRUE(err.has_error());
  }

  // In the allowlist
  allowlist->insert(SourceFile("//BUILD.gn"));
  {
    Err err;
    Value result = functions::RunExpandDirectory(
        setup.scope(), &function,
        {Value(nullptr, FilePathToUTF8(temp_dir_.GetPath())),
         Value(nullptr, true)},
        &err);
    EXPECT_FALSE(err.has_error());
  }
}
