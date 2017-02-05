#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "../src/slcan.h"
#include "../src/can_driver.h"

TEST_GROUP(SlcanTestGroup)
{
    char line[100];

    void setup()
    {
        memset(line, 0, sizeof(line));
    }

    void teardown()
    {
        mock().checkExpectations();
        mock().clear();
    }
};

TEST(SlcanTestGroup, CanEncodeStandardFrame)
{
    struct can_frame_s frame = {
        .id = 0x72a,
        .extended = false,
        .remote = false,
        .length = 4,
        .data = {0x12, 0x89, 0xab, 0xef}
    };
    size_t len = slcan_frame_to_ascii(line, &frame, false);
    const char *expect = "t72a41289abef\r";
    STRCMP_EQUAL(expect, line);
    CHECK_EQUAL(strlen(expect), len);
}

TEST(SlcanTestGroup, CanEncodeExtendedFrame)
{
    struct can_frame_s frame = {
        .id = 0x1234abcd,
        .extended = true,
        .remote = false,
        .length = 8,
        .data = {0,1,2,3,4,5,6,7}
    };
    size_t len = slcan_frame_to_ascii(line, &frame, false);
    const char *expect = "T1234abcd80001020304050607\r";
    STRCMP_EQUAL(expect, line);
    CHECK_EQUAL(strlen(expect), len);
}

TEST(SlcanTestGroup, CanEncodeStandardRemoteFrame)
{
    struct can_frame_s frame = {
        .id = 0x72a,
        .extended = false,
        .remote = true,
        .length = 8,
    };
    size_t len = slcan_frame_to_ascii(line, &frame, false);
    const char *expect = "r72a8\r";
    STRCMP_EQUAL(expect, line);
    CHECK_EQUAL(strlen(expect), len);
}

TEST(SlcanTestGroup, CanEncodeExtendedRemoteFrame)
{
    struct can_frame_s frame = {
        .id = 0x1234abcd,
        .extended = true,
        .remote = true,
        .length = 4,
    };
    size_t len = slcan_frame_to_ascii(line, &frame, false);
    const char *expect = "R1234abcd4\r";
    STRCMP_EQUAL(expect, line);
    CHECK_EQUAL(strlen(expect), len);
}

TEST(SlcanTestGroup, CanEncodeFrameWithTimestamp)
{
    struct can_frame_s frame = {
        .id = 0x100,
        .extended = false,
        .remote = false,
        .length = 1,
        .data = {0x2a},
        .timestamp = 0xdead
    };
    size_t len = slcan_frame_to_ascii(line, &frame, true);
    const char *expect = "t10012adead\r";
    STRCMP_EQUAL(expect, line);
    CHECK_EQUAL(strlen(expect), len);
}

TEST(SlcanTestGroup, CanDecodeStandardFrame)
{
    uint8_t data[] = {0x2a};
    mock().expectOneCall("can_send")
          .withParameter("id", 0x100)
          .withParameter("extended", false)
          .withParameter("remote", false)
          .withMemoryBufferParameter("data", data, sizeof(data))
          .withParameter("length", sizeof(data));

    strcpy(line, "t10012a\r");
    slcan_send_frame(line);

    STRCMP_EQUAL("\r", line);
}

TEST(SlcanTestGroup, CanDecodeExtendedFrame)
{
    uint8_t data[] = {1,2,3,4,5,6,7,8};
    mock().expectOneCall("can_send")
          .withParameter("id", 0xabcdef)
          .withParameter("extended", true)
          .withParameter("remote", false)
          .withMemoryBufferParameter("data", data, sizeof(data))
          .withParameter("length", sizeof(data));

    strcpy(line, "T00abcdef80102030405060708\r");
    slcan_send_frame(line);

    STRCMP_EQUAL("\r", line);
}

TEST(SlcanTestGroup, CanDecodeExtendedRemoteFrame)
{
    uint8_t data[] = {};
    mock().expectOneCall("can_send")
          .withParameter("id", 0x1000)
          .withParameter("extended", true)
          .withParameter("remote", true)
          .withMemoryBufferParameter("data", data, 0)
          .withParameter("length", 8);

    strcpy(line, "R000010008\r");
    slcan_send_frame(line);

    STRCMP_EQUAL("\r", line);
}

TEST(SlcanTestGroup, CanDecodeStandardRemoteFrame)
{
    uint8_t data[] = {};
    mock().expectOneCall("can_send")
          .withParameter("id", 0x100)
          .withParameter("extended", false)
          .withParameter("remote", true)
          .withMemoryBufferParameter("data", data, 0)
          .withParameter("length", 2);

    strcpy(line, "r1002\r");
    slcan_send_frame(line);

    STRCMP_EQUAL("\r", line);
}

int main(int ac, char** av)
{
    return CommandLineTestRunner::RunAllTests(ac, av);
}

// mocks
extern "C" {

const char * softwar_version_str = "software version str";
const char * hardware_version_str = "hardware version str";

int slcan_serial_get(void *arg)
{
    return 0;
}

int slcan_serial_write(void *arg, const char *buf, size_t len)
{
    return 0;
}

struct can_frame_s *can_receive(void)
{
    return NULL;
}

void can_frame_delete(struct can_frame_s *f)
{
    (void) f;
}

bool can_send(uint32_t id, bool extended, bool remote, uint8_t *data, size_t length)
{
    size_t data_length = remote ? 0 : length;
    mock().actualCall("can_send")
          .withParameter("id", id)
          .withParameter("extended", extended)
          .withParameter("remote", remote)
          .withMemoryBufferParameter("data", data, data_length)
          .withParameter("length", length);
    return true;
}

bool can_set_bitrate(uint32_t bitrate)
{
    return true;
}

void can_silent_mode(bool enable)
{
    (void) enable;
}

void can_loopback_mode(bool enable)
{
    (void) enable;
}

bool can_open(void)
{
    return true;
}

void can_close(void)
{
    return;
}

}
