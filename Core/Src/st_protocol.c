/*
 * st_protocol.c
 *
 *  See st_protocol.h and README.md.
 */
#include <stdio.h>
#include <string.h>

#include "st_protocol.h"
#include "st_stimulator.h"
#include "st_engine.h"
#include "st_monitor.h"
#include "st_comm.h"

#define MAX_TOKENS		(MAX_SLOTS + 2)

/** Private variables */
static uint32_t watchdogMs;

static const char *const polarityNames[] = {"POS", "NEG", "ALT", "BIPH"};


/** Private functions */

static int keywordIs(const char *token, const char *keyword){
	while ((*token != '\0') && (*keyword != '\0')){
		char c = *token;
		if ((c >= 'a') && (c <= 'z'))
			c = (char)(c - 'a' + 'A');
		if (c != *keyword)
			return 0;
		token++;
		keyword++;
	}
	return (*token == '\0') && (*keyword == '\0');
}


/**
 * Strict unsigned decimal parser. Returns 0 if token is not a number
 * or is greater than max.
 */
static int parseUint(const char *token, uint32_t max, uint32_t *value){
	uint32_t result = 0;
	if (*token == '\0')
		return 0;
	while (*token != '\0'){
		uint32_t digit;
		if ((*token < '0') || (*token > '9'))
			return 0;
		digit = (uint32_t)(*token - '0');
		if ((digit > max) || (result > (max - digit) / 10U))
			return 0;
		result = result * 10U + digit;
		token++;
	}
	*value = result;
	return 1;
}


static uint8_t tokenize(char *line, char **tokens){
	uint8_t count = 0;
	char *p = line;
	while (*p != '\0'){
		while ((*p == ' ') || (*p == '\t') || (*p == ','))
			*p++ = '\0';
		if (*p == '\0')
			break;
		if (count == MAX_TOKENS)
			return MAX_TOKENS + 1;	// too many arguments
		tokens[count++] = p;
		while ((*p != '\0') && (*p != ' ') && (*p != '\t') && (*p != ','))
			p++;
	}
	return count;
}


static const char *statusName(st_status_t status){
	switch (status){
	case ST_OK: return "OK";
	case ST_ERR_CHANNEL: return "ERR CHANNEL";
	case ST_ERR_RANGE: return "ERR RANGE";
	case ST_ERR_TIMING: return "ERR TIMING";
	case ST_ERR_BUSY: return "ERR BUSY";
	case ST_ERR_UNAVAILABLE: return "ERR UNAVAILABLE";
	case ST_ERR_ARG:
	default: return "ERR ARG";
	}
}


static void reply(char *response, uint16_t length, st_status_t status){
	snprintf(response, length, "%s", statusName(status));
}


static st_status_t channelCommand(char **tokens, uint8_t count, char *response, uint16_t length){
	uint32_t ch, value;
	uint8_t it;

	if (!parseUint(tokens[1], 255, &ch))
		return ST_ERR_ARG;
	if (ch >= CHAN_LENGTH)
		return ST_ERR_CHANNEL;

	if (count == 2){
		const st_channel_t *channel = stGetChannel((uint8_t)ch);
		snprintf(response, length, "OK CH=%u EN=%u AMP=%u POL=%s LABEL=%s",
				(unsigned)ch, (unsigned)(channel->stActiveState == st_enabled),
				(unsigned)channel->stAmplitude, polarityNames[channel->stPolarity],
				channel->stLabel);
		return ST_OK;
	}
	if (count != 4)
		return ST_ERR_ARG;

	if (keywordIs(tokens[2], "EN")){
		if (!parseUint(tokens[3], 1, &value))
			return ST_ERR_ARG;
		return stSetChannelState((uint8_t)ch, value ? st_enabled : st_disabled);
	}
	if (keywordIs(tokens[2], "AMP")){
		if (!parseUint(tokens[3], 0xFFFF, &value))
			return ST_ERR_ARG;
		return stSetChannelAmplitude((uint8_t)ch, (uint16_t)value);
	}
	if (keywordIs(tokens[2], "POL")){
		for (it = 0; it < (sizeof(polarityNames) / sizeof(polarityNames[0])); it++){
			if (keywordIs(tokens[3], polarityNames[it]))
				return stSetChannelPolarity((uint8_t)ch, (st_polarity_t)it);
		}
		return ST_ERR_ARG;
	}
	if (keywordIs(tokens[2], "LABEL"))
		return stSetChannelLabel((uint8_t)ch, tokens[3]);
	return ST_ERR_ARG;
}


/**
 * AMPS a0 a1 ... / ENS e0 e1 ...: sets the first channels at once. All the
 * values are checked before applying any of them.
 */
static st_status_t bulkCommand(char **tokens, uint8_t count, uint8_t amplitudes){
	uint32_t values[CHAN_LENGTH];
	uint32_t max = amplitudes ? AMP_MAX : 1;
	uint8_t it;

	if ((count < 2) || ((count - 1) > CHAN_LENGTH))
		return ST_ERR_ARG;
	for (it = 1; it < count; it++){
		if (!parseUint(tokens[it], 0xFFFF, &values[it - 1]))
			return ST_ERR_ARG;
		if (values[it - 1] > max)
			return ST_ERR_RANGE;
	}
	for (it = 0; it < (count - 1); it++){
		if (amplitudes)
			stSetChannelAmplitude(it, (uint16_t)values[it]);
		else
			stSetChannelState(it, values[it] ? st_enabled : st_disabled);
	}
	return ST_OK;
}


static void getCommand(char *response, uint16_t length){
	const st_config_t *config = stGetConfig();
	int written;
	uint8_t it;

	written = snprintf(response, length,
			"OK PERIOD=%lu SETTLE=%u TRIG=%u DELAY=%u PW=%u TAIL=%u DACFS=%u SEQ=",
			(unsigned long)config->stPeriod, (unsigned)config->stSettle,
			(unsigned)config->stTrigger, (unsigned)config->stDs8rDelay,
			(unsigned)config->stPulse, (unsigned)config->stTail,
			(unsigned)config->stDacFullScale);
	for (it = 0; (it < config->stSeqLength) && (written > 0) && (written < (int)length); it++)
		written += snprintf(&response[written], (size_t)(length - written), it ? ",%u" : "%u",
				(unsigned)config->stSequence[it]);
}


static st_status_t configCommand(char **tokens, uint8_t count){
	uint32_t values[4];
	uint8_t sequence[MAX_SLOTS];
	uint8_t it;

	if (keywordIs(tokens[0], "PERIOD")){
		if ((count != 2) || !parseUint(tokens[1], 0xFFFFFFFFU, &values[0]))
			return ST_ERR_ARG;
		return stSetPeriod(values[0]);
	}
	if (keywordIs(tokens[0], "SEQ")){
		if ((count < 2) || ((count - 1) > MAX_SLOTS))
			return ST_ERR_ARG;
		for (it = 1; it < count; it++){
			if (!parseUint(tokens[it], 255, &values[0]))
				return ST_ERR_ARG;
			sequence[it - 1] = (uint8_t)values[0];
		}
		return stSetSequence(sequence, (uint8_t)(count - 1));
	}
	if (keywordIs(tokens[0], "TIMING")){
		if (count != 5)
			return ST_ERR_ARG;
		for (it = 0; it < 4; it++){
			if (!parseUint(tokens[it + 1], 0xFFFF, &values[it]))
				return ST_ERR_ARG;
		}
		return stSetTiming((uint16_t)values[0], (uint16_t)values[1], (uint16_t)values[2], (uint16_t)values[3]);
	}
	if (keywordIs(tokens[0], "PW")){
		if ((count != 2) || !parseUint(tokens[1], 0xFFFF, &values[0]))
			return ST_ERR_ARG;
		return stSetPulseWidth((uint16_t)values[0]);
	}
	if (keywordIs(tokens[0], "DACFS")){
		if ((count != 2) || !parseUint(tokens[1], 0xFFFF, &values[0]))
			return ST_ERR_ARG;
		return stSetDacFullScale((uint16_t)values[0]);
	}
	if (keywordIs(tokens[0], "DEFAULT")){
		if (count != 1)
			return ST_ERR_ARG;
		stConfigureDefault();
		return ST_OK;
	}
	return ST_ERR_ARG;
}


/** Public functions */

