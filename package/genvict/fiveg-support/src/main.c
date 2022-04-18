/*
 *
 * Multiple-Connection-Manager for SIM7500/SIM7600
 * Version 1
 * Author xiaobin.wang@simcom.com
 * 123787504@qq.com
 *
*/

#include "QMIThread.h"
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <dirent.h>
#include <syslog.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#define PROGRAM_VERSION "V20210115"
int debug_qmi = 0;
int main_loop = 0;
char * qmichannel;
char * usbnet_adapter;
int qmidevice_control_fd[2];
static int signal_control_fd[2];
pthread_t gQmiThreadID;
int num_of_profile = 2;

static int s_link[MAX_PROFILE_COUNT];

static int is_didiport(int port)
{
    int ret;
    char* portenv = NULL;
    char temp[8]={'\0'};
    ret=access("/tmp/.ddnet",F_OK);
    if((ret == 0) && (port==0)){
        syslog(LOG_INFO,"didi network detected.");
        return 1;
    }

    portenv=getenv("ddport");
    if(portenv == NULL){
        syslog(LOG_DEBUG,"qmimux%d is not didi network.\n",port);
        return 0;
    }

    snprintf(temp,8,"qmimux%d",port);
    ret=memcmp(temp,portenv,7);
    if(ret==0){
        syslog(LOG_DEBUG,"qmimux%d is didi network.\n",port);
        return 1;
    }

    return 0;
}

