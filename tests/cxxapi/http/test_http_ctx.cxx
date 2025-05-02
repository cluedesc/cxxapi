#include <gtest/gtest.h>

#include <cxxapi.hxx>

using namespace cxxapi::http;

static request_t make_basic_request(e_method m = e_method::get) {
    request_t req;

    req.method() = m;
    req.uri() = uri_t("/test");

    return req;
}

TEST(HttpCtxTest, DirectCtorAccessors) {
    request_t req = make_basic_request(e_method::post);
    params_t parms = {{"a", "1"}, {"b", "2"}};

    http_ctx_t ctx(req, parms);

    EXPECT_TRUE(ctx.files().empty());
}

TEST(HttpCtxTest, FileLookupPositiveAndNegative) {
    files_t files;

    files.emplace("key", file_t("n.txt", "text/plain", std::vector<char>{'x', 'y'}));

    http_ctx_t ctx;

    ctx.files() = std::move(files);

    file_t* f = ctx.file("key");

    if (!f)
        throw std::runtime_error("file not found");

    EXPECT_EQ(f->name(), "n.txt");
    EXPECT_EQ(ctx.file("missing"), nullptr);
}

TEST(HttpCtxTest, FactorySkipsParsingWhenNotMultipart) {
    boost::asio::io_context io;

    request_t req = make_basic_request();
    params_t parms = {{"x", "42"}};

    auto fut = boost::asio::co_spawn(
        io,
        [&]() -> boost::asio::awaitable<void> {
            auto ex = co_await boost::asio::this_coro::executor;
            auto ctx = co_await http_ctx_t::create(ex, req, parms);

            EXPECT_TRUE(ctx.files().empty());
            EXPECT_EQ(ctx.params().at("x"), "42");

            co_return;
        },
        boost::asio::use_future
    );

    io.run();

    fut.get();
}

TEST(HttpCtxTest, FactoryParsesMultipartMemoryFile) {
    boost::asio::io_context io;

    const std::string boundary = "bnd123";
    const std::string body =
        "--bnd123\r\n"
        "Content-Disposition: form-data; name=\"f\"; filename=\"hello.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "hello world\r\n"
        "--bnd123--";

    request_t req = make_basic_request(e_method::post);

    req.headers().emplace("Content-Type", "multipart/form-data; boundary=" + boundary);
    req.body() = body;

    params_t parms;

    auto fut = boost::asio::co_spawn(
        io,
        [&]() -> boost::asio::awaitable<void> {
            auto ex = co_await boost::asio::this_coro::executor;
            auto ctx = co_await http_ctx_t::create(ex, req, parms);

            EXPECT_EQ(ctx.files().size(), 1u);
            EXPECT_NE(ctx.file("f"), nullptr);

            const file_t& f = *ctx.file("f");

            EXPECT_EQ(f.name(), "hello.txt");
            EXPECT_EQ(f.content_type(), "text/plain");
            EXPECT_EQ(f.size(), 11u);

            EXPECT_TRUE(f.in_memory());
            co_return;
        },
        boost::asio::use_future
    );

    io.run();

    fut.get();
}

TEST(HttpCtxTest, FactoryParsesMultipartLargeFileToTemp) {
    boost::asio::io_context io;

    const std::string boundary = "large_bnd";

    std::string large_body(20485760u, 'Z');

    const std::string body =
        "--large_bnd\r\n"
        "Content-Disposition: form-data; name=\"big\"; filename=\"big.bin\"\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n"
        + large_body + "\r\n--large_bnd--";

    request_t req = make_basic_request(e_method::post);

    req.headers().emplace("Content-Type", "multipart/form-data; boundary=" + boundary);
    req.body() = body;

    auto fut = boost::asio::co_spawn(
        io,

        [&]() -> boost::asio::awaitable<void> {
            auto ex = co_await boost::asio::this_coro::executor;
            auto ctx = co_await http_ctx_t::create(ex, req, params_t{});

            EXPECT_EQ(ctx.files().size(), 1u);

            const file_t& f = *ctx.file("big");

            EXPECT_FALSE(f.in_memory());
            EXPECT_TRUE(boost::filesystem::exists(f.temp_path()));

            EXPECT_EQ(f.size(), large_body.size());

            co_return;
        },

        boost::asio::use_future
    );

    io.run();

    fut.get();
}

