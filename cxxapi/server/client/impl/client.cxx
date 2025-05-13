#include <cxxapi.hxx>

namespace cxxapi::server {
    boost::asio::awaitable<void> client_t::start() {
        if (!m_server.running(std::memory_order_acquire))
            co_return;

        while (true) {
            std::tuple<bool, std::size_t> catch_tuple{};

            try {
                if (!m_server.running(std::memory_order_relaxed))
                    break;

                m_parsed_request = {};

                m_buffer.consume(m_buffer.size());

                boost::system::error_code error_code{};

                boost::beast::http::request_parser<boost::beast::http::empty_body> hdr_parser{};

                hdr_parser.body_limit(m_cxxapi.cfg().m_server.m_max_request_size);

                co_await boost::beast::http::async_read_header(
                    m_socket,
                    m_buffer,

                    hdr_parser,

                    boost::asio::redirect_error(boost::asio::use_awaitable, error_code)
                );

                if (error_code == boost::beast::http::error::end_of_stream
                    || error_code == boost::asio::error::connection_reset
                    || error_code == boost::asio::error::operation_aborted)
                    break;

                if (error_code)
                    throw exceptions::client_exception_t(error_code.message(), 500u);

                const auto& hdr_req = hdr_parser.get();

                http::request_t req{};

                auto is_websocket = false;

                if (!m_parsed_request.target().empty()) {
                    is_websocket = boost::beast::websocket::is_upgrade(m_parsed_request);
                }
                else
                    is_websocket = boost::beast::websocket::is_upgrade(hdr_req);

                {
                    req.uri() = hdr_req.target();
                    req.method() = http::str_to_method(hdr_req.method_string());

                    for (auto& field : hdr_req)
                        req.headers().emplace(field.name_string(), field.value());
                }

                auto parse = false;

                auto ctype_it = req.headers().find("content-type");

                if (ctype_it != req.headers().end()) {
                    auto ctype = ctype_it->second;

                    if (boost::algorithm::istarts_with(ctype, "multipart/form-data"))
                        parse = true;

                    if (parse) {
                        auto boundary = http::utils::_extract_boundary(ctype);

                        auto clen_it = req.headers().find("content-length");

                        if (clen_it == req.headers().end())
                            throw exceptions::client_exception_t("Missing Content-Length for multipart", 400u);

                        auto clen = std::stoull(clen_it->second);

                        const auto& cxxapi_cfg = m_cxxapi.cfg();

                        if (clen > cxxapi_cfg.m_server.m_max_request_size)
                            throw exceptions::client_exception_t("Max request size reached", 400u);

                        auto tmp_file = cxxapi_cfg.m_server.m_tmp_dir
                                      / boost::filesystem::unique_path("upload-%%%%-%%%%");

                        co_await http::utils::stream_request(
                            m_socket,
                            m_buffer,

                            clen,

                            cxxapi_cfg.m_server.m_max_chunk_size,

                            tmp_file
                        );

                        req.parse_path() = std::move(tmp_file);
                    }
                }

                if (!parse) {
                    boost::beast::http::request_parser<boost::beast::http::string_body> parser{std::move(hdr_parser)};

                    co_await boost::beast::http::async_read(
                        m_socket,
                        m_buffer,

                        parser,

                        boost::asio::redirect_error(boost::asio::use_awaitable, error_code)
                    );

                    if (error_code == boost::beast::http::error::end_of_stream
                        || error_code == boost::asio::error::connection_reset
                        || error_code == boost::asio::error::operation_aborted)
                        break;

                    if (error_code)
                        throw exceptions::client_exception_t(error_code.message(), 500u);

                    m_parsed_request = std::move(parser.release());

                    req.body() = std::move(m_parsed_request.body());
                }

                {
                    auto remote_endpoint = m_socket.remote_endpoint();

                    auto client_info = cxxapi::http::request_t::client_info_t(
                        remote_endpoint.address().to_string(),

                        remote_endpoint.port()
                    );

                    req.client() = std::move(client_info);
                }

                if (is_websocket) {
                    break;
                }
                else {
                    co_await handle_request(std::move(req));

                    if (m_close)
                        break;
                }
            }
#ifdef CXXAPI_USE_LOGGING_IMPL
            catch (const exceptions::client_exception_t& e) {
                g_logging->log(
                    e_log_level::error,

                    "[Server-Client] Exception while handling client (id: {}): {}",

                    m_socket.native_handle(),

                    e.message()
                );

                catch_tuple = std::make_tuple(true, e.status());
            }
            catch (const boost::system::system_error& e) {
                g_logging->log(
                    e_log_level::error,

                    "[Server-Client] Exception while handling client (id: {}): code={}, category={}, message={}",

                    m_socket.native_handle(), e.code().value(), e.code().category().name(), e.what()
                );

                catch_tuple = std::make_tuple(true, 500u);
            }
            catch (const std::exception& e) {
                g_logging->log(
                    e_log_level::error,

                    "[Server-Client] Exception while handling client (id: {}): {}",

                    m_socket.native_handle(),

                    e.what()
                );

                catch_tuple = std::make_tuple(true, 500u);
            }
            catch (...) {
                g_logging->log(
                    e_log_level::error,

                    "[Server-Client] Exception while handling client (id: {}): unknown",

                    m_socket.native_handle()
                );

                catch_tuple = std::make_tuple(true, 500u);
            }
#else
            catch (const exceptions::client_exception_t& e) {
                std::cerr << fmt::format(
                    "[Server-Client] Exception while handling client (id: {}): {}",

                    m_socket.native_handle(),

                    e.message()
                ) << "\n";

                catch_tuple = std::make_tuple(true, e.status());
            }
            catch (const boost::system::system_error& e) {
                std::cerr << fmt::format(
                    "[Server-Client] Exception while handling client (id: {}): code={}, category={}, message={}",

                    m_socket.native_handle(), e.code().value(), e.code().category().name(), e.what()
                ) << "\n";

                catch_tuple = std::make_tuple(true, 500u);
            }
            catch (const std::exception& e) {
                std::cerr << fmt::format(
                    "[Server-Client] Exception while handling client (id: {}): {}",

                    m_socket.native_handle(),

                    e.what()
                ) << "\n";

                catch_tuple = std::make_tuple(true, 500u);
            }
            catch (...) {
                std::cerr << fmt::format(
                    "[Server-Client] Exception while handling client (id: {}): unknown",

                    m_socket.native_handle()
                ) << "\n";

                catch_tuple = std::make_tuple(true, 500u);
            }
#endif // CXXAPI_USE_LOGGING_IMPL

            if (std::get<0u>(catch_tuple)) {
                boost::beast::http::response<boost::beast::http::string_body> response{};

                {
                    response.version(m_parsed_request.version());

                    switch (std::get<1u>(catch_tuple)) {
                        case 400u:
                            {
                                response.result(boost::beast::http::status::bad_request);

                                if (m_cxxapi.cfg().m_http.m_response_class == http::e_response_class::plain) {
                                    response.body() = "Bad request";
                                }
                                else {
                                    response.body() = http::json_t::serialize(
                                        http::json_t::json_obj_t{
                                            {"message", "Bad request"}
                                        }
                                    );
                                }

                                break;
                            }

                        case 500u:
                            {
                                response.result(boost::beast::http::status::internal_server_error);

                                if (m_cxxapi.cfg().m_http.m_response_class == http::e_response_class::plain) {
                                    response.body() = "Internal server error";
                                }
                                else {
                                    response.body() = http::json_t::serialize(
                                        http::json_t::json_obj_t{
                                            {"message", "Internal server error"}
                                        }
                                    );
                                }

                                break;
                            }

                        default:
                            {
                                response.result(boost::beast::http::status::internal_server_error);

                                if (m_cxxapi.cfg().m_http.m_response_class == http::e_response_class::plain) {
                                    response.body() = "Internal server error";
                                }
                                else {
                                    response.body() = http::json_t::serialize(
                                        http::json_t::json_obj_t{
                                            {"message", "Internal server error"}
                                        }
                                    );
                                }

                                break;
                            }
                    }

                    response.prepare_payload();
                }

                co_await boost::beast::http::async_write(m_socket, response, boost::asio::use_awaitable);
            }
        }

        co_return;
    }

