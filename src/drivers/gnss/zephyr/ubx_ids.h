#ifndef UBX_IDS_H_
#define UBX_IDS_H_

/* Message Class and IDs */
#define UBX_ACK 0x05
#define UBX_ACK_ACK 0x01
#define UBX_ACK_NAK 0x00

#define UBX_CFG 0x06
#define UBX_CFG_RST 0x04
#define UBX_CFG_VALDEL 0x8C
#define UBX_CFG_VALGET 0x8B
#define UBX_CFG_VALSET 0x8A

#define UBX_MGA 0x13
#define UBX_MGA_ANO 0x20
#define UBX_MGA_ACK 0x60

#define UBX_NAV 0x01
#define UBX_NAV_DOP 0x04
#define UBX_NAV_PVT 0x07
#define UBX_NAV_STATUS 0x03
#define UBX_NAV_PL 0x62
#define UBX_NAV_SAT 0x35
#define UBX_MON 0x0A
#define UBX_MON_VER 0x04

#define UBX_RXM 0x02
#define UBX_RXM_PMREQ 0x41
#define UBX_RXM_PMREQ_FLAGS_BACKUP_BITFIELD (1 << 1)
#define UBX_RXM_PMREQ_FLAGS_FORCE (1 << 2)
#define UBX_RXM_PMREQ_WAKEUPSOURCES_EXTINT0_BITFIELD (1 << 5)
#define UBX_RXM_PMREQ_WAKEUPSOURCES_EXTINT1_BITFIELD (1 << 6)

#endif /* UBX_IDS_H_ */
