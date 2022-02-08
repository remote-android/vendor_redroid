/*
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "FrameOutput"
//#define LOG_NDEBUG 0
#include <utils/Log.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <utils/SystemClock.h>

#include "FrameOutput.h"
#include "IInputManager.h"
#include "android_keycode.h"

using namespace android;

static constexpr int kGlBytesPerPixel = 4;      // GL_RGBA

static InputEventFactoryInterface *gInputEventFactory = new PreallocatedInputEventFactory();
static int32_t gTouchDeviceId, gKbdDeviceId;

/* Here we create a structure so that every client has its own pointer */
typedef struct ClientData {
    int32_t buttonMask;
    int32_t oldx,oldy;
    int32_t meta;
} ClientData;

static void clientgone(rfbClientPtr cl)
{
    free(cl->clientData);
    cl->clientData = NULL;
}

static enum rfbNewClientAction newclient(rfbClientPtr cl)
{
    ALOGD("new VNC client connected");
    connectService(); // connect to Input Manager Service

    gTouchDeviceId = getInputDeviceId(InputDevice::SOURCE_TOUCHSCREEN);
    ALOGD("gTouchDeviceId: %d", gTouchDeviceId);

    gKbdDeviceId = getInputDeviceId(InputDevice::SOURCE_KEYBOARD);
    ALOGD("gKbdDeviceId: %d", gKbdDeviceId);

    cl->clientData = (void*)calloc(sizeof(ClientData),1);
    cl->clientGoneHook = clientgone;
    return RFB_CLIENT_ACCEPT;
}

static void doptr(int buttonMask, int x, int y, rfbClientPtr cl)
{
    ALOGV("doptr(), buttonMask: %d, x: %d, y: %d", buttonMask, x, y);

    static PointerCoords gPointer;
    static PointerProperties gPp;

    gPp.clear();
    gPp.id = 0;
    gPointer.clear();

    ClientData *data = (ClientData *) (cl->clientData);
    uint64_t now = systemTime(SYSTEM_TIME_MONOTONIC);
    MotionEvent *ev = gInputEventFactory->createMotionEvent();
    int32_t action = 0, actionButton = 0, buttonState = 0;
    if (buttonMask)
    {
        if (buttonMask & (rfbButton1Mask | rfbButton2Mask | rfbButton3Mask))
        {
            // button pressed
            if (data->buttonMask & buttonMask & rfbButton1Mask)
            {
                ALOGV("mouse moving...");
                action = AMOTION_EVENT_ACTION_MOVE;
            } else {
                ALOGV("mouse pressed down");
                action = AMOTION_EVENT_ACTION_DOWN;
                actionButton = AMOTION_EVENT_ACTION_BUTTON_PRESS;
                if (buttonMask & rfbButton1Mask) buttonState = AMOTION_EVENT_BUTTON_PRIMARY;
                else if (buttonMask & rfbButton2Mask) buttonState = AMOTION_EVENT_BUTTON_TERTIARY;
                else buttonState = AMOTION_EVENT_BUTTON_SECONDARY;
            }
            gPointer.setAxisValue(AMOTION_EVENT_AXIS_X, x);
            gPointer.setAxisValue(AMOTION_EVENT_AXIS_Y, y);
            gPointer.setAxisValue(AMOTION_EVENT_AXIS_PRESSURE, 1.0f);
            gPointer.setAxisValue(AMOTION_EVENT_AXIS_SIZE, 1.0f);
        } else if (buttonMask & (rfbButton4Mask | rfbButton5Mask)) {
            ALOGV("mouse scrolling...");
            action = AMOTION_EVENT_ACTION_SCROLL;
            gPointer.setAxisValue(AMOTION_EVENT_AXIS_X, x);
            gPointer.setAxisValue(AMOTION_EVENT_AXIS_Y, y);
            gPointer.setAxisValue(AMOTION_EVENT_AXIS_PRESSURE, 0.0f);
            gPointer.setAxisValue(AMOTION_EVENT_AXIS_SIZE, 1.0f);
            gPointer.setAxisValue(AMOTION_EVENT_AXIS_VSCROLL, (buttonMask & rfbButton4Mask) ? 1.0f : -1.0f);
        } else {
            ALOGW("unporcessed mouse buttonMask: %d", buttonMask);
            goto end;
        }
    } else {
        // button released or mouse hover
        if (data->buttonMask)
        {
            if (data->buttonMask & (rfbButton1Mask | rfbButton2Mask | rfbButton3Mask))
            {
                ALOGV("mouse released");
                action = AMOTION_EVENT_ACTION_UP;
                actionButton = AMOTION_EVENT_ACTION_BUTTON_RELEASE;
                gPointer.setAxisValue(AMOTION_EVENT_AXIS_X, x);
                gPointer.setAxisValue(AMOTION_EVENT_AXIS_Y, y);
                gPointer.setAxisValue(AMOTION_EVENT_AXIS_PRESSURE, 0.0f);
                gPointer.setAxisValue(AMOTION_EVENT_AXIS_SIZE, 1.0f);
            } else if (data->buttonMask & (rfbButton4Mask | rfbButton5Mask)) {
                // scroll step, just ignore
                goto end;
            } else {
                ALOGW("unporcessed mouse buttonMask: %d", data->buttonMask);
                goto end;
            }
        } else {
            // mouse hover
            // action = AMOTION_EVENT_ACTION_HOVER_MOVE;
            // gPointer.setAxisValue(AMOTION_EVENT_AXIS_X, x);
            // gPointer.setAxisValue(AMOTION_EVENT_AXIS_Y, y);
            // gPointer.setAxisValue(AMOTION_EVENT_AXIS_PRESSURE, 0.0f);
            // gPointer.setAxisValue(AMOTION_EVENT_AXIS_SIZE, 1.0f);
            goto end;
        }
    }

    ev->initialize(
            0/*id*/,
            gTouchDeviceId /*deviceId*/,
            InputDevice::SOURCE_TOUCHSCREEN /*source*/,
            0 /*displayId*/,
            { {} } /*std::array<uint8_t, 32> hmac*/,
            action /*action*/,
            actionButton /*actionButton*/,
            0 /*flags*/,
            0 /*edgeFlags*/,
            0 /*metaState*/,
            buttonState /*buttonState*/,
            MotionClassification::NONE /*classification*/,
            0.0f /*xScale*/,
            0.0f /*yScale*/,
            0.0f /*xOffset*/,
            0.0f /*yOffset*/,
            1.0f /*xPrecision*/,
            1.0f /*yPrecision*/,
            0.0f /*rawXCursorPosition*/,
            0.0f /*rawYCursorPosition*/,
            now /*downTime*/,
            now /*eventTime*/,
            1 /*pointerCount*/,
            &gPp /*pointerProperties*/,
            &gPointer/*pointerCoords*/);

    if (!injectInputEvent(*ev) && !isConnected())
    {
        connectService();
    }
   
end: 
    data->buttonMask = buttonMask;
}

