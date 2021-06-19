#!/bin/bash

: ${OUT?"AOSP build out path not set, not launch?"}
: ${BUILD_OUT?"BUILD_OUT not set"}

BUILD_HOME="$BUILD_OUT/$TARGET_PRODUCT"

sudo mkdir -p $BUILD_HOME
# comment --size-only if file not synced
sudo rsync -rl $OUT/{root,system,vendor} $BUILD_HOME --delete #--size-only

do_fs_config()
{
    cd $1 && sudo find $2 \( -type d -printf "%p/\n" , -type f -print \) \
        | fs_config -C | sudo awk -v CWD=`pwd` '{
            cmd="chown " $2 ":" $3 " " $1 " && chmod " $4 " " $1;
            if ($5 != "capabilities=0x0") {
                val=substr($5, 14);
                cmd = cmd " && IFS='"'='"' read _ cap <<< `capsh --decode=" val "` && setcap $cap+ep " $1;
            }
            print cmd;
        }' | sudo bash -v || echo "**ERROR**" && exit 1
}

echo "****** config root / system / vendor fs ******"
do_fs_config "$BUILD_HOME" 'system vendor'

echo "****** config root fs ******"
do_fs_config "$BUILD_HOME/root" '*'

