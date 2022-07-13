
#include "ancs.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack-config.h"

#include <btstack/run_loop.h>
#include <btstack/utils.h>
#include "debug.h"
#include "btstack/sdp_util.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#include "l2cap.h"

#include "sm.h"
#include "att.h"
#include "att_server.h"
#include "att_client.h"
#include "gap_le.h"
#include "central_device_db.h"

//XX: we should not use window.h here
#include "window.h"

static const uint8_t ancsuuid[] = {
    0x79, 0x05, 0xF4, 0x31, 0xB5, 0xCE, 0x4E, 0x99, 0xA4, 0x0F, 0x4B, 0x1E, 0x12, 0x2D, 0x00, 0xD0
};
static const uint8_t notificationuuid[] = {
    0x9F, 0xBF, 0x12, 0x0D, 0x63, 0x01, 0x42, 0xD9, 0x8C, 0x58, 0x25, 0xE6, 0x99, 0xA2, 0x1D, 0xBD
};
static const uint8_t controluuid[] = {
    0x69, 0xD1, 0xD8, 0xF3, 0x45, 0xE1, 0x49, 0xA8, 0x98, 0x21, 0x9B, 0xBD, 0xFD, 0xAA, 0xD9, 0xD9
};
static const uint8_t datauuid[] = {
    0x22, 0xEA, 0xC6, 0xE9, 0x24, 0xD6, 0x4B, 0xB5, 0xBE, 0x44, 0xB3, 0x6A, 0xCE, 0x7C, 0x7B, 0xFB
};
static const uint8_t* attribute_uuids[] =
{
    notificationuuid, controluuid, datauuid
};
static uint16_t start_group_handle, end_group_handle;

#define NOTIFICATION 0
#define CONTROLPOINT 1
#define DATASOURCE   2
static uint16_t attribute_handles[3];

static uint16_t write_handle;

uint16_t report_gatt_services(att_connection_t *conn, uint8_t * packet,  uint16_t size){
    // log_info(" report_gatt_services for %02X\n", peripheral->handle);
    uint8_t attr_length = packet[1];
    uint8_t uuid_length = attr_length - 4;
    uint8_t uuid128[16];

    int i;
    for (i = 2; i < size; i += attr_length){
        start_group_handle = READ_BT_16(packet,i);
        end_group_handle = READ_BT_16(packet,i+2);
        
        if (uuid_length == 2){
            uint16_t uuid16 = READ_BT_16(packet, i+4);
            sdp_normalize_uuid((uint8_t*) &uuid128, uuid16);
        } else {
            swap128(&packet[i+4], uuid128);
        }
        printUUID128(uuid128);
        log_info("\nstart_group_handle: %d, end_group_handle: %d\n", start_group_handle, end_group_handle);

        if (memcmp(ancsuuid, uuid128, 16) == 0)
        {
            for(int i = 0 ; i < 3; i++)
                attribute_handles[i] = 0xffff;
            att_server_read_gatt_service(start_group_handle, end_group_handle);
            return 0xff;
        }
    }

    return end_group_handle;
}

uint16_t report_service_characters(att_connection_t *conn, uint8_t * packet,  uint16_t size){
    uint8_t attr_length = packet[1];
    uint8_t uuid_length = attr_length - 5;
    uint16_t value_handle;
    int i;        
    for (i = 2; i < size; i += attr_length){
#ifdef ENABLE_LOG_INFO
        uint16_t start_handle = READ_BT_16(packet, i);
        uint8_t  properties = packet[i+2];
#endif
        value_handle = READ_BT_16(packet, i+3);

        uint8_t uuid128[16];
        if (uuid_length == 2){
            uint16_t uuid16 = READ_BT_16(packet, i+5);
            sdp_normalize_uuid((uint8_t*) &uuid128, uuid16);
        } else {
            swap128(&packet[i+5], uuid128);
        }

        printUUID128(uuid128);
        log_info("\nproperties: %x start_handle:%d value_handle: %d\n", properties, start_handle, value_handle);

        for(int i = 0 ;i < 3; i++)
        {
            if (memcmp(uuid128, attribute_uuids[i], 16) == 0)
            {
                attribute_handles[i] = value_handle;
            }
        }

    }

    if (attribute_handles[0] != -1 && 
        attribute_handles[1] != -1 &&
        attribute_handles[2] != -1)
    {
        // subscribe event
        window_notify_ancs_init();
        log_info("sub to %d\n", attribute_handles[NOTIFICATION]);
        write_handle = attribute_handles[NOTIFICATION] + 1;
        att_server_subscribe(attribute_handles[NOTIFICATION] + 1); // write to CCC
        return 0xffff;
    }
    else
        return value_handle;
}

void report_write_done(att_connection_t *conn, uint16_t handle)
{
    log_info("report_write_done: %d\n", handle);
    if (handle == attribute_handles[NOTIFICATION] + 1)
    {
        write_handle = attribute_handles[DATASOURCE] + 1;
        att_server_subscribe(attribute_handles[DATASOURCE] + 1); // write to CCC
    }
    else if (handle == attribute_handles[DATASOURCE] + 1)
    {
        printf("subscribe to ANCS finished.\n");
        att_enter_mode(MODE_SLEEP);
    }
}

#define MAX_TITLE 43
#define MAX_MESSAGE 250
static enum 
{
    STATE_NONE,
    STATE_UID,
    STATE_ATTRIBUTEID,
    STATE_ATTRIBUTELEN,
    STATE_ATTRIBUTE,
    STATE_DONE
}parse_state = STATE_NONE;
static uint8_t attributeid;
static uint16_t attrleftlen, len;
static char* bufptr;
static char appidbuf[32];
static char titlebuf[MAX_TITLE + 1];
static char subtitlebuf[MAX_TITLE + 1];
static char msgbuf[MAX_MESSAGE + 1];
static char datebuf[16];

