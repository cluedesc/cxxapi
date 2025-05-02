/**
 * @file request.hxx
 * @brief HTTP request abstraction containing method, URI, headers, body and etc...
 */

#ifndef CXXAPI_HTTP_REQUEST_HXX
#define CXXAPI_HTTP_REQUEST_HXX

namespace cxxapi::http {
    /**
     * @brief Represents an HTTP request, including method, target URI, headers, body and etc...
     */
    struct request_t {
        /**
         * @brief Default-construct a new empty request.
         */
        CXXAPI_INLINE constexpr request_t() = default;

      public:
        /**
         * @brief Determine whether the client requested a persistent connection.
         *
         * Checks the "Connection" header; if absent, defaults to true (keep-alive).
         *
         * @return true if the Connection header is missing or equals "keep-alive" (case-insensitive).
         * @return false otherwise.
         */
        CXXAPI_INLINE bool keep_alive() const {
            const auto it = m_headers.find("connection");

            if (it == m_headers.end())
                return true;

            return boost::iequals(it->second, "keep-alive");
        }

        /**
         * @brief Retrieve the value of a named cookie from the "Cookie" header.
         * @param name The name of the cookie to look up.
         *
         * Parses the Cookie header into name=value pairs separated by ';' and trims whitespace.
         *
         * @return std::optional<std::string_view> The cookie value if found, or std::nullopt if not present.
         */
        CXXAPI_INLINE std::optional<std::string_view> cookie(const std::string_view& name) const {
            const auto it = m_headers.find("cookie");

            if (it == m_headers.end())
                return std::nullopt;

            const auto& header_value = it->second;

            std::size_t pos{};

            while (pos < header_value.length()) {
                auto next_semi = header_value.find(';', pos);

                if (next_semi == std::string_view::npos)
                    next_semi = header_value.length();

                std::string_view cookie_pair{header_value.data() + pos, next_semi - pos};

                pos = next_semi + 1u;

                cookie_pair.remove_prefix(std::min(cookie_pair.find_first_not_of(" \t"), cookie_pair.size()));

                auto eq_pos = cookie_pair.find('=');

                if (eq_pos != std::string_view::npos) {
                    std::string_view current_name = cookie_pair.substr(0u, eq_pos);

                    while (!current_name.empty()
                           && std::isspace(static_cast<unsigned char>(current_name.back())))
                        current_name.remove_suffix(1u);

                    if (current_name == name) {
                        std::string_view value = cookie_pair.substr(eq_pos + 1u);

                        value.remove_prefix(std::min(value.find_first_not_of(" \t"), value.size()));

                        while (!value.empty()
                               && std::isspace(static_cast<unsigned char>(value.back())))
                            value.remove_suffix(1u);

                        return value;
                    }
                }
            }

            return std::nullopt;
        }

      public:
        /**
         * @brief Holds information about the client making the HTTP request.
         *
         * Encapsulates details such as the client's remote address.
         */
        struct client_info_t {
            /**
             * @brief Default constructor for an empty client info object.
             */
            CXXAPI_INLINE constexpr client_info_t() = default;

            /**
             * @brief Constructs a client info object with a specified remote address.
             * @param remote_addr The client's remote address.
             * @param remote_port The client's remote port.
             */
            CXXAPI_INLINE client_info_t(
                const std::string_view& remote_addr, 
                const std::uint16_t& remote_port
            )
                : m_remote_addr(remote_addr),
                m_remote_port(remote_port) {
            }

          public:
            /**
             * @brief Access the remote address for reading or writing.
             * @return Reference to the string.
             */
            CXXAPI_INLINE auto& remote_addr() { return m_remote_addr; }

            /**
             * @brief Access the remote address for reading.
             * @return Const reference to the string.
             */
            CXXAPI_INLINE const auto& remote_addr() const { return m_remote_addr; }

            /**
             * @brief Access the remote port for reading or writing.
             * @return Reference to the uint16.
             */
            CXXAPI_INLINE auto& remote_port() { return m_remote_port; }

            /**
             * @brief Access the remote port for reading.
             * @return Const reference to the uint16.
             */
            CXXAPI_INLINE const auto& remote_port() const { return m_remote_port; }

          private:
            /** @brief The client's remote address. */
            std::string m_remote_addr{};

            /** @brief The client's remote port. */
            std::uint16_t m_remote_port{};
        };

      public:
        /**
         * @brief Access the HTTP method for reading or writing.
         * @return Reference to the e_method enum.
         */
        CXXAPI_INLINE auto& method() { return m_method; }

        /**
         * @brief Access the HTTP method for reading.
         * @return Const reference to the e_method enum.
         */
        CXXAPI_INLINE const auto& method() const { return m_method; }

        /**
         * @brief Access the request URI for reading or writing.
         * @return Reference to the uri_t object.
         */
        CXXAPI_INLINE auto& uri() { return m_uri; }

        /**
         * @brief Access the request URI for reading.
         * @return Const reference to the uri_t object.
         */
        CXXAPI_INLINE const auto& uri() const { return m_uri; }

        /**
         * @brief Access the message body for reading or writing.
         * @return Reference to the body_t object.
         */
        CXXAPI_INLINE auto& body() { return m_body; }

        /**
         * @brief Access the message body for reading.
         * @return Const reference to the body_t object.
         */
        CXXAPI_INLINE const auto& body() const { return m_body; }

        /**
         * @brief Access the header map for reading or writing.
         * @return Reference to the headers_t container.
         */
        CXXAPI_INLINE auto& headers() { return m_headers; }

        /**
         * @brief Access the header map for reading.
         * @return Const reference to the headers_t container.
         */
        CXXAPI_INLINE const auto& headers() const { return m_headers; }

        /**
         * @brief Access the parse path for reading or writing.
         * @return Reference to the string.
         */
        CXXAPI_INLINE auto& parse_path() { return m_parse_path; }

        /**
         * @brief Access the parse path for reading.
         * @return Const reference to the string.
         */
        CXXAPI_INLINE const auto& parse_path() const { return m_parse_path; }

        /**
         * @brief Access the client info for reading or writing.
         * @return Reference to the client_info_t struct.
         */
        CXXAPI_INLINE auto& client() { return m_client_info; }

        /**
         * @brief Access the clientinfo for reading.
         * @return Const reference to the client_info_t struct.
         */
        CXXAPI_INLINE const auto& client() const { return m_client_info; }

      private:
        /** @brief HTTP method. */
        e_method m_method{};

        /** @brief Request URI. */
        uri_t m_uri{};

        /** @brief Request body. */
        body_t m_body{};

        /** @brief HTTP headers. */
        headers_t m_headers{};

        /** @brief Client information. */
        client_info_t m_client_info{};

        /** @brief Path for parse files */
        boost::filesystem::path m_parse_path{};
    };
}

#endif // CXXAPI_HTTP_REQUEST_HXX