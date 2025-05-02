/**
 * @file route.hxx
 * @brief Core routing functionality for handling HTTP requests.
 */

#ifndef CXXAPI_ROUTE_HXX
#define CXXAPI_ROUTE_HXX

#include "internal/internal.hxx"

namespace cxxapi::route {
    /**
     * @brief Base interface for HTTP route handlers.
     */
    struct route_t {
        virtual ~route_t() = default;

        /**
         * @brief Handle an HTTP request synchronously.
         * @param ctx HTTP context containing request data.
         * @return HTTP response to send back.
         */
        virtual http::response_t handle(http::http_ctx_t&& ctx) const = 0;

        /**
         * @brief Handle an HTTP request asynchronously.
         * @param ctx HTTP context containing request data.
         * @return Awaitable HTTP response to send back.
         */
        virtual boost::asio::awaitable<http::response_t> handle_async(http::http_ctx_t&& ctx) const = 0;

        /**
         * @brief Check if this route uses asynchronous handling.
         * @return true if asynchronous, false if synchronous.
         */
        virtual bool is_async() const = 0;
    };

    /**
     * @brief Concrete route implementation using a function handler.
     * @tparam _fn_t Type of the handler function.
     */
    template <typename _fn_t>
    struct fn_route_t : public route_t {
        CXXAPI_INLINE constexpr fn_route_t() = default;

        /**
         * @brief Construct a new route with handler function.
         * @param method HTTP method to handle.
         * @param path URL path pattern to match.
         * @param fn Handler function to invoke.
         */
        CXXAPI_INLINE fn_route_t(const http::e_method& method, const http::path_t& path, _fn_t&& fn)
            : m_method(method), m_path(path), m_fn(std::move(fn)) {}

      public:
        CXXAPI_INLINE fn_route_t(const fn_route_t&) = delete;

        CXXAPI_INLINE fn_route_t& operator=(const fn_route_t&) = delete;

        CXXAPI_INLINE fn_route_t(fn_route_t&&) noexcept = default;

        CXXAPI_INLINE fn_route_t& operator=(fn_route_t&&) noexcept = default;

      public:
        /**
         * @brief Handle request synchronously.
         * @param ctx HTTP context containing request data.
         * @return HTTP response to send back.
         * @throws base_exception_t if called on async handler.
         */
        CXXAPI_INLINE http::response_t handle(http::http_ctx_t&& ctx) const override {
            if constexpr (internal::sync_handler_c<_fn_t>)
                return m_fn(std::move(ctx));

            throw base_exception_t("Asynchronous handler called synchronously");
        }

        /**
         * @brief Handle request asynchronously.
         * @param ctx HTTP context containing request data.
         * @return Awaitable HTTP response to send back.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<http::response_t> handle_async(http::http_ctx_t&& ctx) const override {
            if constexpr (internal::async_handler_c<_fn_t>)
                co_return co_await m_fn(std::move(ctx));

            co_return handle(std::move(ctx));
        }

        /**
         * @brief Check if this route uses async handling.
         * @return true if async handler, false if sync handler.
         */
        CXXAPI_INLINE bool is_async() const override { return internal::async_handler_c<_fn_t>; }

      private:
        /** @brief Handler function for this route. */
        _fn_t m_fn{};

        /** @brief URL path pattern to match. */
        http::path_t m_path{};

        /** @brief HTTP method to handle. */
        http::e_method m_method{};
    };
}

#endif // CXXAPI_ROUTE_HXX
