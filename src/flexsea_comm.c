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
	[This file] flexsea_comm: Data-Link layer of the FlexSEA protocol
*****************************************************************************
	[Change log] (Convention: YYYY-MM-DD | author | comment)
	* 2016-09-09 | jfduval | Initial GPL-3.0 release
	*
****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

//FlexSEA comm. prototype:
//=======================
//[HEADER][# of BYTES][DATA...][CHECKSUM][FOOTER]
//=> Number of bytes includes the ESCAPE bytes
//=> Checksum is done on the payload (data + ESCAPEs) and on the BYTES byte.

//To transmit a message:
//======================
// 1) Place the payload in an array (no header, no footer: pure data)
// 2) Call comm_gen_str(your_data_array, number_of_bytes)
// 2b) It will return the index of the last byte of the message (add 1 for the length)
// 2c) The message is in comm_str[]
// 3) Send comm_str[] (x_puts(comm_str, msg_length));

//To receive a message:
//=====================
// 1) Assuming that you have dealt with all the previous messages, call comm_str_payload();
//    to fill the buffer with zeros
// 2) Every time you receive a byte update the buffer: comm_update_rx_buffer(your_new_byte);
// 3) Call payload_str_available_in_buffer = comm_decode_str(). If you get >= 1, read the
//    comm_str_payload buffer and do something with the data!
// 4) At this point you might want to flush the read payload from rx_buf

//****************************************************************************
// Include(s)
//****************************************************************************
#include "flexsea_buffers.h"
#include <string.h>
#include <stdlib.h>
#include "../inc/flexsea.h"
#include "flexsea_board.h"
#include "flexsea_system.h"
#include <flexsea_comm.h>

//****************************************************************************
// Variable(s)
//****************************************************************************

uint8_t comm_str_tmp[COMM_STR_BUF_LEN];

#ifdef ENABLE_FLEXSEA_BUF_1
uint8_t comm_str_1[COMM_STR_BUF_LEN];
uint8_t rx_command_1[PACKAGED_PAYLOAD_LEN];
#endif	//ENABLE_FLEXSEA_BUF_1

#ifdef ENABLE_FLEXSEA_BUF_2
uint8_t comm_str_2[COMM_STR_BUF_LEN];
uint8_t rx_command_2[PACKAGED_PAYLOAD_LEN];
#endif	//ENABLE_FLEXSEA_BUF_2

#ifdef ENABLE_FLEXSEA_BUF_3
uint8_t comm_str_3[COMM_STR_BUF_LEN];
uint8_t rx_command_3[PACKAGED_PAYLOAD_LEN];
#endif	//ENABLE_FLEXSEA_BUF_3

#ifdef ENABLE_FLEXSEA_BUF_4
uint8_t comm_str_4[COMM_STR_BUF_LEN];
uint8_t rx_command_4[PACKAGED_PAYLOAD_LEN];
#endif	//ENABLE_FLEXSEA_BUF_4

#ifdef ENABLE_FLEXSEA_BUF_5
uint8_t comm_str_5[COMM_STR_BUF_LEN];
uint8_t rx_command_5[PACKAGED_PAYLOAD_LEN];
#endif	//ENABLE_FLEXSEA_BUF_5

uint32_t cmd_valid = 0;
uint32_t cmd_bad_checksum = 0;

struct comm_s slaveComm[COMM_SLAVE_BUS];
struct comm_s masterComm[COMM_MASTERS];

struct commSpy_s commSpy1 = {0,0,0,0,0,0,0};

MsgQueue unpacked_packet_queue;
MsgQueue usb_reply_queue;

//****************************************************************************
// Private Function Prototype(s):
//****************************************************************************



//****************************************************************************
// Public Function(s)
//****************************************************************************

//Takes payload, adds ESCAPES, checksum, header, ...
uint8_t comm_gen_str(uint8_t payload[], uint8_t *cstr, uint8_t bytes)
{
	unsigned int i = 0, escapes = 0, idx = 0, total_bytes = 0;
	uint8_t checksum = 0;

	//Fill comm_str with known values ('a')
	memset(cstr, 0xAA, COMM_STR_BUF_LEN);

	//Fill comm_str with payload and add ESCAPE characters
	escapes = 0;
	idx = 2;
	for(i = 0; i < bytes; i++)
	{
		if ((payload[i] == HEADER) || (payload[i] == FOOTER) || (payload[i] == ESCAPE))
		{
			escapes = escapes + 1;
			cstr[idx] = ESCAPE;
			cstr[idx+1] = payload[i];
			idx = idx + 1;
		}
		else
		{
			cstr[idx] = payload[i];
		}
		idx++;
	}

	total_bytes = bytes + escapes;

	commSpy1.bytes = bytes;
	commSpy1.escapes = (uint8_t) escapes;
	commSpy1.total_bytes = (uint8_t) total_bytes;
	commSpy1.error++;

	//String length?
	if(total_bytes >= COMM_STR_BUF_LEN)
	{
		//Too long, abort:
		memset(cstr, 0, COMM_STR_BUF_LEN);	//Clear string
		commSpy1.retVal = 0;
		return 0;
	}

	//Checksum:
	checksum = 0;
	for (i = 0; i < total_bytes; i++)
	{
		checksum = checksum + cstr[2+i];
	}

	commSpy1.checksum = checksum;

	//Build comm_str:
	cstr[0] = HEADER;
	cstr[1] = total_bytes;
	cstr[2 + total_bytes] = checksum;
	cstr[3 + total_bytes] = FOOTER;

	//Return the length of the valid data
	commSpy1.retVal = 3 + (uint8_t)total_bytes;
	return (3 + total_bytes);
}

