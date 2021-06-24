#!/system/bin/sh

find_render_node() {
    cd /sys/kernel/debug/dri
    for d in * ; do
        if [ "$d" -ge "128" ]; then
            driver="`cat $d/name | cut -d' ' -f1`"
            echo "DRI node exists, driver: $driver"
            case $driver in
                i915|amdgpu|virtio_gpu)
                    node="/dev/dri/renderD$d"
                    echo "use render node: $node"
                    setprop gralloc.gbm.device $node
                    return 0
                    ;;
            esac
        fi
    done

    echo "NO qualified render node found"
    return 1
}

gpu_setup() {
    ## setup qemu.gles.vendor, ro.hardware.gralloc, gralloc.gbm.device
    ## choose GLES user specify->host->indirect->guest
    ## redroid.gpu.mode=(auto, host, swiftshader_indirect*, guest)
    ## redroid.gpu.node= (/dev/dri/renderDxxx)
    mode=`getprop ro.kernel.redroid.gpu.mode`
    if [ "$mode" = "host" ]; then
        echo "use GPU host mode"
        node=`getprop ro.kernel.redroid.gpu.node`
        if [ -z "$node" ]; then
            find_render_node
        else
            setprop gralloc.gbm.device $node
        fi
    elif [ "$mode" = "swiftshader_indirect" ]; then
        echo "use GPU swiftshader_indirect mode NOT READY!!!"
    elif [ "$mode" = "guest" ]; then
        echo "use GPU guest mode"
    elif [ -z "$mode" ] || [ "$mode" = "auto" ]; then
         echo "use GPU auto detect mode"
         if find_render_node; then
            echo "GPU ready"
            setprop qemu.gles.vendor mesa
            setprop ro.hardware.gralloc gbm
         fi
    else
        echo "unknown mode: $mode"
    fi
}

gpu_setup

