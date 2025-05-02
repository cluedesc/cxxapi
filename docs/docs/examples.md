# Examples

## REST API server

```cpp
#include <cxxapi.hxx>

// Some async handler
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

cxxapi::http::response_t sync_handler(cxxapi::http::http_ctx_t&& ctx) {
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

        return response;
    }

    auto response = cxxapi::http::json_response_t{
        cxxapi::http::json_t::json_obj_t{
            {"id", num}
        },

        cxxapi::http::e_status::ok
    };

    return response;
}

int main() {
    // Create cxxapi configuration
    auto cxxapi_cfg = cxxapi::cxxapi_cfg_t{
        .m_host = "127.0.0.1",
        .m_port = "8080",

        .m_logger.m_level = cxxapi::e_log_level::info,

        .m_server.m_max_connections = 16192u,
        .m_server.m_workers = 8u
    };

    // Create cxxapi instance
    auto api = cxxapi::c_cxxapi();

    { // Add async method
        api.add_method(
            cxxapi::http::e_method::get,

            "/async/{num}",

            async_handler
        );
    }

    { // Add sync method
        api.add_method(
            cxxapi::http::e_method::get,

            "/sync/{num}",

            sync_handler
        );
    }

    {
        // Start cxxapi
        api.start(std::move(cxxapi_cfg));

        {
            // ...
        }

        // Wait for cxxapi to finish (blocking main thread until cxxapi is stopped)
        api.wait();
    }

    return 0;
}
```

## Middleware integration

```cpp
int main() {
    // Create cxxapi configuration
    auto cxxapi_cfg = cxxapi::cxxapi_cfg_t{
        // ...
    };

    // Create cxxapi instance
    auto api = cxxapi::c_cxxapi();

    { // CORS middleware (for example)
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
        // Start cxxapi
        api.start(std::move(cxxapi_cfg));

        {
            // ...
        }

        // Wait for cxxapi to finish (blocking main thread until cxxapi is stopped)
        api.wait();
    }

    return 0;
}
```

## Redis integration

```cpp
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