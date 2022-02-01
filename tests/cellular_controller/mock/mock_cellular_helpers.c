#include <ztest.h>
#include <mock_cellular_helpers.h>

extern bool simulate_modem_down;

uint8_t mock_cellular_controller_init(){
    return ztest_get_return_value();
}

//uint8_t receive_tcp(struct data *data)
//{
//    return ztest_get_return_value();
//}

uint8_t socket_receive(struct data* socket_data)
{
    return ztest_get_return_value();
}

int8_t socket_connect(struct data *dummy_data, struct sockaddr *dummy_add,
                      size_t dummy_len)
{
    return ztest_get_return_value();
}
void stop_tcp(void){
    return;
};

int8_t send_tcp(char* dummy, size_t dummy_len){
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
const struct device* bind_modem(void)
{
    if(simulate_modem_down)
    {
        return NULL;
    }
    return &dummy_device;
}