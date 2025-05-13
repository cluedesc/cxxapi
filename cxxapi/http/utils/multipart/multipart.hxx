/**
 * @file multipart.hxx
 * @brief Multipart form data parser for HTTP file uploads, supporting both in-memory and temporary file storage.
 */

#ifndef CXXAPI_HTTP_UTILS_MULTIPART_HXX
#define CXXAPI_HTTP_UTILS_MULTIPART_HXX

namespace cxxapi::http {
    /**
     * @brief Handles multipart file uploads, parsing the body of the request asynchronously.
     */
    struct multipart_t {
        /**
         * @brief Parse the multipart form data asynchronously.
         * @param executor The executor for asynchronous tasks.
         * @param body The body of the multipart form data.
         * @param boundary The boundary separating the different parts of the form data.
         * @param chunk_size_disk The size of each chunk for disk storage.
         * @param max_size_file_in_memory The maximum size of a file to be stored in memory.
         * @param max_size_files_in_memory The maximum size of all files to be stored in memory.
         * @return A map of file names to uploaded file data.
         */
        CXXAPI_NOINLINE static boost::asio::awaitable<files_t> parse_async(
            const boost::asio::any_io_executor& executor,

            const std::string_view body,
            const std::string_view boundary,

            const std::size_t chunk_size_disk = 65536u,

            const std::size_t max_size_file_in_memory = 1048576u,
            const std::size_t max_size_files_in_memory = 10485760u
        ) {
            files_t ret{};

            const std::string dash_boundary = "--" + std::string(boundary);

            if (body.find(dash_boundary) == std::string::npos)
                co_return ret;

            const std::string part_end_marker = "\r\n";

            const auto step_after = part_end_marker.size();

            std::size_t in_memory_total{};

            std::size_t pos{};

            bool saw_closing_boundary{};

            while ((pos = body.find(dash_boundary, pos)) != std::string::npos) {
                pos += dash_boundary.size();

                if (body.compare(pos, 2u, "--") == 0u) {
                    saw_closing_boundary = true;

                    break;
                }

                if (body.compare(pos, 2u, "\r\n") == 0u)
                    pos += 2u;

                const std::size_t header_end = body.find("\r\n\r\n", pos);

                if (header_end == std::string::npos)
                    break;

                const std::string_view headers = body.substr(pos, header_end - pos);

                pos = header_end + 4u;

                std::string name{}, filename{}, ctype{};

                for (auto line : _split(headers, "\r\n")) {
                    if (boost::ifind_first(line, "content-disposition")) {
                        name = std::string(_extract_between(line, "name=\"", "\""));

                        filename = std::string(_extract_between(line, "filename=\"", "\""));
                    }
                    else if (boost::ifind_first(line, "content-type"))
                        ctype = std::string(line.substr(line.find(":") + 2u));
                }

                const auto part_end = body.find(part_end_marker, pos);

                if (part_end == std::string::npos)
                    break;

                const auto content_len = part_end - pos;

                if (!name.empty()
                    && !filename.empty()) {
                    if (content_len <= max_size_file_in_memory
                        && in_memory_total + content_len <= max_size_files_in_memory) {
                        std::vector<char> data(content_len);

                        std::copy(body.data() + pos, body.data() + part_end, data.begin());

                        in_memory_total += content_len;

                        if (content_len > 65536u)
                            co_await boost::asio::post(executor, boost::asio::use_awaitable);

                        ret.emplace(std::move(name), file_t(std::move(filename), std::move(ctype), std::move(data)));
                    }
                    else {
                        boost::filesystem::path tmp = boost::filesystem::temp_directory_path()
                                                    / boost::filesystem::unique_path("cxxapi_tmp-%%%%-%%%%");

                        try {
                            std::ofstream ofs(tmp.string(), std::ios::binary);

                            for (std::size_t w{}; w < content_len; w += chunk_size_disk) {
                                const auto chunk = std::min(chunk_size_disk, content_len - w);

                                ofs.write(body.data() + pos + w, chunk);

                                co_await boost::asio::post(executor, boost::asio::use_awaitable);
                            }

                            ofs.close();

                            ret.emplace(std::move(name), file_t(std::move(filename), std::move(ctype), std::move(tmp)));
                        }
                        catch (const std::exception& e) {
                            throw exceptions::processing_exception_t(fmt::format("Can't write temp file: {}", e.what()));
                        }
                    }
                }

                pos = part_end + step_after;
            }

            if (!saw_closing_boundary) {
                ret.clear();

                co_return ret;
            }

            co_return ret;
        }

