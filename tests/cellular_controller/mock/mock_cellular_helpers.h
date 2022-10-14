
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>

// typedef enum {
//     /* NB! Do NOT change or reorder the identifiers of NVS Id-data pairs without 
// 	 * a sector erase of all NVS sectors */
//     STG_U8_WARN_MAX_DURATION = 0,
// 	STG_U8_WARN_MIN_DURATION,
// 	STG_U8_PAIN_CNT_DEF_ESCAPED,
// 	STG_U8_COLLAR_MODE,
// 	STG_U8_FENCE_STATUS,
// 	STG_U8_COLLAR_STATUS,
// 	STG_U8_TEACH_MODE_FINISHED,
// 	STG_U8_EMS_PROVIDER,
// 	STG_U8_PRODUCT_RECORD_REV,
// 	STG_U8_BOM_MEC_REV,
// 	STG_U8_BOM_PCB_REV,
// 	STG_U8_HW_VERSION,
// 	STG_U8_KEEP_MODE,
// 	STG_U8_RESET_REASON,
// 	STG_U16_ACC_SIGMA_NOACTIVITY_LIMIT,
// 	STG_U16_OFF_ANIMAL_TIME_LIMIT_SEC,
// 	STG_U16_ACC_SIGMA_SLEEP_LIMIT,
// 	STG_U16_ZAP_CNT_TOT,
// 	STG_U16_ZAP_CNT_DAY,
// 	STG_U16_PRODUCT_TYPE,
//     STG_U32_UID, 
//     STG_U32_WARN_CNT_TOT,
//     STG_STR_HOST_PORT,
//     STG_STR_BLE_KEY,
// 	STG_PARAM_ID_CNT
// }stg_config_param_id_t;

struct data {
};
struct sockaddr {
};

uint8_t mock_cellular_controller_init();

uint8_t socket_receive(struct data *, char **);

int reset_modem(void);

void stop_tcp(void);

int8_t send_tcp(char *, size_t);

int send_tcp_q(char *, size_t);

int8_t socket_connect(struct data *, struct sockaddr *, size_t);

int socket_listen(struct data *, uint16_t);

int8_t lte_init(void);

bool lte_is_ready(void);

const struct device *bind_modem(void);

// int stg_config_str_read(stg_config_param_id_t id, char *str, uint8_t *len);

// int stg_config_str_write(stg_config_param_id_t id, const char *str, const uint8_t len);

int check_ip(void);

int get_ip(char **);

void send_tcp_fn(void);

bool query_listen_sock(void);
//int8_t cache_server_address(void);
