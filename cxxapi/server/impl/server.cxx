#include <cxxapi.hxx>

namespace cxxapi::server {
    c_server::c_server(c_cxxapi& api, const std::string& host, const std::int32_t port)
        : m_cxxapi(api),
          m_port(port),
          m_acceptor(m_io_ctx, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(host), port)) {
        boost::system::error_code error_code{};

        const auto& cfg = m_cxxapi.cfg();

        {
            m_acceptor.non_blocking(cfg.m_server.m_acceptor_nonblocking, error_code);

            if (error_code) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                g_logging->log(e_log_level::warning, "[Server] Failed to set acceptor nonblocking option: {}", error_code.message());
#endif // CXXAPI_USE_LOGGING_IMPL

                error_code = {};
            }
        }

        {
#if defined(SO_REUSEADDR)
            const boost::asio::detail::socket_option::boolean<SOL_SOCKET, SO_REUSEADDR> reuse_addr(true);

            m_acceptor.set_option(reuse_addr, error_code);

            if (error_code) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                g_logging->log(e_log_level::warning, "[Server] Failed to set REUSEADDR option: {}", error_code.message());
#endif // CXXAPI_USE_LOGGING_IMPL

                error_code = {};
            }
#endif
        }

        {
#if defined(SO_REUSEPORT)
            const boost::asio::detail::socket_option::boolean<SOL_SOCKET, SO_REUSEPORT> reuse_port(true);

            m_acceptor.set_option(reuse_port, error_code);

            if (error_code) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                g_logging->log(e_log_level::warning, "[Server] Failed to set REUSEPORT option: {}", error_code.message());
#endif // CXXAPI_USE_LOGGING_IMPL

                error_code = {};
            }
#endif
        }

        {
#if defined(TCP_FASTOPEN)
            constexpr auto qlen = 5;

            setsockopt(m_acceptor.native_handle(), IPPROTO_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen));
#endif
        }

        {
            m_acceptor.listen(cfg.m_server.m_max_connections, error_code);

            if (error_code) {
                throw exceptions::server_exception_t(fmt::format("Failed to listen: {}", error_code.message()));
            }
        }

#ifdef CXXAPI_USE_LOGGING_IMPL
        g_logging->log(e_log_level::debug, "[Server] Acceptor max connections: {}", cfg.m_server.m_max_connections);
#endif // CXXAPI_USE_LOGGING_IMPL
    }

    void c_server::start(const std::int32_t workers_count) {
        m_running.store(true, std::memory_order_release);

        try {
            const auto workers = workers_count <= 0
                                   ? static_cast<std::int32_t>(std::thread::hardware_concurrency())
                                   : workers_count;

            if (workers_count <= 0) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                g_logging->log(e_log_level::debug, "[Server] Overriding workers count to {} based on hardware concurrency", workers);
#endif // CXXAPI_USE_LOGGING_IMPL
            }

            auto acceptors_count = 0;

            if (workers <= 4) {
                acceptors_count = 1;
            }
            else if (workers <= 16) {
                acceptors_count = std::max(2, workers / 6);
            }
            else
                acceptors_count = std::max(3, workers / 8);

            acceptors_count = std::max(1, acceptors_count);

            auto regular_workers = workers - acceptors_count;

            if (regular_workers < 1) {
                regular_workers = 1;

                if (workers == 1)
                    acceptors_count = 1;
            }

#ifdef CXXAPI_USE_LOGGING_IMPL
            g_logging->log(
                e_log_level::debug,

                "[Server] Spawning {} acceptor threads and {} regular worker threads",

                acceptors_count, regular_workers
            );
#endif // CXXAPI_USE_LOGGING_IMPL

            for (std::int32_t i{}; i < acceptors_count; ++i)
                boost::asio::co_spawn(m_io_ctx, do_accept(), boost::asio::detached);

            {
                m_thread_pool = std::make_unique<boost::asio::thread_pool>(workers);

                for (std::int32_t i{}; i < workers; i++) {
                    boost::asio::post(*m_thread_pool, [this] {
                        try {
                            m_io_ctx.run();
                        }
#ifdef CXXAPI_USE_LOGGING_IMPL
                        catch (const boost::system::system_error& e) {
                            g_logging->log(
                                e_log_level::error,

                                "[Server] Boost system error in worker thread: code={}, category={}, message={}",

                                e.code().value(), e.code().category().name(), e.what()
                            );
                        }
                        catch (const std::exception& e) {
                            g_logging->log(e_log_level::error, "[Server] Exception in worker thread: {}", e.what());
                        }
                        catch (...) {
                            g_logging->log(e_log_level::error, "[Server] Unknown exception in worker thread");
                        }
#else
                        catch (const boost::system::system_error& e) {
                            std::cerr << fmt::format(
                                "[Server] Boost system error in worker thread: code={}, category={}, message={}",

                                e.code().value(), e.code().category().name(), e.what()
                            ) << "\n";
                        }
                        catch (const std::exception& e) {
                            std::cerr << fmt::format("[Server] Exception in worker thread: {}", e.what()) << "\n";
                        }
                        catch (...) {
                            std::cerr << "[Server] Unknown exception in worker thread" << "\n";
                        }
#endif // CXXAPI_USE_LOGGING_IMPL
                    });
                }
            }
        }
        catch (const boost::system::system_error& e) {
            throw exceptions::server_exception_t(
                fmt::format(
                    "Boost system error during server start: code={}, category={}, message={}",

                    e.code().value(), e.code().category().name(), e.what()
                )
            );
        }
        catch (const std::exception& e) {
            throw exceptions::server_exception_t(fmt::format("Exception during server start: {}", e.what()));
        }
        catch (...) {
            throw exceptions::server_exception_t(fmt::format("Unknown exception during server start"));
        }
    }

    void c_server::stop() {
        if (!m_running.load(std::memory_order_acquire))
            return;

        m_running.store(false, std::memory_order_release);

        try {
            if (m_acceptor.is_open()) {
                boost::system::error_code error_code{};

                m_acceptor.cancel(error_code);

                if (error_code) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                    g_logging->log(e_log_level::error, "[Server] Failed to cancel acceptor: {}", error_code.message());
#endif // CXXAPI_USE_LOGGING_IMPL

                    error_code = {};
                }

                m_acceptor.close(error_code);

                if (error_code) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                    g_logging->log(e_log_level::error, "[Server] Failed to close acceptor: {}", error_code.message());
#endif // CXXAPI_USE_LOGGING_IMPL
                }
            }

            m_io_ctx.stop();

