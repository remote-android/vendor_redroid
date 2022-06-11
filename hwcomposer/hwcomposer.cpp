/*
 * Copyright (C) 2012 The Android Open Source Project
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

//#define LOG_NDEBUG 0

#include <sys/resource.h>
#include <unistd.h>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <thread>
#include <mutex>

#include <log/log.h>
#include <hardware/hwcomposer.h>
#include <sync/sync.h>
#include <cutils/properties.h>

#define v(...) ALOGV(__VA_ARGS__)
#define d(...) ALOGD(__VA_ARGS__)
#define w(...) ALOGW(__VA_ARGS__)
#define e(...) ALOGE(__VA_ARGS__)

using namespace std;

struct redroid_hwc_composer_device_1 {
    hwc_composer_device_1_t base; // constant after init
    const hwc_procs_t *procs;     // constant after init
    thread vsync_thread;       // constant after init
    bool vsync_thread_exit;
    int32_t vsync_period_ns;      // constant after init

    mutex vsync_lock;
    bool vsync_callback_enabled; // protected by this->vsync_lock
};

static bool g_stream_enabled = false;

static int hwc_prepare(hwc_composer_device_1_t* dev __unused,
                       size_t numDisplays, hwc_display_contents_1_t** displays) {

    if (!numDisplays || !displays) return 0;

    hwc_display_contents_1_t* contents = displays[HWC_DISPLAY_PRIMARY];

    if (!contents) return 0;

    for (size_t i = 0; i < contents->numHwLayers; i++) {
        if (HWC_FRAMEBUFFER_TARGET == contents->hwLayers[i].compositionType) continue;

        if (g_stream_enabled) {
            contents->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
        } else {
            switch (contents->hwLayers[i].compositionType) {
                case HWC_FRAMEBUFFER:
                    contents->hwLayers[i].compositionType = HWC_OVERLAY;
                    break;
                // ignore default
            }
        }
    }
    return 0;
}

static int hwc_set(struct hwc_composer_device_1* /*dev*/,size_t numDisplays,
                   hwc_display_contents_1_t** displays) {

    if (!numDisplays || !displays) return 0;

    auto contents = displays[HWC_DISPLAY_PRIMARY];

    int retireFenceFd = -1;
    for (size_t layer = 0; layer < contents->numHwLayers; layer++) {
        auto fb_layer = &contents->hwLayers[layer];

        int releaseFenceFd = -1;
        if (fb_layer->acquireFenceFd > 0) {
            const int kAcquireWarningMS= 3000;
            int err = sync_wait(fb_layer->acquireFenceFd, kAcquireWarningMS);
            if (err < 0 && errno == ETIME) {
                e("hwcomposer waited on fence %d for %d ms", fb_layer->acquireFenceFd, kAcquireWarningMS);
            }
            close(fb_layer->acquireFenceFd);
            releaseFenceFd = dup(fb_layer->acquireFenceFd);
            fb_layer->acquireFenceFd = -1;
        }

        if (fb_layer->compositionType != HWC_FRAMEBUFFER_TARGET) continue;

        fb_layer->releaseFenceFd = releaseFenceFd;

		if (releaseFenceFd > 0) {
            if (retireFenceFd == -1) {
                retireFenceFd = dup(releaseFenceFd);
            } else {
                auto mergedFenceFd = sync_merge("hwc_set retireFence",
                                               releaseFenceFd, retireFenceFd);
                close(retireFenceFd);
                retireFenceFd = mergedFenceFd;
            }
        }
    }

    contents->retireFenceFd = retireFenceFd;
    return 0;
}

static int hwc_query(struct hwc_composer_device_1* dev, int what, int* value) {
    auto pdev = (struct redroid_hwc_composer_device_1*) dev;

    switch (what) {
        case HWC_BACKGROUND_LAYER_SUPPORTED:
            // we do not support the background layer
            value[0] = 0;
            break;
        case HWC_VSYNC_PERIOD:
            value[0] = pdev->vsync_period_ns;
            break;
        default:
            // unsupported query
            ALOGE("%s badness unsupported query what=%d", __FUNCTION__, what);
            return -EINVAL;
    }
    return 0;
}

static int hwc_event_control(struct hwc_composer_device_1* dev, int dpy __unused,
                             int event, int enabled) {
    auto pdev = (struct redroid_hwc_composer_device_1*) dev;
    int ret = -EINVAL;

    // enabled can only be 0 or 1
    if (event == HWC_EVENT_VSYNC) {
        lock_guard<mutex> _l(pdev->vsync_lock);
        pdev->vsync_callback_enabled = enabled;
        ALOGD("VSYNC event status:%d", enabled);
        ret = 0;
    }
    return ret;
}

static int hwc_blank(struct hwc_composer_device_1* dev __unused, int disp,
                     int blank __unused) {
    if (disp != HWC_DISPLAY_PRIMARY) {
        return -EINVAL;
    }
    return 0;
}

static void hwc_dump(hwc_composer_device_1* dev __unused, char* buff __unused,
                     int buff_len __unused) {
    // This is run when running dumpsys.
    // No-op for now.
}


