
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>

struct data {};
struct sockaddr {};

uint8_t mock_cellular_controller_init();

uint8_t socket_receive(struct data *);

void stop_tcp(void);

int8_t send_tcp(char*, size_t);

int8_t socket_connect(struct data *, struct sockaddr *,
                      size_t);

int8_t lte_init(void);

bool lte_is_ready(void);

const struct device* bind_modem(void);

int eep_read_host_port(char *, size_t);

//int8_t cache_server_address(void);