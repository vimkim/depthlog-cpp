#pragma once

#include "spdlog/sinks/ansicolor_sink.h"
#include "sstream"
#include <iomanip>
#include <memory>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>
#include <string>

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

#include <spdlog/common.h> // spdlog::color_mode
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/base_sink.h>

class stderr_indent_color_sink_mt final
    : public spdlog::sinks::ansicolor_stderr_sink_mt {
public:
  explicit stderr_indent_color_sink_mt(std::size_t spaces_per_depth = 4)
      : spaces_per_depth_(spaces_per_depth) {}

  // NOTE: ansicolor_* sinks override sink::log(), not base_sink::sink_it_()
  void log(const spdlog::details::log_msg &msg) override {
    const int d = depthlog::depth();
    const std::size_t indent =
        (d > 0) ? static_cast<std::size_t>(d) * spaces_per_depth_ : 0;

    if (indent == 0) {
      spdlog::sinks::ansicolor_stderr_sink_mt::log(msg);
      return;
    }

    // Build "<spaces><original payload>"
    std::string spaces(indent, ' ');

    spdlog::memory_buf_t buf;
    buf.append(spaces.data(), spaces.data() + spaces.size());
    buf.append(msg.payload.data(), msg.payload.data() + msg.payload.size());

    // Copy msg and point payload to our temporary buffer
    auto msg2 = msg;
    msg2.payload = spdlog::string_view_t(buf.data(), buf.size());

    // Delegate to base (keeps %^...%$ coloring behavior)
    spdlog::sinks::ansicolor_stderr_sink_mt::log(msg2);
  }

private:
  std::size_t spaces_per_depth_;
};

constexpr auto max_size = 20ull * 1024 * 1024 * 1024; // 20GB
constexpr auto max_files = 1;

inline std::unique_ptr<spdlog::formatter> make_logfmt_formatter() {
  auto f = spdlog::details::make_unique<spdlog::pattern_formatter>();
  f->add_flag<depthlog::depth_flag>('D');
  f->set_pattern(
      R"(ts="%Y-%m-%dT%H:%M:%S.%e%z" level=%l depth=%D tid=%t file="%s" line=%# func="%!" msg="%v")");
  return f;
}

inline void init(const std::string &log_file_prefix) {
  auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      depthlog::make_log_filename(log_file_prefix), max_size, max_files);
  // Set per-sink formatters
  file_sink->set_formatter(make_logfmt_formatter());

  auto stderr_sink = std::make_shared<depthlog::stderr_indent_color_sink_mt>(4);

  stderr_sink->set_pattern(R"(%H:%M:%S.%e [%^%4!L%$] %10s:%# %20! | %v)");

  auto lg = std::make_shared<spdlog::logger>(
      "main", spdlog::sinks_init_list{file_sink, stderr_sink});
  spdlog::set_default_logger(lg);

  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);
}

} // namespace depthlog

// RAII scope helper
#define DEPTHLOG_SCOPE() ::depthlog::Scope depthlog_scope_##__LINE__

// LOG MACROs
#define DEPTHLOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define DEPTHLOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define DEPTHLOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define DEPTHLOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define DEPTHLOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define DEPTHLOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
