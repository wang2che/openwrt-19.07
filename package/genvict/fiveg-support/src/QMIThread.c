

#include "QMIThread.h"

extern int num_of_profile;

int qmiclientId[QMUX_TYPE_WDS_ADMIN + 1];

static uint32_t WdsConnectionIPv4Handle[MAX_PROFILE_COUNT];
static uint32_t WdsConnectionIPv6Handle[MAX_PROFILE_COUNT];


static char *qstrcpy(char *to, const char *from) { //no __strcpy_chk
    char *save = to;
    for (; (*to = *from) != '\0'; ++from, ++to);
    return(save);
}

typedef USHORT (*CUSTOMQMUX)(PQMUX_MSG pMUXMsg, void *arg);

// To retrieve the ith (Index) TLV
PQMI_TLV_HDR GetTLV (PQCQMUX_MSG_HDR pQMUXMsgHdr, int TLVType) {
    int TLVFind = 0;
    USHORT Length = le16_to_cpu(pQMUXMsgHdr->Length);
    PQMI_TLV_HDR pTLVHdr = (PQMI_TLV_HDR)(pQMUXMsgHdr + 1);

    while (Length >= sizeof(QMI_TLV_HDR)) {
        TLVFind++;
        if (TLVType > 0x1000) {
            if ((TLVFind + 0x1000) == TLVType)
                return pTLVHdr;
        } else  if (pTLVHdr->TLVType == TLVType) {
            return pTLVHdr;
        }

        Length -= (le16_to_cpu((pTLVHdr->TLVLength)) + sizeof(QMI_TLV_HDR));
        pTLVHdr = (PQMI_TLV_HDR)(((UCHAR *)pTLVHdr) + le16_to_cpu(pTLVHdr->TLVLength) + sizeof(QMI_TLV_HDR));
    }

   return NULL;
}

static USHORT GetQMUXTransactionId(void) {
    static int TransactionId = 0;
    if (++TransactionId > 0xFFFF)
        TransactionId = 1;
    return TransactionId;
}

static PQCQMIMSG ComposeQMUXMsg(UCHAR QMIType, USHORT Type, CUSTOMQMUX customQmuxMsgFunction, void *arg) {
    UCHAR QMIBuf[WDM_DEFAULT_BUFSIZE];
    PQCQMIMSG pRequest = (PQCQMIMSG)QMIBuf;
    int Length;

    memset(QMIBuf, 0x00, sizeof(QMIBuf));
    pRequest->QMIHdr.IFType = USB_CTL_MSG_TYPE_QMI;
    pRequest->QMIHdr.CtlFlags = 0x00;
    pRequest->QMIHdr.QMIType = QMIType;
    pRequest->QMIHdr.ClientId = qmiclientId[QMIType] & 0xFF;

    if (qmiclientId[QMIType] == 0) {
        syslog(LOG_INFO,"QMIType %d has no clientID", QMIType);
        return NULL;
    }

    pRequest->MUXMsg.QMUXHdr.CtlFlags = QMUX_CTL_FLAG_SINGLE_MSG | QMUX_CTL_FLAG_TYPE_CMD;
    pRequest->MUXMsg.QMUXHdr.TransactionId = cpu_to_le16(GetQMUXTransactionId());
    pRequest->MUXMsg.QMUXMsgHdr.Type = cpu_to_le16(Type);
    if (customQmuxMsgFunction)
        pRequest->MUXMsg.QMUXMsgHdr.Length = cpu_to_le16(customQmuxMsgFunction(&pRequest->MUXMsg, arg) - sizeof(QCQMUX_MSG_HDR));
    else
        pRequest->MUXMsg.QMUXMsgHdr.Length = cpu_to_le16(0x0000);

    pRequest->QMIHdr.Length = cpu_to_le16(le16_to_cpu(pRequest->MUXMsg.QMUXMsgHdr.Length) + sizeof(QCQMUX_MSG_HDR) + sizeof(QCQMUX_HDR)
        + sizeof(QCQMI_HDR) - 1);
    Length = le16_to_cpu(pRequest->QMIHdr.Length) + 1;

    pRequest = (PQCQMIMSG)malloc(Length);
    if (pRequest == NULL) {
        syslog(LOG_INFO,"%s fail to malloc", __func__);
    } else {
        memcpy(pRequest, QMIBuf, Length);
    }

    return pRequest;
}

static USHORT UimGetCardStausReq(PQMUX_MSG pMUXMsg, void *arg) {
   pMUXMsg->UIMGetCardStatusReq.TLVType = 0x10;
   pMUXMsg->UIMGetCardStatusReq.TLVLength = cpu_to_le16(0x01);
   pMUXMsg->UIMGetCardStatusReq.Extended_card_status = (UCHAR )0x00;

   return sizeof(QMIUIM_GET_CARD_STATUS_REQ_MSG);
}


static USHORT NasSetLteBandPriorityReq(PQMUX_MSG pMUXMsg, void *arg)
{
   pMUXMsg->SetLteBandPriorityReq.TLVType = 0x01;
   //pMUXMsg->SetLteBandPriorityReq.TLVLength = cpu_to_le16(0x08);
   pMUXMsg->SetLteBandPriorityReq.band_priority_list_len = (UCHAR)0x06;
   pMUXMsg->SetLteBandPriorityReq.band_priority_list[0] = 158;  /* band 28*/
   pMUXMsg->SetLteBandPriorityReq.band_priority_list[1] = 122;  /* band 3*/
   pMUXMsg->SetLteBandPriorityReq.band_priority_list[2] = 127;  /* band 8*/
   pMUXMsg->SetLteBandPriorityReq.band_priority_list[3] = 120;  /* band 1*/
   pMUXMsg->SetLteBandPriorityReq.band_priority_list[4] = 124; /* band 5*/
   pMUXMsg->SetLteBandPriorityReq.band_priority_list[5] = 126; /* band 7*/

   pMUXMsg->SetLteBandPriorityReq.TLVLength = cpu_to_le16(0x01 +  pMUXMsg->SetLteBandPriorityReq.band_priority_list_len*2);

   return sizeof(QMINAS_SET_LTE_BAND_PRIORITY_REQ_MSG) + pMUXMsg->SetLteBandPriorityReq.band_priority_list_len*2;
}

static USHORT WdsBindMuxDataPortReq(PQMUX_MSG pMUXMsg, void *arg) {

   UCHAR *muxid = (UCHAR *)arg;

   pMUXMsg->BindMuxDataPortReq.TLVType = 0x10;
   pMUXMsg->BindMuxDataPortReq.TLVLength = cpu_to_le16(0x08);
   pMUXMsg->BindMuxDataPortReq.ep_type = (ULONG )0x02;
   pMUXMsg->BindMuxDataPortReq.iface_id = (ULONG )0x05;

   pMUXMsg->BindMuxDataPortReq.TLV2Type = 0x11;
   pMUXMsg->BindMuxDataPortReq.TLV2Length = cpu_to_le16(0x01);
   pMUXMsg->BindMuxDataPortReq.MuxId = (UCHAR)(*muxid);

   pMUXMsg->BindMuxDataPortReq.TLV3Type = 0x13;
   pMUXMsg->BindMuxDataPortReq.TLV3Length = cpu_to_le16(0x04);
   pMUXMsg->BindMuxDataPortReq.client_type = (ULONG )0x01;

   return sizeof(QMIWDS_BIND_MUX_DATA_PORT_REQ_MSG);
}


