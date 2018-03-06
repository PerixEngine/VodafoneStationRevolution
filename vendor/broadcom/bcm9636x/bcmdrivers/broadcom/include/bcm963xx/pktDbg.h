#ifndef __PKT_DBG_H_INCLUDED__
#define __PKT_DBG_H_INCLUDED__

/*
<:copyright-broadcom

 Copyright (c) 2007 Broadcom Corporation
 All Rights Reserved
 No portions of this material may be reproduced in any form without the
 written permission of:
          Broadcom Corporation
          5300 California Avenue
          Irvine, California 92617
 All information contained in this document is Broadcom Corporation
 company private, proprietary, and trade secret.

:>
*/

#if defined( __KERNEL__ )
#define print               printk
#else
#define print               printf
#endif



#include <linux/bcm_colors.h>


/*
 *------------------------------------------------------------------------------
 * Macros for generic debug and assert support per subsystem/layer.
 * These Macros should not be used for data path (per packet processing) from
 * Release 4.10.1 onwards.
 * - declare an int variable per subsystem scope: pktDbgLvl.
 * - define a constant string prefix: DBGsys
 *
 * - define: PKT_DBG_SUPPORTED
 * - define: PKT_ASSERT_SUPPORTED
 *------------------------------------------------------------------------------
 */
#if defined(PKT_DBG_SUPPORTED)
#define DBGCODE(code)       code
#else
#define DBGCODE(code)       NULL_STMT
#endif

/* Suggested debug levels and verbosity */
#define DBG_BASIC           0           /* Independent of pktDbgLvl */
#define DBG_STATE           1           /* Subsystem state change */
#define DBG_EXTIF           2           /* External interface */
#define DBG_INTIF           3           /* Internal interface */
#define DBG_CTLFL           4           /* Algorithm and control flow */
#define DBG_PDATA           5           /* Context, state and data dumps */
#define DBG_BKGRD           6           /* Background timers */

#define DBG(stmts)                                                          \
        DBGCODE( if ( pktDbgLvl ) do { stmts } while(0) )

#define DBGL(lvl, stmts)                                                    \
        DBGCODE( if ( pktDbgLvl >= lvl ) do { stmts } while(0) )

#define dbgl_func(lvl)                                                       \
        DBGCODE( if ( pktDbgLvl >= lvl )                                    \
                     print( CLRsys DBGsys " %-10s:" CLRnl, __FUNCTION__ ) )

#define dbg_print(fmt, arg...)                                              \
        DBGCODE( if ( pktDbgLvl )                                           \
                     print( CLRsys DBGsys fmt CLRnl, ##arg ) )

#define dbgl_print(lvl, fmt, arg...)                                        \
        DBGCODE( if ( pktDbgLvl >= lvl )                                    \
                     print( CLRsys DBGsys " %-10s:" fmt CLRnl,              \
                                          __FUNCTION__, ##arg ) )

#define dbg_error(fmt, arg...)                                              \
        DBGCODE( if ( pktDbgLvl )                                           \
                 print( CLRerr DBGsys " %-10s ERROR: " fmt CLRnl,           \
                                      __FUNCTION__,  ##arg ) )

#define dbg_config(lvl)                                                     \
        DBGCODE( pktDbgLvl = lvl;                                           \
                 print( CLRhigh DBGsys " pktDbgLvl[0x%08x]=%d" CLRnl,       \
                                       (int)&pktDbgLvl, pktDbgLvl ) )

#if defined(PKT_ASSERT_SUPPORTED)
#define ASSERTCODE(code)    code
#else
#define ASSERTCODE(code)    NULL_STMT
#endif

#define dbg_assertv(cond)                                                   \
        ASSERTCODE( if ( !cond )                                            \
                    {                                                       \
                        print( CLRerr DBGsys " %-10s ASSERT: "              \
                               #cond CLRnl, __FUNCTION__ );                 \
                        return;                                             \
                    } )

#define dbg_assertr(cond, rtn)                                              \
        ASSERTCODE( if ( !cond )                                            \
                    {                                                       \
                        print( CLRerr DBGsys " %-10s ASSERT: "              \
                               #cond CLRnl, __FUNCTION__ );                 \
                        return rtn;                                         \
                    } )

/*
 *------------------------------------------------------------------------------
 * Macros for generic debug and assert support for data path.
 * - define: PKTDBG
 *------------------------------------------------------------------------------
 */
#if defined(PKTDBG)
#define pkt_dbgl_func   dbgl_func
#define pkt_dbg_print   dbg_print
#define pkt_dbgl_print  dbgl_print
#define pkt_dbg_error   dbg_error
#define pkt_dbg_assertv dbg_assertv
#define pkt_dbg_assertr dbg_assertr
#else
#define pkt_dbgl_func(lvl)               NULL_STMT
#define pkt_dbg_print(fmt, arg...)       NULL_STMT
#define pkt_dbgl_print(lvl, fmt, arg...) NULL_STMT
#define pkt_dbg_error(fmt, arg...)       NULL_STMT
#define pkt_dbg_assertv(cond)            NULL_STMT 
#define pkt_dbg_assertr(cond, rtn)       NULL_STMT
#endif

#endif  /* defined(__PKT_DBG_H_INCLUDED__) */

