#include "gn/bazel_generator.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <mutex>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "gn/build_settings.h"
#include "gn/err.h"
#include "gn/label.h"
#include "gn/scope.h"
#include "gn/settings.h"
#include "gn/source_dir.h"
#include "gn/value.h"
#include "gn/variables.h"

struct Context {
  Label toolchain;
  SourceDir source_dir;
  std::string_view root_path;
};
using Formatter = void(std::ostream&, const Value&, const Context&);
using Field = std::tuple<const char*, Formatter*>;

void StringField(std::ostream& os, const Value& value, const Context& context) {
  // value.tostring quotes incorrectly \$ is quoted, which is not allowed in
  // bazel.
  os << '"';
  for (char ch : value.string_value()) {
    switch (ch) {
      case '"':
      case '\\':
        os << '\\';
      default:
        os << ch;
    }
  }
  os << '"';
}

void BoolField(std::ostream& os, const Value& value, const Context& context) {
  os << (value.boolean_value() ? "True" : "False");
}

void IntField(std::ostream& os, const Value& value, const Context& context) {
  os << value.int_value();
}

void PathField(std::ostream& os, const Value& value, const Context& context) {
  std::string_view path = value.string_value();
  // It's a directory, strip the suffix and let bazel deal with it.
  if (path.ends_with("/")) {
    path = path.substr(0, path.size() - 1);
  }
  if (path.starts_with("//")) {
    os << "\"@@//:" << path.substr(2) << "\"";
  } else {
    os << "\"@@//:" << context.source_dir.value().substr(2) << path << "\"";
  }
}

std::string ToolchainSuffix(const Label& toolchain) {
  auto toolchain_str = toolchain.GetUserVisibleName(false);
  std::ranges::replace_if(
      toolchain_str, [](char c) { return c == '/' || c == ':'; }, '_');
  return toolchain_str;
}

void LabelField(std::ostream& os, const Value& value, const Context& context) {
  Err err;
  auto label = Label::Resolve(context.source_dir, context.root_path,
                              context.toolchain, value, &err);
  if (err.has_error()) {
    os << "\"Invalid label \" + " << value.ToString(true);
  }
  os << '"' << label.GetUserVisibleName(false);
  if (auto toolchain = label.GetToolchainLabel();
      toolchain != context.toolchain) {
    os << ToolchainSuffix(toolchain);
  }
  os << '"';
}

// Note: This only works on string-like values.
template <Formatter F>
void SetField(std::ostream& os, const Value& value, const Context& context) {
  Value copy = value;
  auto& l = copy.list_value();
  std::sort(l.begin(), l.end(), [](const Value& lhs, const Value& rhs) {
    return lhs.string_value() < rhs.string_value();
  });
  // Remove duplicates
  l.erase(std::unique(l.begin(), l.end()), l.end());
  ListField<F>(os, copy, context);
}

template <Formatter F>
void ListField(std::ostream& os, const Value& value, const Context& context) {
  const auto& l = value.list_value();
  if (l.empty()) {
    os << "[]";
  } else {
    os << "[\n";
    for (const auto& item : l) {
      os << "    ";
      F(os, item, context);
      os << ",\n";
    }
    os << "  ]";
  }
}

template <Formatter F>
void DictField(std::ostream& os, const Value& value, const Context& context) {
  auto scope = value.scope_value();
  Scope::KeyValueMap scope_values;
  scope->GetCurrentScopeValues(&scope_values);
  if (scope_values.empty()) {
    os << "{}";
  } else {
    os << "{\n";
    for (const auto& [k, v] : scope_values) {
      os << "    "
            " << k << "
            ": ";
      F(os, v, context);
      os << "\n";
    }
    os << "  }";
  }
}

constexpr std::array<Field, 4> kCommonFields = {
    std::make_tuple(variables::kDeps, SetField<LabelField>),
    std::make_tuple(variables::kPublicDeps, ListField<LabelField>),
    std::make_tuple(variables::kConfigs, ListField<LabelField>),
    std::make_tuple(variables::kPublicConfigs, ListField<LabelField>),
};

