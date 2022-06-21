#ifndef MODEM_NF_H_
#define MODEM_NF_H_

#include <zephyr.h>
int modem_nf_reset(void);
int get_pdp_addr(char **);
int modem_nf_wakeup(void);
int modem_nf_sleep(void);
int set_socket_linger_time(int , int);
int get_ccid(char **);
#endif /* MODEM_NF_H_ */