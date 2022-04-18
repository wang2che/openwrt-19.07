#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <net/if.h>
#include "QMIThread.h"

int iAutoIPAddress = 0;

static int sc_system(const char *shell_cmd) {
    dbg_time("%s", shell_cmd);
    return system(shell_cmd);
}

static void sc_set_mtu(const char *usbnet_adapter, int ifru_mtu) {
    int inet_sock;
    struct ifreq ifr;

    inet_sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (inet_sock > 0) {
        strcpy(ifr.ifr_name, usbnet_adapter);

        if (!ioctl(inet_sock, SIOCGIFMTU, &ifr)) {
            if (ifr.ifr_ifru.ifru_mtu != ifru_mtu) {
                dbg_time("change mtu %d -> %d", ifr.ifr_ifru.ifru_mtu , ifru_mtu);
                ifr.ifr_ifru.ifru_mtu = ifru_mtu;
                ioctl(inet_sock, SIOCSIFMTU, &ifr);
            }
        }

        close(inet_sock);
    }
}

static int set_rawip_data_format(const char *ifname) {
    int fd;
    char raw_ip[128];
    char shell_cmd[128];
    char mode[2] = "N";
    int mode_change = 0;

    snprintf(raw_ip, sizeof(raw_ip), "/sys/class/net/%s/qmi/raw_ip", ifname);
    if (access(raw_ip, F_OK))
        return 0;

    fd = open(raw_ip, O_RDWR | O_NONBLOCK | O_NOCTTY);
    if (fd < 0) {
        dbg_time("Warning:%s fail to open(%s), errno:%d (%s)",  __LINE__, raw_ip, errno, strerror(errno));
        return 0;
    }

    read(fd, mode, 2);
    if (mode[0] == '0' || mode[0] == 'N') {
        snprintf(shell_cmd, sizeof(shell_cmd), "ifconfig %s down", ifname);
        sc_system(shell_cmd);
        dbg_time("echo Y > /sys/class/net/%s/qmi/raw_ip", ifname);
        mode[0] = 'Y';
        write(fd, mode, 2);
        mode_change = 1;
        snprintf(shell_cmd, sizeof(shell_cmd), "ifconfig %s up", ifname);
        sc_system(shell_cmd);
    }

    close(fd);
    return mode_change;
}

static void* udhcpc_thread_function(void* arg) {
    FILE * udhcpc_fp;
    char *udhcpc_cmd = (char *)arg;

    if (udhcpc_cmd == NULL)
        return NULL;

    dbg_time("%s", udhcpc_cmd);
    udhcpc_fp = popen(udhcpc_cmd, "r");
    free(udhcpc_cmd);
    if (udhcpc_fp) {
        char buf[0xff];

        while((fgets(buf, sizeof(buf), udhcpc_fp)) != NULL) {
            if ((strlen(buf) > 1) && (buf[strlen(buf) - 1] == '\n'))
                buf[strlen(buf) - 1] = '\0';
            dbg_time("%s", buf);
        }

        pclose(udhcpc_fp);
    }

    return NULL;
}

//#define USE_DHCLIENT
#ifdef USE_DHCLIENT
static int dhclient_alive = 0;
#endif
static int dibbler_client_alive = 0;

