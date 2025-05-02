/**
 * @file redis.hxx
 * @brief Redis client interface and configuration structures for shared module.
 */

#ifndef CXXAPI_SHARED_REDIS_HXX
#define CXXAPI_SHARED_REDIS_HXX

namespace shared {
#ifdef CXXAPI_USE_REDIS_IMPL
    /**
     * @brief Configuration parameters for connecting to a Redis server.
     */
    struct redis_cfg_t {
        /** @brief Redis server hostname or IP address. */
        std::string m_host{};

        /** @brief Redis server port number as string. */
        std::string m_port{};

        /** @brief Password for authenticating with Redis server. */
        std::string m_password{};

        /** @brief Username for authenticating with Redis server. */
        std::string m_user{};

        /** @brief Name to identify the Redis client connection. */
        std::string m_client_name{};

        /** @brief Identifier used for health check operations. */
        std::string m_health_check_id{};

        /** @brief Prefix for log messages related to Redis operations. */
        std::string m_log_prefix{};

        /**
         * @brief Interval between health check commands.
         *
         * If set to zero duration, health checks are disabled.
         */
        std::chrono::steady_clock::duration m_health_check_interval{std::chrono::seconds{5}};

        /**
         * @brief Interval to wait before attempting to reconnect after disconnection.
         *
         * If set to zero duration, automatic reconnection is disabled.
         */
        std::chrono::steady_clock::duration m_reconnect_interval{std::chrono::seconds{0}};

        /** @brief Log verbosity level for Redis operations. */
        boost::redis::logger::level m_log_level{boost::redis::logger::level::info};

#ifdef CXXAPI_HAS_LOGGING_IMPL
        /**
         * @brief Configuration for the internal Redis logger.
         */
        struct cxxapi_logger_t {
            /** @brief Minimum severity level to log. */
            e_log_level m_level{e_log_level::info};

            /** @brief Whether to flush output immediately after each message. */
            bool m_force_flush{false};

            /** @brief Enable asynchronous logging. */
            bool m_async{true};

            /** @brief Size of the internal log buffer. */
            std::size_t m_buffer_size{16384u};

            /** @brief Strategy for handling buffer overflows. */
            c_logging::e_overflow_strategy m_strategy{c_logging::e_overflow_strategy::discard_oldest};
        };

        /** @brief Logger configuration for Redis client. */
        cxxapi_logger_t m_cxxapi_logger{};
#endif
    };

    /**
     * @brief Redis connection state.
     */
    enum struct e_redis_state : std::int16_t {
        unknown = -1,     ///< State is unknown or uninitialized.
        relax = 0,        ///< Idle state, not connected or disconnected.
        connected = 1,    ///< Successfully connected to Redis server.
        disconnected = 2, ///< Disconnected from Redis server.
        abort = 3         ///< Connection attempt failed or aborted.
    };

    /**
     * @brief Redis client wrapper providing asynchronous connection management.
     */
    class c_redis {
      public:
        /** @brief Alias for shared pointer to Boost.Redis connection. */
        using connection_t = std::shared_ptr<boost::redis::connection>;

        /**
         * @brief Alias for optional Redis response value.
         * @tparam _type_t Underlying value type.
         */
        template <typename _type_t>
        using opt_value_t = std::optional<_type_t>;

      public:
#ifdef CXXAPI_HAS_LOGGING_IMPL
        /**
         * @brief Default constructor. Initializes internal state.
         */
        CXXAPI_INLINE c_redis()
            : m_thread(),
              m_io_ctx(),
              m_connection(),
              m_log_level(boost::redis::logger::level::info),
              m_state(e_redis_state::relax),
              m_logger() {
        }
#else
        /**
         * @brief Default constructor. Initializes internal state.
         */
        CXXAPI_INLINE c_redis()
            : m_thread(),
              m_io_ctx(),
              m_connection(),
              m_log_level(boost::redis::logger::level::info),
              m_state(e_redis_state::relax) {
        }
#endif

        /**
         * @brief Destructor. Calls shutdown() to clean up resources.
         */
        CXXAPI_INLINE ~c_redis() { shutdown(); }