void att_client_notify(uint16_t handle, uint8_t *data, uint16_t length)
{
    if (handle == attribute_handles[NOTIFICATION])
    {
        uint32_t uid =  READ_BT_32(data, 4);
        //uint32_t combine = READ_BT_32(data, 4);
        log_info("id: %d flags:%d catery:%d count: %d UID:%ld\n",
            data[0], data[1], data[2], data[3],
            uid
            );

        if (data[2] == CategoryIDIncomingCall)
        {
            // need convert the title to CLIP command

        }
        else
        {
            window_notify_ancs(data[0], uid, data[1], data[2]);
        }
    }
    else if (handle == attribute_handles[DATASOURCE])
    {
        log_info("data received\n");
        // start notification
        uint16_t l;
        int index = 0;
        while(index < length)
        {
            switch(parse_state)
            {
                case STATE_NONE:
                    log_info("Command: %d\t", data[index]);
                    index++;
                    parse_state = STATE_UID;
                    break;
                case STATE_UID:
                    log_info("uid: %ld\t", READ_BT_32(data, index));
                    index += 4;
                    parse_state = STATE_ATTRIBUTEID;
                    break;
                case STATE_ATTRIBUTEID:
                    attributeid = data[index];
                    log_info("\nattributeid: %d\t", attributeid);
                    switch(attributeid)
                    {
                        case NotificationAttributeIDAppIdentifier:
                        bufptr = appidbuf;
                        break;
                        case NotificationAttributeIDTitle:
                        bufptr = titlebuf;
                        break;
                        case NotificationAttributeIDSubtitle:
                        bufptr = subtitlebuf;
                        break;
                        case NotificationAttributeIDMessage:
                        bufptr = msgbuf;
                        break;
                        case NotificationAttributeIDDate:
                        bufptr = datebuf;
                        break;
                    }
                    index++;
                    parse_state = STATE_ATTRIBUTELEN;
                    break;
                case STATE_ATTRIBUTELEN:
                    len = attrleftlen = READ_BT_16(data, index);
                    log_info("len: %d\t", attrleftlen);
                    index+=2;
                    parse_state = STATE_ATTRIBUTE;
                    break;
                case STATE_ATTRIBUTE:
                    if (length - index > attrleftlen)
                        l = attrleftlen;
                    else
                        l = length - index;
                    for(int i = 0; i < l; i++)
                    {
                        //putchar(data[index + i]);
                        bufptr[i + len - attrleftlen] = data[index + i];
                    }
                    index += l;
                    attrleftlen -= l;
                    if (attrleftlen == 0)
                    {
                        bufptr[len] = '\0';
                        if (attributeid == NotificationAttributeIDDate)
                            parse_state = STATE_NONE;
                        else
                            parse_state = STATE_ATTRIBUTEID;
                    }
                    break;
            }
        }
        // parse the data
        char icon = -1;
#define ICON_FACEBOOK 's'
#define ICON_TWITTER  't'
#define ICON_MSG      'u' 

        if (strcmp("com.apple.MobileSMS", appidbuf) == 0)
        {
            icon = ICON_MSG;
        }
        else if (strcmp("XX", appidbuf) == 0)
        {
            icon = ICON_TWITTER;
        }
        else if (strcmp("XX", appidbuf) == 0)
        {
            icon = ICON_FACEBOOK;
        }

        if (parse_state == STATE_NONE)
        {
            window_notify_content(titlebuf, subtitlebuf, msgbuf, datebuf, 0, icon);
        }
    }
    else
    {
        log_info("handle: %d\n", handle);
    }
}


void att_handle_response(att_connection_t *att_connection, uint8_t* buffer, uint16_t length)
{
    uint16_t lasthandle;
    switch(buffer[0])
    {
        case ATT_READ_BY_GROUP_TYPE_RESPONSE:
            // check service
            lasthandle = report_gatt_services(att_connection, buffer, length);

            if (lasthandle != 0xff)
            {
                att_server_send_gatt_services_request(lasthandle + 1);
            }
            break;
        case ATT_READ_BY_TYPE_RESPONSE:
            lasthandle = report_service_characters(att_connection, buffer, length);
            if (lasthandle != 0xff)
            {
                att_server_read_gatt_service(lasthandle + 1, 0xffff);
            }
            break;
        case ATT_WRITE_RESPONSE:
            report_write_done(att_connection, write_handle);
            break;
        case ATT_ERROR_RESPONSE:
            log_error("Get error from ancs server\n");
            break;

        case ATT_HANDLE_VALUE_NOTIFICATION:
            lasthandle = READ_BT_16(buffer, 1);
            att_client_notify(lasthandle, buffer + 3, length - 3);
        break;
    }
}

void att_fetch_next(uint32_t uid, uint32_t combine)
{
    // based on catery to fetch infomation
    uint8_t buffer[] = {
            0, // command id
            0, 0, 0, 0, // uid
            NotificationAttributeIDAppIdentifier,          // appid
            NotificationAttributeIDTitle, MAX_TITLE, 0, // 16 bytes title
            NotificationAttributeIDSubtitle, MAX_TITLE, 0,
            NotificationAttributeIDMessage, MAX_MESSAGE, 0, // 64bytes message
            NotificationAttributeIDDate,
    };
    bt_store_32(buffer, 1, uid);
    att_server_write(attribute_handles[CONTROLPOINT], buffer, sizeof(buffer));
}