#include <string.h>

#include "l2cap.h"
#include "btstack-config.h"
#include "debug.h"

#include "avctp.h"

// bluetooth assgined number
#define AV_REMOTE_SVCLASS_ID		0x110e

/* Message types */
#define AVCTP_COMMAND		0
#define AVCTP_RESPONSE		1

/* Packet types */
#define AVCTP_PACKET_SINGLE	0
#define AVCTP_PACKET_START	1
#define AVCTP_PACKET_CONTINUE	2
#define AVCTP_PACKET_END	3

#define htons(x) __swap_bytes(x)

#pragma pack(1)
struct avctp_header {
  uint8_t ipid:1;
  uint8_t cr:1;
  uint8_t packet_type:2;
  uint8_t transaction:4;
  uint16_t pid;
};

struct avctp_header_start {
  uint8_t ipid:1;
  uint8_t cr:1;
  uint8_t packet_type:2;
  uint8_t transaction:4;
  uint8_t num_package;
  uint16_t pid;
};
#define AVCTP_HEADER_LENGTH 3

struct avc_header {
  uint8_t code:4;
  uint8_t _hdr0:4;
  uint8_t subunit_id:3;
  uint8_t subunit_type:5;
  uint8_t opcode;
};
#pragma pack()

static uint16_t l2cap_cid;
static void     *avctp_response_buffer;
static uint8_t  avctp_response_size;
static void     avctp_packet_handler(uint8_t packet_type, uint16_t channel,
                                     uint8_t *packet, uint16_t size);

#define MAX_PAYLOAD_SIZE 64
#define MAX_RESPONSE_SIZE 256
static uint8_t avctp_buf[AVCTP_HEADER_LENGTH + AVC_HEADER_LENGTH + MAX_PAYLOAD_SIZE];
static uint8_t *avctp_resp_buf;
static uint16_t resp_size;
static uint8_t id = 0;
static uint8_t need_send_release = 0;
static void (*packet_handler) (uint8_t code, uint8_t *packet, uint16_t size);
static uint16_t current_pid;

void avctp_init()
{
  packet_handler = NULL;
  avctp_resp_buf = NULL;
  l2cap_cid = 0;
  l2cap_register_service_internal(NULL, avctp_packet_handler, PSM_AVCTP, 0xffff, LEVEL_0);
}

void avctp_connect(bd_addr_t remote_addr)
{
  if (l2cap_cid) return;

  l2cap_create_channel_internal(&l2cap_cid, avctp_packet_handler,
                                remote_addr , PSM_AVCTP, 0xffff);
}

static void avctp_try_respond(void){
  if (!avctp_response_size ) return;
  if (!l2cap_cid) return;
  if (!l2cap_can_send_packet_now(l2cap_cid)) return;

  // update state before sending packet (avoid getting called when new l2cap credit gets emitted)
  uint16_t size = avctp_response_size;
  avctp_response_size = 0;
  int error = l2cap_send_internal(l2cap_cid, avctp_response_buffer, size);
  if (error)
  {
    avctp_response_size = size;
    printf("avctp_try_respond l2cap_send_internal error: %d\n", error);
    return;
  }

  if (need_send_release)
  {
    struct avctp_header *avctp = (void *) avctp_buf;
    uint8_t *operands = &avctp_buf[AVCTP_HEADER_LENGTH + AVC_HEADER_LENGTH];

    need_send_release = 0;
    avctp_response_size = size;

    avctp->transaction = id++;
    operands[0] |= 0x80;

    avctp_try_respond();
  }
}

void avctp_register_pid(uint16_t pid, void (*handler)(uint8_t code, uint8_t *packet, uint16_t size))
{
  packet_handler = handler;

  current_pid = __swap_bytes(pid);
}

int avctp_connected()
{
  return (l2cap_cid != 0);
}

void avctp_disconnect()
{
  if (avctp_connected())
  {
    l2cap_disconnect_internal(l2cap_cid, 0);
  }
  l2cap_cid = 0;
}

