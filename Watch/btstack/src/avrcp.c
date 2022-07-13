#include <stdio.h>
#include <string.h>

#include "l2cap.h"
#include "btstack-config.h"
#include "debug.h"

#include "sdp.h"
#include "avrcp.h"
#include "avctp.h"
#include "btstack/sdp_util.h"
#include "btstack/utils.h"

#include "window.h"

#define htons(x) __swap_bytes(x)

/* Company IDs for vendor dependent commands */
#define IEEEID_BTSIG		0x001958

/* Error codes for metadata transfer */
#define E_INVALID_COMMAND	0x00#define E_INVALID_PARAM		0x01
#define E_PARAM_NOT_FOUND	0x02
#define E_INTERNAL		0x03

/* Packet types */
#define AVRCP_PACKET_TYPE_SINGLE	0x00
#define AVRCP_PACKET_TYPE_START		0x01
#define AVRCP_PACKET_TYPE_CONTINUING	0x02
#define AVRCP_PACKET_TYPE_END		0x03

/* PDU types for metadata transfer */
#define AVRCP_GET_CAPABILITIES		0x10
#define AVRCP_LIST_PLAYER_ATTRIBUTES	0X11
#define AVRCP_LIST_PLAYER_VALUES	0x12
#define AVRCP_GET_CURRENT_PLAYER_VALUE	0x13
#define AVRCP_SET_PLAYER_VALUE		0x14
#define AVRCP_GET_PLAYER_ATTRIBUTE_TEXT	0x15
#define AVRCP_GET_PLAYER_VALUE_TEXT	0x16
#define AVRCP_DISPLAYABLE_CHARSET	0x17
#define AVRCP_CT_BATTERY_STATUS		0x18
#define AVRCP_GET_ELEMENT_ATTRIBUTES	0x20
#define AVRCP_GET_PLAY_STATUS		0x30
#define AVRCP_REGISTER_NOTIFICATION	0x31
#define AVRCP_REQUEST_CONTINUING	0x40
#define AVRCP_ABORT_CONTINUING		0x41
#define AVRCP_SET_ABSOLUTE_VOLUME	0x50

/* Capabilities for AVRCP_GET_CAPABILITIES pdu */
#define CAP_COMPANY_ID		0x02
#define CAP_EVENTS_SUPPORTED	0x03

#define AVRCP_REGISTER_NOTIFICATION_PARAM_LENGTH 5

#define AVRCP_FEATURE_CATEGORY_1	0x0001
#define AVRCP_FEATURE_CATEGORY_2	0x0002
#define AVRCP_FEATURE_CATEGORY_3	0x0004
#define AVRCP_FEATURE_CATEGORY_4	0x0008
#define AVRCP_FEATURE_PLAYER_SETTINGS	0x0010

enum battery_status {
  BATTERY_STATUS_NORMAL =		0,
  BATTERY_STATUS_WARNING =	1,
  BATTERY_STATUS_CRITICAL =	2,
  BATTERY_STATUS_EXTERNAL =	3,
  BATTERY_STATUS_FULL_CHARGE =	4,
};

static service_record_item_t avrcp_service_record;
static const uint8_t   avrcp_service_buffer[87] =
{
  0x36,0x00,0x54,0x09,0x00,0x00,0x0A,0x00,0x01,0x00,0x03,0x09,0x00,0x01,0x36,
  0x00,0x06,0x19,0x11,0x0E,0x19,0x11,0x0F,0x09,0x00,0x04,0x36,0x00,0x12,0x36,
  0x00,0x06,0x19,0x01,0x00,0x09,0x00,0x17,0x36,0x00,0x06,0x19,0x00,0x17,0x09,
  0x01,0x04,0x09,0x00,0x05,0x36,0x00,0x03,0x19,0x10,0x02,0x09,0x00,0x09,0x36,
  0x00,0x09,0x36,0x00,0x06,0x19,0x11,0x0E,0x09,0x01,0x05,0x09,0x01,0x00,0x25,
  0x05,0x41,0x56,0x52,0x43,0x50,0x09,0x03,0x11,0x09,0x00,0x01
};

