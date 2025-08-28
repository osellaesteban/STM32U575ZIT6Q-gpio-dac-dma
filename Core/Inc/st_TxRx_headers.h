/*
 * st_TxRx_headers.h
 *
 *  Created on: Jun 6, 2025
 *      Author: osell
 */

#ifndef INC_ST_TXRX_HEADERS_H_
#define INC_ST_TXRX_HEADERS_H_

#include "st_definitions.h"

#define ST_START 0XAA
#define ST_STOP	 0X55


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


#pragma pack(push, 1) // Ensure byte-aligned packing

typedef struct {
    uint8_t start_byte;
    uint8_t msg_type;
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

#endif /* INC_ST_TXRX_HEADERS_H_ */
