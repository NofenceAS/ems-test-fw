
#ifndef NMEA_PARSER_H_
#define NMEA_PARSER_H_

/* NOT IMPLEMENTED, ONLY USED FOR GRACEFULLY DISCARDING */

#include <string.h>

#define NMEA_MAX_SIZE		82
#define NMEA_START_DELIMITER	'$'
#define NMEA_END_DELIMITER	"\r\n"
#define NMEA_END_DELIMITER_LEN	2

#if CONFIG_GNSS_NMEA_PARSER

/**
 * @brief Parses buffer as NMEA data. 
 *
 * @param[in] data Pointer to buffer containing data to parse.
 * @param[in] size Number of bytes in buffer.
 *
 * @return Number of bytes parsed/consumed. 
 */
uint32_t nmea_parse(uint8_t* data, uint32_t size);

#else
/* Use dummy parser for NMEA data when not enabled. 
 * This will look for start and end to gracefully ignore data, 
 * but not parse it. */

static inline uint32_t nmea_parse(uint8_t* data, uint32_t size)
{
	uint32_t parsed = 0;
	for (uint32_t i = 0; i < size; i++) {
		if (i > NMEA_MAX_SIZE) {
			/* Exceeded max size, ignore max and stop parsing. */
			parsed = NMEA_MAX_SIZE;
			break;
		}

		if ((size-i) >= NMEA_END_DELIMITER_LEN) {
			if (strncmp(&data[i], NMEA_END_DELIMITER, 
					     NMEA_END_DELIMITER_LEN)) {
				/* Found end delimiter, ignore data (including 
				 * end) and stop parsing. */
				parsed = i + NMEA_END_DELIMITER_LEN;
				break;
			}
		}
	}
	return parsed;
}
#endif

#endif /* NMEA_PARSER_H_ */