
#include <zephyr.h>
//#include "cellular_helpers_header.h"

struct data {};
struct sockaddr {};

uint8_t mock_cellular_controller_init();

uint8_t socket_receive(struct data *);

void stop_tcp(void);

int8_t send_tcp(char*, size_t);

int8_t socket_connect(struct data *, struct sockaddr *,
                      size_t);

int8_t lteInit(void);

bool lteIsReady(void);
//uint8_t receive_tcp(struct data *);