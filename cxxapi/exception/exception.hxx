/**
 * @file exception.hxx
 * @brief Defines the exception hierarchy used throughout the CXXAPI framework.
 * 
 * Provides a base exception class (`base_exception_t`) derived from `std::runtime_error`,
 * as well as several specialized exception types used to signal various error conditions
 * encountered in client-server interaction, server processing, and general CXXAPI operations.
 * All exceptions support status codes, custom message prefixes, and full error formatting.
 */

#ifndef CXXAPI_EXCEPTION_HXX
#define CXXAPI_EXCEPTION_HXX

namespace cxxapi {
    /**
     * @brief Base exception type for all errors in CXXAPI.
     *
     * Inherits from std::runtime_error and provides status code handling,
     * optional message prefixes, and full error formatting.
     */
    struct base_exception_t : public std::runtime_error {
        /**
         * @brief Construct a base exception with a plain message.
         * @param str Error message.
         */
        CXXAPI_INLINE base_exception_t(const std::string& str)
            : std::runtime_error(str), m_message(str), m_what(str) {
        }

        /**
         * @brief Construct a base exception with a message, status code, and optional prefix.
         * @param str Error message.
         * @param status Associated status code.
         * @param prefix Optional prefix to include in the formatted message.
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
         * @brief Obtaining the status of the code associated with an exception (mutable).
         * @return Reference to the status code.
         */
        CXXAPI_INLINE auto& status() { return m_status; }

        /**
         * @brief Obtaining the status of the code associated with an exception (read-only).
         * @return Const reference to the status code.
         */
        CXXAPI_INLINE const auto& status() const { return m_status; }

        /**
         * @brief Get the prefix of the message used in the exception (mutable).
         * @return Reference to the message prefix.
         */
        CXXAPI_INLINE auto& prefix() { return m_prefix; }

        /**
         * @brief Get the prefix of the message used in the exception (read-only).
         * @return Const Reference to the message prefix.
         */
        CXXAPI_INLINE const auto& prefix() const { return m_prefix; }

        /**
         * @brief Get the message used in the exception (mutable).
         * @return Reference to the message.
         */
        CXXAPI_INLINE auto& message() { return m_message; }

        /**
         * @brief Get the message used in the exception (read-only).
         * @return Const reference to the message.
         */
        CXXAPI_INLINE const auto& message() const { return m_message; }

        /**
         * @brief Get the full formatted error message.
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
        /**
         * @brief Exception type for server-client errors in CXXAPI.
         *
         * Automatically sets the prefix to "Server-Client".
         */
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

        /**
         * @brief Exception type for server errors in CXXAPI.
         *
         * Automatically sets the prefix to "Server".
         */
        struct server_exception_t : public base_exception_t {
            /**
             * @brief Construct a new server_exception_t with a message and status.
             * @param str Error message.
             * @param status Associated status code.
             * @param prefix Optional prefix to include in the formatted message.
             */
            CXXAPI_INLINE server_exception_t(
                const std::string& str,

                const std::size_t& status = 0u,
                const std::string_view& prefix = "Server"
            )
                : base_exception_t(str, status, prefix) {
            }
        };

        /**
         * @brief Exception type for HTTP processing errors in CXXAPI.
         *
         * Automatically sets the prefix to "HTTP-Processing".
         */
        struct processing_exception_t : public base_exception_t {
            /**
             * @brief Construct a new processing_exception_t with a message and status.
             * @param str Error message.
             * @param status Associated status code.
             * @param prefix Optional prefix to include in the formatted message.
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
