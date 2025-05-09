/**
 * @file response.hxx
 * @brief HTTP response abstractions.
 */

#ifndef CXXAPI_HTTP_RESPONSE_HXX
#define CXXAPI_HTTP_RESPONSE_HXX

namespace cxxapi::http {
    /**
     * @brief Represents a generic HTTP response, including status, headers, body, cookies, and optional streaming.
     */
    struct response_t {
        /**
         * @brief Type alias for a callback invoked to send a streaming response.
         *
         * The callback receives a connected TCP socket and should co_await
         * asynchronous write operations on it.
         */
        using callback_t = std::function<boost::asio::awaitable<void>(boost::asio::ip::tcp::socket&)>;

      public:
        /**
         * @brief Default constructor.
         */
        CXXAPI_INLINE constexpr response_t() = default;

        /**
         * @brief Virtual destructor.
         */
        CXXAPI_INLINE virtual ~response_t() = default;

        /**
         * @brief Construct a plain-text response.
         * @param body The response body as a string.
         * @param status_code The HTTP status code to send (default is 200 OK).
         * @param headers Additional headers to include.
         */
        CXXAPI_INLINE response_t(std::string&& body, e_status&& status_code = e_status::ok, headers_t&& headers = {}) {
            m_body = std::move(body);

            {
                if (!headers.empty()) {
                    for (auto& header : headers)
                        m_headers[std::move(header.first)] = std::move(header.second);
                }

                m_headers.emplace("Content-Type", "text/plain");
            }

            m_status = std::move(status_code);
        }

      public:
        /**
         * @brief Add a Set-Cookie header for response.
         * @param cookie The cookie struct to add.
         *
         * Builds a cookie string including optional attributes.
         */
        CXXAPI_INLINE void set_cookie(cookie_t cookie) {
            if (cookie.m_name.rfind("__Secure-", 0) == 0 && !cookie.m_secure)
                throw std::invalid_argument("__Secure- cookies require Secure flag");

            if (cookie.m_name.rfind("__Host-", 0) == 0) {
                if (!cookie.m_secure
                    || cookie.m_domain != ""
                    || cookie.m_path != "/") {
                    throw std::invalid_argument("__Host- cookies require Secure, no Domain, Path=/");
                }
            }

            fmt::memory_buffer buf;

            fmt::format_to(std::back_inserter(buf), "{}={}", cookie.m_name, cookie.m_value);

            if (!cookie.m_domain.empty())
                fmt::format_to(std::back_inserter(buf), "; Domain={}", cookie.m_domain);

            if (!cookie.m_path.empty())
                fmt::format_to(std::back_inserter(buf), "; Path={}", cookie.m_path);

            if (cookie.m_max_age.count() > 0) {
                fmt::format_to(std::back_inserter(buf), "; Max-Age={}", cookie.m_max_age.count());

                auto now = boost::posix_time::second_clock::universal_time();

                auto exp = now + boost::posix_time::seconds{cookie.m_max_age.count()};

                auto expires = fmt::format("{} {}", boost::posix_time::to_iso_extended_string(exp), "GMT");

                expires = boost::posix_time::to_simple_string(exp);

                fmt::format_to(std::back_inserter(buf), "; Expires={}", expires);
            }

            if (cookie.m_secure)
                fmt::format_to(std::back_inserter(buf), "; Secure");

            if (cookie.m_http_only)
                fmt::format_to(std::back_inserter(buf), "; HttpOnly");

            if (!cookie.m_same_site.empty())
                fmt::format_to(std::back_inserter(buf), "; SameSite={}", cookie.m_same_site);

            m_cookies.emplace_back(std::move(fmt::to_string(buf)));
        }

      public:
        /**
         * @brief Get the body (mutable).
         * @return Reference to the body struct.
         */
        CXXAPI_INLINE auto& body() { return m_body; }

        /**
         * @brief Get the headers (mutable).
         * @return Reference to the headers struct.
         */
        CXXAPI_INLINE auto& headers() { return m_headers; }

        /**
         * @brief Get the cookies (mutable).
         * @return Reference to the cookies struct.
         */
        CXXAPI_INLINE auto& cookies() { return m_cookies; }

        /**
         * @brief Get the status (mutable).
         * @return Reference to the status enum.
         */
        CXXAPI_INLINE auto& status() { return m_status; }

        /**
         * @brief Get the cookies (mutable).
         * @return Reference to the callback struct.
         */
        CXXAPI_INLINE auto& callback() { return m_callback; }

