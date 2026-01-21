option(DEPTHLOG_FETCH_SPDLOG "Fetch spdlog if not found" ON)
option(DEPTHLOG_ENABLE "Enable depthlog logging" ON)

if(DEPTHLOG_FETCH_SPDLOG)
  include(FetchContent)
  find_package(spdlog QUIET CONFIG)
  if(NOT spdlog_FOUND)
    # Ensure fetched dependencies build with PIC (needed if they end up in a .so)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    # spdlog options (avoid surprises)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_PIC ON CACHE BOOL "" FORCE)

    # If spdlog builds/uses bundled fmt, ensure it is PIC as well
    # (Some setups use FMT_* options; harmless if ignored)
    set(FMT_DOC OFF CACHE BOOL "" FORCE)
    set(FMT_TEST OFF CACHE BOOL "" FORCE)
    set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
      spdlog
      GIT_REPOSITORY https://github.com/gabime/spdlog.git
      GIT_TAG v1.17.0
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

target_link_libraries(depthlog INTERFACE
  $<BUILD_INTERFACE:spdlog::spdlog>
  $<INSTALL_INTERFACE:spdlog::spdlog>
)

# DEPTHLOG_ENABLE & Debug mode -> trace
# DEPTHLOG_ENABLE & non-Debug mode -> info
# DEPTHLOG_ENABLE off -> off
target_compile_definitions(depthlog INTERFACE
  $<$<AND:$<BOOL:${DEPTHLOG_ENABLE}>,$<CONFIG:Debug>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>
  $<$<AND:$<BOOL:${DEPTHLOG_ENABLE}>,$<NOT:$<CONFIG:Debug>>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>
  $<$<NOT:$<BOOL:${DEPTHLOG_ENABLE}>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_OFF>
)

target_compile_features(depthlog INTERFACE cxx_std_17)

