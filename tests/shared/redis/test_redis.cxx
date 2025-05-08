#ifdef CXXAPI_USE_REDIS_IMPL

#include <gtest/gtest.h>

#include <cxxapi.hxx>

using namespace shared;

TEST(RedisTest, InitNotThrow) {
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

                EXPECT_TRUE(established);

                auto is_alive = co_await connection.alive();

                EXPECT_TRUE(is_alive);

                connection.shutdown();
            }

            redis.shutdown();

            co_return;
        },

        boost::asio::use_future
    );

    io_ctx.run();

    EXPECT_NO_THROW(fut.get());

    io_ctx.stop();
}

TEST(RedisTest, InitFailed) {
    boost::asio::io_context io_ctx;

    auto fut = boost::asio::co_spawn(
        io_ctx,

        [&]() -> boost::asio::awaitable<void> {
            auto boost_connection = std::make_shared<boost::redis::connection>(io_ctx);

            redis_cfg_t redis_cfg;

            {
                redis_cfg.m_host = "127.0.0.1";
                redis_cfg.m_port = "6378";

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

                EXPECT_FALSE(established);

                auto is_alive = co_await connection.alive();

                EXPECT_FALSE(is_alive);

                connection.shutdown();
            }

            redis.shutdown();

            co_return;
        },

        boost::asio::use_future
    );

    io_ctx.run();

    EXPECT_NO_THROW(fut.get());

    io_ctx.stop();
}

TEST(RedisTest, PoolInitSuccess) {
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

                EXPECT_TRUE(result);

                pool->shutdown();
            }

            redis.shutdown();

            co_return;
        },

        boost::asio::use_future
    );

    io_ctx.run();

    EXPECT_NO_THROW(fut.get());

    io_ctx.stop();
}

TEST(RedisTest, PoolInitFailure) {
    boost::asio::io_context io_ctx;

    auto fut = boost::asio::co_spawn(
        io_ctx,

        [&]() -> boost::asio::awaitable<void> {
            redis_cfg_t redis_cfg;

            {
                redis_cfg.m_host = "127.0.0.1";
                redis_cfg.m_port = "6378";

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

                EXPECT_FALSE(result);

                pool->shutdown();
            }

            redis.shutdown();

            co_return;
        },

        boost::asio::use_future
    );

    io_ctx.run();

    EXPECT_NO_THROW(fut.get());

    io_ctx.stop();
}

TEST(RedisTest, PoolInitAcquireAndReleaseConnection) {
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

                EXPECT_TRUE(result);

                {
                    auto conn_opt = co_await pool->acquire_connection();

                    EXPECT_TRUE(conn_opt.has_value());

                    if (conn_opt.has_value()) {
                        auto& conn = conn_opt.value();

                        EXPECT_TRUE(conn);

                        auto alive_result = co_await conn->alive(true);

                        EXPECT_TRUE(alive_result);
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

    EXPECT_NO_THROW(fut.get());

    io_ctx.stop();
}

TEST(RedisTest, PoolMaxConnectionsLimit) {
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
                    pool_cfg.m_initial_connections = 2;
                    pool_cfg.m_max_connections = 5;
                }

                auto pool = std::make_shared<c_redis::c_connection_pool>(
                    std::move(pool_cfg),

                    redis,

                    io_ctx
                );

                auto result = co_await pool->init();

                EXPECT_TRUE(result);

                {
                    std::vector<std::optional<c_redis::c_connection_pool::scoped_connection_t>> connections;

                    for (std::size_t i{}; i < pool_cfg.m_max_connections; ++i) {
                        auto conn = co_await pool->acquire_connection();

                        EXPECT_TRUE(conn.has_value());

                        connections.push_back(std::move(conn));
                    }

                    auto extra_conn = co_await pool->acquire_connection();

                    EXPECT_FALSE(extra_conn.has_value());

                    connections.clear();

                    auto conn = co_await pool->acquire_connection();

                    EXPECT_TRUE(conn.has_value());
                }

                pool->shutdown();
            }

            redis.shutdown();

            co_return;
        },

        boost::asio::use_future
    );

    io_ctx.run();

    EXPECT_NO_THROW(fut.get());

    io_ctx.stop();
}

TEST(RedisTest, PoolConnectionReacquisition) {
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
                    pool_cfg.m_max_connections = 1;
                }

                auto pool = std::make_shared<c_redis::c_connection_pool>(
                    std::move(pool_cfg),

                    redis,

                    io_ctx
                );

                auto result = co_await pool->init();

                EXPECT_TRUE(result);

                {
                    auto conn_opt = co_await pool->acquire_connection();

                    EXPECT_TRUE(conn_opt.has_value());
                }

                {
                    auto conn_opt = co_await pool->acquire_connection();

                    EXPECT_TRUE(conn_opt.has_value());

                    if (conn_opt.has_value()) {
                        auto& conn = conn_opt.value();

                        auto alive_result = co_await conn->alive(true);

                        EXPECT_TRUE(alive_result);
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

    EXPECT_NO_THROW(fut.get());

    io_ctx.stop();
}

TEST(RedisTest, ConnectionBasicMethods) {
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

                EXPECT_TRUE(established);

                auto is_alive = co_await connection.alive();

                EXPECT_TRUE(is_alive);

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

    EXPECT_NO_THROW(fut.get());

    io_ctx.stop();
}

TEST(RedisTest, ConnectionOtherMethods) {
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

                EXPECT_TRUE(established);

                auto is_alive = co_await connection.alive();

                EXPECT_TRUE(is_alive);

                {
                    auto key = "WOOOPY";

                    co_await connection.del(key);

                    auto len1 = co_await connection.lpush(key, "one");

                    EXPECT_EQ(len1, 1);

                    auto len2 = co_await connection.lpush(key, "two");

                    EXPECT_EQ(len2, 2);

                    auto len3 = co_await connection.lpush(key, "three");

                    EXPECT_EQ(len3, 3);

                    auto list_full = co_await connection.lrange(key, 0, -1);

                    EXPECT_EQ(list_full.size(), 3);

                    if (list_full.size() == 3) {
                        EXPECT_EQ(list_full[0], "three");
                        EXPECT_EQ(list_full[1], "two");
                        EXPECT_EQ(list_full[2], "one");

                        auto trim_ok = co_await connection.ltrim(key, 0, 1);

                        EXPECT_TRUE(trim_ok);

                        auto list_trimmed = co_await connection.lrange(key, 0, -1);

                        EXPECT_EQ(list_trimmed.size(), 2);

                        if (list_trimmed.size() == 2) {
                            EXPECT_EQ(list_trimmed[0], "three");
                            EXPECT_EQ(list_trimmed[1], "two");

                            auto expire_ok = co_await connection.expire(key, 10);

                            EXPECT_TRUE(expire_ok);

                            auto ttl1 = co_await connection.ttl(key);

                            EXPECT_GT(ttl1, 0);
                        }
                    }

                    auto try_to_delete = co_await connection.del(key);

                    EXPECT_TRUE(try_to_delete);
                }

                connection.shutdown();
            }

            redis.shutdown();

            co_return;
        },

        boost::asio::use_future
    );

    io_ctx.run();

    EXPECT_NO_THROW(fut.get());

    io_ctx.stop();
}

#endif // CXXAPI_USE_REDIS_IMPL