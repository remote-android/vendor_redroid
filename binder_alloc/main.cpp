#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BINDERFS_MAX_NAME 255
struct binderfs_device {
    char name[BINDERFS_MAX_NAME + 1];
    __u32 major;
    __u32 minor;
};
#define BINDER_CTL_ADD _IOWR('b', 1, struct binderfs_device)

void usage(char *bin) {
    printf("USAGE: %s BINDER-CONTROL-PATH DEVICE1 [DEVICE2 ...]\n", bin);
    printf("EXAMPLE: binder_alloc /dev/binderfs/binder-control binder hwbinder vndbinder\n");
}

int main(int argc, char *argv[])
{
    int fd, ret;
    size_t len;
    struct binderfs_device device{};

    if (argc < 3) {
        usage(basename(argv[0]));
        exit(EXIT_FAILURE);
    }

    fd = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        printf("%s - Failed to open binder-control device\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    char *dir = dirname(argv[1]); // "/dev/binderfs"
    char *pathname = (char *) malloc(strlen(dir) + BINDERFS_MAX_NAME + 2); // "/dev/binderfs/binder"

    for (int i = 2; i < argc; ++i) {
        sprintf(pathname, "%s/%s", dir, argv[i]);
        if (access(pathname, F_OK) != -1) {
            printf("binder device already allocated, path: %s\n", pathname);
            chmod(pathname, 0666);
            continue;
        }
        len = strlen(argv[i]);
        if (len > BINDERFS_MAX_NAME) exit(EXIT_FAILURE);

        memcpy(device.name, argv[i], len);
        device.name[len] = '\0';

        ret = ioctl(fd, BINDER_CTL_ADD, &device);
        if (ret < 0) {
            printf("%s - Failed to allocate new binder device\n",
                    strerror(errno));
            exit(EXIT_FAILURE);
        }
        chmod(pathname, 0666);
        printf("Allocated new binder device with major %d, minor %d, and "
                "name %s\n", device.major, device.minor,
                device.name);
    }
    free(pathname);
    close(fd);

    exit(EXIT_SUCCESS);
}
