//
// Created by alaa on 04.04.2022.
//

#include "eeprom.h"
#include <ztest.h>

int eep_read_serial(uint32_t *p_serial){
	ARG_UNUSED(p_serial);
	return ztest_get_return_value();
}