#if BOOST_VERSION <= 108600
            m_io_ctx.reset();
#endif

            if (m_thread_pool) {
                m_thread_pool->stop();
                m_thread_pool->join();

                m_thread_pool.reset();
            }
        }
        catch (const boost::system::system_error& e) {
            throw exceptions::server_exception_t(
                fmt::format(
                    "Boost system error during server stop: code={}, category={}, message={}",

                    e.code().value(), e.code().category().name(), e.what()
                )
            );
        }
        catch (const std::exception& e) {
            throw exceptions::server_exception_t(fmt::format("Exception during server stop: {}", e.what()));
        }
        catch (...) {
            throw exceptions::server_exception_t("Unknown exception during server stop");
        }
    }

    boost::asio::awaitable<void> c_server::do_accept() {
        if (!m_running.load(std::memory_order_acquire))
            co_return;

        const auto executor = co_await boost::asio::this_coro::executor;

        while (m_running.load(std::memory_order_relaxed)) {
            try {
                boost::system::error_code error_code{};

                boost::asio::ip::tcp::socket socket(m_io_ctx);

                co_await m_acceptor.async_accept(socket, boost::asio::redirect_error(boost::asio::use_awaitable, error_code));

                if (error_code) {
                    if (error_code == boost::asio::error::operation_aborted)
                        co_return;

                    continue;
                }

                {
                    const auto& cfg = m_cxxapi.cfg();

                    if (cfg.m_socket.m_tcp_no_delay) {
                        boost::asio::ip::tcp::no_delay no_delay_option(true);

                        socket.set_option(no_delay_option, error_code);

#if defined(TCP_QUICKACK) && defined(__linux__)
                        int quickack = 1;

                        setsockopt(socket.native_handle(), IPPROTO_TCP, TCP_QUICKACK, &quickack, sizeof(quickack));
#endif
                    }

                    if (cfg.m_socket.m_rcv_buf_size) {
                        boost::asio::socket_base::receive_buffer_size rcv_buf_size(cfg.m_socket.m_rcv_buf_size);

                        socket.set_option(rcv_buf_size, error_code);
                    }

                    if (cfg.m_socket.m_snd_buf_size) {
                        boost::asio::socket_base::send_buffer_size snd_buf_size(cfg.m_socket.m_snd_buf_size);

                        socket.set_option(snd_buf_size, error_code);
                    }

                    if (error_code) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                        g_logging->log(e_log_level::error, "[Server] Failed to set socket option: {}", error_code.message());
#endif // CXXAPI_USE_LOGGING_IMPL

                        boost::system::error_code close_ec{};

                        socket.close(close_ec);

                        if (close_ec) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                            g_logging->log(e_log_level::error, "[Server] Failed to close socket after error: {}", close_ec.message());
#endif
                        }

                        continue;
                    }
                }

                boost::asio::co_spawn(
                    executor,

                    [self = shared_from_this(), sock = std::move(socket)]() mutable -> boost::asio::awaitable<void> {
                        co_await client_t(std::move(sock), self->m_cxxapi, *self).start();
                    },

                    boost::asio::detached
                );
            }
#ifdef CXXAPI_USE_LOGGING_IMPL
            catch (const boost::system::system_error& e) {
                g_logging->log(
                    e_log_level::error,

                    "[Server] Boost system error in acceptor: code={}, category={}, message={}",

                    e.code().value(), e.code().category().name(), e.what()
                );
            }
            catch (const std::exception& e) {
                g_logging->log(e_log_level::error, "[Server] Exception in acceptor: {}", e.what());
            }
            catch (...) {
                g_logging->log(e_log_level::error, "[Server] Unknown exception in acceptor");
            }
#else
            catch (const boost::system::system_error& e) {
                std::cerr << fmt::format(
                    "[Server] Boost system error in acceptor: code={}, category={}, message={}",

                    e.code().value(), e.code().category().name(), e.what()
                ) << "\n";
            }
            catch (const std::exception& e) {
                std::cerr << fmt::format("[Server] Exception in acceptor: {}", e.what()) << "\n";
            }
            catch (...) {
                std::cerr << "[Server] Unknown exception in acceptor";
            }
#endif // CXXAPI_USE_LOGGING_IMPL
        }

        co_return;
    }
}