// Declarations
constexpr std::array<Field, 9> target_vars = {
    std::make_tuple(variables::kSources, ListField<PathField>),
    std::make_tuple(variables::kPublic, ListField<PathField>),
    std::make_tuple(variables::kAllDependentConfigs, ListField<LabelField>),
    std::make_tuple(variables::kData, ListField<PathField>),
    std::make_tuple(variables::kDataDeps, ListField<LabelField>),
    std::make_tuple(variables::kTestonly, BoolField),
    std::make_tuple(variables::kAssertNoDeps, ListField<LabelField>),
    std::make_tuple(variables::kVisibility, ListField<LabelField>),
    std::make_tuple(variables::kWriteRuntimeDeps, PathField),
};

constexpr std::array<Field, 32> tool_vars = {
    std::make_tuple("command", StringField),
    std::make_tuple("command_launcher", StringField),
    std::make_tuple("default_output_extension", StringField),
    std::make_tuple("depfile", StringField),
    std::make_tuple("depsformat", StringField),
    std::make_tuple("description", StringField),
    std::make_tuple("exe_output_extension", StringField),
    std::make_tuple("rlib_output_extension", StringField),
    std::make_tuple("dylib_output_extension", StringField),
    std::make_tuple("cdylib_output_extension", StringField),
    std::make_tuple("rust_proc_macro_output_extension", StringField),
    std::make_tuple("lib_switch", StringField),
    std::make_tuple("lib_dir_switch", StringField),
    std::make_tuple("framework_switch", StringField),
    std::make_tuple("weak_framework_switch", StringField),
    std::make_tuple("framework_dir_switch", StringField),
    std::make_tuple("swiftmodule_switch", StringField),
    std::make_tuple("rust_swiftmodule_switch", StringField),
    std::make_tuple("outputs", ListField<StringField>),
    std::make_tuple("partial_outputs", ListField<StringField>),
    std::make_tuple("pool", StringField),
    std::make_tuple("link_output", StringField),
    std::make_tuple("depend_output", StringField),
    std::make_tuple("output_prefix", StringField),
    std::make_tuple("default_output_dir", StringField),
    std::make_tuple("precompiled_header_type", StringField),
    std::make_tuple("rspfile", StringField),
    std::make_tuple("rspfile_content", StringField),
    std::make_tuple("runtime_outputs", ListField<StringField>),
    std::make_tuple("rust_sysroot", StringField),
    std::make_tuple("dynamic_link_switch", StringField),
    std::make_tuple("action", StringField),
};

constexpr std::array<Field, 3> toolchain_vars = {
    // It's a list of labels but they're pre-formatted as strings.
    std::make_tuple("tools", ListField<StringField>),
    std::make_tuple("root_build_dir", StringField),
    std::make_tuple("is_default", BoolField),
};

constexpr std::array<Field, 3> copy_vars = {
    std::make_tuple(variables::kPublic, ListField<PathField>),
    std::make_tuple(variables::kSources, ListField<PathField>),
    std::make_tuple(variables::kOutputs, ListField<StringField>),
};

constexpr std::array<Field, 15> config_vars = {
    std::make_tuple(variables::kAsmflags, ListField<StringField>),
    std::make_tuple(variables::kCflags, ListField<StringField>),
    std::make_tuple(variables::kCflagsC, ListField<StringField>),
    std::make_tuple(variables::kCflagsCC, ListField<StringField>),
    std::make_tuple(variables::kCflagsObjC, ListField<StringField>),
    std::make_tuple(variables::kCflagsObjCC, ListField<StringField>),
    std::make_tuple(variables::kDefines, ListField<StringField>),
    std::make_tuple(variables::kIncludeDirs, ListField<StringField>),
    std::make_tuple(variables::kInputs, ListField<PathField>),
    std::make_tuple(variables::kLdflags, ListField<StringField>),
    std::make_tuple(variables::kLibDirs, ListField<PathField>),
    std::make_tuple(variables::kLibs, ListField<StringField>),
    std::make_tuple(variables::kPrecompiledHeader, StringField),
    std::make_tuple(variables::kPrecompiledSource, PathField),
    std::make_tuple(variables::kSwiftflags, ListField<StringField>),
};

constexpr std::array<const char*, 4> disallowed_tools = {
    "action",
    "copy",
    "phony",
    "stamp",
};

namespace {
void WriteValues(const std::ranges::forward_range auto& vars,
                 const Scope* scope,
                 std::ostringstream& oss,
                 const Context& context) {
  for (const auto& [name, fn] : vars) {
    const Value* val = scope->GetValue(name);
    if (val) {
      oss << "  " << name << " = ";
      fn(oss, *val, context);
      oss << ",\n";
    }
  }
}

bool AllEqual(const std::vector<BazelTarget>& targets) {
  if (targets.empty()) {
    return true;
  }
  auto first = targets.front();
  for (auto target = targets.begin() + 1; target != targets.end(); ++target) {
    if (target->rule != first.rule || target->content != first.content) {
      return false;
    }
  }
  return true;
}
}  // namespace

