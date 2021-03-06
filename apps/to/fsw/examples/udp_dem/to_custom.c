/******************************************************************************/
/** \file  to_custom.c
*  
*   \author Guy de Carufel (Odyssey Space Research), NASA, JSC, ER6
*
*   \brief Function Definitions for Custom Layer of TO Application for DEM (UDP)
*
*   \par
*     This file defines the functions for a custom implementation of the custom
*     layer of the TO application sending DEM Messsages over UDP.
*
*   \par API Functions Defined:
*     - TO_CustomInit() - Initialize the transport protocol
*     - TO_CustomAppCmds() - Process custom App Commands
*     - TO_CustomEnableOutputCmd() - Enable telemetry output
*     - TO_CustomDisableOutputCmd() - Disable telemetry output
*     - TO_CustomCleanup() - Cleanup callback to close transport channel.
*     - TO_CustomProcessData() - Send output data over transport protocol.
*
*   \par Private Functions Defined:
*
*   \par Limitations, Assumptions, External Events, and Notes:
*     - Outputs both DEM and CCSDS messages over 3 seperate routes
*     - All config macros defined in to_platform_cfg.h
*     - Routes are enabled one at a time in this example
*
*   \par Modification History:
*     - 2015-07-07 | Guy de Carufel | Code Started
*     - 2015-09-22 | Guy de Carufel | Revised Routing scheme
*******************************************************************************/

/*
** Pragmas
*/

/*
** Include Files
*/
#include "cfe.h"
#include "network_includes.h"
#include "trans_udp.h"
#include "dem_ccsds.h"

#include "to_app.h"

#include "cfe_msgids.h"

#include "cf_msgids.h"
#include "sch_msgids.h"
#include "hs_msgids.h"
#include "hk_msgids.h"
#include "sch_msgids.h"
#include "ci_msgids.h"

/*
** Local Defines
*/

/*
** Local Structure Declarations
*/
typedef struct
{
    IO_TransUdp_t           udp[3];       /**< UDP working              */
    uint8                   demMsgBuffer[TO_CUSTOM_DEM_BUFFER_SIZE]; 
    DEM_CCSDS_OutConfig_t   outConfigTbl[TO_CUSTOM_CCSDS_DEM_CONFIG_SIZE];
} TO_CustomData_t;

/*
** External Global Variables
*/
extern TO_AppData_t g_TO_AppData; 

/*
** Global Variables
*/

/*
** Local Variables
*/
static TO_CustomData_t g_TO_CustomData;

/*
** Local Function Definitions
*/
extern void TO_SendDataTypePktCmd(CFE_SB_MsgPtr_t);
static int32 TO_CustomProcessSizeSent(int32, int32, int32, uint16);

/*******************************************************************************
** Custom Application Functions 
*******************************************************************************/