static int hwc_get_display_configs(struct hwc_composer_device_1* dev __unused,
                                   int disp, uint32_t* configs, size_t* numConfigs) {
    if (*numConfigs == 0) {
        return 0;
    }

    if (disp == HWC_DISPLAY_PRIMARY) {
        configs[0] = 0;
        *numConfigs = 1;
        return 0;
    }

    return -EINVAL;
}


static int32_t hwc_attribute(struct redroid_hwc_composer_device_1* pdev,
                             const uint32_t attribute) {
    switch(attribute) {
        case HWC_DISPLAY_VSYNC_PERIOD:
            return pdev->vsync_period_ns;
        case HWC_DISPLAY_WIDTH:
			return property_get_int32("ro.kernel.redroid.width", 720);
        case HWC_DISPLAY_HEIGHT:
			return property_get_int32("ro.kernel.redroid.height", 1280);
        case HWC_DISPLAY_DPI_X:
        case HWC_DISPLAY_DPI_Y:
			return property_get_int32("ro.sf.lcd_density", 320) / 2;
        default:
            ALOGW("unknown display attribute %u", attribute);
            return -EINVAL;
    }
}

static int hwc_get_display_attributes(struct hwc_composer_device_1* dev __unused,
                                      int disp, uint32_t config __unused,
                                      const uint32_t* attributes, int32_t* values) {

    auto pdev = (struct redroid_hwc_composer_device_1*)dev;
    for (int i = 0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; i++) {
        if (disp == HWC_DISPLAY_PRIMARY) {
            values[i] = hwc_attribute(pdev, attributes[i]);
        } else {
            ALOGE("unknown display type %u", disp);
            return -EINVAL;
        }
    }

    return 0;
}

static int hwc_close(hw_device_t* dev) {
    auto pdev = (struct redroid_hwc_composer_device_1*)dev;
    pdev->vsync_thread_exit = true;
    pdev->vsync_thread.join();
    free(dev);
    return 0;
}

static void* hwc_vsync_thread(void* data) {
    auto pdev = (struct redroid_hwc_composer_device_1*)data;
    setpriority(PRIO_PROCESS, 0, -8 /*HAL_PRIORITY_URGENT_DISPLAY*/);

    using namespace std::chrono;

    auto now = high_resolution_clock::now();
    nanoseconds vsync_interval(pdev->vsync_period_ns);
    auto last_log_time = now;
    seconds log_interval(60);
    int sent = 0, last_sent = 0;

    while (true) {
        v("before sleep");
        std::this_thread::sleep_until(now += vsync_interval);
        v("after sleep");

        if (pdev->vsync_thread_exit) {
            ALOGI("vsync thread exiting");
            break;
        }

        {
            lock_guard<mutex> _l(pdev->vsync_lock);
            if (!pdev->vsync_callback_enabled) continue;
        }

        v("before vsync");
        pdev->procs->vsync(pdev->procs, 0, duration_cast<nanoseconds>(now.time_since_epoch()).count());
        v("after vsync");

        {
            auto period = duration_cast<seconds>(now - last_log_time);
            if (period.count() > log_interval.count()) {
                ALOGD("hw_composer sent %d syncs in %llds", sent - last_sent, period.count());
                last_log_time = now;
                last_sent = sent;
            }
        }
        ++sent;
    }

    return NULL;
}

static void hwc_register_procs(struct hwc_composer_device_1* dev,
                               hwc_procs_t const* procs) {
    auto pdev = (struct redroid_hwc_composer_device_1*)dev;
    pdev->procs = procs;
}

static int hwc_open(const struct hw_module_t* module, const char* name,
                    struct hw_device_t** device) {

    if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
        ALOGE("%s called with bad name %s", __FUNCTION__, name);
        return -EINVAL;
    }

    auto pdev = new redroid_hwc_composer_device_1();
    assert(pdev);

    pdev->base = {
        .common = {
            .tag = HARDWARE_DEVICE_TAG,
            .version = HWC_DEVICE_API_VERSION_1_1,
            .module = const_cast<hw_module_t *>(module),
            .close = hwc_close,
        },

        .prepare = hwc_prepare,
        .set = hwc_set,
        .eventControl = hwc_event_control,
        .blank = hwc_blank,
        .query = hwc_query,
        .registerProcs = hwc_register_procs,
        .dump = hwc_dump,
        .getDisplayConfigs = hwc_get_display_configs,
        .getDisplayAttributes = hwc_get_display_attributes,
    };

    int fps = property_get_int32("ro.kernel.redroid.fps", 15);
    if (fps <= 0 || fps > 120) fps = 15;
    ALOGD("Set vsync period = %d", fps);
    pdev->vsync_period_ns = 1000 * 1000 * 1000 / fps;

    pdev->vsync_callback_enabled = false;
    pdev->vsync_thread_exit = false;

    pdev->vsync_thread = thread(hwc_vsync_thread, pdev);

    *device = &pdev->base.common;

    return 0;
}


static struct hw_module_methods_t hwc_module_methods = {
    .open = hwc_open,
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = HWC_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = HWC_HARDWARE_MODULE_ID,
        .name = "redroid hwcomposer module",
        .author = "redroid",
        .methods = &hwc_module_methods,
    }
};
