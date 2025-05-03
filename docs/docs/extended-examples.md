# Extended examples

## Interacting with cookies

```cpp
cxxapi::http::response_t handler(cxxapi::http::http_ctx_t&& ctx) {
    // get cookie from request
    auto test_cookie = ctx.request().cookie("Test");

    // cookie is optional
    if (test_cookie.has_value()) {
        auto response = cxxapi::http::json_response_t{
            cxxapi::http::json_t::json_obj_t{
                // ...
            },

            cxxapi::http::e_status::ok
        };

        {
            // set cookie to response
            response.set_cookie(
                cxxapi::http::cookie_t{
                    .m_name = "__Secure-NewTest",
                    .m_value = test_cookie.value(),

                    .m_secure = true
                }
            );
        }

        return response;
    }
}
```

## Interacting with stream response

```cpp
struct file_chunk_provider_t {
    CXXAPI_INLINE file_chunk_provider_t(
        boost::asio::any_io_executor exec,
        const std::string& file_path,
        const std::size_t chunk_size = 512000u
    )
        : m_executor(exec), m_file(file_path, std::ios::binary), m_chunk(chunk_size) {
        if (!m_file.is_open())
            throw std::runtime_error(fmt::format("Failed to open file: {}", file_path));
    }

  public:
    CXXAPI_NOINLINE boost::asio::awaitable<std::vector<char>> next() {
        co_await boost::asio::post(m_executor, boost::asio::use_awaitable);

        std::vector<char> buffer(m_chunk, 0u);

        m_file.read(buffer.data(), static_cast<std::streamsize>(m_chunk));

        const auto bytes_read = m_file.gcount();

        if (bytes_read <= 0u)
            co_return std::vector<char>{};

        buffer.resize(static_cast<std::size_t>(bytes_read));

        co_return buffer;
    }

  private:
    boost::asio::any_io_executor m_executor;

    std::ifstream m_file{};
    std::size_t m_chunk{};
};

boost::asio::awaitable<cxxapi::http::response_t> get_file(cxxapi::http::http_ctx_t&& ctx) {
    try {
        const auto file_chunks = std::make_shared<file_chunk_provider_t>(
            co_await boost::asio::this_coro::executor, "file_path"
        );

        cxxapi::http::headers_t headers{};

        {
            headers.emplace(
                "Content-Disposition",

                fmt::format("attachment; filename=\"{}\"", "FILE_NAME.EXAMPLE")
            );
        }

        co_return cxxapi::http::stream_response_t{
            [file_chunks](boost::asio::ip::tcp::socket& socket) -> boost::asio::awaitable<void> {
                while (true) {
                    const auto chunk = co_await file_chunks->next();

                    if (chunk.empty())
                        break;

                    co_await cxxapi::http::send_chunk_async(socket, std::string_view(chunk.data(), chunk.size()));
                }
            },

            "application/octet-stream",

            cxxapi::http::e_status::ok,

            std::move(headers)
        };
    }
    catch (const std::exception& e) {
        cxxapi::g_logging->log(cxxapi::e_log_level::error, "Error in 'get_file' route: {}", e.what());

        co_return cxxapi::http::json_response_t{
            cxxapi::http::json_t::json_obj_t{
                {"message", "Internal server error"},

                {"error", e.what()}
            },

            cxxapi::http::e_status::internal_server_error
        };
    }
}
```

## Interacting with request files

```cpp
boost::asio::awaitable<cxxapi::http::response_t> upload_file(cxxapi::http::http_ctx_t&& ctx) {
    try {
        cxxapi::http::file_t file;

        {
            auto files = std::move(ctx.files());

            if (files.size() > 1u) {
                co_return cxxapi::http::json_response_t{
                    cxxapi::http::json_t::json_obj_t{
                        {"message", "Bad request"},

                        {"error", "Too many files"}
                    },

                    cxxapi::http::e_status::bad_request
                };
            }

            auto file = std::move(files.front());

            if (file.size() > 100u * 1024u * 1024u) {
                co_return cxxapi::http::json_response_t{
                    cxxapi::http::json_t::json_obj_t{
                        {"message", "Bad request"},

                        {"error", "File is too large"}
                    },

                    cxxapi::http::e_status::bad_request
                };
            }

            const auto original_name = file.name();

            if (file.in_memory()) {
                // file stored in std::vector<char>
            }
            else
                // file stored in temp file

            co_return cxxapi::http::json_response_t{
                cxxapi::http::json_t::json_obj_t{
                    {"data",

                     cxxapi::http::json_t::json_obj_t{
                         {"message", "Successful"}
                     }}
                },

                cxxapi::http::e_status::ok
            };
        }
    }
    catch (const std::exception& e) {
        cxxapi::g_logging->log(cxxapi::e_log_level::error, "Error in 'upload_file' route: {}", e.what());

        co_return cxxapi::http::json_response_t{
            cxxapi::http::json_t::json_obj_t{
                {"message", "Internal server error"},

                {"error", e.what()}
            },

            cxxapi::http::e_status::internal_server_error
        };
    }
}
```