static USHORT WdsStartNwInterfaceReq(PQMUX_MSG pMUXMsg, void *arg) {
    PQMIWDS_TECHNOLOGY_PREFERECE pTechPref;
    PQMIWDS_AUTH_PREFERENCE pAuthPref;
    PQMIWDS_USERNAME pUserName;
    PQMIWDS_PASSWD pPasswd;
    PQMIWDS_APNNAME pApnName;
    PQMIWDS_IP_FAMILY_TLV pIpFamily;
    USHORT TLVLength = 0;
    UCHAR *pTLV;
    PROFILE_T *profile = (PROFILE_T *)arg;
    const char *profile_user = profile->user;
    const char *profile_password = profile->password;
    int profile_auth = profile->auth;

    pTLV = (UCHAR *)(&pMUXMsg->StartNwInterfaceReq + 1);
    pMUXMsg->StartNwInterfaceReq.Length = 0;

    // Set technology Preferece
    pTechPref = (PQMIWDS_TECHNOLOGY_PREFERECE)(pTLV + TLVLength);
    pTechPref->TLVType = 0x30;
    pTechPref->TLVLength = cpu_to_le16(0x01);
    pTechPref->TechPreference = 0x01;

    TLVLength +=(le16_to_cpu(pTechPref->TLVLength) + sizeof(QCQMICTL_TLV_HDR));

    // Set APN Name
    if (profile->apn) {
        pApnName = (PQMIWDS_APNNAME)(pTLV + TLVLength);
        pApnName->TLVType = 0x14;
        pApnName->TLVLength = cpu_to_le16(strlen(profile->apn));
        qstrcpy((char *)&pApnName->ApnName, profile->apn);
        TLVLength +=(le16_to_cpu(pApnName->TLVLength) + sizeof(QCQMICTL_TLV_HDR));
    }

    // Set User Name
    if (profile_user) {
        pUserName = (PQMIWDS_USERNAME)(pTLV + TLVLength);
        pUserName->TLVType = 0x17;
        pUserName->TLVLength = cpu_to_le16(strlen(profile_user));
        qstrcpy((char *)&pUserName->UserName, profile_user);
        TLVLength += (le16_to_cpu(pUserName->TLVLength) + sizeof(QCQMICTL_TLV_HDR));
    }

    // Set Password
    if (profile_password) {
        pPasswd = (PQMIWDS_PASSWD)(pTLV + TLVLength);
        pPasswd->TLVType = 0x18;
        pPasswd->TLVLength = cpu_to_le16(strlen(profile_password));
        qstrcpy((char *)&pPasswd->Passwd, profile_password);
        TLVLength += (le16_to_cpu(pPasswd->TLVLength) + sizeof(QCQMICTL_TLV_HDR));
    }

    // Set Auth Protocol
    if (profile_user && profile_password) {
        pAuthPref = (PQMIWDS_AUTH_PREFERENCE)(pTLV + TLVLength);
        pAuthPref->TLVType = 0x16;
        pAuthPref->TLVLength = cpu_to_le16(0x01);
        pAuthPref->AuthPreference = profile_auth; // 0 ~ None, 1 ~ Pap, 2 ~ Chap, 3 ~ MsChapV2
        TLVLength += (le16_to_cpu(pAuthPref->TLVLength) + sizeof(QCQMICTL_TLV_HDR));
    }

    // Add IP Family Preference
    pIpFamily = (PQMIWDS_IP_FAMILY_TLV)(pTLV + TLVLength);
    pIpFamily->TLVType = 0x19;
    pIpFamily->TLVLength = cpu_to_le16(0x01);
    pIpFamily->IpFamily = profile->curIpFamily;
    TLVLength += (le16_to_cpu(pIpFamily->TLVLength) + sizeof(QCQMICTL_TLV_HDR));

    //Set Profile Index
    if (profile->localProfileIndex >= 0 ) {
        PQMIWDS_PROFILE_IDDEX pProfileIndex = (PQMIWDS_PROFILE_IDDEX)(pTLV + TLVLength);
        pProfileIndex->TLVLength = cpu_to_le16(0x01);
        pProfileIndex->TLVType = 0x31;
        pProfileIndex->ProfileIndex = profile->modemProfileIndex;
        TLVLength += (le16_to_cpu(pProfileIndex->TLVLength) + sizeof(QCQMICTL_TLV_HDR));
    }

    return sizeof(QMIWDS_START_NETWORK_INTERFACE_REQ_MSG) + TLVLength;
}

static USHORT WdsStopNwInterfaceReq(PQMUX_MSG pMUXMsg, void *arg) {
    PROFILE_T *profile = (PROFILE_T *)arg;
    pMUXMsg->StopNwInterfaceReq.TLVType = 0x01;
    pMUXMsg->StopNwInterfaceReq.TLVLength = cpu_to_le16(0x04);
    if (profile->curIpFamily == IpFamilyV4)
        pMUXMsg->StopNwInterfaceReq.Handle =  cpu_to_le32(WdsConnectionIPv4Handle[profile->localProfileIndex]);
    else
        pMUXMsg->StopNwInterfaceReq.Handle =  cpu_to_le32(WdsConnectionIPv6Handle[profile->localProfileIndex]);
    return sizeof(QMIWDS_STOP_NETWORK_INTERFACE_REQ_MSG);
}

#if 1
static USHORT WdaSetDataFormat(PQMUX_MSG pMUXMsg, void *arg) {


    //Indicates whether the Quality of Service(QOS) data format is used by the client.
        pMUXMsg->SetDataFormatReq.QosDataFormatTlv.TLVType = 0x10;
        pMUXMsg->SetDataFormatReq.QosDataFormatTlv.TLVLength = cpu_to_le16(0x0001);
        pMUXMsg->SetDataFormatReq.QosDataFormatTlv.QOSSetting = 0; /* no-QOS header */

    //Underlying Link Layer Protocol
        pMUXMsg->SetDataFormatReq.UnderlyingLinkLayerProtocolTlv.TLVType = 0x11;
        pMUXMsg->SetDataFormatReq.UnderlyingLinkLayerProtocolTlv.TLVLength = cpu_to_le16(4);
        pMUXMsg->SetDataFormatReq.UnderlyingLinkLayerProtocolTlv.Value = cpu_to_le32(0x02);     /* Set IP  mode */

    //Uplink (UL) data aggregation protocol to be used for uplink data transfer.
        pMUXMsg->SetDataFormatReq.UplinkDataAggregationProtocolTlv.TLVType = 0x12;
        pMUXMsg->SetDataFormatReq.UplinkDataAggregationProtocolTlv.TLVLength = cpu_to_le16(4);
        pMUXMsg->SetDataFormatReq.UplinkDataAggregationProtocolTlv.Value = cpu_to_le32(0x05); //UL QMAP is enabled

    //Downlink (DL) data aggregation protocol to be used for downlink data transfer
        pMUXMsg->SetDataFormatReq.DownlinkDataAggregationProtocolTlv.TLVType = 0x13;
        pMUXMsg->SetDataFormatReq.DownlinkDataAggregationProtocolTlv.TLVLength = cpu_to_le16(4);
        pMUXMsg->SetDataFormatReq.DownlinkDataAggregationProtocolTlv.Value = cpu_to_le32(0x05); //DL QMAP is enabled

    //Maximum number of datagrams in a single aggregated packet on downlink
        pMUXMsg->SetDataFormatReq.DownlinkDataAggregationMaxDatagramsTlv.TLVType = 0x15;
        pMUXMsg->SetDataFormatReq.DownlinkDataAggregationMaxDatagramsTlv.TLVLength = cpu_to_le16(4);
        pMUXMsg->SetDataFormatReq.DownlinkDataAggregationMaxDatagramsTlv.Value = cpu_to_le32(16384/512);

    //Maximum size in bytes of a single aggregated packet allowed on downlink
        pMUXMsg->SetDataFormatReq.DownlinkDataAggregationMaxSizeTlv.TLVType = 0x16;
        pMUXMsg->SetDataFormatReq.DownlinkDataAggregationMaxSizeTlv.TLVLength = cpu_to_le16(4);
        pMUXMsg->SetDataFormatReq.DownlinkDataAggregationMaxSizeTlv.Value = cpu_to_le32(16384);

    //Peripheral End Point ID
        pMUXMsg->SetDataFormatReq.epTlv.TLVType = 0x17;
        pMUXMsg->SetDataFormatReq.epTlv.TLVLength = cpu_to_le16(8);
        pMUXMsg->SetDataFormatReq.epTlv.ep_type = cpu_to_le32(0x000002);
        pMUXMsg->SetDataFormatReq.epTlv.iface_id = cpu_to_le32(0x0000005);

        return sizeof(QMIWDS_ADMIN_SET_DATA_FORMAT_REQ_MSG);
}
#else
static USHORT WdaSetDataFormat(PQMUX_MSG pMUXMsg, void *arg) {
    PQMIWDS_ADMIN_SET_DATA_FORMAT_TLV_QOS pWdsAdminQosTlv;
    PQMIWDS_ADMIN_SET_DATA_FORMAT_TLV linkProto;
    PQMIWDS_ADMIN_SET_DATA_FORMAT_TLV ulTlp;
    PQMIWDS_ADMIN_SET_DATA_FORMAT_TLV dlTlp;
    PQMIWDS_ADMIN_SET_DATA_FORMAT_TLV_EPID ep_id;

    pWdsAdminQosTlv = (PQMIWDS_ADMIN_SET_DATA_FORMAT_TLV_QOS)(&pMUXMsg->QMUXMsgHdr + 1);
    pWdsAdminQosTlv->TLVType = 0x10;
    pWdsAdminQosTlv->TLVLength = cpu_to_le16(0x0001);
    pWdsAdminQosTlv->QOSSetting = 0; /* no-QOS header */

    linkProto = (PQMIWDS_ADMIN_SET_DATA_FORMAT_TLV)(pWdsAdminQosTlv + 1);
    linkProto->TLVType = 0x11;
    linkProto->TLVLength = cpu_to_le16(4);
    //linkProto->Value = cpu_to_le32(0x01);     /* Set Ethernet mode */
    linkProto->Value = cpu_to_le32(0x02);     /* Set Raw IP mode */

    ulTlp = (PQMIWDS_ADMIN_SET_DATA_FORMAT_TLV)(linkProto + 1);;
    ulTlp->TLVType = 0x12;
    ulTlp->TLVLength = cpu_to_le16(4);
    ulTlp->Value = cpu_to_le32(0x05);   /* UL QMAP enabled */

    dlTlp = (PQMIWDS_ADMIN_SET_DATA_FORMAT_TLV)(ulTlp + 1);;
    dlTlp->TLVType = 0x13;
    dlTlp->TLVLength = cpu_to_le16(4);
    dlTlp->Value = cpu_to_le32(0x05);  /* DL QMAP enabled */

    ep_id = (PQMIWDS_ADMIN_SET_DATA_FORMAT_TLV_EPID)(dlTlp + 1);;
    ep_id->TLVType = 0x17;
    ep_id->TLVLength = cpu_to_le16(8);
    ep_id->EpType = cpu_to_le32(0x000002); /* EP_ID */
    ep_id->IfaceID = cpu_to_le32(0x0000005); /* EP_ID */

    return sizeof(QCQMUX_MSG_HDR) +
      sizeof(*pWdsAdminQosTlv) + sizeof(*linkProto) + sizeof(*ulTlp) + sizeof(*dlTlp) + sizeof(*ep_id);
}
#endif

