 /*
 * Copyright (C) 2011-2012 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at contact@bluekitchen-gmbh.com
 *
 */

//
// ATT Server Globals
//
//#define ENABLE_LOG_INFO
//#define ENABLE_LOG_DEBUG

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack-config.h"

#include <btstack/run_loop.h>
#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#include "att.h"
#include "att_server.h"

#include "l2cap.h"

#include "sm.h"
#include "att.h"
#include "att_server.h"
#include "att_client.h"
#include "gap_le.h"
#include "central_device_db.h"

#define MTU 100

static void att_run(void);

static uint16_t att_build_request(att_connection_t *, uint8_t *buffer);
static void att_handle_response(att_connection_t *, uint8_t*, uint16_t);
static void att_handle_notification(att_connection_t *conn, uint8_t* buffer, uint16_t length);
static void att_clear_request();


typedef enum {
    ATT_SERVER_IDLE,
    ATT_SERVER_REQUEST_RECEIVED,
    ATT_SERVER_W4_SIGNED_WRITE_VALIDATION,
    ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED,

    ATT_SERVER_W4_RESPONSE,
    ATT_SERVER_RESPONSE_RECEIVED,
} att_server_state_t;

static att_connection_t att_connection;
static att_server_state_t att_server_state;
static uint16_t request_type;

static uint8_t   att_client_addr_type;
static bd_addr_t att_client_address;
static uint16_t  att_request_handle = 0;
static uint16_t  att_request_size   = 0;
static uint8_t   att_request_buffer[MTU + 20];

static int       att_ir_central_device_db_index = -1;
static int       att_ir_lookup_active = 0;

static int       att_handle_value_indication_handle = 0;    
static timer_source_t att_handle_value_indication_timer;

static btstack_packet_handler_t att_client_packet_handler = NULL;

static const uint16_t connection_params[] =
{
//  min, max, latency, timeout,
    20, 600, 0, 200,// normal situation
    24, 45, 3, 500,// idle situation
    16, 32, 0, 10,// fast situation
};
static uint8_t target_connection_params = 0xff;

static void att_handle_value_indication_notify_client(uint8_t status, uint16_t client_handle, uint16_t attribute_handle){
    uint8_t event[7];
    int pos = 0;
    event[pos++] = ATT_HANDLE_VALUE_INDICATION_COMPLETE;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = status;
    bt_store_16(event, pos, client_handle);
    pos += 2;
    bt_store_16(event, pos, attribute_handle);
    pos += 2;
    (*att_client_packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}

static void att_handle_value_indication_timeout(timer_source_t *ts){
    uint16_t att_handle = att_handle_value_indication_handle;
    att_handle_value_indication_notify_client(ATT_HANDLE_VALUE_INDICATION_TIMEOUT, att_request_handle, att_handle);
}

static void att_event_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){    
    //printf("type=%x ev:%x\n", packet_type, packet[0]);

    switch (packet_type)
    {
    case HCI_EVENT_PACKET:
        switch (packet[0])
        {
        case DAEMON_EVENT_HCI_PACKET_SENT:
            att_run();
            break;
            
        case HCI_EVENT_LE_META:
            switch (packet[2])
            {
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                	// store connection info 
                    att_request_handle = READ_BT_16(packet, 4);
                	att_client_addr_type = packet[7];
                    bt_flip_addr(att_client_address, &packet[8]);
                    // reset connection properties
                    att_connection.mtu = MTU;
                    att_connection.encryption_key_size = 0;
                    att_connection.authenticated = 0;
                	att_connection.authorized = 0;
                    break;

                default:
                    break;
            }
            break;

        case HCI_EVENT_ENCRYPTION_CHANGE: 
        	// check handle
        	if (att_request_handle != READ_BT_16(packet, 3)) break;
        	att_connection.encryption_key_size = sm_encryption_key_size(att_client_addr_type, att_client_address);
        	att_connection.authenticated = sm_authenticated(att_client_addr_type, att_client_address);
        	break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            {
                uint16_t handle = READ_BT_16(packet, 3);
                if (handle <= 1024)
                    break;
            
            // restart advertising if we have been connected before
            // -> avoid sending advertise enable a second time before command complete was received 
            att_server_state = ATT_SERVER_IDLE;
            att_request_handle = 0;
            att_handle_value_indication_handle = 0; // reset error state
            att_clear_transaction_queue();
            break;
            }

        case SM_IDENTITY_RESOLVING_STARTED:
            printf("SM_IDENTITY_RESOLVING_STARTED\n");
            att_ir_lookup_active = 1;
            break;
        case SM_IDENTITY_RESOLVING_SUCCEEDED:
            att_ir_lookup_active = 0;
            att_ir_central_device_db_index = ((sm_event_t*) packet)->central_device_db_index;
            printf("SM_IDENTITY_RESOLVING_SUCCEEDED id %u\n", att_ir_central_device_db_index);
            break;
        case SM_IDENTITY_RESOLVING_FAILED:
            printf("SM_IDENTITY_RESOLVING_FAILED\n");
            att_ir_lookup_active = 0;
            att_ir_central_device_db_index = -1;
            printf("SM_IDENTITY_RESOLVING_FAILED--\n");
            break;

        case SM_AUTHORIZATION_RESULT:
            {
                sm_event_t * event = (sm_event_t *) packet;
                if (event->addr_type != att_client_addr_type) break;
                if (memcmp(event->address, att_client_address, 6) != 0) break;
                att_connection.authorized = event->authorization_result;
            }
            break;

        case SM_BONDING_FINISHED:
            printf("pairing finished\n");
            att_server_send_gatt_services_request(1);
            break;
        }

        default:
            break;
    }
    
    if (att_client_packet_handler){
        att_client_packet_handler(packet_type, channel, packet, size);
    }
}

