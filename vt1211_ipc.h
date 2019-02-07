/*
 * GPIO Resource manager for VT1211 Super I/O chip
 *
 * Copyright 2019 by Roman Serov <roman@serov.co>
 * 
 * This file is part of VT1211 GPIO Userspace library.
 *
 * VT1211 GPIO Userspace library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * VT1211 GPIO Userspace library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with VT1211 GPIO Userspace library. If not, see <http://www.gnu.org/licenses/>.
 * 
 * @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>
*/

#include <sys/types.h>
#include <stdint.h>

#define VT1211_GET_INFO     __DIOTF (_DCMD_MISC, 0x200700, gpio_portinfo_t) 
#define VT1211_CONFIG_PIN   __DIOT  (_DCMD_MISC, 0x200701, gpio_data_t) 
#define VT1211_SET_PIN      __DIOT  (_DCMD_MISC, 0x200702, gpio_data_t) 
#define VT1211_GET_PIN      __DIOTF (_DCMD_MISC, 0x200703, gpio_data_t) 
#define VT1211_CONFIG_PORT  __DIOT  (_DCMD_MISC, 0x200704, gpio_data_t) 
#define VT1211_SET_PORT     __DIOT  (_DCMD_MISC, 0x200705, gpio_data_t) 
#define VT1211_GET_PORT     __DIOTF (_DCMD_MISC, 0x200706, gpio_data_t) 

#define VT1211_PORT_1       0x00 //GP10...GP17
#define VT1211_PORT_3       0x01 //GP30...GP37
#define VT1211_PORT_4       0x02 //GP40...GP47
#define VT1211_PORT_5       0x03 //GP50...GP57
#define VT1211_PORT_6       0x04 //GP60...GP62

#define VT1211_PORT_INPUT   0x00
#define VT1211_PORT_OUTPUT  0xFF

#define VT1211_PIN_INPUT    0x1
#define VT1211_PIN_OUTPUT   0x0

typedef struct {
  uint8_t count;
} gpio_portinfo_t;

typedef struct {
  uint8_t port;
  uint8_t pin;
  uint8_t data;
} gpio_data_t;

