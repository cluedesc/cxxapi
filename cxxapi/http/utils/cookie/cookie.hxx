/**
 * @file cookie.hxx
 * @brief Represents an HTTP cookie.
 */

#ifndef CXXAPI_HTTP_UTILS_COOKIE_HXX
#define CXXAPI_HTTP_UTILS_COOKIE_HXX

namespace cxxapi::http {
    /**
     * @brief Represents an HTTP cookie.
     */
    struct cookie_t {
        /** @brief Cookie name. */
        std::string_view m_name{};

        /** @brief Cookie value. */
        std::string_view m_value{};

        /** @brief Cookie path. */
        std::string_view m_path{"/"};

        /** @brief Cookie domain. */
        std::string_view m_domain{""};

        /** @brief If true, the cookie is secure. */
        bool m_secure{false};

        /** @brief If true, the cookie is HTTP-only. */
        bool m_http_only{false};

        /** @brief Max-Age attribute in seconds. */
        std::chrono::seconds m_max_age{std::chrono::hours(24)};

        /** @brief SameSite attribute. */
        std::string_view m_same_site{};
    };
}

#endif // CXXAPI_HTTP_UTILS_COOKIE_HXX