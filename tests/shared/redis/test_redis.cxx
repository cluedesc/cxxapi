#ifdef CXXAPI_USE_REDIS_IMPL

#include <gtest/gtest.h>

#include <cxxapi.hxx>

using namespace shared;

TEST(RedisTest, InitNotThrow) {
    boost::asio::io_context io_ctx;

    redis_cfg_t cfg;

    {
        cfg.m_host = "127.0.0.1";
        cfg.m_port = "6379";

        cfg.m_log_level = boost::redis::logger::level::err;
    }

    c_redis redis;

    auto fut = boost::asio::co_spawn(
        io_ctx,

        [&]() -> boost::asio::awaitable<void> {
            EXPECT_NO_THROW({ redis.init(cfg); });

            EXPECT_TRUE(redis.alive());

            co_return redis.shutdown();
        },

        boost::asio::use_future
    );

    io_ctx.run();

    EXPECT_NO_THROW(fut.get());

    io_ctx.stop();
}

TEST(RedisTest, InitFailed) {
    boost::asio::io_context io_ctx;

    redis_cfg_t cfg;

    {
        cfg.m_host = "127.0.0.1";
        cfg.m_port = "6378";

        cfg.m_log_level = boost::redis::logger::level::err;
    }

    c_redis redis;

    auto fut = boost::asio::co_spawn(
        io_ctx,

        [&]() -> boost::asio::awaitable<void> {
            redis.init(cfg);

            EXPECT_FALSE(redis.alive());

            co_return redis.shutdown();
        },

        boost::asio::use_future
    );

    io_ctx.run();

    EXPECT_NO_THROW(fut.get());

    io_ctx.stop();
}

TEST(RedisTest, BasicMethods) {
    boost::asio::io_context io_ctx;

    redis_cfg_t cfg;

    {
        cfg.m_host = "127.0.0.1";
        cfg.m_port = "6379";

        cfg.m_log_level = boost::redis::logger::level::err;
    }

    c_redis redis;

    auto fut = boost::asio::co_spawn(
        io_ctx,

        [&]() -> boost::asio::awaitable<void> {
            EXPECT_NO_THROW({ redis.init(cfg); });

            EXPECT_TRUE(redis.alive());

            {
                auto key = "WOOOPY";

                auto key_exists = co_await redis.exists(key);

                EXPECT_FALSE(key_exists);

                auto try_key_delete = co_await redis.del(key);

                EXPECT_FALSE(try_key_delete);

                auto try_key_set = co_await redis.set(key, "WOOOPY");

                EXPECT_TRUE(try_key_set);

                auto key_exists_2 = co_await redis.exists(key);

                EXPECT_TRUE(key_exists_2);

                auto key_value = co_await redis.get<std::string>(key);

                EXPECT_EQ(key_value, "WOOOPY");

                auto try_key_delete_2 = co_await redis.del(key);

                EXPECT_TRUE(try_key_delete_2);

                auto key_exists_3 = co_await redis.exists(key);

                EXPECT_FALSE(key_exists_3);
            }

            co_return redis.shutdown();
        },

        boost::asio::use_future
    );

    io_ctx.run();

    EXPECT_NO_THROW(fut.get());

    io_ctx.stop();
}

TEST(RedisTest, OtherMethods) {
    boost::asio::io_context io_ctx;

    redis_cfg_t cfg;

    {
        cfg.m_host = "127.0.0.1";
        cfg.m_port = "6379";

        cfg.m_log_level = boost::redis::logger::level::err;
    }

    c_redis redis;

    auto fut = boost::asio::co_spawn(
        io_ctx,

        [&]() -> boost::asio::awaitable<void> {
            EXPECT_NO_THROW({ redis.init(cfg); });

            EXPECT_TRUE(redis.alive());

            {
                auto key = "WOOOPY";

                co_await redis.del(key);

                auto len1 = co_await redis.lpush(key, "one");

                EXPECT_EQ(len1, 1);

                auto len2 = co_await redis.lpush(key, "two");

                EXPECT_EQ(len2, 2);

                auto len3 = co_await redis.lpush(key, "three");

                EXPECT_EQ(len3, 3);

                auto list_full = co_await redis.lrange(key, 0, -1);

                EXPECT_EQ(list_full.size(), 3);

                if (list_full.size() == 3) {
                    EXPECT_EQ(list_full[0], "three");
                    EXPECT_EQ(list_full[1], "two");
                    EXPECT_EQ(list_full[2], "one");

                    auto trim_ok = co_await redis.ltrim(key, 0, 1);

                    EXPECT_TRUE(trim_ok);

                    auto list_trimmed = co_await redis.lrange(key, 0, -1);

                    EXPECT_EQ(list_trimmed.size(), 2);

                    if (list_trimmed.size() == 2) {
                        EXPECT_EQ(list_trimmed[0], "three");
                        EXPECT_EQ(list_trimmed[1], "two");

                        auto expire_ok = co_await redis.expire(key, 10);

                        EXPECT_TRUE(expire_ok);

                        auto ttl1 = co_await redis.ttl(key);

                        EXPECT_GT(ttl1, 0);
                    }
                }

                auto try_to_delete = co_await redis.del(key);

                EXPECT_TRUE(try_to_delete);
            }

            co_return redis.shutdown();
        },

        boost::asio::use_future
    );

    io_ctx.run();

    EXPECT_NO_THROW(fut.get());

    io_ctx.stop();
}

#endif // CXXAPI_USE_REDIS_IMPL