static int getKeyCode(rfbBool /*down*/, rfbKeySym key, rfbClientPtr cl)
{
    int code = (int) key;
    ClientData *pData = (ClientData *) (cl->clientData);
    if (code >= '0' && code <= '9') {
        return KEYCODE_0 + (code - '0');
    } else if (code >= 'A' && code <= 'Z') {
        // CAPS on or SHIFT down
        (pData->meta & META_SHIFT_ON) || (pData->meta |= META_CAP_LOCKED); // CAPS on
        return KEYCODE_A + (code - 'A');
    } else if (code >= 'a' && code <= 'z') {
        pData->meta &= ~META_CAP_LOCKED; // CAPS off
        return KEYCODE_A + (code - 'a');
    } else if (code >= 0xFF50 && code <= 0xFF57) {
        static const uint16_t map[] =
        { 
            KEYCODE_MOVE_HOME, KEYCODE_DPAD_LEFT, KEYCODE_DPAD_UP, KEYCODE_DPAD_RIGHT, KEYCODE_DPAD_DOWN,
            KEYCODE_PAGE_UP, KEYCODE_PAGE_DOWN, KEYCODE_MOVE_END,
        };
        return map[code - 0xFF50];
    } else if (code >= 0xFFBE && code <= 0xFFC9) {
        return KEYCODE_F1 + (code - 0xFFBE);
    } else if (code >= 0xFFE1 && code <= 0xFFE4) {
        static const uint16_t map[] =
        {   KEYCODE_SHIFT_LEFT, KEYCODE_SHIFT_RIGHT,
            KEYCODE_CTRL_LEFT, KEYCODE_CTRL_RIGHT,
        };
        return map[code - 0xFFE1];
    } else {
        switch (code) {
            case 0x0020:    return KEYCODE_SPACE;
            case 0x0021:    return KEYCODE_1; // shift
            case 0x0022:    return KEYCODE_APOSTROPHE; // shift
            case 0x0023:    return KEYCODE_2; // shift
            case 0x0024:    return KEYCODE_4; // shift
            case 0x0025:    return KEYCODE_5; // shift
            case 0x0026:    return KEYCODE_7; // shift
            case 0x0027:    return KEYCODE_APOSTROPHE;
            case 0x0028:    return KEYCODE_9; // shift
            case 0x0029:    return KEYCODE_0; // shift
            case 0x002a:    return KEYCODE_8; // shift
            case 0x002b:    return KEYCODE_EQUALS; // shift
            case 0x002C:    return KEYCODE_COMMA;
            case 0x002d:    return KEYCODE_MINUS;
            case 0x002E:    return KEYCODE_PERIOD;
            case 0x002F:    return KEYCODE_SLASH;
            case 0x003a:    return KEYCODE_SEMICOLON; // shift
            case 0x003b:    return KEYCODE_SEMICOLON;
            case 0x003c:    return KEYCODE_COMMA; // shift
            case 0x003d:    return KEYCODE_EQUALS;
            case 0x003e:    return KEYCODE_PERIOD; // shift
            case 0x003f:    return KEYCODE_SLASH; // shift
            case 0x0040:    return KEYCODE_2; // shift
            case 0x005b:    return KEYCODE_LEFT_BRACKET;
            case 0x005c:    return KEYCODE_BACKSLASH;
            case 0x005d:    return KEYCODE_RIGHT_BRACKET;
            case 0x005e:    return KEYCODE_6; // shift
            case 0x005f:    return KEYCODE_MINUS; // shift
            case 0x0060:    return KEYCODE_GRAVE;
            case 0x007b:    return KEYCODE_LEFT_BRACKET; // shift
            case 0x007c:    return KEYCODE_BACKSLASH; // shift
            case 0x007d:    return KEYCODE_RIGHT_BRACKET; // shift
            case 0x007e:    return KEYCODE_GRAVE; // shift
            case 0xfe03:    return KEYCODE_ALT_LEFT;
            case 0xFF08:    return KEYCODE_DEL;
            case 0xFF1B:    return /*KEYCODE_ESCAPE*/ KEYCODE_BACK;
            case 0xFF09:    return KEYCODE_TAB;
            case 0xFF0D:    return KEYCODE_ENTER;
            case 0xff6a:    return KEYCODE_INSERT;
            case 0xff7e:    return KEYCODE_ALT_RIGHT;
            case 0xffe9:    return /*WIN key*/ KEYCODE_HOME;
            case 0xffff:    return KEYCODE_FORWARD_DEL;
            default: ALOGW("unprocess key code: %04x", code); break;
        }
    }
    return KEYCODE_UNKNOWN;
}