#pragma pack(1)
struct avrcp_header {
  uint8_t company_id[3];
  uint8_t pdu_id;
  uint8_t packet_type:2;
  uint8_t rsvd:6;
  uint16_t params_len;
  uint8_t params[0];
};
#define AVRCP_HEADER_LENGTH 7
#pragma pack()

static int init_state;
static uint8_t events_flag;

static void handle_notification(uint8_t code, struct avrcp_header *pdu )
{
  switch(code)
  {
  case AVC_CTYPE_INTERIM:
    if (!(events_flag & (1 << pdu->params[0])))
    {
      return;
    }
    break;
  case AVC_CTYPE_CHANGED:
    // reneable
    log_info("reenable event notification %d\n", pdu->params[0]);
    avrcp_enable_notification(pdu->params[0]);
    events_flag |= 1 << (pdu->params[0]);
    return;
  case AVC_CTYPE_ACCEPTED:
    break;
  default:
    log_error("fail to register event: %d\n", pdu->params[0]);
    return;
  }

  switch(pdu->params[0])
  {
  case AVRCP_EVENT_STATUS_CHANGED:
    log_info("current status is %d\n", pdu->params[1]);
    window_postmessage(EVENT_AV, EVENT_AV_STATUS, (void*)pdu->params[1]);
    //avrcp_get_playstatus();
    break;
  case AVRCP_EVENT_TRACK_CHANGED:
    {
      // uint32_t id[2];
      //TODO : read id
      window_postmessage(EVENT_AV, EVENT_AV_TRACK, NULL);
      log_info("AVRCP_EVENT_TRACK_CHANGED\n");
      break;
    }
  case AVRCP_EVENT_PLAYBACK_POS_CHANGED:
    {
      log_info("AVRCP_EVENT_PLAYBACK_POS_CHANGED\n");
      window_postmessage(EVENT_AV, EVENT_AV_POS, (void*)READ_NET_32(pdu->params, 1));
      break;
    }
  case AVRCP_EVENT_TRACK_REACHED_END:
  case AVRCP_EVENT_TRACK_REACHED_START:
    log_info("AVRCP_EVENT_TRACK_REACHED_START/END\n");
    //window_postmessage(EVENT_AV, EVENT_AV_POS, (void*)0);
    break;
  case AVRCP_EVENT_NOW_PLAYING_CONTENT_CHANGED:
    log_info("AVRCP_EVENT_NOW_PLAYING_CONTENT_CHANGED\n");  
    break;
  case AVRCP_EVENT_ADDRESSED_PLAYER_CHANGED:
    log_info("AVRCP_EVENT_ADDRESSED_PLAYER_CHANGED\n");  
    break;
  case AVRCP_EVENT_AVAILABLE_PLAYERS_CHANGED:
    log_info("AVRCP_EVENT_AVAILABLE_PLAYERS_CHANGED\n");  
    break;
  }
}

static void handle_attributes(struct avrcp_header *pdu)
{
  uint8_t elementnum = pdu->params[0];
  uint16_t offset = 1;
  for (int i = 0; i < elementnum; i++)
  {
    uint32_t attributeid = READ_NET_32(pdu->params, offset);
    offset += 4;
    uint16_t charsetid = READ_NET_16(pdu->params, offset);
    offset += 2;
    uint16_t len = READ_NET_16(pdu->params, offset);
    offset += 2;
    if (len > 0)
    {
      pdu->params[offset + len] = 0;
      log_info("attribute %ld charset %d len %d : %s\n", attributeid, charsetid,
             len, &pdu->params[offset]);
      const char* data = (const char*)&pdu->params[offset];
      switch(attributeid)
      {
        case AVRCP_MEDIA_ATTRIBUTE_TITLE:
        window_postmessage(EVENT_AV, EVENT_AV_TITLE, (void*)data);
        break;
        case AVRCP_MEDIA_ATTRIBUTE_ARTIST:
        window_postmessage(EVENT_AV, EVENT_AV_ARTIST, (void*)data);
        break;
        case AVRCP_MEDIA_ATTRIBUTE_DURATION:
        {
          uint32_t length;
          if (sscanf(data, "%ld", &length) == 1)
            window_postmessage(EVENT_AV, EVENT_AV_LENGTH, (void*)length);
          break;
        }
      }
      offset += len;
    }
  }
}

