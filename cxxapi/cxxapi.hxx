/**
 * @file cxxapi.hxx
 * @brief Main public API and configuration structures for the cxxapi.
 */

#ifndef CXXAPI_HXX
#define CXXAPI_HXX

#include "shared/shared.hxx"

/**
 * @namespace cxxapi
 * @brief Main namespace for the CXXAPI.
 */
namespace cxxapi {
#ifdef CXXAPI_USE_LOGGING_IMPL
    /** @brief Alias for the shared logging implementation. */
    using c_logging = shared::c_logging;

    /** @brief Alias for the shared logging level enumeration. */
    using e_log_level = shared::e_log_level;

    /** @brief Global logger instance for cxxapi. */
    inline const auto g_logging = std::make_unique<shared::c_logging>();
#endif // CXXAPI_USE_LOGGING_IMPL

#ifdef CXXAPI_USE_REDIS_IMPL
    /** @brief Alias for the shared Redis client implementation. */
    using c_redis = shared::c_redis;

    /** @brief Alias for the shared Redis configuration structure. */
    using redis_cfg_t = shared::redis_cfg_t;

    /** @brief Global Redis client instance for cxxapi. */
    inline const auto g_redis = std::make_unique<shared::c_redis>();
#endif // CXXAPI_USE_REDIS_IMPL
}

#include "exception/exception.hxx"

#include "http/http.hxx"

#include "route/route.hxx"

#include "middleware/middleware.hxx"

#include "server/server.hxx"

namespace cxxapi {
    /**
     * @brief Configuration parameters for the cxxapi.
     */
    struct cxxapi_cfg_t {
        /** @brief Hostname or IP address for the main service. (default: localhost) */
        std::string m_host{"localhost"};

        /** @brief Port number for the main service. (default: 8080) */
        std::string m_port{"8080"};

        /**
         * @brief Server-specific configuration parameters.
         */
        struct {
            /** @brief Number of worker threads for the server. (default: 4) */
            std::int32_t m_workers{4u};

            /** @brief Maximum number of simultaneous connections. (default: 2048) */
            std::int32_t m_max_connections{2048u};

            /** @brief Maximum request size in bytes. (default: 100 MB) */
            std::size_t m_max_request_size{104857600u};

            /** @brief Maximum chunk size in bytes. (default: 128 KB) */
            std::size_t m_max_chunk_size{131072u};

            /** @brief Maximum chunk size for writing to disk in bytes. (default: 512 KB) */
            std::size_t m_max_chunk_size_disk{524288u};

            /** @brief Maximum file size in memory. (default: 1 MB) */
            std::size_t m_max_file_size_in_memory{1048576u};

            /** @brief Maximum files size in memory. (default: 10 MB) */
            std::size_t m_max_files_size_in_memory{10485760u};

            /** @brief Temp directory for temporary files. (default: /tmp/cxxapi_tmp) */
            boost::filesystem::path m_tmp_dir{"/tmp/cxxapi_tmp"};
        } m_server{};

        /**
         * @brief HTTP-specific configuration parameters.
         */
        struct http_t {
            /** @brief Response class to use for internal HTTP responses. (default: plain) */
            http::e_response_class m_response_class{http::e_response_class::plain};

            /** @brief Keep-alive timeout in seconds. (default: 30 seconds) */
            std::chrono::seconds m_keep_alive_timeout{std::chrono::seconds(30)};
        } m_http{};

        /**
         * @brief Socket-specific configuration parameters.
         */
        struct {
            /** @brief Enable or disable TCP_NODELAY option. (default: true) */
            bool m_tcp_no_delay{true};

            /** @brief Receive buffer size. (default: 512 KB) */
            std::size_t m_rcv_buf_size{524288u};

            /** @brief Send buffer size. (default: 512 KB) */
            std::size_t m_snd_buf_size{524288u};
        } m_socket{};

#ifdef CXXAPI_HAS_LOGGING_IMPL
        /**
         * @brief Configuration for the internal CXXAPI logger.
         */
        struct logger_t {
            /** @brief Minimum severity level to log. (default: info) */
            e_log_level m_level{e_log_level::info};

            /** @brief Whether to flush output immediately after each message. (default: false) */
            bool m_force_flush{false};

