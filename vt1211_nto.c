/*
 * GPIO Resource manager for VT1211 Super I/O chip
 *
 * Copyright 2019 by Roman Serov <roman@serov.co>
 * 
 * This file is part of VT1211 GPIO Resource manager.
 *
 * VT1211 GPIO Resource manager is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * VT1211 GPIO Resource manager is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with VT1211 GPIO Resource manager. If not, see <http://www.gnu.org/licenses/>.
 * 
 * @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>
*/

#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <devctl.h>
#include <string.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include "vt1211_ipc.h"
#include "vt1211_gpio/src/vt1211_gpio.h"

static resmgr_connect_funcs_t    connect_funcs;
static resmgr_io_funcs_t         io_funcs;
static iofunc_attr_t             attr;

int io_devctl(resmgr_context_t *ctp, io_devctl_t *msg, RESMGR_OCB_T *ocb) {
  int   rc;
  int   nbytes;
  void* data;

  data   = _DEVCTL_DATA (msg->i);
  nbytes = 0;
  rc     = ENOSYS;

  gpio_portinfo_t *port_info;
  gpio_data_t     *port_data;

  //printf("dcmd: %0X\n", msg->i.dcmd);

  switch (msg->i.dcmd) {
    case VT1211_GET_INFO: {
      printf("Action: info\n");
      gpio_portinfo_t *port_info = (gpio_portinfo_t *)data;
      port_info->count = 1;
 
      nbytes = sizeof(gpio_portinfo_t);
      rc = EOK;
      break;
    }
    case VT1211_CONFIG_PIN: {
      port_data = (gpio_data_t *)data;
      
      vt_pin_mode(port_data->port, port_data->pin, port_data->data);
      printf("Action: config pin. Port: %d, Pin: %d\n", port_data->port, port_data->pin);

      rc = EOK;
      break;
    }
    case VT1211_SET_PIN: {
      port_data = (gpio_data_t *)data;
      
      vt_pin_set(port_data->port, port_data->pin, port_data->data);
      printf("Action: set pin. Port: %d, Pin: %d\n", port_data->port, port_data->pin);

      rc = EOK;
      break;
    }
    case VT1211_GET_PIN: {
      port_data = (gpio_data_t *)data;
      
      port_data->data = vt_pin_get(port_data->port, port_data->pin);
      printf("Action: get pin. Port: %d, Pin: %d\n", port_data->port, port_data->pin);

      nbytes = sizeof(gpio_data_t);
      rc = EOK;
      break;
    }
    case VT1211_CONFIG_PORT: {
      port_data = (gpio_data_t *)data;

      vt_port_mode(port_data->port, port_data->data);
      printf("Action: config port. Port: %d", port_data->port);

      rc = EOK;
      break;
    }
    case VT1211_SET_PORT: {
      port_data = (gpio_data_t *)data;
      
      vt_port_write(port_data->port, port_data->data);
      printf("Action: set port. Port: %d", port_data->port);

      rc = EOK;
      break;
    }
    case VT1211_GET_PORT: {
      port_data = (gpio_data_t *)data;

      port_data->data = vt_port_read(port_data->port);
      printf("Action: get port. Port: %d", port_data->port);

      nbytes = sizeof(gpio_data_t);
      rc = EOK;
      break;
    }
    default: {
      rc = ENOSYS;
      break;
    }
  }

  if (rc != EOK)
    return rc;

  memset(&msg->o, 0, sizeof (msg->o));

  msg->o.ret_val = EOK;
  msg->o.nbytes  = nbytes;

  rc = _RESMGR_PTR (ctp, &msg->o, sizeof (msg->o) + nbytes);

  return rc;
}

int main(int argc, char **argv) {
  printf("==============================================\n");
  printf("Request I/O privileges:\t");

  if (!io_request()) {
    printf("ERROR\n");
    return EXIT_FAILURE;
  } else {
    printf("OK\n");
  }

  printf("VT1211 Init:\t\t");

  int r = vt_init(VT_CONFIG_PORT_1 | VT_CONFIG_PORT_3_6);

  switch (r) {
    case VT_INIT_NOT_FOUND: {
      printf("ERROR VT1211 Not found\n");
      printf("==============================================\n");
      return EXIT_FAILURE;
    }
    case VT_INIT_NO_PORT: {
      printf("ERROR No port selected\n");
      printf("==============================================\n");
      return EXIT_FAILURE;
    }
    case VT_INIT_OK:
    default: {
      printf("OK\n");
    }
  }

  uint8_t vt_id     = vt_get_dev_id();
  uint8_t vt_rev    = vt_get_dev_rev();
  uint16_t vt_base  = vt_get_baddr();

  printf("VT1211 ID: %02X, Revision: %02X, Base addr.: %04x\n", vt_id, vt_rev, vt_base);
  printf("==============================================\n");

  /* declare variables we'll be using */
  resmgr_attr_t        resmgr_attr;
  dispatch_t           *dpp;
  dispatch_context_t   *ctp;
  int                  id;

    /* initialize dispatch interface */
  if((dpp = dispatch_create()) == NULL) {
    fprintf(stderr, "%s: Unable to allocate dispatch handle.\n", argv[0]);
    return EXIT_FAILURE;
  }

  /* initialize resource manager attributes */
  memset(&resmgr_attr, 0, sizeof resmgr_attr);
  resmgr_attr.nparts_max      = 1;
  resmgr_attr.msg_max_size    = 1024;

  /* initialize functions for handling messages */    
  iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs, _RESMGR_IO_NFUNCS, &io_funcs);

  /* initialize attribute structure used by the device */
  iofunc_attr_init(&attr, S_IFNAM | 0666, 0, 0);
  //io_funcs.write  = vt1211_write;
  //io_funcs.read   = vt1211_read;
  io_funcs.devctl = io_devctl;

    /* attach our device name */
  id = resmgr_attach(
            dpp,            /* dispatch handle        */
            &resmgr_attr,   /* resource manager attrs */
            "/dev/vt1211",  /* device name            */
            _FTYPE_ANY,     /* open type              */
            0,              /* flags                  */
            &connect_funcs, /* connect routines       */
            &io_funcs,      /* I/O routines           */
            &attr);         /* handle                 */
        
  if(id == -1) {
    fprintf(stderr, "%s: Unable to attach name.\n", argv[0]);
    return EXIT_FAILURE;
  }

    /* allocate a context structure */
  ctp = dispatch_context_alloc(dpp);

    /* start the resource manager message loop */
  while(1) {
    if((ctp = dispatch_block(ctp)) == NULL) {
      fprintf(stderr, "block error\n");
      return EXIT_FAILURE;
    }
    dispatch_handler(ctp);
  }
}