static USHORT WdsCreateProfileReq(PQMUX_MSG pMUXMsg, void *arg) {
    pMUXMsg->CreateProfileReq.TLVType = 0x01;
    pMUXMsg->CreateProfileReq.TLVLength = cpu_to_le16(0x01);
    pMUXMsg->CreateProfileReq.ProfileType = 0x00; // 0 ~ 3GPP, 1 ~ 3GPP2

    return sizeof(QMIWDS_CREATE_PROFILE_REQ_MSG);
}

static USHORT WdsGetProfileSettingsReqSend(PQMUX_MSG pMUXMsg, void *arg) {
    PROFILE_T *profile = (PROFILE_T *)arg;
    pMUXMsg->GetProfileSettingsReq.Length = cpu_to_le16(sizeof(QMIWDS_GET_PROFILE_SETTINGS_REQ_MSG) - 4);
    pMUXMsg->GetProfileSettingsReq.TLVType = 0x01;
    pMUXMsg->GetProfileSettingsReq.TLVLength = cpu_to_le16(0x02);
    pMUXMsg->GetProfileSettingsReq.ProfileType = 0x00; // 0 ~ 3GPP, 1 ~ 3GPP2
    pMUXMsg->GetProfileSettingsReq.ProfileIndex = profile->modemProfileIndex;
    return sizeof(QMIWDS_GET_PROFILE_SETTINGS_REQ_MSG);
}

static USHORT WdsModifyProfileSettingsReq(PQMUX_MSG pMUXMsg, void *arg) {
    USHORT TLVLength = 0;
    UCHAR *pTLV;
    PROFILE_T *profile = (PROFILE_T *)arg;
    PQMIWDS_PDPTYPE pPdpType;

    pMUXMsg->ModifyProfileSettingsReq.Length = cpu_to_le16(sizeof(QMIWDS_MODIFY_PROFILE_SETTINGS_REQ_MSG) - 4);
    pMUXMsg->ModifyProfileSettingsReq.TLVType = 0x01;
    pMUXMsg->ModifyProfileSettingsReq.TLVLength = cpu_to_le16(0x02);
    pMUXMsg->ModifyProfileSettingsReq.ProfileType = 0x00; // 0 ~ 3GPP, 1 ~ 3GPP2
    pMUXMsg->ModifyProfileSettingsReq.ProfileIndex = profile->modemProfileIndex;

    pTLV = (UCHAR *)(&pMUXMsg->ModifyProfileSettingsReq + 1);

    pPdpType = (PQMIWDS_PDPTYPE)(pTLV + TLVLength);
    pPdpType->TLVType = 0x11;
    pPdpType->TLVLength = cpu_to_le16(0x01);
// 0 ?C PDP-IP (IPv4)
// 1 ?C PDP-PPP
// 2 ?C PDP-IPv6
// 3 ?C PDP-IPv4v6
    pPdpType->PdpType = 3;
    TLVLength +=(le16_to_cpu(pPdpType->TLVLength) + sizeof(QCQMICTL_TLV_HDR));

    // Set APN Name
    if (profile->apn) {
        PQMIWDS_APNNAME pApnName = (PQMIWDS_APNNAME)(pTLV + TLVLength);
        pApnName->TLVType = 0x14;
        pApnName->TLVLength = cpu_to_le16(strlen(profile->apn));
        qstrcpy((char *)&pApnName->ApnName, profile->apn);
        TLVLength +=(le16_to_cpu(pApnName->TLVLength) + sizeof(QCQMICTL_TLV_HDR));
    }

    // Set User Name
    if (profile->user) {
        PQMIWDS_USERNAME pUserName = (PQMIWDS_USERNAME)(pTLV + TLVLength);
        pUserName->TLVType = 0x1B;
        pUserName->TLVLength = cpu_to_le16(strlen(profile->user));
        qstrcpy((char *)&pUserName->UserName, profile->user);
        TLVLength += (le16_to_cpu(pUserName->TLVLength) + sizeof(QCQMICTL_TLV_HDR));
    }

    // Set Password
    if (profile->password) {
        PQMIWDS_PASSWD pPasswd = (PQMIWDS_PASSWD)(pTLV + TLVLength);
        pPasswd->TLVType = 0x1C;
        pPasswd->TLVLength = cpu_to_le16(strlen(profile->password));
        qstrcpy((char *)&pPasswd->Passwd, profile->password);
        TLVLength +=(le16_to_cpu(pPasswd->TLVLength) + sizeof(QCQMICTL_TLV_HDR));
    }

    // Set Auth Protocol
    if (profile->user && profile->password) {
        PQMIWDS_AUTH_PREFERENCE pAuthPref = (PQMIWDS_AUTH_PREFERENCE)(pTLV + TLVLength);
        pAuthPref->TLVType = 0x1D;
        pAuthPref->TLVLength = cpu_to_le16(0x01);
        pAuthPref->AuthPreference = profile->auth; // 0 ~ None, 1 ~ Pap, 2 ~ Chap, 3 ~ MsChapV2
        TLVLength += (le16_to_cpu(pAuthPref->TLVLength) + sizeof(QCQMICTL_TLV_HDR));
    }

    return sizeof(QMIWDS_MODIFY_PROFILE_SETTINGS_REQ_MSG) + TLVLength;
}

