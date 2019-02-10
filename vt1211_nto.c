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
#include "libds/src/hashmap.h"

typedef struct {
  uint16_t cir;
  uint16_t cdr;
  uint8_t  ports36;
  uint8_t  verbose;
} params_t;

typedef struct {
  bool  busy;
  pid_t pid;
} gpio_pin_status_t;

typedef struct {
  bool            busy;
  pid_t           pid;
  struct hashmap  *pins;
} gpio_port_status_t;

static params_t                   params;
static const char*                params_str = "i:d:pv";
static resmgr_connect_funcs_t     connect_funcs;
static resmgr_io_funcs_t          io_funcs;
static iofunc_attr_t              attr;
static struct hashmap             *ports_status;
static gpio_portsinfo_t           ports_info;

static void debugf(const char *format, ... ) {
  if (params.verbose) {
    va_list args;
    va_start (args, format);
    vprintf(format, args);
    va_end (args);
  }
}

bool vt1211_port_check(gpio_data_t *port_data) {
  uint8_t     port_id   = port_data->port;
  struct hkey port_key  = {&port_id, sizeof(port_id)};

  return hashmap_contains(ports_status, &port_key);
}

bool vt1211_port_check_perm1(pid_t pid, gpio_data_t *port_data) {
  uint8_t     port_id   = port_data->port;
  struct hkey port_key  = {&port_id, sizeof(port_id)};

  gpio_port_status_t *port_status = hashmap_get(ports_status, &port_key);

  if (port_status->pid == pid) {
    return true;
  } else {
    return false;
  }
}

bool vt1211_port_is_busy(gpio_data_t *port_data) {
  uint8_t     port_id   = port_data->port;
  struct hkey port_key  = {&port_id, sizeof(port_id)};

  gpio_port_status_t *port_status = hashmap_get(ports_status, &port_key);
  return port_status->busy;
}

void vt1211_port_set_busy(pid_t pid, gpio_data_t *port_data, bool busy) {
  uint8_t     port_id   = port_data->port;
  struct hkey port_key  = {&port_id, sizeof(port_id)};

  gpio_port_status_t *port_status = hashmap_get(ports_status, &port_key);
  port_status->pid  = pid;
  port_status->busy = busy;
}







bool vt1211_port_check_perm(pid_t pid, gpio_data_t *port_data) {
  uint8_t     port_id   = port_data->port;   
  struct hkey port_key  = {&port_id, sizeof(port_id)};

  gpio_port_status_t *port_status = hashmap_get(ports_status, &port_key);
      
  if (port_status->busy && port_status->pid == pid) {
    return true;
  } else {
    return false;
  }
}

