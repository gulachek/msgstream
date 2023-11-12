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

BOOST_AUTO_TEST_SUITE(header_size)

#define EXPAND(X) X

#define DO_TEST(BUF_SZ, RET)                                                   \
  auto b##BUF_SZ = msgstream_header_size(BUF_SZ, NULL);                        \
  BOOST_TEST(EXPAND(b##BUF_SZ) == RET);

BOOST_AUTO_TEST_CASE(EmptyBufferIsMeaninglessAndErrors) {
  auto ret = msgstream_header_size(0, NULL);
  BOOST_TEST(ret < 0);
}

BOOST_AUTO_TEST_CASE(TwoByteHeader){DO_TEST(0x1, 2) DO_TEST(0xff, 2)}

BOOST_AUTO_TEST_CASE(ThreeByteHeader){DO_TEST(0x100, 3) DO_TEST(0xffff, 3)}

BOOST_AUTO_TEST_CASE(FourByteHeader){DO_TEST(0x10000, 4) DO_TEST(0xffffff, 4)}

BOOST_AUTO_TEST_CASE(FiveByteHeader){DO_TEST(0x1000000, 5)
                                         DO_TEST(0xffffffff, 5)}

BOOST_AUTO_TEST_CASE(SixByteHeader){DO_TEST(0x100000000, 6)
                                        DO_TEST(0xffffffffff, 6)}

BOOST_AUTO_TEST_CASE(SevenByteHeader){DO_TEST(0x10000000000, 7)
                                          DO_TEST(0xffffffffffff, 7)}

BOOST_AUTO_TEST_CASE(EightByteHeader){DO_TEST(0x1000000000000, 8)
                                          DO_TEST(0xffffffffffffff, 8)}

BOOST_AUTO_TEST_CASE(NineByteHeader){DO_TEST(0x100000000000000, 9)
                                         DO_TEST(0xffffffffffffffff, 9)}

#undef EXPAND
#undef DO_TEST

BOOST_AUTO_TEST_SUITE_END() // header_size

    BOOST_AUTO_TEST_SUITE(encode_header)

        BOOST_AUTO_TEST_CASE(EmptyBufferIsMeaninglessAndErrors) {
  uint8_t header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  auto ret =
      msgstream_encode_header(header_buf, sizeof(header_buf), 0, 0, NULL);
  BOOST_TEST(ret < 0);
}

BOOST_AUTO_TEST_CASE(MsgSizeLargerThanBufSizeIsError) {
  uint8_t header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  auto ret =
      msgstream_encode_header(header_buf, sizeof(header_buf), 1, 2, NULL);
  BOOST_TEST(ret < 0);
}

BOOST_AUTO_TEST_CASE(HeaderBufTooSmallIsError) {
  uint8_t header_buf[1]; // too small for 2 byte header
  auto ret =
      msgstream_encode_header(header_buf, sizeof(header_buf), 1, 1, NULL);
  BOOST_TEST(ret < 0);
}

BOOST_AUTO_TEST_CASE(EncodesSingleByteMessageHeaderAsTwoBytes) {
  char header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  auto ret =
      msgstream_encode_header(header_buf, sizeof(header_buf), 1, 1, NULL);
  BOOST_TEST(ret == 2);

  std::string_view header{header_buf, header_buf + ret};
  BOOST_TEST(header == "\x02\x01");
}

BOOST_AUTO_TEST_CASE(EncodesMultiByteHeaderWithLittleEndianMsgSize) {
  char header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  auto ret = msgstream_encode_header(header_buf, sizeof(header_buf), 0xffffffff,
                                     0x04030201, NULL);
  BOOST_TEST(ret == 5);

  std::string_view header{header_buf, header_buf + ret};
  BOOST_TEST(header == "\x05\x01\x02\x03\x04");
}

BOOST_AUTO_TEST_CASE(EncodesNineByteHeaderWithLittleEndianMsgSize) {
  char header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  auto ret =
      msgstream_encode_header(header_buf, sizeof(header_buf),
                              0xffffffffffffffff, 0x0807060504030201, NULL);
  BOOST_TEST(ret == 9);

  std::string_view header{header_buf, header_buf + ret};
  BOOST_TEST(header == "\x09\x01\x02\x03\x04\x05\x06\x07\x08");
}
BOOST_AUTO_TEST_SUITE_END() // encode_header