static void dokey(rfbBool down, rfbKeySym key, rfbClientPtr cl)
{
    ALOGV("dokey(): Got keysym: %04x (down=%d)", (unsigned int)key, (int)down);

    if (int32_t keyCode = getKeyCode(down, key, cl))
    {
        ALOGV("dokey(): keyCode: %d", keyCode);

        // update meta state
        ClientData *pData = (ClientData *) (cl->clientData);
        switch (keyCode)
        {
            case KEYCODE_ALT_LEFT:
            case KEYCODE_ALT_RIGHT:
                down ? pData->meta |= META_ALT_ON : pData->meta &= ~META_ALT_ON;
                return;
            case KEYCODE_SHIFT_LEFT:
            case KEYCODE_SHIFT_RIGHT:
                down ? pData->meta |= META_SHIFT_ON : pData->meta &= ~META_SHIFT_ON;
                return;
            case KEYCODE_CTRL_LEFT:
            case KEYCODE_CTRL_RIGHT:
                down ? pData->meta |= META_CTRL_ON : pData->meta &= ~META_CTRL_ON;
                return;
        }

        KeyEvent *ev = gInputEventFactory->createKeyEvent();
        uint64_t now = uptimeMillis();
        ev->initialize(
                0/*id*/,
                gKbdDeviceId /*deviceId*/,
                InputDevice::SOURCE_KEYBOARD /*source*/,
                0 /*displayId*/,
                { {} } /*std::array<uint8_t, 32> hmac*/,
                down ? AKEY_EVENT_ACTION_DOWN : AKEY_EVENT_ACTION_UP /* action*/,
                0 /*flags*/,
                keyCode /*keyCode*/,
                0 /*scanCode*/,
                pData->meta /*metaState*/,
                0 /*repeatCount*/,
                now /*downTime*/,
                now /*eventTime*/);
        if (!injectInputEvent(*ev) && !isConnected())
        {
            connectService();
        }
    }
}

