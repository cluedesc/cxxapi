#include <gtest/gtest.h>

#include <cxxapi.hxx>

using namespace cxxapi::http;

TEST(HttpTest, MethodToStrAndStrToMethod) {
    EXPECT_EQ(method_to_str(e_method::get), "GET");
    EXPECT_EQ(method_to_str(e_method::post), "POST");
    EXPECT_EQ(method_to_str(e_method::put), "PUT");
    EXPECT_EQ(method_to_str(e_method::delete_), "DELETE");
    EXPECT_EQ(method_to_str(e_method::patch), "PATCH");
    EXPECT_EQ(method_to_str(e_method::unknown), "UNKNOWN");

    EXPECT_EQ(str_to_method("GET"), e_method::get);
    EXPECT_EQ(str_to_method("POST"), e_method::post);
    EXPECT_EQ(str_to_method("PUT"), e_method::put);
    EXPECT_EQ(str_to_method("DELETE"), e_method::delete_);
    EXPECT_EQ(str_to_method("PATCH"), e_method::patch);
    EXPECT_EQ(str_to_method("OPTIONS"), e_method::options);
    EXPECT_EQ(str_to_method("UNKNOWN"), e_method::unknown);
    EXPECT_EQ(str_to_method("notarealmethod"), e_method::unknown);
}

TEST(HttpTest, CiLessTComparison) {
    internal::ci_less_t cmp;

    EXPECT_TRUE(cmp("abc", "DEF"));

    EXPECT_FALSE(cmp("DEF", "abc"));
    EXPECT_FALSE(cmp("abc", "ABC"));
    EXPECT_FALSE(cmp("same", "same"));
}

TEST(HttpTest, MimeTypesKnownAndUnknown) {
    EXPECT_EQ(mime_types_t::get(boost::filesystem::path("file.html")), "text/html");
    EXPECT_EQ(mime_types_t::get(boost::filesystem::path("file.JPG")), "image/jpeg");
    EXPECT_EQ(mime_types_t::get(boost::filesystem::path("file.unknownext")), "application/octet-stream");
    EXPECT_EQ(mime_types_t::get(boost::filesystem::path("")), "application/octet-stream");
    EXPECT_EQ(mime_types_t::get(boost::filesystem::path("file")), "application/octet-stream");
}

TEST(HttpTest, FileTInMemory) {
    std::string name = "test.txt";
    std::string type = "text/plain";

    std::vector<char> data = {'a', 'b', 'c'};

    file_t f(std::move(name), std::move(type), std::move(data));

    EXPECT_TRUE(f.in_memory());

    EXPECT_EQ(f.size(), 3u);
    EXPECT_EQ(f.name(), "test.txt");
    EXPECT_EQ(f.content_type(), "text/plain");
    EXPECT_EQ(f.data().size(), 3u);
}

TEST(HttpTest, FileTTempFile) {
    std::string name = "temp.bin";
    std::string type = "application/octet-stream";

    boost::filesystem::path temp_path = boost::filesystem::unique_path();

    {
        std::ofstream ofs(temp_path.string(), std::ios::binary);

        ofs << "hello";
    }

    file_t f(std::move(name), std::move(type), boost::filesystem::path(temp_path));

    EXPECT_FALSE(f.in_memory());

    EXPECT_EQ(f.size(), 5u);
    EXPECT_EQ(f.temp_path(), temp_path);

    auto temp_path_copy = temp_path;

    f = std::move(f);

    EXPECT_EQ(f.temp_path(), temp_path_copy);
}

TEST(HttpTest, MultipartSplitBasic) {
    auto result = multipart_t::_split("part1|part2|part3", "|");

    ASSERT_EQ(result.size(), 3);

    EXPECT_EQ(result[0], "part1");
    EXPECT_EQ(result[1], "part2");
    EXPECT_EQ(result[2], "part3");
}