static void att_signed_write_handle_cmac_result(uint8_t hash[8]){
    
    if (att_server_state != ATT_SERVER_W4_SIGNED_WRITE_VALIDATION) return;

    if (memcmp(hash, &att_request_buffer[att_request_size-8], 8)){
        printf("ATT Signed Write, invalid signature\n");
        att_server_state = ATT_SERVER_IDLE;
        return;
    }

    // update sequence number
    uint32_t counter_packet = READ_BT_32(att_request_buffer, att_request_size-12);
    central_device_db_counter_set(att_ir_central_device_db_index, counter_packet+1);
    att_server_state = ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED;
    att_run();
}

static void att_run(void){
    //printf("ATT %d\n", att_server_state);
    switch (att_server_state){
        case ATT_SERVER_W4_RESPONSE:
        case ATT_SERVER_W4_SIGNED_WRITE_VALIDATION:
            return;
        case ATT_SERVER_REQUEST_RECEIVED:
            if (att_request_buffer[0] == ATT_SIGNED_WRITE_COMAND){
                printf("ATT Signed Write!\n");
                if (!sm_cmac_ready()) {
                    printf("ATT Signed Write, sm_cmac engine not ready. Abort\n");
                    att_server_state = ATT_SERVER_IDLE;
                     return;
                }  
                if (att_request_size < (3 + 12)) {
                    printf("ATT Signed Write, request to short. Abort.\n");
                    att_server_state = ATT_SERVER_IDLE;
                    return;
                }
                if (att_ir_lookup_active){
                    return;
                }
                if (att_ir_central_device_db_index < 0){
                    printf("ATT Signed Write, CSRK not available\n");
                    att_server_state = ATT_SERVER_IDLE;
                    return;
                }

                // check counter
                uint32_t counter_packet = READ_BT_32(att_request_buffer, att_request_size-12);
                uint32_t counter_db     = central_device_db_counter_get(att_ir_central_device_db_index);
                printf("ATT Signed Write, DB counter %lu, packet counter %lu\n", counter_db, counter_packet);
                if (counter_packet < counter_db){
                    printf("ATT Signed Write, db reports higher counter, abort\n");
                    att_server_state = ATT_SERVER_IDLE;
                    return;
                }

                // signature is { sequence counter, secure hash }
                sm_key_t csrk;
                central_device_db_csrk(att_ir_central_device_db_index, csrk);
                att_server_state = ATT_SERVER_W4_SIGNED_WRITE_VALIDATION;
                printf("Orig Signature: ");
                hexdump( &att_request_buffer[att_request_size-8], 8);
                sm_cmac_start(csrk, att_request_size - 8, att_request_buffer, att_signed_write_handle_cmac_result);
                return;
            } 
            // NOTE: fall through for regular commands

        case ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED:
        {
            if (!hci_can_send_packet_now(HCI_LE_DATA_PACKET)) return;

            uint8_t  att_response_buffer[MTU + 20];
            uint16_t att_response_size;

            att_response_size = att_handle_request(&att_connection, att_request_buffer, att_request_size, att_response_buffer);

            // intercept "insufficient authorization" for authenticated connections to allow for user authorization
            if (att_response_buffer[0] == ATT_ERROR_RESPONSE
             && att_response_buffer[4] == ATT_ERROR_INSUFFICIENT_AUTHORIZATION
             && att_connection.authenticated){
            	switch (sm_authorization_state(att_client_addr_type, att_client_address)){
            		case AUTHORIZATION_UNKNOWN:
		             	sm_request_authorization(att_client_addr_type, att_client_address);
	    		        return;
	    		    case AUTHORIZATION_PENDING:
	    		    	return;
	    		    default:
	    		    	break;
            	}
            }

            if (att_response_size == 0)
            {
                log_error("response size is zero.\n");
                att_server_state = ATT_SERVER_IDLE;
                return;
            }

            if (!l2cap_send_connectionless(att_request_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, att_response_buffer, att_response_size))
            {
                att_server_state = ATT_SERVER_IDLE;
            }
            break;
        }
        case ATT_SERVER_RESPONSE_RECEIVED:
            {
                att_server_state = ATT_SERVER_IDLE;
                att_handle_response(&att_connection, att_request_buffer, att_request_size);
            }
            /* FALL THROUGH */
        case ATT_SERVER_IDLE:
            // check if we have anything to send
            if (!hci_can_send_packet_now(HCI_LE_DATA_PACKET)) return;

            uint8_t  buffer[MTU + 20];
            uint16_t size = att_build_request(&att_connection, buffer);

            if (size == 0)
            {
                if ((target_connection_params != 0xff) && (att_request_handle > 1024))
                {
                    printf("try to set connection parameters\n");
                    int ret;
                    ret = l2cap_le_request_connection_parameter_update(att_request_handle, 
                            connection_params[target_connection_params * 4],
                            connection_params[target_connection_params * 4+1],
                            connection_params[target_connection_params * 4+2],
                            connection_params[target_connection_params * 4+3]
                            );
                    if (!ret)
                    {
                        target_connection_params = 0xff;
                    }
                    else
                    {
                        printf("with error %x\n", ret);
                    }
                }

                return; // no request need sent
            }

            hexdump(buffer, size);

            if (!l2cap_send_connectionless(att_request_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, buffer, size))
            {
                att_server_state = ATT_SERVER_W4_RESPONSE;
                att_clear_request();
            }
            else
            {
                printf("fail to send ble packet.\n");
            }

            break;

    }
}

