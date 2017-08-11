#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTest/CommandLineTestRunner.h"
#include <cstring>
#include "../src/slcan.h"
#include "../src/can_driver.h"
#include "../src/bus_power.h"


extern "C" {
#include <stdint.h>
    size_t slcan_frame_to_ascii(char *buf, const struct can_frame_s *f, bool timestamp);
    void slcan_send_frame(char *line);
    void slcan_decode_line(char *line);
}

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
        .timestamp = 0,
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
        .timestamp = 0,
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
        .timestamp = 0,
        .id = 0x72a,
        .extended = false,
        .remote = true,
        .length = 8,
        .data = {0}
    };
    size_t len = slcan_frame_to_ascii(line, &frame, false);
    const char *expect = "r72a8\r";
    STRCMP_EQUAL(expect, line);
    CHECK_EQUAL(strlen(expect), len);
}

TEST(SlcanTestGroup, CanEncodeExtendedRemoteFrame)
{
    struct can_frame_s frame = {
        .timestamp = 0,
        .id = 0x1234abcd,
        .extended = true,
        .remote = true,
        .length = 4,
        .data = {0}
    };
    size_t len = slcan_frame_to_ascii(line, &frame, false);
    const char *expect = "R1234abcd4\r";
    STRCMP_EQUAL(expect, line);
    CHECK_EQUAL(strlen(expect), len);
}

TEST(SlcanTestGroup, CanEncodeFrameWithTimestamp)
{
    struct can_frame_s frame = {
        .timestamp = 0xdead,
        .id = 0x100,
        .extended = false,
        .remote = false,
        .length = 1,
        .data = {0x2a},
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

TEST(SlcanTestGroup, OpenCommand)
{
    mock().expectOneCall("can_open").withParameter("mode", CAN_MODE_NORMAL);
    strcpy(line, "O\r");
    slcan_decode_line(line);
    STRCMP_EQUAL("\r", line);
}

TEST(SlcanTestGroup, OpenLoopbackCommand)
{
    mock().expectOneCall("can_open").withParameter("mode", CAN_MODE_LOOPBACK);
    strcpy(line, "l\r");
    slcan_decode_line(line);
    STRCMP_EQUAL("\r", line);
}

TEST(SlcanTestGroup, OpenSilentCommand)
{
    mock().expectOneCall("can_open").withParameter("mode", CAN_MODE_SILENT);
    strcpy(line, "L\r");
    slcan_decode_line(line);
    STRCMP_EQUAL("\r", line);
}

TEST(SlcanTestGroup, SetBitrateCommand)
{
    mock().expectOneCall("can_set_bitrate").withParameter("bitrate", 1000000);
    strcpy(line, "S8\r");
    slcan_decode_line(line);
    STRCMP_EQUAL("\r", line);
}

TEST(SlcanTestGroup, CloseCommand)
{
    mock().expectOneCall("can_close");
    strcpy(line, "C\r");
    slcan_decode_line(line);
    STRCMP_EQUAL("\r", line);
}

TEST(SlcanTestGroup, HardwareVersionCommand)
{
    strcpy(line, "V\r");
    slcan_decode_line(line);
    STRCMP_EQUAL("hardware version str\r", line);
}

TEST(SlcanTestGroup, SoftwareVersionCommand)
{
    strcpy(line, "v\r");
    slcan_decode_line(line);
    STRCMP_EQUAL("software version str\r", line);
}

TEST(SlcanTestGroup, PowerOnBusCommand)
{
    mock().expectOneCall("bus_power").withParameter("enable", true);
    strcpy(line, "P\r");
    slcan_decode_line(line);
    STRCMP_EQUAL("\r", line);
}

TEST(SlcanTestGroup, PowerOffBusCommand)
{
    mock().expectOneCall("bus_power").withParameter("enable", false);
    strcpy(line, "p\r");
    slcan_decode_line(line);
    STRCMP_EQUAL("\r", line);
}


int main(int ac, char** av)
{
    return CommandLineTestRunner::RunAllTests(ac, av);
}

// mocks
extern "C" {

const char * software_version_str = "software version str";
const char * hardware_version_str = "hardware version str";

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
    mock().actualCall("can_set_bitrate").withParameter("bitrate", bitrate);
    return true;
}

bool can_open(int mode)
{
    mock().actualCall("can_open").withParameter("mode", mode);
    return true;
}

void can_close(void)
{
    mock().actualCall("can_close");
}

/* dummy functions */

int slcan_serial_write(void *arg, const char *buf, size_t len)
{
    (void) arg;
    (void) buf;
    (void) len;
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

char *slcan_getline(void *arg)
{
    (void) arg;
    return NULL;
}

bool bus_power(bool enable)
{
    mock().actualCall("bus_power").withParameter("enable", enable);
    return true;
}

}
