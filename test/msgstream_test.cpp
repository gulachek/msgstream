#define BOOST_TEST_MODULE example
#include <boost/test/unit_test.hpp>

#include "msgstream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string_view>
#include <thread>

struct f {
  f() {
    int fds[2];
    if (pipe(fds) == -1) {
      perror("pipe");
      BOOST_FAIL("Failed to allocate pipe");
    }

    read_ = fds[0];
    write_ = fds[1];
  }

  ~f() {
    close(read_);
    close(write_);
  }

  int read_;
  int write_;
};

BOOST_FIXTURE_TEST_CASE(TransferSingleByte, f) {
  int8_t b42 = 42, recv = 0;
  auto sret = msgstream_send(write_, &b42, sizeof(b42), 1, NULL);

  auto rret = msgstream_recv(read_, &recv, sizeof(recv), NULL);

  BOOST_TEST(sret == 1);
  BOOST_TEST(rret == 1);
  BOOST_TEST(recv == 42);
}

BOOST_FIXTURE_TEST_CASE(EofIsDistinctError, f) {
  int8_t recv = 0;
  close(write_);
  auto rret = msgstream_recv(read_, &recv, sizeof(recv), NULL);

  BOOST_TEST(rret == MSGSTREAM_EOF);
  BOOST_TEST(rret < 0);
}

BOOST_FIXTURE_TEST_CASE(TransferEmptyMsg, f) {
  int8_t b42 = 42, recv = 96;
  auto sret = msgstream_send(write_, &b42, sizeof(b42), 0, NULL);

  auto rret = msgstream_recv(read_, &recv, sizeof(recv), NULL);

  BOOST_TEST(sret == 0);
  BOOST_TEST(rret == 0);
  BOOST_TEST(recv == 96); // no change
}

BOOST_FIXTURE_TEST_CASE(TransferStringHello, f) {
  char hello[9] = "hello", recv[9] = {};
  size_t len = strlen(hello);

  auto sret = msgstream_send(write_, hello, sizeof(hello), len, NULL);

  auto rret = msgstream_recv(read_, recv, sizeof(recv), NULL);

  BOOST_TEST(sret == len);
  BOOST_TEST(rret == len);

  std::string_view recv_sv{recv, recv + rret};
  BOOST_TEST(recv_sv == "hello");
}

#undef HUGE
#define HUGE 0x12345

BOOST_FIXTURE_TEST_CASE(TransferHugeMessage, f) {
  uint8_t huge[HUGE], recv[HUGE] = {};

  for (size_t i = 0; i < HUGE; ++i)
    huge[i] = i % 256;

  msgstream_size sret = 0;
  std::thread th{
      [&] { sret = msgstream_send(write_, huge, HUGE, HUGE, stderr); }};

  auto rret = msgstream_recv(read_, recv, HUGE, stderr);
  th.join();

  BOOST_TEST(sret == HUGE);
  BOOST_TEST(rret == HUGE);

  for (size_t i = 0; i < HUGE; ++i) {
    if (recv[i] != (i % 256)) {
      std::ostringstream os;
      os << "Failed to read byte at index " << i << ". Expected " << (i % 256)
         << " but received " << recv[i];

      BOOST_FAIL(os.str());
    }
  }
}