        /**
         * @brief Get the streaming flag (mutable).
         * @return Reference to the streaming flag.
         */
        CXXAPI_INLINE auto& stream() { return m_stream; }

      public:
        /** @brief Body. */
        body_t m_body{};

        /** @brief Headers. */
        headers_t m_headers{};

        /** @brief Cookies. */
        cookies_t m_cookies{};

        /** @brief Status code. */
        e_status m_status{e_status::ok};

        /** @brief Streaming callback. */
        callback_t m_callback{};

        /** @brief Streaming flag. */
        bool m_stream{false};
    };

    /**
     * @brief A JSON response, serializing a JSON object to the body.
     *
     * Sets "Content-Type: application/json".
     */
    struct json_response_t : public response_t {
        /**
         * @brief Default constructor.
         */
        CXXAPI_INLINE constexpr json_response_t() = default;

        /**
         * @brief Construct a JSON response from a JSON object.
         * @param body The JSON object to serialize.
         * @param status_code The HTTP status code (default is 200 OK).
         * @param headers Additional headers to include.
         */
        CXXAPI_INLINE json_response_t(const json_t::json_obj_t& body, e_status&& status_code = e_status::ok, headers_t&& headers = {}) {
            m_body = body.empty() ? "" : json_t::serialize(body);

            {
                if (!headers.empty()) {
                    for (auto& header : headers)
                        m_headers[std::move(header.first)] = std::move(header.second);
                }

                m_headers.emplace("Content-Type", "application/json");
            }

            m_status = std::move(status_code);
        }
    };

    /**
     * @brief A file response, streaming a file from disk.
     *
     * Sets appropriate Content-Type, Content-Length, and ETag headers.
     * On errors, returns an error status and plain-text message.
     */
    struct file_response_t : public response_t {
        /**
         * @brief Default constructor.
         */
        CXXAPI_INLINE constexpr file_response_t() = default;

        /**
         * @brief Construct a file response for a given file path.
         * @param file_path Path to the file on disk.
         * @param status_code The HTTP status code (default is 200 OK).
         * @param headers Additional headers to include.
         *
         * If the file is missing or not regular, returns 404 or 400 with a text message.
         * Otherwise sets up streaming callback sending file chunks.
         */
        CXXAPI_INLINE file_response_t(
            const boost::filesystem::path& file_path,

            e_status&& status_code = e_status::ok,

            headers_t&& headers = {}
        ) {
            if (!boost::filesystem::exists(file_path)) {
                m_body = "File not found";

                m_status = e_status::not_found;

                return;
            }

            if (!boost::filesystem::is_regular_file(file_path)) {
                m_body = "Bad request";

                m_status = e_status::bad_request;

                return;
            }

            m_stream = true;

            try {
                auto file_size = boost::filesystem::file_size(file_path);

                if (!headers.empty()) {
                    for (auto& header : headers)
                        m_headers[std::move(header.first)] = std::move(header.second);
                }

                const auto content_type = mime_types_t::get(file_path);

                m_headers.try_emplace("Content-Type", std::string(content_type));
                m_headers.try_emplace("Content-Length", boost::lexical_cast<std::string>(file_size));

                {
                    auto last_write = boost::filesystem::last_write_time(file_path);
                    auto etag_value = fmt::format("\"{}-{}\"", last_write, file_size);

                    m_headers.emplace("ETag", std::move(etag_value));
                }

                m_status = std::move(status_code);

                auto path_copy = file_path;

                m_callback = [path_copy, file_size](boost::asio::ip::tcp::socket& socket) -> boost::asio::awaitable<void> {
                    std::ifstream file_stream(path_copy.string(), std::ios::binary);

                    if (!file_stream) 
                        throw base_exception_t("Failed to open file");
                    
                    std::array<char, 8192u> buffer{};

                    std::streamsize total_sent{};

                    while (total_sent < file_size && file_stream && socket.is_open()) {
                        file_stream.read(buffer.data(), buffer.size());

                        auto bytes_read = file_stream.gcount();

                        if (bytes_read <= 0)
                            break;

                        co_await send_chunk_async(
                            socket,

                            std::string(buffer.data(), bytes_read)
                        );

                        total_sent += bytes_read;
                    }
                };
            }
            catch (const boost::filesystem::filesystem_error&) {
                m_body = "Internal server error";

                m_status = e_status::internal_server_error;
            }
            catch (const std::exception&) {
                m_body = "Internal server error";

                m_status = e_status::internal_server_error;
            }
        }
    };

