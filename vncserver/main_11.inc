/*
 * Copyright 2013 The Android Open Source Project
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

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <termios.h>
#include <unistd.h>

#define LOG_TAG "VncServer"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0
#include <utils/Log.h>

#include <binder/IPCThreadState.h>
#include <utils/Errors.h>
#include <utils/SystemClock.h>
#include <utils/Timers.h>
#include <utils/Trace.h>

#include <gui/ISurfaceComposer.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include <ui/DisplayConfig.h>
#include <ui/DisplayState.h>

#include "FrameOutput.h"

using android::DisplayConfig;
using android::FrameOutput;
using android::IBinder;
using android::IGraphicBufferProducer;
using android::ISurfaceComposer;
using android::PhysicalDisplayId;
using android::ProcessState;
using android::Rect;
using android::String8;
using android::SurfaceComposerClient;
using android::Vector;
using android::sp;
using android::status_t;

using android::INVALID_OPERATION;
using android::NAME_NOT_FOUND;
using android::NO_ERROR;
using android::UNKNOWN_ERROR;

namespace ui = android::ui;

#define UNUSED __attribute__((__unused__))

static bool gVerbose = false;           // chatty on stdout
static bool gRotate = false;            // rotate 90 degrees
static uint32_t gVideoWidth = 0;        // default width+height
static uint32_t gVideoHeight = 0;
static PhysicalDisplayId gPhysicalDisplayId;

// Set by signal handler to stop recording.
static volatile bool gStopRequested = false;

static inline uint32_t floorToEven(uint32_t num) {
    return num & ~1;
}

/*
 * Sets the display projection, based on the display dimensions, video size,
 * and device orientation.
 */
static status_t setDisplayProjection(
        SurfaceComposerClient::Transaction& t,
        const sp<IBinder>& dpy,
        const ui::DisplayState& displayState) {
    const ui::Size& viewport = displayState.viewport;

    // Set the region of the layer stack we're interested in, which in our
    // case is "all of it".
    Rect layerStackRect(viewport);

    // We need to preserve the aspect ratio of the display.
    float displayAspect = viewport.getHeight() / static_cast<float>(viewport.getWidth());


    // Set the way we map the output onto the display surface (which will
    // be e.g. 1280x720 for a 720p video).  The rect is interpreted
    // post-rotation, so if the display is rotated 90 degrees we need to
    // "pre-rotate" it by flipping width/height, so that the orientation
    // adjustment changes it back.
    //
    // We might want to encode a portrait display as landscape to use more
    // of the screen real estate.  (If players respect a 90-degree rotation
    // hint, we can essentially get a 720x1280 video instead of 1280x720.)
    // In that case, we swap the configured video width/height and then
    // supply a rotation value to the display projection.
    uint32_t videoWidth, videoHeight;
    uint32_t outWidth, outHeight;
    if (!gRotate) {
        videoWidth = gVideoWidth;
        videoHeight = gVideoHeight;
    } else {
        videoWidth = gVideoHeight;
        videoHeight = gVideoWidth;
    }
    if (videoHeight > (uint32_t)(videoWidth * displayAspect)) {
        // limited by narrow width; reduce height
        outWidth = videoWidth;
        outHeight = (uint32_t)(videoWidth * displayAspect);
    } else {
        // limited by short height; restrict width
        outHeight = videoHeight;
        outWidth = (uint32_t)(videoHeight / displayAspect);
    }
    uint32_t offX, offY;
    offX = (videoWidth - outWidth) / 2;
    offY = (videoHeight - outHeight) / 2;
    Rect displayRect(offX, offY, offX + outWidth, offY + outHeight);

    if (gVerbose) {
        if (gRotate) {
            printf("Rotated content area is %ux%u at offset x=%d y=%d\n",
                    outHeight, outWidth, offY, offX);
            fflush(stdout);
        } else {
            printf("Content area is %ux%u at offset x=%d y=%d\n",
                    outWidth, outHeight, offX, offY);
            fflush(stdout);
        }
    }

    t.setDisplayProjection(dpy,
            gRotate ? ui::ROTATION_90 : ui::ROTATION_0,
            layerStackRect, displayRect);
    return NO_ERROR;
}

