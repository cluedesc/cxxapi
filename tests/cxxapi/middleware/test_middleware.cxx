#include <gtest/gtest.h>

#include <cxxapi.hxx>

using namespace cxxapi::middleware;
using namespace cxxapi::http;

namespace {
    class mock_middleware : public c_base_middleware {
      public:
        mock_middleware(std::string header, std::string value)
            : m_header(std::move(header)), m_value(std::move(value)) {}

        boost::asio::awaitable<response_t> handle(
            const request_t& request,
            
            std::function<boost::asio::awaitable<response_t>(const request_t&)> next
        ) override {
            auto response = co_await next(request);

            response.headers()[m_header] = m_value;

            co_return response;
        }

      private:
        std::string m_header;
        std::string m_value;
    };

    class terminating_middleware : public c_base_middleware {
      public:
        terminating_middleware(response_t response)
            : m_response(std::move(response)) {}

        boost::asio::awaitable<response_t> handle(
            const request_t& request,
            
            std::function<boost::asio::awaitable<response_t>(const request_t&)> next
        ) override {
            co_return m_response;
        }

      private:
        response_t m_response;
    };
}

TEST(MiddlewareTest, SingleMiddlewareChain) {
    boost::asio::io_context io_context;

    auto middleware = std::make_shared<mock_middleware>("X-Test", "Value");

    auto future = boost::asio::co_spawn(
        io_context,

        [&]() -> boost::asio::awaitable<void> {
            request_t request;

            auto response = co_await middleware->handle(
                request,

                [](const request_t&) -> boost::asio::awaitable<response_t> {
                    co_return response_t("Hello", e_status::ok);
                }
            );

            EXPECT_EQ(response.headers().at("X-Test"), "Value");
            EXPECT_EQ(response.body(), "Hello");
            EXPECT_EQ(response.status(), e_status::ok);

            co_return;
        },

        boost::asio::use_future
    );

    io_context.run();

    future.get();
}

TEST(MiddlewareTest, MultipleMiddlewareChain) {
    boost::asio::io_context io_context;

    auto middleware1 = std::make_shared<mock_middleware>("X-First", "1");
    auto middleware2 = std::make_shared<mock_middleware>("X-Second", "2");

    auto future = boost::asio::co_spawn(
        io_context,

        [&]() -> boost::asio::awaitable<void> {
            request_t request;

            auto response = co_await middleware1->handle(
                request,

                [&](const request_t& req) -> boost::asio::awaitable<response_t> {
                    return middleware2->handle(req, [](const request_t&) -> boost::asio::awaitable<response_t> {
                        co_return response_t("Chain", e_status::ok);
                    });
                }
            );

            EXPECT_EQ(response.headers().at("X-First"), "1");
            EXPECT_EQ(response.headers().at("X-Second"), "2");
            EXPECT_EQ(response.body(), "Chain");
            EXPECT_EQ(response.status(), e_status::ok);

            co_return;
        },

        boost::asio::use_future
    );

    io_context.run();

    future.get();
}

TEST(MiddlewareTest, TerminatingMiddleware) {
    boost::asio::io_context io_context;

    auto middleware = std::make_shared<terminating_middleware>(
        response_t("Terminated", e_status::ok)
    );

    auto future = boost::asio::co_spawn(
        io_context,

        [&]() -> boost::asio::awaitable<void> {
            request_t request;

            auto response = co_await middleware->handle(request, [](const request_t&) -> boost::asio::awaitable<response_t> {
                ADD_FAILURE() << "Next middleware should not be called";

                co_return response_t("Should not reach here", e_status::internal_server_error);
            });

            EXPECT_EQ(response.body(), "Terminated");
            EXPECT_EQ(response.status(), e_status::ok);

            co_return;
        },

        boost::asio::use_future
    );

    io_context.run();

    future.get();
}

TEST(MiddlewareTest, EmptyMiddlewareChain) {
    boost::asio::io_context io_context;

    auto future = boost::asio::co_spawn(
        io_context,

        []() -> boost::asio::awaitable<void> {
            request_t request;

            auto response = co_await [](const request_t&) -> boost::asio::awaitable<response_t> {
                co_return response_t("Empty", e_status::ok);
            }(request);

            EXPECT_EQ(response.body(), "Empty");
            EXPECT_EQ(response.status(), e_status::ok);

            co_return;
        },

        boost::asio::use_future
    );

    io_context.run();

    future.get();
}
