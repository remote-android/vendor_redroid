#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <net/route.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>

#include <cutils/properties.h>

#include "data.h"

struct ipconfig {
    uint32_t mask;
    char ipv4[16];
    char gateway[16];
};

static int get_gateway(char *dev, char *ret) {
    FILE *fp;
    char buf[256]; // 128 is enough for linux
    char iface[16];
    unsigned long dest_addr, gate_addr;
    fp = fopen("/proc/net/route", "r");
    if (fp == NULL) return -1;
    /* Skip title line */
    fgets(buf, sizeof(buf), fp);
    while (fgets(buf, sizeof(buf), fp)) {
        if (sscanf(buf, "%s\t%lX\t%lX", iface, &dest_addr, &gate_addr) != 3 || dest_addr != 0 || strcmp(dev, iface)) continue;
        inet_ntop(AF_INET, &gate_addr, ret, INET_ADDRSTRLEN);
        break;
    }

    fclose(fp);
    return 0;
}

static int bitcount(uint32_t n)
{
    int count=0;
    while (n) {
        count++;
        n &= (n - 1);
    }
    return count;
}

static int get_conf(struct ipconfig *conf) {
    struct ifaddrs *ifAddrStruct;
    void *tmpAddrPtr=NULL;
    getifaddrs(&ifAddrStruct);
    while (ifAddrStruct != NULL) {
        if (ifAddrStruct->ifa_addr->sa_family==AF_INET && !strcmp("eth0", ifAddrStruct->ifa_name)) {
            tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, tmpAddrPtr, conf->ipv4, INET_ADDRSTRLEN);
            conf->mask = bitcount(((struct sockaddr_in *)ifAddrStruct->ifa_netmask)->sin_addr.s_addr);
            break;
        }
        ifAddrStruct=ifAddrStruct->ifa_next;
    }
    freeifaddrs(ifAddrStruct);
    get_gateway("eth0", conf->gateway);
    return 0;
}

static int write_conf(struct ipconfig *conf, uint32_t v) {
    FILE *fp = fopen("/data/misc/ethernet/ipconfig.txt", "w+");
    char prop[PROP_VALUE_MAX];

    writePackedUInt32(v, fp); // version

    writePackedString("ipAssignment", fp);
    writePackedString("STATIC", fp);

    writePackedString("linkAddress", fp);
    writePackedString(conf->ipv4, fp);
    writePackedUInt32(conf->mask, fp);

    writePackedString("gateway", fp);
    writePackedUInt32(1, fp); // Default route (dest).
    writePackedString("0.0.0.0", fp);
    writePackedUInt32(0, fp);
    writePackedUInt32(1, fp); // Have a gateway.
    writePackedString(conf->gateway, fp);

    writePackedString("dns", fp);
    property_get("ro.kernel.net.eth0.dns1", prop, "8.8.8.8");
    writePackedString(prop, fp); // TODO multiple dns

    // static | pac | none | unassigned
    property_get("ro.kernel.net.eth0.proxy.type", prop, NULL);
    if (!strcmp(prop, "static")) {
        writePackedString("proxySettings", fp);
        writePackedString("STATIC", fp);

        writePackedString("proxyHost", fp);
        property_get("ro.kernel.net.eth0.proxy.host", prop, "");
        writePackedString(prop, fp);

        writePackedString("proxyPort", fp);
        int32_t port = property_get_int32("ro.kernel.net.eth0.proxy.port", 3128);
        writePackedUInt32(port, fp);

        property_get("ro.kernel.net.eth0.proxy.exclusionList", prop, NULL);
        if (strlen(prop)) {
            writePackedString("exclusionList", fp);
            writePackedString(prop, fp);
        }
    } else if (!strcmp(prop, "pac")) {
        writePackedString("proxySettings", fp);
        writePackedString("PAC", fp);

        writePackedString("proxyPac", fp);
        property_get("ro.kernel.net.eth0.proxy.pac", prop, "");
        writePackedString(prop, fp);
    } else if (!strcmp(prop, "none")) {
        writePackedString("proxySettings", fp);
        writePackedString("NONE", fp);
    } else {
        // ignored
    }

    writePackedString("id", fp);
    if (v == 2) writePackedUInt32(0, fp);
    else writePackedString("eth0", fp);

    writePackedString("eos", fp);

    fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    uint32_t v = 3;
    // use V2 for Android 8.1
    if (property_get_int32("ro.build.version.sdk", 0) <= 27) v = 2;

    struct ipconfig conf;
    get_conf(&conf);
    printf("ipconfig: ipv4: %s, mask: %i, gateway: %s", conf.ipv4, conf.mask, conf.gateway);
    write_conf(&conf, v);
    return 0;
}