static void handle_playstatus(struct avrcp_header* pdu)
{
  uint32_t length = READ_NET_32(pdu->params, 0);
  uint32_t pos  = READ_NET_32(pdu->params, 4);

  uint8_t status = pdu->params[8];

  log_info("play status : %ld of %ld, status: %d\n", pos, length, (uint16_t)status);
  
  window_postmessage(EVENT_AV, EVENT_AV_STATUS, (void*)status);
  if (length != -1)
    window_postmessage(EVENT_AV, EVENT_AV_LENGTH, (void*)length);
  if (pos != -1)
    window_postmessage(EVENT_AV, EVENT_AV_POS, (void*)pos);

  return;
}

static void handle_pdu(uint8_t code, uint8_t *data, uint16_t size)
{
  struct avrcp_header *pdu = (struct avrcp_header *)data;

  if (data == NULL)
  {
    // special event
    switch(size)
    {
    case 0:
      // disconect
      window_postmessage(EVENT_AV, EVENT_AV_DISCONNECTED, 0);
      return;
    case 1:
      init_state = 1;
      events_flag = 0;
      avrcp_get_playstatus();
      window_postmessage(EVENT_AV, EVENT_AV_CONNECTED, 0);
      return;
    }
  }

  if (init_state)
  {
    switch(init_state++)
    {
      case 1:
        avrcp_enable_notification(AVRCP_EVENT_AVAILABLE_PLAYERS_CHANGED);
        break;
      case 2:
        avrcp_enable_notification(AVRCP_EVENT_ADDRESSED_PLAYER_CHANGED);
        break;
      case 3:
	{
        uint16_t playerid = READ_NET_16(pdu->params, 1);
        avrcp_set_player(playerid);
        break;
	}
      case 4:
        avrcp_enable_notification(AVRCP_EVENT_TRACK_CHANGED);
      break;
      case 5:
        avrcp_enable_notification(AVRCP_EVENT_STATUS_CHANGED);
      break;
      case 6:
        avrcp_enable_notification(AVRCP_EVENT_NOW_PLAYING_CONTENT_CHANGED);
        break;
      case 7:
        avrcp_get_playstatus();
        break;
      case 8:
        avrcp_get_attributes(0);
      default:
        init_state = 0;
        break;
    }
  }

  log_info("response pdu code=%d, id=%d\n", code, pdu->pdu_id);
  if (code == AVC_CTYPE_REJECTED || code == AVC_CTYPE_NOT_IMPLEMENTED )
    return;
  //hexdump(pdu->params, __swap_bytes(pdu->params_len));
  switch(pdu->pdu_id)
  {
  case AVRCP_REGISTER_NOTIFICATION:
    handle_notification(code, pdu);
    break;
  case AVRCP_GET_CAPABILITIES:
    //handle_capabilities(code, pdu);
    break;
  case AVRCP_GET_ELEMENT_ATTRIBUTES:
    handle_attributes(pdu);
    break;
  case AVRCP_GET_PLAY_STATUS:
    handle_playstatus(pdu);
    break;
  default:
    log_info("unknow pdu\n");
    break;
  }
}