/******************************************************************************/
/** \brief Custom Initialization
*******************************************************************************/
int32 TO_CustomInit(void)
{
    int32 iStatus = TO_SUCCESS;
    
    /* Create socket for outgoing ccsds */
    if (IO_TransUdpCreateSocket(&g_TO_CustomData.udp[0]) < 0)
    {
        iStatus = TO_ERROR;
        goto end_of_function;
    }
    
    /* Create socket for outgoing dem */
    if (IO_TransUdpCreateSocket(&g_TO_CustomData.udp[1]) < 0)
    {
        iStatus = TO_ERROR;
        goto end_of_function;
    }
    
    /* Create socket for outgoing dem-ksc */
    if (IO_TransUdpCreateSocket(&g_TO_CustomData.udp[2]) < 0)
    {
        iStatus = TO_ERROR;
        goto end_of_function;
    }

    /* Set Critical Message Ids which must always be
     * in config table. */
    g_TO_AppData.criticalMid[0] = CFE_ES_SHELL_TLM_MID;
    g_TO_AppData.criticalMid[1] = CFE_EVS_EVENT_MSG_MID;
    g_TO_AppData.criticalMid[2] = CFE_SB_ALLSUBS_TLM_MID;

    /* Set CCSDS to DEM Translation Config Table */
    DEM_CCSDS_OutConfig_t config[TO_CUSTOM_CCSDS_DEM_CONFIG_SIZE] = { 
        {CF_CONFIG_TLM_MID,        100, 0x03, 0x00, 0x3200, 0, 0},         
        {CF_HK_TLM_MID,            100, 0x03, 0x01, 0x3200, 0, 0},  
        {CF_SPACE_TO_GND_PDU_MID,  100, 0x03, 0x02, 0x3200, 0, 0},  
        {CF_TRANS_TLM_MID,         100, 0x03, 0x03, 0x3200, 0, 0},  
        {CFE_ES_APP_TLM_MID,       100, 0x04, 0x00, 0x3200, 0, 0},  
        {CFE_ES_HK_TLM_MID,        100, 0x04, 0x01, 0x3200, 0, 0},  
        {CFE_ES_MEMSTATS_TLM_MID,  100, 0x04, 0x02, 0x3200, 0, 0},  
        {CFE_ES_SHELL_TLM_MID,     100, 0x04, 0x03, 0x3200, 0, 0},  
        {CFE_EVS_EVENT_MSG_MID,    100, 0x05, 0x00, 0x3200, 0, 0},  
        {CFE_EVS_HK_TLM_MID,       100, 0x05, 0x01, 0x3200, 0, 0},  
        {CFE_SB_ALLSUBS_TLM_MID,   100, 0x06, 0x00, 0x3200, 0, 0},  
        {CFE_SB_HK_TLM_MID,        100, 0x06, 0x01, 0x3200, 0, 0},  
        {CFE_SB_ONESUB_TLM_MID,    100, 0x06, 0x02, 0x3200, 0, 0},  
        {CFE_SB_STATS_TLM_MID,     100, 0x06, 0x03, 0x3200, 0, 0},  
        {CFE_TBL_HK_TLM_MID,       100, 0x07, 0x00, 0x3200, 0, 0},  
        {CFE_TBL_REG_TLM_MID,      100, 0x07, 0x01, 0x3200, 0, 0},  
        {CFE_TIME_DIAG_TLM_MID,    100, 0x08, 0x00, 0x3200, 0, 0},  
        {CFE_TIME_HK_TLM_MID,      100, 0x08, 0x01, 0x3200, 0, 0},  
        {HS_HK_TLM_MID,            100, 0x09, 0x00, 0x3200, 0, 0},  
        {SCH_DIAG_TLM_MID,         100, 0x0a, 0x00, 0x3200, 0, 0},  
        {SCH_HK_TLM_MID,           100, 0x0a, 0x01, 0x3200, 0, 0},  
        {HK_HK_TLM_MID,            100, 0x0b, 0x00, 0x3200, 0, 0},  
        {CI_HK_TLM_MID,            100, 0x0c, 0x00, 0x3200, 0, 0},  
        {TO_HK_TLM_MID,            100, 0x0d, 0x00, 0x3200, 0, 0}, 
        {TO_DATA_TYPE_MID,         100, 0x0d, 0x01, 0x3200, 0, 0},  
        {TA1_HK_TLM_MID,           100, 0x0e, 0x00, 0x3200, 0, 0},  
        {TA2_HK_TLM_MID,           100, 0x0f, 0x00, 0x3200, 0, 0},  
        {TA3_HK_TLM_MID,           100, 0x10, 0x00, 0x3200, 0, 0}
    };

    CFE_PSP_MemCpy((void *) &g_TO_CustomData.outConfigTbl[0],
                   (void *) &config[0], sizeof(config));

    /* Route 0: Udp - ccsds */
    g_TO_AppData.routes[0].usExists = 1;
    /* Route 1: Udp - dem */
    g_TO_AppData.routes[1].usExists = 1;
    /* Route 2: Udp - dem-ksc */
    g_TO_AppData.routes[2].usExists = 1;

    /* Tie route 0 to CF channel 0 */
    g_TO_AppData.routes[0].sCfChnlIdx = 0;

end_of_function:
    return iStatus;
}

