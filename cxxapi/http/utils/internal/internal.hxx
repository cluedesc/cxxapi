/**
 * @file internal.hxx
 * @brief Internal utilities for HTTP, including case-insensitive string comparison.
 */

#ifndef CXXAPI_HTTP_UTILS_INTERNAL_HXX
#define CXXAPI_HTTP_UTILS_INTERNAL_HXX

/**
 * @brief Internal namespace for HTTP utilities.
 */
namespace cxxapi::http::internal {
    /**
     * @brief Case-insensitive comparator for string views.
     */
    struct ci_less_t {
        /**
         * @brief Compare two strings case-insensitively.
         * @param lhs Left-hand side string view.
         * @param rhs Right-hand side string view.
         * @return True if lhs is less than rhs (case-insensitive), false otherwise.
         */
        CXXAPI_INLINE bool operator()(const std::string_view& lhs, const std::string_view& rhs) const {
            return boost::algorithm::ilexicographical_compare(lhs, rhs);
        }
    };
}

#endif // CXXAPI_HTTP_UTILS_INTERNAL_HXX
