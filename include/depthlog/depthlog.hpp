#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/pattern_formatter.h>

#include <memory>

namespace depthlog {

// thread-local depth state
inline thread_local int g_depth = 0;

struct Scope {
  Scope() { ++g_depth; }
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;
  ~Scope() { if (g_depth > 0) --g_depth; }
};

inline int depth() { return g_depth; }

// Custom pattern flag: %D => current thread-local depth
class depth_flag final : public spdlog::custom_flag_formatter {
public:
  void format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& dest) override {
    fmt::format_to(std::back_inserter(dest), "{}", g_depth);
  }

  std::unique_ptr<spdlog::custom_flag_formatter> clone() const override {
    return spdlog::details::make_unique<depth_flag>();
  }
};

// Installs a formatter globally via spdlog::set_formatter().
// Pattern emits logfmt-like output.
inline void install_depth_flag(std::string pattern =
  R"(ts="%Y-%m-%dT%H:%M:%S.%e%z" level=%l depth=%D tid=%t file="%s" line=%# func="%!" msg="%v")")
{
  auto fmtter = std::make_unique<spdlog::pattern_formatter>();
  fmtter->add_flag<depth_flag>('D');
  fmtter->set_pattern(std::move(pattern));
  spdlog::set_formatter(std::move(fmtter));
}

} // namespace depthlog

// RAII scope helper
#define DEPTHLOG_SCOPE() ::depthlog::Scope depthlog_scope_##__LINE__