//To avoid sharing buffers in multiple files we use specific functions:

#ifdef ENABLE_FLEXSEA_BUF_1
int8_t unpack_payload_1(void)
{
	return unpack_payload(rx_buf_1, rx_command_1);
}
#endif	//ENABLE_FLEXSEA_BUF_1

#ifdef ENABLE_FLEXSEA_BUF_2
int8_t unpack_payload_2(void)
{
	return unpack_payload(rx_buf_2, rx_command_2);
}
#endif	//ENABLE_FLEXSEA_BUF_2

#ifdef ENABLE_FLEXSEA_BUF_3
int8_t unpack_payload_3(void)
{
	return unpack_payload(rx_buf_3, rx_command_3);
}
#endif	//ENABLE_FLEXSEA_BUF_3

#ifdef ENABLE_FLEXSEA_BUF_4
int8_t unpack_payload_4(void)
{
	return unpack_payload(rx_buf_4, rx_command_4);
}
#endif	//ENABLE_FLEXSEA_BUF_4

#ifdef ENABLE_FLEXSEA_BUF_5
int8_t unpack_payload_5(void)
{
	return unpack_payload(rx_buf_5, rx_command_5);
}
#endif	//ENABLE_FLEXSEA_BUF_5

//Special wrapper for unit test code:
int8_t unpack_payload_test(uint8_t *buf, uint8_t rx_cmd[PACKAGED_PAYLOAD_LEN])
{
	return unpack_payload(buf, rx_cmd);
}

void initRandomGenerator(int seed)
{
	srand(seed);
}

uint8_t generateRandomUint8(void)
{
	int r = rand();
	return (uint8_t)(r % 255);
}

void generateRandomUint8Array(uint8_t *arr, uint8_t size)
{
	int i = 0;

	for(i = 0; i < size; i++)
	{
		arr[i] = generateRandomUint8();
	}
}

//****************************************************************************
// Private Function(s)
//****************************************************************************

//New version of comm_decode_str
//Take a buffer as an argument, returns the number of decoded payload packets
//ToDo: The error codes are not always right, but if it's < 0 you know it didn't
//find a valid string
int8_t unpack_payload(uint8_t *buf, uint8_t rx_cmd[PACKAGED_PAYLOAD_LEN])
{
	uint32_t i = 0, j = 0, k = 0, idx = 0, h = 0;
	uint32_t bytes = 0, possible_footer = 0, possible_footer_pos = 0;
	uint8_t checksum = 0, skip = 0, payload_strings = 0;
	uint8_t rx_buf_tmp[RX_BUF_LEN];
	uint8_t foundHeader = 0;
	int8_t tmpRetVal = 0;

	memset(rx_buf_tmp, 0, RX_BUF_LEN);

	for(i = 0; i < (RX_BUF_LEN - 2); i++)
	{
		if(buf[i] == HEADER)
		{
			foundHeader++;
			bytes = buf[i+1];
			possible_footer_pos = i+3+bytes;

			if(possible_footer_pos <= RX_BUF_LEN)
			{
				//We have enough bytes for a full string
				possible_footer = buf[possible_footer_pos];
				if(possible_footer == FOOTER)
				{
					//Correctly framed string
					k = 0;
					for(j = i; j <= possible_footer_pos; j++)
					{
						//Copy string in temp buffer
						rx_buf_tmp[k] = buf[j];
						k++;
					}

					//Is the checksum OK?
					checksum = 0;
					for (k = 0; k < bytes; k++)
					{
						checksum = checksum + rx_buf_tmp[2+k];
					}

					if(checksum == rx_buf_tmp[2+bytes])
					{
						//Now we de-escap and de-frame to get the payload
						idx = 0;
						skip = 0;
						for(k = 2; k < (unsigned int)(2+bytes); k++)
						{
							if(((rx_buf_tmp[k] == HEADER) || \
								(rx_buf_tmp[k] == FOOTER) || \
								(rx_buf_tmp[k] == ESCAPE)) && skip == 0)
							{
								skip = 1;
							}
							else
							{
								skip = 0;
								rx_cmd[idx] = rx_buf_tmp[k];
								idx++;
							}
						}

						//At this point we have extracted a valid string
						payload_strings++;
						cmd_valid++;
						return 1;

						//Remove the string to avoid double detection
						for(h = i; h <= possible_footer_pos; h++)
						{
							buf[h] = 0;
						}

						tmpRetVal = payload_strings;
					}
					else
					{
						//Remove the string to avoid double detection
						for(h = i; h <= possible_footer_pos; h++)
						{
							buf[h] = 0;
						}

						cmd_bad_checksum++;

						tmpRetVal = UNPACK_ERR_CHECKSUM;
					}
				}
				else
				{
					tmpRetVal = UNPACK_ERR_FOOTER;
				}
			}
			else
			{
				tmpRetVal = UNPACK_ERR_LEN;
			}
		}
	}

	if(payload_strings > 0)
	{
		//Returns the number of decoded strings
		return payload_strings;
	}

	if(tmpRetVal != 0)
	{
		return tmpRetVal;
	}

	if(!foundHeader)
	{
		//Error - return with error code:
		return UNPACK_ERR_HEADER;
	}

	//Default
	return 0;
}

#ifdef __cplusplus
}
#endif
