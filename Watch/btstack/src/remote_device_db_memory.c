/*
 * Copyright (C) 2010 by Matthias Ringwald
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

#include <string.h>
#include <stdlib.h>

#include <btstack/utils.h>

#include "remote_device_db.h"
#include "btstack_memory.h"
#include "debug.h"

#include <btstack/linked_list.h>

#include "cfs/cfs.h"

struct element
{
  link_key_t link_key;
  char device_name[DEVICE_NAME_LEN];
  link_key_type_t link_type;
};

// Device info
static void db_open(void){
}

static void db_close(void){
}

static int get_name(bd_addr_t *bd_addr, device_name_t *device_name) {

  return 1;
}

static void formatname(char* filename, bd_addr_t addr)
{
  sprintf(filename, "_%02x%02x%02x%02x%02x%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static int get_link_key(bd_addr_t *bd_addr, link_key_t *link_key, link_key_type_t *type) {
  log_info("get link key for %s\n", bd_addr_to_str(*bd_addr));

  char filename[32];
  formatname(filename, *bd_addr);
  int fd = cfs_open(filename, CFS_READ);
  struct element entry;
  if (fd == -1)
    return 0;

  int len = cfs_read(fd, &entry, sizeof(struct element));
  if (len == sizeof(struct element))
  {
      memcpy(*link_key, entry.link_key, LINK_KEY_LEN);
      if (type)
        *type = entry.link_type;
      cfs_close(fd);
      return 1;
  }

  cfs_close(fd);
  return 0;
}

static void delete_link_key(bd_addr_t *bd_addr){
  log_info("delete link key for %s\n", bd_addr_to_str(*bd_addr));
  char filename[32];
  formatname(filename, *bd_addr);

  cfs_remove(filename);
}


static void put_link_key(bd_addr_t *bd_addr, link_key_t *link_key, link_key_type_t type){
  log_info("put link key for %s\n", bd_addr_to_str(*bd_addr));
  char filename[32];
  formatname(filename, *bd_addr);
  struct element entry;
  int fd = cfs_open(filename, CFS_READ);
  if (fd != -1)
  {
    cfs_read(fd, &entry, sizeof(struct element));
    cfs_close(fd);
  }

  fd = cfs_open(filename, CFS_WRITE);
  
  memcpy(entry.link_key, *link_key, LINK_KEY_LEN);
  entry.link_type = type;
  cfs_write(fd, &entry, sizeof(struct element));

  cfs_close(fd);
}

static void delete_name(bd_addr_t *bd_addr){
}

static void put_name(bd_addr_t *bd_addr, device_name_t *device_name){
}


// MARK: PERSISTENT RFCOMM CHANNEL ALLOCATION
static uint8_t persistent_rfcomm_channel(char *serviceName){

  return 1;
}


const remote_device_db_t remote_device_db_memory = {
    db_open,
    db_close,
    get_link_key,
    put_link_key,
    delete_link_key,
    get_name,
    put_name,
    delete_name,
    persistent_rfcomm_channel
};
