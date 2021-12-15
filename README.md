# x3-fw-app

The X3  nRF52840 firmware, using Nordic Connect SDK and Zephyr RTOS.


## Getting started

*Note*: Although Nordic Connect SDK is available for Windows and Mac, we
strongly recommend to use Linux Ubuntu 20.1 for development. Some of the features in
the build system will not be available on windows (mainly due to POSIX compliance)

1. Install the Nordic Connect SDK **manually** as described in https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_installing.html
   Only install west and the GNU ARM Embedded Toolchain.

2. Create a top level directory `x3` placed somewhere inside your home directory structure, 
then fetch the repo:
   ```
   mkdir x3
   cd x3
   git clone git@gitlab.com:nofence/x3-fw-app
   ```
3. Fetch the Nordic connect SDK and Zephyr
   ```
   cd x3-fw-app
   git pull
   west init -l .
   west update
   ```

4. Install the protobuf compiler:
   ```
   wget https://github.com/protocolbuffers/protobuf/releases/download/v3.19.1/protobuf-all-3.19.1.tar.gz
   tar -xvf protobuf-all-3.19.1.tar.gz
   cd protobuf-all-3.19.1
   ./configure
   make
   (sudo) make install
   (sudo) ldconfig
   ```


You can now try to build the hardware-test application or the main Zephyr app. 

To build the hardware-test:
````
west build -b nf_x25_nrf52840 -- -DHARDWARE_TEST=1
```

To build the real X3 app:
```
west build -b nf_x25_nrf52840
```