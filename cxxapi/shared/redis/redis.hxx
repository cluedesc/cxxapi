/**
 * @file redis.hxx
 * @brief Redis client interface.
 */

#ifndef CXXAPI_SHARED_REDIS_HXX
#define CXXAPI_SHARED_REDIS_HXX

namespace shared {
#ifdef CXXAPI_USE_REDIS_IMPL
    /**
     * @brief Configuration for the Redis client.
     */
    struct redis_cfg_t {
        /** @brief Redis server host. */
        std::string m_host{};

        /** @brief Redis server port. */
        std::string m_port{};

        /** @brief Password for Redis authentication. */
        std::string m_password{};

        /** @brief Username for Redis authentication. */
        std::string m_user{};

        /** @brief Logging level for the underlying Redis library. */
        boost::redis::logger::level m_log_level{boost::redis::logger::level::info};

#ifdef CXXAPI_HAS_LOGGING_IMPL
        /**
         * @brief Logging configuration for CXXAPI.
         */
        struct cxxapi_logger_t {
            /** @brief Logging level for CXXAPI. */
            e_log_level m_level{e_log_level::info};

            /** @brief Force flush after each log entry. */
            bool m_force_flush{false};

            /** @brief Enable asynchronous logging. */
            bool m_async{true};

            /** @brief Size of the internal logging buffer. */
            std::size_t m_buffer_size{16384u};

            /** @brief Strategy to handle buffer overflows. */
            c_logging::e_overflow_strategy m_strategy{c_logging::e_overflow_strategy::discard_oldest};
        };

        /** @brief CXXAPI-specific logger settings. */
        cxxapi_logger_t m_cxxapi_logger{};
#endif
    };

    /**
     * @brief Core Redis client class.
     */
    class c_redis {
      public:
        /** @brief Shared pointer to a Boost.Redis connection. */
        using boost_connection_t = std::shared_ptr<boost::redis::connection>;

        /**
         * @brief Alias for optional Redis response value.
         * @tparam _type_t Underlying value type.
         */
        template <typename _type_t>
        using opt_value_t = std::optional<_type_t>;

      public:
        /**
         * @brief Initialize the Redis client with the given configuration.
         * @param cfg Configuration parameters for the Redis client.
         * @return true if initialization succeeded, false otherwise.
         */
        bool init(const redis_cfg_t& cfg);

        /**
         * @brief Shutdown the Redis client.
         */
        void shutdown();

      public:
        /**
         * @brief Access the mutable configuration.
         */
        CXXAPI_INLINE auto& cfg() { return m_cfg; }

        /**
         * @brief Access the read-only configuration.
         */
        [[nodiscard]] CXXAPI_INLINE const auto& cfg() const { return m_cfg; }

        /**
         * @brief Access the initialization flag mutable.
         */
        CXXAPI_INLINE auto& inited() { return m_inited; }

        /**
         * @brief Access the initialization flag read-only.
         */
        [[nodiscard]] CXXAPI_INLINE const auto& inited() const { return m_inited; }

#ifdef CXXAPI_USE_LOGGING_IMPL
        /**
         * @brief Access the mutable logger.
         */
        CXXAPI_INLINE auto& logger() { return m_logger; }

        /**
         * @brief Access the read-only logger.
         */
        CXXAPI_INLINE const auto& logger() const { return m_logger; }
#endif // CXXAPI_USE_LOGGING_IMPL

      public:
        /**
         * @brief Represents a single Redis connection.
         */
        struct connection_t {
            /**
             * @brief Possible states of a Redis connection.
             */
            enum struct e_status : std::int16_t {
                unknown = -1,      ///< Connection state is unknown.
                relax,             ///< Not yet connected.
                connected,         ///< Successfully connected.
                disconnected,      ///< Gracefully disconnected.
                abort,             ///< Aborted due to error.
                connection_refused ///< Connection was refused.
            };

