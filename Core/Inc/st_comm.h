/*
 * st_comm.h
 *
 *  Line oriented serial port on the ST-LINK virtual COM port (USART1,
 *  initialized by BSP_COM_Init). Reception is interrupt driven into a ring
 *  buffer; transmission is blocking and must only be used from the main
 *  loop.
 */

#ifndef INC_ST_COMM_H_
#define INC_ST_COMM_H_

#include <stdint.h>

#define COMM_LINE_LENGTH	160

/**
 * Enables the reception interrupt. BSP_COM_Init must be called before.
 */
void stCommInit(void);

/**
 * Collects received characters. Returns 1 when a complete line ('\r' or
 * '\n' terminated, terminator removed) is stored in line, -1 if a line
 * longer than length was discarded and 0 otherwise.
 */
int8_t stCommReadLine(char *line, uint16_t length);

/**
 * Sends text followed by "\r\n".
 */
void stCommWriteLine(const char *text);

#endif /* INC_ST_COMM_H_ */
