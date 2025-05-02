/**
 * @file server.hxx
 * @brief Server implementation for handling client connections.
 */

#ifndef CXXAPI_SERVER_HXX
#define CXXAPI_SERVER_HXX

#include "client/client.hxx"

namespace cxxapi::server {
    /**
     * @brief Main server class that manages client connections.
     *
     * Handles incoming connections, manages worker threads, and coordinates
     * with the main CXXAPI instance for request processing.
     */
    class c_server : public std::enable_shared_from_this<c_server> {
      public:
        /**
         * @brief Constructs a new server instance.
         * @param api Reference to the main CXXAPI instance
         * @param host Hostname or IP address to bind to
         * @param port Port number to listen on
         */
        c_server(c_cxxapi& api, const std::string& host, std::int32_t port);

      public:
        /**
         * @brief Destructor that ensures server is stopped.
         */
        CXXAPI_INLINE ~c_server() { stop(); }

      public:
        /**
         * @brief Starts the server with the specified number of worker threads.
         * @param workers_count Number of worker threads to start
         * @throws boost::system::system_error If server fails to start
         */
        void start(std::int32_t workers_count);

        /**
         * @brief Stops the server and all client connections.
         */
        void stop();

      public:
        /**
         * @brief Gets the IO context used by the server.
         * @return Reference to the IO context
         */
        CXXAPI_INLINE auto& io_ctx() { return m_io_ctx; }

        /**
         * @brief Checks if the server is running.
         * @param m Memory order for the atomic load
         * @return true if server is running, false otherwise
         */
        CXXAPI_INLINE const auto running(const std::memory_order& m) const { return m_running.load(m); }

      private:
        /**
         * @brief Asynchronously accepts incoming client connections.
         * @return Awaitable that completes when a connection is accepted
         */
        boost::asio::awaitable<void> do_accept();

      private:
        /** @brief Reference to the main CXXAPI instance. */
        c_cxxapi& m_cxxapi;

        /** @brief IO context for handling asynchronous operations. */
        boost::asio::io_context m_io_ctx;
        
        /** @brief TCP acceptor for incoming connections. */
        boost::asio::ip::tcp::acceptor m_acceptor;

        /** @brief Atomic flag indicating if server is running. */
        std::atomic_bool m_running{false};

        /** @brief Port number the server is listening on. */
        std::int32_t m_port{};

        /** @brief Thread pool for handling client connections. */
        std::unique_ptr<boost::asio::thread_pool> m_thread_pool{};
    };
}

#endif // CXXAPI_SERVER_HXX
