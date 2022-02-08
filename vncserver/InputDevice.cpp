#include "InputDevice.h"

status_t InputDevice::readFromParcel(Parcel *parcel)
{
    if (parcel->readInt32())
    {
        mId = parcel->readInt32();
        mGeneration = parcel->readInt32();
        mControllerNumber = parcel->readInt32();
        mName = parcel->readString16();
        mVendorId = parcel->readInt32();
        mProductId = parcel->readInt32();
        mDescriptor = parcel->readString16();
        mIsExternal = parcel->readInt32();
        mSources  = parcel->readInt32();
    }

    return NO_ERROR;
}

bool InputDevice::supportsSource(int32_t source)
{
    return (mSources & source) == source;
}

