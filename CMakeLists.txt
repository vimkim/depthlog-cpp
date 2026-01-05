option(DEPTHLOG_FETCH_SPDLOG "Fetch spdlog if not found" ON)

if(DEPTHLOG_FETCH_SPDLOG)
  include(FetchContent)
  find_package(spdlog QUIET CONFIG)
  if(NOT spdlog_FOUND)
    FetchContent_Declare(
      spdlog
      GIT_REPOSITORY https://github.com/gabime/spdlog.git
      GIT_TAG v1.14.1
    )
    FetchContent_MakeAvailable(spdlog)
  endif()
else()
  find_package(spdlog REQUIRED CONFIG)
endif()

add_library(depthlog INTERFACE)
add_library(depthlog::depthlog ALIAS depthlog)

target_include_directories(depthlog INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# IMPORTANT: Use generator expressions so install/export doesn't try to "own" spdlog
target_link_libraries(depthlog INTERFACE
  $<BUILD_INTERFACE:spdlog::spdlog>
  $<INSTALL_INTERFACE:spdlog::spdlog>
)

target_compile_features(depthlog INTERFACE cxx_std_17)

