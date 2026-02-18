// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/worker_pool.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "gn/switches.h"
#include "util/build_config.h"
#include "util/sys_info.h"

namespace {

int GetThreadCount() {
  std::string thread_count =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueString(
          switches::kThreads);

  // See if an override was specified on the command line.
  int result;
  if (!thread_count.empty() && base::StringToInt(thread_count, &result) &&
      result >= 1) {
    return result;
  }

  // Almost all CPUs now are hyperthreaded.
  int num_cores = NumberOfProcessors() / 2;

#if defined(OS_WIN)
  // Experiments on Windows show that 8 threads is a good value for a 12-core
  // machine, whereas anything over 12-14 threads on a 64-core machine gets
  // progressively worse as the thread count increases.
  return std::min(std::max(num_cores - 1, 8), 14);
#else
  // Use logical processor count - 1, capped at a reasonable high value
  // to avoid excessive contention.
  return std::min(std::max(NumberOfProcessors() - 1, 8), 32);
#endif
}

}  // namespace

WorkerPool::WorkerPool() : WorkerPool(GetThreadCount()) {}

WorkerPool::WorkerPool(size_t thread_count) : should_stop_processing_(false) {
  threads_.reserve(thread_count);
  for (size_t i = 0; i < thread_count; ++i)
    threads_.emplace_back([this]() { Worker(); });
}

WorkerPool::~WorkerPool() {
  {
    std::unique_lock<std::mutex> queue_lock(queue_mutex_);
    should_stop_processing_ = true;
  }

  pool_notifier_.notify_all();

  for (auto& task_thread : threads_) {
    task_thread.join();
  }
}

void WorkerPool::PostTask(std::function<void()> work) {
  {
    std::unique_lock<std::mutex> queue_lock(queue_mutex_);
    CHECK(!should_stop_processing_);
    task_queue_.emplace(std::move(work));
  }

  pool_notifier_.notify_one();
}

void WorkerPool::Worker() {
  for (;;) {
    std::function<void()> task;

    {
      std::unique_lock<std::mutex> queue_lock(queue_mutex_);

      pool_notifier_.wait(queue_lock, [this]() {
        return (!task_queue_.empty()) || should_stop_processing_;
      });

      if (should_stop_processing_ && task_queue_.empty())
        return;

      task = std::move(task_queue_.front());
      task_queue_.pop();
    }

    task();
  }
}
