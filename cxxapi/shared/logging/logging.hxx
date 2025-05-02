/**
 * @file logging.hxx
 * @brief Asynchronous logging system with buffering and overflow strategies.
 */

#ifndef CXXAPI_SHARED_LOGGING_HXX
#define CXXAPI_SHARED_LOGGING_HXX

#define CXXAPI_HAS_LOGGING_IMPL

namespace shared {
#ifdef CXXAPI_USE_LOGGING_IMPL
    /**
     * @brief Log severity levels.
     */
    enum struct e_log_level : std::int16_t {
        debug,    ///< Debug-level messages.
        info,     ///< Informational messages.
        warning,  ///< Warning conditions.
        error,    ///< Error conditions.
        critical, ///< Critical conditions.
        none = -1 ///< Logging disabled.
    };

    /**
     * @brief Represents a single log message with metadata.
     */
    struct log_message_t {
        /** @brief Severity level of the log message. */
        e_log_level m_level{};

        /** @brief Log message text. */
        std::string m_message{};

        /** @brief Timestamp when the message was created. */
        std::chrono::system_clock::time_point m_timestamp{};
    };

    /**
     * @brief Thread-safe buffer for storing log messages.
     */
    class log_buffer_t {
      public:
        /**
         * @brief Construct a log buffer with a given capacity.
         * @param capacity Maximum number of messages to buffer.
         */
        CXXAPI_INLINE log_buffer_t(std::size_t capacity = 4096u) : m_capacity(capacity) { m_buffer.reserve(capacity); }

        /**
         * @brief Destructor. Clears and shrinks the buffer.
         */
        CXXAPI_INLINE ~log_buffer_t() {
            m_buffer.clear();
            m_buffer.shrink_to_fit();
        }

        /**
         * @brief Add a log message to the buffer.
         * @param msg Log message to add (rvalue reference).
         * @return True if added successfully, false if buffer is full.
         */
        CXXAPI_INLINE bool push(log_message_t&& msg) {
            std::unique_lock<std::shared_mutex> lock(m_mutex);

            if (m_buffer.size() >= m_capacity) 
                return false;
            
            m_buffer.push_back(std::move(msg));

            return true;
        }

        /**
         * @brief Remove the oldest log message from the buffer.
         * @param msg Reference to store the removed message.
         * @return True if a message was removed, false if buffer was empty.
         */
        CXXAPI_INLINE bool pop(log_message_t& msg) {
            std::unique_lock<std::shared_mutex> lock(m_mutex);

            if (m_buffer.empty()) 
                return false;

            msg = std::move(m_buffer.front());

            m_buffer.erase(m_buffer.begin());

            return true;
        }

        /**
         * @brief Get the current number of buffered messages.
         * @return Number of messages in the buffer.
         */
        CXXAPI_INLINE std::size_t size() const {
            std::unique_lock<std::shared_mutex> lock(m_mutex);

            return m_buffer.size();
        }

        /**
         * @brief Check if the buffer is empty.
         * @return True if empty, false otherwise.
         */
        CXXAPI_INLINE bool empty() const {
            std::shared_lock<std::shared_mutex> lock(m_mutex);

            return m_buffer.empty();
        }

        /**
         * @brief Retrieve and remove a batch of messages from the buffer.
         * @param batch_size Maximum number of messages to retrieve.
         * @return Vector of log messages.
         */
        CXXAPI_INLINE std::vector<log_message_t> get_batch(std::size_t batch_size) {
            std::shared_lock<std::shared_mutex> lock(m_mutex);

            std::vector<log_message_t> batch{};

            batch.reserve(std::min(batch_size, m_buffer.size()));

            auto count = std::min(batch_size, m_buffer.size());

            for (std::size_t i{}; i < count; i++) 
                batch.push_back(std::move(m_buffer[i]));
            
            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + count);

            return batch;
        }

      private:
        /** @brief Container for buffered log messages. */
        std::vector<log_message_t> m_buffer;

        /** @brief Mutex for synchronizing access to the buffer. */
        mutable std::shared_mutex m_mutex;

