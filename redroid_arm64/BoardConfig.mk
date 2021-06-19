include device/generic/arm64/BoardConfig.mk

# Android 8 / 9
TARGET_USES_HWC2 := true

# want all fonts
SMALLER_FONT_FOOTPRINT := false
MINIMAL_FONT_FOOTPRINT := false

# Android 11 enabled this
BUILD_EMULATOR_OPENGL := false

# use seperate vendor partition
TARGET_COPY_OUT_VENDOR := vendor

# TODO add panfrost
BOARD_GPU_DRIVERS := freedreno lima
BOARD_USES_MINIGBM := true

PLATFORM_VERSION_MAJOR := $(word 1, $(subst ., ,$(PLATFORM_VERSION)))
DEVICE_MANIFEST_FILE := vendor/redroid/manifest.$(PLATFORM_VERSION_MAJOR).xml
PRODUCT_ENFORCE_VINTF_MANIFEST := true

# ~ 1.3G
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 1388314624
