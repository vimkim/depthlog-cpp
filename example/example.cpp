#include <depthlog/depthlog.hpp>
#include <spdlog/sinks/basic_file_sink.h>

void foo() {
  DEPTHLOG_SCOPE();
  SPDLOG_INFO("enter foo");
}

int main() {
  auto lg = spdlog::basic_logger_mt("main", "app.log");
  spdlog::set_default_logger(lg);

  depthlog::install_depth_flag();

  foo();
}