        /**
         * @brief Parse multipart/form-data from file.
         * @param executor The executor.
         * @param path The path to the file.
         * @param boundary The boundary.
         * @param chunk_size The chunk size.
         * @param chunk_size_disk The chunk size for disk.
         * @param max_size_file_in_memory The maximum size of file in memory.
         * @param max_size_files_in_memory The maximum size of files in memory.
         */
        CXXAPI_NOINLINE static boost::asio::awaitable<files_t> parse_from_file_async(
            const boost::asio::any_io_executor& executor,

            const boost::filesystem::path& path,

            const std::string_view boundary,

            const std::size_t chunk_size = 16384u,
            const std::size_t chunk_size_disk = 65536u,

            const std::size_t max_size_file_in_memory = 1048576u,
            const std::size_t max_size_files_in_memory = 10485760u
        ) {
            files_t ret{};

            boost::system::error_code ec{};

            boost::asio::stream_file file(executor);

            file.open(path.string(), boost::asio::stream_file::read_only, ec);

            if (ec)
                throw exceptions::processing_exception_t(fmt::format("Can't open input file: {}", ec.message()));

            if (boundary.empty())
                throw exceptions::processing_exception_t("Empty boundary is not allowed");

            if (!boundary.empty()
                && std::isspace(boundary.back()))
                throw exceptions::processing_exception_t("Boundary can't end with whitespace");

            std::string dash_boundary = "--" + std::string(boundary);
            std::string dash_boundary_end = dash_boundary + "--";

            std::string full_boundary = "\r\n" + dash_boundary;
            std::string full_boundary_end = "\r\n" + dash_boundary_end;

            std::vector<char> buffer(chunk_size_disk);

            try {
                std::string accumulated_data;

                std::string line;

                std::vector<char> line_buffer;

                {
                    auto boundary_found = false;

                    do {
                        line = co_await async_read_line(file, line_buffer, chunk_size);

                        std::string normalized_line = line;

                        if (!normalized_line.empty()
                            && normalized_line.back() == '\n')
                            normalized_line.pop_back();

                        if (!normalized_line.empty()
                            && normalized_line.back() == '\r')
                            normalized_line.pop_back();

                        boundary_found = (normalized_line == dash_boundary);
                    } while (!line.empty() && !boundary_found);

                    if (!boundary_found)
                        throw exceptions::processing_exception_t("Invalid format, initial boundary not found");
                }

                while (true) {
                    std::string headers_blob;

                    auto headers_end_found = false;

                    while (!headers_end_found) {
                        line = co_await async_read_line(file, line_buffer, chunk_size);

                        std::string normalized_line = line;
                        if (!normalized_line.empty()
                            && normalized_line.back() == '\n')
                            normalized_line.pop_back();

                        if (!normalized_line.empty()
                            && normalized_line.back() == '\r')
                            normalized_line.pop_back();

                        if (normalized_line.empty()) {
                            headers_end_found = true;

                            break;
                        }

                        headers_blob += normalized_line + "\r\n";
                    }

                    if (!headers_end_found)
                        throw exceptions::processing_exception_t("Headers section is not properly terminated");

                    std::string name, filename, content_type;

                    co_await parse_part_headers(executor, headers_blob, name, filename, content_type);

                    if (name.empty())
                        throw exceptions::processing_exception_t("Missing name parameter in Content-Disposition header");

                    auto in_memory = filename.empty();

                    boost::filesystem::path tmp_path{};

                    boost::asio::stream_file tmp_file(executor);

                    if (!in_memory) {
                        tmp_path = boost::filesystem::temp_directory_path()
                                 / boost::filesystem::unique_path("cxxapi_tmp-%%%%-%%%%");

                        tmp_file.open(tmp_path.string(), boost::asio::stream_file::write_only | boost::asio::stream_file::create, ec);

                        if (ec)
                            throw exceptions::processing_exception_t(fmt::format("Can't create temp file: {}", ec.message()));
                    }

                    std::vector<char> data{};

                    if (in_memory)
                        data.reserve(chunk_size);

                    std::size_t part_bytes{};

                    auto boundary_found = false;
                    auto is_final_boundary = false;

                    std::vector<char> search_buffer{};

                    const size_t max_boundary_size = std::max(full_boundary.size(), full_boundary_end.size()) * 2u;

                    search_buffer.reserve(max_boundary_size);

                    while (!boundary_found) {
                        boost::system::error_code read_ec{};

                        auto bytes_read = co_await file.async_read_some(
                            boost::asio::buffer(buffer),

                            boost::asio::redirect_error(boost::asio::use_awaitable, read_ec)
                        );

                        if (read_ec && read_ec != boost::asio::error::eof)
                            throw boost::system::system_error(read_ec);

                        if (bytes_read == 0u)
                            break;

                        search_buffer.insert(search_buffer.end(), buffer.begin(), buffer.begin() + bytes_read);

                        auto find_boundary = [&](const std::string& boundary_str) -> size_t {
                            const auto& it = std::search(
                                search_buffer.begin(), search_buffer.end(),

                                boundary_str.begin(), boundary_str.end()
                            );

                            if (it != search_buffer.end())
                                return std::distance(search_buffer.begin(), it);

                            return std::string::npos;
                        };

                        auto normal_pos = find_boundary(full_boundary);
                        auto end_pos = find_boundary(full_boundary_end);

                        if (normal_pos != std::string::npos
                            || end_pos != std::string::npos) {
                            std::size_t boundary_pos{};

                            if (normal_pos != std::string::npos
                                && end_pos != std::string::npos) {
                                boundary_pos = std::min(normal_pos, end_pos);

                                is_final_boundary = (boundary_pos == end_pos);
                            }
                            else if (normal_pos != std::string::npos) {
                                boundary_pos = normal_pos;

                                is_final_boundary = false;
                            }
                            else {
                                boundary_pos = end_pos;

                                is_final_boundary = true;
                            }

                            if (boundary_pos > 0u) {
                                auto content_bytes = boundary_pos;

                                if (in_memory
                                    && part_bytes + content_bytes > max_size_file_in_memory) {
                                    tmp_path = boost::filesystem::temp_directory_path()
                                             / boost::filesystem::unique_path("cxxapi_tmp-%%%%-%%%%");

                                    tmp_file.open(tmp_path.string(), boost::asio::stream_file::write_only | boost::asio::stream_file::create, ec);

                                    if (ec)
                                        throw exceptions::processing_exception_t(fmt::format("Can't create temp file: {}", ec.message()));

                                    if (!data.empty()) {
                                        co_await boost::asio::async_write(
                                            tmp_file,

                                            boost::asio::buffer(data),

                                            boost::asio::use_awaitable
                                        );

                                        data.clear();
                                    }

                                    in_memory = false;
                                }

                                if (in_memory) {
                                    data.insert(data.end(), search_buffer.begin(), search_buffer.begin() + content_bytes);
                                }
                                else {
                                    co_await boost::asio::async_write(
                                        tmp_file,

                                        boost::asio::buffer(search_buffer.data(), content_bytes),

                                        boost::asio::use_awaitable
                                    );
                                }

                                part_bytes += content_bytes;
                            }

                            auto boundary_length = is_final_boundary ? full_boundary_end.size() : full_boundary.size();

                            auto rewind_amount = search_buffer.size() - (boundary_pos + boundary_length);

                            if (rewind_amount > 0) {
                                file.seek(-static_cast<int64_t>(rewind_amount), boost::asio::file_base::seek_cur, ec);

                                if (ec)
                                    throw exceptions::processing_exception_t(fmt::format("Error seeking in file: {}", ec.message()));
                            }

                            boundary_found = true;
                        }
                        else if (search_buffer.size() > max_boundary_size) {
                            auto keep_size = max_boundary_size;

                            auto write_size = search_buffer.size() - keep_size;

                            if (in_memory
                                && part_bytes + write_size > max_size_file_in_memory) {
                                tmp_path = boost::filesystem::temp_directory_path()
                                         / boost::filesystem::unique_path("cxxapi_tmp-%%%%-%%%%");

                                tmp_file.open(tmp_path.string(), boost::asio::stream_file::write_only | boost::asio::stream_file::create, ec);

                                if (ec)
                                    throw exceptions::processing_exception_t(fmt::format("Can't create temp file: {}", ec.message()));

                                if (!data.empty()) {
                                    co_await boost::asio::async_write(
                                        tmp_file,

                                        boost::asio::buffer(data),

                                        boost::asio::use_awaitable
                                    );

                                    data.clear();
                                }

                                in_memory = false;
                            }

                            if (in_memory) {
                                data.insert(data.end(), search_buffer.begin(), search_buffer.begin() + write_size);
                            }
                            else {
                                co_await boost::asio::async_write(
                                    tmp_file,

                                    boost::asio::buffer(search_buffer.data(), write_size),

                                    boost::asio::use_awaitable
                                );
                            }

                            part_bytes += write_size;

                            std::vector<char> temp(search_buffer.end() - keep_size, search_buffer.end());

                            search_buffer = std::move(temp);
                        }

                        if (read_ec == boost::asio::error::eof) {
                            if (!boundary_found) {
                                if (!search_buffer.empty()) {
                                    if (in_memory) {
                                        data.insert(data.end(), search_buffer.begin(), search_buffer.end());
                                    }
                                    else {
                                        co_await boost::asio::async_write(
                                            tmp_file,

                                            boost::asio::buffer(search_buffer),

                                            boost::asio::use_awaitable
                                        );
                                    }

                                    part_bytes += search_buffer.size();
                                }
                            }

                            break;
                        }
                    }

                    if (tmp_file.is_open()) {
                        tmp_file.close(ec);

                        if (ec)
                            throw exceptions::processing_exception_t(fmt::format("Error closing temp file: {}", ec.message()));
                    }

                    if (in_memory) {
                        ret.emplace(name, file_t{std::move(name), std::move(content_type), std::move(data)});
                    }
                    else
                        ret.emplace(name, file_t{std::move(filename), std::move(content_type), std::move(tmp_path)});

                    if (is_final_boundary)
                        break;
                }
            }
            catch (const std::exception& e) {
                if (file.is_open())
                    file.close(ec);

                throw exceptions::processing_exception_t(fmt::format("Error reading file: {}", e.what()));
            }

            if (file.is_open()) {
                file.close(ec);

                if (ec)
                    throw exceptions::processing_exception_t(fmt::format("Error closing input file: {}", ec.message()));
            }

            co_return ret;
        }

