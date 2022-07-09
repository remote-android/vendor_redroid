#!/system/bin/sh

## enable `memfd` if `ashmem` missing
## memfd is disabled in post-fs-data (init.rc)
[ -c /dev/ashmem ] || setprop sys.use_memfd 1