static USHORT WdsGetRuntimeSettingReq(PQMUX_MSG pMUXMsg, void *arg) {
   pMUXMsg->GetRuntimeSettingsReq.TLVType = 0x10;
   pMUXMsg->GetRuntimeSettingsReq.TLVLength = cpu_to_le16(0x04);

   pMUXMsg->GetRuntimeSettingsReq.Mask = cpu_to_le32(QMIWDS_GET_RUNTIME_SETTINGS_MASK_DNS_ADDR |
                                          QMIWDS_GET_RUNTIME_SETTINGS_MASK_IP_ADDR |
                                          QMIWDS_GET_RUNTIME_SETTINGS_MASK_MTU |
                                          QMIWDS_GET_RUNTIME_SETTINGS_MASK_GATEWAY_ADDR |
                                          QMIWDS_GET_RUNTIME_SETTINGS_MASK_PCSCF_ADDR |
                                          QMIWDS_GET_RUNTIME_SETTINGS_MASK_PCSCF_NAME |
                                          QMIWDS_GET_RUNTIME_SETTINGS_MASK_DOMAIN_NAME);

    return sizeof(QMIWDS_GET_RUNTIME_SETTINGS_REQ_MSG);
}

static PQCQMIMSG s_pRequest;
static PQCQMIMSG s_pResponse;
static pthread_mutex_t s_commandmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_commandcond = PTHREAD_COND_INITIALIZER;

static int is_response(const PQCQMIMSG pRequest, const PQCQMIMSG pResponse) {
    if ((pRequest->QMIHdr.QMIType == pResponse->QMIHdr.QMIType)
        && (pRequest->QMIHdr.ClientId == pResponse->QMIHdr.ClientId)) {
            USHORT requestTID, responseTID;
        if (pRequest->QMIHdr.QMIType == QMUX_TYPE_CTL) {
            requestTID = pRequest->CTLMsg.QMICTLMsgHdr.TransactionId;
            responseTID = pResponse->CTLMsg.QMICTLMsgHdr.TransactionId;
        } else {
            requestTID = le16_to_cpu(pRequest->MUXMsg.QMUXHdr.TransactionId);
            responseTID = le16_to_cpu(pResponse->MUXMsg.QMUXHdr.TransactionId);
        }
        return (requestTID == responseTID);
    }
    return 0;
}

int QmiThreadSendQMITimeout(PQCQMIMSG pRequest, PQCQMIMSG *ppResponse, unsigned msecs) {
    int ret;

    if (!pRequest)
    {
        return -EINVAL;
    }

    pthread_mutex_lock(&s_commandmutex);

    if (ppResponse)
        *ppResponse = NULL;

    dump_qmi(pRequest, le16_to_cpu(pRequest->QMIHdr.Length) + 1);

    s_pRequest = pRequest;
    s_pResponse = NULL;

    ret = QmiWwanSendQMI(pRequest);

    if (ret == 0) {
        ret = pthread_cond_timeout_np(&s_commandcond, &s_commandmutex, msecs);
        if (!ret) {
            if (s_pResponse && ppResponse) {
                *ppResponse = s_pResponse;
            } else {
                if (s_pResponse) {
                    free(s_pResponse);
                    s_pResponse = NULL;
                }
            }
        } else {
            syslog(LOG_INFO,"%s pthread_cond_timeout_np=%d, errno: %d (%s)", __func__, ret, errno, strerror(errno));
        }
    }

    pthread_mutex_unlock(&s_commandmutex);

    return ret;
}

int QmiThreadSendQMI(PQCQMIMSG pRequest, PQCQMIMSG *ppResponse) {
    return QmiThreadSendQMITimeout(pRequest, ppResponse, 30 * 1000);
}

void QmiThreadRecvQMI(PQCQMIMSG pResponse) {
    pthread_mutex_lock(&s_commandmutex);
    if (pResponse == NULL) {
        if (s_pRequest) {
            free(s_pRequest);
            s_pRequest = NULL;
            s_pResponse = NULL;
            pthread_cond_signal(&s_commandcond);
        }
        pthread_mutex_unlock(&s_commandmutex);
        return;
    }

    dump_qmi(pResponse, le16_to_cpu(pResponse->QMIHdr.Length) + 1);

    if (s_pRequest && is_response(s_pRequest, pResponse)) {
        free(s_pRequest);
        s_pRequest = NULL;
        s_pResponse = malloc(le16_to_cpu(pResponse->QMIHdr.Length) + 1);
        if (s_pResponse != NULL) {
            memcpy(s_pResponse, pResponse, le16_to_cpu(pResponse->QMIHdr.Length) + 1);
        }
        pthread_cond_signal(&s_commandcond);
    } else if ((pResponse->QMIHdr.QMIType == QMUX_TYPE_NAS)
                    && (le16_to_cpu(pResponse->MUXMsg.QMUXMsgHdrResp.Type) == QMINAS_SERVING_SYSTEM_IND)) {
        qmidevice_send_event_to_main(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED);
    } else if ((pResponse->QMIHdr.QMIType == QMUX_TYPE_WDS)
                    && (le16_to_cpu(pResponse->MUXMsg.QMUXMsgHdrResp.Type) == QMIWDS_GET_PKT_SRVC_STATUS_IND)) {
        qmidevice_send_event_to_main(RIL_UNSOL_DATA_CALL_LIST_CHANGED);
    } else if ((pResponse->QMIHdr.QMIType == QMUX_TYPE_NAS)
                    && (le16_to_cpu(pResponse->MUXMsg.QMUXMsgHdrResp.Type) == QMINAS_SYS_INFO_IND)) {
        qmidevice_send_event_to_main(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED);
    } else {
        if (debug_qmi)
            syslog(LOG_INFO,"nobody care this qmi msg!!");
    }
    pthread_mutex_unlock(&s_commandmutex);
}

int requestSetEthMode(void) {
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    int err;
    PQMIWDS_ADMIN_SET_DATA_FORMAT_TLV linkProto;

    pRequest = ComposeQMUXMsg(QMUX_TYPE_WDS_ADMIN, QMIWDS_ADMIN_SET_DATA_FORMAT_REQ, WdaSetDataFormat, NULL);
    err = QmiThreadSendQMI(pRequest, &pResponse);

    if (err < 0 || pResponse == NULL) {
        syslog(LOG_INFO,"%s err = %d", __func__, err);
        return err;
    }

    pMUXMsg = &pResponse->MUXMsg;
    if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError)) {
        syslog(LOG_INFO,"%s QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
            le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));
        return le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
    }

    linkProto = (PQMIWDS_ADMIN_SET_DATA_FORMAT_TLV)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, 0x11);
    if (linkProto != NULL) {
        syslog(LOG_INFO,"%s resp prot = 0x%x", __func__,le32_to_cpu(linkProto->Value));
    }

    free(pResponse);
    return 0;
}


int requestBindMuxDataPort(void) {
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    int err = 0;
    int loop = 0;
    UCHAR QMIType = QMUX_TYPE_WDS;
    UCHAR muxid = 0;

    for(loop = 0; loop < num_of_profile; loop++)
    {
        if(0 == loop)
        {
          QMIType = QMUX_TYPE_WDS; muxid = 1;
        }else if(1 == loop)
        {
          QMIType = QMUX_TYPE_WDS_2; muxid = 2;
        }else if(2 == loop)
        {
          QMIType = QMUX_TYPE_WDS_3; muxid = 3;
        }
        else
        {
        }

        pRequest = ComposeQMUXMsg(QMIType, QMIWDS_BIND_MUX_DATA_PORT_REQ, WdsBindMuxDataPortReq, (void*)&muxid);
        err = QmiThreadSendQMITimeout(pRequest, &pResponse, 120 * 1000);
        if (err < 0 || pResponse == NULL) {
            syslog(LOG_INFO,"%s bind err = %d", __func__, err);
            return err;
        }

        pMUXMsg = &pResponse->MUXMsg;
        if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError)) {
            syslog(LOG_INFO,"%s bind QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
                le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));
            return le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
        }
        if (pResponse) free(pResponse);
    }

    return 0;
}

