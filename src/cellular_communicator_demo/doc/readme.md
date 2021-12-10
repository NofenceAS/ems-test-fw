# Cellular communication demo

A simple app to demonstrate the use of sara-r4 modem driver api. It creates an echo-client that connects to a port on u-blox test echo server "echo.u-blox.com" through a TCP socket. The app is based on https://github.com/zephyrproject-rtos/zephyr/blob/main//samples/net/sockets/echo_client/README.rst .

#### U-blox sara r422 modem dts node
```
    &uart1 {
        status = "okay";
        current-speed = <115200>;
        tx-pin = <26>;
        rx-pin = <5>;
        rts-pin = <11>;
        cts-pin = <10>;
    sara_r4 {   compatible = "ublox,sara-r4";
                label = "ublox-sara-r4";
                status = "okay";

                 mdm-power-gpios = <&gpio0 4 0>;
                //mdm-power-gpios = <&gpio1 11 GPIO_ACTIVE_LOW>;
                mdm-reset-gpios = <&gpio1 1 0>;
                mdm-vint-gpios = <&gpio1 3 0>;
        };

};
```
#### Important config parameters (cell_comm_demo.conf)
```
CONFIG_NET_IPV4=y
CONFIG_NET_TCP=y
CONFIG_MODEM_UBLOX_SARA_R4_APN="onomondo"
CONFIG_NET_CONFIG_PEER_IPV4_ADDR="185.215.195.137"
CONFIG_SERVER_PORT=7
```

To build the cellular_communication sample on nrf-5340-PDK:
````
west build -b nrf5340dk_nrf5340_cpuapp --DCELLULAR_DEMO=1 
````
    
## Comments on ublox sara r4 driver (next steps)
* Power handling is not implemented. (a must)
* Socket listening is not implemented. (a must)
* Network loss doesn't seem to be handled properly.
* http/s control is not implemented in the driver, could be added to the driver but it might be easier to use Zephyr's "http implementation".

## Integrating the modem drivers tree into our build system 

## Comments on the app
* The kernel takes too long to boot up!







