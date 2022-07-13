#include <string.h>

#include "l2cap.h"
#include "rfcomm.h"
#include "btstack-config.h"
#include "debug.h"
#include "sdp.h"
#include "obex.h"
#include <btstack/sdp_util.h>
#include <btstack/utils.h>
#include <string.h>

#include "window.h"
/*
 * MAS implementation
 * Connect, EnableNotification
 */

static const uint8_t MAS_TARGET[16] =
{
 0xbb, 0x58, 0x2b, 0x40, 0x42, 0x0c, 0x11, 0xdb, 0xb0, 0xde, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66
};

static void mas_callback(int code, uint8_t* lparam, uint16_t rparam);
static void mas_send(void *data, uint16_t length);
static uint16_t rfcomm_channel_id;
static struct obex_state mas_obex_state;
static uint16_t handler[16];
static uint8_t handler_size = 0;
static const struct obex mas_obex = 
{
  &mas_obex_state, mas_callback, mas_send
};
static uint16_t mas_response_size;
static void*    mas_response_buffer;

static void mas_try_respond(uint16_t rfcomm_channel_id){
    if (!mas_response_size) return;
    if (!rfcomm_channel_id) return;

    // update state before sending packet (avoid getting called when new l2cap credit gets emitted)
    uint16_t size = mas_response_size;
    mas_response_size = 0;
    if (rfcomm_send_internal(rfcomm_channel_id, mas_response_buffer, size) != 0)
    {
      // if error, we need retry
      mas_response_size = size;
    }
    else
    {
      //printf("MAS Sent %d byte: ", size);
      //hexdump(mas_response_buffer, size);
    }
}

const static uint8_t appparams_notify[] = 
{
  0x0e,0x01,0x01
};
const static char type_notify[] = "x-bt/MAP-NotificationRegistration";

const static uint8_t appparams_getmessage[] = 
{
  0x0a, 0x01, 0x00, // Attachment
  0x14, 0x01, 0x00, //Charset
  0x15, 0x01, 0x00,//FractionRequest
};

const static uint8_t appparams_getmessage_next[] = 
{
  0x0a, 0x01, 0x00, // Attachment
  0x14, 0x01, 0x00, //Charset
  0x15, 0x01, 0x01,//FractionRequest
};

const static char type_getmessage[] = "x-bt/message";

static uint8_t mas_buf[100];

/*
 * Parse content-type = x-bt/message
 */
static const char BEGINMSG[] = "BEGIN:MSG\r\n";
static const char BEGINTYPE[] = "TYPE:";
static const char BEGINFN[] = "FN;CHARSET=UTF-8:";
static const char END[] = "\r\n";
static char content[220];
static char title[20];
static uint8_t content_type, content_size;

#define ICON_FACEBOOK 's'
#define ICON_TWITTER  't'
#define ICON_MSG      'u'

static enum {
  STATE_INIT,
//  STATE_GETTYPE,
  STATE_GETFROM,
  STATE_GETCONTENTSTART,
  STATE_GETCONTENTEND,

  STATE_ERROR
}state;

static void mas_send(void *data, uint16_t length)
{
  mas_response_buffer = data;
  mas_response_size = length;

  mas_try_respond(rfcomm_channel_id);  
}

static void mas_getmessage_internal(int first)
{
  uint8_t *ptr = obex_create_request(&mas_obex, OBEX_OP_GET + OBEX_OP_FINAL, mas_buf);
  
  ptr = obex_header_add_uint32(ptr, OBEX_HEADER_CONNID, mas_obex.state->connection);
  ptr = obex_header_add_text(ptr, OBEX_HEADER_NAME, handler, handler_size);
  ptr = obex_header_add_bytes(ptr, OBEX_HEADER_TYPE, (uint8_t*)type_getmessage, sizeof(type_getmessage));
  if (first)
    ptr = obex_header_add_bytes(ptr, OBEX_HEADER_APPPARMS, appparams_getmessage, sizeof(appparams_getmessage));
  else
    ptr = obex_header_add_bytes(ptr, OBEX_HEADER_APPPARMS, appparams_getmessage, sizeof(appparams_getmessage_next));
    
  obex_send_request(&mas_obex, mas_buf, ptr - mas_buf);  
}

void mas_getmessage(char* id)
{
  //printf("get message for %s\n", id);

  if (handler_size != 0)
  {
    // someone already ongoing
    return;
  }

  int i;
  for(i = 0; id[i] != '\0' && i < sizeof(handler) / 2; i++)
  {
    handler[i] = __swap_bytes((uint16_t)id[i]);
  }

  handler_size = i;
  state = STATE_INIT;
  mas_getmessage_internal(1);
}




static void msg_gettype(char* buf)
{
  content[0] = 0;
  content_size = sizeof(content) - 1;

  // identify the type
  char* start = strstr(buf, BEGINTYPE);
  char* end = strstr(start, END);

  if (start == NULL || end == NULL)
  {
    state = STATE_ERROR;
    printf("didn't find end of type\n");
    return;
  }

  if (strncmp(start + sizeof(BEGINTYPE) - 1, "SMS", 3) == 0)
  {
    content_type = ICON_MSG;
  }
  else
    content_type = 0;

//  state = STATE_GETTYPE;

// assume this must be in same package
  start = strstr(end, BEGINFN);
  end = strstr(start, END);

  if (start == NULL || end == NULL)
  {
    state = STATE_ERROR;
    printf("didn't find end of FN\n");
    return;
  }

  start += sizeof(BEGINFN) - 1;

  size_t size;
  if (end - start < sizeof(title) - 1)
    size = end - start;
  else
    size = sizeof(title) - 1;


  memcpy(title, start, size);
  title[size] = 0;

  state = STATE_GETFROM;
}

