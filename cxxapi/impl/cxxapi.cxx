#include <cxxapi.hxx>

namespace cxxapi {
    void c_cxxapi::start(cxxapi_cfg_t cfg) {
        m_cfg = std::move(cfg);

#ifdef CXXAPI_USE_LOGGING_IMPL
        g_logging->init(
            m_cfg.m_logger.m_level,
            m_cfg.m_logger.m_force_flush,
            m_cfg.m_logger.m_async,
            m_cfg.m_logger.m_buffer_size,
            m_cfg.m_logger.m_strategy
        );
#endif // CXXAPI_USE_LOGGING_IMPL

        if (m_cfg.m_host.compare("localhost") == 0u)
            m_cfg.m_host = "127.0.0.1";

        auto port = 8080u;

        if (auto [ptr, ec] = std::from_chars(m_cfg.m_port.c_str(), m_cfg.m_port.c_str() + m_cfg.m_port.size(), port); ec != std::errc())
            port = 0;

        if (port <= 0) {
#ifdef CXXAPI_USE_LOGGING_IMPL
            g_logging->log(e_log_level::warning, "[Core] Port number '{}' is not supported, using '8080' instead", m_cfg.m_port);
#endif // CXXAPI_USE_LOGGING_IMPL

            port = 8080;

            m_cfg.m_port = "8080";
        }

        if (!boost::filesystem::exists(m_cfg.m_server.m_tmp_dir)) {
            boost::filesystem::create_directory(m_cfg.m_server.m_tmp_dir);

#ifdef CXXAPI_USE_LOGGING_IMPL
            g_logging->log(e_log_level::debug, "[Core] Created tmp directory: {}", m_cfg.m_server.m_tmp_dir.string());
#endif // CXXAPI_USE_LOGGING_IMPL
        }

        init_middlewares_chain();

        m_running.store(true, std::memory_order_release);

#ifdef CXXAPI_USE_LOGGING_IMPL
        g_logging->log(e_log_level::info, "[{}:{}] Starting server...", m_cfg.m_host, m_cfg.m_port);
#endif // CXXAPI_USE_LOGGING_IMPL

        if (m_server = std::make_shared<server::c_server>(*this, m_cfg.m_host, port); m_server) {
            m_signals.emplace(m_server->io_ctx(), SIGINT, SIGTERM, SIGQUIT);

            if (m_signals.has_value()) {
                m_signals->async_wait([&](const boost::system::error_code& err_code, [[maybe_unused]] std::int32_t signo) {
                    if (!err_code)
                        this->stop();
                });
            }

            return m_server->start(m_cfg.m_server.m_workers);
        }
    }

    void c_cxxapi::stop() {
        if (!m_running)
            return;

        m_running.store(false, std::memory_order_release);

        {
            std::lock_guard lock(m_wait_mutex);
        }

        m_wait_cv.notify_all();

        if (m_signals.has_value()) {
            boost::system::error_code error_code{};

            m_signals->cancel(error_code);

            if (error_code) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                g_logging->log(e_log_level::error, "[{}:{}] Failed to cancel signals: {}", m_cfg.m_host, m_cfg.m_port, error_code.message());
#endif // CXXAPI_USE_LOGGING_IMPL
            }
        }

