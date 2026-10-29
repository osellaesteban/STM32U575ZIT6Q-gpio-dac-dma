/*
 * st_comm.h
 *
 *  Line oriented serial port on the ST-LINK virtual COM port (USART1,
 *  initialized by BSP_COM_Init). Reception and transmission are interrupt
 *  driven through ring buffers, so writing never blocks the main loop for
 *  long. Only the main loop may call these functions.
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
 * Queues text followed by "\r\n", waiting for free space if needed (used
 * for command responses, which must not be lost).
 */
void stCommWriteLine(const char *text);

/**
 * Queues text followed by "\r\n" only if there is space, otherwise the
 * line is dropped and counted. Returns 1 if queued.
 */
uint8_t stCommTryWriteLine(const char *text);

/**
 * Free bytes in the transmission buffer.
 */
uint16_t stCommTxFree(void);

/**
 * Lines dropped by stCommTryWriteLine.
 */
uint32_t stCommTxDropped(void);

#endif /* INC_ST_COMM_H_ */
