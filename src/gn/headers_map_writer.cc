// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/headers_map_writer.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/json/string_escape.h"
#include "gn/build_settings.h"
#include "gn/filesystem_utils.h"
#include "gn/settings.h"
#include "gn/string_output_buffer.h"
#include "gn/target.h"

// static
StringOutputBuffer HeadersMapWriter::GenerateFiles(
    const Label& default_toolchain,
    std::map<std::string_view, std::vector<const Label*>>& header_to_targets) {
  StringOutputBuffer out;
  for (auto& [header_path, targets] : header_to_targets) {
    out.Append(header_path);

    std::sort(targets.begin(), targets.end());

    auto is_default = [&default_toolchain](const Label* label) {
      return label->toolchain_dir() == default_toolchain.toolchain_dir() &&
             label->toolchain_name() == default_toolchain.toolchain_name();
    };

    std::sort(
        targets.begin(), targets.end(),
        [&is_default](const Label* lhs, const Label* rhs) {
          // We use !is_default to ensure the default toolchain appears at the
          // front of the list. We use std::make_tuple instead of std::tie
          // because dir() returns by value and would create dangling
          // references.
          return std::make_tuple(lhs->dir(), lhs->name(), !is_default(lhs),
                                 lhs->toolchain_dir(), lhs->toolchain_name()) <
                 std::make_tuple(rhs->dir(), rhs->name(), !is_default(rhs),
                                 rhs->toolchain_dir(), rhs->toolchain_name());
        });
    auto last = std::unique(targets.begin(), targets.end(),
                            [&is_default](const Label* lhs, const Label* rhs) {
                              // Strip all non-default toolchains if the default
                              // toolchain exists here.
                              return lhs->dir() == rhs->dir() &&
                                     lhs->name() == rhs->name() &&
                                     is_default(lhs);
                            });
    targets.erase(last, targets.end());

    for (const Label* label : targets) {
      out.Append(" ");
      out.Append(label->GetUserVisibleName(default_toolchain));
    }
    out.Append("\n");
  }

  return out;
}

// static
StringOutputBuffer HeadersMapWriter::RunAndGenerate(
    const std::vector<const Target*>& targets) {
  std::map<std::string_view, std::vector<const Label*>> header_to_targets;

  for (const auto* target : targets) {
    auto process_file = [&](const SourceFile& file) {
      if (file.GetType() == SourceFile::SOURCE_H) {
        std::string_view header_path = file.value();
        if (header_path.rfind("//", 0) == 0) {
          header_path = header_path.substr(2);
        }
        header_to_targets[header_path].push_back(&target->label());
      }
    };

    for (const auto& file : target->sources()) {
      process_file(file);
    }

    for (const auto& file : target->public_headers()) {
      process_file(file);
    }
  }

  const Label& default_toolchain =
      targets.empty() ? Label()
                      : targets[0]->settings()->default_toolchain_label();
  return GenerateFiles(default_toolchain, header_to_targets);
}
