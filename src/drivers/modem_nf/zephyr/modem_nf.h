#ifndef MODEM_NF_H_
#define MODEM_NF_H_

#include <zephyr.h>

int modem_nf_reset(void);
int get_pdp_addr(char **);
#endif /* MODEM_NF_H_ */