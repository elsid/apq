#include <ozo/io/send.h>
#include <ozo/io/array.h>
#include <ozo/ext/std.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <string_view>

namespace {

using namespace testing;

struct send : Test {
    std::vector<char> buffer;
    ozo::detail::ostreambuf obuf{buffer};
    ozo::ostream os{&obuf};

    struct badbuf_t : std::streambuf{} badbuf;
    ozo::ostream bad_ostream{&badbuf};

    ozo::empty_oid_map oid_map;
};

TEST_F(send, with_single_byte_type_and_bad_ostream_should_throw) {
    EXPECT_THROW(
        ozo::send(bad_ostream, oid_map, char(42)),
        ozo::system_error
    );
}

TEST_F(send, with_multi_byte_type_and_bad_ostream_should_throw) {
    EXPECT_THROW(
        ozo::send(bad_ostream, oid_map, std::int64_t(42)),
        ozo::system_error
    );
}

TEST_F(send, with_std_int8_t_should_store_it_as_is) {
    ozo::send(os, oid_map, char(42));
    EXPECT_THAT(buffer, ElementsAre(42));
}

TEST_F(send, with_std_int16_t_should_store_it_in_big_endian_order) {
    ozo::send(os, oid_map, std::int16_t(42));
    EXPECT_THAT(buffer, ElementsAre(0, 42));
}

TEST_F(send, with_std_int32_t_should_store_it_in_big_endian_order) {
    ozo::send(os, oid_map, std::int32_t(42));
    EXPECT_THAT(buffer, ElementsAre(0, 0, 0, 42));
}

TEST_F(send, with_std_int64_t_should_store_it_in_big_endian_order) {
    ozo::send(os, oid_map, std::int64_t(42));
    EXPECT_THAT(buffer, ElementsAre(0, 0, 0, 0, 0, 0, 0, 42));
}

TEST_F(send, with_float_should_store_it_as_integral_in_big_endian_order) {
    ozo::send(os, oid_map, 42.13f);
    EXPECT_THAT(buffer, ElementsAre(0x42, 0x28, 0x85, 0x1F));
}

TEST_F(send, with_std_string_should_store_it_as_is) {
    ozo::send(os, oid_map, std::string("text"));
    EXPECT_THAT(buffer, ElementsAre('t', 'e', 'x', 't'));
}

TEST_F(send, with_std_string_view_should_store_it_as_is) {
    using namespace std::string_view_literals;
    ozo::send(os, oid_map, "view"sv);
    EXPECT_THAT(buffer, ElementsAre('v', 'i', 'e', 'w'));
}

TEST_F(send, with_std_vector_of_float_should_store_with_one_dimension_array_header_and_values) {
    ozo::send(os, oid_map, std::vector<float>({42.13f}));
    EXPECT_EQ(buffer, std::vector<char>({
        0, 0, 0, 1,
        0, 0, 0, 0,
        0, 0, 2, '\xBC',
        0, 0, 0, 1,
        0, 0, 0, 0,
        0, 0, 0, 4,
        0x42, 0x28, char(0x85), 0x1F,
    }));
}

TEST_F(send, should_convert_pg_name_as_string) {
    ozo::send(os, oid_map, ozo::pg::name {"name"});
    EXPECT_THAT(buffer, ElementsAre('n', 'a', 'm', 'e'));
}

TEST_F(send, should_send_nothing_for_std_nullptr_t) {
    ozo::send(os, oid_map, nullptr);
    EXPECT_TRUE(buffer.empty());
}

TEST_F(send, should_send_nothing_for_std_nullopt_t) {
    ozo::send(os, oid_map, __OZO_NULLOPT);
    EXPECT_TRUE(buffer.empty());
}

TEST(send_impl, should_send_nothing_for_std_nullptr_t) {
    std::vector<char> buffer;
    ozo::detail::ostreambuf obuf{buffer};
    ozo::ostream os{&obuf};
    ozo::empty_oid_map oid_map;

    ozo::send_impl<std::nullptr_t>::apply(os, oid_map, nullptr);
    EXPECT_TRUE(buffer.empty());
}

TEST(send_impl, should_send_nothing_for_std_nullopt_t) {
    std::vector<char> buffer;
    ozo::detail::ostreambuf obuf{buffer};
    ozo::ostream os{&obuf};
    ozo::empty_oid_map oid_map;

    ozo::send_impl<__OZO_NULLOPT_T>::apply(os, oid_map, __OZO_NULLOPT);
    EXPECT_TRUE(buffer.empty());
}

} // namespace
