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

int main(int ac, char** av)
{
    return CommandLineTestRunner::RunAllTests(ac, av);
}

// mocks
extern "C" {

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

bool can_send(uint32_t id, bool extended, bool remote, void *data, size_t length)
{
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
