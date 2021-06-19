#ifndef INPUT_DEVICE_H
#define INPUT_DEVICE_H

#include <binder/Parcel.h>
#include <utils/Errors.h>
#include <utils/String16.h>

using namespace android;

class InputDevice {
    public:
        static constexpr int32_t SOURCE_CLASS_BUTTON = 0x00000001;
        static constexpr int32_t SOURCE_CLASS_POINTER = 0x00000002;
        static constexpr int32_t SOURCE_TOUCHSCREEN = 0x00001000 | SOURCE_CLASS_POINTER;
        static constexpr int32_t SOURCE_KEYBOARD = 0x00000100 | SOURCE_CLASS_BUTTON;

        status_t readFromParcel(Parcel* parcel);
        bool supportsSource(int32_t source);

    private:
        int32_t mId;
        int32_t mGeneration;
        int32_t mControllerNumber;
        String16 mName;
        int32_t mVendorId;
        int32_t mProductId;
        String16 mDescriptor;
        int32_t mIsExternal;
        int32_t mSources;
};

#endif // INPUT_DEVICE_H

