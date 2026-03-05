#ifndef TOOLS_GN_BAZEL_GENERATOR_H_
#define TOOLS_GN_BAZEL_GENERATOR_H_

#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <ostream>

#include "base/files/file_path.h"
#include "gn/label.h"
#include "gn/scope.h"
#include "gn/source_dir.h"
#include "gn/value.h"

struct BazelTarget {
  Label toolchain;
  std::string rule;
  std::string content;

  BazelTarget(Label toolchain, std::string rule, std::string content)
      : toolchain(toolchain), rule(rule), content(content) {}

  void Generate(const std::string& name, std::ostream& os) const;
  std::string FullName(const std::string& name) const;
};

class BazelPackageGenerator {
 public:
  void AddTarget(const std::string& rule,
                 const Label& label,
                 const Scope* scope);
  void Generate(const base::FilePath& build_file_path);

 private:
  std::mutex target_mutex_;
  std::map<std::string, std::vector<BazelTarget>> targets_;
};

class BazelGenerator {
 public:
  BazelGenerator() = default;
  BazelPackageGenerator& GetPackage(const SourceDir& source_dir);
  void Generate(const base::FilePath& base_dir);

 private:
  std::mutex packages_mutex_;
  std::unordered_map<std::string, BazelPackageGenerator> packages_;
};

extern BazelGenerator bazel_generator;

bool IsToolAllowed(const std::string& tool);

#endif  // TOOLS_GN_BAZEL_GENERATOR_H_