static void att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
    //printf("packet data(%d): ", packet_type);
    //hexdump(packet, size);

    if (packet_type != ATT_DATA_PACKET) return;

    // handle value indication confirms
    if (packet[0] == ATT_HANDLE_VALUE_CONFIRMATION && att_handle_value_indication_handle){
        run_loop_remove_timer(&att_handle_value_indication_timer);
        uint16_t att_handle = att_handle_value_indication_handle;
        att_handle_value_indication_handle = 0;    
        att_handle_value_indication_notify_client(0, att_request_handle, att_handle);
        return;
    }

    // check size
    if (size > sizeof(att_request_buffer)) 
    {
        log_error("packet is larger than mtu.\n");
        return;
    }

    if (packet[0] == ATT_HANDLE_VALUE_NOTIFICATION)
    {
        att_handle_notification(&att_connection, packet, size);
        return;
    }
    else if (packet[0] & 0x01 == 1)
    {
        att_server_state = ATT_SERVER_RESPONSE_RECEIVED;
    }
    else
    {
        att_server_state = ATT_SERVER_REQUEST_RECEIVED;
    }

    // store packet
    att_request_size = size;
    memcpy(att_request_buffer, packet, size);

    att_run();
}

void att_server_init(uint8_t const * db, att_read_callback_t read_callback, att_write_callback_t write_callback){

    sm_register_packet_handler(att_event_packet_handler);

    l2cap_register_fixed_channel(att_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);

    att_server_state = ATT_SERVER_IDLE;
    att_set_db(db);
    att_set_read_callback(read_callback);
    att_set_write_callback(write_callback);

    request_type = 0;
}

void att_server_register_packet_handler(btstack_packet_handler_t handler){
    att_client_packet_handler = handler;    
}

