#include <ozo/impl/async_connect.h>
#include "test_error.h"
#include "connection_mock.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace {

namespace hana = boost::hana;
using ozo::empty_oid_map;
using ozo::tests::native_handle;
using callback_mock = ozo::tests::callback_gmock<>;
using ozo::tests::wrap;
using testing::StrictMock;

template <typename OidMap>
struct fixture_impl {
    StrictMock<ozo::tests::connection_gmock> connection{};
    StrictMock<callback_mock> callback{};
    StrictMock<ozo::tests::executor_gmock> io_context{};
    StrictMock<ozo::tests::executor_gmock> strand{};
    StrictMock<ozo::tests::strand_executor_service_gmock> strand_service{};
    StrictMock<ozo::tests::stream_descriptor_gmock> socket{};
    ozo::tests::io_context io{io_context, strand_service};
    decltype(ozo::tests::make_connection(connection, io, socket)) conn =
            ozo::tests::make_connection(connection, io, socket);
};

using fixture = fixture_impl<empty_oid_map>;

template <typename OidMap>
auto make_fixture(OidMap) { return fixture_impl<OidMap>{}; }
using namespace testing;
using ozo::error_code;

struct async_connect : Test {};

TEST_F(async_connect, should_start_connection_assign_socket_and_wait_for_compile) {
    fixture f;
    *(f.conn->handle_) = native_handle::good;

    EXPECT_CALL(f.connection, start_connection("conninfo")).WillOnce(Return(error_code{}));
    EXPECT_CALL(f.connection, assign_socket()).WillOnce(Return(error_code{}));
    EXPECT_CALL(f.socket, async_write_some(_)).WillOnce(Return());
    ozo::impl::make_async_connect_op(f.conn, wrap(f.callback)).perform("conninfo");
}

