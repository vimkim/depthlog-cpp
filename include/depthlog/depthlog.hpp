#pragma once

#include "spdlog/sinks/ansicolor_sink.h"
#include "sstream"
#include <iomanip>
#include <memory>
#include <spdlog/details/log_msg.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/ansicolor_sink.h>
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
  explicit stderr_indent_color_sink_mt(std::size_t spaces_per_depth = 4,
                                       std::string fn_color = "cyan")
      : spaces_per_depth_(spaces_per_depth),
        fn_color_code_(std::move(fn_color)) {}

  void set_spaces_per_depth(std::size_t v) noexcept { spaces_per_depth_ = v; }
  void set_fn_color(std::string color) {
    fn_color_code_ = std::move(color);
  } // e.g. "cyan", "yellow", "bright_magenta"

  void log(const spdlog::details::log_msg &msg) override {
    // Fast path: no indent and no funcname decoration needed.
    const int d = depthlog::depth();
    const std::size_t indent =
        (d > 0) ? static_cast<std::size_t>(d) * spaces_per_depth_ : 0;

    const spdlog::string_view_t fn = msg.source.funcname;
    const bool has_fn = fn.size() > 0;

    if (indent == 0 && !has_fn) {
      spdlog::sinks::ansicolor_stderr_sink_mt::log(msg);
      return;
    }

    // Build: "<spaces><colored funcname>: <original payload>"
    spdlog::memory_buf_t buf;

    if (indent) {
      // Avoid allocating a std::string for spaces.
      // 64 is arbitrary; chunked append keeps it efficient.
      static constexpr char kSpaces[64] =
          "                                                               ";
      std::size_t remaining = indent;
      while (remaining) {
        const std::size_t n = (remaining > sizeof(kSpaces) - 1)
                                  ? (sizeof(kSpaces) - 1)
                                  : remaining;
        buf.append(kSpaces, kSpaces + n);
        remaining -= n;
      }
    }

    if (has_fn) {
      append_ansi_color_code_(buf, fn_color_code_);
      buf.append(fn.data(), fn.data() + fn.size());
      buf.append(reset.data(), reset.data() + reset.size());

      // separator
      buf.push_back(':');
      buf.push_back(' ');
    }

    buf.append(msg.payload.data(), msg.payload.data() + msg.payload.size());

    // Preserve msg metadata; just swap payload.
    spdlog::details::log_msg msg2 = msg;
    msg2.payload = spdlog::string_view_t(buf.data(), buf.size());

    // Delegate so the formatter still honors %^...%$ for the rest of the line.
    spdlog::sinks::ansicolor_stderr_sink_mt::log(msg2);
  }

private:
  static void append_spaces_(spdlog::memory_buf_t &buf, std::size_t n) {
    // 64 visible spaces (NOT NUL-terminated); avoid string-literal size issues.
    static constexpr char kSpaces[] = "                                        "
                                      "                        "; // 64
    static_assert(sizeof(kSpaces) == 65, "kSpaces must be 64 spaces + NUL");

    while (n) {
      const std::size_t chunk = (n > 64) ? 64 : n;
      buf.append(kSpaces, kSpaces + chunk);
      n -= chunk;
    }
  }

  static void append_reset_(spdlog::memory_buf_t &buf) {
    // ANSI SGR reset
    static constexpr char kReset[] = "\x1b[0m";
    buf.append(kReset, kReset + (sizeof(kReset) - 1));
  }

  static void append_ansi_color_code_(spdlog::memory_buf_t &buf,
                                      const std::string &name) {
    // Minimal named-color mapping (extend as you like).
    // Uses standard SGR color codes. "bright_*" uses 90-97.
    const char *code = nullptr;

    if (name == "black")
      code = "\x1b[30m";
    else if (name == "red")
      code = "\x1b[31m";
    else if (name == "green")
      code = "\x1b[32m";
    else if (name == "yellow")
      code = "\x1b[33m";
    else if (name == "blue")
      code = "\x1b[34m";
    else if (name == "magenta")
      code = "\x1b[35m";
    else if (name == "cyan")
      code = "\x1b[36m";
    else if (name == "white")
      code = "\x1b[37m";
    else if (name == "bright_black")
      code = "\x1b[90m";
    else if (name == "bright_red")
      code = "\x1b[91m";
    else if (name == "bright_green")
      code = "\x1b[92m";
    else if (name == "bright_yellow")
      code = "\x1b[93m";
    else if (name == "bright_blue")
      code = "\x1b[94m";
    else if (name == "bright_magenta")
      code = "\x1b[95m";
    else if (name == "bright_cyan")
      code = "\x1b[96m";
    else if (name == "bright_white")
      code = "\x1b[97m";
    // If unknown or empty -> no color

    if (!code)
      return;
    buf.append(code, code + std::char_traits<char>::length(code));
  }

private:
  std::size_t spaces_per_depth_{4};
  std::string fn_color_code_{"cyan"};
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

  stderr_sink->set_pattern(R"(%H:%M:%S [%^%1!L%$] %20s:%-6# | %v)");

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