TEST(HttpCtxTest, MoveSemantics) {
    request_t req = make_basic_request();
    params_t p = {{"k", "v"}};

    http_ctx_t a(req, p);

    http_ctx_t b(std::move(a));

    EXPECT_EQ(b.params().at("k"), "v");

    http_ctx_t c;

    c = std::move(b);

    EXPECT_EQ(c.params().at("k"), "v");
}

TEST(HttpCtxEdgeTest, ExtractBoundaryVariants) {
    http_ctx_t dummy;

    EXPECT_TRUE(dummy._extract_boundary("multipart/form-data").empty());
    EXPECT_TRUE(dummy._extract_boundary("multipart/form-data; boundary").empty());

    EXPECT_EQ(dummy._extract_boundary("multipart/form-data; boundary=\"abc\""), "abc");
    EXPECT_EQ(dummy._extract_boundary("multipart/form-data; charset=utf-8; boundary=xyz; foo=bar"), "xyz");
}

TEST(HttpCtxInvalidTest, MultipartHeaderButEmptyBody) {
    boost::asio::io_context io;

    request_t req;
    params_t p;

    req.headers().emplace("Content-Type", "multipart/form-data; boundary=none");

    auto fut = boost::asio::co_spawn(
        io,
        [&]() -> boost::asio::awaitable<void> {
            auto ex = co_await boost::asio::this_coro::executor;
            auto ctx = co_await http_ctx_t::create(ex, req, p);

            EXPECT_TRUE(ctx.files().empty());

            co_return;
        },

        boost::asio::use_future
    );

    io.run();

    fut.get();
}

TEST(HttpCtxInvalidTest, BoundaryMismatch) {
    boost::asio::io_context io;

    const std::string body =
        "--aaa\r\n"
        "Content-Disposition: form-data; name=\"x\"; filename=\"a.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "data\r\n"
        "--aaa--";

    request_t req;

    req.headers().emplace("Content-Type", "multipart/form-data; boundary=bbb");
    req.body() = body;

    auto fut = boost::asio::co_spawn(
        io,
        [&]() -> boost::asio::awaitable<void> {
            auto ex = co_await boost::asio::this_coro::executor;
            auto ctx = co_await http_ctx_t::create(ex, req, params_t{});

            EXPECT_TRUE(ctx.files().empty());

            co_return;
        },

        boost::asio::use_future
    );

    io.run();

    fut.get();
}

TEST(HttpCtxEdgeTest, DuplicateFieldNamesKeepFirst) {
    boost::asio::io_context io;

    const std::string body =
        "--dup\r\n"
        "Content-Disposition: form-data; name=\"dup\"; filename=\"one.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\none\r\n"
        "--dup\r\n"
        "Content-Disposition: form-data; name=\"dup\"; filename=\"two.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\ntwo\r\n"
        "--dup--";

    request_t req;

    req.headers().emplace("Content-Type", "multipart/form-data; boundary=dup");
    req.body() = body;

    auto fut = boost::asio::co_spawn(
        io,
        [&]() -> boost::asio::awaitable<void> {
            auto ex = co_await boost::asio::this_coro::executor;
            auto ctx = co_await http_ctx_t::create(ex, req, params_t{});

            EXPECT_EQ(ctx.files().size(), 1u);
            EXPECT_EQ(ctx.file("dup")->name(), "one.txt");

            co_return;
        },

        boost::asio::use_future
    );

    io.run();

    fut.get();
}

TEST(HttpCtxEdgeTest, QuotedBoundaryMixedCaseHeader) {
    boost::asio::io_context io;

    const std::string body =
        "--qb\r\n"
        "Content-Disposition: form-data; name=\"f\"; filename=\"ok.bin\"\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n123\r\n"
        "--qb--";

    request_t req;

    req.headers().emplace("content-type", "Multipart/Form-Data; boundary=\"qb\"");
    req.body() = body;

    auto fut = boost::asio::co_spawn(
        io,
        [&]() -> boost::asio::awaitable<void> {
            auto ex = co_await boost::asio::this_coro::executor;
            auto ctx = co_await http_ctx_t::create(ex, req, params_t{});

            auto file = ctx.file("f");

            if (!file)
                throw std::runtime_error("file not found");

            EXPECT_EQ(file->size(), 3u);

            co_return;
        },

        boost::asio::use_future
    );

    io.run();

    fut.get();
}

