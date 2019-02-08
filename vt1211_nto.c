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
#include <stdarg.h>
#include <unistd.h>
#include <devctl.h>
#include <string.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include "vt1211_ipc.h"
#include "vt1211_gpio/src/vt1211_gpio.h"

typedef struct {
  uint16_t cir;
  uint16_t cdr;
  uint8_t ports36;
  uint8_t verbose;
} params_t;

static params_t                   params;
static const char*                params_str = "i:d:pv";
static resmgr_connect_funcs_t     connect_funcs;
static resmgr_io_funcs_t          io_funcs;
static iofunc_attr_t              attr;

static void debugf(const char *format, ... ) {
  if (params.verbose) {
    va_list args;
    va_start (args, format);
    vprintf(format, args);
    va_end (args);
  }
}

int io_devctl(resmgr_context_t *ctp, io_devctl_t *msg, RESMGR_OCB_T *ocb) {
  int   rc;
  int   nbytes;
  void* data;

  data   = _DEVCTL_DATA (msg->i);
  nbytes = 0;
  rc     = ENOSYS;

  gpio_portinfo_t *port_info;
  gpio_data_t     *port_data;

  debugf("dcmd: %0X\n", msg->i.dcmd);

  switch (msg->i.dcmd) {
    case VT1211_GET_INFO: {
      debugf("Action: info\n");
      gpio_portinfo_t *port_info = (gpio_portinfo_t *)data;
      port_info->count = 1;
 
      nbytes = sizeof(gpio_portinfo_t);
      rc = EOK;
      break;
    }
    case VT1211_CONFIG_PIN: {
      port_data = (gpio_data_t *)data;
      
      vt_pin_mode(port_data->port, port_data->pin, port_data->data);
      debugf("Action: config pin. Port: %d, Pin: %d\n", port_data->port, port_data->pin);

      rc = EOK;
      break;
    }
    case VT1211_SET_PIN: {
      port_data = (gpio_data_t *)data;
      
      vt_pin_set(port_data->port, port_data->pin, port_data->data);
      debugf("Action: set pin. Port: %d, Pin: %d\n", port_data->port, port_data->pin);

      rc = EOK;
      break;
    }
    case VT1211_GET_PIN: {
      port_data = (gpio_data_t *)data;
      
      port_data->data = vt_pin_get(port_data->port, port_data->pin);
      debugf("Action: get pin. Port: %d, Pin: %d\n", port_data->port, port_data->pin);

      nbytes = sizeof(gpio_data_t);
      rc = EOK;
      break;
    }
    case VT1211_CONFIG_PORT: {
      port_data = (gpio_data_t *)data;

      vt_port_mode(port_data->port, port_data->data);
      debugf("Action: config port. Port: %d", port_data->port);

      rc = EOK;
      break;
    }
    case VT1211_SET_PORT: {
      port_data = (gpio_data_t *)data;
      
      vt_port_write(port_data->port, port_data->data);
      debugf("Action: set port. Port: %d Data: %02X\n", port_data->port, port_data->data);

      rc = EOK;
      break;
    }
    case VT1211_GET_PORT: {
      port_data = (gpio_data_t *)data;

      port_data->data = vt_port_read(port_data->port);
      debugf("Action: get port. Port: %d", port_data->port);

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
void params_init(int argc, char **argv) {
  params.verbose  = 0;
  params.ports36  = 0;
  params.cir      = 0x002E;
  params.cdr      = 0x002F;

  int opt = getopt( argc, argv, params_str);
  while( opt != -1 ) {
    switch( opt ) {
      case 'i': {
        params.cir = (int)strtol(optarg, NULL, 16);
        break;
      }
      case 'd': {
        params.cdr = (int)strtol(optarg, NULL, 16);
        break;
      }
      case 'p': {
        params.ports36 = 1;
        break;
      }
      case 'v': {
        params.verbose = 1;
        break;
      }
      default: {
        break;
      }
    }
         
    opt = getopt( argc, argv, params_str );
  }
}

int vt1211_init() {
  debugf("==============================================\n");
  debugf("Request I/O privileges:\t");

  if (!io_request()) {
    debugf("ERROR\n");
    return EXIT_FAILURE;
  } else {
    debugf("OK\n");
  }

  debugf("VT1211 Init:\t\t");

  int r;

  if (params.ports36) {
    r = vt_init(VT_CONFIG_PORT_1 | VT_CONFIG_PORT_3_6);
  } else {
    r = vt_init(VT_CONFIG_PORT_1);
  }

  switch (r) {
    case VT_INIT_NOT_FOUND: {
      debugf("ERROR VT1211 Not found\n");
      debugf("==============================================\n");
      return EXIT_FAILURE;
    }
    case VT_INIT_NO_PORT: {
      debugf("ERROR No port selected\n");
      debugf("==============================================\n");
      return EXIT_FAILURE;
    }
    case VT_INIT_OK:
    default: {
      debugf("OK\n");
    }
  }

  uint8_t   vt_id    = vt_get_dev_id();
  uint8_t   vt_rev   = vt_get_dev_rev();
  uint16_t  vt_base  = vt_get_baddr();

  debugf("VT1211 ID: %02X, Revision: %02X, Base addr.: %04x\n", vt_id, vt_rev, vt_base);
  debugf("==============================================\n");

  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  params_init(argc, argv);

  if (vt1211_init() != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }

  resmgr_attr_t        resmgr_attr;
  dispatch_t           *dpp;
  dispatch_context_t   *ctp;
  int                  id;

  if((dpp = dispatch_create()) == NULL) {
    fprintf(stderr, "%s: Unable to allocate dispatch handle.\n", argv[0]);
    return EXIT_FAILURE;
  }

  memset(&resmgr_attr, 0, sizeof resmgr_attr);
  resmgr_attr.nparts_max      = 1;
  resmgr_attr.msg_max_size    = 1024;

  iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs, _RESMGR_IO_NFUNCS, &io_funcs);
  iofunc_attr_init(&attr, S_IFNAM | 0666, 0, 0);

  io_funcs.devctl = io_devctl;

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

  ctp = dispatch_context_alloc(dpp);

  while(1) {
    if((ctp = dispatch_block(ctp)) == NULL) {
      fprintf(stderr, "block error\n");
      return EXIT_FAILURE;
    }
    dispatch_handler(ctp);
  }
}