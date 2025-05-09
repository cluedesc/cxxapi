/**
 * @file middleware.hxx
 * @brief Defines the middleware interface for HTTP request processing.
 */

#ifndef CXXAPI_MIDDLEWARE_HXX
#define CXXAPI_MIDDLEWARE_HXX

namespace cxxapi::middleware {
    /**
     * @brief Base class for HTTP middleware components.
     *
     * Middleware components process HTTP requests in a chain, with each
     * component having the ability to modify the request, generate a response,
     * or pass control to the next middleware in the chain.
     */
    class c_base_middleware {
      public:
        /**
         * @brief Processes an HTTP request.
         *
         * Handles the HTTP request and either generates a response or
         * delegates to the next middleware in the chain.
         *
         * @param request The HTTP request to process.
         * @param next Function to call the next middleware in the chain.
         * @return Awaitable that resolves to an HTTP response.
         */
        virtual boost::asio::awaitable<http::response_t> handle(
            const http::request_t& request,

            std::function<boost::asio::awaitable<http::response_t>(const http::request_t&)> next
        ) = 0;

        /**
         * @brief Virtual destructor.
         */
        virtual ~c_base_middleware() = default;
    };

    /**
     * @brief Shared pointer type for middleware components.
     *
     * This type alias provides a convenient way to manage middleware instances
     * with automatic memory management through shared pointers.
     */
    using middleware_t = std::shared_ptr<c_base_middleware>;
}

#include "cors/cors.hxx"

#endif // CXXAPI_MIDDLEWARE_HXX