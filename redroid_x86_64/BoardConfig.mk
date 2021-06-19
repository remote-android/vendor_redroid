include device/generic/x86_64/BoardConfig.mk

# Android 8 / 9
TARGET_USES_HWC2 := true

# want all fonts
SMALLER_FONT_FOOTPRINT := false
MINIMAL_FONT_FOOTPRINT := false

# use seperate vendor partition
TARGET_COPY_OUT_VENDOR := vendor

# i915g soong build error
BOARD_GPU_DRIVERS := i915 i965 nouveau kmsro r300g r600g virgl vmwgfx etnaviv iris tegra
BOARD_USES_MINIGBM := true

PLATFORM_VERSION_MAJOR := $(word 1, $(subst ., ,$(PLATFORM_VERSION)))
DEVICE_MANIFEST_FILE := vendor/redroid/manifest.$(PLATFORM_VERSION_MAJOR).xml
PRODUCT_ENFORCE_VINTF_MANIFEST := true