void udhcpc_start(PROFILE_T *profile) {
    char *ifname = profile->usbnet_adapter;
    char shell_cmd[128];

    if (profile->qmapnet_adapter) {
        ifname = profile->qmapnet_adapter;
    }
    if (profile->rawIP && profile->ipv4.Address && profile->ipv4.Mtu) {
        dbg_time("force to set %s mtu to 1500.\n",ifname);
        sc_set_mtu(ifname, 1500);
    }else{
        if (ifname != NULL){
            dbg_time("Another case force to set %s mtu to 1500.\n",ifname);
            sc_set_mtu(ifname, 1500);
        }else{
            dbg_time("No ifname,jump sc_set_mtu operation.\n");
        }
    }

    snprintf(shell_cmd, sizeof(shell_cmd) - 1, "ifconfig %s up", ifname);
    sc_system(shell_cmd);

#if 1 //for bridge mode, only one public IP, so donot run udhcpc to obtain
    {
        const char *BRIDGE_MODE_FILE = "/sys/module/GobiNet/parameters/bridge_mode";
        const char *BRIDGE_IPV4_FILE = "/sys/module/GobiNet/parameters/bridge_ipv4";

        if (strncmp(qmichannel, "/dev/qcqmi", strlen("/dev/qcqmi"))) {
            BRIDGE_MODE_FILE = "/sys/module/qmi_wwan/parameters/bridge_mode";
            BRIDGE_IPV4_FILE = "/sys/module/qmi_wwan/parameters/bridge_ipv4";
        }

        if (profile->ipv4.Address && !access(BRIDGE_MODE_FILE, R_OK)) {
            int bridge_fd = open(BRIDGE_MODE_FILE, O_RDONLY);
            char bridge_mode[2] = {0, 0};

            if (bridge_fd > 0) {
                read(bridge_fd, &bridge_mode, sizeof(bridge_mode));
                close(bridge_fd);
                if(bridge_mode[0] != '0') {
                    snprintf(shell_cmd, sizeof(shell_cmd), "echo 0x%08x > %s", profile->ipv4.Address, BRIDGE_IPV4_FILE);
                    sc_system(shell_cmd);
                    return;
                }
            }
        }
    }
#endif

//because must use udhcpc to obtain IP when working on ETH mode,
//so it is better also use udhcpc to obtain IP when working on IP mode.
//use the same policy for all modules
    if (iAutoIPAddress > 0 && profile->rawIP != 0) //mdm9x07/
    {
        if (profile->ipv4.Address) {
            unsigned char *ip = (unsigned char *)&profile->ipv4.Address;
            unsigned char *gw = (unsigned char *)&profile->ipv4.Gateway;
            unsigned char *netmask = (unsigned char *)&profile->ipv4.SubnetMask;
            unsigned char *dns1 = (unsigned char *)&profile->ipv4.DnsPrimary;
            unsigned char *dns2 = (unsigned char *)&profile->ipv4.DnsSecondary;

            snprintf(shell_cmd, sizeof(shell_cmd), "ifconfig %s %d.%d.%d.%d netmask %d.%d.%d.%d",ifname,
                ip[3], ip[2], ip[1], ip[0], netmask[3], netmask[2], netmask[1], netmask[0]);
            sc_system(shell_cmd);

            //Resetting default route
            snprintf(shell_cmd, sizeof(shell_cmd), "route del default gw 0.0.0.0 dev %s", ifname);
            while(!system(shell_cmd));

            snprintf(shell_cmd, sizeof(shell_cmd), "route add default gw %d.%d.%d.%d dev %s metric 0", gw[3], gw[2], gw[1], gw[0], ifname);
            sc_system(shell_cmd);

            //Adding DNS
            if (profile->ipv4.DnsSecondary == 0)
                profile->ipv4.DnsSecondary = profile->ipv4.DnsPrimary;

            if (dns1[0]) {
                dbg_time("Adding DNS %d.%d.%d.%d %d.%d.%d.%d", dns1[3], dns1[2], dns1[1], dns1[0], dns2[3], dns2[2], dns2[1], dns2[0]);
                snprintf(shell_cmd, sizeof(shell_cmd), "echo -n \"nameserver %d.%d.%d.%d\nnameserver %d.%d.%d.%d\n\" > /etc/resolv.conf",
                    dns1[3], dns1[2], dns1[1], dns1[0], dns2[3], dns2[2], dns2[1], dns2[0]);
                system(shell_cmd);
            }
        }


        if (profile->ipv6.Address[0] && profile->ipv6.PrefixLengthIPAddr) {
            unsigned char *ip = (unsigned char *)profile->ipv6.Address;
#if 1
            snprintf(shell_cmd, sizeof(shell_cmd), "ifconfig %s inet6 add %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x/%u",
                ifname, ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7], ip[8], ip[9], ip[10], ip[11], ip[12], ip[13], ip[14], ip[15], profile->ipv6.PrefixLengthIPAddr);
#else
            snprintf(shell_cmd, sizeof(shell_cmd),
                "ip -6 addr add %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x/%u dev %s",
                ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7], ip[8], ip[9], ip[10], ip[11], ip[12], ip[13], ip[14], ip[15],profile->ipv6.PrefixLengthIPAddr, ifname);
#endif
            sc_system(shell_cmd);
            snprintf(shell_cmd, sizeof(shell_cmd), "route -A inet6 add default dev %s", ifname);
            sc_system(shell_cmd);
        }
        return;
    }

    {
        char udhcpc_cmd[128];
        pthread_attr_t udhcpc_thread_attr;
        pthread_t udhcpc_thread_id;

        pthread_attr_init(&udhcpc_thread_attr);
        pthread_attr_setdetachstate(&udhcpc_thread_attr, PTHREAD_CREATE_DETACHED);

        if (profile->ipv4.Address) {
#ifdef USE_DHCLIENT
            snprintf(udhcpc_cmd, sizeof(udhcpc_cmd), "dhclient -4 -d --no-pid %s", ifname);
            dhclient_alive++;
#else
            if (access("/usr/share/udhcpc/default.script", X_OK)) {
                dbg_time("Fail to access /usr/share/udhcpc/default.script, errno: %d (%s)", errno, strerror(errno));
            }

            if (!access("/etc/udhcpc/default.script", X_OK)) {
                dbg_time("Found DHCP /etc/udhcpc/default.script!\n");
            }

            //-f,--foreground    Run in foreground
            //-b,--background    Background if lease is not obtained
            //-n,--now        Exit if lease is not obtained
            //-q,--quit        Exit after obtaining lease
            //-t,--retries N        Send up to N discover packets (default 3)
            //-s,--script PROG	Run PROG at DHCP events (default /etc/udhcpc/default.script)
            snprintf(udhcpc_cmd, sizeof(udhcpc_cmd), "busybox udhcpc -f -n -q -t 5 -i %s", ifname);
#endif

#ifdef USE_DHCLIENT
            pthread_create(&udhcpc_thread_id, &udhcpc_thread_attr, udhcpc_thread_function, (void*)strdup(udhcpc_cmd));
            sleep(1);
#else
            if (profile->rawIP != 0)
            {
                set_rawip_data_format(ifname);
            }
            pthread_create(&udhcpc_thread_id, NULL, udhcpc_thread_function, (void*)strdup(udhcpc_cmd));
            pthread_join(udhcpc_thread_id, NULL);
#endif
        }

        if (profile->ipv6.Address[0] && profile->ipv6.PrefixLengthIPAddr) {
#ifdef USE_DHCLIENT
            snprintf(udhcpc_cmd, sizeof(udhcpc_cmd), "dhclient -6 -d --no-pid %s",  ifname);
            dhclient_alive++;
#else
            /*
                DHCPv6: Dibbler - a portable DHCPv6
                1. download from http://klub.com.pl/dhcpv6/
                2. cross-compile
                    2.1 ./configure --host=arm-linux-gnueabihf
                    2.2 copy dibbler-client to your board
                3. mkdir -p /var/log/dibbler/ /var/lib/ on your board
                4. create /etc/dibbler/client.conf on your board, the content is
                    log-mode short
                    log-level 7
                    iface wwan0 {
                        ia
                        option dns-server
                    }
                 5. run "dibbler-client start" to get ipV6 address
                 6. run "route -A inet6 add default dev wwan0" to add default route
            */
            snprintf(shell_cmd, sizeof(shell_cmd), "route -A inet6 add default %s", ifname);
            sc_system(shell_cmd);
            snprintf(udhcpc_cmd, sizeof(udhcpc_cmd), "dibbler-client run");
            dibbler_client_alive++;
#endif

            pthread_create(&udhcpc_thread_id, &udhcpc_thread_attr, udhcpc_thread_function, (void*)strdup(udhcpc_cmd));
        }
    }
}

void udhcpc_stop(PROFILE_T *profile) {
    char *ifname = profile->usbnet_adapter;
    char shell_cmd[128];

    if (profile->qmapnet_adapter) {
        ifname = profile->qmapnet_adapter;
    }

#ifdef USE_DHCLIENT
    if (dhclient_alive) {
        system("killall dhclient");
        dhclient_alive = 0;
    }
#endif
    if (dibbler_client_alive) {
        system("killall dibbler-client");
        dibbler_client_alive = 0;
    }
    snprintf(shell_cmd, sizeof(shell_cmd), "ifconfig %s 0.0.0.0", ifname);
    sc_system(shell_cmd);
    snprintf(shell_cmd, sizeof(shell_cmd), "ifconfig %s down", ifname);
    sc_system(shell_cmd);    
}
