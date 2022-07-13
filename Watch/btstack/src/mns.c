#include <string.h>

#include "l2cap.h"
#include "rfcomm.h"
#include "btstack-config.h"
#include "debug.h"
#include "sdp.h"
#include "obex.h"
#include <btstack/sdp_util.h>
#include <btstack/utils.h>

#define MNS_CHANNEL 17

extern void mas_getmessage(char* id);

static void mns_callback(int code, uint8_t* lparam, uint16_t rparam);
static void mns_send(void *data, uint16_t length);

static service_record_item_t mns_service_record;
static const uint8_t  mns_service_buffer[]=
{
  0x36, 0x00, 0x4D, 0x09, 0x00, 0x00, 0x0A, 0x00, 0x01, 0x00, 0x04, 0x09, 0x00,
  0x01, 0x36, 0x00, 0x03, 0x19, 0x11, 0x33, 0x09, 0x00, 0x04, 0x36, 0x00, 0x14, 
  0x36, 0x00, 0x03, 0x19, 0x01, 0x00, 0x36, 0x00, 0x05, 0x19, 0x00, 0x03, 0x08, 
  0x11, 0x36, 0x00, 0x03, 0x19, 0x00, 0x08, 0x09, 0x00, 0x09, 0x36, 0x00, 0x09, 
  0x36, 0x00, 0x06, 0x19, 0x11, 0x34, 0x09, 0x01, 0x00, 0x09, 0x01, 0x00, 0x25, 
  0x0E, 0x4D, 0x41, 0x50, 0x20, 0x4D, 0x4E, 0x53, 0x2D, 0x4B, 0x72, 0x65, 0x79, 
  0x6F, 0x73,
};
static uint16_t rfcomm_channel_id;
static struct obex_state mns_obex_state;
static const struct obex mns_obex = 
{
  &mns_obex_state, mns_callback, mns_send
};
static uint16_t mns_response_size;
static void*    mns_response_buffer;

static enum {STATE_0} state;

static const uint8_t MNS_TARGET[16] =
{
  0xbb, 0x58, 0x2b, 0x41, 0x42, 0x0c, 0x11, 0xdb, 0xb0, 0xde, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66
};


static void sdp_create_map_service(uint8_t *service, int service_id, const char *name) {

	uint8_t* attribute;
	de_create_sequence(service);

        // 0x0000 "Service Record Handle"
	de_add_number(service, DE_UINT, DE_SIZE_16, SDP_ServiceRecordHandle);
	de_add_number(service, DE_UINT, DE_SIZE_32, 0x10004);

	// 0x0001 "Service Class ID List"
	de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ServiceClassIDList);
	attribute = de_push_sequence(service);
	{
		de_add_number(attribute,  DE_UUID, DE_SIZE_16, 0x1133 );
	}
	de_pop_sequence(service, attribute);

	// 0x0004 "Protocol Descriptor List"
	de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ProtocolDescriptorList);
	attribute = de_push_sequence(service);
	{
		uint8_t* l2cpProtocol = de_push_sequence(attribute);
		{
			de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, 0x0100);
		}
		de_pop_sequence(attribute, l2cpProtocol);

		uint8_t* rfcomm = de_push_sequence(attribute);
		{
			de_add_number(rfcomm,  DE_UUID, DE_SIZE_16, 0x0003);  // rfcomm_service
			de_add_number(rfcomm,  DE_UINT, DE_SIZE_8,  service_id);  // rfcomm channel
		}
		de_pop_sequence(attribute, rfcomm);

                uint8_t* obex = de_push_sequence(attribute);
		{
			de_add_number(obex,  DE_UUID, DE_SIZE_16, 0x0008);
		}
		de_pop_sequence(attribute, obex);
	}
	de_pop_sequence(service, attribute);

	// 0x0009 "Bluetooth Profile Descriptor List"
	de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BluetoothProfileDescriptorList);
	attribute = de_push_sequence(service);
	{
		uint8_t *mapProfile = de_push_sequence(attribute);
		{
			de_add_number(mapProfile,  DE_UUID, DE_SIZE_16, 0x1134);
                        de_add_number(mapProfile,  DE_UINT, DE_SIZE_16, 0x0100);
		}
		de_pop_sequence(attribute, mapProfile);
	}
	de_pop_sequence(service, attribute);

	// 0x0100 "ServiceName"
	de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
	de_add_data(service,  DE_STRING, strlen(name), (uint8_t *) name);
}

static void mns_try_respond(uint16_t rfcomm_channel_id){
    if (!mns_response_size) return;
    if (!rfcomm_channel_id) return;

    // update state before sending packet (avoid getting called when new l2cap credit gets emitted)
    uint16_t size = mns_response_size;
    mns_response_size = 0;
    if (rfcomm_send_internal(rfcomm_channel_id, mns_response_buffer, size) != 0)
    {
      // if error, we need retry
      mns_response_size = size;
    }
    else
    {
      log_info("MNS Sent %d byte: ", size);
      hexdump(mns_response_buffer, size);
    }
}

static void mns_send(void *data, uint16_t length)
{
  mns_response_buffer = data;
  mns_response_size = length;

  mns_try_respond(rfcomm_channel_id);  
}

