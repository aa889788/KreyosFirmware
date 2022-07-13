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
#include "btstack-config.h"
#include "central_device_db.h"
#include "debug.h"

#include "contiki.h"
#include "cfs/cfs.h"
#include <stdio.h>
#include <string.h>
#define NAMESERIAL "BLEDB%d.db"
// Central Device db implemenation using static memory
typedef struct central_device_memory_db {
    uint8_t addr_type;
    bd_addr_t addr;
    sm_key_t csrk;
    sm_key_t irk;
    uint32_t signing_counter;
} central_device_memory_db_t;

#define CENTRAL_DEVICE_MEMORY_SIZE 4
static int central_devices_count;

static void central_device_db_dump();

/* Read the content from disk */
static int central_device_db_read(int index, central_device_memory_db_t *device)
{
    char name[20];
    sprintf(name, NAMESERIAL, index);
    int handle = cfs_open(name, CFS_READ);
    if (handle == -1)
    {
        log_error("central file %s failed to open read\n", name);
        return -1;
    }

    int size = cfs_read(handle, device, sizeof(central_device_memory_db_t));
#if 1
    if (size != sizeof(central_device_memory_db_t))
    {
        log_error("read, size mismatch for central file: %d(%d != %d)\n", index, size, sizeof(central_device_memory_db_t));
        cfs_close(handle);
        cfs_remove(name);
        return -1;
    }
#endif
    cfs_close(handle);
    return 0;
}

static int central_device_db_write(int index, central_device_memory_db_t *device)
{
    char name[20];
    sprintf(name, NAMESERIAL, index);
    cfs_remove(name);
    int handle = cfs_open(name, CFS_WRITE);
    if (handle == -1)
    {
        log_error("central file %s failed to open write\n", name);
        return -1;
    }

    int size = cfs_write(handle, device, sizeof(central_device_memory_db_t));
    if (size != sizeof(central_device_memory_db_t))
    {
        log_error("written, size mismatch for central file: %d(%d != %d)\n", index, size, sizeof(central_device_memory_db_t));
        cfs_close(handle);
        cfs_remove(name);
        return -1;
    }
    printf("%d bytes written.\n", size);

    cfs_close(handle);
    return 0;    
}

void central_device_db_init(){
    central_devices_count = CENTRAL_DEVICE_MEMORY_SIZE;

    printf("central device db init: count = %d\n", central_devices_count);
    central_device_db_dump();
}

// @returns number of device in db
int central_device_db_count(void){
    return central_devices_count;
}

void central_device_db_remove(int index){
    char name[20];
    sprintf(name, NAMESERIAL, index);

    cfs_remove(name);
}

int central_device_db_add(int addr_type, bd_addr_t addr, sm_key_t irk, sm_key_t csrk){
    printf("Central Device DB adding type %u - ", addr_type);
    print_bd_addr(addr);
    print_key("irk", irk);
    print_key("csrk", csrk);

    int index;
    if (central_devices_count >= CENTRAL_DEVICE_MEMORY_SIZE) 
    {
        index = clock_seconds() % CENTRAL_DEVICE_MEMORY_SIZE;
    }
    else
    {
        index = central_devices_count++;
    }

    central_device_memory_db_t info;

    info.addr_type = addr_type;
    memcpy(info.addr, addr, 6);
    memcpy(info.csrk, csrk, 16);
    memcpy(info.irk, irk, 16);
    info.signing_counter = 0; 

    if (central_device_db_write(index, &info))
        return -1;

    return index;
}


// get device information: addr type and address
void central_device_db_info(int index, int * addr_type, bd_addr_t addr, sm_key_t irk){
    central_device_memory_db_t info;

    if (central_device_db_read(index, &info))
    {
        printf("faile to read the device info: %d\n", index);
        if (addr_type) *addr_type = -1;
        return;
    }

    if (addr_type) *addr_type = info.addr_type;
    if (addr) memcpy(addr, info.addr, 6);
    if (irk) memcpy(irk, info.irk, 16);
}

// get signature key
void central_device_db_csrk(int index, sm_key_t csrk){
    if (csrk) 
    {
        central_device_memory_db_t info;

        if (central_device_db_read(index, &info))
            return;
        memcpy(csrk, info.csrk, 16);
    }
}


// query last used/seen signing counter
uint32_t central_device_db_counter_get(int index){
    central_device_memory_db_t info;

    if (central_device_db_read(index, &info))
        return 0;
    else
        return info.signing_counter;
}

// update signing counter
void central_device_db_counter_set(int index, uint32_t counter){
    central_device_memory_db_t info;

    if (central_device_db_read(index, &info))
        return;

    info.signing_counter = counter;
    central_device_db_write(index, &info);
}

static void central_device_db_dump(){
    printf("Central Device DB dump, devices: %u\n", central_devices_count);
    int i;
    for (i=0;i<central_devices_count;i++){
        central_device_memory_db_t info;

        if (central_device_db_read(i, &info))
            continue;
        printf("%u: %u ", i, info.addr_type);
        print_bd_addr(info.addr);
        print_key("irk", info.irk);
        print_key("csrk", info.csrk);
    }
}