TEST(HttpCtxEdgeTest, FileExactlyAtMemoryThreshold) {
    boost::asio::io_context io;

    std::string content(8192u, 'A');

    const std::string body =
        "--mem\r\n"
        "Content-Disposition: form-data; name=\"f\"; filename=\"equal.bin\"\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n"
        + content + "\r\n--mem--";

    request_t req;

    req.headers().emplace("Content-Type", "multipart/form-data; boundary=mem");
    req.body() = body;

    auto fut = boost::asio::co_spawn(
        io,
        [&]() -> boost::asio::awaitable<void> {
            auto ex = co_await boost::asio::this_coro::executor;
            auto ctx = co_await http_ctx_t::create(ex, req, params_t{});

            const file_t& f = *ctx.file("f");

            EXPECT_TRUE(f.in_memory());
            EXPECT_EQ(f.size(), content.size());

            co_return;
        },

        boost::asio::use_future
    );

    io.run();

    fut.get();
}

TEST(HttpCtxEdgeTest, ZeroLengthFile) {
    boost::asio::io_context io;

    const std::string body =
        "--z\r\n"
        "Content-Disposition: form-data; name=\"zf\"; filename=\"zero.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "\r\n--z--";

    request_t req;

    req.headers().emplace("Content-Type", "multipart/form-data; boundary=z");
    req.body() = body;

    auto fut = boost::asio::co_spawn(
        io,
        [&]() -> boost::asio::awaitable<void> {
            auto ex = co_await boost::asio::this_coro::executor;
            auto ctx = co_await http_ctx_t::create(ex, req, params_t{});

            const file_t& f = *ctx.file("zf");

            EXPECT_EQ(f.size(), 0u);

            co_return;
        },

        boost::asio::use_future
    );

    io.run();

    fut.get();
}

TEST(HttpCtxInvalidTest, PartWithoutFilenameIgnored) {
    boost::asio::io_context io;
    
    const std::string body =
        "--nf\r\n"
        "Content-Disposition: form-data; name=\"nofile\"\r\n"
        "\r\n"
        "ignored\r\n"
        "--nf--";

    request_t req;

    req.headers().emplace("Content-Type", "multipart/form-data; boundary=nf");
    req.body() = body;

    auto fut = boost::asio::co_spawn(
        io,
        [&]() -> boost::asio::awaitable<void> {
            auto ex = co_await boost::asio::this_coro::executor;
            auto ctx = co_await http_ctx_t::create(ex, req, params_t{});

            EXPECT_TRUE(ctx.files().empty());

            co_return;
        },

        boost::asio::use_future
    );

    io.run();

    fut.get();
}

TEST(HttpCtxEdgeTest, BoundaryInsideFileContent) {
    boost::asio::io_context io;

    const std::string boundary = "xx";
    const std::string file_payload = "some --xx text";

    const std::string body =
        "--xx\r\n"
        "Content-Disposition: form-data; name=\"in\"; filename=\"b.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        + file_payload + "\r\n"
                         "--xx--";

    request_t req;

    req.headers().emplace("Content-Type", "multipart/form-data; boundary=xx");
    req.body() = body;

    auto fut = boost::asio::co_spawn(
        io,
        [&]() -> boost::asio::awaitable<void> {
            auto ex = co_await boost::asio::this_coro::executor;
            auto ctx = co_await http_ctx_t::create(ex, req, params_t{});

            const file_t& f = *ctx.file("in");

            EXPECT_EQ(f.size(), file_payload.size());
            EXPECT_EQ(std::string_view(f.data().data(), f.size()), file_payload);

            co_return;
        },

        boost::asio::use_future
    );

    io.run();

    fut.get();
}