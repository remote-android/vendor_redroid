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

#include <set>

#include <android-base/properties.h>

#include "data.h"

using namespace android::base;

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
    get_gateway((char *) "eth0", conf->gateway);
    return 0;
}

static void write_dns(FILE *fp) {
    std::set<std::string> dnsList;
    auto ndns = GetIntProperty("ro.boot.redroid_net_ndns", 0);
    for (int i = 1; i <= ndns; ++i) {
        dnsList.insert(GetProperty("ro.boot.redroid_net_dns" + std::to_string(i), ""));
    }
    if (dnsList.empty()) dnsList.insert("8.8.8.8");

    for (auto& dns: dnsList) {
        writePackedString("dns", fp);
        writePackedString(dns.c_str(), fp);
    }
}

static void write_proxy(FILE *fp) {
    // static | pac | none | unassigned
    std::string proxy_type = GetProperty("ro.boot.redroid_net_proxy_type", "");
    if ("static" == proxy_type) {
        writePackedString("proxySettings", fp);
        writePackedString("STATIC", fp);

        writePackedString("proxyHost", fp);
        writePackedString(GetProperty("ro.boot.redroid_net_proxy_host", "").c_str(), fp);

        writePackedString("proxyPort", fp);
        writePackedUInt32(GetIntProperty("ro.boot.redroid_net_proxy_port", 3128), fp);

        writePackedString("exclusionList", fp);
        writePackedString(GetProperty("ro.boot.redroid_net_proxy_exclude_list", "").c_str(), fp);
    } else if ("pac" == proxy_type) {
        writePackedString("proxySettings", fp);
        writePackedString("PAC", fp);

        writePackedString("proxyPac", fp);
        writePackedString(GetProperty("ro.boot.redroid_net_proxy_pac", "").c_str(), fp);
    } else if ("none" == proxy_type) {
        writePackedString("proxySettings", fp);
        writePackedString("NONE", fp);
    } else {
        // ignored
    }
}

static int write_conf(struct ipconfig *conf, uint32_t v) {
    FILE *fp = fopen("/data/misc/ethernet/ipconfig.txt", "w+");

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

    write_dns(fp);

    write_proxy(fp);

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
    if (GetIntProperty("ro.build.version.sdk", 0) <= 27) v = 2;

    struct ipconfig conf;
    get_conf(&conf);
    printf("ipconfig: ipv4: %s, mask: %i, gateway: %s", conf.ipv4, conf.mask, conf.gateway);
    write_conf(&conf, v);
    return 0;
}

