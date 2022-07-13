/*
 * Copyright (C) 2011-2013 by Matthias Ringwald
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
 * 4. This software may not be used in a commercial product
 *    without an explicit license granted by the copyright holder. 
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
 */

//*****************************************************************************
//
// BLE Peripheral Demo
//
//*****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

 #include "contiki.h"
 #include "window.h"

#include "btstack-config.h"

#include <btstack/run_loop.h>
#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#include "l2cap.h"

#include "sm.h"
#include "att.h"
#include "att_server.h"
#include "gap_le.h"
#include "central_device_db.h"

#include "ble_handler.h"

#define HEARTBEAT_PERIOD_MS 5000

// test profile
#include "profile.h"

///------
static int advertisements_enabled = 0;

typedef enum {
    DISABLE_ADVERTISEMENTS   = 1 << 0,
    SET_ADVERTISEMENT_PARAMS = 1 << 1,
    SET_ADVERTISEMENT_DATA   = 1 << 2,
    SET_SCAN_RESPONSE_DATA   = 1 << 3,
    ENABLE_ADVERTISEMENTS    = 1 << 4,
} todo_t;
static uint16_t todos = 0;

static void gap_run();

// write requests
static int att_write_callback(uint16_t handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size, signature_t * signature){
    if (transaction_mode == ATT_TRANSACTION_MODE_CANCEL)
        return 0;
    return att_handler(handle, offset, buffer, buffer_size, ATT_HANDLE_MODE_WRITE);
}

static uint16_t att_read_callback(uint16_t handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size) {
    return att_handler(handle, offset, buffer, buffer_size, ATT_HANDLE_MODE_READ);
}

/*
static const uint8_t ancsuuid[] = {
    0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4, 0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79
};
*/

static void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                
                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started
                    if (packet[2] == HCI_STATE_WORKING) {
                        log_info("SM Init completed\n");
                        todos = SET_ADVERTISEMENT_DATA | SET_SCAN_RESPONSE_DATA | ENABLE_ADVERTISEMENTS;
                        gap_run();
                    }
                    break;

                    case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            todos = DISABLE_ADVERTISEMENTS;

                            // request connection parameter update - test parameters
                            //l2cap_le_request_connection_parameter_update(READ_BT_16(packet, 4), 20, 1000, 100, 100);
                            //att_server_query_service(ancsuuid);
                            //sm_send_security_request();
                            gap_run();
                            break;

                        default:
                            break;
                    }
                    break;

                    case HCI_EVENT_DISCONNECTION_COMPLETE:
                        {
                            uint16_t handle = READ_BT_16(packet, 3);
                            if (handle < 1024)
                                break;
                            todos = ENABLE_ADVERTISEMENTS;
                            gap_run();
                            break;
                        }

                case SM_PASSKEY_DISPLAY_NUMBER: {
                    // display number
                    sm_event_t * event = (sm_event_t *) packet;
                    log_error("%06lu\n", event->passkey);
                    break;
                }

                case SM_PASSKEY_DISPLAY_CANCEL: 
                    log_error("GAP Bonding: Display cancel\n");
                    break;

                case SM_AUTHORIZATION_REQUEST: {
                    // auto-authorize connection if requested
                    sm_event_t * event = (sm_event_t *) packet;
                    sm_authorization_grant(event->addr_type, event->address);
                    break;
                }
                case ATT_HANDLE_VALUE_INDICATION_COMPLETE:
                    log_info("ATT_HANDLE_VALUE_INDICATION_COMPLETE status %u\n", packet[2]);
                    break;

                default:
                    break;
            }
    }

    gap_run();
}

//static const uint8_t sm_oob_data[] = "0123456789012345";
/*
static int get_oob_data_callback(uint8_t addres_type, bd_addr_t * addr, uint8_t * oob_data){
    memcpy(oob_data, sm_oob_data, 16);
    return 1;
}
*/

extern const uint8_t* system_getserial();

