#include <gtest/gtest.h>

#include <cxxapi.hxx>

using namespace cxxapi::route;
using namespace cxxapi::http;

TEST(RouteTest, SyncHandlerCreationAndExecution) {
    auto handler = [](http_ctx_t ctx) -> response_t {
        return response_t("Hello", e_status::ok);
    };

    fn_route_t route(e_method::get, "/test", std::move(handler));

    EXPECT_FALSE(route.is_async());
    
    http_ctx_t ctx;
    auto response = route.handle(std::move(ctx));
    
    EXPECT_EQ(response.body(), "Hello");
    EXPECT_EQ(response.status(), e_status::ok);
}

TEST(RouteTest, AsyncHandlerCreationAndExecution) {
    auto handler = [](http_ctx_t ctx) -> boost::asio::awaitable<response_t> {
        co_return response_t("Async", e_status::ok);
    };

    fn_route_t route(e_method::post, "/async", std::move(handler));

    EXPECT_TRUE(route.is_async());

    boost::asio::io_context io_context;
    
    auto future = boost::asio::co_spawn(io_context,
        [&]() -> boost::asio::awaitable<void> {
            http_ctx_t ctx;
            auto response = co_await route.handle_async(std::move(ctx));
            
            EXPECT_EQ(response.body(), "Async");
            EXPECT_EQ(response.status(), e_status::ok);
            
            co_return;
        },

        boost::asio::use_future
    );

    io_context.run();
    future.get();
}

TEST(RouteTest, TrieNodeInsertAndFind) {
    cxxapi::route::internal::trie_node_t<std::function<response_t(http_ctx_t)>> node;

    auto handler = [](http_ctx_t ctx) -> response_t {
        return response_t("Found", e_status::ok);
    };

    EXPECT_TRUE(node.insert(e_method::get, "/test/path", handler));
    
    auto result = node.find(e_method::get, "/test/path");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->second.size(), 0);
    
    auto not_found = node.find(e_method::post, "/test/path");

    EXPECT_FALSE(not_found.has_value());
}

TEST(RouteTest, TrieNodeDynamicSegments) {
    cxxapi::route::internal::trie_node_t<std::function<response_t(http_ctx_t)>> node;

    auto handler = [](http_ctx_t ctx) -> response_t {
        return response_t("Dynamic", e_status::ok);
    };

    EXPECT_TRUE(node.insert(e_method::get, "/user/{id}", handler));
    
    auto result = node.find(e_method::get, "/user/123");

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->second.size(), 1);
    EXPECT_EQ(result->second["id"], "123");
}

TEST(RouteTest, TrieNodeNormalization) {
    cxxapi::route::internal::trie_node_t<std::function<response_t(http_ctx_t)>> node;

    auto handler = [](http_ctx_t ctx) -> response_t {
        return response_t("Normalized", e_status::ok);
    };

    EXPECT_TRUE(node.insert(e_method::get, "/test/", handler));
    
    auto result = node.find(e_method::get, "/test");

    ASSERT_TRUE(result.has_value());
    
    result = node.find(e_method::get, "/test/");

    ASSERT_TRUE(result.has_value());
}

TEST(RouteTest, HandlerConcepts) {
    auto sync_handler = [](http_ctx_t ctx) -> response_t {
        return response_t("Sync", e_status::ok);
    };

    auto async_handler = [](http_ctx_t ctx) -> boost::asio::awaitable<response_t> {
        co_return response_t("Async", e_status::ok);
    };

    EXPECT_TRUE(cxxapi::route::internal::sync_handler_c<decltype(sync_handler)>);
    EXPECT_TRUE(cxxapi::route::internal::async_handler_c<decltype(async_handler)>);

    auto invalid_handler = [](http_ctx_t ctx) -> int { return 42; };

    EXPECT_FALSE(cxxapi::route::internal::sync_handler_c<decltype(invalid_handler)>);
    EXPECT_FALSE(cxxapi::route::internal::async_handler_c<decltype(invalid_handler)>);
}

