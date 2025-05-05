#ifdef CXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL

#include <cxxapi.hxx>

namespace cxxapi::middleware::cors {
    c_cors_middleware::c_cors_middleware(cors_options_t options)
        : m_options(std::move(options)) {
        for (const auto& origin : m_options.m_allowed_origins) {
            if (origin.compare("*") == 0) {
                m_options.m_allow_all_origins = true;

                m_origins_set.clear();

                break;
            }

            m_origins_set.insert(origin);
        }

        for (const auto& method : m_options.m_allowed_methods) {
            if (method.compare("*") == 0) {
                m_options.m_allow_all_methods = true;

                break;
            }
        }

        for (const auto& header : m_options.m_allowed_headers) {
            if (header.compare("*") == 0) {
                m_options.m_allow_all_headers = true;

                break;
            }
        }
    }

    boost::asio::awaitable<http::response_t> c_cors_middleware::handle(
        const http::request_t& request,
        
        std::function<boost::asio::awaitable<http::response_t>(const http::request_t&)> next
    ) {
        std::string origin{};

        auto it = request.headers().find("Origin");

        if (it != request.headers().end()) 
            origin = it->second;
        
        if (request.method() == http::e_method::options) {
            http::response_t response{};

            response.m_status = http::e_status::no_content;

            add_cors_headers(response, origin);

            if (m_options.m_allow_all_methods) {
                response.m_headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS, PATCH, HEAD";
            }
            else if (!m_options.m_allowed_methods.empty()) 
                response.m_headers["Access-Control-Allow-Methods"] =
                    fmt::format("{}", fmt::join(m_options.m_allowed_methods, ", "));
        
            std::string requested_headers{};

            auto req_headers_it = request.headers().find("Access-Control-Request-Headers");

            if (req_headers_it != request.headers().end()) 
                requested_headers = req_headers_it->second;
            
            if (m_options.m_allow_all_headers) {
                if (!requested_headers.empty()) {
                    response.m_headers["Access-Control-Allow-Headers"] = requested_headers;
                }
                else 
                    response.m_headers["Access-Control-Allow-Headers"] =
                        "Content-Type, Authorization, X-Requested-With, Accept";              
            }
            else if (!m_options.m_allowed_headers.empty()) 
                response.m_headers["Access-Control-Allow-Headers"] =
                    fmt::format("{}", fmt::join(m_options.m_allowed_headers, ", "));
            
            if (m_options.m_max_age > 0) 
                response.m_headers["Access-Control-Max-Age"] = std::to_string(m_options.m_max_age);
            
            co_return response;
        }

        auto response = co_await next(request);

        add_cors_headers(response, origin);

        co_return response;
    }

    bool c_cors_middleware::is_origin_allowed(const std::string& origin) const {
        if (m_options.m_allow_all_origins) 
            return true;
        
        return m_origins_set.find(origin) != m_origins_set.end();
    }

    void c_cors_middleware::add_cors_headers(http::response_t& response, const std::string& origin) const {
        if (m_options.m_allow_all_origins) {
            response.headers()["Access-Control-Allow-Origin"] = "*";
        }
        else if (!origin.empty() && is_origin_allowed(origin)) {
            response.headers()["Access-Control-Allow-Origin"] = origin;

            if (m_options.m_allow_credentials) 
                response.headers()["Access-Control-Allow-Credentials"] = "true";
        }

        if (!m_options.m_exposed_headers.empty()) 
            response.headers()["Access-Control-Expose-Headers"] =
                fmt::format("{}", fmt::join(m_options.m_exposed_headers, ", "));    
    }
}
#endif // CXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL