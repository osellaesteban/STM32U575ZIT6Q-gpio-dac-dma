/*
 * st_protocol.h
 *
 *  ASCII command protocol, one command per line, one response line per
 *  command ("OK ..." or "ERR <reason>"). Keywords are case insensitive.
 *  See README.md for the command list.
 *
 *  This module has no hardware dependencies (it uses st_stimulator and
 *  the st_engine, st_monitor and st_comm APIs) so it can be tested on a PC.
 */

#ifndef INC_ST_PROTOCOL_H_
#define INC_ST_PROTOCOL_H_

#include <stdint.h>

/**
 * Executes a command line and writes the response (without line
 * terminator) in response. line is modified (tokenized).
 */
void stProtocolHandleLine(char *line, char *response, uint16_t length);

/**
 * Host watchdog in milliseconds, 0 = disabled. While stimulating, the
 * stimulation must be stopped if no command arrives within this time.
 */
uint32_t stProtocolGetWatchdog(void);

#endif /* INC_ST_PROTOCOL_H_ */