static void usbnet_link_change(int link, PROFILE_T *profile) {
    char* privnet_network =NULL;
    char* privnet_netmask =NULL;
    char* privnet_gateway =NULL;
    unsigned char *r = NULL;
    unsigned char *dns1 = NULL;
    unsigned char *dns2 = NULL;
    unsigned char* addr = NULL;
    char cmd[128] = {0};

    syslog(LOG_INFO,"usbnet_link_change() ID = mux%d, LINK = %d", profile->localProfileIndex, link);

    if (s_link[profile->localProfileIndex] == link)
        return;

    s_link[profile->localProfileIndex] = link;

    if (link)
    {
        if(profile->curIpFamily == IpFamilyV4)
        {
            requestGetIPAddress(profile, IpFamilyV4);

            r = (unsigned char *)&profile->ipv4.Address;
            dns1= (unsigned char *)&profile->ipv4.DnsPrimary;
            dns2= (unsigned char *)&profile->ipv4.DnsSecondary;
            syslog(LOG_INFO,"Mux%d local IP address %d.%d.%d.%d", profile->localProfileIndex, r[3], r[2], r[1], r[0]);
            syslog(LOG_INFO,"Mux%d Primary DNS Server address %d.%d.%d.%d", profile->localProfileIndex, dns1[3], dns1[2], dns1[1], dns1[0]);
            syslog(LOG_INFO,"Mux%d Secondary DNS Server address %d.%d.%d.%d", profile->localProfileIndex, dns2[3], dns2[2], dns2[1], dns2[0]);
            if(profile->localProfileIndex == 0)
            {
                system("ip link set qmimux0 down");
                snprintf(cmd, sizeof(cmd), "ip address add %d.%d.%d.%d dev qmimux0",r[3], r[2], r[1], r[0]);
                system(cmd);
                system("ip link set qmimux0 up");
            }
            else if(profile->localProfileIndex == 1)
            {
                system("ip link set qmimux1 down");
                snprintf(cmd, sizeof(cmd), "ip address add %d.%d.%d.%d dev qmimux1",r[3], r[2], r[1], r[0]);
                system(cmd);
                system("ip link set qmimux1 up");
            }
            else if(profile->localProfileIndex == 2)
            {
                system("ip link set qmimux2 down");
                snprintf(cmd, sizeof(cmd), "ip address add %d.%d.%d.%d dev qmimux2",r[3], r[2], r[1], r[0]);
                system(cmd);
                system("ip link set qmimux2 up");
            }
            else if(profile->localProfileIndex == 3)
            {
                system("ip link set qmimux3 down");
                snprintf(cmd, sizeof(cmd), "ip address add %d.%d.%d.%d dev qmimux3",r[3], r[2], r[1], r[0]);
                system(cmd);
                system("ip link set qmimux3 up");
            }
            else
            {
                syslog(LOG_INFO,"!!ERROR!!");
            }

            r = (unsigned char *)&profile->ipv4.Gateway;
            syslog(LOG_INFO,"Mux%d GW IP address %d.%d.%d.%d", profile->localProfileIndex, r[3], r[2], r[1], r[0]);

#if 0
            if(profile->localProfileIndex == 0)
            {
                snprintf(cmd, sizeof(cmd), "ip route add default via %d.%d.%d.%d dev qmimux0", r[3], r[2], r[1], r[0]);
            }
            else if(profile->localProfileIndex == 1)
            {
                snprintf(cmd, sizeof(cmd), "ip route add default via %d.%d.%d.%d dev qmimux1", r[3], r[2], r[1], r[0]);
            }
            else if(profile->localProfileIndex == 2)
            {
                snprintf(cmd, sizeof(cmd), "ip route add default via %d.%d.%d.%d dev qmimux2", r[3], r[2], r[1], r[0]);
            }
            else if(profile->localProfileIndex == 3)
            {
                snprintf(cmd, sizeof(cmd), "ip route add default via %d.%d.%d.%d dev qmimux3", r[3], r[2], r[1], r[0]);
            }
            else
            {
                syslog(LOG_INFO,"!!ERROR!!");
            }
            syslog(LOG_INFO,"%s\n",cmd);
            system(cmd);
#else
            if(profile->localProfileIndex == 0)
            {
                if(is_didiport(profile->localProfileIndex)){
                    privnet_network=getenv("NETWORK");
                    privnet_netmask=getenv("NETMASK");
                    privnet_gateway=getenv("GATEWAY");

                    if( ( NULL == privnet_gateway  ) || ( 0 == strlen(privnet_gateway) ) ){
                        snprintf(cmd, sizeof(cmd), "route add -net %s netmask %s dev qmimux0", privnet_network, privnet_netmask);
                    }else{
                        snprintf(cmd, sizeof(cmd), "route add -net %s netmask %s gateway %s dev qmimux0", privnet_network, privnet_netmask, privnet_gateway);
                    }
                    syslog(LOG_INFO,"%s\n",cmd);
                    system(cmd);
                    if(access("/tmp/.ddroutedone",F_OK) != 0){
                        creat("/tmp/.ddroutedone", 0777);
                    }
                }else{
                    syslog(LOG_INFO,"do dhcp operation for qmimux0\n");
                    system("/sbin/udhcpc -i qmimux0");
                    if(access("/tmp/.pdnroutedone",F_OK) != 0){
                        creat("/tmp/.pdnroutedone", 0777);
                    }
                }
            }
            else if(profile->localProfileIndex == 1)
            {
                if(is_didiport(profile->localProfileIndex)){
                    privnet_network=getenv("NETWORK");
                    privnet_netmask=getenv("NETMASK");
                    privnet_gateway=getenv("GATEWAY");
                    if( ( NULL == privnet_gateway  ) || ( 0 == strlen(privnet_gateway) ) ){
                        snprintf(cmd, sizeof(cmd), "route add -net %s netmask %s dev qmimux1", privnet_network, privnet_netmask);
                    }else{
                        snprintf(cmd, sizeof(cmd), "route add -net %s netmask %s gateway %s dev qmimux1", privnet_network, privnet_netmask, privnet_gateway);
                    }
                    syslog(LOG_INFO,"%s\n",cmd);
                    system(cmd);
                    if(access("/tmp/.ddroutedone",F_OK) != 0){
                        creat("/tmp/.ddroutedone", 0777);
                    }
                }else{
                    syslog(LOG_INFO,"do dhcp operation for qmimux1\n");
                    system("/sbin/udhcpc -i qmimux1");
                    if(access("/tmp/.pdnroutedone",F_OK) != 0){
                        creat("/tmp/.pdnroutedone", 0777);
                    }
                }
            }
            else
            {
                syslog(LOG_INFO,"!!ERROR!!");
            }
#endif
        }
        else if(profile->curIpFamily == IpFamilyV6)
        {
            requestGetIPAddress(profile, IpFamilyV6);

            addr = (unsigned char *)&profile->ipv6.Address;

            syslog(LOG_INFO,"Mux%d local IP address %s", profile->localProfileIndex, addr);
            syslog(LOG_INFO,"Mux%d local PrefixLengthIPAddr is %d", profile->localProfileIndex, profile->ipv6.PrefixLengthIPAddr);

            if(profile->localProfileIndex == 0)
            {
                // here you should assing this mtu value to the interface..
            }
            else if(profile->localProfileIndex == 1)
            {
            }
            else if(profile->localProfileIndex == 2)
            {
            }
            else if(profile->localProfileIndex == 3)
            {
            }
            else
            {
                syslog(LOG_INFO,"!!ERROR!!");
            }
        }
    }
    else
    {

    }
}

