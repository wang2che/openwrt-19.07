#ifndef __QMI_THREAD_H__
#define __QMI_THREAD_H__

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <stddef.h>
#include <syslog.h>

#include "MPQMI.h"
#include "MPQCTL.h"
#include "MPQMUX.h"

struct wwan_data_class_str {
    ULONG class;
    CHAR *str;
};

#pragma pack(push, 1)

typedef struct _QCQMIMSG {
    QCQMI_HDR QMIHdr;
    union {
        QMICTL_MSG CTLMsg;
        QMUX_MSG MUXMsg;
    };
} __attribute__ ((packed)) QCQMIMSG, *PQCQMIMSG;

#pragma pack(pop)


typedef enum {
    SIM_ABSENT = 0,
    SIM_NOT_READY = 1,
    SIM_READY = 2, /* SIM_READY means the radio state is RADIO_STATE_SIM_READY */
    SIM_PIN = 3,
    SIM_PUK = 4,
    SIM_NETWORK_PERSONALIZATION = 5,
    SIM_BAD = 6,
} SIM_Status;

typedef struct __IPV4 {
    uint32_t Address;
    uint32_t Gateway;
    uint32_t SubnetMask;
    uint32_t DnsPrimary;
    uint32_t DnsSecondary;
    uint32_t Mtu;
} IPV4_T;

typedef struct __IPV6 {
    UCHAR Address[16];
    UCHAR Gateway[16];
    UCHAR SubnetMask[16];
    UCHAR DnsPrimary[16];
    UCHAR DnsSecondary[16];
    UCHAR PrefixLengthIPAddr;
    UCHAR PrefixLengthGateway;
    ULONG Mtu;
} IPV6_T;

#define MAX_PROFILE_COUNT 4

#define IpFamilyV4 (0x04)
#define IpFamilyV6 (0x06)
typedef struct __PROFILE {


    int localProfileIndex;
    int modemProfileIndex;
    int qmap_mux_id;

    const char *apn;
    int auth;
    const char *user;
    const char *password;

    int IsDualIPSupported;
    int curIpFamily;

    IPV4_T ipv4;
    IPV6_T ipv6;

} PROFILE_T;

#define WDM_DEFAULT_BUFSIZE	256
#define RIL_REQUEST_QUIT    0x1000
#define RIL_INDICATE_DEVICE_CONNECTED    0x1002
#define RIL_INDICATE_DEVICE_DISCONNECTED    0x1003
#define RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED    0x1004
#define RIL_UNSOL_DATA_CALL_LIST_CHANGED    0x1005

extern int pthread_cond_timeout_np(pthread_cond_t *cond, pthread_mutex_t * mutex, unsigned msecs);
extern int QmiThreadSendQMI(PQCQMIMSG pRequest, PQCQMIMSG *ppResponse);
extern int QmiThreadSendQMITimeout(PQCQMIMSG pRequest, PQCQMIMSG *ppResponse, unsigned msecs);
extern void QmiThreadRecvQMI(PQCQMIMSG pResponse);
extern int QmiWwanInit(void);
extern int QmiWwanDeInit(void);
extern int QmiWwanSendQMI(PQCQMIMSG pRequest);
extern void * QmiWwanThread(void *pData);

extern void dump_qmi(void *dataBuffer, int dataLen);
extern void qmidevice_send_event_to_main(int triger_event);
extern int requestSetEthMode(void);
extern int requestBindMuxDataPort(void);

extern int requestQueryDataCall(UCHAR  *pConnectionStatus,PROFILE_T *profile, int curIpFamily);
extern int requestSetupDataCall(PROFILE_T *profile, int curIpFamily);
extern int requestDeactivateDefaultPDP(PROFILE_T *profile, int curIpFamily);
extern int requestCreateProfile(PROFILE_T *profile);
extern int requestSetProfile(PROFILE_T *profile);
extern int requestGetProfile(PROFILE_T *profile);
extern int requestBaseBandVersion(const char **pp_reversion);
extern int requestSetLteBandPriority(UCHAR *blist);
extern int requestGetPSAttachState(UCHAR *pPSAttachedState);
extern int requestGetLteAttachParams(char *pApn);
extern int requestGetIPAddress(PROFILE_T *profile, int curIpFamily);
extern int requestGetPINStatus(SIM_Status *pSIMStatus);
extern int requestGetSIMStatus(SIM_Status *pSIMStatus);
extern int requestISIMAuthenticate(UCHAR *nonce);


extern FILE *logfilefp;
extern int debug_qmi;
extern char * qmichannel;
extern int qmidevice_control_fd[2];
extern int qmiclientId[QMUX_TYPE_WDS_ADMIN + 1];

extern void dbg_time (const char *fmt, ...);
extern USHORT le16_to_cpu(USHORT v16);
extern UINT  le32_to_cpu (UINT v32);
extern UINT  ql_swap32(UINT v32);
extern USHORT cpu_to_le16(USHORT v16);
extern UINT cpu_to_le32(UINT v32);
#endif
