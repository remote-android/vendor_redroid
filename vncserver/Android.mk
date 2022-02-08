# Copyright 2013 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	main.cpp \
	EglWindow.cpp \
	FrameOutput.cpp \
	Program.cpp \
	InputManager.cpp \
	InputDevice.cpp

LOCAL_SHARED_LIBRARIES := \
	libutils libbinder libgui libEGL libGLESv2 \
	libz libcrypto libssl libjpeg libpng libcutils liblog libinput libui


LOCAL_C_INCLUDES := external/libvncserver \

LOCAL_STATIC_LIBRARIES += libvncserver

LOCAL_INIT_RC := vncserver.rc

LOCAL_CFLAGS := -Werror -Wall
LOCAL_CFLAGS += -Wno-multichar
#LOCAL_CFLAGS += -UNDEBUG

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= vncserver

include $(BUILD_EXECUTABLE)