        /** @brief Maximum capacity of the buffer. */
        std::size_t m_capacity;
    };

    /**
     * @brief Asynchronous logger with configurable buffering and overflow handling.
     */
    class c_logging {
      public:
        /**
         * @brief Construct a logger with optional log level and flush behavior.
         * @param log_level Minimum severity level to log.
         * @param force_flush Whether to flush output immediately.
         */
        CXXAPI_INLINE c_logging(const e_log_level& log_level = e_log_level::none, bool force_flush = false)
            : m_log_level(log_level), m_force_flush(force_flush), m_running(false) {
        }

        /**
         * @brief Destructor. Stops asynchronous logging and flushes remaining messages.
         */
        CXXAPI_INLINE ~c_logging() { stop_async(); }

      public:
        /**
         * @brief Overflow handling strategies for the log buffer.
         */
        enum struct e_overflow_strategy {
            block,          ///< Block producer threads until space is available.
            discard_oldest, ///< Discard the oldest message to make room.
            discard_newest  ///< Discard the new incoming message.
        };

        /**
         * @brief Initialize the logger with configuration options.
         * @param log_level Minimum severity level to log.
         * @param force_flush Whether to flush output immediately.
         * @param async Enable asynchronous logging.
         * @param buffer_size Size of the internal log buffer.
         * @param strategy Overflow handling strategy.
         */
        CXXAPI_INLINE void init(
            const e_log_level& log_level,

            bool force_flush = false,
            bool async = true,

            std::size_t buffer_size = 16384u,

            e_overflow_strategy strategy = e_overflow_strategy::discard_oldest
        ) {
            m_log_level = log_level;
            m_force_flush = force_flush;
            m_buffer_size = buffer_size;
            m_overflow_strategy = strategy;

            if (!async)
                return; 

            start_async();           
        }

        /**
         * @brief Convert a log level enum to a string representation.
         * @param level Log level to convert.
         * @return String representation of the log level.
         */
        CXXAPI_INLINE const char* lvl_to_str(const e_log_level& level) const {
            switch (level) {
                case e_log_level::info:
                    return "INFO";
                case e_log_level::debug:
                    return "DEBUG";
                case e_log_level::warning:
                    return "WARNING";
                case e_log_level::error:
                    return "ERROR";
                case e_log_level::critical:
                    return "CRITICAL";
                default:
                    return "UNKNOWN";
            }
        }

        /**
         * @brief Immediately print a formatted log message, bypassing buffering.
         * @tparam _args_t Variadic format argument types.
         * @param log_level Severity level of the message.
         * @param message Format string.
         * @param args Format arguments.
         */
        template <typename... _args_t>
        CXXAPI_INLINE void force_log(const e_log_level& log_level, fmt::format_string<_args_t...> message, _args_t&&... args) {
            auto now = std::chrono::system_clock::now();

            auto in_time_t = std::chrono::system_clock::to_time_t(now);

            fmt::print(
                "[{:%Y-%m-%d %H:%M:%S}] {} - {}\n",

                fmt::styled(
                    std::chrono::system_clock::time_point(std::chrono::system_clock::from_time_t(in_time_t)),
                    fmt::emphasis::bold | fg(fmt::rgb(245, 245, 184))
                ),

                fmt::styled(
                    lvl_to_str(log_level),
                    fmt::emphasis::bold
                ),

                fmt::styled(
                    fmt::format(message, std::forward<_args_t>(args)...),
                    fg(fmt::rgb(255, 255, 230))
                )
            );

            std::fflush(stdout);
        }

        /**
         * @brief Log a formatted message, asynchronously if enabled.
         * @tparam _args_t Variadic format argument types.
         * @param log_level Severity level of the message.
         * @param message Format string.
         * @param args Format arguments.
         */
        template <typename... _args_t>
        CXXAPI_INLINE void log(const e_log_level& log_level, fmt::format_string<_args_t...> message, _args_t&&... args) {
            if (log_level < m_log_level 
                || m_log_level == e_log_level::none) 
                return;
            
            auto now = std::chrono::system_clock::now();

            auto formatted_message = fmt::format(message, std::forward<_args_t>(args)...);

            if (m_running && m_log_buffer) {
                log_message_t log_msg{log_level, std::move(formatted_message), now};

                if (!m_log_buffer->push(std::move(log_msg))) {
                    handle_overflow(std::move(log_msg));
                }
                else 
                    m_condition.notify_one();         
            }
            else {
                auto in_time_t = std::chrono::system_clock::to_time_t(now);

                fmt::print(
                    "[{:%Y-%m-%d %H:%M:%S}] {} - {}\n",

                    fmt::styled(
                        std::chrono::system_clock::time_point(std::chrono::system_clock::from_time_t(in_time_t)),
                        fmt::emphasis::bold | fg(fmt::rgb(245, 245, 184))
                    ),

                    fmt::styled(
                        lvl_to_str(log_level),
                        fmt::emphasis::bold
                    ),

                    fmt::styled(
                        formatted_message,
                        fg(fmt::rgb(255, 255, 230))
                    )
                );

                if (!m_force_flush) 
                    return;

                std::fflush(stdout);             
            }
        }

        /**
         * @brief Start the asynchronous logging thread.
         */
        CXXAPI_INLINE void start_async() {
            if (m_running) 
                return;
            
            m_log_buffer = std::make_unique<log_buffer_t>(m_buffer_size);

            m_running = true;

            m_worker_thread = std::thread(&c_logging::process_logs, this);
        }

        /**
         * @brief Stop the asynchronous logging thread and flush remaining messages.
         */
        CXXAPI_INLINE void stop_async() {
            if (!m_running) 
                return;     

            {
                std::lock_guard<std::mutex> lock(m_mutex);

                m_running = false;
            }

            m_condition.notify_all();

            if (m_worker_thread.joinable()) 
                m_worker_thread.join();

            if (m_log_buffer) {
                process_remaining_logs();

                m_log_buffer.reset();
            }
        }

      private:
        /**
         * @brief Handle buffer overflow according to the configured strategy.
         * @param msg Log message that could not be added.
         */
        CXXAPI_INLINE void handle_overflow(log_message_t&& msg) {
            switch (m_overflow_strategy) {
                case e_overflow_strategy::block:
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);

                        m_condition.wait(lock, [this] {
                            return !m_running || (m_log_buffer && m_log_buffer->size() < m_buffer_size);
                        });

                        if (m_running && m_log_buffer) {
                            m_log_buffer->push(std::move(msg));

                            m_condition.notify_one();
                        }

                        break;
                    }

                case e_overflow_strategy::discard_oldest:
                    {
                        if (m_log_buffer) {
                            log_message_t old_msg{};

                            if (m_log_buffer->pop(old_msg)) {
                                m_log_buffer->push(std::move(msg));

                                m_condition.notify_one();
                            }
                        }
                        break;
                    }

                case e_overflow_strategy::discard_newest:
                    break;
            }
        }

        /**
         * @brief Process and flush any remaining log messages in the buffer.
         */
        CXXAPI_INLINE void process_remaining_logs() {
            if (!m_log_buffer)
                return;

            auto remaining = m_log_buffer->get_batch(m_buffer_size);

            for (const auto& msg : remaining) {
                auto in_time_t = std::chrono::system_clock::to_time_t(msg.m_timestamp);

                fmt::print(
                    "[{:%Y-%m-%d %H:%M:%S}] {} - {}\n",

                    fmt::styled(
                        std::chrono::system_clock::time_point(std::chrono::system_clock::from_time_t(in_time_t)),
                        fmt::emphasis::bold | fg(fmt::rgb(245, 245, 184))
                    ),

                    fmt::styled(
                        lvl_to_str(msg.m_level),
                        fmt::emphasis::bold
                    ),

                    fmt::styled(
                        msg.m_message,
                        fg(fmt::rgb(255, 255, 230))
                    )
                );
            }

            std::fflush(stdout);
        }

        /**
         * @brief Worker thread function for processing log messages asynchronously.
         */
        CXXAPI_INLINE void process_logs() {
            constexpr std::size_t k_batch_size = 256u;

            while (true) {
                std::vector<log_message_t> batch{};

                {
                    std::unique_lock<std::mutex> lock(m_mutex);

                    m_condition.wait_for(lock, std::chrono::milliseconds(50), [this] {
                        return !m_running || (m_log_buffer && !m_log_buffer->empty());
                    });

                    if (!m_running 
                        && (!m_log_buffer 
                            || m_log_buffer->empty())) 
                        break;    
                }

                if (m_log_buffer) 
                    batch = m_log_buffer->get_batch(k_batch_size);
                
                if (batch.empty()) 
                    continue;     

                for (const auto& msg : batch) {
                    auto in_time_t = std::chrono::system_clock::to_time_t(msg.m_timestamp);

                    fmt::print(
                        "[{:%Y-%m-%d %H:%M:%S}] {} - {}\n",

                        fmt::styled(
                            std::chrono::system_clock::time_point(std::chrono::system_clock::from_time_t(in_time_t)),
                            fmt::emphasis::bold | fg(fmt::rgb(245, 245, 184))
                        ),

                        fmt::styled(
                            lvl_to_str(msg.m_level),
                            fmt::emphasis::bold
                        ),

                        fmt::styled(
                            msg.m_message,
                            fg(fmt::rgb(255, 255, 230))
                        )
                    );
                }

                std::fflush(stdout);

                m_condition.notify_all();
            }
        }

      private:
        /** @brief Whether to flush output immediately after each message. */
        bool m_force_flush{};

        /** @brief Minimum severity level to log. */
        e_log_level m_log_level{e_log_level::none};

        /** @brief Indicates if the asynchronous logger is running. */
        std::atomic<bool> m_running{};

        /** @brief Pointer to the internal log message buffer. */
        std::unique_ptr<log_buffer_t> m_log_buffer;

        /** @brief Mutex for synchronizing access to logger state. */
        std::mutex m_mutex;

        /** @brief Condition variable for coordinating producer and consumer threads. */
        std::condition_variable m_condition;

        /** @brief Worker thread for asynchronous logging. */
        std::thread m_worker_thread;

        /** @brief Maximum size of the log buffer. */
        std::size_t m_buffer_size{16384u};

        /** @brief Strategy for handling buffer overflows. */
        e_overflow_strategy m_overflow_strategy{e_overflow_strategy::discard_oldest};
    };
#endif // CXXAPI_USE_LOGGING_IMPL
}

#endif // CXXAPI_SHARED_LOGGING_HXX