#pragma once

#include "sstream"
#include <iomanip>
#include <memory>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace depthlog {

// thread-local depth state
inline thread_local int g_depth = 0;

struct Scope {
  Scope() { ++g_depth; }
  Scope(const Scope &) = delete;
  Scope &operator=(const Scope &) = delete;
  ~Scope() {
    if (g_depth > 0)
      --g_depth;
  }
};

inline int depth() { return g_depth; }

// Custom pattern flag: %D => current thread-local depth
class depth_flag final : public spdlog::custom_flag_formatter {
public:
  void format(const spdlog::details::log_msg &, const std::tm &,
              spdlog::memory_buf_t &dest) override {
    fmt::format_to(std::back_inserter(dest), "{}", g_depth);
  }

  std::unique_ptr<spdlog::custom_flag_formatter> clone() const override {
    return spdlog::details::make_unique<depth_flag>();
  }
};

// Installs a formatter globally via spdlog::set_formatter().
// Pattern emits logfmt-like output.
inline void install_depth_flag(
    std::string pattern =
        R"(ts="%Y-%m-%dT%H:%M:%S.%e%z" level=%l depth=%D tid=%t file="%s" line=%# func="%!" msg="%v")") {
  auto fmtter = spdlog::details::make_unique<spdlog::pattern_formatter>();
  fmtter->add_flag<depth_flag>('D');
  fmtter->set_pattern(std::move(pattern));
  spdlog::set_formatter(std::move(fmtter));
}

// util
inline const std::string make_log_filename(const std::string &prefix) {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);

  std::tm tm{};
  localtime_r(&t, &tm);

  std::ostringstream oss;
  oss << prefix << std::put_time(&tm, "_%Y%m%d_%H%M%S") << ".log";
  return oss.str();
};

constexpr auto max_size = 20ull * 1024 * 1024 * 1024; // 20GB
constexpr auto max_files = 1;

inline void init(const std::string &log_file_prefix) {
  // --- sinks ---
  auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      depthlog::make_log_filename(log_file_prefix), max_size, max_files);

  auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

  // --- logger with both sinks ---
  auto lg = std::make_shared<spdlog::logger>(
      "main", spdlog::sinks_init_list{file_sink, stdout_sink});

  spdlog::set_default_logger(lg);

  // Install logfmt-ish pattern including depth=%D
  /* *INDENT-OFF* */
  depthlog::install_depth_flag(
      R"(ts="%Y-%m-%dT%H:%M:%S.%e%z" level=%l depth=%D tid=%t file="%s" line=%# func="%!" msg="%v")");
  /* *INDENT-ON* */

  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);
}

} // namespace depthlog

// RAII scope helper
#define DEPTHLOG_SCOPE() ::depthlog::Scope depthlog_scope_##__LINE__
