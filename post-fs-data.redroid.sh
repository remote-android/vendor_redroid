#!/system/bin/sh

## enable `memfd` if `ashmem` missing
## memfd is disabled in post-fs-data (init.rc)
[ -c /dev/ashmem ] || setprop sys.use_memfd 1

## Android (bionic's sysconf, Runtime.availableProcessors(), engines like Unity)
## counts CPUs from /sys/devices/system/cpu/{online,possible,present}, which a
## container always sees as the host's, even when it is pinned to a few CPUs
## (docker --cpuset-cpus): apps size their thread pools for every host core.
## Show the container's own cpuset instead (cgroup v2), when it is narrower.
cpus=$(cat /sys/fs/cgroup/cpuset.cpus.effective 2>/dev/null)
if [ -n "$cpus" ] && [ "$cpus" != "$(cat /sys/devices/system/cpu/online)" ]; then
    echo "$cpus" > /dev/redroid-cpus
    chmod 0444 /dev/redroid-cpus
    for f in online possible present; do
        mount -o bind /dev/redroid-cpus /sys/devices/system/cpu/$f
    done
fi
