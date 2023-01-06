
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>

struct data {};

struct sockaddr {};

uint8_t mock_cellular_controller_init();

uint8_t socket_receive(struct data *, char **);

int reset_modem(void);

int stop_tcp(void);

int8_t send_tcp(char *, size_t);

void send_tcp_q(char *, size_t);

int8_t socket_connect(struct data *, struct sockaddr *, size_t);

int socket_listen(struct data *, uint16_t);

int8_t lte_init(void);

bool lte_is_ready(void);

const struct device *bind_modem(void);

int check_ip(void);

int get_ip(char **);

void send_tcp_fn(void);

bool query_listen_sock(void);