        if (m_server) {
            m_server->stop();
            m_server.reset();

#ifdef CXXAPI_USE_LOGGING_IMPL
            g_logging->log(e_log_level::info, "[{}:{}] Server stopped...", m_cfg.m_host, m_cfg.m_port);
#endif // CXXAPI_USE_LOGGING_IMPL
        }
    }

    void c_cxxapi::wait() {
#ifdef CXXAPI_USE_LOGGING_IMPL
        g_logging->log(
            e_log_level::info,

            "[{}:{}] CXXAPI wait state initiated, thread blocked pending shutdown signal",

            m_cfg.m_host, m_cfg.m_port
        );
#endif // CXXAPI_USE_LOGGING_IMPL

        {
            std::unique_lock lock(m_wait_mutex);

            m_wait_cv.wait(lock, [this] {
                return !m_running.load(std::memory_order_acquire);
            });
        }

#ifdef CXXAPI_USE_LOGGING_IMPL
        g_logging->log(
            e_log_level::info,

            "[{}:{}] CXXAPI wait state terminated, shutdown procedure complete",

            m_cfg.m_host, m_cfg.m_port
        );
#endif // CXXAPI_USE_LOGGING_IMPL
    }

    boost::asio::awaitable<http::response_t> c_cxxapi::_handle_request(http::request_t&& request) const {
        try {
            co_return co_await m_middlewares_chain(request);
        }
#ifdef CXXAPI_USE_LOGGING_IMPL
        catch (const boost::system::system_error& e) {
            g_logging->log(
                e_log_level::error,

                "[Core] Boost system error in handle request: code={}, category={}, message={}",

                e.code().value(), e.code().category().name(), e.what()
            );
        }
        catch (const std::exception& e) {
            g_logging->log(e_log_level::error, "[Core] Exception in handle request outer: {}", e.what());
        }
        catch (...) {
            g_logging->log(e_log_level::error, "[Core] Unknown exception in handle request outer");
        }
#else
        catch (const boost::system::system_error& e) {
            std::cerr << fmt::format(
                "[Core] Boost system error in handle request: code={}, category={}, message={}",

                e.code().value(), e.code().category().name(), e.what()
            ) << "\n";
        }
        catch (const std::exception& e) {
            std::cerr << fmt::format("[Core] Exception in handle request outer: {}", e.what()) << "\n";
        }
        catch (...) {
            std::cerr << "[Core] Unknown exception in handle request outer" << "\n";
        }
#endif // CXXAPI_USE_LOGGING_IMPL

        if (m_cfg.m_http.m_response_class == http::e_response_class::plain) {
            co_return http::response_class_t<http::response_t>::make_response(
                "Internal server error",

                http::e_status::internal_server_error
            );
        }

        co_return http::response_class_t<http::json_response_t>::make_response(
            http::json_t::json_obj_t{
                {"message", "Internal server error"}
            },

            http::e_status::internal_server_error
        );
    }

    void c_cxxapi::init_middlewares_chain() {
        std::function<boost::asio::awaitable<http::response_t>(const http::request_t&)> core = [this](const http::request_t& req)
            -> boost::asio::awaitable<http::response_t> {
            auto opt_pair = m_route_trie.find(req.method(), req.uri());

            if (!opt_pair.has_value()) {
                if (m_cfg.m_http.m_response_class == http::e_response_class::plain) {
                    co_return http::response_class_t<http::response_t>::make_response(
                        "Not found",

                        http::e_status::not_found
                    );
                }

                co_return http::response_class_t<http::json_response_t>::make_response(
                    http::json_t::json_obj_t{
                        {"message", "Not found"}
                    },

                    http::e_status::not_found
                );
            }

            auto [route, params] = opt_pair.value();

            if (!route) {
                if (m_cfg.m_http.m_response_class == http::e_response_class::plain) {
                    co_return http::response_class_t<http::response_t>::make_response(
                        "Internal server error",

                        http::e_status::internal_server_error
                    );
                }

                co_return http::response_class_t<http::json_response_t>::make_response(
                    http::json_t::json_obj_t{
                        {"message", "Internal server error"}
                    },

                    http::e_status::internal_server_error
                );
            }

            auto executor = co_await boost::asio::this_coro::executor;

            auto ctx = co_await http::http_ctx_t::create(
                executor,

                req, params,

                m_cfg.m_server.m_max_chunk_size,
                m_cfg.m_server.m_max_chunk_size_disk,

                m_cfg.m_server.m_max_file_size_in_memory,
                m_cfg.m_server.m_max_files_size_in_memory
            );

            if (route->is_async())
                co_return co_await route->handle_async(std::move(ctx));

            co_return route->handle(std::move(ctx));
        };

        auto chain = std::move(core);

        for (auto& middleware : std::ranges::reverse_view(m_middlewares)) {
            auto next_chain = std::move(chain);

            chain = [middleware, next_chain = std::move(next_chain)](const http::request_t& req) mutable
                -> boost::asio::awaitable<http::response_t> {
                co_return co_await middleware->handle(req, std::move(next_chain));
            };
        }

        m_middlewares_chain = std::move(chain);
    }
}