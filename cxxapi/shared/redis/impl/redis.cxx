#ifdef CXXAPI_USE_REDIS_IMPL

// ...
#include <boost/redis/src.hpp>
// ...

#include <cxxapi.hxx>

namespace shared {
    bool c_redis::init(const redis_cfg_t& cfg) {
        boost::redis::config redis_cfg{};

        {
            redis_cfg.addr = boost::redis::address(
                cfg.m_host, cfg.m_port
            );

            if (!cfg.m_user.empty())
                redis_cfg.username = cfg.m_user;

            if (!cfg.m_password.empty())
                redis_cfg.password = cfg.m_password;

            {
                if (!cfg.m_client_name.empty())
                    redis_cfg.clientname = cfg.m_client_name;

                if (!cfg.m_health_check_id.empty())
                    redis_cfg.health_check_id = cfg.m_health_check_id;

                if (!cfg.m_log_prefix.empty())
                    redis_cfg.log_prefix = cfg.m_log_prefix;
            }

            {
                redis_cfg.health_check_interval = cfg.m_health_check_interval;
                redis_cfg.reconnect_wait_interval = cfg.m_reconnect_interval;
            }

            m_log_level = cfg.m_log_level;
        }

        {
#ifdef CXXAPI_HAS_LOGGING_IMPL
            m_logger.init(
                cfg.m_cxxapi_logger.m_level,
                cfg.m_cxxapi_logger.m_force_flush,
                cfg.m_cxxapi_logger.m_async,
                cfg.m_cxxapi_logger.m_buffer_size,
                cfg.m_cxxapi_logger.m_strategy
            );
#endif
        }

        {
            auto fut = boost::asio::co_spawn(
                m_io_ctx,

                [this, redis_cfg = std::move(redis_cfg)]() mutable -> boost::asio::awaitable<e_redis_state> {
                    co_return co_await _establish_connection(redis_cfg);
                },

                boost::asio::use_future
            );

            m_thread = std::thread([&]() {
                try {
                    m_io_ctx.run();
                }
                catch (const boost::system::system_error& e) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] exception: {}",

                        e.code().message()
                    );
#else
                    std::cerr << "[redis] exception: " << e.code().message() << std::endl;
#endif
                }
                catch (const std::exception& e) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] exception: {}",

                        e.what()
                    );
#else
                    std::cerr << "[redis] exception: " << e.what() << std::endl;
#endif
                }
                catch (...) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] exception: unknown"
                    );
#else
                    std::cerr << "[redis] exception: unknown " << std::endl;
#endif
                }

                if (m_state != e_redis_state::abort)
                    return;

                if (m_connection) {
                    m_connection->cancel();

                    m_connection.reset();
                }

                if (!m_io_ctx.stopped())
                    m_io_ctx.stop();

                m_state = e_redis_state::disconnected;
            });

            m_state = fut.get();
        }

        return m_state == e_redis_state::connected;
    }

    boost::asio::awaitable<e_redis_state> c_redis::_establish_connection(const boost::redis::config& cfg) {
        try {
            std::optional<boost::system::error_code> connection_error{};

            {
                m_connection = std::make_shared<boost::redis::connection>(m_io_ctx);

                m_connection->async_run(
                    cfg, {m_log_level},

                    [&](const boost::system::error_code& ec) {
                        if (ec) {
                            if (m_connection) {
                                m_connection->cancel();

                                m_connection.reset();
                            }

                            connection_error.emplace(ec);
                        }
                    }
                );
            }

            {
                boost::asio::steady_timer sleep(m_io_ctx);

                sleep.expires_after(std::chrono::milliseconds(150u));

                co_await sleep.async_wait(boost::asio::use_awaitable);
            }

            if (connection_error.has_value()) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                m_logger.log(
                    e_log_level::error,

                    "[redis] _establish_connection() failed: {}",

                    connection_error->message()
                );
#else
                std::cerr << "[redis] _establish_connection() failed: " << connection_error->message() << std::endl;
#endif

                co_return e_redis_state::abort;
            }

            boost::redis::request request{};

            {
                request.push("PING", "PONG");
            }

            boost::redis::response<std::string> response{};

            {
                boost::system::error_code ec{};

                co_await m_connection->async_exec(request, response, boost::asio::redirect_error(boost::asio::use_awaitable, ec));

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] _establish_connection() failed: {}",

                        connection_error->message()
                    );
#else
                    std::cerr << "[redis] _establish_connection() failed: " << connection_error->message() << std::endl;
#endif
                    co_return e_redis_state::abort;
                }
            }

            co_return std::get<0u>(response).value() == "PONG"
                ? e_redis_state::connected
                : e_redis_state::abort;
        }
        catch (...) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                m_logger.log(
                    e_log_level::error,

                    "[redis] _establish_connection() failed: unknown"
                );
#else
                std::cerr << "[redis] _establish_connection() failed: unknown" << std::endl;
#endif
        }
        
        co_return e_redis_state::abort;
    }

    void c_redis::shutdown() {
        if (m_state != e_redis_state::connected
            && m_state != e_redis_state::unknown) {
            if (m_state == e_redis_state::abort) {
                if (!m_io_ctx.stopped())
                    m_io_ctx.stop();

                if (m_thread.joinable())
                    m_thread.join();

                
#ifdef CXXAPI_HAS_LOGGING_IMPL
                m_logger.stop_async();
#endif
            }

            return;
        }

        if (m_connection) {
            m_connection->cancel();

            m_connection.reset();
        }

        if (!m_io_ctx.stopped())
            m_io_ctx.stop();

        if (m_thread.joinable())
            m_thread.join();

#ifdef CXXAPI_HAS_LOGGING_IMPL
                m_logger.stop_async();
#endif

        m_state = e_redis_state::disconnected;
    }
}

#endif // CXXAPI_USE_REDIS_IMPL
