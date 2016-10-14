/****************************************************************************
	[Project] FlexSEA: Flexible & Scalable Electronics Architecture
	[Sub-project] 'flexsea-comm' Communication stack
	Copyright (C) 2016 Dephy, Inc. <http://dephy.com/>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************
	[Lead developper] Jean-Francois (JF) Duval, jfduval at dephy dot com.
	[Origin] Based on Jean-Francois Duval's work at the MIT Media Lab
	Biomechatronics research group <http://biomech.media.mit.edu/>
	[Contributors]
*****************************************************************************
	[This file] flexsea_payload: deals with the "intelligent" data packaged
	in a comm_str
*****************************************************************************
	[Change log] (Convention: YYYY-MM-DD | author | comment)
	* 2016-09-09 | jfduval | Initial GPL-3.0 release
	*
****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

//****************************************************************************
// Include(s)
//****************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include "../inc/flexsea.h"
#include "../../flexsea-system/inc/flexsea_system.h"
#include "flexsea_board.h"

//****************************************************************************
// Variable(s)
//****************************************************************************

uint8_t payload_str[PAYLOAD_BUF_LEN];

//****************************************************************************
// Private Function Prototype(s):
//****************************************************************************

static uint8_t get_rid(uint8_t *pldata);
static void route_to_slave(uint8_t port, uint8_t *buf, uint32_t len);

//****************************************************************************
// Public Function(s):
//****************************************************************************

//Decode/parse received string
//ToDo fix: for now, supports only one command per string
unsigned int payload_parse_str(uint8_t *cp_str)
{
	unsigned char cmd = 0, cmd_7bits = 0, output = PARSE_SUCCESSFUL, numb  = 0;
	unsigned int id = 0;

	//Command
	cmd = cp_str[P_CMD1];		//CMD w/ R/W bit
	cmd_7bits = CMD_7BITS(cmd);	//CMD code, no R/W information

	//First, get RID code
	id = get_rid(cp_str);
	if(id == ID_MATCH)
	{
		//It's addressed to me. Function pointer array will call
		//the appropriate handler (as defined in flexsea_system):
		(*flexsea_payload_ptr[cmd_7bits]) (cp_str);

		return PARSE_SUCCESSFUL;
	}
	else if(id == ID_SUB1_MATCH)
	{
		//For a slave on bus #1:

		route_to_slave(PORT_SUB1, cp_str, PAYLOAD_BUF_LEN);
		//ToDo compute length rather then sending the max
	}
	else if(id == ID_SUB2_MATCH)
	{
		//For a slave on bus #2:

		route_to_slave(PORT_SUB2, cp_str, PAYLOAD_BUF_LEN);
		//ToDo compute length rather then sending the max
	}
	else if(id == ID_UP_MATCH)
	{
		//For my master:

		#ifdef BOARD_TYPE_FLEXSEA_MANAGE

		//Manage is the only board that can receive a package destined to his master

		//Repackages the payload. ToDo: would be more efficient to just resend the comm_str
		numb = comm_gen_str(cp_str, comm_str_usb, PAYLOAD_BUF_LEN);		//ToDo: shouldn't be fixed at spi or usb
		numb = COMM_STR_BUF_LEN;    //Fixed length for now
		flexsea_send_serial_master(PORT_USB, comm_str_usb, numb);	//Same comment here - ToDo fix
		//(the SPI driver will grab comm_str_spi directly)

		#endif	//BOARD_TYPE_FLEXSEA_MANAGE
	}
	else
	{
		return PARSE_ID_NO_MATCH;
	}

	//Shouldn't get here...
	return PARSE_DEFAULT;
}


//Start a new payload string
void prepare_empty_payload(uint8_t from, uint8_t to, uint8_t *buf, uint32_t len)
{
	//Start fresh:
	fill_uint8_buf(buf, len, 0);

	//Addresses:
	buf[P_XID] = from;
	buf[P_RID] = to;
}

//Add a buffer at the end of a partially filled payload buffer
//Payload is the partially filled buffer, idx is the next position to use, new_data is.. well, the new bytes
//and len is the number of bytes you want to add
uint32_t append_to_payload(uint8_t *payload, uint32_t idx, uint8_t *new_data, uint32_t len)
{
	uint32_t i = 0, cnt = 0;

	//Append the new data:
	for(i = idx; i < (idx+len); i++)
	{
		payload[i] = new_data[cnt++];
	}

	//Index for the next call:
	return i;
}

//Returns one if it was sent from a slave, 0 otherwise
uint8_t sent_from_a_slave(uint8_t *buf)
{
	//Slaves have higher addresses than their master.
	if(buf[P_XID] > buf[P_RID])
	{
		//Slave
		return 1;
	}
	else
	{
		//Master
		return 0;
	}

	//Should not happen
	return 0;
}

//****************************************************************************
// Private Function(s):
//****************************************************************************

//ToDo not the greatest function...
static void route_to_slave(uint8_t port, uint8_t *buf, uint32_t len)
{
	uint32_t numb = 0 , i  = 0;
	uint8_t *comm_str_ptr = slaves_485_1.xmit.str;

	#ifdef BOARD_TYPE_FLEXSEA_MANAGE

	//Repackages the payload. ToDo: would be more efficient to just resend the comm_str
	numb = comm_gen_str(buf, comm_str_tmp, len);
	//numb = COMM_STR_BUF_LEN;    //Fixed length for now

	//Port specific flags and buffer:
	if(port == PORT_RS485_1)
	{
		comm_str_ptr = slaves_485_1.xmit.str;
		slaves_485_1.xmit.cmd = buf[P_CMD1];
		slaves_485_1.xmit.flag = 1;
	}
	else if(port == PORT_RS485_2)
	{
		comm_str_ptr = slaves_485_2.xmit.str;
		slaves_485_2.xmit.cmd = buf[P_CMD1];
		slaves_485_2.xmit.flag = 1;
	}

	//Copy string:
	for(i = 0; i < numb+1; i++)
	{
		comm_str_ptr[i] = comm_str_tmp[i];
	}

	#endif 	//BOARD_TYPE_FLEXSEA_MANAGE
}

//Is it addressed to me? To a board "below" me? Or to my Master?
static uint8_t get_rid(uint8_t *pldata)
{
	uint8_t cp_rid = pldata[P_RID];
	uint8_t i = 0;

	if(cp_rid == board_id)				//This board?
	{
		return ID_MATCH;
	}
	else if(cp_rid == board_up_id)		//Master?
	{
		return ID_UP_MATCH;
	}
	else
	{
		//Can be on a slave bus, or can be invalid.

		//Search on slave bus #1:
		for(i = 0; i < SLAVE_BUS_1_CNT; i++)
		{
			if(cp_rid == board_sub1_id[i])
			{
				return ID_SUB1_MATCH;
			}
		}

		//Then on bus #2:
		for(i = 0; i < SLAVE_BUS_1_CNT; i++)
		{
			if(cp_rid == board_sub2_id[i])
			{
				return ID_SUB2_MATCH;
			}
		}
	}

	//If we end up here it's because we didn't get a match:
	return ID_NO_MATCH;
}

#ifdef __cplusplus
}
#endif
