/**
 * @file mimes.hxx
 * @brief MIME type mapping utilities for HTTP file handling.
 */

#ifndef CXXAPI_HTTP_UTILS_MIMES_HXX
#define CXXAPI_HTTP_UTILS_MIMES_HXX

namespace cxxapi::http {
    /**
     * @brief Provides MIME type lookup for file extensions.
     */
    struct mime_types_t {
      private:
        /** @brief Type alias for the MIME map. */
        using mime_map_t = boost::unordered_map<std::string_view, std::string_view>;

        /** @brief MIME map entries. */
        inline static constexpr std::array<std::pair<std::string_view, std::string_view>, 64u> k_mime_entries{
            {{".html", "text/html"},
             {".htm", "text/html"},
             {".css", "text/css"},
             {".js", "application/javascript"},
             {".json", "application/json"},
             {".png", "image/png"},
             {".jpg", "image/jpeg"},
             {".jpeg", "image/jpeg"},
             {".gif", "image/gif"},
             {".svg", "image/svg+xml"},
             {".ico", "image/x-icon"},
             {".pdf", "application/pdf"},
             {".txt", "text/plain"},
             {".xml", "application/xml"},
             {".mp3", "audio/mpeg"},
             {".mp4", "video/mp4"},
             {".webm", "video/webm"},
             {".woff", "font/woff"},
             {".woff2", "font/woff2"},
             {".ttf", "font/ttf"},
             {".otf", "font/otf"},
             {".zip", "application/zip"},
             {".gz", "application/gzip"},
             {".tar", "application/x-tar"},
             {".csv", "text/csv"},
             {".doc", "application/msword"},
             {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
             {".xls", "application/vnd.ms-excel"},
             {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
             {".ppt", "application/vnd.ms-powerpoint"},
             {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
             {".avi", "video/x-msvideo"},
             {".bmp", "image/bmp"},
             {".epub", "application/epub+zip"},
             {".flv", "video/x-flv"},
             {".m4a", "audio/mp4"},
             {".m4v", "video/mp4"},
             {".mkv", "video/x-matroska"},
             {".ogg", "audio/ogg"},
             {".ogv", "video/ogg"},
             {".oga", "audio/ogg"},
             {".opus", "audio/opus"},
             {".wav", "audio/wav"},
             {".webp", "image/webp"},
             {".tiff", "image/tiff"},
             {".tif", "image/tiff"},
             {".md", "text/markdown"},
             {".markdown", "text/markdown"},
             {".yaml", "application/yaml"},
             {".yml", "application/yaml"},
             {".rar", "application/vnd.rar"},
             {".7z", "application/x-7z-compressed"},
             {".apk", "application/vnd.android.package-archive"},
             {".exe", "application/x-msdownload"},
             {".dll", "application/x-msdownload"},
             {".swf", "application/x-shockwave-flash"},
             {".rtf", "application/rtf"},
             {".eot", "application/vnd.ms-fontobject"},
             {".ps", "application/postscript"},
             {".sqlite", "application/x-sqlite3"},
             {".db", "application/x-sqlite3"}}
        };

      public:
        /**
         * @brief Get the static map of file extensions to MIME types.
         * @return Reference to the MIME map.
         */
        CXXAPI_INLINE static const mime_map_t& get_mime_map() {
            static const mime_map_t mime_map = [] {
                mime_map_t ret;

                ret.reserve(k_mime_entries.size());

                for (const auto& [ext, mime] : k_mime_entries) {
                    if (!ext.empty()
                        && !mime.empty())
                        ret.emplace(ext, mime);
                }

                return ret;
            }();

            return mime_map;
        }

        /** @brief Default MIME type for unknown extensions. */
        static constexpr std::string_view k_default_mime_type = "application/octet-stream";

      public:
        /**
         * @brief Get the MIME type for a given file path.
         * @param path File path to check.
         * @return MIME type as a string view.
         */
        CXXAPI_INLINE static std::string_view get(const boost::filesystem::path& path) {
            if (path.empty())
                return k_default_mime_type;

            const auto ext_str = path.extension().string();

            if (ext_str.empty()) 
                return k_default_mime_type;
            
            const auto& mime_map = get_mime_map();

            const auto it = mime_map.find(boost::algorithm::to_lower_copy(ext_str));

            return (it != mime_map.end()) ? it->second : k_default_mime_type;
        }
    };
}

#endif // CXXAPI_HTTP_UTILS_MIMES_HXX
