# Examples

## REST API server

```cpp
#include <cxxapi.hxx>

// Some async handler
boost::asio::awaitable<cxxapi::http::response_t> async_handler(cxxapi::http::http_ctx_t ctx) {
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

// Some sync handler
cxxapi::http::response_t sync_handler(cxxapi::http::http_ctx_t ctx) {
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
    boost::asio::io_context io_ctx;

    auto fut = boost::asio::co_spawn(
        io_ctx,

        [&]() -> boost::asio::awaitable<void> {
            auto boost_connection = std::make_shared<boost::redis::connection>(io_ctx);

            redis_cfg_t redis_cfg;

            {
                redis_cfg.m_host = "127.0.0.1";
                redis_cfg.m_port = "6379";

                redis_cfg.m_log_level = boost::redis::logger::level::err;
            }

            c_redis redis{};

            redis.init(redis_cfg);

            {
                auto connection_cfg = c_redis::connection_t::cfg_t{};

                {
                    connection_cfg.m_host = redis_cfg.m_host;
                    connection_cfg.m_port = redis_cfg.m_port;

                    connection_cfg.m_log_level = boost::redis::logger::level::err;
                }

                auto connection = c_redis::connection_t(
                    std::move(connection_cfg),

                    std::move(boost_connection),

                    redis,

                    io_ctx
                );

                auto established = co_await connection.establish();

                if (!established)
                    throw std::runtime_error("Failed to establish connection");

                {
                    auto key = "WOOOPY";

                    auto key_exists = co_await connection.exists(key);

                    EXPECT_FALSE(key_exists);

                    auto try_key_delete = co_await connection.del(key);

                    EXPECT_FALSE(try_key_delete);

                    auto try_key_set = co_await connection.set(key, "WOOOPY");

                    EXPECT_TRUE(try_key_set);

                    auto key_exists_2 = co_await connection.exists(key);

                    EXPECT_TRUE(key_exists_2);

                    auto key_value = co_await connection.get<std::string>(key);

                    EXPECT_EQ(key_value, "WOOOPY");

                    auto try_key_delete_2 = co_await connection.del(key);

                    EXPECT_TRUE(try_key_delete_2);

                    auto key_exists_3 = co_await connection.exists(key);

                    EXPECT_FALSE(key_exists_3);
                }

                connection.shutdown();
            }

            redis.shutdown();

            co_return;
        },

        boost::asio::use_future
    );

    io_ctx.run();

    fut.get();

    io_ctx.stop();
}

int main() {
    redis_awaitable();

    return 0;
}
```

## Redis connection pool integration

```cpp
#include <cxxapi.hxx>

void redis_awaitable() {
    boost::asio::io_context io_ctx;

    auto fut = boost::asio::co_spawn(
        io_ctx,

        [&]() -> boost::asio::awaitable<void> {
            redis_cfg_t redis_cfg;

            {
                redis_cfg.m_host = "127.0.0.1";
                redis_cfg.m_port = "6379";

                redis_cfg.m_log_level = boost::redis::logger::level::err;
            }

            c_redis redis{};

            redis.init(redis_cfg);

            {
                c_redis::c_connection_pool::pool_cfg_t pool_cfg{};

                {
                    pool_cfg.m_initial_connections = 1;
                    pool_cfg.m_max_connections = 3;
                }

                auto pool = std::make_shared<c_redis::c_connection_pool>(
                    std::move(pool_cfg),

                    redis,

                    io_ctx
                );

                auto result = co_await pool->init();

                if (!result)
                    throw std::runtime_error("Failed to initialize connection pool");

                {
                    auto conn_opt = co_await pool->acquire_connection();

                    if (!conn_opt.has_value())
                        throw std::runtime_error("Failed to acquire connection");

                    if (conn_opt.has_value()) {
                        auto& conn = conn_opt.value();

                        if (!conn)
                            throw std::runtime_error("Failed to acquire connection");

                        auto alive_result = co_await conn->alive(true);

                        if (!alive_result)
                            throw std::runtime_error("Failed to check connection");
                    }
                }

                pool->shutdown();
            }

            redis.shutdown();

            co_return;
        },

        boost::asio::use_future
    );

    io_ctx.run();

    fut.get();

    io_ctx.stop();
}

int main() {
    redis_awaitable();

    return 0;
}
```