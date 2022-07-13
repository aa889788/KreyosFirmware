/*
 * Copyright (C) 2009 by Matthias Ringwald
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

/*
 *  hci_dump.c
 *
 *  Dump HCI trace in various formats:
 *
 *  - BlueZ's hcidump format
 *  - Apple's PacketLogger
 *  - stdout hexdump
 *
 *  Created by Matthias Ringwald on 5/26/09.
 */

#include "btstack-config.h"

#include "hci_dump.h"
#include "hci.h"
#include "hci_transport.h"
#include <btstack/hci_cmds.h>
#include <stdio.h>

#ifndef EMBEDDED
#include <fcntl.h>        // open
#include <arpa/inet.h>    // hton..
#include <unistd.h>       // write
#include <time.h>
#include <sys/time.h>     // for timestamps
#include <sys/stat.h>     // for mode flags
#include <stdarg.h>       // for va_list
#endif

//#define DUMP

// BLUEZ hcidump
typedef struct {
	uint16_t	len;
	uint8_t		in;
	uint8_t		pad;
	uint32_t	ts_sec;
	uint32_t	ts_usec;
    uint8_t     packet_type;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
hcidump_hdr;

// APPLE PacketLogger
typedef struct {
	uint32_t	len;
	uint32_t	ts_sec;
	uint32_t	ts_usec;
	uint8_t		type;   // 0xfc for note
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
pktlog_hdr;

#ifndef EMBEDDED
static int dump_file = -1;
static hcidump_hdr header_bluez;
static pktlog_hdr  header_packetlogger;
static char time_string[40];
static int  max_nr_packets = -1;
static int  nr_packets = 0;
static char log_message_buffer[256];
#endif
static int dump_format = HCI_DUMP_STDOUT;

static void getPDUName(char** name, uint8_t *buf, uint8_t in);

void hci_dump_open(char *filename, hci_dump_format_t format){
#ifndef EMBEDDED
    dump_format = format;
    if (dump_format == HCI_DUMP_STDOUT) {
        dump_file = fileno(stdout);
    } else {
        dump_file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    }
#endif
}

#ifndef EMBEDDED
void hci_dump_set_max_packets(int packets){
    max_nr_packets = packets;
}
#endif

#ifdef DUMP
void hci_dump_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len) {
#ifndef EMBEDDED

    if (dump_file < 0) return; // not activated yet

    // don't grow bigger than max_nr_packets
    if (dump_format != HCI_DUMP_STDOUT && max_nr_packets > 0){
        if (nr_packets >= max_nr_packets){
            lseek(dump_file, 0, SEEK_SET);
            ftruncate(dump_file, 0);
            nr_packets = 0;
        }
        nr_packets++;
    }

    // get time
    struct timeval curr_time;
    struct tm* ptm;
    gettimeofday(&curr_time, NULL);
#endif

    switch (dump_format){
        case HCI_DUMP_STDOUT: {
#ifndef EMBEDDED
            /* Obtain the time of day, and convert it to a tm struct. */
            ptm = localtime (&curr_time.tv_sec);
            /* Format the date and time, down to a single second. */
            strftime (time_string, sizeof (time_string), "[%Y-%m-%d %H:%M:%S", ptm);
            /* Compute milliseconds from microseconds. */
            uint16_t milliseconds = curr_time.tv_usec / 1000;
            /* Print the formatted time, in seconds, followed by a decimal point
             and the milliseconds. */
            printf ("%s.%03u] ", time_string, milliseconds);
#endif
            char *buf = NULL;
            switch (packet_type){
                case HCI_COMMAND_DATA_PACKET:
                    getPDUName(&buf, packet, 0);
                    if (buf == NULL)
                      buf = "Unknown";
                    printf("CMD (%s) => ", buf);
                    break;
                case HCI_EVENT_PACKET:
                    if (packet[0] > 0x50)
                      return; // skip all the bt events
                    getPDUName(&buf, packet, 1);
                    if (buf == NULL)
                      buf = "Unknow";
                    printf("EVT (%s) <= ", buf);
                    break;
                case HCI_ACL_DATA_PACKET:
                    if (in) {
                        printf("ACL <= ");
                    } else {
                        printf("ACL => ");
                    }
                    break;
                case LOG_MESSAGE_PACKET:
                    // assume buffer is big enough
                    packet[len] = 0;
                    printf("LOG -- %s\n", (char*) packet);
                    return;
                default:
                    return;
            }
            hexdump(packet, len);
            break;
        }
#ifndef EMBEDDED
        case HCI_DUMP_BLUEZ:
            bt_store_16( (uint8_t *) &header_bluez.len, 0, 1 + len);
            header_bluez.in  = in;
            header_bluez.pad = 0;
            bt_store_32( (uint8_t *) &header_bluez.ts_sec,  0, curr_time.tv_sec);
            bt_store_32( (uint8_t *) &header_bluez.ts_usec, 0, curr_time.tv_usec);
            header_bluez.packet_type = packet_type;
            write (dump_file, &header_bluez, sizeof(hcidump_hdr) );
            write (dump_file, packet, len );
            break;

        case HCI_DUMP_PACKETLOGGER:
            header_packetlogger.len = htonl( sizeof(pktlog_hdr) - 4 + len);
            header_packetlogger.ts_sec =  htonl(curr_time.tv_sec);
            header_packetlogger.ts_usec = htonl(curr_time.tv_usec);
            switch (packet_type){
                case HCI_COMMAND_DATA_PACKET:
                    header_packetlogger.type = 0x00;
                    break;
                case HCI_ACL_DATA_PACKET:
                    if (in) {
                        header_packetlogger.type = 0x03;
                    } else {
                        header_packetlogger.type = 0x02;
                    }
                    break;
                case HCI_EVENT_PACKET:
                    header_packetlogger.type = 0x01;
                    break;
                case LOG_MESSAGE_PACKET:
                    header_packetlogger.type = 0xfc;
                    break;
                default:
                    return;
            }
            write (dump_file, &header_packetlogger, sizeof(pktlog_hdr) );
            write (dump_file, packet, len );
            break;
#endif
        default:
            break;
    }
}
#else
void hci_dump_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len) {
}
#endif
void hci_dump_log(const char * format, ...){
#ifndef EMBEDDED
    va_list argptr;
    va_start(argptr, format);
    int len = vsnprintf(log_message_buffer, sizeof(log_message_buffer), format, argptr);
    hci_dump_packet(LOG_MESSAGE_PACKET, 0, (uint8_t*) log_message_buffer, len);
    va_end(argptr);
#endif
}