            /**
             * @brief Configuration for an individual connection.
             */
            struct cfg_t {
                /** @brief Host for this connection (default: "127.0.0.1"). */
                std::string m_host{"127.0.0.1"};

                /** @brief Port for this connection (default: "6379"). */
                std::string m_port{"6379"};

                /** @brief Username for this connection. */
                std::string m_user{};

                /** @brief Password for this connection. */
                std::string m_password{};

                /** @brief Unique identifier for the connection. */
                std::string m_uuid{};

                /** @brief Client name reported to the server. */
                std::string m_client_name{};

                /** @brief Logging level for this connection. */
                boost::redis::logger::level m_log_level{boost::redis::logger::level::info};
            };

          public:
            /**
             * @brief Construct a Redis connection wrapper.
             * @param cfg Connection configuration.
             * @param connection Underlying Boost.Redis connection.
             * @param redis Parent Redis client instance.
             * @param io_ctx Boost.Asio I/O context.
    #ifdef CXXAPI_USE_LOGGING_IMPL
             * @param logger Logger instance.
    #endif
             */
            CXXAPI_INLINE connection_t(
                cfg_t&& cfg,

                boost_connection_t connection,

                c_redis& redis,

                boost::asio::io_context& io_ctx
            ) : m_cfg(std::move(cfg)),
                m_connection(std::move(connection)),
                m_redis(redis),
                m_status(e_status::relax),
                m_io_ctx(io_ctx)

#ifdef CXXAPI_USE_LOGGING_IMPL
                ,
                m_logger(redis.logger())
#endif // CXXAPI_USE_LOGGING_IMPL
            {
                // ..
            }

            /**
             * @brief Deleted default constructor.
             */
            CXXAPI_INLINE connection_t() = delete;

            /**
             * @brief Destructor shuts down the connection.
             */
            CXXAPI_INLINE ~connection_t() { shutdown(); }

          public:
            /**
             * @brief Establish the connection to Redis asynchronously.
             * @return Awaitable resolving to true if connected, false otherwise.
             */
            CXXAPI_NOINLINE boost::asio::awaitable<bool> establish() {
                if (!m_redis.inited())
                    co_return false;

                boost::redis::config redis_cfg{};

                if (m_cfg.m_client_name.empty()) {
                    m_cfg.m_client_name = "Connection-Unknown";
                }
                else {
                    m_cfg.m_client_name = fmt::format(
                        "{}-{}",

                        m_cfg.m_client_name, m_cfg.m_uuid.empty() ? "Unknown" : m_cfg.m_uuid
                    );
                }

                {
                    redis_cfg.addr = boost::redis::address(
                        m_cfg.m_host, m_cfg.m_port
                    );

                    if (!m_cfg.m_user.empty())
                        redis_cfg.username = m_cfg.m_user;

                    if (!m_cfg.m_password.empty())
                        redis_cfg.password = m_cfg.m_password;

                    {
                        redis_cfg.clientname = m_cfg.m_client_name;

                        redis_cfg.health_check_id = fmt::format(
                            "{}-HealthCheck",

                            m_cfg.m_client_name
                        );

                        redis_cfg.log_prefix = fmt::format(
                            "[{}] ",

                            m_cfg.m_client_name
                        );
                    }

                    {
                        redis_cfg.health_check_interval = std::chrono::seconds(0u);
                        redis_cfg.reconnect_wait_interval = std::chrono::seconds(0u);
                    }
                }

                {
                    {
                        m_connection->async_run(
                            redis_cfg, {m_cfg.m_log_level},

                            boost::asio::consign(
                                [&](const boost::system::error_code& error_code) {
                                    if (error_code) {
                                        const auto& message = error_code.message();

                                        if (boost::algorithm::icontains(message, "connection refused")) {
                                            m_status = e_status::connection_refused;
                                        }
                                        else if (!boost::algorithm::icontains(message, "operation cancel"))
                                            m_status = e_status::abort;
                                    }
                                },

                                m_connection
                            )
                        );
                    }

                    {
                        auto executor = co_await boost::asio::this_coro::executor;

                        {
                            boost::asio::steady_timer sleep(executor);

                            sleep.expires_after(std::chrono::milliseconds(100u));

                            co_await sleep.async_wait(boost::asio::use_awaitable);
                        }

                        if (m_status == e_status::abort
                            || m_status == e_status::connection_refused)
                            co_return false;
                    }

                    co_await alive(true);
                }

                co_return m_status == e_status::connected;
            }

