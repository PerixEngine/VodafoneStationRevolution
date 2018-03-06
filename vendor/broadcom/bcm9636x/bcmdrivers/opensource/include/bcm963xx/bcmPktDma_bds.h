#ifndef __PKTDMA_BDS_H_INCLUDED__
#define __PKTDMA_BDS_H_INCLUDED__

#if defined (CONFIG_BCM_ETH_JUMBO_FRAME) /* Not chip specific but feature specific */
#define ENET_MAX_MTU_PAYLOAD_SIZE  (2048)  /* Ethernet Max Payload Size - mini jumbo */
#else
#define ENET_MAX_MTU_PAYLOAD_SIZE  (1500)  /* Ethernet Max Payload Size */
#endif
#define ENET_MAX_MTU_EXTRA_SIZE  (32) /* EH_SIZE(14) + VLANTAG(4) + BRCMTAG(6) + FCS(4) + Extra(??) (4)*/
#define ENET_MAX_MTU_SIZE       (ENET_MAX_MTU_PAYLOAD_SIZE + ENET_MAX_MTU_EXTRA_SIZE)    

#define ENET_MIN_MTU_SIZE       60            /* Without FCS */
#define ENET_MIN_MTU_SIZE_EXT_SWITCH       64            /* Without FCS */

#define DMA_MAX_BURST_LENGTH    8       /* in 64 bit words */

/* SKB_HEADROOM_BUFFER_ALIGN --
 When buffers are allocated at the ethernet switch, they are in the form of
               [ FKB  | SKB_HEADROOM  | ETHERNET BUFFER (including IP)]
 byte offset : 0     32             240

 so the ethernet header is aligned on 8 byte, assuming its length is 14 or 18
 (depending on whether we use VLAN tag), the IP header will not start on 4
 byte aligned address. This will cause CPU exceptions when accessing the 32
 bit values of src/dst address.

 When this flag is on, we increase the headroom to 210 which causes the IP
 header start to be aligned.
 If you are using different configs that cause packets to come with other
 sizes of ethernet headers (e.g. with broadcom tag), you may want to turn this
 flag off. */
#define SKB_HEADROOM_BUFFER_ALIGN

#ifdef SKB_HEADROOM_BUFFER_ALIGN
#define RX_ENET_SKB_HEADROOM    (((208 + 0x0f) & ~0x0f) + 2)
#else
#define RX_ENET_SKB_HEADROOM    ((208 + 0x0f) & ~0x0f)
#endif

#define RX_BONDING_EXTRA        0
#define RX_ENET_FKB_INPLACE     sizeof(FkBuff_t)
#define SKB_ALIGNED_SIZE        ((sizeof(struct sk_buff) + 0x0f) & ~0x0f)
#define RX_BUF_LEN              ((ENET_MAX_MTU_SIZE + 63) & ~63)
#define RX_BUF_SIZE             (SKB_DATA_ALIGN(RX_ENET_FKB_INPLACE  + \
                                                RX_ENET_SKB_HEADROOM + \
                                                RX_BONDING_EXTRA     + \
                                                RX_BUF_LEN           + \
                                                sizeof(struct skb_shared_info)))

#define NON_JUMBO_MAX_MTU_SIZE  (1500 + ENET_MAX_MTU_EXTRA_SIZE)
#define NON_JUMBO_RX_BUF_LEN    ((NON_JUMBO_MAX_MTU_SIZE + 63) & ~63)

#define NON_JUMBO_RX_BUF_SIZE    (SKB_DATA_ALIGN(RX_ENET_FKB_INPLACE  + \
                                                RX_ENET_SKB_HEADROOM + \
                                                RX_BONDING_EXTRA     + \
                                                NON_JUMBO_RX_BUF_LEN + \
                                                sizeof(struct skb_shared_info)))

#endif /* __PKTDMA_BDS_H_INCLUDED__ */