int requestQueryDataCall(UCHAR  *pConnectionStatus, PROFILE_T *profile, int curIpFamily) {
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    int err;
    PQMIWDS_PKT_SRVC_TLV pPktSrvc;
    UCHAR oldConnectionStatus = *pConnectionStatus;
#if 0
    UCHAR QMIType = (curIpFamily == IpFamilyV4) ? QMUX_TYPE_WDS : QMUX_TYPE_WDS_IPV6;
#else
    UCHAR QMIType;

    if(profile->localProfileIndex == 0 ) QMIType = QMUX_TYPE_WDS;
    if(profile->localProfileIndex == 1 ) QMIType = QMUX_TYPE_WDS_2;
    if(profile->localProfileIndex == 2 ) QMIType = QMUX_TYPE_WDS_3;
#endif

    pRequest = ComposeQMUXMsg(QMIType, QMIWDS_GET_PKT_SRVC_STATUS_REQ, NULL, NULL);
    err = QmiThreadSendQMI(pRequest, &pResponse);

    if (err < 0 || pResponse == NULL) {
        syslog(LOG_INFO,"%s err = %d", __func__, err);
        return err;
    }

    pMUXMsg = &pResponse->MUXMsg;
    if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError)) {
        syslog(LOG_INFO,"%s QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
            le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));
        return le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
    }

    *pConnectionStatus = QWDS_PKT_DATA_DISCONNECTED;
    pPktSrvc = (PQMIWDS_PKT_SRVC_TLV)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, 0x01);
    if (pPktSrvc) {
        *pConnectionStatus = pPktSrvc->ConnectionStatus;
        if ((le16_to_cpu(pPktSrvc->TLVLength) == 2) && (pPktSrvc->ReconfigReqd == 0x01))
            *pConnectionStatus = QWDS_PKT_DATA_DISCONNECTED;
    }

    if (*pConnectionStatus == QWDS_PKT_DATA_DISCONNECTED) {
        if (curIpFamily == IpFamilyV4)
            WdsConnectionIPv4Handle[profile->localProfileIndex] = 0;
        else
            WdsConnectionIPv6Handle[profile->localProfileIndex] = 0;

    }

    if (oldConnectionStatus != *pConnectionStatus || debug_qmi) {
        syslog(LOG_INFO,"mux%s Profile %d %sConnectionStatus: %s", __func__, profile->localProfileIndex, (curIpFamily == IpFamilyV4) ? "IPv4" : "IPv6",
            (*pConnectionStatus == QWDS_PKT_DATA_CONNECTED) ? "CONNECTED" : "DISCONNECTED");
    }

    free(pResponse);
    return 0;
}

int requestSetupDataCall(PROFILE_T *profile, int curIpFamily) {
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    int err = 0;
    int old_auth = profile->auth;

#if 0
    UCHAR QMIType = (curIpFamily == IpFamilyV4) ? QMUX_TYPE_WDS : QMUX_TYPE_WDS_IPV6;
#else
    UCHAR QMIType;
    if(profile->localProfileIndex == 0 ) QMIType = QMUX_TYPE_WDS;
    if(profile->localProfileIndex == 1 ) QMIType = QMUX_TYPE_WDS_2;
    if(profile->localProfileIndex == 2 ) QMIType = QMUX_TYPE_WDS_3;
#endif

    //DualIPSupported means can get ipv4 & ipv6 address at the same time, one wds for ipv4, the other wds for ipv6
__requestSetupDataCall:
    profile->curIpFamily = curIpFamily;
    pRequest = ComposeQMUXMsg(QMIType, QMIWDS_START_NETWORK_INTERFACE_REQ, WdsStartNwInterfaceReq, profile);
    err = QmiThreadSendQMITimeout(pRequest, &pResponse, 120 * 1000);

    if (err < 0 || pResponse == NULL) {
        syslog(LOG_INFO,"%s err = %d", __func__, err);
        return err;
    }

    pMUXMsg = &pResponse->MUXMsg;
    if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError))
    {
        syslog(LOG_INFO,"%s QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
            le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));

        if (curIpFamily == IpFamilyV4 && old_auth == profile->auth
            && profile->user && profile->user[0] && profile->password && profile->password[0]) {
            profile->auth = (profile->auth == 1) ? 2 : 1;
            free(pResponse);
            goto __requestSetupDataCall;
        }

        profile->auth = old_auth;

        err = le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
        free(pResponse);
        return err;
    }

    if (curIpFamily == IpFamilyV4) {
        WdsConnectionIPv4Handle[profile->localProfileIndex] = le32_to_cpu(pResponse->MUXMsg.StartNwInterfaceResp.Handle);
        syslog(LOG_INFO,"%s WdsConnectionIPv4Handle: 0x%08x", __func__, WdsConnectionIPv4Handle[profile->localProfileIndex]);
    } else {
        WdsConnectionIPv6Handle[profile->localProfileIndex] = le32_to_cpu(pResponse->MUXMsg.StartNwInterfaceResp.Handle);
        syslog(LOG_INFO,"%s WdsConnectionIPv6Handle: 0x%08x", __func__, WdsConnectionIPv6Handle[profile->localProfileIndex]);
    }

    free(pResponse);

    return 0;
}

int requestDeactivateDefaultPDP(PROFILE_T *profile, int curIpFamily) {
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    int err;
#if 0
    UCHAR QMIType = (curIpFamily == 0x04) ? QMUX_TYPE_WDS : QMUX_TYPE_WDS_IPV6;
#else
    UCHAR QMIType;
    if(profile->localProfileIndex == 0 ) QMIType = QMUX_TYPE_WDS;
    if(profile->localProfileIndex == 1 ) QMIType = QMUX_TYPE_WDS_2;
    if(profile->localProfileIndex == 2 ) QMIType = QMUX_TYPE_WDS_3;
#endif

    if (curIpFamily == IpFamilyV4 && WdsConnectionIPv4Handle[profile->localProfileIndex] == 0)
        return 0;
    if (curIpFamily == IpFamilyV6 && WdsConnectionIPv6Handle[profile->localProfileIndex] == 0)
        return 0;

    pRequest = ComposeQMUXMsg(QMIType, QMIWDS_STOP_NETWORK_INTERFACE_REQ , WdsStopNwInterfaceReq, profile);
    err = QmiThreadSendQMI(pRequest, &pResponse);

    if (err < 0 || pResponse == NULL) {
        syslog(LOG_INFO,"%s err = %d", __func__, err);
        return err;
    }

    pMUXMsg = &pResponse->MUXMsg;
    if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError)) {
        syslog(LOG_INFO,"%s QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
            le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));
        return le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
    }

    if (curIpFamily == IpFamilyV4)
        WdsConnectionIPv4Handle[profile->localProfileIndex] = 0;
    else
        WdsConnectionIPv6Handle[profile->localProfileIndex] = 0;
    free(pResponse);
    return 0;
}