static void msg_getcontent(char *buf, int len)
{
  char* start;
  char* stop;
  if (state == STATE_GETCONTENTSTART)
  {
    start = buf;
    stop = strstr(start, "END:MSG");
    if (stop != NULL)
    {
      state = STATE_GETCONTENTEND;
      *stop = '\0';
    }
  }
  else
  {
    start = strstr(buf, BEGINMSG);
    int length;
    if (start == NULL)
      return;
    state = STATE_GETCONTENTSTART;

    stop = strstr(start, "END:MSG");
    if (stop != NULL)
    {
      state = STATE_GETCONTENTEND;
      *stop = '\0';
    }
    else
    {
      stop = buf + len;
    }
    start += sizeof(BEGINMSG) - 1;
  }

  size_t size;
  if (stop - start < content_size)
    size = stop - start;
  else
    size = content_size;

  strncat(content, start, size);
  content_size -= size;

  if (content_size < 0)
  {
    state = STATE_GETCONTENTEND;
  }

  //printf("%d %s\n", content_type, title);
}


static void mas_callback(int code, uint8_t* header, uint16_t length)
{
  //printf("Mas Callback with code %d\n", code);
 
  if (code == OBEX_RESPCODE_CONNECTED)
  {
    uint8_t *ptr = obex_create_request(&mas_obex, OBEX_OP_PUT + OBEX_OP_FINAL, mas_buf);
    uint8_t Fillerbyte = 0x30;
    
    ptr = obex_header_add_uint32(ptr, OBEX_HEADER_CONNID, mas_obex.state->connection);
    ptr = obex_header_add_bytes(ptr, OBEX_HEADER_TYPE, (uint8_t*)type_notify, sizeof(type_notify));
    ptr = obex_header_add_bytes(ptr, OBEX_HEADER_APPPARMS, appparams_notify, sizeof(appparams_notify));
    ptr = obex_header_add_bytes(ptr, OBEX_HEADER_ENDBODY, &Fillerbyte, 1);
      
    obex_send_request(&mas_obex, mas_buf, ptr - mas_buf);
    return;
  }

  if (handler_size == 0)
  {
    // we just set the enable notification
    // nothing

    return;
  }

  if ((code & 0x70) == OBEX_RESPCODE_CONTINUE ||
       (code & 0x70) == OBEX_RESPCODE_OK)
  {
      // parse headers
    while(header != NULL && length > 0)
    {
      switch(*header)
      {
        case OBEX_HEADER_BODY:
        case OBEX_HEADER_ENDBODY:
        {
          uint16_t len = READ_NET_16(header, 1);
          //printf("====>%s", header + 3);
          header[len] = '\0';
          switch (state)
          {
            case STATE_INIT:
              msg_gettype(header + 3);
              if (state != STATE_GETFROM)
                break;
              // fallthrough
            case STATE_GETFROM:
            case STATE_GETCONTENTSTART:
              msg_getcontent(header+3, len - 3);
              break;

            default:
              break;
          }
          break;
        }

        case OBEX_HEADER_APPPARMS:
        case OBEX_HEADER_TYPE:
        {
          uint16_t len = READ_NET_16((uint8_t*)header, 1);
          //printf("type %x: ", *header);
          //hexdump((uint8_t*)header, len);
          break;
        }
        case OBEX_HEADER_CONNID:
        {
          //printf("connid ");
          //hexdump((uint8_t*)header + 1, 4);   
          break;
        }
      }
      header = obex_header_get_next(header, &length);
    }
  
   if ((code == (OBEX_RESPCODE_OK | OBEX_OP_FINAL)) || (state == STATE_GETCONTENTEND))
    {
      // done with the message
      handler_size = 0;

      if (state == STATE_GETCONTENTEND)
      {
        window_notify(title, content, 0, content_type);
      }
    }
    else if (code == (OBEX_RESPCODE_CONTINUE | OBEX_OP_FINAL))
    {
      // send next request
      mas_getmessage_internal(0);
    }
  }
}

static void mas_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
  switch(packet_type)
  {
  case RFCOMM_DATA_PACKET:
//    printf("MAS received: ");
//    hexdump(packet, size);
    obex_handle(&mas_obex, packet, size);
    rfcomm_grant_credits(rfcomm_channel_id, 1); // get the next packet
    break;
  case DAEMON_EVENT_PACKET:
    switch(packet[0])    
    {
      case DAEMON_EVENT_NEW_RFCOMM_CREDITS:
      case DAEMON_EVENT_HCI_PACKET_SENT:
      case RFCOMM_EVENT_CREDITS:
        {
          mas_try_respond(rfcomm_channel_id);
          break;
        }
    }
    break;
  case HCI_EVENT_PACKET:
    {
      switch(packet[0])
      {
      case RFCOMM_EVENT_INCOMING_CONNECTION:
        break;
      case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
        {
          if (packet[2])
          {
            rfcomm_channel_id = 0;
          }
          else
          {
            rfcomm_channel_id = READ_BT_16(packet, 12);
            printf("MAS connection is created channel = %d.\n", rfcomm_channel_id);
            obex_connect_request(&mas_obex, MAS_TARGET, sizeof(MAS_TARGET));
          }
          break;
        }
      case DAEMON_EVENT_NEW_RFCOMM_CREDITS:
      case DAEMON_EVENT_HCI_PACKET_SENT:
      case RFCOMM_EVENT_CREDITS:
        {
          mas_try_respond(rfcomm_channel_id);
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


int mas_open(const bd_addr_t *remote_addr, uint8_t port)
{
  mas_response_size = 0;
  obex_init(&mas_obex);
  rfcomm_create_channel_internal(NULL, mas_packet_handler, (bd_addr_t*)remote_addr, port);

  return 0;
}