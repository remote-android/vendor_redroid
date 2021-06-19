#ifndef IINPUT_MANAGER_H
#define IINPUT_MANAGER_H

#include <binder/IInterface.h>
#include <input/Input.h>

#include "./InputDevice.h"

using namespace android;

class IInputManager: public android::IInterface
{
    public:
        DECLARE_META_INTERFACE(InputManager);

        virtual InputDevice getInputDevice(int32_t deviceId) = 0;

        virtual status_t getInputDeviceIds(/*out*/int32_t *count, /*out*/int32_t *ids) = 0;

        virtual bool injectInputEvent(InputEvent &ev, int mode) = 0;

        enum {
            GET_INPUT_DEVICE = android::IBinder::FIRST_CALL_TRANSACTION,
            GET_INPUT_DEVICE_IDS,
            INJECT_INPUT_EVENT = android::IBinder::FIRST_CALL_TRANSACTION + 7,
        };
};

bool isConnected();

bool connectService();

bool injectInputEvent(InputEvent &ev, int mode = 2);

int32_t getInputDeviceId(int32_t source);

#endif // IINPUT_MANAGER_H

