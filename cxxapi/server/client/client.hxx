/**
 * @file client.hxx
 * @brief Client connection handling for the CXXAPI server.
 */

#ifndef CXXAPI_SERVER_CLIENT_HXX
#define CXXAPI_SERVER_CLIENT_HXX

namespace cxxapi {
    class c_cxxapi;

    namespace server {
        class c_server;

        /**
         * @brief Represents a client connection to the server.
         *
         * Handles incoming requests and manages the connection lifecycle.
         */
        struct client_t {
            /**
             * @brief Constructs a new client session.
             * @param socket TCP socket for the connection
             * @param api Reference to the main CXXAPI instance
             * @param server Reference to the server instance
             */
            CXXAPI_INLINE client_t(boost::asio::ip::tcp::socket&& socket, c_cxxapi& api, c_server& server)
                : m_cxxapi(api),
                  m_server(server),
                  m_socket(std::move(socket)),
                  m_web_socket(m_socket) {
            }

          public:
            /**
             * @brief Starts processing client requests.
             * @return Awaitable that completes when the client disconnects
             * @throws base_exception_t If an error occurs during processing
             */
            boost::asio::awaitable<void> start();

          private:
            /**
             * @brief Handles an individual HTTP request.
             * @param req The HTTP request to handle
             * @return Awaitable that completes when the request is processed
             * @throws base_exception_t If request handling fails
             */
            boost::asio::awaitable<void> handle_request(http::request_t&& req);

          private:
            /** @brief Flag indicating if the connection should be closed. */
            bool m_close{false};

            /** @brief Reference to the main CXXAPI instance. */
            c_cxxapi& m_cxxapi;

            /** @brief Reference to the server instance. */
            c_server& m_server;

            /** @brief TCP socket for the connection. */
            boost::asio::ip::tcp::socket m_socket;

            /** @brief Buffer for incoming data. */
            boost::beast::flat_buffer m_buffer;

            /** @brief WebSocket stream for the connection. */
            boost::beast::websocket::stream<boost::asio::ip::tcp::socket&> m_web_socket;

            /** @brief Parsed HTTP request. */
            boost::beast::http::message<true, boost::beast::http::string_body> m_parsed_request;
        };
    }
}

#endif // CXXAPI_SERVER_CLIENT_HXX