bool vt1211_pin_check_perm(pid_t pid, gpio_data_t *port_data) {
  uint8_t     port_id   = port_data->port;   
  struct hkey port_key  = {&port_id, sizeof(port_id)};

  gpio_port_status_t *port_status = hashmap_get(ports_status, &port_key);
      
  if (!port_status->busy) {
    uint8_t     pin_id  = port_data->pin;
    struct hkey pin_key = {&pin_id, sizeof(pin_id)};

    gpio_pin_status_t *pin_status = hashmap_get(port_status->pins, &pin_key);

    if (pin_status->busy && pin_status->pid == pid) {
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

int io_devctl(resmgr_context_t *ctp, io_devctl_t *msg, RESMGR_OCB_T *ocb) {
  int             rc;
  int             nbytes;
  void            *data;
  pid_t           pid;
  gpio_data_t     *port_data;

  data      = _DEVCTL_DATA (msg->i);
  nbytes    = 0;
  rc        = ENOSYS;
  pid       = ctp->info.pid;
  port_data = (gpio_data_t *) data;

  debugf("dcmd: %0X from pid: %d\n", msg->i.dcmd, pid);

  switch (msg->i.dcmd) {
    case VT1211_GET_INFO: {
      debugf("Action: info\n");
      gpio_portsinfo_t *info = (gpio_portsinfo_t *) data;

      memcpy(info, &ports_info, sizeof(gpio_portsinfo_t));

      nbytes = sizeof(gpio_portsinfo_t);
      rc = EOK;
      break;
    }
    case VT1211_REQ_PIN: {
      uint8_t     port_id   = port_data->port;   
      struct hkey port_key  = {&port_id, sizeof(port_id)};

      debugf("Port %d pin %d request. Status: ", port_data->port, port_data->pin);

      gpio_port_status_t *port_status = hashmap_get(ports_status, &port_key);
      if (!port_status->busy) {
        uint8_t     pin_id  = port_data->pin;
        struct hkey pin_key  = {&pin_id, sizeof(pin_id)};

        gpio_pin_status_t *pin_status = hashmap_get(port_status->pins, &pin_key);

        if (pin_status != NULL) {
          if (!pin_status->busy) {
            pin_status->busy = true;
            pin_status->pid  = pid;

            debugf("OK\n");
            rc = EOK;
          } else {
            debugf("Pin is busy\n");
            rc = VT1211_ERR_PIN_BUSY;
          }
        } else {
          debugf("Incorrect pin\n");
          rc = VT1211_ERR_INCRCT_PIN;
        }
      } else {
        debugf("Port is busy\n");
        rc = VT1211_ERR_PORT_BUSY;
      }
      break;
    }
    case VT1211_FREE_PIN: {
      uint8_t     port_id   = port_data->port;   
      struct hkey port_key  = {&port_id, sizeof(port_id)};

      gpio_port_status_t *port_status = hashmap_get(ports_status, &port_key);

      debugf("Port %d pin %d free request. Status: ", port_data->port, port_data->pin);

      if (port_status != NULL) {
        if (!port_status->busy) {
          uint8_t     pin_id  = port_data->pin;
          struct hkey pin_key  = {&pin_id, sizeof(pin_id)};

          gpio_pin_status_t *pin_status = hashmap_get(port_status->pins, &pin_key);

          if (pin_status != NULL) {
            if (pin_status->pid == pid) {
              if (pin_status->busy) {

                pin_status->busy = false;
                pin_status->pid  = NULL;

                debugf("OK\n");
                rc = EOK;
              } else {
                debugf("Already free\n");
                rc = VT1211_ERR_ALREADY;
              }
            } else {
              debugf("Only owner can free pin\n"); 
              rc = VT1211_ERR_PERM;
            }            
          } else {
            debugf("Incorrect pin\n");
            rc = VT1211_ERR_INCRCT_PIN;
          }
        } else {
          debugf("Port is busy\n");
          rc = VT1211_ERR_PORT_BUSY;
        }
      } else {
        debugf("Incorrect port\n");
        rc = VT1211_ERR_INCRCT_PORT;
      }
      break;
    }
    case VT1211_CONFIG_PIN: {
      debugf("Config port %d pin %d: ", port_data->port, port_data->pin);

      if (vt1211_pin_check_perm(pid, port_data)) {
        vt_pin_mode(port_data->port, port_data->pin, port_data->data);

        debugf("OK\n");
        rc = EOK;
      } else {
        debugf("Only owner can configure pin\n");
        rc = VT1211_ERR_PERM;
      }

      break;
    }
    case VT1211_SET_PIN: { 
      debugf("Set port %d pin %d data %02X: ", port_data->port, port_data->pin, port_data->data);

      if (vt1211_pin_check_perm(pid, port_data)) {
        vt_pin_set(port_data->port, port_data->pin, port_data->data);

        debugf("OK\n");
        rc = EOK;
      } else {
        debugf("Only owner can set pin\n");
        rc = VT1211_ERR_PERM;
      }

      break;
    }
    case VT1211_GET_PIN: {
      debugf("Get port %d pin %d: ", port_data->port, port_data->pin);

      if (vt1211_pin_check_perm(pid, port_data)) {
        port_data->data = vt_pin_get(port_data->port, port_data->pin);

        debugf("OK. Data: %02X\n", port_data->data);
        nbytes = sizeof(gpio_data_t);
        rc = EOK;
      } else {
        debugf("Only owner can get pin\n");
        rc = VT1211_ERR_PERM;
      }

      break;
    }
    case VT1211_REQ_PORT: {
      debugf("Port %d request. Status: ", port_data->port);

      if (!vt1211_port_check(port_data)) {
        debugf("Incorrect port\n");
        rc = VT1211_ERR_INCRCT_PORT;
        break;
      }

      if (vt1211_port_is_busy(port_data)) {
        debugf("Busy\n");
        rc = VT1211_ERR_PORT_BUSY;
        break;
      }

      vt1211_port_set_busy(pid, port_data, true);

      debugf("OK\n");
      rc = EOK;
      break;
    }
    case VT1211_FREE_PORT: {
      debugf("Port %d free request. Status: ", port_data->port);

      if (!vt1211_port_check(port_data)) {
        debugf("Incorrect port\n");
        rc = VT1211_ERR_INCRCT_PORT;
        break;
      }

      if (!vt1211_port_check_perm1(pid, port_data)) {
        debugf("Only owner can free port\n");
        rc = VT1211_ERR_PERM;
        break;
      }

      if (!vt1211_port_is_busy(port_data)) {
        debugf("Already free\n");
        rc = VT1211_ERR_ALREADY;
        break;
      }

      vt1211_port_set_busy(pid, port_data, false);

      debugf("OK\n");
      rc = EOK;
      break;
    }
    case VT1211_CONFIG_PORT: {
      debugf("Config port %d: ", port_data->port);

      if (!vt1211_port_check(port_data)) {
        debugf("Incorrect port\n");
        rc = VT1211_ERR_INCRCT_PORT;
        break;
      }

      if (!vt1211_port_check_perm1(pid, port_data) && vt1211_port_is_busy(port_data)) {
        debugf("Only owner can configure port\n");
        rc = VT1211_ERR_PERM;
        break;
      }

      vt_port_mode(port_data->port, port_data->data);

      debugf("OK\n");
      rc = EOK;
    }
    case VT1211_SET_PORT: {
      debugf("Set port %d Data %02X: ", port_data->port, port_data->data);

      if (!vt1211_port_check(port_data)) {
        debugf("Incorrect port\n");
        rc = VT1211_ERR_INCRCT_PORT;
        break;
      }

      if (!vt1211_port_check_perm1(pid, port_data) && vt1211_port_is_busy(port_data)) {
        debugf("Only owner can set port\n");
        rc = VT1211_ERR_PERM;
        break;
      }      

      vt_port_write(port_data->port, port_data->data);

      debugf("OK\n");
      rc = EOK;
      break;
    }
    case VT1211_GET_PORT: {
      debugf("Get port %d: ", port_data->port);

      if (!vt1211_port_check(port_data)) {
        debugf("Incorrect port\n");
        rc = VT1211_ERR_INCRCT_PORT;
        break;
      }

      if (!vt1211_port_check_perm1(pid, port_data) && vt1211_port_is_busy(port_data)) {
        debugf("Only owner can set port\n");
        rc = VT1211_ERR_PERM;
        break;
      }   

      port_data->data = vt_port_read(port_data->port);
 
      debugf("OK. Data: %02X\n", port_data->data);

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
        params.cir = (uint16_t) strtol(optarg, NULL, 16);
        break;
      }
      case 'd': {
        params.cdr = (uint16_t) strtol(optarg, NULL, 16);
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
  debugf("CIR:\t\t\t0x%04X\n", params.cir);
  debugf("CDR:\t\t\t0x%04X\n", params.cdr);
  debugf("VT1211 Init:\t\t");

  int r;

  if (params.ports36) {
    r = vt_init(VT_CONFIG_PORT_1 | VT_CONFIG_PORT_3_6, params.cir, params.cdr);
  } else {
    r = vt_init(VT_CONFIG_PORT_1, params.cir, params.cdr);
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

  uint8_t pins[8] = {
    VT1211_PIN_0,
    VT1211_PIN_1,
    VT1211_PIN_2,
    VT1211_PIN_3,
    VT1211_PIN_4,
    VT1211_PIN_5,
    VT1211_PIN_6,
    VT1211_PIN_7,      
  };

  ports_info.count = 1;
  ports_info.pins_by_port[VT1211_PORT_1] = 8;
  ports_info.pins_by_port[VT1211_PORT_3] = 8;
  ports_info.pins_by_port[VT1211_PORT_4] = 8;
  ports_info.pins_by_port[VT1211_PORT_5] = 8;
  ports_info.pins_by_port[VT1211_PORT_6] = 3;

  if (params.ports36) {
    ports_info.count = 5;
  }

  ports_status  = hashmap_create();

  for (int port = 0; port < ports_info.count; ++port) {

    gpio_port_status_t *port_status = malloc(sizeof(gpio_port_status_t));
    uint8_t     port_id   = port;
    struct hkey port_key  = {&port_id, sizeof(port_id)};

    port_status->busy = false;
    port_status->pid  = NULL;
    port_status->pins = hashmap_create();

    for (int i = 0; i < ports_info.pins_by_port[port]; ++i) {
      uint8_t           pin_id   = pins[i];
      struct hkey       pin_key  = {&pin_id, sizeof(pin_id)};
      gpio_pin_status_t *pin     = malloc(sizeof(gpio_pin_status_t));

      pin->busy = false;
      pin->pid  = NULL;

      hashmap_set(port_status->pins, &pin_key, pin);
    }

    hashmap_set(ports_status, &port_key, port_status);
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