static void sdp_create_avrcp_service(uint8_t *service, const char *name){

  uint8_t* attribute;
  de_create_sequence(service);

  // 0x0000 "Service Record Handle"
  de_add_number(service, DE_UINT, DE_SIZE_16, SDP_ServiceRecordHandle);
  de_add_number(service, DE_UINT, DE_SIZE_32, 0x10003);

  // 0x0001 "Service Class ID List"
  de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ServiceClassIDList);
  attribute = de_push_sequence(service);
  {
    de_add_number(attribute,  DE_UUID, DE_SIZE_16, 0x110E );
    de_add_number(attribute,  DE_UUID, DE_SIZE_16, 0x110F );
  }
  de_pop_sequence(service, attribute);

  // 0x0004 "Protocol Descriptor List"
  de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ProtocolDescriptorList);
  attribute = de_push_sequence(service);
  {
    uint8_t* l2cpProtocol = de_push_sequence(attribute);
    {
      de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, 0x0100);
      de_add_number(l2cpProtocol,  DE_UINT, DE_SIZE_16, 0x0017);  // PSM_AVRCP
    }
    de_pop_sequence(attribute, l2cpProtocol);

    uint8_t* avctp = de_push_sequence(attribute);
    {
      de_add_number(avctp,  DE_UUID, DE_SIZE_16, 0x0017);  // avctp_service
      de_add_number(avctp,  DE_UINT, DE_SIZE_16, 0x0104);  // version
    }
    de_pop_sequence(attribute, avctp);
  }
  de_pop_sequence(service, attribute);

  // 0x0005 "Public Browse Group"
  de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BrowseGroupList); // public browse group
  attribute = de_push_sequence(service);
  {
    de_add_number(attribute,  DE_UUID, DE_SIZE_16, 0x1002 );
  }
  de_pop_sequence(service, attribute);

  // 0x0009 "Bluetooth Profile Descriptor List"
  de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BluetoothProfileDescriptorList);
  attribute = de_push_sequence(service);
  {
    uint8_t *avrcpProfile = de_push_sequence(attribute);
    {
      de_add_number(avrcpProfile,  DE_UUID, DE_SIZE_16, 0x110E);
      de_add_number(avrcpProfile,  DE_UINT, DE_SIZE_16, 0x0105); // version
    }
    de_pop_sequence(attribute, avrcpProfile);
  }
  de_pop_sequence(service, attribute);

  // 0x0100 "ServiceName"
  de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
  de_add_data(service,  DE_STRING, strlen(name), (uint8_t *) name);

  // 0x0311 "SupportedFeatures"
  de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_SupportedFeatures);
  de_add_number(service,  DE_UINT, DE_SIZE_16, 0x01); // Catagory 1

}

void avrcp_init()
{
  memset(&avrcp_service_record, 0, sizeof(avrcp_service_record));
  avrcp_service_record.service_record = (uint8_t*)&avrcp_service_buffer[0];
#if 0
  sdp_create_avrcp_service(avrcp_service_record.service_record, "AVRCP");
  log_info("SDP service buffer size: %u\n", de_get_len(avrcp_service_record.service_record));
  hexdump((void*)avrcp_service_buffer, de_get_len(avrcp_service_record.service_record));
  //de_dump_data_element(service_record_item->service_record);
#endif
  sdp_register_service_internal(NULL, &avrcp_service_record);
  avctp_register_pid(0x110E, handle_pdu);
}

/*
* set_company_id:
*
* Set three-byte Company_ID into outgoing AVRCP message
*/
static inline void set_company_id(uint8_t cid[3], const uint32_t cid_in)
{
  cid[0] = cid_in >> 16;
  cid[1] = cid_in >> 8;
  cid[2] = cid_in;
}


int avrcp_enable_notification(uint8_t id)
{
  log_info("avrcp_enable_notification: %d\n", id);
  uint8_t buf[AVRCP_HEADER_LENGTH + 5];
  struct avrcp_header *pdu = (void *) buf;

  memset(buf, 0, sizeof(buf));

  set_company_id(pdu->company_id, IEEEID_BTSIG);

  pdu->pdu_id = AVRCP_REGISTER_NOTIFICATION;
  pdu->params[0] = id;
  net_store_32(pdu->params, 1, 1000); // only for track change
  pdu->params_len = htons(5);

  return avctp_send_vendordep(AVC_CTYPE_NOTIFY, AVC_SUBUNIT_PANEL,
                              buf, 5 + AVRCP_HEADER_LENGTH);
}