status_t FrameOutput::createInputSurface(int width, int height,
        sp<IGraphicBufferProducer>* pBufferProducer) {
    status_t err;

    err = mEglWindow.createPbuffer(width, height);
    if (err != NO_ERROR) {
        return err;
    }
    mEglWindow.makeCurrent();

    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // Shader for rendering the external texture.
    err = mExtTexProgram.setup(Program::PROGRAM_EXTERNAL_TEXTURE);
    if (err != NO_ERROR) {
        return err;
    }

    // Input side (buffers from virtual display).
    glGenTextures(1, &mExtTextureName);
    if (mExtTextureName == 0) {
        ALOGE("glGenTextures failed: %#x", glGetError());
        return UNKNOWN_ERROR;
    }

    sp<IGraphicBufferProducer> producer;
    sp<IGraphicBufferConsumer> consumer;
    BufferQueue::createBufferQueue(&producer, &consumer);
    mGlConsumer = new GLConsumer(consumer, mExtTextureName,
            GL_TEXTURE_EXTERNAL_OES, true, false);
    mGlConsumer->setName(String8("virtual display"));
    mGlConsumer->setDefaultBufferSize(width, height);
    producer->setMaxDequeuedBufferCount(4);
    mGlConsumer->setConsumerUsageBits(GRALLOC_USAGE_HW_TEXTURE); // use SW for performance?

    mGlConsumer->setFrameAvailableListener(this);

    mPixelBuf = new uint8_t[width * height * kGlBytesPerPixel];

    createRfbScreen(width, height);

    *pBufferProducer = producer;

    ALOGD("FrameOutput::createInputSurface OK");
    return NO_ERROR;
}

status_t FrameOutput::copyFrame(long timeoutUsec) {
    Mutex::Autolock _l(mMutex);

    if (!mFrameAvailable) {
        nsecs_t timeoutNsec = (nsecs_t)timeoutUsec * 1000;
        int cc = mEventCond.waitRelative(mMutex, timeoutNsec);
        if (cc == -ETIMEDOUT) {
            ALOGV("cond wait timed out");
            return ETIMEDOUT;
        } else if (cc != 0) {
            ALOGW("cond wait returned error %d", cc);
            return cc;
        }
    }
    if (!mFrameAvailable) {
        // This happens when Ctrl-C is hit.  Apparently POSIX says that the
        // pthread wait call doesn't return EINTR, treating this instead as
        // an instance of a "spurious wakeup".  We didn't get a frame, so
        // we just treat it as a timeout.
        return ETIMEDOUT;
    }

    // A frame is available.  Clear the flag for the next round.
    mFrameAvailable = false;

    float texMatrix[16];
    mGlConsumer->updateTexImage();
    mGlConsumer->getTransformMatrix(texMatrix);

    // The data is in an external texture, so we need to render it to the
    // pbuffer to get access to RGB pixel data.  We also want to flip it
    // upside-down for easy conversion to a bitmap.
    int width = mEglWindow.getWidth();
    int height = mEglWindow.getHeight();
    status_t err = mExtTexProgram.blit(mExtTextureName, texMatrix, 0, 0,
            width, height, true);
    if (err != NO_ERROR) {
        return err;
    }

    // GLES only guarantees that glReadPixels() will work with GL_RGBA, so we
    // need to get 4 bytes/pixel and reduce it.  Depending on the size of the
    // screen and the device capabilities, this can take a while.
    GLenum glErr;
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, mPixelBuf);
    if ((glErr = glGetError()) != GL_NO_ERROR) {
        ALOGE("glReadPixels failed: %#x", glErr);
        return UNKNOWN_ERROR;
    }

    rfbMarkRectAsModified(mRfbScreen,0,0,width,height);

    return NO_ERROR;
}

// Callback; executes on arbitrary thread.
void FrameOutput::onFrameAvailable(const BufferItem& /*item*/) {
    Mutex::Autolock _l(mMutex);
    mFrameAvailable = true;
    mEventCond.signal();
}

void FrameOutput::createRfbScreen(int w, int h) {
    rfbScreenInfoPtr rfbScreen = rfbGetScreen(nullptr, nullptr, w ,h ,8 ,3 ,4);
    if(!rfbScreen) {
        ALOGE("create RFB screen Failed!");
        return;
    }
    rfbScreen->desktopName = "ReDroid";
    rfbScreen->frameBuffer = (char *) mPixelBuf;
    rfbScreen->alwaysShared = TRUE;
    rfbScreen->ptrAddEvent = doptr;
    rfbScreen->kbdAddEvent = dokey;
    rfbScreen->newClientHook = newclient;
    //  rfbScreen->httpDir = "../webclients";
    //  rfbScreen->httpEnableProxyConnect = TRUE;

    rfbInitServer(rfbScreen);

    mRfbScreen = rfbScreen;

    rfbRunEventLoop(rfbScreen, -1, TRUE);

    ALOGD("start RFB loop in background.");
}
