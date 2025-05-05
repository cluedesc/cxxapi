/**
 * @file http_ctx.hxx
 * @brief HTTP context for handling request data and parsing.
 */

#ifndef CXXAPI_HTTP_HTTP_CTX_HXX
#define CXXAPI_HTTP_HTTP_CTX_HXX

namespace cxxapi::http {
    /**
     * @brief HTTP context structure for handling request data and parsing.
     */
    struct http_ctx_t {
        /**
         * @brief Default constructor.
         */
        CXXAPI_INLINE http_ctx_t() = default;

        /**
         * @brief Constructor initializing with request and parameters.
         * @param request HTTP request object.
         * @param params Request parameters.
         */
        CXXAPI_INLINE http_ctx_t(const request_t& request, const params_t& params)
            : m_request(request), m_params(params) {
        }

      public:
        /**
         * @brief Deleted copy constructor to prevent copying.
         */
        CXXAPI_INLINE http_ctx_t(const http_ctx_t&) = delete;

        /**
         * @brief Deleted copy assignment operator to prevent copying.
         */
        CXXAPI_INLINE http_ctx_t& operator=(const http_ctx_t&) = delete;

        /**
         * @brief Move constructor.
         */
        CXXAPI_INLINE http_ctx_t(http_ctx_t&&) = default;

        /**
         * @brief Move assignment operator.
         */
        CXXAPI_INLINE http_ctx_t& operator=(http_ctx_t&&) = default;

      private:
        /**
         * @brief Asynchronously parses the request body (e.g., multipart/form-data).
         * @param executor Executor for asynchronous operations.
         * @param chunk_size Size of the chunk for parsing.
         * @param chunk_size_disk Size of the chunk for disk operations.
         * @param max_size_file_in_memory Maximum size of a file in memory.
         * @param max_size_files_in_memory Maximum total size of files in memory.
         */
        CXXAPI_NOINLINE boost::asio::awaitable<void> init_parsing(
            boost::asio::any_io_executor& executor,

            std::size_t chunk_size,
            std::size_t chunk_size_disk,

            std::size_t max_size_file_in_memory,
            std::size_t max_size_files_in_memory
        ) {
            const auto content_type_it = m_request.headers().find("content-type");

            if (content_type_it != m_request.headers().end()) {
                const auto content_type = content_type_it->second;

                const auto boundary = _extract_boundary(content_type);

                if (!boundary.empty()) {
                    if (!m_request.parse_path().empty()) {
                        m_files = co_await multipart_t::parse_from_file_async(
                            executor,

                            m_request.parse_path(),

                            boundary,

                            chunk_size,
                            chunk_size_disk,

                            max_size_file_in_memory,
                            max_size_files_in_memory
                        );

                        boost::system::error_code error_code{};

                        boost::filesystem::remove(m_request.parse_path(), error_code);

                        if (error_code) {
#ifdef CXXAPI_USE_LOGGING_IMPL
                            g_logging->log(
                                e_log_level::warning, 
                                
                                "[HTTP-Processing] Failed to delete temp file (path: {}): {}", 
                                
                                m_request.parse_path().string(), error_code.message()
                            );
#endif // CXXAPI_USE_LOGGING_IMPL
                        }
                    }
                    else {
                        if (boost::ifind_first(content_type, "multipart/form-data")) {
                            m_files = co_await multipart_t::parse_async(
                                executor,

                                m_request.body(),

                                boundary,

                                chunk_size_disk,
                                
                                max_size_file_in_memory,
                                max_size_files_in_memory
                            );
                        }
                    }
                }
            }

            co_return;
        }

      public:
        /**
         * @brief Static factory method to create http_ctx_t with parsing.
         * @param executor Executor for asynchronous operations.
         * @param request HTTP request object.
         * @param params Request parameters.
         * @param chunk_size Size of the chunk for parsing.
         * @param chunk_size_disk Size of the chunk for disk operations.
         * @param max_size_file_in_memory Maximum size of a file in memory.
         * @param max_size_files_in_memory Maximum total size of files in memory.
         * @return Awaitable http_ctx_t instance.
         */
        CXXAPI_NOINLINE static boost::asio::awaitable<http_ctx_t> create(
            boost::asio::any_io_executor& executor,
            
            const request_t& request,
            const params_t& params,

            std::size_t chunk_size = 16384u,
            std::size_t chunk_size_disk = 65536u,

            std::size_t max_size_file_in_memory = 1048576u,
            std::size_t max_size_files_in_memory = 10485760u
        ) {
            http_ctx_t ctx{request, params};

            {
                co_await ctx.init_parsing(
                    executor, 

                    chunk_size,
                    chunk_size_disk,
                    
                    max_size_file_in_memory, 
                    max_size_files_in_memory
                );
            }

            co_return ctx;
        }

        /**
         * @brief Retrieves a file by field name from the parsed multipart data.
         * @param field_name Name of the file field.
         * @return Pointer to the file_t object or nullptr if not found.
         */
        CXXAPI_INLINE file_t* file(const std::string& field_name) {
            auto it = m_files.find(field_name);

            return (it != m_files.end()) ? &it->second : nullptr;
        }

      public:
        /**
         * @brief Getter for the HTTP request object.
         * @return Reference to the request_t object.
         */
        CXXAPI_INLINE auto& request() { return m_request; }

        /**
         * @brief Getter for the request parameters.
         * @return Reference to the params_t object.
         */
        CXXAPI_INLINE auto& params() { return m_params; }

        /**
         * @brief Getter for the parsed files.
         * @return Reference to the files_t container.
         */
        CXXAPI_INLINE auto& files() { return m_files; }

      public:
        /**
         * @brief Extracts the boundary from a Content-Type header.
         * @param content_type Content-Type header value.
         * @return Extracted boundary string.
         */
        CXXAPI_INLINE std::string _extract_boundary(const std::string_view& content_type) {
            std::string ct{content_type};

            std::vector<std::string> parts{};

            boost::split(parts, ct, boost::is_any_of(";"));

            for (auto& part : parts) {
                boost::trim(part);

                if (boost::istarts_with(part, "boundary=")) {
                    auto val = part.substr(std::string("boundary=").size());

                    boost::trim(val);

                    if (val.size() >= 2u
                        && ((val.front() == '"' && val.back() == '"')
                            || (val.front() == '\'' && val.back() == '\'')))
                        val = val.substr(1u, val.size() - 2u);

                    return val;
                }
            }
            return {};
        }

      private:
        /** @brief HTTP request object. */
        request_t m_request{};

        /** @brief Request parameters. */
        params_t m_params{};

        /** @brief Parsed files from multipart request. */
        files_t m_files{};
    };
}

#endif // CXXAPI_HTTP_HTTP_CTX_HXX
