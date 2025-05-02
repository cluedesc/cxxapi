/**
 * @file file.hxx
 * @brief File abstraction for HTTP file uploads, supporting in-memory and temporary file storage.
 */

#ifndef CXXAPI_HTTP_UTILS_FILE_HXX
#define CXXAPI_HTTP_UTILS_FILE_HXX

namespace cxxapi::http {
    /**
     * @brief Represents an uploaded file, either in memory or as a temporary file.
     */
    struct file_t {
        /**
         * @brief Default constructor.
         */
        CXXAPI_INLINE file_t() = default;

        /**
         * @brief Construct an in-memory file.
         * @param name File name.
         * @param content_type MIME type of the file.
         * @param data File data as a vector of bytes.
         */
        CXXAPI_INLINE file_t(std::string&& name, std::string&& content_type, std::vector<char>&& data)
            : m_name(std::move(name)), m_content_type(std::move(content_type)), m_data(std::move(data)), m_in_memory(true) {
        }

        /**
         * @brief Construct a file backed by a temporary file on disk.
         * @param name File name.
         * @param content_type MIME type of the file.
         * @param temp_path Path to the temporary file.
         */
        CXXAPI_INLINE file_t(std::string&& name, std::string&& content_type, boost::filesystem::path&& temp_path)
            : m_name(std::move(name)), m_content_type(std::move(content_type)), m_temp_path(std::move(temp_path)), m_in_memory(false) {
        }

        /**
         * @brief Destructor. Removes the temporary file if present.
         */
        CXXAPI_INLINE ~file_t() {
            if (!m_in_memory && !m_temp_path.empty()) {
                boost::system::error_code error_code{};

                boost::filesystem::remove(m_temp_path, error_code);
            }
        }

      public:
        /**
         * @brief Deleted copy constructor.
         */
        CXXAPI_INLINE file_t(const file_t&) = delete;

        /**
         * @brief Deleted copy assignment operator.
         */
        CXXAPI_INLINE file_t& operator=(const file_t&) = delete;

      public:
        /**
         * @brief Move constructor.
         * @param other File to move from.
         */
        CXXAPI_INLINE file_t(file_t&& other)
            : m_name(std::move(other.m_name)),
              m_content_type(std::move(other.m_content_type)),
              m_data(std::move(other.m_data)),
              m_temp_path(std::move(other.m_temp_path)),
              m_in_memory(other.m_in_memory) {
            other.m_temp_path.clear();
            other.m_in_memory = true;
        }

        /**
         * @brief Move assignment operator.
         * @param other File to move from.
         * @return Reference to this file.
         */
        CXXAPI_INLINE file_t& operator=(file_t&& other) {
            if (this != &other) {
                if (!m_in_memory && !m_temp_path.empty()) {
                    boost::system::error_code error_code{};

                    boost::filesystem::remove(m_temp_path, error_code);
                }

                m_name = std::move(other.m_name);
                m_content_type = std::move(other.m_content_type);

                m_data = std::move(other.m_data);
                m_temp_path = std::move(other.m_temp_path);

                m_in_memory = other.m_in_memory;

                other.m_temp_path.clear();
                other.m_in_memory = true;
            }

            return *this;
        }

      public:
        /**
         * @brief Get the size of the file in bytes.
         * @return File size in bytes.
         */
        CXXAPI_INLINE std::size_t size() const {
            if (m_in_memory)
                return m_data.size();

            if (!m_temp_path.empty() && boost::filesystem::exists(m_temp_path)) {
                boost::system::error_code error_code{};

                const auto sz = boost::filesystem::file_size(m_temp_path, error_code);

                return error_code ? 0u : sz;
            }

            return 0u;
        }

      public:
        /**
         * @brief Get the file name.
         * @return Reference to the file name string.
         */
        CXXAPI_INLINE const auto& name() const { return m_name; }

        /**
         * @brief Get the MIME type of the file.
         * @return Reference to the content type string.
         */
        CXXAPI_INLINE const auto& content_type() const { return m_content_type; }

        /**
         * @brief Get the file data (for in-memory files).
         * @return Reference to the data vector.
         */
        CXXAPI_INLINE const auto& data() const { return m_data; }

        /**
         * @brief Get the path to the temporary file (if not in memory).
         * @return Reference to the temporary file path.
         */
        CXXAPI_INLINE const auto& temp_path() const { return m_temp_path; }

        /**
         * @brief Check if the file is stored in memory.
         * @return True if in memory, false if backed by a temp file.
         */
        CXXAPI_INLINE const auto& in_memory() const { return m_in_memory; }

      private:
        /** @brief File name. */
        std::string m_name{};
        
        /** @brief MIME type of the file. */
        std::string m_content_type{};

        /** @brief File data (for in-memory files). */
        std::vector<char> m_data{};

        /** @brief Path to the temporary file (if not in memory). */
        boost::filesystem::path m_temp_path{};

        /** @brief True if the file is stored in memory, false if backed by a temp file. */
        bool m_in_memory{};
    };

    /** @brief Type alias for a map of files. */
    using files_t = boost::unordered_map<std::string, file_t>;
}

#endif // CXXAPI_HTTP_UTILS_FILE_HXX