      public:
        /**
         * @brief Initialize the Redis client and establish connection asynchronously.
         * @param cfg Redis connection configuration parameters.
         * @return True if connection was successfully established, false otherwise.
         */
        bool init(const redis_cfg_t& cfg);

        /**
         * @brief Shutdown the Redis client, cancel operations, and release resources.
         */
        void shutdown();

      public:
        /**
         * @brief Execute a Redis command asynchronously and capture the error code.
         * @tparam _tuple_t Types of the response tuple elements.
         * @param request Redis command request to execute (rvalue).
         * @param response Response object to store the result.
         * @return Error code from the operation (success or failure).
         */
        template <typename... _tuple_t>
        CXXAPI_NOINLINE boost::asio::awaitable<boost::system::error_code> async_exec(
            const boost::redis::request& request,

            boost::redis::response<_tuple_t...>& response
        ) {
            if (!alive())
                co_return boost::system::error_code(boost::asio::error::operation_aborted);

            boost::system::error_code ec{};

            {
                co_await m_connection->async_exec(std::move(request), response, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            }

            co_return ec;
        }

      public:
        /**
         * @brief Set a string value for a key with optional expiration.
         * @param key Redis key.
         * @param value String value to set.
         * @param expire Optional expiration time in seconds.
         * @return True if the operation was successful, false otherwise.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<bool> set(
            const std::string_view& key,
            const std::string_view& value,

            std::optional<std::int32_t> expire = std::nullopt
        ) {
            if (!alive())
                co_return false;

            boost::redis::request request{};

            {
                if (expire.has_value()) {
                    request.push("SET", key, value, "EX", std::to_string(expire.value()));
                }
                else
                    request.push("SET", key, value);
            }

            boost::redis::response<std::string> response{};

            {
                auto ec = co_await async_exec(request, response);

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] set() failed: {}",

                        ec.message()
                    );
#endif

                    co_return false;
                }
            }

            co_return std::get<0u>(response).value() == "OK";
        }

        /**
         * @brief Get the value of a key.
         * @tparam _type_t Expected value type (default: std::string).
         * @param key Redis key.
         * @return Optional value if the key exists, std::nullopt otherwise.
         */
        template <typename _type_t = std::string>
        CXXAPI_NOINLINE boost::asio::awaitable<opt_value_t<_type_t>> get(const std::string_view& key) {
            if (!alive())
                co_return std::nullopt;

            boost::redis::request request{};

            {
                request.push("GET", key);
            }

            boost::redis::response<opt_value_t<_type_t>> response{};

            {
                auto ec = co_await async_exec(request, response);

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] get() failed: {}",

                        ec.message()
                    );
#endif

                    co_return std::nullopt;
                }
            }

