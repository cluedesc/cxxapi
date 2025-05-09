/**
 * @file internal.hxx
 * @brief Internal implementation details for route handling.
 */

#ifndef CXXAPI_ROUTE_INTERNAL_HXX
#define CXXAPI_ROUTE_INTERNAL_HXX

namespace cxxapi::route::internal {
    /**
     * @brief Trie node for efficient route path matching.
     * @tparam _type_t Type of the handler stored in the node.
     */
    template <typename _type_t>
    struct trie_node_t {
        /**
         * @brief Construct a new trie node with pre-allocated capacity.
         */
        CXXAPI_INLINE trie_node_t() {
            m_values.reserve(8u);
            m_child.reserve(16u);
        }

      public:
        /**
         * @brief Insert a new route handler into the trie.
         * @param method HTTP method to handle.
         * @param path URL path pattern to match.
         * @param handler Handler function to store.
         * @return true if inserted successfully, false if already exists.
         */
        bool insert(const http::e_method& method, const http::path_t& path, _type_t handler);

        /**
         * @brief Find a matching route handler in the trie.
         * @param method HTTP method to match.
         * @param path URL path to match.
         * @return Optional containing handler and parameters if found.
         */
        std::optional<std::pair<_type_t, http::params_t>> find(const http::e_method& method, const http::path_t& path);

      private:
        /**
         * @brief Normalize a URL path by ensuring proper formatting.
         * @param path Path to normalize.
         * @return Normalized path string.
         */
        CXXAPI_INLINE constexpr http::path_t normalize_path(const http::path_t& path) const {
            if (path.empty())
                return "/";

            return (path.size() > 1u && path.back() == '/') ? path.substr(0, path.size() - 1u) : path;
        }

        /**
         * @brief Check if a path segment is dynamic (contains parameters).
         * @param segment Path segment to check.
         * @return true if dynamic segment, false otherwise.
         */
        CXXAPI_INLINE constexpr bool is_dynamic_segment(const std::string_view& segment) const {
            return segment.size() >= 2u && segment.front() == '{' && segment.back() == '}';
        }

        /**
         * @brief Check if a path segment is broken (missing opening or closing brace).
         * @param segment Path segment to check.
         * @return true if broken segment, false otherwise.
         */
        CXXAPI_INLINE constexpr bool is_broken_segment(const std::string_view& segment) const {
            return (segment.front() == '{' && segment.back() != '}') || (segment.front() != '{' && segment.back() == '}');
        }

        /**
         * @brief Extract parameter name from a dynamic segment.
         * @param segment Path segment to process.
         * @return Extracted parameter name.
         */
        CXXAPI_INLINE constexpr std::string extract_param_name(const std::string_view& segment) const {
            return segment.size() > 2u ? std::string{segment.substr(1u, segment.size() - 2u)} : "";
        }

        /**
         * @brief Splits a path into segments.
         * @param path The path to split.
         * @return Vector of path segments.
         */
        std::vector<http::path_t> split_path(const http::path_t& path) const;

      private:
        /** @brief Map of HTTP methods to their handlers. */
        boost::unordered_map<http::e_method, _type_t> m_values{};

        /** @brief Child nodes for static path segments. */
        boost::unordered_map<std::string, std::shared_ptr<trie_node_t>> m_child{};

        /** @brief Parameter name for dynamic segments. */
        std::string m_param{};

        /** @brief Child node for dynamic path segments. */
        std::shared_ptr<trie_node_t> m_dynamic_child{};
    };

    /**
     * @brief Traits for extracting value type from awaitable.
     * @tparam _type_t Type to analyze.
     */
    template <typename>
    struct awaitable_traits_t;

    /**
     * @brief Specialization for boost::asio::awaitable.
     * @tparam _type_t Value type of the awaitable.
     * @tparam _executor Executor type.
     */
    template <typename _type_t, typename _executor>
    struct awaitable_traits_t<boost::asio::awaitable<_type_t, _executor>> {
        using value_type_t = _type_t;
    };

    /**
     * @brief Check if a type is an awaitable HTTP response.
     * @tparam _assigned_t Type to check.
     */
    template <typename _assigned_t>
    struct is_awaitable_response_t {
        static constexpr bool value = false;
    };

    /**
     * @brief Specialization for awaitable HTTP responses.
     * @tparam _executor Executor type.
     */
    template <typename _executor>
    struct is_awaitable_response_t<boost::asio::awaitable<http::response_t, _executor>> {
        static constexpr bool value = true;
    };

    /**
     * @brief Concept for async handler functions.
     * @tparam _fn_t Type to check.
     */
    template <typename _fn_t>
    concept async_handler_c = requires(_fn_t fn, http::http_ctx_t ctx) {
        { fn(std::move(ctx)) } -> std::same_as<boost::asio::awaitable<http::response_t>>;
    } || is_awaitable_response_t<std::invoke_result_t<_fn_t, http::http_ctx_t>>::value;

    /**
     * @brief Concept for sync handler functions.
     * @tparam _fn_t Type to check.
     */
    template <typename _fn_t>
    concept sync_handler_c = requires(_fn_t fn, http::http_ctx_t ctx) {
        { fn(std::move(ctx)) } -> std::same_as<http::response_t>;
    };
}

#endif // CXXAPI_ROUTE_INTERNAL_HXX