int  att_server_can_send(){
	if (att_request_handle == 0) return 0;
	return hci_can_send_packet_now(HCI_LE_DATA_PACKET);
}

int att_server_notify(uint16_t handle, uint8_t *value, uint16_t value_len){
    uint8_t *packet_buffer = malloc(att_connection.mtu);
    if (packet_buffer == NULL)
        return BTSTACK_MEMORY_ALLOC_FAILED;
    uint16_t size = att_prepare_handle_value_notification(&att_connection, handle, value, value_len, packet_buffer);
    printf("notify handle:%d data: ", handle);
    hexdump(value, value_len);
	int ret = l2cap_send_connectionless(att_request_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, packet_buffer, size);

    free(packet_buffer);
    return ret;
}

int att_server_indicate(uint16_t handle, uint8_t *value, uint16_t value_len){
    if (att_handle_value_indication_handle) return ATT_HANDLE_VALUE_INDICATION_IN_PORGRESS;
    if (!hci_can_send_packet_now(HCI_LE_DATA_PACKET)) return BTSTACK_ACL_BUFFERS_FULL;

    // track indication
    att_handle_value_indication_handle = handle;
    run_loop_set_timer_handler(&att_handle_value_indication_timer, att_handle_value_indication_timeout);
    run_loop_set_timer(&att_handle_value_indication_timer, ATT_TRANSACTION_TIMEOUT_MS);
    run_loop_add_timer(&att_handle_value_indication_timer);

    uint8_t *packet_buffer = malloc(att_connection.mtu);
    if (packet_buffer == NULL)
        return BTSTACK_MEMORY_ALLOC_FAILED;
    uint16_t size = att_prepare_handle_value_indication(&att_connection, handle, value, value_len, packet_buffer);
    int ret = l2cap_send_connectionless(att_request_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, packet_buffer, size);

    free(packet_buffer);
    return ret;
}


/**
 * TODO: move this to seperate file
*/

static union request_info
{
    struct
    {
        uint16_t attribute_group_type;
        uint16_t start_handle;
        uint16_t end_handle;
        uint8_t  value[16];
    }_find_by_type_value;
    struct
    {
        uint16_t attribute_group_type;
        uint16_t start_handle;
        uint16_t end_handle;
    }_read_by_group_type;
    struct
    {
        uint16_t attribute_handle;
        uint8_t *data;
        uint8_t length;
    }_write_attribute;
}request;

void att_server_query_service(const uint8_t *uuid128)
{
    request._find_by_type_value.attribute_group_type = GATT_PRIMARY_SERVICE_UUID;
    request._find_by_type_value.start_handle = 1;
    request._find_by_type_value.end_handle = 0xffff;
    swap128(request._find_by_type_value.value, (uint8_t *)uuid128);
    
    request_type = ATT_FIND_BY_TYPE_VALUE_REQUEST;

    att_run();
}

void att_server_send_gatt_services_request(uint16_t start_handle)
{
    request._read_by_group_type.attribute_group_type = GATT_PRIMARY_SERVICE_UUID;
    request._read_by_group_type.start_handle = start_handle;
    request._read_by_group_type.end_handle = 0xffff;

    request_type = ATT_READ_BY_GROUP_TYPE_REQUEST;

    att_run();
}

void att_server_read_gatt_service(uint16_t start_handle, uint16_t end_handle)
{
    request._read_by_group_type.attribute_group_type = GATT_CHARACTERISTICS_UUID;
    request._read_by_group_type.start_handle = start_handle;
    request._read_by_group_type.end_handle = end_handle;

    request_type = ATT_READ_BY_TYPE_REQUEST;

    att_run();
}

void att_server_subscribe(uint16_t handle)
{
    request._write_attribute.attribute_handle = handle;
    request._write_attribute.data = malloc(2);
    if (!request._write_attribute.data)
    {
        log_error("Cannot allocate memory.\n");
        return;
    }
    request._write_attribute.length = 2;

    request._write_attribute.data[0] = 0x01;
    request._write_attribute.data[1] = 0x00;

    request_type = ATT_WRITE_REQUEST;

    att_run();
}

void att_server_write(uint16_t handle, uint8_t *buffer, uint16_t length)
{
    request._write_attribute.attribute_handle = handle;
    request._write_attribute.data = malloc(length);
    if (!request._write_attribute.data)
    {
        log_error("Cannot allocate memory.\n");
        return;
    }
    request._write_attribute.length = length;

    memcpy(request._write_attribute.data, buffer, length);

    request_type = ATT_WRITE_REQUEST;

    att_run();
}

