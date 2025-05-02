# cxxapi

[![Docs](https://img.shields.io/badge/Docs-site-blue.svg)](https://cluedesc.github.io/cxxapi/examples/)
[![Version](https://img.shields.io/badge/Version-1.0.0a-blue.svg)](.)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Build](https://github.com/cluedesc/cxxapi-reworked/actions/workflows/ci.yml/badge.svg)](https://github.com/cluedesc/cxxapi-reworked/actions/workflows/ci.yml)

---

## Overview

**cxxapi** is a modern, high-performance, asynchronous C++20 (optional C++23) web framework and toolkit for building scalable HTTP APIs, servers, and network applications.
It leverages Boost.Asio coroutines, OpenSSL, fmt, nlohmann or Glaze json variant, providing a flexible, efficient, and extensible foundation.  

- **Easy to learn, with a simple and well-organized structure that makes understanding the server architecture straightforward.**
- **Inspired by the popular Python framework FastAPI, this project adopts a similar approach to create a minimalistic yet high-performance framework for building servers.**

**The project is currently in active development, with many essential features to be added in the near future. It will also be continuously improved and supplemented with comprehensive documentation. I welcome your support, feedback, and suggestions to help shape and enhance this promising project.**
---

## Features

- **Asynchronous Core:** Boost.Asio with C++20 coroutines for non-blocking I/O
- **HTTP/1.1 Support:** keep-alive, cookies, chunked transfer, streaming
- **Multipart/Form-Data:** async parsing, in-memory or temp files, easy file access
- **MIME Detection:** automatic, extensible

- **Routing:**
  - Trie-based, supports static and dynamic segments (`/users/{id}`)
  - Sync and async handlers

- **Middleware Pipeline:**
  - Chain of async processors
  - Built-in: CORS, *new ones will be added in the next releases*
  - Easy to extend with custom middleware
 
- **JSON:** seamless integration with nlohmann/json and Glaze
- **Static Files:** MIME detection, ETag, async chunked streaming
- **Streaming Responses:** custom async callbacks
- **Configurable Server:** host, port, workers, socket options, logging options, and etc...
- **Redis Integration:** async client, hashes, lists, strings

- **Advanced Logging:**
  - Async or sync
  - Log levels, overflow strategies
  - Colorized output
 
- **Modern C++:** concepts, templates, constexpr, inline, and other solutions
---

## Examples

Basic REST API server

```C++
#include <cxxapi.hxx>

boost::asio::awaitable<cxxapi::http::response_t> async_handler(cxxapi::http::http_ctx_t&& ctx) {
    auto num = 0u;

    try {
        num = std::stoul(ctx.params().at("num"));
    }
    catch (const std::exception& e) {
        auto response = cxxapi::http::json_response_t{
            cxxapi::http::json_t::json_obj_t{
                {"error", e.what()}
            },

            cxxapi::http::e_status::internal_server_error
        };

        co_return response;
    }

    auto response = cxxapi::http::json_response_t{
        cxxapi::http::json_t::json_obj_t{
            {"id", num}
        },

        cxxapi::http::e_status::ok
    };

    co_return response;
}

int main() {
    auto cxxapi_cfg = cxxapi::cxxapi_cfg_t{
        .m_host = "127.0.0.1",
        .m_port = "8080",

        .m_logger.m_level = cxxapi::e_log_level::info,

        .m_server.m_max_connections = 16192u,
        .m_server.m_workers = 8u
    };

    auto api = cxxapi::c_cxxapi();

    { // optional CORS middleware
        auto cors_cfg = cxxapi::middleware::cors::cors_options_t{};

        {
            cors_cfg.m_allowed_origins = {"*"};
            cors_cfg.m_allowed_methods = {"*"};
            cors_cfg.m_allowed_headers = {"*"};
        }

        auto cors_middleware = std::make_shared<
            cxxapi::middleware::cors::c_cors_middleware>(std::move(cors_cfg));

        api.add_middleware(cors_middleware);
    }

    {
        api.add_method(
            cxxapi::http::e_method::get,

            "/async/{num}",

            async_handler
        );
    }

    {
        api.start(std::move(cxxapi_cfg));

        {
            // ...
        }

        api.wait();
    }

    return 0;
}
```

Redis integration

```C++
#include <cxxapi.hxx>

void redis_awaitable() {
    auto io_ctx = std::make_shared<boost::asio::io_context>();

    auto future = boost::asio::co_spawn(
        *io_ctx,

        []() -> boost::asio::awaitable<void> {
            cxxapi::redis_cfg_t redis_cfg{
                .m_host = "127.0.0.1",
                .m_port = "6379",

                .m_log_level = boost::redis::logger::level::err
            };

            cxxapi::g_redis->init(redis_cfg);

            {
                co_await cxxapi::g_redis->set("key", "value");

                if ((co_await cxxapi::g_redis->exists("key"))) {
                    std::cerr << fmt::format(
                        "Key exists, value: {}",

                        (co_await cxxapi::g_redis->get("key")).value_or("oh no...")
                    ) << "\n";
                }
                else
                    std::cerr << "Key does not exist" << "\n";

                co_await cxxapi::g_redis->del("key");
            }

            cxxapi::g_redis->shutdown();

            co_return;
        },

        boost::asio::use_future
    );

    io_ctx->run();

    future.get();
}

int main() {
    redis_awaitable();

    return 0;
}
```

## Benchmark

Code

```C++
#include <cxxapi.hxx>

int main() {
    auto cxxapi_cfg = cxxapi::cxxapi_cfg_t{
        .m_host = "127.0.0.1",
        .m_port = "8080",

        .m_logger.m_level = cxxapi::e_log_level::info,

        .m_server.m_max_connections = 96000u,
        .m_server.m_workers = 8u
    };

    auto api = cxxapi::c_cxxapi();

    {
        api.add_method(
            cxxapi::http::e_method::get,

            "/root",

            root_handler
        );
    }

    {
        api.start(std::move(cxxapi_cfg));

        {
            // ...
        }

        api.wait();
    }

    return 0;
}
```

Server config

```
CPU: AMD Ryzen 5 7535U
RAM: 16gb DDR5

OS: Ubuntu-24.04
```

Command

```bash
wrk -t4 -c100 -d30s http://localhost:8080/root
```

Result

```
Running 30s test @ http://localhost:8080/root
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   265.53us   80.65us   9.04ms   91.51%
    Req/Sec    87.73k     4.35k  146.72k    73.02%
  10483178 requests in 30.10s, 0.95GB read
Requests/sec: 348284.47
Transfer/sec:     32.22MB
```

## Build and Installation

### Prerequisites

- **CMake >= 3.16**

- **C++ compiler with C++20 support (C++23 required for Glaze JSON)**

- **Boost 1.84.0 or higher (system, filesystem, iostreams components)**

- **OpenSSL**

- **IO-Uring**

- **Installing via git modules**
    - fmt library
    - Nlohmann/json and Glaze JSON

- **Google Test (for building tests)**

- **Linux operating system (Windows is not supported for now, it will be fixed later)**

### CMake Options

cxxapi provides several configuration options that can be enabled or disabled during the build process:

| Option | Description | Default |
|--------|-------------|---------|
| `CXXAPI_USE_NLOHMANN_JSON` | Enable nlohmann/json support | ON |
| `CXXAPI_USE_GLAZE_JSON` | Enable Glaze JSON support (requires C++23) | OFF |
| `CXXAPI_USE_REDIS_IMPL` | Enable Redis implementation support | ON |
| `CXXAPI_USE_LOGGING_IMPL` | Enable Logging implementation support | ON |
| `CXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL` | Enable builtin middlewares (CORS) | ON |
| `CXXAPI_BUILD_TESTS` | Build cxxapi tests | OFF |

### JSON Library Selection

cxxapi requires at least one JSON library to be enabled. You can choose between:
- nlohmann/json (C++20)
- Glaze JSON (C++23)

At least one of `CXXAPI_USE_NLOHMANN_JSON` or `CXXAPI_USE_GLAZE_JSON` must be set to ON.

To enable Glaze JSON (which requires C++23 support):
```bash
cmake -DCXXAPI_USE_GLAZE_JSON=ON ..
```

### Optional Components

To disable Redis support:
```bash
cmake -DCXXAPI_USE_REDIS_IMPL=OFF ..
```

To disable Logging support:
```bash
cmake -DCXXAPI_USE_LOGGING_IMPL=OFF ..
```

To disable builtin middlewares:
```bash
cmake -DCXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL=OFF ..
```

### Building with tests

```bash
cmake -DCXXAPI_BUILD_TESTS=ON -S . -B build && cmake --build build --target all
```

### Installation

Installation via FetchContent from SOURCE_DIR

```cmake
include(FetchContent)

FetchContent_Declare(
    cxxapi
    SOURCE_DIR YOUR_PATH_TO_CXXAPI
)

FetchContent_MakeAvailable(cxxapi)

target_link_libraries(YOUR_TARGET PRIVATE cxxapi::cxxapi)
```

Installation via FetchContent from GIT_REPOSITORY

```cmake
FetchContent_Declare(
    cxxapi
    GIT_REPOSITORY https://github.com/cluedesc/cxxapi.git
    GIT_TAG 1.0.0a
)

FetchContent_MakeAvailable(cxxapi)

target_link_libraries(YOUR_TARGET PRIVATE cxxapi::cxxapi)
```

## License

This project is licensed under the terms of the MIT license.