/******************************************************************************/
/** \brief Process of custom app commands 
*******************************************************************************/
int32 TO_CustomAppCmds(CFE_SB_Msg_t* pMsg)
{
    int32 iStatus = TO_SUCCESS;
    uint32 uiCmdCode = CFE_SB_GetCmdCode(pMsg);
    switch (uiCmdCode)
    {
        case TO_SEND_DATA_TYPE_CC:
            TO_SendDataTypePktCmd(pMsg);
            break;
        
        default:
            iStatus = -1;
            break;
    }

    return iStatus;
}

/******************************************************************************/
/** \brief Process of output telemetry
*******************************************************************************/
int32 TO_CustomProcessData(CFE_SB_Msg_t * pMsg, int32 size, int32 iTblIdx,
                           uint16 usRouteId)
{
    int32 iStatus = -1;
    int32 iSentSize = -1;
    int32 demSize = 0;

    /* Send over UDP 0 as CCSDS */
    if (usRouteId == 0)
    {
        iSentSize = IO_TransUdpSnd((IO_TransUdp_t *) &g_TO_CustomData.udp[0], 
                                   (uint8 *) pMsg, size);
        iStatus = TO_CustomProcessSizeSent(size, iSentSize, iTblIdx, 0); 
    }


    /* Send over UDP 1 & 2 as DEM */
    if ((usRouteId == 1) || (usRouteId == 2))
    {
        /* Convert from input CCSDS Format to DEM Format */
        demSize = DEM_CCSDS_TranslateCCSDS(&g_TO_CustomData.outConfigTbl[0], 
                                           TO_CUSTOM_CCSDS_DEM_CONFIG_SIZE,
                                           pMsg,
                                           &g_TO_CustomData.demMsgBuffer[0],
                                           TO_CUSTOM_DEM_BUFFER_SIZE);
        if (demSize > 0)
        {
            
            if (usRouteId == 1)
            {
                iSentSize = IO_TransUdpSnd((IO_TransUdp_t *) &g_TO_CustomData.udp[1], 
                                             &g_TO_CustomData.demMsgBuffer[0], 
                                             demSize);
                iStatus = TO_CustomProcessSizeSent(demSize, iSentSize, iTblIdx, 1); 
            }
            if (usRouteId == 2)
            {
                iSentSize = IO_TransUdpSnd((IO_TransUdp_t *) &g_TO_CustomData.udp[2], 
                                             &g_TO_CustomData.demMsgBuffer[0], 
                                             demSize);
                iStatus = TO_CustomProcessSizeSent(demSize, iSentSize, iTblIdx, 2); 
            }
        }
    }

    return iStatus;
}

/******************************************************************************/
/** \brief Check Data Sent Size (Local)
*******************************************************************************/
int32 TO_CustomProcessSizeSent(int32 size, int32 iSentSize, int32 idx, 
                               uint16 routeId)
{
    int32 iStatus = TO_SUCCESS;
    
    if (iSentSize < 0)
    {
        CFE_EVS_SendEvent(TO_CUSTOM_ERR_EID, CFE_EVS_ERROR,
                          "TO Output errno %d. Route ID:%u disabled ",
                          errno, routeId);
        TO_DisableRoute(routeId);
        iStatus = TO_ERROR;
    }
    else if (iSentSize != size)
    {
        CFE_SB_MsgId_t  usMsgId = TO_GetMessageID(idx);
        CFE_EVS_SendEvent(TO_CUSTOM_ERR_EID, CFE_EVS_ERROR,
                          "TO sent incomplete message. MID:%d, Route ID:%u ",
                          usMsgId, routeId);
        iStatus = TO_ERROR;
    }

    return iStatus;
}