static uint16_t att_build_request(att_connection_t *connection, uint8_t *buffer)
{
    log_debug("ATT: try to send request\n");

    switch(request_type)
    {
        case ATT_FIND_BY_TYPE_VALUE_REQUEST:
            log_debug("ATT: try to send ATT_FIND_BY_TYPE_VALUE_REQUEST request\n");
            return att_find_by_type_value_request(buffer,
                request._find_by_type_value.attribute_group_type,
                request._find_by_type_value.start_handle, 
                request._find_by_type_value.end_handle,
                request._find_by_type_value.value,
                16);
        case ATT_READ_BY_GROUP_TYPE_REQUEST:
            log_debug("ATT: try to send ATT_READ_BY_GROUP_TYPE_REQUEST request\n");
            return att_read_by_group_request(buffer,
                request._read_by_group_type.attribute_group_type,
                request._read_by_group_type.start_handle,
                request._read_by_group_type.end_handle
                );
        case ATT_READ_BY_TYPE_REQUEST:
            log_debug("ATT: try to send ATT_READ_BY_TYPE_REQUEST request\n");
             return att_read_by_type_request(buffer,
                request._read_by_group_type.attribute_group_type,
                request._read_by_group_type.start_handle,
                request._read_by_group_type.end_handle
                );
        case ATT_WRITE_REQUEST:
            log_debug("ATT: try to send ATT_WRITE_REQUEST request\n");
            return att_write_request(buffer,
                request._write_attribute.attribute_handle,
                request._write_attribute.data,
                request._write_attribute.length);
        case ATT_WRITE_COMMAND:
            log_debug("ATT: try to send ATT_WRITE_COMMAND request\n");
            return att_write_command(buffer,
                request._write_attribute.attribute_handle,
                request._write_attribute.data,
                request._write_attribute.length);
        
        default:
            return 0;
    }
}

static void att_clear_request()
{
    if (request_type == ATT_WRITE_REQUEST)
    {
        if (request._write_attribute.data)
            free(request._write_attribute.data);
        request._write_attribute.data = NULL;
    }
    request_type = 0;
}

static void att_handle_response(att_connection_t *att_connection, uint8_t* buffer, uint16_t length)
{
    uint16_t lasthandle;
    switch(buffer[0])
    {
        case ATT_READ_BY_GROUP_TYPE_RESPONSE:
            // check service
            lasthandle = report_gatt_services(att_connection, buffer, length);

            if (lasthandle != 0xff)
            {
                request._read_by_group_type.start_handle = lasthandle + 1;

                request_type = ATT_READ_BY_GROUP_TYPE_REQUEST;
            }
            break;
        case ATT_READ_BY_TYPE_RESPONSE:
            lasthandle = report_service_characters(att_connection, buffer, length);
            if (lasthandle != 0xff)
            {
                request._read_by_group_type.start_handle = lasthandle + 1;

                request_type = ATT_READ_BY_TYPE_REQUEST;
            }
            break;
        case ATT_WRITE_RESPONSE:
            report_write_done(att_connection, request._write_attribute.attribute_handle);
            break;
        case ATT_ERROR_RESPONSE:
            // done the last step
            hexdump(buffer, length);
            if ((buffer[4] == ATT_ERROR_INSUFFICIENT_AUTHORIZATION
                    || buffer[4] == ATT_ERROR_INSUFFICIENT_AUTHENTICATION)
                && att_connection->authenticated){
                switch (sm_authorization_state(att_client_addr_type, att_client_address)){
                    case AUTHORIZATION_UNKNOWN:
                        sm_request_authorization(att_client_addr_type, att_client_address);
                        return;
                    case AUTHORIZATION_PENDING:
                        return;
                    default:
                        break;
                }
                att_run();
            }
            break;
    }
}

static void att_handle_notification(att_connection_t *conn, uint8_t* buffer, uint16_t length)
{
    //printf("notification data : ");
    //hexdump(buffer, length);

    uint16_t handler = READ_BT_16(buffer, 1);

    att_client_notify(handler, buffer + 3, length - 3);
}

void att_enter_mode(int mode)
{
    printf("set mode: %d\n", mode);
    target_connection_params = mode;   
}
