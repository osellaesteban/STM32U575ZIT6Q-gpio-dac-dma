/*
 * st_TxRx_headers.h
 *
 *  Created on: Jun 6, 2025
 *      Author: osell
 */

#ifndef INC_ST_TXRX_HEADERS_H_
#define INC_ST_TXRX_HEADERS_H_

#include "st_definitions.h"
#include "st_stimulator.h"
#define BUFF_SIZE	9710+16
#define ST_START 0XAA
#define ST_STOP	 0X55


typedef enum {
	ser_read_head = 0,
	ser_read_msg
	} ser_status_t;

typedef enum {
    CMD_SET_CONFIG = 0x01,
    CMD_GET_CONFIG = 0x02,
    CMD_START_STIM = 0x03,
    CMD_STOP_STIM = 0x04,
	CMD_SET_CHANNEL = 0X05,
    RESP_ACK = 0x06,
    RESP_NACK = 0x07,
    RESP_DATA = 0x08
} ProtocolCmd;


typedef enum {
    STATE_HEADER,
    STATE_PAYLOAD
} uart_state_t;


#pragma pack(push, 1) // Ensure byte-aligned packing

typedef struct {
    uint8_t start_byte;
    uint8_t msg_type; // command
    uint16_t length;
    // Payload follows
} msg_header_t;

typedef struct {
    uint8_t active_channels;
    uint8_t global_state;
    uint32_t period;
    uint32_t stim_port;
    uint32_t stim_Trigger_pin;
    uint32_t sign_pin;
    uint16_t reserved;
} global_header_t;

typedef struct {
    uint8_t active_state;
    uint32_t pin;
    uint16_t stimulus_length;
} channel_header_t;

#pragma pack(pop)


void st_Rx_DecodeHeader(uint8_t * buff,msg_header_t* head);
void st_Rx_DecodeMsg(uint8_t * buff);


#endif /* INC_ST_TXRX_HEADERS_H_ */
