/*
 * SerialICE 
 *
 * Copyright (C) 2009 coresystems GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#define ECHO_MODE	1
#define SIO_SPEED	115200
#define SIO_PORT	0x3f8

#define VIA_ROMSTRAP	0
#define HAVE_SSE	1       /* Set to 0 for CPUs without SSE. */

#define MAINBOARD	"kontron_986lcd-m.c"
//#define MAINBOARD	"intel_d945gclf.c"
//#define MAINBOARD	"asus_m2v-mx_se.c"
//#define MAINBOARD	"msi_ms6178.c"
//#define MAINBOARD	"rca_rm4100.c"
//#define MAINBOARD	"thomson_ip1000.c"
