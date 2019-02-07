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

#ifdef __USAGE
%C - driver for the VT1211 Super I/O chip

Options:
 -i   CIR   Configuration Index Register (hex). Default is 0x002E
 -d   CDR   Configuration Data Register (hex).  Default is 0x002F
 -p36 Ports 3..6 enable
 -v   Verbose
#endif
