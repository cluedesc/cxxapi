#include <cxxapi.hxx>

namespace cxxapi::route::internal {
    template <typename _type_t>
    bool trie_node_t<_type_t>::insert(
        const http::e_method& method,
        const http::path_t& path,

        _type_t handler
    ) {
        try {
            const auto normalized_path = normalize_path(path);
            const auto segments = split_path(normalized_path);

            auto node = this;

            for (const auto& segment : segments) {
                if (segment.empty())
                    throw base_exception_t(fmt::format("Empty segment in path: {}", normalized_path));

                if (node->is_broken_segment(segment))
                    throw base_exception_t(fmt::format("Malformed dynamic segment: {}", segment));

                if (node->is_dynamic_segment(segment)) {
                    auto param_name = node->extract_param_name(segment);

                    if (param_name.empty())
                        throw base_exception_t(fmt::format("Dynamic segment without name: {}", normalized_path));

                    if (!node->m_dynamic_child) {
                        node->m_dynamic_child = std::make_shared<trie_node_t>();

                        node->m_param = param_name;
                    }

                    node = node->m_dynamic_child.get();
                }
                else {
                    auto& child = node->m_child[segment];

                    if (!child)
                        child = std::make_shared<trie_node_t>();

                    node = child.get();
                }
            }

            if (node->m_values.find(method) != node->m_values.end())
                throw base_exception_t(fmt::format("Route already exists for method: {}", normalized_path));

            node->m_values.emplace(method, std::move(handler));

            return true;
        }
        catch (const base_exception_t& e) {
            throw base_exception_t(fmt::format("Error while inserting route: {}", e.what()));
        }
        catch (const std::exception& e) {
            throw base_exception_t(fmt::format("Error while inserting route: {}", e.what()));
        }
        catch (...) {
            throw base_exception_t("Unknown error while inserting route.");
        }

        return false;
    }

    template <typename _type_t>
    std::optional<std::pair<_type_t, http::params_t>> trie_node_t<_type_t>::find(
        const http::e_method& method,
        const http::path_t& path
    ) {
        try {
            const auto normalized_path = normalize_path(path);
            const auto segments = split_path(normalized_path);

            auto node = this;

            http::params_t params{};

            for (const auto& segment : segments) {
                if (segment.empty())
                    throw base_exception_t("Empty segment detected.");

                auto it = node->m_child.find(segment);

                if (it != node->m_child.end()) {
                    node = it->second.get();
                }
                else if (node->m_dynamic_child) {
                    params.emplace(node->m_param, segment);

                    node = node->m_dynamic_child.get();
                }
                else
                    return std::nullopt;
            }

            auto it = node->m_values.find(method);

            if (it != node->m_values.end())
                return std::make_pair(it->second, params);

            return std::nullopt;
        }
        catch (const base_exception_t& e) {
            throw base_exception_t(fmt::format("Error while finding route: {}", e.what()));
        }
        catch (...) {
            throw base_exception_t(fmt::format("Unknown error while finding route."));
        }

        return std::nullopt;
    }

    template <typename _type_t>
    std::vector<http::path_t> trie_node_t<_type_t>::split_path(const http::path_t& path) const {
        std::vector<std::string> segments{};

        if (path == "/") 
            return segments;
        
        segments.reserve(path.size() / 2u);

        auto start = 1u;
        auto end = path.find('/', start);

        while (end != std::string::npos) {
            segments.emplace_back(path.substr(start, end - start));

            start = end + 1u;
            end = path.find('/', start);
        }

        if (start < path.size()) 
            segments.emplace_back(path.substr(start));
        
        return segments;
    }

    /**
     * @brief Explicit template instantiation for trie_node_t 
     *        with std::function<http::response_t(http::http_ctx_t)> as the template parameter.
     */
    template struct trie_node_t<std::function<http::response_t(http::http_ctx_t)>>;

    /**
     * @brief Explicit template instantiation for trie_node_t with std::shared_ptr<route_t> as the template parameter.
     */
    template struct trie_node_t<std::shared_ptr<route_t>>;
}