TEST_F(async_connect, should_call_handler_with_pq_connection_start_failed_on_error_in_start_connection) {
    fixture f;
    *(f.conn->handle_) = native_handle::good;

    EXPECT_CALL(f.connection, start_connection("conninfo"))
        .WillOnce(Return(error_code{ozo::error::pq_connection_start_failed}));

    EXPECT_CALL(f.io_context, post(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(f.callback, context_preserved()).WillOnce(Return());
    EXPECT_CALL(f.callback, call(error_code{ozo::error::pq_connection_start_failed}))
        .WillOnce(Return());

    ozo::impl::make_async_connect_op(f.conn, wrap(f.callback)).perform("conninfo");
}

TEST_F(async_connect, should_call_handler_with_pq_connection_status_bad_if_connection_status_is_bad) {
    fixture f;
    *(f.conn->handle_) = native_handle::bad;

    EXPECT_CALL(f.connection, start_connection("conninfo")).WillOnce(Return(error_code{}));

    EXPECT_CALL(f.io_context, post(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(f.callback, context_preserved()).WillOnce(Return());
    EXPECT_CALL(f.callback, call(error_code{ozo::error::pq_connection_status_bad}))
        .WillOnce(Return());

    ozo::impl::make_async_connect_op(f.conn, wrap(f.callback)).perform("conninfo");
}

TEST_F(async_connect, should_call_handler_with_error_if_assign_socket_returns_error) {
    fixture f;
    *(f.conn->handle_) = native_handle::good;

    EXPECT_CALL(f.connection, start_connection("conninfo")).WillOnce(Return(error_code{}));
    EXPECT_CALL(f.connection, assign_socket()).WillOnce(Return(error_code{ozo::tests::error::error}));

    EXPECT_CALL(f.io_context, post(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(f.callback, context_preserved()).WillOnce(Return());
    EXPECT_CALL(f.callback, call(error_code{ozo::tests::error::error})).WillOnce(Return());

    ozo::impl::make_async_connect_op(f.conn, wrap(f.callback)).perform("conninfo");
}

TEST_F(async_connect, should_wait_for_write_complete_if_connect_poll_returns_PGRES_POLLING_WRITING) {
    fixture f;
    *(f.conn->handle_) = native_handle::good;

    testing::InSequence s;
    EXPECT_CALL(f.connection, start_connection("conninfo")).WillOnce(Return(error_code{}));
    EXPECT_CALL(f.connection, assign_socket()).WillOnce(Return(error_code{}));

    EXPECT_CALL(f.socket, async_write_some(_)).WillOnce(InvokeArgument<0>(error_code{}));
    EXPECT_CALL(f.callback, context_preserved()).WillOnce(Return());

    EXPECT_CALL(f.connection, connect_poll()).WillOnce(Return(PGRES_POLLING_WRITING));

    EXPECT_CALL(f.socket, async_write_some(_)).WillOnce(Return());
    ozo::impl::make_async_connect_op(f.conn, wrap(f.callback)).perform("conninfo");
}

TEST_F(async_connect, should_wait_for_read_complete_if_connect_poll_returns_PGRES_POLLING_READING) {
    fixture f;
    *(f.conn->handle_) = native_handle::good;

    testing::InSequence s;
    EXPECT_CALL(f.connection, start_connection("conninfo")).WillOnce(Return(error_code{}));
    EXPECT_CALL(f.connection, assign_socket()).WillOnce(Return(error_code{}));

    EXPECT_CALL(f.socket, async_write_some(_)).WillOnce(InvokeArgument<0>(error_code{}));
    EXPECT_CALL(f.callback, context_preserved()).WillOnce(Return());

    EXPECT_CALL(f.connection, connect_poll()).WillOnce(Return(PGRES_POLLING_READING));

    EXPECT_CALL(f.socket, async_read_some(_)).WillOnce(Return());
    ozo::impl::make_async_connect_op(f.conn, wrap(f.callback)).perform("conninfo");
}

TEST_F(async_connect, should_call_handler_with_no_error_if_connect_poll_returns_PGRES_POLLING_OK) {
    fixture f;
    *(f.conn->handle_) = native_handle::good;

    testing::InSequence s;
    EXPECT_CALL(f.connection, start_connection("conninfo")).WillOnce(Return(error_code{}));
    EXPECT_CALL(f.connection, assign_socket()).WillOnce(Return(error_code{}));

    EXPECT_CALL(f.socket, async_write_some(_)).WillOnce(InvokeArgument<0>(error_code{}));
    EXPECT_CALL(f.callback, context_preserved()).WillOnce(Return());

    EXPECT_CALL(f.connection, connect_poll()).WillOnce(Return(PGRES_POLLING_OK));

    EXPECT_CALL(f.io_context, post(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(f.callback, context_preserved()).WillOnce(Return());
    EXPECT_CALL(f.callback, call(error_code{})).WillOnce(Return());
    ozo::impl::make_async_connect_op(f.conn, wrap(f.callback)).perform("conninfo");
}

TEST_F(async_connect, should_call_handler_with_pq_connect_poll_failed_if_connect_poll_returns_PGRES_POLLING_FAILED) {
    fixture f;
    *(f.conn->handle_) = native_handle::good;

    testing::InSequence s;
    EXPECT_CALL(f.connection, start_connection("conninfo")).WillOnce(Return(error_code{}));
    EXPECT_CALL(f.connection, assign_socket()).WillOnce(Return(error_code{}));

    EXPECT_CALL(f.socket, async_write_some(_)).WillOnce(InvokeArgument<0>(error_code{}));
    EXPECT_CALL(f.callback, context_preserved()).WillOnce(Return());

    EXPECT_CALL(f.connection, connect_poll()).WillOnce(Return(PGRES_POLLING_FAILED));

    EXPECT_CALL(f.io_context, post(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(f.callback, context_preserved()).WillOnce(Return());
    EXPECT_CALL(f.callback, call(error_code{ozo::error::pq_connect_poll_failed}))
        .WillOnce(Return());
    ozo::impl::make_async_connect_op(f.conn, wrap(f.callback)).perform("conninfo");
}

TEST_F(async_connect, should_call_handler_with_pq_connect_poll_failed_if_connect_poll_returns_PGRES_POLLING_ACTIVE) {
    fixture f;
    *(f.conn->handle_) = native_handle::good;

    testing::InSequence s;
    EXPECT_CALL(f.connection, start_connection("conninfo")).WillOnce(Return(error_code{}));
    EXPECT_CALL(f.connection, assign_socket()).WillOnce(Return(error_code{}));

    EXPECT_CALL(f.socket, async_write_some(_)).WillOnce(InvokeArgument<0>(error_code{}));
    EXPECT_CALL(f.callback, context_preserved()).WillOnce(Return());

    EXPECT_CALL(f.connection, connect_poll()).WillOnce(Return(PGRES_POLLING_ACTIVE));

    EXPECT_CALL(f.io_context, post(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(f.callback, context_preserved()).WillOnce(Return());
    EXPECT_CALL(f.callback, call(error_code{ozo::error::pq_connect_poll_failed}))
        .WillOnce(Return());
    ozo::impl::make_async_connect_op(f.conn, wrap(f.callback)).perform("conninfo");
}

TEST_F(async_connect, should_call_handler_with_the_error_if_polling_operation_invokes_callback_with_it) {
    fixture f;
    *(f.conn->handle_) = native_handle::good;

    testing::InSequence s;
    EXPECT_CALL(f.connection, start_connection("conninfo")).WillOnce(Return(error_code{}));
    EXPECT_CALL(f.connection, assign_socket()).WillOnce(Return(error_code{}));

    EXPECT_CALL(f.socket, async_write_some(_))
        .WillOnce(InvokeArgument<0>(ozo::tests::error::error));
    EXPECT_CALL(f.callback, context_preserved()).WillOnce(Return());

    EXPECT_CALL(f.io_context, post(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(f.callback, context_preserved()).WillOnce(Return());
    EXPECT_CALL(f.callback, call(error_code{ozo::tests::error::error}))
        .WillOnce(Return());
    ozo::impl::make_async_connect_op(f.conn, wrap(f.callback)).perform("conninfo");
}

} // namespace
