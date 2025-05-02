/**
 * @file json_traits.hxx
 * @brief Type traits and utilities for JSON serialization/deserialization using Glaze or nlohmann::json.
 */

#ifndef CXXAPI_SHARED_JSON_TRAITS_HXX
#define CXXAPI_SHARED_JSON_TRAITS_HXX

namespace shared {
    /**
     * @brief Type traits for JSON serialization and deserialization.
     *
     * Specializations are provided for Glaze and nlohmann::json backends.
     */
    struct json_traits_t;

#if defined(CXXAPI_USE_GLAZE_JSON)
    /**
     * @brief JSON traits specialization using Glaze for serialization.
     */
    struct json_traits_t final {
        /** @brief The JSON type used for serialization (std::string). */
        using json_type_t = std::string;

        /** @brief The map type used for deserialization (std::unordered_map<std::string, std::string>). */
        using json_obj_t = glz::json_t;

        /**
         * @brief Serialize a value to a JSON string using Glaze.
         * @tparam _type_t The type of the value to serialize (default is json_obj_t).
         * @param value The value to serialize.
         * @return The serialized JSON string.
         */
        template <typename _type_t = json_obj_t>
        CXXAPI_INLINE static json_type_t serialize(const _type_t& value) {
            json_type_t ret;

            auto err = glz::write_json(value, ret);

            if (err)
                throw std::runtime_error("Can't serialize value to json");

            return ret;
        }

        /**
         * @brief Deserialize a value from a JSON string using Glaze.
         * @tparam _type_t The type of the value to deserialize to (default is json_obj_t).
         * @param json The JSON string to deserialize from.
         * @param value The value to populate.
         */
        template <typename _type_t = json_obj_t>
        CXXAPI_INLINE static _type_t deserialize(const json_type_t& json) {
            auto ret = glz::read_json<_type_t>(json);

            if (!ret)
                throw std::runtime_error("Can't deserialize json to value");

            return *ret;
        }

        /**
         * @brief Get a value from a JSON object using Glaze.
         * @tparam _type_t The type of the value to retrieve.
         * @param obj The JSON object to access.
         * @param key The key to access in the JSON object.
         * @return The value retrieved from the JSON object.
         */
        template <typename _type_t>
        CXXAPI_INLINE static _type_t at(const json_obj_t& obj, const std::string_view& key) { return obj[key].get<_type_t>(); }
    };
#elif defined(CXXAPI_USE_NLOHMANN_JSON)
    /**
     * @brief JSON traits specialization using nlohmann::json for serialization.
     */
    struct json_traits_t final {
        /** @brief The JSON type used for serialization (std::string). */
        using json_type_t = std::string;

        /** @brief The map type used for deserialization (nlohmann::json). */
        using json_obj_t = nlohmann::json;

        /**
         * @brief Serialize a value to a JSON string using nlohmann::json.
         * @tparam _type_t The type of the value to serialize (default is json_obj_t).
         * @param value The value to serialize.
         * @return The serialized JSON string.
         */
        template <typename _type_t = json_obj_t>
        CXXAPI_INLINE static json_type_t serialize(const _type_t& value) {
            try {
                nlohmann::json j = value;

                return j.dump();
            }
            catch (const std::exception& e) {
                throw std::runtime_error(fmt::format("Can't serialize value to json: {}", e.what()));
            }
        }

        /**
         * @brief Deserialize a value from a JSON string using nlohmann::json.
         * @tparam _type_t The type of the value to deserialize to (default is json_obj_t).
         * @param json The JSON string to deserialize from.
         * @param value The value to populate.
         */
        template <typename _type_t = json_obj_t>
        CXXAPI_INLINE static _type_t deserialize(const json_type_t& json) {
            try {
                nlohmann::json j = nlohmann::json::parse(json);

                return j.get<_type_t>();
            }
            catch (const std::exception& e) {
                throw std::runtime_error(fmt::format("Can't deserialize json to value: {}", e.what()));
            }
        }

        /**
         * @brief Get a value from a JSON object using nlohmann::json.
         * @tparam _type_t The type of the value to retrieve.
         * @param obj The JSON object to access.
         * @param key The key to access in the JSON object.
         * @return The value retrieved from the JSON object.
         */
        template <typename _type_t>
        CXXAPI_INLINE static _type_t at(const json_obj_t& obj, const std::string_view& key) { return obj.at(key).get<_type_t>(); }
    };
#endif
}

#endif // CXXAPI_SHARED_JSON_TRAITS_HXX
