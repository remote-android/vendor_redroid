#if PLATFORM_VERSION == 11
#include "main_11.inc"
#else
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
#include <utils/Timers.h>
#include <utils/Trace.h>

#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include <ui/DisplayInfo.h>

#include "FrameOutput.h"

#define UNUSED __attribute__((__unused__))

using namespace android;

static bool gRotate = false;            // rotate 90 degrees
static uint32_t gVideoWidth = 0;        // default width+height
static uint32_t gVideoHeight = 0;

// Set by signal handler to stop recording.
static volatile bool gStopRequested = false;

// Previous signal handler state, restored after first hit.
static struct sigaction gOrigSigactionINT;
static struct sigaction gOrigSigactionHUP;

/*
 * Catch keyboard interrupt signals.  On receipt, the "stop requested"
 * flag is raised, and the original handler is restored (so that, if
 * we get stuck finishing, a second Ctrl-C will kill the process).
 */
static void signalCatcher(int signum)
{
	gStopRequested = true;
	switch (signum) {
		case SIGINT:
		case SIGHUP:
			sigaction(SIGINT, &gOrigSigactionINT, NULL);
			sigaction(SIGHUP, &gOrigSigactionHUP, NULL);
			break;
		default:
			abort();
			break;
	}
}

/*
 * Configures signal handlers.  The previous handlers are saved.
 *
 * If the command is run from an interactive adb shell, we get SIGINT
 * when Ctrl-C is hit.  If we're run from the host, the local adb process
 * gets the signal, and we get a SIGHUP when the terminal disconnects.
 */
static status_t configureSignals() {
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = signalCatcher;
	if (sigaction(SIGINT, &act, &gOrigSigactionINT) != 0) {
		status_t err = -errno;
		fprintf(stderr, "Unable to configure SIGINT handler: %s\n",
				strerror(errno));
		return err;
	}
	if (sigaction(SIGHUP, &act, &gOrigSigactionHUP) != 0) {
		status_t err = -errno;
		fprintf(stderr, "Unable to configure SIGHUP handler: %s\n",
				strerror(errno));
		return err;
	}
	return NO_ERROR;
}

/*
 * Returns "true" if the device is rotated 90 degrees.
 */
static bool isDeviceRotated(int orientation) {
	return orientation != DISPLAY_ORIENTATION_0 &&
		orientation != DISPLAY_ORIENTATION_180;
}

/*
 * Sets the display projection, based on the display dimensions, video size,
 * and device orientation.
 */
static status_t setDisplayProjection(
#if PLATFORM_VERSION > 8 // Android 9+
        SurfaceComposerClient::Transaction& t,
#endif
        const sp<IBinder>& dpy,
		const DisplayInfo& mainDpyInfo) {

	// Set the region of the layer stack we're interested in, which in our
	// case is "all of it".  If the app is rotated (so that the width of the
	// app is based on the height of the display), reverse width/height.
	bool deviceRotated = isDeviceRotated(mainDpyInfo.orientation);
	uint32_t sourceWidth, sourceHeight;
	if (!deviceRotated) {
		sourceWidth = mainDpyInfo.w;
		sourceHeight = mainDpyInfo.h;
	} else {
		ALOGV("using rotated width/height");
		sourceHeight = mainDpyInfo.w;
		sourceWidth = mainDpyInfo.h;
	}
	Rect layerStackRect(sourceWidth, sourceHeight);

	// We need to preserve the aspect ratio of the display.
	float displayAspect = (float) sourceHeight / (float) sourceWidth;


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

	if (gRotate) {
		ALOGV("Rotated content area is %ux%u at offset x=%d y=%d\n",
				outHeight, outWidth, offY, offX);
	} else {
		ALOGV("Content area is %ux%u at offset x=%d y=%d\n",
				outWidth, outHeight, offX, offY);
	}

#if PLATFORM_VERSION == 8
	SurfaceComposerClient::setDisplayProjection(dpy,
			gRotate ? DISPLAY_ORIENTATION_90 : DISPLAY_ORIENTATION_0,
			layerStackRect, displayRect);
#else // Android 9+
    t.setDisplayProjection(dpy,
            gRotate ? DISPLAY_ORIENTATION_90 : DISPLAY_ORIENTATION_0,
            layerStackRect, displayRect);
#endif
	return NO_ERROR;
}

/*
 * Configures the virtual display.  When this completes, virtual display
 * frames will start arriving from the buffer producer.
 */
static status_t prepareVirtualDisplay(const DisplayInfo& mainDpyInfo,
		const sp<IGraphicBufferProducer>& bufferProducer,
		sp<IBinder>* pDisplayHandle) {
	sp<IBinder> dpy = SurfaceComposerClient::createDisplay(
			String8("ScreenRecorder"), false /*secure*/);
#if PLATFORM_VERSION == 8
	SurfaceComposerClient::openGlobalTransaction();
	SurfaceComposerClient::setDisplaySurface(dpy, bufferProducer);
	setDisplayProjection(dpy, mainDpyInfo);
	SurfaceComposerClient::setDisplayLayerStack(dpy, 0);    // default stack
	SurfaceComposerClient::closeGlobalTransaction();
#else // Android 9+
    SurfaceComposerClient::Transaction t;
    t.setDisplaySurface(dpy, bufferProducer);
    setDisplayProjection(t, dpy, mainDpyInfo);
    t.setDisplayLayerStack(dpy, 0);    // default stack
    t.apply();
#endif
	*pDisplayHandle = dpy;

	return NO_ERROR;
}

int main(int argc UNUSED, char* const argv[] UNUSED) {
	status_t err;

	// Configure signal handler.
	err = configureSignals();
	if (err != NO_ERROR) return err;

	// Start Binder thread pool.  MediaCodec needs to be able to receive
	// messages from mediaserver.
	sp<ProcessState> self = ProcessState::self();
	self->startThreadPool();

	// Get main display parameters.
#if PLATFORM_VERSION > 9 // Android 10+
    const sp<IBinder> mainDpy = SurfaceComposerClient::getInternalDisplayToken();
    if (mainDpy == nullptr) {
        ALOGE("ERROR: no display");
        return NAME_NOT_FOUND;
    }
#else
	sp<IBinder> mainDpy = SurfaceComposerClient::getBuiltInDisplay(
			ISurfaceComposer::eDisplayIdMain);
#endif
	DisplayInfo mainDpyInfo;
	err = SurfaceComposerClient::getDisplayInfo(mainDpy, &mainDpyInfo);
	if (err != NO_ERROR) {
		ALOGE("ERROR: unable to get display characteristics\n");
		return err;
	}
	ALOGV("Main display is %dx%d @%.2ffps (orientation=%u)\n",
			mainDpyInfo.w, mainDpyInfo.h, mainDpyInfo.fps,
			mainDpyInfo.orientation);

	bool rotated = isDeviceRotated(mainDpyInfo.orientation);
	if (gVideoWidth == 0) {
		gVideoWidth = rotated ? mainDpyInfo.h : mainDpyInfo.w;
	}
	if (gVideoHeight == 0) {
		gVideoHeight = rotated ? mainDpyInfo.w : mainDpyInfo.h;
	}

	sp<FrameOutput> frameOutput = new FrameOutput();
	sp<IGraphicBufferProducer> bufferProducer;
	err = frameOutput->createInputSurface(gVideoWidth, gVideoHeight, &bufferProducer);
	if (err != NO_ERROR) {
		return err;
	}

	// Configure virtual display.
	sp<IBinder> dpy;
	err = prepareVirtualDisplay(mainDpyInfo, bufferProducer, &dpy);

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
#endif