            /** @brief Enable asynchronous logging. (default: true) */
            bool m_async{true};

            /** @brief Size of the internal log buffer. (default: 16384) */
            std::size_t m_buffer_size{16384u};

            /** @brief Strategy for handling buffer overflows. (default: discard_oldest) */
            c_logging::e_overflow_strategy m_strategy{c_logging::e_overflow_strategy::discard_oldest};
        };

        /** @brief Logger configuration for CXXAPI. */
        logger_t m_logger{};
#endif
    };

    /**
     * @brief Main API class for the cxxapi.
     *
     * Provides access to configuration and running state.
     */
    class c_cxxapi {
      public:
        /**
         * @brief Default constructor. Initializes configuration and running state.
         */
        CXXAPI_INLINE c_cxxapi() : m_cfg(), m_running(false) {}

      public:
        /**
         * @brief Starts the CXXAPI server with the given configuration.
         * @param cfg Configuration settings.
         */
        void start(cxxapi_cfg_t cfg);

        /**
         * @brief Stops the CXXAPI server.
         */
        void stop();

        /**
         * @brief Waits for the CXXAPI to finish.
         * @note This function blocks until the CXXAPI server is stopped.
         */
        void wait();

      public:
        /**
         * @brief Adds a new route handler for a specific HTTP method and path.
         * @tparam _fn_t Type of the handler function
         * @param method HTTP method to handle
         * @param path URL path pattern to match
         * @param fn Handler function to add
         */
        template <typename _fn_t>
        CXXAPI_INLINE void add_method(
            const http::e_method& method,
            const http::path_t& path,
            _fn_t&& fn
        ) {
            try {
                auto new_route = std::make_shared<route::fn_route_t<std::decay_t<_fn_t>>>(method, path, std::forward<_fn_t>(fn));

                if (!m_route_trie.insert(method, path, std::move(new_route)))
                    throw base_exception_t(fmt::format("Failed to insert route: {} {}", http::method_to_str(method), path));
            }
            catch (const base_exception_t& e) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                g_logging->log(e_log_level::error, "Error adding route: {}", e.what());
#endif // CXXAPI_USE_LOGGING_IMPL
            }
        }

        /**
         * @brief Adds a middleware to the processing chain.
         * @param middleware Middleware instance to add
         */
        CXXAPI_INLINE void add_middleware(middleware::middleware_t middleware) {
            if (m_running.load(std::memory_order_acquire))
                throw base_exception_t("Can't add middleware after server started");

            m_middlewares.push_back(std::move(middleware));
        }

      public:
        /**
         * @brief Internal request handler for processing HTTP requests.
         * @param request HTTP request to handle
         * @return Awaitable that resolves to an HTTP response
         */
        boost::asio::awaitable<http::response_t> _handle_request(http::request_t&& request);

      public:
        /**
         * @brief Get a reference to the configuration.
         * @return Reference to the configuration structure.
         */
        CXXAPI_INLINE auto& cfg() { return m_cfg; }

        /**
         * @brief Get the running state of the API.
         * @return Const reference to the running state flag.
         */
        [[nodiscard]] CXXAPI_INLINE const auto& running() const { return m_running; }

      private:
        /**
         * @brief Initializes the middleware chain.
         *
         * @note This function is called automatically when the API is started.
         */
        void init_middlewares_chain();

      private:
        /** @brief Configuration parameters for the API. */
        cxxapi_cfg_t m_cfg{};

        /** @brief Atomic flag indicating whether the API is running. */
        std::atomic_bool m_running{};

        /** @brief Server instance. */
        std::shared_ptr<server::c_server> m_server;

        /** @brief Trie for routes. */
        route::internal::trie_node_t<std::shared_ptr<route::route_t>> m_route_trie;

        /** @brief Middleware instances. */
        std::vector<middleware::middleware_t> m_middlewares;

        /** @brief Compiled middleware chain. */
        std::function<boost::asio::awaitable<http::response_t>(const http::request_t&)> m_middlewares_chain;

        /** @brief Signal set for handling termination signals. */
        std::optional<boost::asio::signal_set> m_signals;
    };
}

#endif // CXXAPI_HXX