TEST(HttpTest, MultipartSplitEdgeCases) {
    EXPECT_TRUE(multipart_t::_split("", "|").empty());

    auto result = multipart_t::_split("|start|middle", "|");

    ASSERT_EQ(result.size(), 3);

    EXPECT_EQ(result[0], "");
    EXPECT_EQ(result[1], "start");
    EXPECT_EQ(result[2], "middle");

    result = multipart_t::_split("beginning|end|", "|");

    ASSERT_EQ(result.size(), 3);

    EXPECT_EQ(result[0], "beginning");
    EXPECT_EQ(result[1], "end");
    EXPECT_EQ(result[2], "");

    result = multipart_t::_split("a##b##c", "##");

    ASSERT_EQ(result.size(), 3);

    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(HttpTest, MultipartExtractBetween) {
    std::string content = "prefix[START]target[END]suffix";

    auto result = multipart_t::_extract_between(content, "[START]", "[END]");
    EXPECT_EQ(result, "target");

    result = multipart_t::_extract_between(content, "[MISSING]", "[END]");
    EXPECT_TRUE(result.empty());

    result = multipart_t::_extract_between(content, "[START]", "[MISSING]");
    EXPECT_TRUE(result.empty());

    result = multipart_t::_extract_between("abc[start]def[start]ghi", "[start]", "[start]");
    EXPECT_EQ(result, "def");

    result = multipart_t::_extract_between("", "[START]", "[END]");
    EXPECT_TRUE(result.empty());
}

TEST(HttpTest, AsyncMultipartParseBasic) {
    boost::asio::io_context io_context;

    std::string boundary = "boundary123";

    std::string body =
        "--boundary123\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"test.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "file content\r\n"
        "--boundary123--";

    auto future = boost::asio::co_spawn(
        io_context,

        [&]() -> boost::asio::awaitable<void> {
            auto executor = co_await boost::asio::this_coro::executor;

            auto files = co_await multipart_t::parse_async(executor, body, boundary);

            EXPECT_EQ(files.size(), 1);
            EXPECT_TRUE(files.contains("file"));

            const auto& file = files.at("file");

            EXPECT_EQ(file.name(), "test.txt");
            EXPECT_EQ(file.content_type(), "text/plain");
            EXPECT_EQ(file.size(), 12);

            EXPECT_TRUE(file.in_memory());

            co_return;
        },

        boost::asio::use_future
    );

    io_context.run();

    future.get();
}

TEST(HttpTest, AsyncMultipartParseMultipleFiles) {
    boost::asio::io_context io_context;

    std::string boundary = "boundary456";

    std::string body =
        "--boundary456\r\n"
        "Content-Disposition: form-data; name=\"file1\"; filename=\"test1.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "file1 content\r\n"
        "--boundary456\r\n"
        "Content-Disposition: form-data; name=\"file2\"; filename=\"test2.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        "file2 data\r\n"
        "--boundary456--";

    auto future = boost::asio::co_spawn(
        io_context,

        [&]() -> boost::asio::awaitable<void> {
            auto executor = co_await boost::asio::this_coro::executor;

            auto files = co_await multipart_t::parse_async(executor, body, boundary);

            EXPECT_EQ(files.size(), 2);

            EXPECT_TRUE(files.contains("file1"));
            EXPECT_TRUE(files.contains("file2"));

            const auto& file1 = files.at("file1");

            EXPECT_EQ(file1.name(), "test1.txt");
            EXPECT_EQ(file1.content_type(), "text/plain");
            EXPECT_EQ(file1.size(), 13);

            EXPECT_TRUE(file1.in_memory());

            const auto& file2 = files.at("file2");

            EXPECT_EQ(file2.name(), "test2.jpg");
            EXPECT_EQ(file2.content_type(), "image/jpeg");
            EXPECT_EQ(file2.size(), 10);

            EXPECT_TRUE(file2.in_memory());

            co_return;
        },

        boost::asio::use_future
    );

    io_context.run();

    future.get();
}

TEST(HttpTest, AsyncMultipartParseLargeFile) {
    boost::asio::io_context io_context;

    std::string large_content(20485760u, 'X');

    std::string boundary = "boundary789";

    std::string body =
        "--boundary789\r\n"
        "Content-Disposition: form-data; name=\"large_file\"; filename=\"large.bin\"\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n"
        + large_content + "\r\n"
                          "--boundary789--";

    auto future = boost::asio::co_spawn(
        io_context,

        [&]() -> boost::asio::awaitable<void> {
            auto executor = co_await boost::asio::this_coro::executor;

            auto files = co_await multipart_t::parse_async(executor, body, boundary);

            EXPECT_EQ(files.size(), 1);

            EXPECT_TRUE(files.contains("large_file"));

            const auto& file = files.at("large_file");

            EXPECT_EQ(file.name(), "large.bin");
            EXPECT_EQ(file.content_type(), "application/octet-stream");
            EXPECT_EQ(file.size(), large_content.size());

            EXPECT_FALSE(file.in_memory());
            EXPECT_FALSE(file.temp_path().empty());

            EXPECT_TRUE(boost::filesystem::exists(file.temp_path()));

            co_return;
        },

        boost::asio::use_future
    );

    io_context.run();

    future.get();
}

TEST(HttpTest, AsyncMultipartParseInvalidFormat) {
    boost::asio::io_context io_context;

    std::string boundary = "boundary";

    std::string body = "--boundary\r\nInvalid format without proper headers\r\n--boundary--";

    auto future = boost::asio::co_spawn(
        io_context,

        [&]() -> boost::asio::awaitable<void> {
            auto executor = co_await boost::asio::this_coro::executor;

            auto files = co_await multipart_t::parse_async(executor, body, boundary);

            EXPECT_TRUE(files.empty());

            co_return;
        },

        boost::asio::use_future
    );

    io_context.run();

    future.get();
}

TEST(HttpTest, AsyncMultipartParseCorruptedBoundary) {
    boost::asio::io_context io_context;

    std::string boundary = "boundary";

    std::string body =
        "--boundary\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"test.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "file content\r\n"
        "--corrupted--";

    auto future = boost::asio::co_spawn(
        io_context,

        [&]() -> boost::asio::awaitable<void> {
            auto executor = co_await boost::asio::this_coro::executor;

            auto files = co_await multipart_t::parse_async(executor, body, boundary);

            EXPECT_TRUE(files.empty());

            co_return;
        },

        boost::asio::use_future
    );

    io_context.run();

    future.get();
}

TEST(HttpRequestTest, KeepAliveDefaultAndExplicit) {
    request_t req;

    EXPECT_TRUE(req.keep_alive());

    req.headers().emplace("Connection", "close");

    EXPECT_FALSE(req.keep_alive());

    req.headers()["Connection"] = "Keep-Alive";

    EXPECT_TRUE(req.keep_alive());
}

TEST(HttpRequestTest, CookieParsing) {
    request_t req;

    EXPECT_EQ(req.cookie("any"), std::nullopt);

    req.headers().emplace("Cookie", "a=1; b=two; empty=");

    {
        auto a = req.cookie("a");

        ASSERT_TRUE(a.has_value());
        EXPECT_EQ(*a, "1");

        auto b = req.cookie("b");

        ASSERT_TRUE(b.has_value());
        EXPECT_EQ(*b, "two");

        auto empty = req.cookie("empty");

        ASSERT_TRUE(empty.has_value());
        EXPECT_EQ(*empty, "");
    }

    EXPECT_EQ(req.cookie("missing"), std::nullopt);

    req.headers()["Cookie"] = "  key = value ; next= v2 ";

    {
        auto k = req.cookie("key");

        ASSERT_TRUE(k);
        EXPECT_EQ(*k, "value");

        auto n = req.cookie("next");

        ASSERT_TRUE(n);
        EXPECT_EQ(*n, "v2");
    }
}

TEST(HttpRequestTest, AccessorsMutateAndRead) {
    request_t req;

    req.method() = e_method::post;

    EXPECT_EQ(req.method(), e_method::post);

    req.headers().emplace("X-Test", "42");

    EXPECT_EQ(req.headers().at("X-Test"), "42");

    req.body() = body_t("hello");

    EXPECT_EQ(req.body(), body_t("hello"));

    req.uri() = uri_t("/path?x=1");

    EXPECT_EQ(req.uri(), "/path?x=1");
}

TEST(HttpResponseTest, DefaultValues) {
    response_t r;

    EXPECT_EQ(r.body(), "");

    EXPECT_TRUE(r.headers().empty());
    EXPECT_TRUE(r.cookies().empty());

    EXPECT_EQ(r.status(), e_status::ok);

    EXPECT_FALSE(r.stream());
    EXPECT_FALSE(static_cast<bool>(r.callback()));
}

TEST(HttpResponseTest, PlainTextConstructor) {
    headers_t extra{{"X", "Y"}};

    response_t r("hi", e_status::created, std::move(extra));

    EXPECT_EQ(r.body(), "hi");
    EXPECT_EQ(r.status(), e_status::created);

    EXPECT_EQ(r.headers().at("X"), "Y");
    EXPECT_EQ(r.headers().at("Content-Type"), "text/plain");
}

TEST(HttpResponseTest, SetCookieBasic) {
    response_t r;

    r.set_cookie(
        cookie_t{
            .m_name = "n",
            .m_value = "v",
        }
    );

    ASSERT_EQ(r.cookies().size(), 1u);

    EXPECT_TRUE(r.cookies()[0].find("n=v; Path=/; ") != std::string::npos);
}

TEST(HttpResponseTest, JsonResponseConstructor) {
    json_t::json_obj_t obj{
        {"foo", "bar"}
    };

    headers_t extra{{"A", "B"}};

    json_response_t jr(obj, e_status::accepted, std::move(extra));

    EXPECT_EQ(jr.status(), e_status::accepted);

    EXPECT_EQ(jr.headers().at("A"), "B");
    EXPECT_EQ(jr.headers().at("Content-Type"), "application/json");

    EXPECT_FALSE(jr.body().empty());
}

TEST(HttpResponseTest, StreamResponseConstructor) {
    response_t::callback_t cb = [](boost::asio::ip::tcp::socket&) -> boost::asio::awaitable<void> {
        co_return;
    };

    headers_t extra{{"H", "V"}};

    stream_response_t sr(std::move(cb), std::string("application/foo"), e_status::partial_content, std::move(extra));

    EXPECT_TRUE(sr.stream());

    EXPECT_EQ(sr.status(), e_status::partial_content);

    EXPECT_EQ(sr.headers().at("H"), "V");
    EXPECT_EQ(sr.headers().at("Cache-Control"), "no-cache");
    EXPECT_EQ(sr.headers().at("Content-Type"), "application/foo");
}

TEST(HttpResponseTest, RedirectResponseValidAndInvalid) {
    headers_t extra{{"X", "Y"}};

    redirect_response_t r1("/new", e_status::see_other, std::move(extra));

    EXPECT_EQ(r1.status(), e_status::see_other);

    EXPECT_EQ(r1.headers().at("Location"), "/new");
    EXPECT_EQ(r1.headers().at("Content-Type"), "text/plain");
    EXPECT_EQ(r1.headers().at("X"), "Y");

    redirect_response_t r2("/other", e_status::ok);

    EXPECT_EQ(r2.status(), e_status::found);
    EXPECT_EQ(r2.headers().at("Location"), "/other");
}