            co_return std::get<0u>(response).value();
        }

        /**
         * @brief Delete a key from Redis.
         * @param key Redis key to delete.
         * @return True if the key was deleted, false if it did not exist.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<bool> del(const std::string_view& key) {
            if (!alive())
                co_return false;

            boost::redis::request request{};

            {
                request.push("DEL", key);
            }

            boost::redis::response<std::int32_t> response{};

            {
                auto ec = co_await async_exec(request, response);

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] del() failed: {}",

                        ec.message()
                    );
#endif

                    co_return false;
                }
            }

            co_return std::get<0u>(response).value() > 0u;
        }

        /**
         * @brief Check if a key exists in Redis.
         * @param key Redis key to check.
         * @return True if the key exists, false otherwise.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<bool> exists(const std::string_view& key) {
            if (!alive())
                co_return false;

            boost::redis::request request{};

            {
                request.push("EXISTS", key);
            }

            boost::redis::response<std::int32_t> response{};

            {
                auto ec = co_await async_exec(request, response);

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] exists() failed: {}",

                        ec.message()
                    );
#endif

                    co_return false;
                }
            }

            co_return std::get<0u>(response).value() > 0u;
        }

        /**
         * @brief Set expiration time for a key.
         * @param key Redis key.
         * @param time Expiration time in seconds.
         * @return True if expiration was set, false otherwise.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<bool> expire(const std::string_view& key, std::int32_t time) {
            if (!alive())
                co_return false;

            boost::redis::request request{};

            {
                request.push("EXPIRE", key, std::to_string(time));
            }

            boost::redis::response<std::int32_t> response{};

            {
                auto ec = co_await async_exec(request, response);

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] expire() failed: {}",

                        ec.message()
                    );
#endif

                    co_return true;
                }
            }

            co_return std::get<0u>(response).value() == 1u;
        }

        /**
         * @brief Push a value onto the head of a list.
         * @param key Redis list key.
         * @param value Value to push.
         * @return The length of the list after the push.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<std::int32_t> lpush(const std::string_view& key, const std::string_view& value) {
            if (!alive())
                co_return false;

            boost::redis::request request{};

            {
                request.push("LPUSH", key, value);
            }

            boost::redis::response<std::int32_t> response{};

            {
                auto ec = co_await async_exec(request, response);

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] lpush() failed: {}",

                        ec.message()
                    );
#endif

                    co_return -1;
                }
            }

            co_return std::get<0u>(response).value();
        }

        /**
         * @brief Trim a list to the specified range.
         * @param key Redis list key.
         * @param start Start index.
         * @param end End index.
         * @return True if the operation was successful, false otherwise.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<bool> ltrim(
            const std::string_view& key,

            std::int32_t start,
            std::int32_t end
        ) {
            if (!alive())
                co_return false;

            boost::redis::request request{};

            {
                request.push("LTRIM", key, std::to_string(start), std::to_string(end));
            }

            boost::redis::response<std::string> response{};

            {
                auto ec = co_await async_exec(request, response);

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] set() failed: {}",

                        ec.message()
                    );
#endif

                    co_return false;
                }
            }

            co_return std::get<0u>(response).value() == "OK";
        }

        /**
         * @brief Get a range of elements from a list.
         * @param key Redis list key.
         * @param start Start index.
         * @param end End index.
         * @return Vector of list elements in the specified range.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<std::vector<std::string>> lrange(
            const std::string_view& key,

            std::int32_t start,
            std::int32_t end
        ) {
            if (!alive())
                co_return false;

            boost::redis::request request{};

            {
                request.push("LRANGE", key, std::to_string(start), std::to_string(end));
            }

            boost::redis::response<std::vector<std::string>> response{};

            {
                auto ec = co_await async_exec(request, response);

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] lrange() failed: {}",

                        ec.message()
                    );
#endif

                    co_return std::vector<std::string>{};
                }
            }

            co_return std::get<0u>(response).value();
        }

        /**
         * @brief Get the time-to-live (TTL) of a key.
         * @param key Redis key.
         * @return TTL in seconds, or -1 if the key has no expiration.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<std::int32_t> ttl(const std::string_view& key) {
            if (!alive())
                co_return false;

            boost::redis::request request{};

            request.push("TTL", key);

            boost::redis::response<std::int32_t> response{};

            {
                auto ec = co_await async_exec(request, response);

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] ttl() failed: {}",

                        ec.message()
                    );
#endif

                    co_return -1;
                }
            }

            co_return std::get<0u>(response).value();
        }

        /**
         * @brief Set multiple fields in a hash to their respective values.
         * @param key Redis hash key.
         * @param mapping Unordered map of field names and their values to set.
         * @return The number of fields that were newly added to the hash.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<std::int32_t> hset(
            const std::string_view& key, 
            
            const std::unordered_map<std::string_view, std::string_view>& mapping
        ) {
            if (!alive())
                co_return false;

            boost::redis::request request{};

            request.push_range("HSET", key, mapping);

            boost::redis::response<std::int32_t> response{};

            {
                auto ec = co_await async_exec(request, response);

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] hset() failed: {}",

                        ec.message()
                    );
#endif

                    co_return -1;
                }
            }

            co_return std::get<0u>(response).value();
        }

        /**
         * @brief Set a single field in a hash to a given value.
         * @param key Redis hash key.
         * @param field Field name within the hash.
         * @param value Value to assign to the specified field.
         * @return 1 if a new field was created and value set, 0 if the field existed and value was overwritten.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<std::int32_t> hsetfield(
            const std::string_view& key,

            const std::string_view& field,
            const std::string_view& value
        ) {
            if (!alive())
                co_return false;

            boost::redis::request request{};

            request.push("HSET", key, field, value);

            boost::redis::response<std::int32_t> response{};

            {
                auto ec = co_await async_exec(request, response);

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] hsetfield() failed: {}",

                        ec.message()
                    );
#endif

                    co_return -1;
                }
            }

            co_return std::get<0u>(response).value();
        }

        /**
         * @brief Delete one or more fields from a hash.
         * @param key Redis hash key.
         * @param fields Vector of field names to remove from the hash.
         * @return The number of fields that were removed.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<std::int32_t> hdel(
            const std::string_view& key,

            const std::vector<std::string_view>& fields
        ) {
            if (!alive())
                co_return false;

            boost::redis::request request{};

            request.push_range("HDEL", key, fields);

            boost::redis::response<std::int32_t> response{};

            {
                auto ec = co_await async_exec(request, response);

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] hdel() failed: {}",

                        ec.message()
                    );
#endif

                    co_return -1;
                }
            }

            co_return std::get<0u>(response).value();
        }

        /**
         * @brief Retrieve all fields and values of a hash.
         * @param key Redis hash key.
         * @return Unordered map of all field–value pairs stored in the hash; empty if key does not exist or on error.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<boost::unordered_map<std::string, std::string>> hgetall(
            const std::string_view& key
        ) {
            if (!alive())
                co_return false;

            boost::redis::request request{};

            request.push("HGETALL", key);

            boost::redis::response<std::vector<std::string>> response{};

            {
                auto ec = co_await async_exec(request, response);

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] hgetall() failed: {}",

                        ec.message()
                    );
#endif

                    co_return -1;
                }
            }

            auto arr = std::get<0u>(response).value();

            boost::unordered_map<std::string, std::string> ret{};

            ret.reserve(arr.size() * 2u);

            for (std::size_t i{}; i + 1 < arr.size(); i += 2) 
                ret[arr[i]] = arr[i + 1];
            
            co_return ret;
        }

        /**
         * @brief Increment the integer value of a hash field by the given amount.
         * @param key Redis hash key.
         * @param field Field name within the hash.
         * @param amount Amount to increment the field’s value by.
         * @return The new value of the field after increment.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<std::int32_t> hincrby(
            const std::string_view& key,

            const std::string_view& field,
            std::int32_t amount
        ) {
            if (!alive())
                co_return false;

            boost::redis::request request{};

            request.push("HINCRBY", key, field, std::to_string(amount));

            boost::redis::response<std::int32_t> response{};

            {
                auto ec = co_await async_exec(request, response);

                if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "[redis] hincrby() failed: {}",

                        ec.message()
                    );
#endif

                    co_return -1;
                }
            }

            co_return std::get<0u>(response).value();
        }

      public:
        /**
         * @brief Get the current connection state.
         * @return Current Redis connection state.
         */
        CXXAPI_INLINE auto& state() const { return m_state; }

        /**
         * @brief Check if the client is currently connected.
         * @return True if connected, false otherwise.
         */
        CXXAPI_INLINE bool alive() const { return m_state == e_redis_state::connected; }

      private:
        /**
         * @brief Coroutine to asynchronously establish a Redis connection.
         * @param cfg Boost.Redis configuration object.
         * @return Redis connection state after attempting to connect.
         */
        boost::asio::awaitable<e_redis_state> _establish_connection(const boost::redis::config& cfg);

      private:
        /** @brief Thread running the Boost ASIO IO context. */
        std::thread m_thread;

        /** @brief Boost ASIO IO context for asynchronous operations. */
        boost::asio::io_context m_io_ctx;

        /**
         * @brief Managed asynchronous Redis connection instance.
         *
         * Holds a shared pointer to the Boost.Redis connection object,
         * responsible for executing asynchronous commands and managing
         * the connection lifecycle.
         */
        connection_t m_connection;

        /** @brief Log verbosity level for Redis operations. */
        boost::redis::logger::level m_log_level;

        /** @brief Current Redis connection state. */
        e_redis_state m_state;

#ifdef CXXAPI_HAS_LOGGING_IMPL
        /** @brief Logging instance for Redis operations. */
        c_logging m_logger;
#endif
    };
#endif // CXXAPI_USE_REDIS_IMPL
}

#endif // CXXAPI_SHARED_REDIS_HXX
