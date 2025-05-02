/**
 * @file cors.hxx
 * @brief CORS middleware for CXXAPI.
 *
 * This file defines the CORS middleware for handling Cross-Origin Resource Sharing
 * in the CXXAPI framework.  It allows configuration of allowed origins, methods,
 * headers, and credentials to manage CORS requests and responses.
 */

#ifndef CXXAPI_MIDDLEWARE_CORS_HXX
#define CXXAPI_MIDDLEWARE_CORS_HXX

#ifdef CXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL
namespace cxxapi::middleware::cors {
    /**
     * @brief Configuration options for the CORS middleware.
     *
     * This struct holds the settings for controlling the behavior of the CORS
     * middleware. It allows specifying allowed origins, methods, headers,
     * exposed headers, credentials handling, and maximum age for preflight
     * requests.
     */
    struct cors_options_t {
        /**
         * @brief Default constructor for cors_options_t.
         */
        CXXAPI_INLINE cors_options_t()
            : m_allow_all_origins(false),
              m_allowed_origins(),

              m_allow_all_methods(false),
              m_allowed_methods(),

              m_allow_all_headers(false),
              m_allowed_headers(),

              m_exposed_headers(),

              m_allow_credentials(true),
              m_max_age(86400) {
        }

      public:
        /** @brief List of allowed origins.  Use "*" to allow all origins. */
        std::vector<std::string> m_allowed_origins{};

        /** @brief List of allowed HTTP methods. Use "*" to allow all methods. */
        std::vector<std::string> m_allowed_methods{};

        /** @brief List of allowed HTTP headers. Use "*" to allow all headers. */
        std::vector<std::string> m_allowed_headers{};

        /** @brief List of headers exposed to the client. */
        std::vector<std::string> m_exposed_headers{};

        /** @brief Whether to allow credentials (cookies, authorization headers, etc.). */
        bool m_allow_credentials{};

        /** Maximum age (in seconds) for preflight requests. */
        int m_max_age{};

      public:
        /** @brief Flag indicating whether all origins are allowed.  Set internally if m_allowed_origins contains "*". */
        bool m_allow_all_origins{};

        /** @brief Flag indicating whether all methods are allowed. Set internally if m_allowed_methods contains "*". */
        bool m_allow_all_methods{};

        /** @brief Flag indicating whether all headers are allowed. Set internally if m_allowed_headers contains "*". */
        bool m_allow_all_headers{};
    };

    /**
     * @brief CORS middleware class.
     *
     * This middleware handles CORS requests and responses based on the provided
     * configuration options.  It intercepts requests, adds appropriate CORS
     * headers to responses, and handles preflight OPTIONS requests.
     */
    class c_cors_middleware : public c_base_middleware {
      public:
        /**
         * @brief Constructs a CORS middleware instance.
         * @param options Configuration options for the CORS middleware.
         */
        c_cors_middleware(cors_options_t options = cors_options_t{});

        /**
         * @brief Destructor for the CORS middleware.
         */
        ~c_cors_middleware() override;

      public:
        /**
         * @brief Handles an incoming HTTP request.
         *
         * This method intercepts the request and adds CORS headers to the response
         * based on the configured options.  It also handles preflight OPTIONS requests.
         *
         * @param request The incoming HTTP request.
         * @param next The next middleware in the chain.
         * @return The HTTP response after processing.
         */
        boost::asio::awaitable<http::response_t> handle(
            const http::request_t& request,

            std::function<boost::asio::awaitable<http::response_t>(const http::request_t&)> next
        ) override;

      private:
        /**
         * @brief Checks if the given origin is allowed.
         * @param origin The origin to check.
         * @return True if the origin is allowed, false otherwise.
         */
        bool is_origin_allowed(const std::string& origin) const;

        /**
         * @brief Adds CORS headers to the response.
         * @param response The HTTP response to modify.
         * @param origin The origin of the request.
         */
        void add_cors_headers(http::response_t& response, const std::string& origin) const;

      private:
        /** @brief CORS configuration options. */
        cors_options_t m_options{};

        /** @brief Set of allowed origins (for faster lookups). */
        std::unordered_set<std::string> m_origins_set{};
    };
}
#endif // CXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL

#endif // CXXAPI_MIDDLEWARE_CORS_HXX