// example/example.cpp
//
// Demonstrates depthlog:
//
// - DEPTHLOG_SCOPE() increments a thread-local depth counter on entry,
//   decrements automatically on scope exit (RAII).
// - depth is printed via spdlog pattern flag %D.
// - Shows nested calls and early-return paths.
// - Shows that each thread has independent depth tracking.

#include <depthlog/depthlog.hpp>

#include <spdlog/sinks/basic_file_sink.h>

#include <thread>
#include <vector>

// A helper to show depth changes on different control-flow paths.
static void leaf_ok() {
  DEPTHLOG_SCOPE();
  SPDLOG_INFO("leaf_ok: work done");
}

static void leaf_early_return(bool bail) {
  DEPTHLOG_SCOPE();
  SPDLOG_INFO("leaf_early_return: entered");
  if (bail) {
    SPDLOG_WARN("leaf_early_return: bailing out early");
    return; // depth decremented automatically here
  }
  SPDLOG_INFO("leaf_early_return: continuing");
}

static void middle(int n) {
  DEPTHLOG_SCOPE();
  SPDLOG_INFO("middle: n={}", n);

  leaf_ok();
  leaf_early_return(n % 2 == 0);

  SPDLOG_INFO("middle: leaving");
}

static void top() {
  DEPTHLOG_SCOPE();
  SPDLOG_INFO("top: enter");

  for (int i = 0; i < 3; ++i) {
    middle(i);
  }

  SPDLOG_INFO("top: exit");
}

static void thread_entry(int idx) {
  // Depth is thread-local; each thread starts at depth 0.
  DEPTHLOG_SCOPE();
  SPDLOG_INFO("thread_entry: idx={}", idx);

  top();

  SPDLOG_INFO("thread_entry: done idx={}", idx);
}

int main() {
  depthlog::init("main");
  SPDLOG_INFO("main: starting");

  // Main thread call tree
  top();

  // Multi-thread call trees (depth should be independent per thread)
  std::vector<std::thread> threads;
  for (int i = 0; i < 2; ++i) {
    threads.emplace_back(thread_entry, i);
  }
  for (auto &t : threads)
    t.join();

  SPDLOG_INFO("main: done");
  return 0;
}
