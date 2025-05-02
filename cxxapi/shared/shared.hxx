/**
 * @file shared.hxx
 * @brief Common includes, macros, and shared namespace for the CXXAPI.
 */

#ifndef CXXAPI_SHARED_HXX
#define CXXAPI_SHARED_HXX

#include <atomic>
#include <chrono>
#include <coroutine>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <regex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <boost/asio.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/stream_file.hpp>
#include <boost/url.hpp>

#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>

#include <boost/system/system_error.hpp>

#include <boost/unordered_map.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <boost/filesystem.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <boost/iostreams/device/mapped_file.hpp>

#include <boost/lexical_cast.hpp>

#ifdef CXXAPI_USE_REDIS_IMPL
#include <boost/redis.hpp>
#endif // CXXAPI_USE_REDIS_IMPL
// ...

#if defined(_WIN32)
#include <stdlib.h>
#endif // _WIN32

#ifdef __MSVC_COMPILER__
/** @brief Forces function inlining. */
#define CXXAPI_INLINE __forceinline

/** @brief Prevents function inlining. */
#define CXXAPI_NOINLINE __declspec(noinline)
#elif __CLANG_COMPILER__ || __GCC_COMPILER__ || __INTEL_COMPILER__
/** @brief Forces function inlining. */
#define CXXAPI_INLINE __attribute__((always_inline)) inline

/** @brief Prevents function inlining. */
#define CXXAPI_NOINLINE __attribute__((noinline))
#else
/** @brief Forces function inlining. */
#define CXXAPI_INLINE inline

/** @brief Prevents function inlining. */
#define CXXAPI_NOINLINE
#endif // ...

#ifdef CXXAPI_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif // CXXAPI_USE_NLOHMANN_JSON

#ifdef CXXAPI_USE_GLAZE_JSON
#include <glaze/glaze.hpp>
#endif // CXXAPI_USE_GLAZE_JSON

#ifdef CXXAPI_USE_LOGGING_IMPL
#include "logging/logging.hxx"
#endif // CXXAPI_USE_LOGGING_IMPL

#ifdef CXXAPI_USE_REDIS_IMPL
#include "redis/redis.hxx"
#endif // CXXAPI_USE_REDIS_IMPL

#include "json_traits/json_traits.hxx"

/**
 * @namespace shared
 * @brief Contains shared types, utilities, and configuration used across the CXXAPI.
 *
 * This namespace is intended for common components that are reused by multiple modules,
 * such as logging, Redis integration, and platform-specific macros.
 */
namespace shared {
    // ...
}

#endif // CXXAPI_SHARED_HXX