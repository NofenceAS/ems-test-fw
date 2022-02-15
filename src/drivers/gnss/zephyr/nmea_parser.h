
#ifndef NMEA_PARSER_H_
#define NMEA_PARSER_H_

/* NOT IMPLEMENTED */

#define NMEA_MAX_SIZE		82
#define NMEA_START_DELIMITER	'$'
#define NMEA_END_DELIMITER	"\r\n"

#if CONFIG_GNSS_NMEA_PARSER
uint32_t nmea_parse(uint8_t* char, uint32_t size);
#else
#define nmea_parse(x,y) (y)
#endif

#endif /* NMEA_PARSER_H_ */