int requestGetIPAddress(PROFILE_T *profile, int curIpFamily) {
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    int err;
    PQMIWDS_GET_RUNTIME_SETTINGS_TLV_IPV4_ADDR pIpv4Addr;
    PQMIWDS_GET_RUNTIME_SETTINGS_TLV_IPV6_ADDR pIpv6Addr = NULL;
    PQMIWDS_GET_RUNTIME_SETTINGS_TLV_MTU pMtu;
    PQMIWDS_GET_RUNTIME_SETTINGS_TLV_PCSCF_IPV6_ADDR pPCSCFIpv6Addr = NULL;
    IPV4_T *pIpv4 = &profile->ipv4;
    IPV6_T *pIpv6 = &profile->ipv6;

#if 0
    UCHAR QMIType = (curIpFamily == 0x04) ? QMUX_TYPE_WDS : QMUX_TYPE_WDS_IPV6;
#else
    UCHAR QMIType;
    if(0 == profile->localProfileIndex )  QMIType = QMUX_TYPE_WDS;
    if(1 == profile->localProfileIndex )  QMIType = QMUX_TYPE_WDS_2;
    if(2 == profile->localProfileIndex )  QMIType = QMUX_TYPE_WDS_3;
#endif

    if (curIpFamily == IpFamilyV4) {
        memset(pIpv4, 0x00, sizeof(IPV4_T));
        if (WdsConnectionIPv4Handle[profile->localProfileIndex] == 0)
            return 0;
    } else if (curIpFamily == IpFamilyV6) {
        memset(pIpv6, 0x00, sizeof(IPV6_T));
        if (WdsConnectionIPv6Handle[profile->localProfileIndex] == 0)
            return 0;
    }


    pRequest = ComposeQMUXMsg(QMIType, QMIWDS_GET_RUNTIME_SETTINGS_REQ, WdsGetRuntimeSettingReq, NULL);
    err = QmiThreadSendQMI(pRequest, &pResponse);

    if (err < 0 || pResponse == NULL) {
        syslog(LOG_INFO,"%s err = %d", __func__, err);
        return err;
    }

    pMUXMsg = &pResponse->MUXMsg;
    if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError)) {
        syslog(LOG_INFO,"%s QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
            le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));
        return le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
    }

    pIpv4Addr = (PQMIWDS_GET_RUNTIME_SETTINGS_TLV_IPV4_ADDR)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, QMIWDS_GET_RUNTIME_SETTINGS_TLV_TYPE_IPV4PRIMARYDNS);
    if (pIpv4Addr) {
        pIpv4->DnsPrimary = pIpv4Addr->IPV4Address;
    }

    pIpv4Addr = (PQMIWDS_GET_RUNTIME_SETTINGS_TLV_IPV4_ADDR)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, QMIWDS_GET_RUNTIME_SETTINGS_TLV_TYPE_IPV4SECONDARYDNS);
    if (pIpv4Addr) {
        pIpv4->DnsSecondary = pIpv4Addr->IPV4Address;
    }

    pIpv4Addr = (PQMIWDS_GET_RUNTIME_SETTINGS_TLV_IPV4_ADDR)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, QMIWDS_GET_RUNTIME_SETTINGS_TLV_TYPE_IPV4GATEWAY);
    if (pIpv4Addr) {
        pIpv4->Gateway = pIpv4Addr->IPV4Address;
    }

    pIpv4Addr = (PQMIWDS_GET_RUNTIME_SETTINGS_TLV_IPV4_ADDR)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, QMIWDS_GET_RUNTIME_SETTINGS_TLV_TYPE_IPV4SUBNET);
    if (pIpv4Addr) {
        pIpv4->SubnetMask = pIpv4Addr->IPV4Address;
    }

    pIpv4Addr = (PQMIWDS_GET_RUNTIME_SETTINGS_TLV_IPV4_ADDR)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, QMIWDS_GET_RUNTIME_SETTINGS_TLV_TYPE_IPV4);
    if (pIpv4Addr) {
        pIpv4->Address = pIpv4Addr->IPV4Address;
    }

    pIpv6Addr = (PQMIWDS_GET_RUNTIME_SETTINGS_TLV_IPV6_ADDR)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, QMIWDS_GET_RUNTIME_SETTINGS_TLV_TYPE_IPV6PRIMARYDNS);
    if (pIpv6Addr) {
        memcpy(pIpv6->DnsPrimary, pIpv6Addr->IPV6Address, 16);
    }

    pIpv6Addr = (PQMIWDS_GET_RUNTIME_SETTINGS_TLV_IPV6_ADDR)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, QMIWDS_GET_RUNTIME_SETTINGS_TLV_TYPE_IPV6SECONDARYDNS);
    if (pIpv6Addr) {
        memcpy(pIpv6->DnsSecondary, pIpv6Addr->IPV6Address, 16);
    }

    pIpv6Addr = (PQMIWDS_GET_RUNTIME_SETTINGS_TLV_IPV6_ADDR)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, QMIWDS_GET_RUNTIME_SETTINGS_TLV_TYPE_IPV6GATEWAY);
    if (pIpv6Addr) {
        memcpy(pIpv6->Gateway, pIpv6Addr->IPV6Address, 16);
        pIpv6->PrefixLengthGateway = pIpv6Addr->PrefixLength;
    }

    pIpv6Addr = (PQMIWDS_GET_RUNTIME_SETTINGS_TLV_IPV6_ADDR)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, QMIWDS_GET_RUNTIME_SETTINGS_TLV_TYPE_IPV6);
    if (pIpv6Addr) {
        memcpy(pIpv6->Address, pIpv6Addr->IPV6Address, 16);
        pIpv6->PrefixLengthIPAddr = pIpv6Addr->PrefixLength;
    }

    pMtu = (PQMIWDS_GET_RUNTIME_SETTINGS_TLV_MTU)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, QMIWDS_GET_RUNTIME_SETTINGS_TLV_TYPE_MTU);
    if (pMtu) {
        pIpv4->Mtu =  pIpv6->Mtu =  le32_to_cpu(pMtu->Mtu);
    }

    pPCSCFIpv6Addr = (PQMIWDS_GET_RUNTIME_SETTINGS_TLV_PCSCF_IPV6_ADDR)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, QMIWDS_GET_RUNTIME_SETTINGS_TLV_TYPE_PCSCF_IPV6_ADDR);
    if(pPCSCFIpv6Addr) {
        printf( "pcscf addr list len is %d \n" ,pPCSCFIpv6Addr->address_list_len);
        printf("pcscf addr: %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
					pPCSCFIpv6Addr->addr[0][0], pPCSCFIpv6Addr->addr[0][1], pPCSCFIpv6Addr->addr[0][2], pPCSCFIpv6Addr->addr[0][3],
					pPCSCFIpv6Addr->addr[0][4], pPCSCFIpv6Addr->addr[0][5], pPCSCFIpv6Addr->addr[0][6], pPCSCFIpv6Addr->addr[0][7],
					pPCSCFIpv6Addr->addr[0][8], pPCSCFIpv6Addr->addr[0][9], pPCSCFIpv6Addr->addr[0][10], pPCSCFIpv6Addr->addr[0][11],
				  pPCSCFIpv6Addr->addr[0][12], pPCSCFIpv6Addr->addr[0][13], pPCSCFIpv6Addr->addr[0][14], pPCSCFIpv6Addr->addr[0][15]);
    }

    free(pResponse);
    return 0;
}


int requestCreateProfile(PROFILE_T *profile) {
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    int err;
    PQMIWDS_PROFILE_IDENTIFIER pProfileIdentifier;

    if (!profile->localProfileIndex)
        return 0;

    pRequest = ComposeQMUXMsg(QMUX_TYPE_WDS, QMIWDS_CREATE_PROFILE_REQ, WdsCreateProfileReq, profile);
    err = QmiThreadSendQMI(pRequest, &pResponse);

    if (err < 0 || pResponse == NULL) {
        syslog(LOG_INFO,"%s err = %d", __func__, err);
        return err;
    }

    pMUXMsg = &pResponse->MUXMsg;
    if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError)) {
        syslog(LOG_INFO,"%s QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
            le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));
        return le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
    }

    pProfileIdentifier = (PQMIWDS_PROFILE_IDENTIFIER)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, 0x01);
    if (pProfileIdentifier) {
        syslog(LOG_INFO,"#####Created Profile%d", pProfileIdentifier->ProfileIndex);
    }

    free(pResponse);
    return 0;
}

int requestSetProfile(PROFILE_T *profile) {
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    int err;

    if (!profile->localProfileIndex)
        return 0;

    syslog(LOG_INFO,"%s[%d] %s/%s/%s/%d", __func__, profile->localProfileIndex, profile->apn, profile->user, profile->password, profile->auth);
    pRequest = ComposeQMUXMsg(QMUX_TYPE_WDS, QMIWDS_MODIFY_PROFILE_SETTINGS_REQ, WdsModifyProfileSettingsReq, profile);
    err = QmiThreadSendQMI(pRequest, &pResponse);

    if (err < 0 || pResponse == NULL) {
        syslog(LOG_INFO,"%s err = %d", __func__, err);
        return err;
    }

    pMUXMsg = &pResponse->MUXMsg;
    if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError)) {
        syslog(LOG_INFO,"%s QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
            le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));
        return le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
    }

    free(pResponse);
    return 0;
}

int requestGetProfile(PROFILE_T *profile) {
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    int err;
#if 0
    char *apn = NULL;
    char *user = NULL;
    char *password = NULL;
    int auth = 0;
    PQMIWDS_APNNAME pApnName;
    PQMIWDS_USERNAME pUserName;
    PQMIWDS_PASSWD pPassWd;
    PQMIWDS_AUTH_PREFERENCE pAuthPref;
#endif
    if (!profile->localProfileIndex)
        return 0;

    pRequest = ComposeQMUXMsg(QMUX_TYPE_WDS, QMIWDS_GET_PROFILE_SETTINGS_REQ, WdsGetProfileSettingsReqSend, profile);
    err = QmiThreadSendQMI(pRequest, &pResponse);

    if (err < 0 || pResponse == NULL) {
        syslog(LOG_INFO,"%s err = %d", __func__, err);
        return err;
    }

    pMUXMsg = &pResponse->MUXMsg;
    if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError)) {
        syslog(LOG_INFO,"%s QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
            le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));
        return le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
    }

