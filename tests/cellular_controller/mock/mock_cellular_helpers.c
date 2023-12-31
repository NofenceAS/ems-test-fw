#include <ztest.h>
#include <mock_cellular_helpers.h>

extern bool simulate_modem_down;

uint8_t mock_cellular_controller_init()
{
	return ztest_get_return_value();
}

//uint8_t receive_tcp(struct data *data)
//{
//    return ztest_get_return_value();
//}
char dummy_msg[] = "1234243rafasdfertqw4reqwrewqwe";
uint8_t socket_receive(struct data *socket_data, char **msg)
{
	*msg = &dummy_msg[0];
	return ztest_get_return_value();
}

int socket_connect(struct data *dummy_data, struct sockaddr *dummy_add, size_t dummy_len)
{
	return ztest_get_return_value();
}

int socket_listen(struct data *data, uint16_t port)
{
	return ztest_get_return_value();
}

int reset_modem(void)
{
	return ztest_get_return_value();
}

int stop_tcp(const bool keep_modem_awake, bool *flag, struct k_sem *sem)
{
	printk("\nStop TCP!\n");
	*flag = false;
	return ztest_get_return_value();
};

int8_t send_tcp(char *dummy, size_t dummy_len)
{
	return ztest_get_return_value();
}

int8_t lte_init(void)
{
	return ztest_get_return_value();
}

bool lte_is_ready(void)
{
	return ztest_get_return_value();
}

const struct device dummy_device;
const struct device *bind_modem(void)
{
	if (simulate_modem_down) {
		return NULL;
	}
	return &dummy_device;
}

// char mock_host_address[24] = "193.146.222.555:123456";
// int stg_config_str_read(stg_config_param_id_t id, char *str, uint8_t *len)
// {
// 	ARG_UNUSED(id);
// 	ARG_UNUSED(len);

// 	memcpy(str, &mock_host_address[0], 24);
// 	return ztest_get_return_value();
// }

// int stg_config_str_write(stg_config_param_id_t id, const char *str, const uint8_t len)
// {
// 	ARG_UNUSED(id);
// 	ARG_UNUSED(str);
// 	ARG_UNUSED(len);

// 	return ztest_get_return_value();
// }

int check_ip(void)
{
	return ztest_get_return_value();
}

int get_ip(char **ip)
{
	ARG_UNUSED(ip);
	return ztest_get_return_value();
}

bool query_listen_sock(void)
{
	return ztest_get_return_value();
}

//int8_t cache_server_address(void)
//{
//    return ztest_get_return_value();
//}
void enable_rssi(void)
{
	return;
}