        /**
         * @brief Reads a line from a stream.
         * @tparam _type_t The type of the stream.
         * @param stream The stream to read from.
         * @param buffer The buffer to read into.
         * @param chunk_size The chunk size to read.
         * @return The line read.
         */
        template <typename _type_t>
        CXXAPI_NOINLINE static boost::asio::awaitable<std::string> async_read_line(_type_t& stream, std::vector<char>& buffer, const std::size_t chunk_size) {
            std::string line{};

            char c{};

            boost::system::error_code error_code{};

            buffer.clear();

            size_t bytes_read_total{};

            while (bytes_read_total < chunk_size) {
                auto bytes_read = co_await stream.async_read_some(
                    boost::asio::buffer(&c, 1u),

                    boost::asio::redirect_error(boost::asio::use_awaitable, error_code)
                );

                if (error_code) {
                    if (error_code == boost::asio::error::eof)
                        break;

                    throw boost::system::system_error(error_code);
                }

                if (bytes_read == 0u)
                    break;

                bytes_read_total++;

                buffer.push_back(c);

                if (c == '\n') {
                    line.assign(buffer.begin(), buffer.end());

                    buffer.clear();

                    co_return line;
                }
            }

            if (!buffer.empty()) {
                line.assign(buffer.begin(), buffer.end());

                buffer.clear();

                co_return line;
            }

            co_return "";
        }