    /**
     * @brief A generic streaming response using a user-provided callback.
     *
     * Sets "Cache-Control: no-cache" and a default or specified Content-Type.
     */
    struct stream_response_t : public response_t {
        /**
         * @brief Default constructor.
         */
        CXXAPI_INLINE constexpr stream_response_t() = default;

        /**
         * @brief Construct a streaming response.
         * @param callback The callback to invoke for streaming data.
         * @param content_type The MIME type of the stream (default application/octet-stream).
         * @param status_code The HTTP status code (default is 200 OK).
         * @param headers Additional headers to include.
         */
        CXXAPI_INLINE stream_response_t(
            callback_t&& callback,

            std::string&& content_type = "application/octet-stream",
            e_status&& status_code = e_status::ok,

            headers_t&& headers = {}
        ) {
            m_callback = std::move(callback);

            if (!headers.empty()) {
                for (auto& header : headers)
                    m_headers[std::move(header.first)] = std::move(header.second);
            }

            m_stream = true;

            m_headers.emplace("Cache-Control", "no-cache");
            m_headers.emplace("Content-Type", std::move(content_type));

            m_status = std::move(status_code);
        }
    };

    /**
     * @brief A redirect response setting the Location header.
     *
     * Validates that status_code is a redirect code (3xx), defaults to 302 Found.
     */
    struct redirect_response_t : public response_t {
        /**
         * @brief Default constructor.
         */
        CXXAPI_INLINE constexpr redirect_response_t() = default;

        /**
         * @brief Construct a redirect response to the given location.
         * @param location The URL to redirect to.
         * @param status_code The 3xx redirect status code (default is 302 Found).
         * @param headers Additional headers to include.
         *
         * Enforces a valid 3xx status code, sets Location and text/plain content type.
         */
        CXXAPI_INLINE redirect_response_t(
            const std::string_view& location,

            e_status&& status_code = e_status::found,

            headers_t&& headers = {}
        ) {
            m_body = "";

            if (status_code != e_status::moved_permanently
                && status_code != e_status::found
                && status_code != e_status::see_other
                && status_code != e_status::temporary_redirect
                && status_code != e_status::permanent_redirect)
                status_code = e_status::found;

            m_status = std::move(status_code);

            if (!headers.empty()) {
                for (auto& header : headers)
                    m_headers[std::move(header.first)] = std::move(header.second);
            }

            m_headers.emplace("Location", std::string(location));
            m_headers.emplace("Content-Type", "text/plain");
        }
    };

    /**
     * @enum e_response_class
     * @brief Enumerates the supported built-in response types.
     *
     * This enum is typically used to represent the type of HTTP response
     * being generated, such as plain text or JSON.
     */
    enum struct e_response_class : std::uint8_t {
        plain, ///< A plain text response (e.g., text/plain).
        json   ///< A JSON-formatted response (e.g., application/json).
    };

    /**
     * @brief A factory wrapper for generating typed responses.
     * @tparam _class_t The response class type to instantiate.
     * 
     * Must inherit from `response_t` and be either `response_t` or `json_response_t`.
     * 
     * This template provides a static `make_response` method to construct
     * a response of a specified type (e.g., `json_response_t`).
     */
    template <typename _class_t = response_t>
    struct response_class_t {
        static_assert(std::is_base_of_v<response_t, _class_t>, "Class must inherit from response_t");

        static_assert(std::is_same_v<response_t, std::decay_t<_class_t>> 
            || std::is_same_v<json_response_t, std::decay_t<_class_t>>, "Class must not be response_t or json_response_t");

      public:
        /**
         * @brief Default constructor.
         */
        CXXAPI_INLINE constexpr response_class_t() = default;

        /**
         * @brief Construct a response of the specified type.
         * @tparam _body_t The type of the response body.
         * @param body The content to be sent as the response body.
         * @param status_code HTTP status code (default is 200 OK).
         * @param headers Optional additional HTTP headers.
         * @return An instance of the specified response type.
         */
        template <typename _body_t>
        CXXAPI_INLINE static _class_t make_response(
            _body_t&& body,

            e_status&& status_code = e_status::ok,

            headers_t&& headers = {}
        ) {
            return _class_t(
                std::forward<_body_t>(body),

                std::move(status_code),

                std::move(headers)
            );
        }
    };
}

#endif // CXXAPI_HTTP_RESPONSE_HXX