/**
 * @file exception.hxx
 * @brief Exception types for the CXXAPI.
 */

#ifndef CXXAPI_EXCEPTION_HXX
#define CXXAPI_EXCEPTION_HXX

namespace cxxapi {
    /**
     * @brief Base exception type for CXXAPI errors.
     *
     * Inherits from std::runtime_error and provides a common base for all exceptions
     * thrown by the CXXAPI.
     */
    struct base_exception_t : public std::runtime_error {
        /**
         * @brief Construct a new base_exception_t with a message.
         * @param str Error message to describe the exception.
         */
        CXXAPI_INLINE base_exception_t(const std::string& str)
            : std::runtime_error(str), m_message(str), m_what(str) {
        }

        /**
         * @brief Construct a new base_exception_t with a message.
         * @param str Error message to describe the exception.
         * @param status Status code associated with the exception.
         * @param prefix Prefix to prepend to the error message.
         */
        CXXAPI_INLINE base_exception_t(const std::string& str, const std::size_t& status, const std::string_view& prefix = "")
            : std::runtime_error(str), m_status(status), m_prefix(prefix), m_message(str) {
            if (!m_prefix.empty()) {
                m_what = fmt::format("[{}] {}", m_prefix, m_message);
            }
            else
                m_what = m_message;
        }

      public:
        /**
         * @brief Get the status code associated with the exception.
         * @return Reference to the status code.
         */
        CXXAPI_INLINE auto& status() { return m_status; }

        /**
         * @brief Get the status code associated with the exception.
         * @return Const reference to the status code.
         */
        CXXAPI_INLINE const auto& status() const { return m_status; }

        /**
         * @brief Get the message prefix used in the exception.
         * @return Reference to the message prefix.
         */
        CXXAPI_INLINE auto& prefix() { return m_prefix; }

        /**
         * @brief Get the message prefix used in the exception.
         * @return Const reference to the message prefix.
         */
        CXXAPI_INLINE const auto& prefix() const { return m_prefix; }

        /**
         * @brief Get the message used in the exception.
         * @return Reference to the message.
         */
        CXXAPI_INLINE auto& message() { return m_message; }

        /**
         * @brief Get the message used in the exception.
         * @return Const reference to the message.
         */
        CXXAPI_INLINE const auto& message() const { return m_message; }

        /**
         * @brief Get the full formatted error message (including prefix if set).
         * @return Pointer to a null-terminated C-string with the exception message.
         */
        CXXAPI_INLINE const char* what() const noexcept override { return m_what.c_str(); }

      private:
        /** @brief Status code associated with the exception. */
        std::size_t m_status{};

        /** @brief Optional prefix used to qualify the error message. */
        std::string_view m_prefix{};

        /** @brief Raw message content (without prefix). */
        std::string m_message{};

        /** @brief Cached full message string used in what(). */
        std::string m_what{};
    };

    namespace exceptions {
        struct client_exception_t : public base_exception_t {
            /**
             * @brief Construct a new client_exception_t with a message and status.
             * @param str Error message to describe the exception.
             * @param status Status code associated with the exception.
             * @param prefix Prefix to prepend to the error message.
             */
            CXXAPI_INLINE client_exception_t(
                const std::string& str,

                const std::size_t& status = 0u,
                const std::string_view& prefix = "Server-Client"
            )
                : base_exception_t(str, status, prefix) {
            }
        };

        struct server_exception_t : public base_exception_t {
            /**
             * @brief Construct a new server_exception_t with a message and status.
             * @param str Error message to describe the exception.
             * @param status Status code associated with the exception.
             * @param prefix Prefix to prepend to the error message.
             */
            CXXAPI_INLINE server_exception_t(
                const std::string& str,

                const std::size_t& status = 0u,
                const std::string_view& prefix = "Server"
            )
                : base_exception_t(str, status, prefix) {
            }
        };

        struct processing_exception_t : public base_exception_t {
            /**
             * @brief Construct a new processing_exception_t with a message and status.
             * @param str Error message to describe the exception.
             * @param status Status code associated with the exception.
             * @param prefix Prefix to prepend to the error message.
             */
            CXXAPI_INLINE processing_exception_t(
                const std::string& str,

                const std::size_t& status = 0u,
                const std::string_view& prefix = "HTTP-Processing"
            )
                : base_exception_t(str, status, prefix) {
            }
        };
    }
}

#endif // CXXAPI_EXCEPTION_HXX