static void main_send_event_to_qmidevice(int triger_event) {
     write(qmidevice_control_fd[0], &triger_event, sizeof(triger_event));
}

static void send_signo_to_main(int signo) {
     write(signal_control_fd[0], &signo, sizeof(signo));
}

void qmidevice_send_event_to_main(int triger_event) {
     write(qmidevice_control_fd[1], &triger_event, sizeof(triger_event));
}

static void ql_sigaction(int signo) {
     if (SIGCHLD == signo)
         waitpid(-1, NULL, WNOHANG);
     else if (SIGALRM == signo)
         send_signo_to_main(SIGUSR1);
     else
     {
        if (SIGTERM == signo || SIGHUP == signo || SIGINT == signo)
            main_loop = 0;
        send_signo_to_main(signo);
        main_send_event_to_qmidevice(signo); //main may be wating qmi response
    }
}

static int qmidevice_detect(char **pp_qmichannel, char **pp_usbnet_adapter) {
    struct dirent* ent = NULL;
    DIR *pDir;
    if ((pDir = opendir("/dev")) == NULL)
    {
        syslog(LOG_INFO,"Cannot open directory: %s, errno:%d (%s)", "/dev", errno, strerror(errno));
        return -ENODEV;
    }

    while ((ent = readdir(pDir)) != NULL)
    {
        if ((strncmp(ent->d_name, "cdc-wdm", strlen("cdc-wdm")) == 0) || (strncmp(ent->d_name, "qcqmi", strlen("qcqmi")) == 0))
        {
            char net_path[64];

            *pp_qmichannel = (char *)malloc(32);
            sprintf(*pp_qmichannel, "/dev/%s", ent->d_name);
            syslog(LOG_INFO,"Find qmichannel = %s", *pp_qmichannel);

            if (strncmp(ent->d_name, "cdc-wdm", strlen("cdc-wdm")) == 0)
            {
                //sprintf(net_path, "/sys/class/net/wwan%s", &ent->d_name[strlen("cdc-wdm")]);
                struct dirent* wwanEnt = NULL;
                DIR *pWwanDir;
                if ((pWwanDir = opendir("/sys/class/net")) == NULL)
                {
                    syslog(LOG_INFO,"Cannot open directory: %s, errno:%d (%s)", "/dev", errno, strerror(errno));
                    return -ENODEV;
                }
                while ((wwanEnt = readdir(pWwanDir)) != NULL)
                {
                    if ((strncmp(wwanEnt->d_name, "wwp", strlen("wwp")) == 0) || (strncmp(wwanEnt->d_name, "wwan", strlen("wwan")) == 0))
                    {
                        sprintf(net_path, "/sys/class/net/%s", wwanEnt->d_name);
                        break;
                    }
                }
            }
            else
            {
                sprintf(net_path, "/sys/class/net/usb%s", &ent->d_name[strlen("qcqmi")]);
                #if 0//ndef ANDROID
                if (kernel_version >= KVERSION( 2,6,39 ))
                    sprintf(net_path, "/sys/class/net/eth%s", &ent->d_name[strlen("qcqmi")]);
                #else
                if (access(net_path, R_OK) && errno == ENOENT)
                    sprintf(net_path, "/sys/class/net/eth%s", &ent->d_name[strlen("qcqmi")]);
                #endif
#if 0 //openWRT like use ppp# or lte#
                if (access(net_path, R_OK) && errno == ENOENT)
                    sprintf(net_path, "/sys/class/net/ppp%s", &ent->d_name[strlen("qcqmi")]);
                if (access(net_path, R_OK) && errno == ENOENT)
                    sprintf(net_path, "/sys/class/net/lte%s", &ent->d_name[strlen("qcqmi")]);
#endif
            }

            if (access(net_path, R_OK) == 0)
            {
                if (*pp_usbnet_adapter && strcmp(*pp_usbnet_adapter, (net_path + strlen("/sys/class/net/"))))
                {
                    free(*pp_qmichannel); *pp_qmichannel = NULL;
                    continue;
                }
                *pp_usbnet_adapter = strdup(net_path + strlen("/sys/class/net/"));
                syslog(LOG_INFO,"Find usbnet_adapter = %s", *pp_usbnet_adapter);
                break;
            }
            else
            {
                syslog(LOG_INFO,"Failed to access %s, errno:%d (%s)", net_path, errno, strerror(errno));
                free(*pp_qmichannel); *pp_qmichannel = NULL;
            }
        }
    }
    closedir(pDir);

    return (*pp_qmichannel && *pp_usbnet_adapter);
}

