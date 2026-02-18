// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TARGET_BITSET_H_
#define TOOLS_GN_TARGET_BITSET_H_

#include <vector>
#include <stdint.h>

class TargetBitSet {
 public:
  TargetBitSet() = default;
  explicit TargetBitSet(size_t target_count) {
    size_t word_count = (target_count + 63) / 64;
    bits_.resize(word_count, 0);
  }

  void Add(int id) {
    if (id < 0) return;
    size_t word_idx = id / 64;
    size_t bit_idx = id % 64;
    if (word_idx >= bits_.size()) {
      bits_.resize(word_idx + 1, 0);
    }
    bits_[word_idx] |= (1ULL << bit_idx);
  }

  bool Contains(int id) const {
    if (id < 0) return false;
    size_t word_idx = id / 64;
    size_t bit_idx = id % 64;
    if (word_idx >= bits_.size()) {
      return false;
    }
    return (bits_[word_idx] & (1ULL << bit_idx)) != 0;
  }

  void Union(const TargetBitSet& other) {
    if (other.bits_.size() > bits_.size()) {
      bits_.resize(other.bits_.size(), 0);
    }
    for (size_t i = 0; i < other.bits_.size(); ++i) {
      bits_[i] |= other.bits_[i];
    }
  }

  bool empty() const {
    for (uint64_t word : bits_) {
      if (word != 0) return false;
    }
    return true;
  }

 private:
  std::vector<uint64_t> bits_;
};

#endif  // TOOLS_GN_TARGET_BITSET_H_