/*
 * Configures the virtual display.  When this completes, virtual display
 * frames will start arriving from the buffer producer.
 */
static status_t prepareVirtualDisplay(
        const ui::DisplayState& displayState,
		const sp<IGraphicBufferProducer>& bufferProducer,
		sp<IBinder>* pDisplayHandle) {
	sp<IBinder> dpy = SurfaceComposerClient::createDisplay(
			String8("ScreenRecorder"), false /*secure*/);
    SurfaceComposerClient::Transaction t;
    t.setDisplaySurface(dpy, bufferProducer);
    setDisplayProjection(t, dpy, displayState);
    t.setDisplayLayerStack(dpy, displayState.layerStack);
    t.apply();
	*pDisplayHandle = dpy;

	return NO_ERROR;
}

int main(int argc UNUSED, char* const argv[] UNUSED) {
	status_t err;

	// Start Binder thread pool.  MediaCodec needs to be able to receive
	// messages from mediaserver.
	sp<ProcessState> self = ProcessState::self();
	self->startThreadPool();

	// Get main display parameters.
    sp<IBinder> display = SurfaceComposerClient::getPhysicalDisplayToken(
            gPhysicalDisplayId);
    if (display == nullptr) {
        ALOGE("ERROR: no display");
        return NAME_NOT_FOUND;
    }

    ui::DisplayState displayState;
    err = SurfaceComposerClient::getDisplayState(display, &displayState);
    if (err != NO_ERROR) {
        fprintf(stderr, "ERROR: unable to get display state\n");
        return err;
    }

    DisplayConfig displayConfig;
    err = SurfaceComposerClient::getActiveDisplayConfig(display, &displayConfig);
    if (err != NO_ERROR) {
        fprintf(stderr, "ERROR: unable to get display config\n");
        return err;
    }

    const ui::Size& viewport = displayState.viewport;
    if (gVerbose) {
        printf("Display is %dx%d @%.2ffps (orientation=%s), layerStack=%u\n",
                viewport.getWidth(), viewport.getHeight(), displayConfig.refreshRate,
                toCString(displayState.orientation), displayState.layerStack);
        fflush(stdout);
    }

    // Encoder can't take odd number as config
    if (gVideoWidth == 0) {
        gVideoWidth = floorToEven(viewport.getWidth());
    }
    if (gVideoHeight == 0) {
        gVideoHeight = floorToEven(viewport.getHeight());
    }

	sp<FrameOutput> frameOutput = new FrameOutput();
	sp<IGraphicBufferProducer> bufferProducer;
	err = frameOutput->createInputSurface(gVideoWidth, gVideoHeight, &bufferProducer);
	if (err != NO_ERROR) {
		return err;
	}

    // Configure virtual display.
    sp<IBinder> dpy;
    err = prepareVirtualDisplay(displayState, bufferProducer, &dpy);

	while (!gStopRequested) {
		// Poll for frames, the same way we do for MediaCodec.  We do
		// all of the work on the main thread.
		//
		// Ideally we'd sleep indefinitely and wake when the
		// stop was requested, but this will do for now.  (It almost
		// works because wait() wakes when a signal hits, but we
		// need to handle the edge cases.)
		err = frameOutput->copyFrame();
		if (err == ETIMEDOUT) {
			err = NO_ERROR;
		} else if (err != NO_ERROR) {
			ALOGE("Got error %d from copyFrame()", err);
			break;
		}
	}

	// Shut everything down, starting with the producer side.
	bufferProducer = NULL;
	SurfaceComposerClient::destroyDisplay(dpy);

	return err;
}

