// SPDX-License-Identifier: Apache-2.0

#include <android/hardware_buffer.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>

TEST(GrallocRedroid, AllocateCpuBlobBuffer) {
    AHardwareBuffer_Desc description = {};
    description.width = 12345;
    description.height = 1;
    description.layers = 1;
    description.format = AHARDWAREBUFFER_FORMAT_BLOB;
    description.usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN |
                        AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;

    AHardwareBuffer* allocated = nullptr;
    int result = AHardwareBuffer_allocate(&description, &allocated);
    std::unique_ptr<AHardwareBuffer, decltype(&AHardwareBuffer_release)> buffer(
            allocated, AHardwareBuffer_release);
    ASSERT_EQ(0, result);

    AHardwareBuffer_Desc actual = {};
    AHardwareBuffer_describe(allocated, &actual);
    EXPECT_EQ(description.width, actual.width);
    EXPECT_EQ(description.width, actual.stride);

    void* data = nullptr;
    result = AHardwareBuffer_lock(allocated, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
                                 -1, nullptr, &data);
    ASSERT_EQ(0, result);
    uint8_t* bytes = static_cast<uint8_t*>(data);
    bytes[0] = 0x12;
    bytes[description.width - 1] = 0x34;
    result = AHardwareBuffer_unlock(allocated, nullptr);
    ASSERT_EQ(0, result);

    result = AHardwareBuffer_lock(allocated, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,
                                 -1, nullptr, &data);
    ASSERT_EQ(0, result);
    bytes = static_cast<uint8_t*>(data);
    EXPECT_EQ(0x12, bytes[0]);
    EXPECT_EQ(0x34, bytes[description.width - 1]);
    result = AHardwareBuffer_unlock(allocated, nullptr);
    EXPECT_EQ(0, result);
}
