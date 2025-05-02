#ifdef CXXAPI_USE_LOGGING_IMPL

#include <gtest/gtest.h>

#include <cxxapi.hxx>

using namespace shared;

TEST(LoggingTest, InitAndShutdown) {
    c_logging logger;

    EXPECT_NO_THROW({
        logger.init(e_log_level::debug, true, true, 1024, c_logging::e_overflow_strategy::discard_oldest);

        logger.start_async();

        logger.stop_async();
    });
}

TEST(LoggingTest, ForceLogDoesNotThrow) {
    c_logging logger;

    logger.init(e_log_level::debug, true, false);

    EXPECT_NO_THROW({ logger.force_log(e_log_level::info, "Force log test: {}", 123); });
}

TEST(LoggingTest, AsyncLogDoesNotThrow) {
    c_logging logger;

    logger.init(e_log_level::debug, true, true, 1024, c_logging::e_overflow_strategy::discard_oldest);

    logger.start_async();

    EXPECT_NO_THROW({ logger.log(e_log_level::info, "Async log test: {}", 456); });

    logger.stop_async();
}

TEST(LoggingTest, LogBufferPushPop) {
    log_buffer_t buffer(2);

    log_message_t msg1{e_log_level::info, "msg1", std::chrono::system_clock::now()};
    log_message_t msg2{e_log_level::warning, "msg2", std::chrono::system_clock::now()};

    EXPECT_TRUE(buffer.push(std::move(msg1)));
    EXPECT_TRUE(buffer.push(std::move(msg2)));

    log_message_t msg3{e_log_level::error, "msg3", std::chrono::system_clock::now()};

    EXPECT_FALSE(buffer.push(std::move(msg3)));

    log_message_t out1, out2;

    EXPECT_TRUE(buffer.pop(out1));
    EXPECT_TRUE(buffer.pop(out2));

    EXPECT_EQ(out1.m_message, "msg1");
    EXPECT_EQ(out2.m_message, "msg2");

    EXPECT_TRUE(buffer.empty());
}

TEST(LoggingTest, LogBufferGetBatch) {
    log_buffer_t buffer(5);

    for (int i = 0; i < 3; ++i) {
        log_message_t msg{e_log_level::info, "msg" + std::to_string(i), std::chrono::system_clock::now()};

        EXPECT_TRUE(buffer.push(std::move(msg)));
    }

    auto batch = buffer.get_batch(2);

    EXPECT_EQ(batch.size(), 2);
    EXPECT_EQ(batch[0].m_message, "msg0");
    EXPECT_EQ(batch[1].m_message, "msg1");

    auto batch2 = buffer.get_batch(5);

    EXPECT_EQ(batch2.size(), 1);
    EXPECT_EQ(batch2[0].m_message, "msg2");

    EXPECT_TRUE(buffer.empty());
}

TEST(LoggingTest, LogLevelToString) {
    c_logging logger;

    EXPECT_STREQ(logger.lvl_to_str(e_log_level::debug), "DEBUG");
    EXPECT_STREQ(logger.lvl_to_str(e_log_level::info), "INFO");
    EXPECT_STREQ(logger.lvl_to_str(e_log_level::warning), "WARNING");
    EXPECT_STREQ(logger.lvl_to_str(e_log_level::error), "ERROR");
    EXPECT_STREQ(logger.lvl_to_str(e_log_level::critical), "CRITICAL");
}

#endif // CXXAPI_USE_LOGGING_IMPL
