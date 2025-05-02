#include <gtest/gtest.h>

#include <cxxapi.hxx>

using namespace shared;

TEST(JsonTraitsTest, DumpAndParse) {
    json_traits_t::json_obj_t test_map{
        {"test", "test_value"},
        {"test2", "test_value2"},
    };

    auto json = json_traits_t::serialize(test_map);

    auto parsed_map = json_traits_t::deserialize(json);

    EXPECT_EQ(json_traits_t::at<std::string>(parsed_map, "test"), "test_value");
    EXPECT_EQ(json_traits_t::at<std::string>(parsed_map, "test2"), "test_value2");
}
