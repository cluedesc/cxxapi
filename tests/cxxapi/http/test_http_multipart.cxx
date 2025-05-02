#include <gtest/gtest.h>

#include <cxxapi.hxx>

using namespace cxxapi::http;

using executor_t = boost::asio::any_io_executor;

static void run_ctx_parse_test(const std::string& content_type, const std::string& boundary, const std::string& body_content, bool expect_file) {
    boost::asio::io_context io_context;

    request_t req;
    params_t params;

    req.headers().emplace("Content-Type", content_type);

    std::string b =
        "--" + boundary + "\r\n"
                          "Content-Disposition: form-data; name=\"f\"; filename=\"a.txt\"\r\n"
                          "Content-Type: text/plain\r\n"
                          "\r\n"
        + body_content + "\r\n"
                         "--"
        + boundary + "--";

    req.body() = body_t(b);

    auto fut = boost::asio::co_spawn(
        io_context,
        [&]() -> boost::asio::awaitable<void> {
            auto ex = co_await boost::asio::this_coro::executor;
            auto ctx = co_await http_ctx_t::create(ex, req, params);

            if (expect_file) {
                EXPECT_EQ(ctx.files().size(), 1u);
                EXPECT_TRUE(ctx.files().contains("f"));

                const auto& f = ctx.files().at("f");

                EXPECT_EQ(f.name(), "a.txt");
                EXPECT_EQ(f.content_type(), "text/plain");
                EXPECT_EQ(f.size(), body_content.size());

                EXPECT_TRUE(f.in_memory());
            }
            else 
                EXPECT_TRUE(ctx.files().empty());
                 
            co_return;
        },

        boost::asio::use_future
    );

    io_context.run();

    fut.get();
}

TEST(HttpTest, AsyncMultipartParse_QuotedBoundaryWithCharset) {
    run_ctx_parse_test(
        R"(multipart/form-data; Charset=UTF-8; boundary="----WebKitFormBoundaryabc123")",
        "----WebKitFormBoundaryabc123",
        "world",
        true
    );
}

TEST(HttpTest, AsyncMultipartParse_SingleQuotedBoundary) {
    run_ctx_parse_test(
        "multipart/form-data; boundary='my-boundary'",
        "my-boundary",
        "data123",
        true
    );
}

TEST(HttpTest, AsyncMultipartParse_UppercaseBoundaryKey) {
    run_ctx_parse_test(
        "multipart/form-data; BOUNDARY=UPPER123",
        "UPPER123",
        "XYZ",
        true
    );
}

TEST(HttpTest, AsyncMultipartParse_MissingBoundary) {
    run_ctx_parse_test(
        "multipart/form-data; charset=UTF-8",
        "",
        "shouldfail",
        false
    );
}