void stProtocolHandleLine(char *line, char *response, uint16_t length){
	char *tokens[MAX_TOKENS];
	uint8_t count = tokenize(line, tokens);
	uint32_t value;

	if ((count == 0) || (count > MAX_TOKENS)){
		reply(response, length, ST_ERR_ARG);
		return;
	}

	/* Commands allowed at any time */
	if (keywordIs(tokens[0], "PING")){
		snprintf(response, length, "OK PONG");
	}
	else if (keywordIs(tokens[0], "INFO")){
		snprintf(response, length, "OK STIMMUX FW=%s CH=%u SLOTS=%u AMPMAX=%u PWMIN=%u PWMAX=%u MININT=%u",
				ST_FW_VERSION, (unsigned)CHAN_LENGTH, (unsigned)MAX_SLOTS, (unsigned)AMP_MAX,
				(unsigned)DS8R_PW_MIN_US, (unsigned)DS8R_PW_MAX_US, (unsigned)DS8R_MIN_TRIG_INTERVAL_US);
	}
	else if (keywordIs(tokens[0], "STATUS")){
		snprintf(response, length, "OK RUN=%u FRAMES=%lu WDOG=%lu MON=%u TRIM=%lu MLOST=%lu TXDROP=%lu MSYNC=%u",
				(unsigned)stEngineIsRunning(), (unsigned long)stEngineFrameCount(),
				(unsigned long)watchdogMs, (unsigned)stMonitorGetReports(),
				(unsigned long)stMonitorGetTrim(), (unsigned long)stMonitorLost(),
				(unsigned long)stCommTxDropped(), (unsigned)stMonitorSyncError());
	}
	/* Current monitor, allowed at any time */
	else if (keywordIs(tokens[0], "MON")){
		if ((count != 2) || !parseUint(tokens[1], 1, &value))
			reply(response, length, ST_ERR_ARG);
		else{
			stMonitorSetReports((uint8_t)value);
			reply(response, length, ST_OK);
		}
	}
	else if (keywordIs(tokens[0], "MONTRIM")){
		if ((count != 2) || !parseUint(tokens[1], 0xFFFF, &value))
			reply(response, length, ST_ERR_ARG);
		else
			reply(response, length, stMonitorSetTrim(value));
	}
	else if (keywordIs(tokens[0], "WAVE")){
		if (count == 1)
			reply(response, length, stMonitorRequestWave(-1));
		else if ((count == 2) && parseUint(tokens[1], 255, &value))
			reply(response, length, stMonitorRequestWave((int16_t)value));
		else
			reply(response, length, ST_ERR_ARG);
	}
	else if (keywordIs(tokens[0], "GET")){
		getCommand(response, length);
	}
	else if (keywordIs(tokens[0], "STOP")){
		stEngineStop();
		reply(response, length, ST_OK);
	}
	else if (keywordIs(tokens[0], "RUN")){
		value = 0;
		if ((count > 2) || ((count == 2) && !parseUint(tokens[1], 0xFFFFFFFFU, &value)))
			reply(response, length, ST_ERR_ARG);
		else
			reply(response, length, stEngineStart(value));
	}
	else if (keywordIs(tokens[0], "WDOG")){
		if ((count != 2) || !parseUint(tokens[1], 600000U, &value))
			reply(response, length, ST_ERR_ARG);
		else{
			watchdogMs = value;
			reply(response, length, ST_OK);
		}
	}
	/* Real time commands, allowed while stimulating */
	else if (keywordIs(tokens[0], "CH")){
		st_status_t status = (count >= 2) ? channelCommand(tokens, count, response, length) : ST_ERR_ARG;
		if ((status != ST_OK) || (count != 2))
			reply(response, length, status);
	}
	else if (keywordIs(tokens[0], "AMPS")){
		reply(response, length, bulkCommand(tokens, count, 1));
	}
	else if (keywordIs(tokens[0], "ENS")){
		reply(response, length, bulkCommand(tokens, count, 0));
	}
	/* Frame configuration, only while stopped (it is copied at start) */
	else if (keywordIs(tokens[0], "PERIOD") || keywordIs(tokens[0], "SEQ") ||
			 keywordIs(tokens[0], "TIMING") || keywordIs(tokens[0], "PW") ||
			 keywordIs(tokens[0], "DACFS") || keywordIs(tokens[0], "DEFAULT")){
		if (stEngineIsRunning())
			reply(response, length, ST_ERR_BUSY);
		else
			reply(response, length, configCommand(tokens, count));
	}
	else{
		snprintf(response, length, "ERR UNKNOWN");
	}
}


uint32_t stProtocolGetWatchdog(void){
	return watchdogMs;
}
