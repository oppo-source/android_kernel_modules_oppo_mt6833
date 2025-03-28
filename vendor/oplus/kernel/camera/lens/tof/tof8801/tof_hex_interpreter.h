/*
********************************************************************************
* Copyright (C) 2021 ams AG                                                    *
*                                                                              *
* This program is free software; you can redistribute it and/or modify it      *
* under the terms of the GNU General Public License as published by the        *
* Free Software Foundation; version 2.                                         *
*                                                                              *
* This program is distributed in the hope that it will be useful, but          *
* WITHOUT ANY WARRANTY; without even the implied warranty of                   *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General     *
* Public License for more details.                                             *
*                                                                              *
* You should have received a copy of the GNU General Public License along      *
* with this program; if not, write to the Free Software Foundation, Inc.,      *
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.                 *
********************************************************************************
*/

#ifndef INTEL_HEX_INTERPRETER_H
#define INTEL_HEX_INTERPRETER_H

#include "tof8801_bootloader.h"

/* intel hex defines ---------------------------------------------------------- */

/* supported intel hex record types: */
#define INTEL_HEX_TYPE_DATA                     0
#define INTEL_HEX_TYPE_EOF                      1
#define INTEL_HEX_TYPE_EXT_LIN_ADDR             4
#define INTEL_HEX_TYPE_START_LIN_ADDR           5


/* return codes: negative numbers are errors */
#define INTEL_HEX_EOF                   1       /* end of file -> reset */
#define INTEL_HEX_CONTINUE              0       /* continue reading in */
#define INTEL_HEX_ERR_NOT_A_NUMBER      -1
#define INTEL_HEX_ERR_TOO_SHORT         -2
#define INTEL_HEX_ERR_CRC_ERR           -3
#define INTEL_HEX_ERR_UNKNOWN_TYPE      -4
#define INTEL_HEX_WRITE_FAILED          -5


/* get the ULBA from a 32-bit address */
#define INTEL_HEX_ULBA( adr)        ((adr) & 0xFFFF0000UL)

/* an intel hex record always has at least:
    :llaaaattcc
   l= length, a= address, t= type, c= crc
   so we need at least 11 ascii uint8_tacters for 1 regular record */
#define INTEL_HEX_MIN_RECORD_SIZE   11
/* the lineLength is at the beginning not the length but the last written
   address. To make out of this the length we substract instead
   of the min_record_size the min_last_address. */
#define INTEL_HEX_MIN_LAST_ADDRESS  ((INTEL_HEX_MIN_RECORD_SIZE) - 1)

/* an intel hex record has a length field in that only 256 = an 8-bit number
 * can be represented, so we can limit the data buffer to half the size,
 * as it converts from ascii to binary */
#define INTEL_HEX_MAX_RECORD_DATA_SIZE  (128)


/*
 *****************************************************************************
 * FUNCTIONS
 *****************************************************************************
 */

void intelHexInterpreterInitialise(void);

/* hand in the line of intel hex record, returns any of the above
 * return codes (1 == reset of tof done) */
int intelHexHandleRecord(void *, struct tof8801_BL_application *, uint8_t lineLength, const uint8_t * line, uint8_t verify);

#endif /* INTEL_HEX_INTERPRETER_H */