int avrcp_set_volume(uint8_t volume)
{
  log_info("avrcp_set_volume: %d\n", volume);
  uint8_t buf[AVRCP_HEADER_LENGTH + 1];
  struct avrcp_header *pdu = (void *) buf;

  memset(buf, 0, sizeof(buf));
  set_company_id(pdu->company_id, IEEEID_BTSIG);

  pdu->pdu_id = AVRCP_SET_ABSOLUTE_VOLUME;
  pdu->params[0] = volume;
  pdu->params_len = htons(1);

  return avctp_send_vendordep(AVC_CTYPE_CONTROL, AVC_SUBUNIT_PANEL, buf, sizeof(buf));
}

int avrcp_get_capability()
{
  log_info("avrcp_get_capability\n");
  uint8_t buf[AVRCP_HEADER_LENGTH + 1];
  struct avrcp_header *pdu = (void *) buf;

  memset(buf, 0, sizeof(buf));
  set_company_id(pdu->company_id, IEEEID_BTSIG);

  pdu->pdu_id = AVRCP_GET_CAPABILITIES;
  pdu->params[0] = 0x03;
  pdu->params_len = htons(1);

  return avctp_send_vendordep(AVC_CTYPE_STATUS, AVC_SUBUNIT_PANEL, buf, sizeof(buf));
}

int avrcp_get_playstatus()
{
  log_info("avrcp_get_playstatus\n");
  uint8_t buf[AVRCP_HEADER_LENGTH];
  struct avrcp_header *pdu = (void *) buf;

  memset(buf, 0, sizeof(buf));
  set_company_id(pdu->company_id, IEEEID_BTSIG);

  pdu->pdu_id = AVRCP_GET_PLAY_STATUS;
  pdu->params_len = 0;

  return avctp_send_vendordep(AVC_CTYPE_STATUS, AVC_SUBUNIT_PANEL, buf, sizeof(buf));
}

int avrcp_get_attributes(uint32_t id)
{
  log_info("avrcp_get_attributes\n");
  uint8_t buf[AVRCP_HEADER_LENGTH + 8 + 1 + 4 * 3]; // 3 parameter
  struct avrcp_header *pdu = (void *) buf;

  memset(buf, 0, sizeof(buf));
  set_company_id(pdu->company_id, IEEEID_BTSIG);

  pdu->pdu_id = AVRCP_GET_ELEMENT_ATTRIBUTES;
  pdu->params[8] = 3;
  net_store_32(pdu->params, 9, AVRCP_MEDIA_ATTRIBUTE_TITLE);
  net_store_32(pdu->params, 13, AVRCP_MEDIA_ATTRIBUTE_ARTIST);
  net_store_32(pdu->params, 17, AVRCP_MEDIA_ATTRIBUTE_DURATION);
  pdu->params_len = htons(21);

  return avctp_send_vendordep(AVC_CTYPE_STATUS, AVC_SUBUNIT_PANEL, buf, sizeof(buf));
}

int avrcp_set_player(uint16_t playerid)
{
  log_info("avrcp_set_player %d\n", playerid);
  uint8_t buf[AVRCP_HEADER_LENGTH + 2]; // 3 parameter
  struct avrcp_header *pdu = (void *) buf;

  memset(buf, 0, sizeof(buf));
  set_company_id(pdu->company_id, IEEEID_BTSIG);

  pdu->pdu_id = AVRCP_SET_PLAYER_VALUE;
  net_store_16(pdu->params, 0, playerid);
  pdu->params_len = htons(2);

  return avctp_send_vendordep(AVC_CTYPE_CONTROL, AVC_SUBUNIT_PANEL, buf, sizeof(buf));
}


void avrcp_connect(bd_addr_t remote_addr)
{
  if (!avctp_connected())
  {
    avctp_connect(remote_addr);
  }
  else
  {
    avrcp_get_playstatus();
  }
}

void avrcp_disconnect()
{
  avctp_disconnect();
}

uint8_t avrcp_connected()
{
 return avctp_connected(); 
}