static void avctp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
  static uint16_t pid;
  switch (packet_type) {
  case L2CAP_DATA_PACKET:
    {
      struct avctp_header *avctph = (struct avctp_header*)packet;
      struct avc_header *avch = (struct avc_header*)(avctph+1);
      uint8_t *buf;

      if (avctph->packet_type == AVCTP_PACKET_SINGLE)
      {
        // single package
        pid = avctph->pid;
        buf = (uint8_t*)(avch+1);
        size -= sizeof(struct avctp_header) + sizeof(struct avc_header);
      }
      else if (avctph->packet_type == AVCTP_PACKET_START)
      {
        // start package
        struct avctp_header_start *avctph2 = (struct avctp_header_start*)packet;
        log_info("pid=%x package#=%d\n", avctph2->pid, avctph2->num_package);
        avch = (struct avc_header*)(avctph2 + 1);
        resp_size = size - sizeof(struct avctp_header_start) - sizeof(struct avc_header);
        if (avctp_resp_buf)
        {
          // safe guard
          free(avctp_resp_buf);
        }
        
        avctp_resp_buf = malloc(MAX_RESPONSE_SIZE);
        if (avctp_resp_buf)
          memcpy(avctp_resp_buf, (void*)(avch+1), resp_size);
        break;
      }
      else if (avctph->packet_type == AVCTP_PACKET_CONTINUE)
      {
        // continues package
        if (resp_size + size - 1 > MAX_RESPONSE_SIZE)
        {
          log_error("resp size is too small\n");
          return;
        }
        if (avctp_resp_buf)
          memcpy(&avctp_resp_buf[resp_size], (void*)(packet+1), size - 1);
        resp_size += size - 1;
        break;
      }
      else if (avctph->packet_type == AVCTP_PACKET_END)
      {
        if (resp_size + size - 1 > MAX_RESPONSE_SIZE)
        {
          log_error("resp size is too small\n");
          return;
        }

        // end packet
        if (avctp_resp_buf)
        {
          memcpy(&avctp_resp_buf[resp_size], (void*)(packet+1), size - 1);
          resp_size += size - 1;
          size = resp_size;
          buf = (uint8_t*)avctp_resp_buf;
          resp_size = 0;
        }
        else
        {
          break;
        }
      }
      
      if (current_pid == pid)
      {
        (*packet_handler)(avch->code, buf, size);
      }
      else
      {
        log_error("unknow pid %dn", pid);
      }
      
      if (avctph->packet_type == AVCTP_PACKET_END)
      {
        if (avctp_resp_buf)
          free(avctp_resp_buf);
      }
      break;
    }
  case HCI_EVENT_PACKET:
    switch (packet[0]) {
    case L2CAP_EVENT_INCOMING_CONNECTION:
      {
        if (l2cap_cid)
        {
          l2cap_decline_connection_internal(channel, 0x0d);
        }
        else
        {
          l2cap_cid = channel;
          avctp_response_size = 0;
          l2cap_accept_connection_internal(channel);
        }
        break;
      }
    case L2CAP_EVENT_CHANNEL_OPENED:
      if (packet[2]) {
        // open failed -> reset
        l2cap_cid = 0;
      }
      else
      {
        log_info("avctp connected\n");
        l2cap_cid = READ_BT_16(packet, 13);
        if (packet_handler)
        {
          packet_handler(0, NULL, 1); // connected
        }
      }
      break;
    case L2CAP_EVENT_CREDITS:
    case DAEMON_EVENT_HCI_PACKET_SENT:
      avctp_try_respond();
      break;
    case L2CAP_EVENT_CHANNEL_CLOSED:
      if (channel == l2cap_cid){
        // reset
        l2cap_cid = 0;
        packet_handler(0, NULL, 0); // disconnected
        log_info("avctp disconnected\n");
      }
      break;
    }
  }
}

int avctp_send_passthrough(uint8_t op)
{
  if (!l2cap_cid) return -1;

  struct avctp_header *avctp = (void *) avctp_buf;
  struct avc_header *avc = (void *) &avctp_buf[AVCTP_HEADER_LENGTH];
  uint8_t *operands = &avctp_buf[AVCTP_HEADER_LENGTH + AVC_HEADER_LENGTH];

  memset(avctp_buf, 0, AVCTP_HEADER_LENGTH + AVC_HEADER_LENGTH + 2);

  avctp->transaction = id++;
  avctp->packet_type = AVCTP_PACKET_SINGLE;
  avctp->cr = AVCTP_COMMAND;
  avctp->pid = htons(AV_REMOTE_SVCLASS_ID);

  avc->code = AVC_CTYPE_CONTROL;
  avc->subunit_type = AVC_SUBUNIT_PANEL;
  avc->opcode = AVC_OP_PASSTHROUGH;

  operands[0] = op & 0x7f;
  operands[1] = 0;

  need_send_release = 1;

  if (avctp_response_size != 0)
  {
    log_error("avctp override unsent data\n");
  }

  avctp_response_size = AVCTP_HEADER_LENGTH + AVC_HEADER_LENGTH + 2;
  avctp_response_buffer = avctp_buf;

  avctp_try_respond();
  return 0;
}

static int avctp_send(uint8_t transaction, uint8_t cr,
                      uint8_t code, uint8_t subunit, uint8_t opcode,
                      uint8_t *operands, uint8_t operand_count)
{
  struct avctp_header *avctp;
  struct avc_header *avc;
  uint8_t *pdu;

  if (!l2cap_cid) return -1;
  if (operand_count > MAX_PAYLOAD_SIZE)
  {
    log_error("too long operand payload.\n");
    return -1;
  }

  avctp = (void *) avctp_buf;
  avc = (void *) &avctp_buf[AVCTP_HEADER_LENGTH];
  pdu = (void *) &avctp_buf[AVCTP_HEADER_LENGTH + AVC_HEADER_LENGTH];

  avctp->transaction = transaction;
  avctp->packet_type = AVCTP_PACKET_SINGLE;
  avctp->cr = cr;
  avctp->pid = htons(AV_REMOTE_SVCLASS_ID);

  avc->code = code;
  avc->subunit_type = subunit;
  avc->opcode = opcode;

  memcpy(pdu, operands, operand_count);

  if (avctp_response_size != 0)
  {
    log_error("avctp override unsent data\n");
  }
  
  avctp_response_size = AVCTP_HEADER_LENGTH + AVC_HEADER_LENGTH + operand_count;
  avctp_response_buffer = avctp_buf;

  avctp_try_respond();

  return 0;
}

int avctp_send_vendordep(uint8_t code, uint8_t subunit,
                         uint8_t *operands, uint8_t operand_count)
{
  if (!l2cap_cid) return -1;

  return avctp_send(id++, AVCTP_COMMAND, code, subunit,
                    AVC_OP_VENDORDEP, operands, operand_count);

}

int avctp_send_vendordep_resp(uint8_t code, uint8_t subunit,
                              uint8_t *operands, uint8_t operand_count)
{
  if (!l2cap_cid) return -1;

  return avctp_send(id++, AVCTP_COMMAND, code, subunit,
                    AVC_OP_VENDORDEP, operands, operand_count);

}