static const char *helptext =
"Usage: multiplex-CM [options]\n\n"
"Examples:\n"
"Options:\n"
"-c, specify the count of the profiles \n"
"-h, help display this help text\n\n";

static int usage(const char *progname) {
    (void)progname;
     printf("%s", helptext);
     return 0;
}

#define has_more_argv() ((opt < argc) && (argv[opt][0] != '-'))
int main(int argc, char *argv[])
{
    int opt = 1;
    int triger_event = 0;
    int signo;
    int loop = 0;
    UCHAR PsAttachedState = 0;
    SIM_Status SIMStatus = SIM_ABSENT;
    UCHAR  IPv4ConnectionStatus[MAX_PROFILE_COUNT]; //unknow state
    UCHAR  IPv6ConnectionStatus[MAX_PROFILE_COUNT];  //unknow state
    char * save_usbnet_adapter = NULL;
    PROFILE_T profile[MAX_PROFILE_COUNT];

    char* devstr = NULL;

    if (!strcmp(argv[argc-1], "&"))
        argc--;

    opt = 1;
    while  (opt < argc) {
        if (argv[opt][0] != '-')
            return usage(argv[0]);

        switch (argv[opt++][1])
        {
            case 'c':
                if (has_more_argv())
                    num_of_profile = argv[opt++][0] - '0';
                break;

            default:
                return usage(argv[0]);
            break;
        }
    }

    syslog(LOG_INFO,"multiplex-CM version: %s", PROGRAM_VERSION);

    qmidevice_detect(&devstr, &save_usbnet_adapter);

    usbnet_adapter = save_usbnet_adapter;

    memset(&IPv4ConnectionStatus[0],0xff,MAX_PROFILE_COUNT*sizeof(UCHAR));
    memset(&IPv6ConnectionStatus[0],0xff,MAX_PROFILE_COUNT*sizeof(UCHAR));
    memset(&s_link[0], 0, MAX_PROFILE_COUNT*sizeof(int));

    for(loop = 0; loop < MAX_PROFILE_COUNT; loop++)
    {
        memset(&profile[loop], 0x00, sizeof(profile[loop]));
        profile[loop].localProfileIndex = loop;
        switch(loop)
        {
        case 0:
            /*
             * local 0 <-> modem cgdcont 1
             */
            profile[loop].modemProfileIndex = 1;
            break;
        default:
            /*
             * Skip the modem profile 2 and 3. Starting from 4
             * local 1 <-> modem cgdcont 4
             * local 2 <-> modem cgdcont 5
             * local 3 <-> modem cgdcont 6
             */
            profile[loop].modemProfileIndex = 3+loop;
            break;
        }
        profile[loop].curIpFamily = IpFamilyV4;
    }

    signal(SIGUSR1, ql_sigaction);
    signal(SIGUSR2, ql_sigaction);
    signal(SIGINT, ql_sigaction);
    signal(SIGTERM, ql_sigaction);
    signal(SIGHUP, ql_sigaction);
    signal(SIGCHLD, ql_sigaction);
    signal(SIGALRM, ql_sigaction);

    if (socketpair( AF_LOCAL, SOCK_STREAM, 0, signal_control_fd) < 0 ) {
        syslog(LOG_INFO,"%s Faild to create main_control_fd: %d (%s)", __func__, errno, strerror(errno));
        return -1;
    }

    if ( socketpair( AF_LOCAL, SOCK_STREAM, 0, qmidevice_control_fd ) < 0 ) {
        syslog(LOG_INFO,"%s Failed to create thread control socket pair: %d (%s)", __func__, errno, strerror(errno));
        return 0;
    }

  __main_loop:
    qmichannel = devstr;
    if (access(qmichannel, R_OK | W_OK)) {
        syslog(LOG_INFO,"Fail to access %s, errno: %d (%s)", qmichannel, errno, strerror(errno));
        return errno;
    }

    if (pthread_create( &gQmiThreadID, 0, QmiWwanThread, (void *)qmichannel) != 0)
    {
        syslog(LOG_INFO,"%s Failed to create QmiWwanThread: %d (%s)", __func__, errno, strerror(errno));
        return 0;
    }

    if ((read(qmidevice_control_fd[0], &triger_event, sizeof(triger_event)) != sizeof(triger_event))
        || (triger_event != RIL_INDICATE_DEVICE_CONNECTED)) {
        syslog(LOG_INFO,"%s Failed to init QMIThread: %d (%s)", __func__, errno, strerror(errno));
        return 0;
    }

    if (QmiWwanInit()) {
        syslog(LOG_INFO,"%s Failed to QmiWwanInit: %d (%s)", __func__, errno, strerror(errno));
        return 0;
    }

    requestBaseBandVersion(NULL);
    requestSetEthMode();
    requestBindMuxDataPort();

    while (SIMStatus != SIM_READY)
    {
        requestGetSIMStatus(&SIMStatus);
        if ((SIMStatus != SIM_READY)) {
            syslog(LOG_INFO,"SIM Card[status %d] not Ready!", SIMStatus);
            sleep(1);
        }
    }
    while (PsAttachedState != 1)
    {
        requestGetPSAttachState(&PsAttachedState);
        if ((PsAttachedState != 1)) {
            syslog(LOG_INFO,"PS not attached !");
            sleep(1);
        }
    }

    send_signo_to_main(SIGUSR1);

    while (1)
    {
        struct pollfd pollfds[] = {{signal_control_fd[1], POLLIN, 0}, {qmidevice_control_fd[0], POLLIN, 0}};
        int ne, ret, nevents = sizeof(pollfds)/sizeof(pollfds[0]);

        do {
            ret = poll(pollfds, nevents,  15*1000);
        } while ((ret < 0) && (errno == EINTR));

        if (ret == 0)
        {
            #if 0
            send_signo_to_main(SIGUSR2);
            #endif
            continue;
        }

        if (ret <= 0) {
            syslog(LOG_INFO,"%s poll=%d, errno: %d (%s)", __func__, ret, errno, strerror(errno));
            goto __main_quit;
        }

        for (ne = 0; ne < nevents; ne++) {
            int fd = pollfds[ne].fd;
            short revents = pollfds[ne].revents;

            if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
                syslog(LOG_INFO,"%s poll err/hup", __func__);
                syslog(LOG_INFO,"epoll fd = %d, events = 0x%04x", fd, revents);
                main_send_event_to_qmidevice(RIL_REQUEST_QUIT);
                if (revents & POLLHUP)
                    goto __main_quit;
            }

            if ((revents & POLLIN) == 0)
                continue;

            if (fd == signal_control_fd[1])
            {
                if (read(fd, &signo, sizeof(signo)) == sizeof(signo))
                {
                    alarm(0);
                    switch (signo)
                    {
                        case SIGUSR1:

                            for(loop = 0; loop < num_of_profile; loop++)
                            {

                              if(profile[loop].curIpFamily == IpFamilyV6)
                              {
                                requestQueryDataCall(&IPv6ConnectionStatus[loop], &profile[loop], IpFamilyV6);

                                if (QWDS_PKT_DATA_CONNECTED != IPv6ConnectionStatus[loop])
                                {
                                    usbnet_link_change(0, &profile[loop]);

                                    if ( /* PSAttachedState == 1 && */ requestSetupDataCall(&profile[loop], IpFamilyV6) == 0)
                                    {
                                    }
                                    else
                                    {
                                        alarm(5); // try to setup data call 5 seconds later
                                    }
                                }
                              }else
                              {
                                requestQueryDataCall(&IPv4ConnectionStatus[loop], &profile[loop], IpFamilyV4);

                                if (QWDS_PKT_DATA_CONNECTED != IPv4ConnectionStatus[loop])
                                {
                                    usbnet_link_change(0, &profile[loop]);

                                    if ( /*PSAttachedState == 1 &&*/ requestSetupDataCall(&profile[loop], IpFamilyV4) == 0)
                                    {
                                    }
                                    else
                                    {
                                        alarm(5); // try to setup data call 5 seconds later
                                    }
                                }
                              }

                            }
                        break;

                        case SIGUSR2:
#if 0
                            for(loop = 0; loop < MAX_PROFILE_COUNT; loop++)
                            {

                              if(profile[loop].curIpFamily == IpFamilyV6)
                              {
                                if (QWDS_PKT_DATA_CONNECTED == IPv6ConnectionStatus[loop])
                                     requestQueryDataCall(&IPv4ConnectionStatus[loop],&profile[loop], IpFamilyV6);

                                // local ip is different with remote ip
                                /*
                                if (QWDS_PKT_DATA_CONNECTED == IPv4ConnectionStatus[loop] && check_ipv4_address(&profile[loop]) == 0) {
                                    requestDeactivateDefaultPDP(&profile[loop], IpFamilyV6);
                                    IPv4ConnectionStatus[loop] = QWDS_PKT_DATA_DISCONNECTED;
                                }
                                */
                                if (QWDS_PKT_DATA_CONNECTED != IPv6ConnectionStatus[loop]) {
                                    send_signo_to_main(SIGUSR1);
                                }
                              }else
                              {
                                if (QWDS_PKT_DATA_CONNECTED == IPv4ConnectionStatus[loop])
                                     requestQueryDataCall(&IPv4ConnectionStatus[loop],&profile[loop], IpFamilyV4);

                                // local ip is different with remote ip
                                /*
                                if (QWDS_PKT_DATA_CONNECTED == IPv4ConnectionStatus[loop] && check_ipv4_address(&profile[loop]) == 0) {
                                    requestDeactivateDefaultPDP(&profile[loop], IpFamilyV6);
                                    IPv4ConnectionStatus[loop] = QWDS_PKT_DATA_DISCONNECTED;
                                }
                                */
                                if (QWDS_PKT_DATA_CONNECTED != IPv4ConnectionStatus[loop]) {
                                    send_signo_to_main(SIGUSR1);
                                }
                              }

                            }// for
      #endif
                        break;

                        case SIGTERM:
                        case SIGHUP:
                        case SIGINT:
                            for(loop = 0; loop < num_of_profile; loop++)
                            {
                              if(profile[loop].curIpFamily == IpFamilyV6)
                              {
                                if (QWDS_PKT_DATA_CONNECTED == IPv6ConnectionStatus[loop]) {
                                    requestDeactivateDefaultPDP(&profile[loop], IpFamilyV6);
                                }
                              }
                              else
                              {
                                if (QWDS_PKT_DATA_CONNECTED == IPv4ConnectionStatus[loop]) {
                                    requestDeactivateDefaultPDP(&profile[loop], IpFamilyV4);
                                }
                              }
                              usbnet_link_change(0, &profile[loop]);
                            }

                            QmiWwanDeInit();
                            main_send_event_to_qmidevice(RIL_REQUEST_QUIT);
                            goto __main_quit;
                        break;

                        default:
                        break;
                    }
                }
            }

            if (fd == qmidevice_control_fd[0]) {
                if (read(fd, &triger_event, sizeof(triger_event)) == sizeof(triger_event)) {
                    switch (triger_event) {
                        case RIL_INDICATE_DEVICE_DISCONNECTED:
                            usbnet_link_change(0, &profile[loop]);
                            if (main_loop)
                            {
                                if (pthread_join(gQmiThreadID, NULL)) {
                                    syslog(LOG_INFO,"%s Error joining to listener thread (%s)", __func__, strerror(errno));
                                }
                                qmichannel = NULL;
                                usbnet_adapter = save_usbnet_adapter;
                                goto __main_loop;
                            }
                            goto __main_quit;
                        break;

                        case RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED:
                            for(loop = 0; loop < num_of_profile; loop++)
                            {
                              if (/*PSAttachedState == 1 && */QWDS_PKT_DATA_DISCONNECTED == IPv4ConnectionStatus[loop])
                                  send_signo_to_main(SIGUSR1);
                              break;
                            }
                        break;

                        case RIL_UNSOL_DATA_CALL_LIST_CHANGED:
                        {
                            for(loop =0;loop<num_of_profile;loop++)
                            {
                              if(profile[loop].curIpFamily == IpFamilyV6)
                              {
                                requestQueryDataCall(&IPv6ConnectionStatus[loop], &profile[loop], IpFamilyV6);

                                if (QWDS_PKT_DATA_CONNECTED != IPv6ConnectionStatus[loop])
                                {
                                    usbnet_link_change(0, &profile[loop]);
                                    send_signo_to_main(SIGUSR1);
                                } else if (QWDS_PKT_DATA_CONNECTED == IPv6ConnectionStatus[loop]) {
                                    usbnet_link_change(1, &profile[loop]);
                                }
                              }else
                              {
                                requestQueryDataCall(&IPv4ConnectionStatus[loop], &profile[loop], IpFamilyV4);

                                if (QWDS_PKT_DATA_CONNECTED != IPv4ConnectionStatus[loop])
                                {
                                    usbnet_link_change(0, &profile[loop]);
                                    send_signo_to_main(SIGUSR1);

                                } else if (QWDS_PKT_DATA_CONNECTED == IPv4ConnectionStatus[loop]) {
                                    usbnet_link_change(1, &profile[loop]);
                                }
                              }
                            }// for
                        }
                        break;

                        default:
                        break;
                    }
                }
            }
        }
    }

__main_quit:
    for(loop =0;loop<num_of_profile;loop++)
      usbnet_link_change(0, &profile[loop]);
    if (pthread_join(gQmiThreadID, NULL)) {
        syslog(LOG_INFO,"%s Error joining to listener thread (%s)", __func__, strerror(errno));
    }
    close(signal_control_fd[0]);
    close(signal_control_fd[1]);
    close(qmidevice_control_fd[0]);
    close(qmidevice_control_fd[1]);
    syslog(LOG_INFO,"%s exit", __func__);
    if (logfilefp)
        fclose(logfilefp);
    if (qmichannel)
        free((void*)qmichannel);
    if (save_usbnet_adapter)
        free((void*)save_usbnet_adapter);

    return 0;
}