    boost::asio::awaitable<void> client_t::handle_request(http::request_t&& req) {
        std::tuple<bool, std::size_t> catch_tuple{};

        try {
            auto keep_alive = req.keep_alive();

            auto response_data = co_await m_cxxapi._handle_request(std::move(req));

            if (response_data.m_stream) {
                boost::beast::http::response<boost::beast::http::empty_body> response;

                {
                    response.version(m_parsed_request.version());

                    {
                        for (auto&& [key, value] : response_data.m_headers)
                            response.insert(key, std::move(value));

                        for (auto&& cookie : response_data.m_cookies)
                            response.insert("Set-Cookie", std::move(cookie));
                    }

                    response.chunked(true);

                    response.result(static_cast<std::int32_t>(response_data.m_status));

                    if (keep_alive) {
                        response.keep_alive(true);

                        response.set(boost::beast::http::field::connection, "keep-alive");

                        response.set(boost::beast::http::field::keep_alive, fmt::format("timeout={}", m_cxxapi.cfg().m_http.m_keep_alive_timeout));
                    }
                    else {
                        response.keep_alive(false);

                        response.set(boost::beast::http::field::connection, "close");

                        m_close = true;
                    }
                }

                {
                    boost::beast::http::serializer<false, boost::beast::http::empty_body> sr{response};

                    co_await boost::beast::http::async_write_header(m_socket, sr, boost::asio::use_awaitable);
                }

                {
                    {
                        co_await response_data.m_callback(m_socket);
                    }

                    co_await boost::asio::async_write(m_socket, boost::asio::buffer("0\r\n\r\n"), boost::asio::use_awaitable);
                }

                if (m_close) {
                    boost::system::error_code ignored_error_code{};

                    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ignored_error_code);
                }

                co_return;
            }

            boost::beast::http::response<boost::beast::http::string_body> response{};

            response.version(m_parsed_request.version());

            {
                for (auto&& [key, value] : response_data.m_headers)
                    response.insert(key, std::move(value));

                for (auto&& cookie : response_data.m_cookies)
                    response.insert("Set-Cookie", std::move(cookie));
            }

            response.body() = std::move(response_data.m_body);

            response.result(static_cast<std::int32_t>(response_data.m_status));

            if (keep_alive) {
                response.keep_alive(true);

                response.set(boost::beast::http::field::connection, "keep-alive");

                response.set(boost::beast::http::field::keep_alive, fmt::format("timeout={}", m_cxxapi.cfg().m_http.m_keep_alive_timeout));
            }
            else {
                response.keep_alive(false);

                response.set(boost::beast::http::field::connection, "close");

                m_close = true;
            }

            response.prepare_payload();

            co_await boost::beast::http::async_write(m_socket, response, boost::asio::use_awaitable);

            if (m_close) {
                boost::system::error_code ignored_error_code{};

                m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ignored_error_code);
            }
        }
        catch (const boost::system::system_error& e) {
#ifdef CXXAPI_USE_LOGGING_IMPL
            g_logging->log(
                e_log_level::error,

                "[Server-Client] Exception while handling client request (id: {}): code={}, category={}, message={}",

                m_socket.native_handle(), e.code().value(), e.code().category().name(), e.what()
            );
#else
            std::cerr << fmt::format(
                "[Server-Client] Exception while handling client request (id: {}): code={}, category={}, message={}",

                m_socket.native_handle(), e.code().value(), e.code().category().name(), e.what()
            ) << "\n";
#endif // CXXAPI_USE_LOGGING_IMPL

            catch_tuple = std::make_tuple(true, 500u);
        }

        if (std::get<0u>(catch_tuple)) {
            boost::beast::http::response<boost::beast::http::string_body> response{};

            {
                response.version(m_parsed_request.version());

                switch (std::get<1u>(catch_tuple)) {
                    case 500u:
                        {
                            response.result(boost::beast::http::status::internal_server_error);
                            response.body() = "Internal server error";

                            break;
                        }

                    default:
                        {
                            response.result(boost::beast::http::status::internal_server_error);
                            response.body() = "Internal server error";

                            break;
                        }
                }

                response.prepare_payload();
            }

            co_await boost::beast::http::async_write(m_socket, response, boost::asio::use_awaitable);

            m_close = true;
        }

        co_return;
    }
}