#if 0
    pApnName = (PQMIWDS_APNNAME)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, 0x14);
    pUserName = (PQMIWDS_USERNAME)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, 0x1B);
    pPassWd = (PQMIWDS_PASSWD)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, 0x1C);
    pAuthPref = (PQMIWDS_AUTH_PREFERENCE)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, 0x1D);

    if (pApnName/* && le16_to_cpu(pApnName->TLVLength)*/)
        apn = strndup((const char *)(&pApnName->ApnName), le16_to_cpu(pApnName->TLVLength));
    if (pUserName/*  && pUserName->UserName*/)
        user = strndup((const char *)(&pUserName->UserName), le16_to_cpu(pUserName->TLVLength));
    if (pPassWd/*  && le16_to_cpu(pPassWd->TLVLength)*/)
        password = strndup((const char *)(&pPassWd->Passwd), le16_to_cpu(pPassWd->TLVLength));
    if (pAuthPref/*  && le16_to_cpu(pAuthPref->TLVLength)*/) {
        auth = pAuthPref->AuthPreference;
    }
#endif

    free(pResponse);
    return 0;
}

int requestBaseBandVersion(const char **pp_reversion) {
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    PDEVICE_REV_ID revId;
    int err;

    if (pp_reversion) *pp_reversion = NULL;

    pRequest = ComposeQMUXMsg(QMUX_TYPE_DMS, QMIDMS_GET_DEVICE_REV_ID_REQ, NULL, NULL);
    err = QmiThreadSendQMI(pRequest, &pResponse);

    if (err < 0 || pResponse == NULL) {
        syslog(LOG_INFO,"%s err = %d", __func__, err);
        return err;
    }

    pMUXMsg = &pResponse->MUXMsg;
    if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError)) {
        syslog(LOG_INFO,"%s QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
            le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));
        return le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
    }

    revId = (PDEVICE_REV_ID)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, 0x01);

    if (revId && le16_to_cpu(revId->TLVLength))
    {
        char *DeviceRevisionID = strndup((const char *)(&revId->RevisionID), le16_to_cpu(revId->TLVLength));
        syslog(LOG_INFO,"%s %s", __func__, DeviceRevisionID);
        if (pp_reversion) *pp_reversion = DeviceRevisionID;
    }

    free(pResponse);
    return 0;
}

int requestSetLteBandPriority(UCHAR *blist) {
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    int err;

    pRequest = ComposeQMUXMsg(QMUX_TYPE_NAS, QMINAS_SET_LTE_BAND_PRIORITY_REQ, NasSetLteBandPriorityReq, blist);
    err = QmiThreadSendQMI(pRequest, &pResponse);

    if (err < 0 || pResponse == NULL) {
        syslog(LOG_INFO,"%s err = %d", __func__, err);
        return err;
    }

    pMUXMsg = &pResponse->MUXMsg;
    if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError)) {
        syslog(LOG_INFO,"%s QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
            le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));
        return le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
    }

    free(pResponse);

    return 0;
}

int requestGetPSAttachState(UCHAR *pPSAttachedState) {
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    int err;
    USHORT MobileCountryCode = 0;
    USHORT MobileNetworkCode = 0;
    LONG remainingLen;
    PSERVICE_STATUS_INFO pServiceStatusInfo;
    PLTE_SYSTEM_INFO pLteSystemInfo;
    PNR5G_SYSTEM_INFO pNR5GSystemInfo;

    *pPSAttachedState = 0;
    pRequest = ComposeQMUXMsg(QMUX_TYPE_NAS, QMINAS_GET_SYS_INFO_REQ, NULL, NULL);
    err = QmiThreadSendQMI(pRequest, &pResponse);

    if (err < 0 || pResponse == NULL) {
        syslog(LOG_INFO,"%s err = %d", __func__, err);
        return err;
    }

    pMUXMsg = &pResponse->MUXMsg;
    if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError)) {
        syslog(LOG_INFO,"%s QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
            le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));
        return le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
    }

    pServiceStatusInfo = (PSERVICE_STATUS_INFO)(((PCHAR)&pMUXMsg->GetSysInfoResp) + QCQMUX_MSG_HDR_SIZE);
    remainingLen = le16_to_cpu(pMUXMsg->GetSysInfoResp.Length);

    while (remainingLen > 0) {
        switch (pServiceStatusInfo->TLVType) {
        case 0x10: // CDMA
            break;
        case 0x11: // HDR
            break;
        case 0x12: // GSM
            break;
        case 0x13: // WCDMA
            break;
        case 0x14: // LTE
            if (pServiceStatusInfo->SrvStatus == 0x02)
            {
                //is_lte = 1;
            }
            break;
        case 0x24: // TDSCDMA
            break;
        case 0x15: // CDMA
            break;
        case 0x16: // HDR
            break;
        case 0x17: // GSM
            break;
        case 0x18: // WCDMA
            break;
        case 0x19: // LTE_SYSTEM_INFO
            pLteSystemInfo = (PLTE_SYSTEM_INFO)pServiceStatusInfo;
            if (pLteSystemInfo->SrvDomainValid == 0x01) {
                *pPSAttachedState = 0;
                if (pLteSystemInfo->SrvDomain & 0x02) {
                    *pPSAttachedState = 1;
                    //is_lte = 1;
                }
            }

            if (pLteSystemInfo->NetworkIdValid == 0x01) {
                int i;
                CHAR temp[10];
                strncpy(temp, (CHAR *)pLteSystemInfo->MCC, 3);
                temp[3] = '\0';
                for (i = 0; i < 4; i++) {
                    if ((UCHAR)temp[i] == 0xFF) {
                        temp[i] = '\0';
                    }
                }
                MobileCountryCode = (USHORT)atoi(temp);

                strncpy(temp, (CHAR *)pLteSystemInfo->MNC, 3);
                temp[3] = '\0';
                for (i = 0; i < 4; i++) {
                    if ((UCHAR)temp[i] == 0xFF) {
                        temp[i] = '\0';
                    }
                }
                MobileNetworkCode = (USHORT)atoi(temp);
            }
            break;
        case 0x25: // TDSCDMA
            break;
        case 0x4A: // NR5G Service Status Info
            if (pServiceStatusInfo->SrvStatus == 0x02) {

            }
            break;
        case 0x4B: //NR5G System Info
            pNR5GSystemInfo = (PNR5G_SYSTEM_INFO)pServiceStatusInfo;
            if (pNR5GSystemInfo->SrvDomainValid == 0x01) {
                *pPSAttachedState = 0;
                if (pNR5GSystemInfo->SrvDomain & 0x02) {
                    *pPSAttachedState = 1;
                }
            }
            break;
        default:
            break;
        } /* switch (pServiceStatusInfo->TLYType) */
        remainingLen -= (le16_to_cpu(pServiceStatusInfo->TLVLength) + 3);
        pServiceStatusInfo = (PSERVICE_STATUS_INFO)((PCHAR)&pServiceStatusInfo->TLVLength + le16_to_cpu(pServiceStatusInfo->TLVLength) + sizeof(USHORT));
    } /* while (remainingLen > 0) */

    syslog(LOG_INFO,"%s MCC: %d, MNC: %d, Attached: %s", __func__,
        MobileCountryCode, MobileNetworkCode, (*pPSAttachedState == 1) ? "Attached" : "Detached");

    free(pResponse);

    return 0;
}