/******************************************************************************/
/** \brief Custom Cleanup 
*******************************************************************************/
void TO_CustomCleanup(void)
{
    if (g_TO_AppData.usOutputEnabled)
    {
        CFE_EVS_SendEvent(TO_CUSTOM_INF_EID, CFE_EVS_INFORMATION, 
                          "TO - Closing Sockets."); 
        IO_TransUdpCloseSocket(&g_TO_CustomData.udp[0]);
        IO_TransUdpCloseSocket(&g_TO_CustomData.udp[1]);
        IO_TransUdpCloseSocket(&g_TO_CustomData.udp[2]);
    }

    return;
}

/******************************************************************************/
/** \brief Enable Output Command Response
*******************************************************************************/
int32 TO_CustomEnableOutputCmd(CFE_SB_Msg_t *pCmdMsg)
{
    char cDestIp[TO_MAX_IP_STRING_SIZE];
    uint16 usDestPort = 0; 
    uint16 usDefaultPort = 0;
    IO_TransUdp_t * pUdp;
    int32 iStatus = TO_ERROR;
    int32 routeMask = TO_ERROR;

    TO_EnableOutputCmd_t * pCustomCmd = (TO_EnableOutputCmd_t *) pCmdMsg;
    strncpy(cDestIp, pCustomCmd->cDestIp, sizeof(cDestIp));

    /* Note: In this example we are enabling one route at a time. Multiple 
       routes could be enabled at once, with a routeMask parameter instead of 
       routeId parameter as input if more appropriate. */

    /* Allow usRoute 0 for default CFS cmd. */
    if (pCustomCmd->usRouteId == 0)
    {
        routeMask = 0x0001; 
        usDefaultPort = TO_DEFAULT_DEST_PORT_CCSDS;
        pUdp = &g_TO_CustomData.udp[0];
    }
    else if (pCustomCmd->usRouteId == 1) 
    {
        routeMask = 0x0002; 
        usDefaultPort = TO_DEFAULT_DEST_PORT_DEM;
        pUdp = &g_TO_CustomData.udp[1];
    }
    else if (pCustomCmd->usRouteId == 2)
    {
        routeMask = 0x0004; 
        usDefaultPort = TO_DEFAULT_DEST_PORT_DEM_KSC;
        pUdp = &g_TO_CustomData.udp[2];
    }
    else
    {
        CFE_EVS_SendEvent(TO_CMD_ERR_EID, CFE_EVS_ERROR,
                          "Route ID suplied (%u) exceeds number of "
                          " configured routes. ENABLE_OUTPUT CMD ignored.",
                          pCustomCmd->usRouteId);
        routeMask = TO_ERROR;
        goto end_of_command;
    }

    if (pCustomCmd->usDestPort > 0)
    {
        usDestPort = pCustomCmd->usDestPort;
    }
    else
    {
        usDestPort = usDefaultPort;
    }
    
    iStatus = IO_TransUdpSetDestAddr(pUdp, pCustomCmd->cDestIp, usDestPort);

    if (iStatus != IO_TRANS_UDP_NO_ERROR)
    {
        routeMask = TO_ERROR;
    }
    else
    {
        TO_SetRouteAsConfigured(pCustomCmd->usRouteId);
    }

end_of_command:
    return routeMask;
}

/******************************************************************************/
/** \brief Disable Output Command Response
*******************************************************************************/
int32 TO_CustomDisableOutputCmd(CFE_SB_Msg_t *pCmdMsg)
{
    /* Disable All Output */
    g_TO_AppData.usOutputEnabled = 0;
    return TO_SUCCESS;
}


/*******************************************************************************
** Non standard custom Commands
*******************************************************************************/

/*==============================================================================
** End of file to_custom.c
**============================================================================*/
