/**
 * @file utils.hxx
 * @brief Aggregates HTTP utility headers for internal, MIME, and file handling in the cxxapi.
 */

#ifndef CXXAPI_HTTP_UTILS_HXX
#define CXXAPI_HTTP_UTILS_HXX

#include "internal/internal.hxx"

#include "mimes/mimes.hxx"

#include "file/file.hxx"

#include "multipart/multipart.hxx"

#include "cookie/cookie.hxx"

namespace cxxapi::http::utils {
    /**
     * @brief Streams a request to a tmp file.
     * @param socket Socket to read from.
     * @param buffer Buffer to read into.
     * @param length Length of the request.
     * @param chunk_size Chunk size to read in.
     * @param path Stream file path.
     */
    CXXAPI_NOINLINE static boost::asio::awaitable<void> stream_request(
        boost::asio::ip::tcp::socket& socket,
        boost::beast::flat_buffer& buffer,

        const std::size_t length,
        const std::size_t chunk_size,

        const boost::filesystem::path& path
    ) {
        const auto executor = socket.get_executor();

        boost::asio::stream_file file(executor);

        boost::system::error_code error_code{};

        file.open(path.string(), boost::asio::stream_file::write_only | boost::asio::stream_file::create, error_code);

        if (error_code)
            throw boost::system::system_error(error_code, "Can't open temp file");

        auto remaining = length;

        while (remaining > 0u) {
            if (buffer.size() > 0u) {
                auto sequence = buffer.data();

                auto to_write = std::min(boost::asio::buffer_size(sequence), remaining);

                auto bytes_written = co_await async_write(
                    file,

                    boost::asio::buffer(sequence, to_write),

                    boost::asio::use_awaitable
                );

                if (bytes_written != to_write)
                    throw base_exception_t("Incomplete write to file");

                buffer.consume(bytes_written);

                remaining -= bytes_written;

                continue;
            }

            auto bytes_read = co_await socket.async_read_some(
                buffer.prepare(chunk_size),

                boost::asio::use_awaitable
            );

            if (bytes_read == 0u)
                throw base_exception_t("Connection closed unexpectedly");

            buffer.commit(bytes_read);
        }

        file.close();
    }

    /**
     * @brief Extracts the boundary from a Content-Type header.
     * @param content_type Content-Type header value.
     * @return Extracted boundary string.
     */
    CXXAPI_INLINE std::string _extract_boundary(const std::string_view& content_type) {
        std::string ct{content_type};

        std::vector<std::string> parts{};

        boost::split(parts, ct, boost::is_any_of(";"));

        for (auto& part : parts) {
            boost::trim(part);

            if (boost::istarts_with(part, "boundary=")) {
                auto val = part.substr(std::string("boundary=").size());

                boost::trim(val);

                if (val.size() >= 2u
                    && ((val.front() == '"' && val.back() == '"')
                        || (val.front() == '\'' && val.back() == '\'')))
                    val = val.substr(1u, val.size() - 2u);

                return val;
            }
        }

        return {};
    }

    /**
     * @brief Computes the 32-bit FNV-1a hash for the given string.
     * @param str Input string to hash.
     * @return 32-bit FNV-1a hash value.
     *
     * Implements the Fowler–Noll–Vo hash function variant 1a.
     * Designed for fast, non-cryptographic hashing.
     */
    CXXAPI_INLINE constexpr std::uint32_t fnv1a_hash(const std::string_view str) {
        std::uint32_t hash = 2166136261u;

        for (const auto c : str) {
            hash ^= static_cast<std::uint8_t>(c);
            hash *= 16777619u;
        }

        return hash;
    }
}

#endif // CXXAPI_HTTP_UTILS_HXX