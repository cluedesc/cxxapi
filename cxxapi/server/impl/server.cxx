#include <cxxapi.hxx>

namespace cxxapi::server {
    c_server::c_server(c_cxxapi& api, const std::string& host, std::int32_t port)
        : m_cxxapi(api),
          m_io_ctx(),
          m_port(port),
          m_acceptor(m_io_ctx, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(host), port)) {
        boost::system::error_code error_code{};

        {
            boost::asio::ip::tcp::acceptor::reuse_address option(true);

            m_acceptor.set_option(option, error_code);

            if (error_code) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                g_logging->log(e_log_level::warning, "[Server] Failed to set REUSE_ADDRESS option: {}", error_code.message());
#endif // CXXAPI_USE_LOGGING_IMPL

                error_code = {};
            }
        }

        const auto& cfg = m_cxxapi.cfg();

        {
#if defined(SO_REUSEPORT)
            boost::asio::detail::socket_option::boolean<SOL_SOCKET, SO_REUSEPORT> reuse_port(true);

            m_acceptor.set_option(reuse_port, error_code);

            if (error_code) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                g_logging->log(e_log_level::warning, "[Server] Failed to set SO_REUSEPORT option: {}", error_code.message());
#endif // CXXAPI_USE_LOGGING_IMPL

                error_code = {};
            }
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

    void c_server::start(std::int32_t workers_count) {
        m_running.store(true, std::memory_order_release);

        try {
            const auto workers = (workers_count <= 0u) ? std::thread::hardware_concurrency() : workers_count;

            if (workers_count <= 0) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                g_logging->log(e_log_level::debug, "[Server] Overriding workers count to {} based on hardware concurrency", workers);
#endif // CXXAPI_USE_LOGGING_IMPL
            }

            auto acceptors_count = workers / 4u;
            auto regular_workers = workers - acceptors_count;

            if (acceptors_count <= 0u && workers > 1u) {
                acceptors_count = 1u;
                regular_workers = workers - 1u;
            }

#ifdef CXXAPI_USE_LOGGING_IMPL
            g_logging->log(
                e_log_level::debug,

                "[Server] Spawning {} acceptor threads and {} regular worker threads",

                acceptors_count, regular_workers
            );
#endif // CXXAPI_USE_LOGGING_IMPL

            for (std::int32_t i{}; i < acceptors_count; ++i) {
                boost::asio::co_spawn(m_io_ctx, do_accept(), boost::asio::detached);
            }

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

        while (true) {
            if (!m_running.load(std::memory_order_relaxed))
                break;

            try {
                boost::system::error_code error_code{};

                boost::asio::ip::tcp::socket socket(m_io_ctx);

                co_await m_acceptor.async_accept(socket, boost::asio::redirect_error(boost::asio::use_awaitable, error_code));

                if (error_code) {
                    if (error_code == boost::asio::error::operation_aborted)
                        break;

                    continue;
                }

                {
                    const auto& cfg = m_cxxapi.cfg();

                    if (cfg.m_socket.m_tcp_no_delay) {
                        boost::asio::ip::tcp::no_delay no_delay_option(true);

                        socket.set_option(no_delay_option, error_code);
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
                    m_io_ctx,

                    [self = shared_from_this(), sock = std::move(socket)]() mutable -> boost::asio::awaitable<void> {
                        co_await client_t(std::move(sock), self->m_cxxapi, *self).start();
                    },

                    [](std::exception_ptr eptr) {
                        try {
                            if (eptr)
                                std::rethrow_exception(eptr);
                        }
#ifdef CXXAPI_USE_LOGGING_IMPL
                        catch (const boost::system::system_error& e) {
                            g_logging->log(
                                e_log_level::error,

                                "[Server] Boost system error in accepting client: code={}, category={}, message={}",

                                e.code().value(), e.code().category().name(), e.what()
                            );
                        }
                        catch (const std::exception& e) {
                            g_logging->log(e_log_level::error, "[Server] Exception in accepting client: {}", e.what());
                        }
                        catch (...) {
                            g_logging->log(e_log_level::error, "[Server] Unknown exception in accepting client");
                        }
#else
                        catch (const boost::system::system_error& e) {
                            std::cerr << fmt::format(
                                "[Server] Boost system error in accepting client: code={}, category={}, message={}",

                                e.code().value(), e.code().category().name(), e.what()
                            ) << "\n";
                        }
                        catch (const std::exception& e) {
                            std::cerr << fmt::format("[Server] Exception while accepting client: {}", e.what()) << "\n";
                        }
                        catch (...) {
                            std::cerr << "[Server] Unknown exception in session";
                        }
#endif // CXXAPI_USE_LOGGING_IMPL
                    }
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