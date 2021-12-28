#define LOG_TAG "InputManager"

#include <mutex>

#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <utils/Log.h>

#include "IInputManager.h"

#define UNUSED __attribute__((__unused__))

constexpr int PARCEL_TOKEN_MOTION_EVENT = 1;
constexpr int PARCEL_TOKEN_KEY_EVENT = 2;

constexpr int32_t MAX_INPUT_DEVICE_COUNT = 32;

class BpInputManager: public android::BpInterface<IInputManager>
{
    public:
        explicit BpInputManager(const android::sp<android::IBinder> &impl)
            : android::BpInterface<IInputManager>(impl)
        {
        }

        virtual InputDevice getInputDevice(int32_t deviceId) override
        {
            android::Parcel data, reply;
            data.writeInterfaceToken(IInputManager::getInterfaceDescriptor());
            data.writeInt32(deviceId);
            remote()->transact(GET_INPUT_DEVICE, data, &reply);
            // TODO check error
            InputDevice device;
            if (reply.readInt32()) device.readFromParcel(&reply);
            return device;
        }

        virtual status_t getInputDeviceIds(/*out*/int32_t *count, /*out*/int32_t *ids) override
        {
            android::Parcel data, reply;
            data.writeInterfaceToken(IInputManager::getInterfaceDescriptor());
            remote()->transact(GET_INPUT_DEVICE_IDS, data, &reply);
            *count = reply.readInt32();
            for (int i = 0; i < *count && i < MAX_INPUT_DEVICE_COUNT; ++i)
            {
                ids[i] = reply.readInt32();
            }
            return NO_ERROR;
        }

        virtual bool injectInputEvent(InputEvent &ev, int mode) override
        {
            android::Parcel data, reply;
            data.writeInterfaceToken(IInputManager::getInterfaceDescriptor());
            data.writeInt32(1); // prepare write object
            const int type = ev.getType();
            switch(type)
            {
                case AINPUT_EVENT_TYPE_KEY:
                    writeKeyEventToParcel((KeyEvent &) ev, data);
                    break;
                case AINPUT_EVENT_TYPE_MOTION:
                    writeMotionEventToParcel((MotionEvent &) ev, data);
                    break;
                default:
                    ALOGE("unknown input type: %d", type);
                    break;
            }
            data.writeInt32(mode);
            remote()->transact(INJECT_INPUT_EVENT, data, &reply);
            return reply.readInt32();
        }

    private:
        void writeKeyEventToParcel(KeyEvent& ev, Parcel& data)
        {
            data.writeInt32(PARCEL_TOKEN_KEY_EVENT);

            data.writeInt32(ev.getId());
            data.writeInt32(ev.getDeviceId());
            data.writeInt32(ev.getSource());
            data.writeInt32(ev.getDisplayId());
            data.writeByteVector(std::vector<uint8_t>(ev.getHmac().begin(), ev.getHmac().end()));
            data.writeInt32(ev.getAction());
            data.writeInt32(ev.getKeyCode());
            data.writeInt32(ev.getRepeatCount());
            data.writeInt32(ev.getMetaState());
            data.writeInt32(ev.getScanCode());
            data.writeInt32(ev.getFlags());
            data.writeInt64(ev.getDownTime());
            data.writeInt64(ev.getEventTime());
        }

        void writeMotionEventToParcel(MotionEvent& ev, Parcel& data)
        {
            data.writeInt32(PARCEL_TOKEN_MOTION_EVENT);
            ev.writeToParcel(&data);
        }
};

IMPLEMENT_META_INTERFACE(InputManager, "android.hardware.input.IInputManager");

static android::sp<IInputManager> gInputManagerService;
static std::mutex gMutex;

class InputDeathRecipient : public android::IBinder::DeathRecipient
{
    public:
        virtual void binderDied(const android::wp<android::IBinder> &who UNUSED) override
        {
            ALOGW("Input Manager Service disconnected");
            std::lock_guard<std::mutex> _l(gMutex);
            gInputManagerService = nullptr;
        }
};

static android::sp<InputDeathRecipient> gDeathNotifier(new InputDeathRecipient);

bool connectService()
{
    android::sp<android::IServiceManager> sm = android::defaultServiceManager();
    android::sp<android::IBinder> b = sm->getService(android::String16("input"));
    if (b != nullptr)
    {
        ALOGI("input manager service connected");
        b->linkToDeath(gDeathNotifier, (void *) "remoted");
        std::lock_guard<std::mutex> _l(gMutex);
        gInputManagerService = android::interface_cast<IInputManager>(b);
        return true;
    } else {
        ALOGE("get input manager service failed");
        return false;
    }
}

bool isConnected()
{
    std::lock_guard<std::mutex> _l(gMutex);
    return gInputManagerService != nullptr;
}

bool injectInputEvent(InputEvent &ev, int mode)
{
    std::lock_guard<std::mutex> _l(gMutex);
    return gInputManagerService != nullptr && gInputManagerService->injectInputEvent(ev, mode);
}

int32_t getInputDeviceId(int32_t source)
{
    std::lock_guard<std::mutex> _l(gMutex);
    if (gInputManagerService == nullptr) return -1;

    constexpr int32_t DEFAULT_DEVICE_ID = 0;
    int32_t ids[MAX_INPUT_DEVICE_COUNT], count;
    gInputManagerService->getInputDeviceIds(&count, ids);
    for (int i = 0; i < count && i < MAX_INPUT_DEVICE_COUNT; ++i)
    {
        InputDevice device = gInputManagerService->getInputDevice(ids[i]);
        if (device.supportsSource(source)) return ids[i];
    }
    return DEFAULT_DEVICE_ID;
}