TEST(RouteTest, TrieNodeSpecialCharacters) {
    cxxapi::route::internal::trie_node_t<std::function<response_t(http_ctx_t)>> node;

    auto handler = [](http_ctx_t ctx) -> response_t {
        return response_t("Special", e_status::ok);
    };

    EXPECT_TRUE(node.insert(e_method::get, "/user/{id}/@me", handler));
    
    auto result = node.find(e_method::get, "/user/123/@me");

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->second.size(), 1);
    EXPECT_EQ(result->second["id"], "123");
}

TEST(RouteTest, TrieNodeEmptyPath) {
    cxxapi::route::internal::trie_node_t<std::function<response_t(http_ctx_t)>> node;

    auto handler = [](http_ctx_t ctx) -> response_t {
        return response_t("Root", e_status::ok);
    };

    EXPECT_TRUE(node.insert(e_method::get, "/", handler));
    
    auto result = node.find(e_method::get, "/");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->second.size(), 0);
}

TEST(RouteTest, TrieNodeDuplicateInsert) {
    cxxapi::route::internal::trie_node_t<std::function<response_t(http_ctx_t)>> node;

    auto handler1 = [](http_ctx_t ctx) -> response_t {
        return response_t("First", e_status::ok);
    };

    auto handler2 = [](http_ctx_t ctx) -> response_t {
        return response_t("Second", e_status::ok);
    };

    EXPECT_TRUE(node.insert(e_method::get, "/test", handler1));
    EXPECT_THROW(node.insert(e_method::get, "/test", handler2), cxxapi::base_exception_t);
}

TEST(RouteTest, TrieNodeMultipleDynamicSegments) {
    cxxapi::route::internal::trie_node_t<std::function<response_t(http_ctx_t)>> node;

    auto handler = [](http_ctx_t ctx) -> response_t {
        return response_t("Multiple", e_status::ok);
    };

    EXPECT_TRUE(node.insert(e_method::get, "/user/{id}/post/{post_id}", handler));
    
    auto result = node.find(e_method::get, "/user/123/post/456");

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->second.size(), 2);
    EXPECT_EQ(result->second["id"], "123");
    EXPECT_EQ(result->second["post_id"], "456");
}

TEST(RouteTest, TrieNodeInvalidDynamicSegment) {
    cxxapi::route::internal::trie_node_t<std::function<response_t(http_ctx_t)>> node;

    auto handler = [](http_ctx_t ctx) -> response_t {
        return response_t("Invalid", e_status::ok);
    };

    EXPECT_THROW(node.insert(e_method::get, "/user/{}/profile", handler), cxxapi::base_exception_t);
    EXPECT_THROW(node.insert(e_method::get, "/user/{id", handler), cxxapi::base_exception_t);
    EXPECT_THROW(node.insert(e_method::get, "/user/id}", handler), cxxapi::base_exception_t);
}

TEST(RouteTest, HandlerConceptVariants) {
    auto sync_handler_variant = [](http_ctx_t ctx) -> response_t {
        return response_t("Variant", e_status::created);
    };

    EXPECT_TRUE(cxxapi::route::internal::sync_handler_c<decltype(sync_handler_variant)>);

    auto async_handler_variant = [](http_ctx_t ctx) -> boost::asio::awaitable<response_t> {
        co_return response_t("Variant", e_status::accepted);
    };

    EXPECT_TRUE(cxxapi::route::internal::async_handler_c<decltype(async_handler_variant)>);

    auto invalid_param_handler = [](int x) -> response_t {
        return response_t("Invalid", e_status::ok);
    };

    EXPECT_FALSE(cxxapi::route::internal::sync_handler_c<decltype(invalid_param_handler)>);
    EXPECT_FALSE(cxxapi::route::internal::async_handler_c<decltype(invalid_param_handler)>);
}