        /**
         * @brief Parses a headers blob.
         * @param executor The executor.
         * @param headers_blob The headers blob.
         * @param name The name of the part.
         * @param filename The filename of the part.
         * @param ctype The content type of the part.
         */
        CXXAPI_NOINLINE static boost::asio::awaitable<void> parse_part_headers(
            const boost::asio::any_io_executor& executor,

            const std::string_view headers_blob,

            std::string& name,
            std::string& filename,
            std::string& ctype
        ) {
            for (auto line_sv : _split(headers_blob, "\r\n")) {
                std::string line(line_sv);

                if (boost::ifind_first(line, "content-disposition")) {
                    name = std::string(_extract_between(line, "name=\"", "\""));

                    filename = std::string(_extract_between(line, "filename=\"", "\""));
                }
                else if (boost::ifind_first(line, "content-type")) {
                    auto pos = line.find(':');

                    if (pos != std::string::npos) {
                        ctype = line.substr(pos + 1);

                        boost::algorithm::trim(ctype);
                    }
                }
            }

            co_await boost::asio::post(executor, boost::asio::use_awaitable);
        }

      public:
        /**
         * @brief Split a string into substrings by a delimiter.
         * @param str The string to split.
         * @param delimiter The delimiter to split by.
         * @return A vector of substrings.
         */
        CXXAPI_INLINE static std::vector<std::string_view> _split(
            const std::string_view& str,
            
            const std::string_view& delimiter
        ) {
            std::vector<std::string_view> ret{};

            if (str.empty())
                return ret;

            std::size_t start{}, end{};

            while ((end = str.find(delimiter, start)) != std::string_view::npos) {
                ret.push_back(str.substr(start, end - start));

                start = end + delimiter.length();
            }

            ret.push_back(str.substr(start));

            return ret;
        }

        /**
         * @brief Extract a substring between two markers in a string.
         * @param str The string to extract from.
         * @param start The starting marker.
         * @param end The ending marker.
         * @return The extracted substring.
         */
        CXXAPI_INLINE static std::string_view _extract_between(
            const std::string_view& str,

            const std::string_view& start,
            const std::string_view& end
        ) {
            auto first = str.find(start);

            if (first == std::string_view::npos)
                return "";

            first += start.size();

            const auto last = str.find(end, first);

            if (last == std::string_view::npos)
                return "";

            return str.substr(first, last - first);
        }
    };
}

#endif // CXXAPI_HTTP_UTILS_MULTIPART_HXX