// BazelTarget method definitions
void BazelTarget::Generate(const std::string& name, std::ostream& os) const {
  os << rule << "(\n  name = \"" << name << "\",\n" << content << ")\n";
}

std::string BazelTarget::FullName(const std::string& name) const {
  return name + ToolchainSuffix(toolchain);
}

// BazelPackageGenerator method definitions
void BazelPackageGenerator::AddTarget(const std::string& rule,
                                      const Label& label,
                                      const Scope* scope) {
  // TODO: support multi-toolchain?
  auto name = label.name();
  Context context{
      .toolchain = label.GetToolchainLabel(),
      .source_dir = scope->GetSourceDir(),
      .root_path = scope->settings()->build_settings()->root_path_utf8(),
  };
  std::ostringstream oss;
  // TODO: can use the technique used in source_file.cc to do a case statement.
  if (rule == "tool") {
    WriteValues(tool_vars, scope, oss, context);
  } else if (rule == "toolchain") {
    WriteValues(toolchain_vars, scope, oss, context);
  } else if (rule == "config") {
    WriteValues(config_vars, scope, oss, context);
  } else if (rule == "copy") {
    WriteValues(copy_vars, scope, oss, context);
  } else {
    WriteValues(target_vars, scope, oss, context);
  }
  WriteValues(kCommonFields, scope, oss, context);

  std::lock_guard<std::mutex> lock(target_mutex_);
  targets_[name].push_back(
      BazelTarget(label.GetToolchainLabel(), rule, oss.str()));
}

void BazelPackageGenerator::Generate(const base::FilePath& build_file_path) {
  std::set<std::string> rules;
  for (const auto& [name, targets] : targets_) {
    for (const auto& target : targets) {
      rules.insert(target.rule);
    }
  }

  std::ofstream os(build_file_path.value());
  os << "load(\"//gn:switch_toolchain.bzl\", \"switch_toolchain\")\n";
  for (const auto& rule : rules) {
    os << "load(\"//gn:" << rule << ".bzl\", \"" << rule << "\")\n";
  }

  os << "\npackage(default_visibility = [\"//visibility:public\"])\n";

  for (const auto& [name, toolchains] : targets_) {
    os << '\n';
    const auto& rule = toolchains.front().rule;
    if (rule.starts_with("tool")) {
      // A toolchain in another toolchain makes no sense.
      toolchains.front().Generate(name, os);
    } else if (AllEqual(toolchains)) {
      toolchains.front().Generate(name, os);
      for (const auto& toolchain : toolchains) {
        os << "switch_toolchain(\n  name = \"" << toolchain.FullName(name)
           << "\",\n  actual = \":" << name << "\",\n  toolchain = \""
           << toolchain.toolchain.GetUserVisibleName(false) << "\"\n)\n";
      }
    } else {
      for (const auto& toolchain : toolchains) {
        toolchain.Generate(toolchain.FullName(name), os);
      }
      os << "alias(\n  name = \"" << name << "\",\n  actual = select({\n";
      for (const auto& toolchain : toolchains) {
        os << "    \"" << toolchain.toolchain.GetUserVisibleName(false)
           << "_current\": \":" << toolchain.FullName(name) << "\",\n";
      }
      os << "  }),\n)";
    }
  }
}

// BazelGenerator method definitions
BazelPackageGenerator& BazelGenerator::GetPackage(const SourceDir& source_dir) {
  std::lock_guard<std::mutex> lock(packages_mutex_);
  return packages_[source_dir.value()];
}

void BazelGenerator::Generate(const base::FilePath& base_dir) {
  for (auto& package : packages_) {
    base::FilePath path = base_dir;
    std::string dir = package.first;
    if (dir.size() >= 2 && dir[0] == '/' && dir[1] == '/') {
      dir = dir.substr(2);
    }
    auto subdir = path.AppendASCII(dir);
    base::CreateDirectory(subdir);
    package.second.Generate(subdir.AppendASCII("BUILD.bazel"));
  }
}

BazelGenerator bazel_generator;

bool IsToolAllowed(const std::string& tool) {
  return std::ranges::find(disallowed_tools, tool) == disallowed_tools.end();
}