static void gap_run(){
    // make sure we can send one packet
    if (todos == 0 || !hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;

    printf("todo %x\n", todos);
    if (todos & DISABLE_ADVERTISEMENTS){
        todos &= ~DISABLE_ADVERTISEMENTS;
        advertisements_enabled = 0;
        printf("GAP_RUN: disable advertisements\n");
        hci_send_cmd(&hci_le_set_advertise_enable, 0);
        return;
    }

    if (todos & SET_ADVERTISEMENT_DATA){
        todos &= ~SET_ADVERTISEMENT_DATA;
        
        uint8_t adv_data[] = { 
            2, 0x1, 0x2,
            3, 0x03, 0xf0, 0xff,
            14, 0x09, 'M','e', 't', 'e', 'o', 'r', 'L', 'E', ' ', 'X','X','X','X','\0'
        };

        const char* addr = (const char*)system_getserial();
        sprintf((char*)&adv_data[18], "%02X%02X", addr[4], addr[5]);
        printf("GAP_RUN: set advertisement data\n");
        hexdump(adv_data, sizeof(adv_data));
        hci_send_cmd(&hci_le_set_advertising_data, sizeof(adv_data) - 1, adv_data);
        return;
    }    

    if (todos & SET_ADVERTISEMENT_PARAMS){
        todos &= ~SET_ADVERTISEMENT_PARAMS;
        uint8_t adv_type;
        if (advertisements_enabled)
            adv_type = 0x00;
        else
            adv_type = 0x02;
        bd_addr_t null_addr;
        memset(null_addr, 0, 6);
        uint16_t adv_int_min = 0x808;
        uint16_t adv_int_max = 0x808;
        switch (adv_type){
            case 0:
            case 2:
            case 3:
                hci_send_cmd(&hci_le_set_advertising_parameters, adv_int_min, adv_int_max, adv_type, 0, 0, &null_addr, 0x07, 0x00);
                break;
        }
        return;
    }    

    if (todos & SET_SCAN_RESPONSE_DATA){
        uint8_t scan_data[] = {
            2, 0x1, 0x2,
            2, 0x11, 3,
            17, 0x15, 0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4, 0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79
        };

        printf("GAP_RUN: set scan response data\n");
        todos &= ~SET_SCAN_RESPONSE_DATA;

        hci_send_cmd(&hci_le_set_scan_response_data, sizeof(scan_data), scan_data);
        return;
    }    

    if (todos & ENABLE_ADVERTISEMENTS){
        printf("GAP_RUN: enable advertisements\n");
        todos &= ~ENABLE_ADVERTISEMENTS;
        advertisements_enabled = 1;
        hci_send_cmd(&hci_le_set_advertise_enable, 1);
        return;
    }
}

void ble_advertise(uint8_t onoff)
{
    printf("enter %d %d---", onoff, advertisements_enabled);
    if (onoff && (advertisements_enabled == 0))
        todos |= ENABLE_ADVERTISEMENTS;

    if ((!onoff) && (advertisements_enabled == 1))
        todos |= DISABLE_ADVERTISEMENTS;

    printf(" todos: %x---\n", todos);
    gap_run();
}

//static uint8_t test_irk[] =  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

void ble_init(void){

    advertisements_enabled = 0;
    // setup central device db
    central_device_db_init();

    sm_init();
//    gap_random_address_set_update_period(300000);
//    gap_random_address_set_mode(GAP_RANDOM_ADDRESS_RESOLVABLE);

    sm_set_authentication_requirements( SM_AUTHREQ_BONDING | SM_AUTHREQ_MITM_PROTECTION); 
    sm_set_request_security(1);

    //strcpy(gap_device_name, "BTstack");
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
//    sm_register_oob_data_callback(get_oob_data_callback);
//    sm_set_encryption_key_size_range(7, 16);
//    sm_test_set_irk(test_irk);

    //gap_random_address_set_update_period(300000);
    //gap_random_address_set_mode(GAP_RANDOM_ADDRESS_RESOLVABLE);

    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);
    att_server_register_packet_handler(app_packet_handler);
}