void hci_dump_close(){
#ifndef EMBEDDED
    close(dump_file);
    dump_file = -1;
#endif
}

#define strcpy(a,b) *a = b
void getPDUName(char** name_string, uint8_t *pdu, uint8_t in)
{
  uint8_t pdu_code = pdu[0];
  if (in)
  {
    switch(pdu_code)
    {
    case 0x01:
      strcpy(name_string, "HCI Inquiry Complete Event");
      break;
    case 0x02:
      strcpy(name_string, "HCI Inquiry Result Event");
      break;
    case 0x03:
      strcpy(name_string, "HCI Connection Complete Event");
      break;
    case 0x04:
      strcpy(name_string, "HCI Connection Request Event");
      break;
    case 0x05:
      strcpy(name_string, "HCI Disconnection Complete Event");
      break;
    case 0x06:
      strcpy(name_string, "HCI Authentication Complete Event");
      break;
    case 0x07:
      strcpy(name_string, "HCI Remote Name Request Complete Event");
      break;
    case 0x08:
      strcpy(name_string, "HCI Encryption Change Event");
      break;
    case 0x09:
      strcpy(name_string, "HCI Change Connection Link Key Complete Event");
      break;
    case 0x0A:
      strcpy(name_string, "HCI Master Link Key Complete Event");
      break;
    case 0x0B:
      strcpy(name_string, "HCI Read Remote Supported Features Complete Event");
      break;
    case 0x0C:
      strcpy(name_string, "HCI Read Remote Version Information Complete Event");
      break;
    case 0x0D:
      strcpy(name_string, "HCI QoS Setup Complete Event");
      break;
    case 0x0E:
      strcpy(name_string, "HCI Command Complete Event");
      break;
    case 0x0F:
      strcpy(name_string, "HCI Command Status Event");
      break;
    case 0x10:
      strcpy(name_string, "HCI Hardware Error Event");
      break;
    case 0x11:
      strcpy(name_string, "HCI Flush Occurred Event");
      break;
    case 0x12:
      strcpy(name_string, "HCI Role Change Event");
      break;
    case 0x13:
      strcpy(name_string, "HCI Number Of Completed Packets Event");
      break;
    case 0x14:
      strcpy(name_string, "HCI Mode Change Event");
      break;
    case 0x15:
      strcpy(name_string, "HCI Return Link Keys Event");
      break;
    case 0x16:
      strcpy(name_string, "HCI PIN Code Request Event");
      break;
    case 0x17:
      strcpy(name_string, "HCI Link Key Request Event");
      break;
    case 0x18:
      strcpy(name_string, "HCI Link Key Notification Event");
      break;
    case 0x19:
      strcpy(name_string, "HCI Loopback Command Event");
      break;
    case 0x1A:
      strcpy(name_string, "HCI Data Buffer Overflow Event");
      break;
    case 0x1B:
      strcpy(name_string, "HCI Max Slots Change Event");
      break;
    case 0x1C:
      strcpy(name_string, "HCI Read Clock Offset Complete Event");
      break;
    case 0x1D:
      strcpy(name_string, "HCI Connection Packet Type Changed Event");
      break;
    case 0x1E:
      strcpy(name_string, "HCI QoS Violation Event");
      break;
    case 0x1F:
      strcpy(name_string, "HCI Page Scan Mode Change Event");
      break;
    case 0x20:
      strcpy(name_string, "HCI Page Scan Repetition Mode Change Event");
      break;
    case 0x21:
      strcpy(name_string, "HCI Flow Specification Complete Event");
      break;
    case 0x22:
      strcpy(name_string, "HCI Inquiry Result with RSSI Event");
      break;
    case 0x23:
      strcpy(name_string, "HCI Read Remote Extended Features Complete Event");
      break;
    case 0x24:
      strcpy(name_string, "HCI Fixed Address Event");
      break;
    case 0x25:
      strcpy(name_string, "HCI Alias Address Event");
      break;
    case 0x26:
      strcpy(name_string, "HCI Generate Alias Request Event");
      break;
    case 0x27:
      strcpy(name_string, "HCI Active Address Event");
      break;
    case 0x28:
      strcpy(name_string, "HCI Allow Private Pairing Event");
      break;
    case 0x29:
      strcpy(name_string, "HCI Alias Address Request Event");
      break;
    case 0x2A:
      strcpy(name_string, "HCI Alias Not Recognised Event");
      break;
    case 0x2B:
      strcpy(name_string, "HCI Fixed Address Attempt Event");
      break;
    case 0x2C:
      strcpy(name_string, "HCI Synchonous Connection Complete Event");
      break;
    case 0x2D:
      strcpy(name_string, "HCI Synchonous Connection Changed Event");
      break;

    case 0x3E:
      {
        // LE META Event
        switch(pdu[2])
        {
          case 0x01:
            strcpy(name_string, "LE Connection Complete");
            break;
          case 0x02:
            strcpy(name_string, "LE Advertising Report");
            break;
          case 0x03:
            strcpy(name_string, "LE Connection Update Complete");
            break;
          case 0x04:
            strcpy(name_string, "LE Read Remote Used Features Complete");
            break;
          case 0x05:
            strcpy(name_string, "LE Long Term Key Request");
            break;
          default:
            strcpy(name_string, "Unknow LE Meta Event");
            break;
        }
      }
      /* Ralink modify start, Tom, 2008/1/20 */
    case 0x38:
      strcpy(name_string, "HCI Link Supervision Timeout Changed Event");
      break;
      /* Ralink modify end, Tom, 2008/1/20 */
    case 0xFF:
      switch(pdu[2])
      {
      case 0x00:
        strcpy(name_string, "TCI Test Control Complete Event");
        break;
      case 0x01:
        strcpy(name_string, "TCI Read Pump Monitors Complete Event");
        break;
      case 0x02:
        strcpy(name_string, "TCI Read Master Slave Switch Clocks Event");
        break;
      case 0x22:
        strcpy(name_string, "TCI Trace LMP Message Complete Event");
        break;
      default:
        strcpy(name_string, "Unknown TCI Event");
      }
      break;
    default:
      strcpy(name_string, "Invalid HCI Event");
    }
  }
  else
  {
    switch((pdu[1] >> 2) & 0x3F)
    {
      //#if 0
    case 0: // No-Op
      if(pdu[0] == 0)
        strcpy(name_string, "No-op");
      else
        strcpy(name_string, "Invalid HCI Command");
      break;
    case 1: /* Link Control Commands */
      switch(pdu[0])
      {
      case 0x01:
        strcpy(name_string, "HCI Inquiry");
        break;
      case 0x02:
        strcpy(name_string, "HCI Inquiry Cancel");
        break;
      case 0x03:
        strcpy(name_string, "HCI Periodic Inquiry Mode");
        break;
      case 0x04:
        strcpy(name_string, "HCI Exit Periodic Inquiry Mode");
        break;
      case 0x05:
        strcpy(name_string, "HCI Create Connection");
        break;
      case 0x06:
        strcpy(name_string, "HCI Disconnect");
        break;
      case 0x07:
        strcpy(name_string, "HCI Add SCO Connection");
        break;
      case 0x08:
        strcpy(name_string, "HCI Create Connection Cancel");
        break;
      case 0x09:
        strcpy(name_string, "HCI Accept Connection Request");
        break;
      case 0x0A:
        strcpy(name_string, "HCI Reject Connection Request");
        break;
      case 0x0B:
        strcpy(name_string, "HCI Link Key Request Reply");
        break;
      case 0x0C:
        strcpy(name_string, "HCI Link Key Request Negative Reply");
        break;
      case 0x0D:
        strcpy(name_string, "HCI PIN Code Request Reply");
        break;
      case 0x0E:
        strcpy(name_string, "HCI PIN Code Request Negative Reply");
        break;
      case 0x0F:
        strcpy(name_string, "HCI Change Connection Packet Type");
        break;

      case 0x11:
        strcpy(name_string, "HCI Authentication Requested");
        break;

      case 0x13:
        strcpy(name_string, "HCI Set Connection Encryption");
        break;

      case 0x15:
        strcpy(name_string, "HCI Change Connection Link Key");
        break;

      case 0x17:
        strcpy(name_string, "HCI Master Link Key");
        break;

      case 0x19:
        strcpy(name_string, "HCI Remote Name Request");
        break;
      case 0x1A:
        strcpy(name_string, "HCI Remote Name Request Cancel");
        break;
      case 0x1B:
        strcpy(name_string, "HCI Read Remote Supported Features");
        break;
      case 0x1C:
        strcpy(name_string, "HCI Read Remote Extended Features");
        break;
      case 0x1D:
        strcpy(name_string, "HCI Read Remote Version Information");
        break;
      case 0x1F:
        strcpy(name_string, "HCI Read Clock Offset");
        break;
      case 0x20:
        strcpy(name_string, "HCI Read LMP Handle");
        break;
      case 0x21:
        strcpy(name_string, "HCI Exchange Fixed Info");
        break;
      case 0x22:
        strcpy(name_string, "HCI Exchange Alias Info");
        break;
      case 0x23:
        strcpy(name_string, "HCI Private Pairing Request Reply");
        break;
      case 0x24:
        strcpy(name_string, "HCI Private Pairing Request Negative Reply");
        break;
      case 0x25:
        strcpy(name_string, "HCI Generated Alias");
        break;
      case 0x26:
        strcpy(name_string, "HCI Alias Address Request Reply");
        break;
      case 0x27:
        strcpy(name_string, "HCI Alias Address Request Negative Reply");
        break;
      case 0x28:
        strcpy(name_string, "HCI Setup Synchronous Connection");
        break;
      case 0x29:
        strcpy(name_string, "HCI Accept Synchronous Connection Request");
        break;
      case 0x2A:
        strcpy(name_string, "HCI Reject Synchronous Connection Request");
        break;
      default:
        strcpy(name_string, "Invalid Link Control Command");
      }
      break;
    case 2: /* Link Policy Commands */
      switch(pdu[0])
      {
      case 0x01:
        strcpy(name_string, "HCI Hold Mode");
        break;
      case 0x03:
        strcpy(name_string, "HCI Sniff Mode");
        break;
      case 0x04:
        strcpy(name_string, "HCI Exit Sniff Mode");
        break;
      case 0x05:
        strcpy(name_string, "HCI Park State");
        break;
      case 0x06:
        strcpy(name_string, "HCI Exit Park State");
        break;
      case 0x07:
        strcpy(name_string, "HCI QoS Setup");
        break;
      case 0x09:
        strcpy(name_string, "HCI Role Discovery");
        break;
      case 0x0B:
        strcpy(name_string, "HCI Switch Role");
        break;
      case 0x0C:
        strcpy(name_string, "HCI Read Link Policy Settings");
        break;
      case 0x0D:
        strcpy(name_string, "HCI Write Link Policy Settings");
        break;
      case 0x0E:
        strcpy(name_string, "HCI Read Default Link Policy Settings");
        break;
      case 0x0F:
        strcpy(name_string, "HCI Write Default Link Policy Settings");
        break;
      case 0x10:
        strcpy(name_string, "HCI Flow Specification");
        break;
      default:
        strcpy(name_string, "Invalid HCI Link Policy Command");
      }
      break;
    case 3: /* HC/BB Commands */
      switch(pdu[0])
      {
      case 0x01:
        strcpy(name_string, "HCI Set Event Mask");
        break;
      case 0x03:
        strcpy(name_string, "HCI Reset");
        break;
      case 0x05:
        strcpy(name_string, "HCI Set Event Filter");
        break;
      case 0x08:
        strcpy(name_string, "HCI Flush");
        break;
      case 0x09:
        strcpy(name_string, "HCI Read PIN Type");
        break;
      case 0x0A:
        strcpy(name_string, "HCI Write PIN Type");
        break;
      case 0x0B:
        strcpy(name_string, "HCI Create New Unit Key");
        break;
      case 0x0D:
        strcpy(name_string, "HCI Read Stored Link Key");
        break;
      case 0x11:
        strcpy(name_string, "HCI Write Stored Link Key");
        break;
      case 0x12:
        strcpy(name_string, "HCI Delete Stored Link Key");
        break;
      case 0x13:
        strcpy(name_string, "HCI Write Local Name");
        break;
      case 0x14:
        strcpy(name_string, "HCI Read Local Name");
        break;
      case 0x15:
        strcpy(name_string, "HCI Read Connection Accept Timeout");
        break;
      case 0x16:
        strcpy(name_string, "HCI Write Connection Accept Timeout");
        break;
      case 0x17:
        strcpy(name_string, "HCI Read Page Timeout");
        break;
      case 0x18:
        strcpy(name_string, "HCI Write Page Timeout");
        break;
      case 0x19:
        strcpy(name_string, "HCI Read Scan Enable");
        break;
      case 0x1A:
        strcpy(name_string, "HCI Write Scan Enable");
        break;
      case 0x1B:
        strcpy(name_string, "HCI Read Page Scan Activity");
        break;
      case 0x1C:
        strcpy(name_string, "HCI Write Page Scan Activity");
        break;
      case 0x1D:
        strcpy(name_string, "HCI Read Inquiry Scan Activity");
        break;
      case 0x1E:
        strcpy(name_string, "HCI Write Inquiry Scan Activity");
        break;
      case 0x1F:
        strcpy(name_string, "HCI Read Authentication Enable");
        break;
      case 0x20:
        strcpy(name_string, "HCI Write Authentication Enable");
        break;
      case 0x21:
        strcpy(name_string, "HCI Read Encryption Mode");
        break;
      case 0x22:
        strcpy(name_string, "HCI Write Encryption Mode");
        break;
      case 0x23:
        strcpy(name_string, "HCI Read Class of Device");
        break;
      case 0x24:
        strcpy(name_string, "HCI Write Class of Device");
        break;
      case 0x25:
        strcpy(name_string, "HCI Read Voice Setting");
        break;
      case 0x26:
        strcpy(name_string, "HCI Write Voice Setting");
        break;
      case 0x27:
        strcpy(name_string, "HCI Read Automatic Flush Timeout");
        break;
      case 0x28:
        strcpy(name_string, "HCI Write Automatic Flush Timeout");
        break;
      case 0x29:
        strcpy(name_string, "HCI Read Num Broadcast Retransmissions");
        break;
      case 0x2A:
        strcpy(name_string, "HCI Write Num Broadcast Retransmissions");
        break;
      case 0x2B:
        strcpy(name_string, "HCI Read Hold Mode Activity");
        break;
      case 0x2C:
        strcpy(name_string, "HCI Write Hold Mode Activity");
        break;
      case 0x2D:
        strcpy(name_string, "HCI Read Transmit Power Level");
        break;
      case 0x2E:
        strcpy(name_string, "HCI Read Synchronous Flow Control Enable");
        break;
      case 0x2F:
        strcpy(name_string, "HCI Write Synchronous Flow Control Enable");
        break;
      case 0x31:
        strcpy(name_string, "HCI Set Controller to Host Flow Control");
        break;
      case 0x33:
        strcpy(name_string, "HCI Host Buffer Size");
        break;
      case 0x35:
        strcpy(name_string, "HCI Host Number Of Completed Packets");
        break;
      case 0x36:
        strcpy(name_string, "HCI Read Link Supervision Timeout");
        break;
      case 0x37:
        strcpy(name_string, "HCI Write Link Supervision Timeout");
        break;
      case 0x38:
        strcpy(name_string, "HCI Read Number of Supported IAC");
        break;
      case 0x39:
        strcpy(name_string, "HCI Read Current IAC LAP");
        break;
      case 0x3A:
        strcpy(name_string, "HCI Write Current IAC LAP");
        break;
      case 0x3B:
        strcpy(name_string, "HCI Read Page Scan Period Mode");
        break;
      case 0x3C:
        strcpy(name_string, "HCI Write Page Scan Period Mode");
        break;
      case 0x3D:
        strcpy(name_string, "HCI Read Page Scan Mode");
        break;
      case 0x3E:
        strcpy(name_string, "HCI Write Page Scan Mode");
        break;
      case 0x3F:
        strcpy(name_string, "HCI Set AFH Host Channel Classification");
        break;

      case 0x42:
        strcpy(name_string, "HCI Read Inquiry Scan Type");
        break;
      case 0x43:
        strcpy(name_string, "HCI Write Inquiry Scan Type");
        break;
      case 0x44:
        strcpy(name_string, "HCI Read Inquiry Mode");
        break;
      case 0x45:
        strcpy(name_string, "HCI Write Inquiry Mode");
        break;
      case 0x46:
        strcpy(name_string, "HCI Read Page Scan Type");
        break;
      case 0x47:
        strcpy(name_string, "HCI Write Page Scan Type");
        break;
      case 0x48:
        strcpy(name_string, "HCI Read AFH Channel Assessment Mode");
        break;
      case 0x49:
        strcpy(name_string, "HCI Write AFH Channel Assessment Mode");
        break;
      case 0x4A:
        strcpy(name_string, "HCI Read Anonymity Mode");
        break;
      case 0x4B:
        strcpy(name_string, "HCI Write Anonymity Mode");
        break;
      case 0x4C:
        strcpy(name_string, "HCI Read Alias Authentication Enable");
        break;
      case 0x4D:
        strcpy(name_string, "HCI Write Alias Authentication Enable");
        break;
      case 0x4E:
        strcpy(name_string, "HCI Read Anonymous Address Change Parameters");
        break;
      case 0x4F:
        strcpy(name_string, "HCI Write Anonymous Address Change Parameters");
        break;
      case 0x50:
        strcpy(name_string, "HCI Reset Fixed Address Attempts Counter");
        break;
      case 0x56:
        strcpy(name_string, "HCI Write Simple Pairing Mode");
        break;
      case 0x6C:
        strcpy(name_string, "HCI Read LE Host Support");
        break;
      case 0x6D:
        strcpy(name_string, "HCI Write LE Host Support");
        break;
      default:
        strcpy(name_string, "Invalid HCI Host Controller & Baseband Command");
      }
      break;
    case 4: /* Informational Parameters */
      switch(pdu[0])
      {
      case 0x01:
        strcpy(name_string, "HCI Read Local Version Information");
        break;
      case 0x02:
        strcpy(name_string, "HCI Read Local Supported Commands");
        break;
      case 0x03:
        strcpy(name_string, "HCI Read Local Supported Features");
        break;
      case 0x04:
        strcpy(name_string, "HCI Read Local Extended Features");
        break;
      case 0x05:
        strcpy(name_string, "HCI Read Buffer Size");
        break;
      case 0x07:
        strcpy(name_string, "HCI Read Country Code");
        break;
      case 0x09:
        strcpy(name_string, "HCI Read BD_ADDR");
        break;
      default:
        strcpy(name_string, "Invalid HCI Informational Parameter");
      }
      break;
    case 5: /* Status Parameters */
      switch(pdu[0])
      {
      case 0x01:
        strcpy(name_string, "HCI Read Failed Contact Counter");
        break;
      case 0x02:
        strcpy(name_string, "HCI Reset Failed Contact Counter");
        break;
      case 0x03:
        strcpy(name_string, "HCI Read Link Quality");
        break;
      case 0x05:
        strcpy(name_string, "HCI Read RSSI");
        break;
      case 0x06:
        strcpy(name_string, "HCI Read AFH Channel Map");
        break;
      case 0x07:
        strcpy(name_string, "HCI Read Clock");
        break;
      default:
        strcpy(name_string, "Invalid HCI Status Parameter Command");
      }
      break;

    case 6: /* Testing Commands */
      switch(pdu[0])
      {
      case 0x01:
        strcpy(name_string, "HCI Read Loopback Mode");
        break;
      case 0x02:
        strcpy(name_string, "HCI Write Loopback Mode");
        break;
      case 0x03:
        strcpy(name_string, "HCI Enable Device Under Test Mode");
        break;
      default:
        strcpy(name_string, "Invalid HCI Testing Command");
      }
      break;
    case OGF_LE_CONTROLLER: /* LE commands */
      switch(pdu[0])
      {
        case 0x01:
          strcpy(name_string, "LE Set Event Mask");
          break;
       case 0x02:
          strcpy(name_string, "LE Read Buffer Size");
          break;
        case 0x03:
          strcpy(name_string, "LE Read Local Supported Features");
          break;
        case 0x05:
          strcpy(name_string, "LE Set Random Address");
          break;
        case 0x06:
          strcpy(name_string, "LE Set Advertising Parameters");
          break;
        case 0x07:
          strcpy(name_string, "LE Read Advertising Channel Tx Power");
          break;
        case 0x08:
          strcpy(name_string, "LE Set Advertising Data");
          break;
        case 0x09:
          strcpy(name_string, "LE Set Scan Response Data");
          break;
        case 0x0a:
          strcpy(name_string, "LE Set Advertise Enable");
          break;
        case 0x0b:
          strcpy(name_string, "LE Set Scan Parameters");
          break;
        case 0x0c:
          strcpy(name_string, "LE Set Scan Enable");
          break;
        case 0x0d:
          strcpy(name_string, "LE CreateConnection");
          break;
         
        case 0x17:
          strcpy(name_string, "LE Encrypt");
          break;
        case 0x18:
          strcpy(name_string, "LE Rand");
          break;
        case 0x19:
          strcpy(name_string, "LE Start Encryption");
          break;
        case 0x1e:
          strcpy(name_string, "LE Start Transmitter Test Command");
          break;
        default:
          strcpy(name_string, "Invalid LE command");
          break;
      }
      break;

      case OGF_VENDOR:
      {
        strcpy(name_string, "Vendor HCI Commands");
        break;
      }

    default:
      strcpy(name_string, "Invalid HCI OGF");


      //#endif
    }
  }
}