# skip androidx.window.extensions check
PRODUCT_BROKEN_VERIFY_USES_LIBRARIES := true

PRODUCT_PACKAGES += \
	binder_alloc \
	gralloc.redroid \
	ipconfigstore \


PRODUCT_COPY_FILES += \
    vendor/redroid/gpu_config.sh:$(TARGET_COPY_OUT_VENDOR)/bin/gpu_config.sh \
    vendor/redroid/post-fs-data.redroid.sh:$(TARGET_COPY_OUT_VENDOR)/bin/post-fs-data.redroid.sh \
    vendor/redroid/redroid.common.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/redroid.common.rc \
    vendor/redroid/redroid.legacy.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/redroid.legacy.rc \


PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.touchscreen.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.touchscreen.xml \