static uint8_t buf[20];
static void mns_callback(int code, uint8_t* header, uint16_t length)
{
  log_info("MNS Callback with code %d\n", code);
  uint8_t *ptr, *handler = NULL;

  switch(code)
  {
    case OBEX_OP_CONNECT:
    // client try to make a connection
    // validate the target
    while(header != NULL && length > 0)
    {
      switch(*header)
      {
        case OBEX_HEADER_TARGET:
        {
          if (READ_NET_16(header, 1) == (16 + 3) &&
            memcmp((uint8_t*)header + 3, MNS_TARGET, 16) == 0)
          {
            // send response
              log_info("connection successful\n");
              uint8_t *ptr = obex_create_connect_request(&mns_obex, 0xA0, buf);
              ptr = obex_header_add_uint32(ptr, OBEX_HEADER_CONNID, 0x123456);
              obex_send_response(&mns_obex, buf, ptr - buf);
          }
          else
          {
              log_info("connection failed\n");
              uint8_t *ptr = obex_create_connect_request(&mns_obex, 0xC4, buf);
              obex_send_response(&mns_obex, buf, ptr - buf);
          }
          return;
        }
      }

      header = obex_header_get_next(header, &length);
    }

    ptr = obex_create_connect_request(&mns_obex, 0xD0, buf);
    obex_send_response(&mns_obex, buf, ptr - buf);
    break;
  case OBEX_OP_PUT:
  case OBEX_OP_PUT | OBEX_OP_FINAL:
    while(header != NULL && length > 0)
    {
      switch(*header)
      {
        case OBEX_HEADER_BODY:
        case OBEX_HEADER_ENDBODY:
        {
          // compose a request to MAS connection
          uint16_t len = READ_NET_16((uint8_t*)header, 1);
          // mark the end
          //hexdump(header, len);
          header[len+1] = '\0';
          log_info("BODY %s: ", header + 3);
          char *start;
          if (!((start = strstr(header + 3, "handle"))
            && (start = strchr(start, '='))
            && (start = strchr(start, '"'))))
          {
            log_info("fail to find start\n");
            break;
          }
          start++;

          char* end = strchr(start, '"');
          if (end != NULL)
          {
            *end = 0;
            handler = start;
            mas_getmessage(handler);
          }
          else
            log_info("fail to find end\n");
          break;
        }

        case OBEX_HEADER_APPPARMS:
        case OBEX_HEADER_TYPE:
        {
          uint16_t len = READ_NET_16((uint8_t*)header, 1);
          log_info("type %x: ", *header);
          hexdump((uint8_t*)header, len);
          break;
        }
        case OBEX_HEADER_CONNID:
        {
          log_info("connid ");
          hexdump((uint8_t*)header + 1, 4);   
          break;
        }
      }
      header = obex_header_get_next(header, &length);
    }

    if (handler)
    {
      ptr = obex_create_request(&mns_obex, 0xA0, buf);
      obex_send_response(&mns_obex, buf, ptr - buf);
    }
    else
    {
      if (code & OBEX_OP_FINAL)
        ptr = obex_create_request(&mns_obex, 0xD0, buf);
      else      
        ptr = obex_create_request(&mns_obex, 0x90, buf);
      obex_send_response(&mns_obex, buf, ptr - buf);   
    }
    break;
  }
}

static void mns_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
  switch(packet_type)
  {
  case RFCOMM_DATA_PACKET:
    log_info("MNS received %d bytes: ", size);
    hexdump(packet, size);
    obex_handle(&mns_obex, packet, size);
    rfcomm_grant_credits(rfcomm_channel_id, 1); // get the next packet
    break;
  case DAEMON_EVENT_PACKET:
    switch(packet[0])    
    {
      case DAEMON_EVENT_NEW_RFCOMM_CREDITS:
      case DAEMON_EVENT_HCI_PACKET_SENT:
      case RFCOMM_EVENT_CREDITS:
        {
          mns_try_respond(rfcomm_channel_id);
          break;
        }
    }
    break;
  case HCI_EVENT_PACKET:
    {
      switch(packet[0])
      {
      case RFCOMM_EVENT_INCOMING_CONNECTION:
      {
          uint8_t   rfcomm_channel_nr;
          bd_addr_t event_addr;
          // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
          bt_flip_addr(event_addr, &packet[2]);
          rfcomm_channel_nr = packet[8];
          uint16_t rfcomm_id = READ_BT_16(packet, 9);
          log_info("MNS channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
          if (rfcomm_channel_id == 0)
          {
            rfcomm_channel_id = rfcomm_id;
            rfcomm_accept_connection_internal(rfcomm_id);
            break;
          }
          else
          {
            rfcomm_decline_connection_internal(rfcomm_id);
          }
          break;
        }
      case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
        {
          if (packet[2])
          {
            rfcomm_channel_id = 0;
          }
          else
          {
            obex_init(&mns_obex);
          }
          break;
        }
      case DAEMON_EVENT_NEW_RFCOMM_CREDITS:
      case DAEMON_EVENT_HCI_PACKET_SENT:
      case RFCOMM_EVENT_CREDITS:
        {
          mns_try_respond(rfcomm_channel_id);
          break;
        }
      case RFCOMM_EVENT_CHANNEL_CLOSED:
        {
          if (rfcomm_channel_id)
          {
            rfcomm_channel_id = 0;
          }
          break;
        }
      }
    }
  }
}

int mns_init()
{
  memset(&mns_service_record, 0, sizeof(mns_service_record));
  mns_service_record.service_record = (uint8_t*)&mns_service_buffer[0];
#if 0
  sdp_create_map_service( (uint8_t*)&mns_service_buffer[0], MNS_CHANNEL, "MAP MNS-Kreyos");
  log_info("MNS service buffer size: %u\n", de_get_len(mns_service_record.service_record));
  hexdump((void*)mns_service_buffer, de_get_len(mns_service_record.service_record));
  //de_dump_data_element(mns_service_record.service_record);
#endif
  sdp_register_service_internal(NULL, &mns_service_record);

  // register to obex
  rfcomm_register_service_internal(NULL, mns_packet_handler, MNS_CHANNEL, 0xffff);
  return 0;
}