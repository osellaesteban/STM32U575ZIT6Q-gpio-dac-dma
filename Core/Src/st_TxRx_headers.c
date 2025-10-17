/*
 * st_TxRx_headers.c
 *
 *  Created on: Sep 30, 2025
 *      Author: osell
 */


#include "st_TxRx_headers.h"

void st_Rx_DecodeHeader(uint8_t * buff,msg_header_t* head){
	head->start_byte = buff[0];
	head->msg_type = buff[1];
	head->length = (uint16_t) ((buff[3]<<8) | buff[2]);
}

void st_Rx_DecodeMsg(uint8_t * buff){

}