            /**
             * @brief Shut down this connection immediately.
             */
            CXXAPI_INLINE void shutdown() {
                if (m_connection) {
                    if (m_status != e_status::abort
                        && m_status != e_status::connection_refused
                        && m_status != e_status::disconnected)
                        m_connection->cancel();
                }

                m_status = e_status::disconnected;
            }

          public:
            /**
             * @brief Execute a Redis command asynchronously.
             * @tparam _tuple_t Types of response tuple elements.
             * @param request Redis request to send.
             * @param response Response container to populate.
             * @return Awaitable resolving to the error code of the operation.
             */
            template <typename... _tuple_t>
            CXXAPI_NOINLINE boost::asio::awaitable<boost::system::error_code> async_exec(
                boost::redis::request&& request,

                boost::redis::response<_tuple_t...>& response
            ) {
                if (!(co_await alive()))
                    co_return boost::system::error_code{boost::asio::error::operation_aborted};

                boost::system::error_code ec{};

                co_await m_connection->async_exec(
                    std::move(request), response,

                    boost::asio::redirect_error(boost::asio::use_awaitable, ec)
                );

                co_return ec;
            }

          public:
            /**
             * @brief Check if the connection is alive (optionally update status).
             * @param update If true, sends a PING to update the status.
             * @return Awaitable resolving to true if connected, false otherwise.
             */
            CXXAPI_NOINLINE boost::asio::awaitable<bool> alive(bool update = false) {
                if (update) {
                    boost::redis::request::config config{
                        .cancel_on_connection_lost = false,
                        .cancel_if_not_connected = false,
                        .cancel_if_unresponded = false
                    };

                    boost::redis::request request(std::move(config));

                    {
                        request.push("PING", "PONG");
                    }

                    boost::redis::response<std::string> response{};

                    {
                        boost::system::error_code ec{};

                        co_await m_connection->async_exec(
                            std::move(request), response,

                            boost::asio::redirect_error(boost::asio::use_awaitable, ec)
                        );

                        if (ec) {
                            m_status = e_status::abort;

                            co_return false;
                        }
                    }

                    m_status = std::get<0u>(response).value() == "PONG"
                                 ? e_status::connected
                                 : e_status::abort;
                }

                co_return m_status == e_status::connected;
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
                if (!(co_await alive()))
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
                    auto ec = co_await async_exec(std::move(request), response);

                    if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                        m_logger.log(
                            e_log_level::error,

                            "[{}] set() failed: {}",

                            m_cfg.m_client_name, ec.message()
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
                if (!(co_await alive()))
                    co_return std::nullopt;

                boost::redis::request request{};

                {
                    request.push("GET", key);
                }

                boost::redis::response<opt_value_t<_type_t>> response{};

                {
                    auto ec = co_await async_exec(std::move(request), response);

                    if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                        m_logger.log(
                            e_log_level::error,

                            "[{}] get() failed: {}",

                            m_cfg.m_client_name, ec.message()
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
                if (!(co_await alive()))
                    co_return false;

                boost::redis::request request{};

                {
                    request.push("DEL", key);
                }

                boost::redis::response<std::int32_t> response{};

                {
                    auto ec = co_await async_exec(std::move(request), response);

                    if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                        m_logger.log(
                            e_log_level::error,

                            "[{}] del() failed: {}",

                            m_cfg.m_client_name, ec.message()
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
                if (!(co_await alive()))
                    co_return false;

                boost::redis::request request{};

                {
                    request.push("EXISTS", key);
                }

                boost::redis::response<std::int32_t> response{};

                {
                    auto ec = co_await async_exec(std::move(request), response);

                    if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                        m_logger.log(
                            e_log_level::error,

                            "[{}] exists() failed: {}",

                            m_cfg.m_client_name, ec.message()
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
                if (!(co_await alive()))
                    co_return false;

                boost::redis::request request{};

                {
                    request.push("EXPIRE", key, std::to_string(time));
                }

                boost::redis::response<std::int32_t> response{};

                {
                    auto ec = co_await async_exec(std::move(request), response);

                    if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                        m_logger.log(
                            e_log_level::error,

                            "[{}] expire() failed: {}",

                            m_cfg.m_client_name, ec.message()
                        );
#endif

                        co_return false;
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
                if (!(co_await alive()))
                    co_return false;

                boost::redis::request request{};

                {
                    request.push("LPUSH", key, value);
                }

                boost::redis::response<std::int32_t> response{};

                {
                    auto ec = co_await async_exec(std::move(request), response);

                    if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                        m_logger.log(
                            e_log_level::error,

                            "[{}] lpush() failed: {}",

                            m_cfg.m_client_name, ec.message()
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
                if (!(co_await alive()))
                    co_return false;

                boost::redis::request request{};

                {
                    request.push("LTRIM", key, std::to_string(start), std::to_string(end));
                }

                boost::redis::response<std::string> response{};

                {
                    auto ec = co_await async_exec(std::move(request), response);

                    if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                        m_logger.log(
                            e_log_level::error,

                            "[{}] ltrim() failed: {}",

                            m_cfg.m_client_name, ec.message()
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
                if (!(co_await alive()))
                    co_return false;

                boost::redis::request request{};

                {
                    request.push("LRANGE", key, std::to_string(start), std::to_string(end));
                }

                boost::redis::response<std::vector<std::string>> response{};

                {
                    auto ec = co_await async_exec(std::move(request), response);

                    if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                        m_logger.log(
                            e_log_level::error,

                            "[{}] lrange() failed: {}",

                            m_cfg.m_client_name, ec.message()
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
                if (!(co_await alive()))
                    co_return false;

                boost::redis::request request{};

                request.push("TTL", key);

                boost::redis::response<std::int32_t> response{};

                {
                    auto ec = co_await async_exec(std::move(request), response);

                    if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                        m_logger.log(
                            e_log_level::error,

                            "[{}] ttl() failed: {}",

                            m_cfg.m_client_name, ec.message()
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
                if (!(co_await alive()))
                    co_return false;

                boost::redis::request request{};

                request.push_range("HSET", key, mapping);

                boost::redis::response<std::int32_t> response{};

                {
                    auto ec = co_await async_exec(std::move(request), response);

                    if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                        m_logger.log(
                            e_log_level::error,

                            "[{}] hset() failed: {}",

                            m_cfg.m_client_name, ec.message()
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
                if (!(co_await alive()))
                    co_return false;

                boost::redis::request request{};

                request.push("HSET", key, field, value);

                boost::redis::response<std::int32_t> response{};

                {
                    auto ec = co_await async_exec(std::move(request), response);

                    if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                        m_logger.log(
                            e_log_level::error,

                            "[{}] hsetfield() failed: {}",

                            m_cfg.m_client_name, ec.message()
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
                if (!(co_await alive()))
                    co_return false;

                boost::redis::request request{};

                request.push_range("HDEL", key, fields);

                boost::redis::response<std::int32_t> response{};

                {
                    auto ec = co_await async_exec(std::move(request), response);

                    if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                        m_logger.log(
                            e_log_level::error,

                            "[{}] hdel() failed: {}",

                            m_cfg.m_client_name, ec.message()
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
                if (!(co_await alive()))
                    co_return false;

                boost::redis::request request{};

                request.push("HGETALL", key);

                boost::redis::response<std::vector<std::string>> response{};

                {
                    auto ec = co_await async_exec(std::move(request), response);

                    if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                        m_logger.log(
                            e_log_level::error,

                            "[{}] hgetall() failed: {}",

                            m_cfg.m_client_name, ec.message()
                        );
#endif

                        co_return boost::unordered_map<std::string, std::string>{};
                    }
                }

                auto arr = std::get<0u>(response).value();

                boost::unordered_map<std::string, std::string> ret{};

                ret.reserve(arr.size() * 2u);

                for (std::size_t i{}; i + 1u < arr.size(); i += 2u)
                    ret[arr[i]] = arr[i + 1u];

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
                if (!(co_await alive()))
                    co_return false;

                boost::redis::request request{};

                request.push("HINCRBY", key, field, std::to_string(amount));

                boost::redis::response<std::int32_t> response{};

                {
                    auto ec = co_await async_exec(std::move(request), response);

                    if (ec) {
#ifdef CXXAPI_HAS_LOGGING_IMPL
                        m_logger.log(
                            e_log_level::error,

                            "[{}] hincrby() failed: {}",

                            m_cfg.m_client_name, ec.message()
                        );
#endif

                        co_return -1;
                    }
                }

                co_return std::get<0u>(response).value();
            }

          public:
            /**
             * @brief Access the connection configuration mutable.
             */
            CXXAPI_INLINE auto& cfg() { return m_cfg; }

            /**
             * @brief Access the connection configuration read-only.
             */
            [[nodiscard]] CXXAPI_INLINE const auto& cfg() const { return m_cfg; }

            /**
             * @brief Access the underlying Boost.Redis connection mutable.
             */
            [[nodiscard]] CXXAPI_INLINE auto& connection() { return m_connection; }

            /**
             * @brief Access the underlying Boost.Redis connection read-only.
             */
            [[nodiscard]] CXXAPI_INLINE const auto& connection() const { return m_connection; }

            /**
             * @brief Access the connection status mutable.
             */
            CXXAPI_INLINE auto& status() { return m_status; }

            /**
             * @brief Access the connection status read-only.
             */
            [[nodiscard]] CXXAPI_INLINE const auto& status() const { return m_status; }

#ifdef CXXAPI_USE_LOGGING_IMPL
            /**
             * @brief Access the mutable logger for this connection.
             */
            CXXAPI_INLINE auto& logger() { return m_logger; }

            /**
             * @brief Access the read-only logger for this connection.
             */
            [[nodiscard]] CXXAPI_INLINE const auto& logger() const { return m_logger; }
#endif

          private:
            /** @brief Connection configuration. */
            cfg_t m_cfg{};

            /** @brief Underlying Boost.Redis connection. */
            boost_connection_t m_connection{};

            /** @brief Connection status. */
            e_status m_status{e_status::relax};

#ifdef CXXAPI_USE_LOGGING_IMPL
            /** @brief Logger for this connection. */
            c_logging& m_logger;
#endif // CXXAPI_USE_LOGGING_IMPL

            /** @brief Redis instance. */
            c_redis& m_redis;

            /** @brief IO context. */
            boost::asio::io_context& m_io_ctx;
        };

      public:
        /**
         * @brief Pool of Redis connections for reuse.
         */
        class c_connection_pool {
          public:
            /**
             * @brief Shared pointer to a connection handle.
             */
            using connection_ptr_t = std::shared_ptr<connection_t>;

            /**
             * @brief Pool configuration parameters.
             */
            struct pool_cfg_t {
                /** @brief Number of connections to create initially. */
                std::size_t m_initial_connections{5u};

                /** @brief Minimum connections to keep alive. */
                std::size_t m_min_connections{5u};

                /** @brief Maximum connections allowed. */
                std::size_t m_max_connections{25u};

                /** @brief Enable periodic health checks. */
                bool m_health_check_enabled{true};

                /** @brief Idle timeout before closing connections. */
                std::chrono::seconds m_idle_timeout{360u};

                /** @brief Interval between cleanup runs. */
                std::chrono::seconds m_cleanup_interval{60u};
            };

            /**
             * @brief Wrapper for a single pooled connection.
             */
            struct connection_handle_t {
                /**
                 * @brief Construct a connection handle.
                 * @param connection Shared connection object.
                 * @param id Unique handle identifier.
                 */
                CXXAPI_INLINE connection_handle_t(connection_ptr_t connection, std::string id)
                    : m_in_use(false),
                      m_last_used(std::chrono::steady_clock::now()),
                      m_connection(std::move(connection)),
                      m_id(std::move(id)) {
                }

                /**
                 * @brief Destructor.
                 */
                CXXAPI_INLINE ~connection_handle_t() = default;

              public:
                /**
                 * @brief Mark this handle as in use.
                 */
                CXXAPI_INLINE void acquire() {
                    m_in_use = true;

                    m_last_used = std::chrono::steady_clock::now();
                }

                /**
                 * @brief Release this handle back to the pool.
                 */
                CXXAPI_INLINE void release() {
                    m_in_use = false;

                    m_last_used = std::chrono::steady_clock::now();
                }

              public:
                /** @brief Whether this handle is currently in use. */
                bool m_in_use{false};

                /** @brief Time of last use. */
                std::chrono::steady_clock::time_point m_last_used{};

                /** @brief Shared connection object. */
                connection_ptr_t m_connection{};

                /** @brief Handle identifier. */
                std::string m_id{};
            };

            /**
             * @brief Shared pointer to a connection handle.
             */
            using connection_handle_ptr_t = std::shared_ptr<connection_handle_t>;

            /**
             * @brief RAII wrapper for automatically releasing a connection.
             */
            struct scoped_connection_t {
                /**
                 * @brief Acquire a connection from the pool.
                 * @param pool Reference to the connection pool.
                 * @param handle Handle to manage.
                 */
                CXXAPI_INLINE scoped_connection_t(c_connection_pool& pool, connection_handle_ptr_t handle)
                    : m_pool(pool), m_handle(std::move(handle)) {
                }

                /**
                 * @brief Destructor releases the connection back to the pool.
                 */
                CXXAPI_INLINE ~scoped_connection_t() {
                    if (m_handle)
                        m_pool.release_connection(m_handle);
                }

                /**
                 * @brief Move constructor.
                 */
                CXXAPI_INLINE scoped_connection_t(scoped_connection_t&& other)
                    : m_pool(other.m_pool), m_handle(std::move(other.m_handle)) {
                    other.m_handle = nullptr;
                }

              public:
                /**
                 * @brief Move assignment.
                 */
                CXXAPI_INLINE scoped_connection_t& operator=(scoped_connection_t&& other) {
                    if (this != &other) {
                        if (m_handle)
                            m_pool.release_connection(m_handle);

                        m_handle = std::move(other.m_handle);

                        other.m_handle = nullptr;
                    }

                    return *this;
                }

                /**
                 * @brief Access the underlying connection pointer.
                 */
                CXXAPI_INLINE connection_t* operator->() const { return m_handle->m_connection.get(); }

                /**
                 * @brief Dereference to the underlying connection.
                 */
                CXXAPI_INLINE connection_t& operator*() const { return *m_handle->m_connection; }

                /**
                 * @brief Check if the scoped connection is valid.
                 */
                CXXAPI_INLINE operator bool() const { return m_handle && m_handle->m_connection; }

              public:
                /**
                 * @brief Copy constructor (deleted).
                 */
                scoped_connection_t(const scoped_connection_t&) = delete;

                /**
                 * @brief Copy assignment (deleted).
                 */
                scoped_connection_t& operator=(const scoped_connection_t&) = delete;

              public:
                /** @brief Reference to the connection pool. */
                c_connection_pool& m_pool;

                /** @brief Handle to manage. */
                connection_handle_ptr_t m_handle{};
            };

          public:
            /**
             * @brief Construct a connection pool.
             * @param cfg Pool configuration parameters.
             * @param redis Parent Redis client.
             * @param io_ctx Boost.Asio I/O context.
    #ifdef CXXAPI_USE_LOGGING_IMPL
             * @param logger Logger instance.
    #endif
             */
            CXXAPI_INLINE c_connection_pool(
                pool_cfg_t&& cfg,

                c_redis& redis,

                boost::asio::io_context& io_ctx
            ) : m_cfg(std::move(cfg)),
                m_redis(redis),
                m_io_ctx(io_ctx),
                m_running(false)
#ifdef CXXAPI_USE_LOGGING_IMPL
                ,
                m_logger(redis.logger())
#endif // CXXAPI_USE_LOGGING_IMPL
            {
                // ...
            }

          public:
            /**
             * @brief Initialize the connection pool asynchronously.
             * @return Awaitable resolving to true if pool is running.
             */
            CXXAPI_NOINLINE boost::asio::awaitable<bool> init() {
                if (!m_redis.inited())
                    co_return false;

                std::lock_guard<std::mutex> lock(m_pool_mutex);

                if (m_running)
                    co_return true;

                for (std::size_t i{}; i < m_cfg.m_initial_connections; i++) {
                    auto conn_handle = co_await create_connection();

                    if (!conn_handle) {
                        shutdown_internal();

                        co_return false;
                    }

                    m_pool.push_back(conn_handle);
                }

                m_running = true;

                co_return true;
            }

            /**
             * @brief Shut down the pool, closing all connections.
             */
            CXXAPI_INLINE void shutdown() {
                std::lock_guard<std::mutex> lock(m_pool_mutex);

                shutdown_internal();
            }

          public:
            /**
             * @brief Create a new connection and wrap it in a handle.
             * @return Awaitable with a handle, or nullptr on failure.
             */
            CXXAPI_NOINLINE boost::asio::awaitable<connection_handle_ptr_t> create_connection() {
                try {
                    auto uuid = boost::uuids::to_string(boost::uuids::random_generator()());

                    auto redis_connection = std::make_shared<boost::redis::connection>(m_io_ctx);

                    {
                        const auto& redis_cfg = m_redis.cfg();

                        connection_t::cfg_t cfg{
                            .m_host = redis_cfg.m_host,
                            .m_port = redis_cfg.m_port,

                            .m_user = redis_cfg.m_user,
                            .m_password = redis_cfg.m_password,

                            .m_uuid = uuid,

                            .m_client_name = "Connection",

                            .m_log_level = redis_cfg.m_log_level
                        };

                        auto connection_handle = std::make_shared<connection_t>(
                            std::move(cfg),

                            std::move(redis_connection),

                            m_redis,

                            m_io_ctx
                        );

                        auto success = co_await connection_handle->establish();

                        if (!success)
                            co_return nullptr;

                        auto handle = std::make_shared<connection_handle_t>(connection_handle, uuid);

                        co_return handle;
                    }
                }
                catch (const std::exception& e) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                    m_logger.log(
                        e_log_level::error,

                        "Failed to create Redis connection: {}",

                        e.what()
                    );
#endif
                }

                co_return nullptr;
            }

            /**
             * @brief Acquire a connection from the pool (waiting if necessary).
             * @return Awaitable with an optional scoped connection.
             */
            CXXAPI_NOINLINE boost::asio::awaitable<std::optional<scoped_connection_t>> acquire_connection() {
                if (!m_running)
                    co_return std::nullopt;

                connection_handle_ptr_t handle{};

                {
                    std::unique_lock<std::mutex> lock(m_pool_mutex);

                    auto it = std::find_if(m_pool.begin(), m_pool.end(), [](const auto& conn) { return !conn->m_in_use; });

                    if (it != m_pool.end()) {
                        handle = *it;

                        handle->acquire();
                    }
                    else if (m_pool.size() < m_cfg.m_max_connections) {
                        lock.unlock();

                        {
                            handle = co_await create_connection();

                            if (!handle)
                                co_return std::nullopt;
                        }

                        lock.lock();

                        handle->m_in_use = true;

                        m_pool.push_back(handle);
                    }
                    else {
                        lock.unlock();

                        {
                            auto executor = co_await boost::asio::this_coro::executor;

                            boost::asio::steady_timer wait_timer(executor);

                            for (std::size_t attempt{}; attempt < 5u; attempt++) {
                                wait_timer.expires_after(std::chrono::milliseconds(100u * (1u << attempt)));

                                co_await wait_timer.async_wait(boost::asio::use_awaitable);

                                lock.lock();

                                auto it = std::find_if(m_pool.begin(), m_pool.end(), [](const auto& conn) { return !conn->m_in_use; });

                                if (it != m_pool.end()) {
                                    handle = *it;

                                    handle->acquire();

                                    lock.unlock();

                                    break;
                                }

                                lock.unlock();
                            }
                        }

                        if (!handle)
                            co_return std::nullopt;
                    }

                    if (m_cfg.m_health_check_enabled) {
                        if (!(co_await handle->m_connection->alive())) {
                            handle->m_connection->status() = connection_t::e_status::relax;

                            if (!(co_await handle->m_connection->establish())) {
                                release_connection(handle);

                                co_return std::nullopt;
                            }
                        }
                    }

                    co_return scoped_connection_t(*this, handle);
                }
            }

            /**
             * @brief Release a previously acquired connection back to the pool.
             * @param handle Handle to release.
             */
            CXXAPI_INLINE void release_connection(connection_handle_ptr_t handle) {
                std::lock_guard<std::mutex> lock(m_pool_mutex);

                if (!handle
                    || !m_running)
                    return;

                handle->release();
            }

            /**
             * @brief Internal shutdown logic for clearing the pool.
             */
            CXXAPI_INLINE void shutdown_internal() {
                m_running = false;

                for (auto& handle : m_pool)
                    if (handle->m_connection)
                        handle->m_connection->shutdown();

                m_pool.clear();
            }

          public:
            /**
             * @brief Access the pool configuration mutable.
             */
            CXXAPI_INLINE auto& cfg() { return m_cfg; }

            /**
             * @brief Access the pool configuration read-only.
             */
            [[nodiscard]] CXXAPI_INLINE const auto& cfg() const { return m_cfg; }

#ifdef CXXAPI_USE_LOGGING_IMPL
            /** @brief Access the mutable logger for the pool. */
            CXXAPI_INLINE auto& logger() { return m_logger; }

            /** @brief Access the read-only logger for the pool. */
            [[nodiscard]] CXXAPI_INLINE const auto& logger() const { return m_logger; }
#endif // CXXAPI_USE_LOGGING_IMPL

          private:
            /** @brief The pool configuration. */
            pool_cfg_t m_cfg{};

            /** @brief The pool of connections. */
            std::vector<connection_handle_ptr_t> m_pool{};

            /** @brief The mutex for the pool. */
            std::mutex m_pool_mutex;

            /** @brief The running state. */
            std::atomic<bool> m_running;

#ifdef CXXAPI_USE_LOGGING_IMPL
            /** @brief The logger for the pool. */
            c_logging& m_logger;
#endif // CXXAPI_USE_LOGGING_IMPL

            /** @brief The redis instance. */
            c_redis& m_redis;

            /** @brief The io context. */
            boost::asio::io_context& m_io_ctx;
        };

      private:
        /** @brief Flag indicating whether init() succeeded. */
        bool m_inited{};

        /** @brief Redis client configuration. */
        redis_cfg_t m_cfg{};

#ifdef CXXAPI_HAS_LOGGING_IMPL
        /** @brief Logger for the Redis client. */
        c_logging m_logger;
#endif
    };
#endif // CXXAPI_USE_REDIS_IMPL
}

#endif // CXXAPI_SHARED_REDIS_HXX