int requestGetLteAttachParams(char *pApn) {
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    int err;
    PQMIWDS_LTE_ATTACH_PARAMS_APN_STRING pApnString = NULL;

    pRequest = ComposeQMUXMsg(QMUX_TYPE_WDS, QMIWDS_GET_LTE_ATTACH_PARAMS_REQ, NULL, NULL);

    err = QmiThreadSendQMI(pRequest, &pResponse);

    if (err < 0 || pResponse == NULL) {
        syslog(LOG_INFO,"%s err = %d", __func__, err);
        return err;
    }

    pMUXMsg = &pResponse->MUXMsg;
    if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError)) {
        syslog(LOG_INFO,"%s QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
            le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));
        return le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
    }

    pApnString = (PQMIWDS_LTE_ATTACH_PARAMS_APN_STRING)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, 0x10);

    if (pApnString != NULL) {

       syslog(LOG_INFO,"get attached apn %d charecters", pApnString->TLVLength);
       memcpy(pApn, pApnString->apn_string, pApnString->TLVLength);
    }

    free(pResponse);
    return 0;
}

int requestGetPINStatus(SIM_Status *pSIMStatus) {
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    int err;int s_9x07=1;
    PQMIDMS_UIM_PIN_STATUS pPin1Status = NULL;
    //PQMIDMS_UIM_PIN_STATUS pPin2Status = NULL;

    if (s_9x07 && qmiclientId[QMUX_TYPE_UIM])
        pRequest = ComposeQMUXMsg(QMUX_TYPE_UIM, QMIUIM_GET_CARD_STATUS_REQ, NULL, NULL);
    else
        pRequest = ComposeQMUXMsg(QMUX_TYPE_DMS, QMIDMS_UIM_GET_PIN_STATUS_REQ, NULL, NULL);
    err = QmiThreadSendQMI(pRequest, &pResponse);

    if (err < 0 || pResponse == NULL) {
        syslog(LOG_INFO,"%s err = %d", __func__, err);
        return err;
    }

    pMUXMsg = &pResponse->MUXMsg;
    if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError)) {
        syslog(LOG_INFO,"%s QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
            le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));
        return le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
    }

    pPin1Status = (PQMIDMS_UIM_PIN_STATUS)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, 0x11);
    //pPin2Status = (PQMIDMS_UIM_PIN_STATUS)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, 0x12);

    if (pPin1Status != NULL) {
        if (pPin1Status->PINStatus == QMI_PIN_STATUS_NOT_VERIF) {
            *pSIMStatus = SIM_PIN;
        } else if (pPin1Status->PINStatus == QMI_PIN_STATUS_BLOCKED) {
            *pSIMStatus = SIM_PUK;
        } else if (pPin1Status->PINStatus == QMI_PIN_STATUS_PERM_BLOCKED) {
            *pSIMStatus = SIM_BAD;
        }
    }

    free(pResponse);
    return 0;
}

int requestGetSIMStatus(SIM_Status *pSIMStatus) { //RIL_REQUEST_GET_SIM_STATUS
    PQCQMIMSG pRequest;
    PQCQMIMSG pResponse;
    PQMUX_MSG pMUXMsg;
    int err;
    int s_9x07 = 1;
    const char * SIM_Status_String[] = {
        "SIM_ABSENT",
        "SIM_NOT_READY",
        "SIM_READY", /* SIM_READY means the radio state is RADIO_STATE_SIM_READY */
        "SIM_PIN",
        "SIM_PUK",
        "SIM_NETWORK_PERSONALIZATION"
    };

__requestGetSIMStatus:
    if (s_9x07 && qmiclientId[QMUX_TYPE_UIM])
        pRequest = ComposeQMUXMsg(QMUX_TYPE_UIM, QMIUIM_GET_CARD_STATUS_REQ, UimGetCardStausReq/*NULL*/, NULL);
    else
        pRequest = ComposeQMUXMsg(QMUX_TYPE_DMS, QMIDMS_UIM_GET_STATE_REQ, NULL, NULL);

    err = QmiThreadSendQMI(pRequest, &pResponse);

    if (err < 0 || pResponse == NULL) {
        syslog(LOG_INFO,"%s err = %d", __func__, err);
        return err;
    }

    pMUXMsg = &pResponse->MUXMsg;
    if (le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult) || le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError)) {
        syslog(LOG_INFO,"%s QMUXResult = 0x%x, QMUXError = 0x%x", __func__,
            le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXResult), le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError));
        if (QMI_ERR_OP_DEVICE_UNSUPPORTED == le16_to_cpu(pResponse->MUXMsg.QMUXMsgHdrResp.QMUXError)) {
            sleep(1);
            goto __requestGetSIMStatus;
        }
        return le16_to_cpu(pMUXMsg->QMUXMsgHdrResp.QMUXError);
    }

    *pSIMStatus = SIM_ABSENT;
    if (s_9x07 && qmiclientId[QMUX_TYPE_UIM])
    {
        PQMIUIM_CARD_STATUS pCardStatus = NULL;
        PQMIUIM_PIN_STATE pPINState = NULL;
        UCHAR CardState = 0x01;
        UCHAR PIN1State = QMI_PIN_STATUS_NOT_VERIF;
        //UCHAR PIN1Retries;
        //UCHAR PUK1Retries;
        //UCHAR PIN2State;
        //UCHAR PIN2Retries;
        //UCHAR PUK2Retries;

        pCardStatus = (PQMIUIM_CARD_STATUS)GetTLV(&pResponse->MUXMsg.QMUXMsgHdr, 0x10);


        if (pCardStatus != NULL)
        {
            /*---------end--------- */
            pPINState = (PQMIUIM_PIN_STATE)((PUCHAR)pCardStatus + sizeof(QMIUIM_CARD_STATUS) + pCardStatus->AIDLength);
            CardState  = pCardStatus->CardState;
            if (pPINState->UnivPIN == 1)
            {
               PIN1State = pCardStatus->UPINState;
               //PIN1Retries = pCardStatus->UPINRetries;
               //PUK1Retries = pCardStatus->UPUKRetries;
            }
            else
            {
               PIN1State = pPINState->PIN1State;
               //PIN1Retries = pPINState->PIN1Retries;
               //PUK1Retries = pPINState->PUK1Retries;
            }
            //PIN2State = pPINState->PIN2State;
            //PIN2Retries = pPINState->PIN2Retries;
            //PUK2Retries = pPINState->PUK2Retries;
        }

        *pSIMStatus = SIM_ABSENT;
        if ((CardState == 0x01) &&  ((PIN1State == QMI_PIN_STATUS_VERIFIED)|| (PIN1State == QMI_PIN_STATUS_DISABLED)))
        {
            *pSIMStatus = SIM_READY;
        }
        else if (CardState == 0x01)
        {
            if (PIN1State == QMI_PIN_STATUS_NOT_VERIF)
            {
                *pSIMStatus = SIM_PIN;
            }
            if ( PIN1State == QMI_PIN_STATUS_BLOCKED)
            {
                *pSIMStatus = SIM_PUK;
            }
            else if (PIN1State == QMI_PIN_STATUS_PERM_BLOCKED)
            {
                *pSIMStatus = SIM_BAD;
            }
            else if (PIN1State == QMI_PIN_STATUS_NOT_INIT || PIN1State == QMI_PIN_STATUS_VERIFIED || PIN1State == QMI_PIN_STATUS_DISABLED)
            {
                *pSIMStatus = SIM_READY;
            }
        }
        else if (CardState == 0x00 || CardState == 0x02)
        {
        }
        else
        {
        }
    }
    else
    {
    //UIM state. Values:
    // 0x00  UIM initialization completed
    // 0x01  UIM is locked or the UIM failed
    // 0x02  UIM is not present
    // 0x03  Reserved
    // 0xFF  UIM state is currently
    //unavailable
        if (pResponse->MUXMsg.UIMGetStateResp.UIMState == 0x00) {
            *pSIMStatus = SIM_READY;
        } else if (pResponse->MUXMsg.UIMGetStateResp.UIMState == 0x01) {
            *pSIMStatus = SIM_ABSENT;
            err = requestGetPINStatus(pSIMStatus);
        } else if ((pResponse->MUXMsg.UIMGetStateResp.UIMState == 0x02) || (pResponse->MUXMsg.UIMGetStateResp.UIMState == 0xFF)) {
            *pSIMStatus = SIM_ABSENT;
        } else {
            *pSIMStatus = SIM_ABSENT;
        }
    }

    syslog(LOG_INFO,"%s SIMStatus: %s", __func__, SIM_Status_String[*pSIMStatus]);

    free(pResponse);

    return 0;
}
