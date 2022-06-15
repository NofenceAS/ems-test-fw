# Nofence Diagnostics
Library implementing communication with the Diagnostics module implemented in the collar firmware. Implementation in firmware for this spec is contained within src/modules/diagnostics/commander

## Transport
Implemented transport layers are RTT (Segger Real Time Transfer) over SWD, and NUS (Nordic UART Service) over BLE. 

## Packetization
Packets are encoded using consistent overhead byte stuffing ([COBS](https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing)), and will identify the packet start and end in the data stream. Thus there is no requirement for packetization in the transport layer itself. 

## Data Protocol
All communication is initiated by the Controller (e.g. host computer) by commands. Every command will receive a response from the Peripheral (Collar Diagnostics module). 
All fields are little endian unless noted otherwise. 

### Command

Commands are defined as below
| Name | Size | Description |
|----|----|----|
| Group | 1B | Group of the command, see separate table. |
| Command | 1B | Command within the group, see separate tables for each group. |
| Checksum | 2B | CRC16 CCITT of the entire packet. This field is set to 0xFFFF during the calculations. |
| Payload | 0-120 Bytes | Payload is specific to the Group/Command combination. See description in separate tables. Payload maximum size is limited by receive/parsing buffers in the firmware. |

Available groups are defined as below
| Name | Value | Description |
|----|----|----|
| SYSTEM | 0x00 | Controls system aspects such as resetting |
| SETTINGS | 0x01 | Accesses the settings API, and can be used for settings/getting e.g. serial number. |
| STIMULATOR | 0x02 | Provides mechanisms for injecting stimuli, e.g. GNSS data or starting buzzer |
| STORAGE | 0x03 | Commands for reading and writing to the storage controller |

#### SYSTEM Group
Available commands are defined as below
| Name | Value | Description |
|----|----|----|
| PING | 0x55 | Sends a data response with the same payload as was received. |
| REBOOT | 0xEB | **NOT IMPLEMENTED** Schedules a reboot of the module to trigger after 1 second |

#### SETTINGS Group
Available commands are defined as below
| Name | Value | Description |
|----|----|----|
| READ | 0x00 | Reads specified settings ID |
| WRITE | 0x01 | Writes to specified settings ID |
| ERASE_ALL | 0xEA | **NOT IMPLEMENTED** Erases all settings |

Settings are addressed using IDs as defined below
| Name | Value | Description |
|----|----|----|
| SERIAL | 0x00 | Serial number to be used by the collar |
| HOST_PORT | 0x01 | Host and port string to be used for server connection |

##### READ Command
Payload must contain the 1-Byte settings identifier.

##### WRITE Command
Payload must contain the 1-Byte settings identifier followed by the value to write to the setting. 

#### STIMULATOR Group
Available commands are defined as below
| Name | Value | Description |
|----|----|----|
| GNSS_HUB | 0x10 | Selects the operation mode of the GNSS hub. See GNSS driver and gnss_hub.c for details. |
| GNSS_SEND | 0x11 | Send payload data to the GNSS hub. |
| GNSS_RECEIVE | 0x12 | Used to receive any incoming data from the GNSS hub. |
| BUZZER_WARN | 0x0B | Activates the max warn frequency of the buzzer. |
| ELECTRICAL_PULSE | 0xE0 | Triggers an electrical pulse. |

#### STORAGE Group
**NOT IMPLEMENTED**

### Response
Responses are sent for each command regardless of the outcome of the command. Responsed str defined as below
| Name | Size | Description |
|----|----|----|
| Group | 1B | Group of the command, see separate table under command. |
| Command | 1B | Command within the group, see separate tables for each group under command. |
| Response | 1B | Response code, see separate table. |
| Checksum | 2B | CRC16 CCITT of the entire packet. This field is set to 0xFFFF during the calculations. |
| Payload | 0-120 Bytes | Payload is specific to the Group/Command combination. See description in separate tables. Payload maximum size is limited by receive/parsing buffers in the firmware. |

Available groups are defined as below
| Name | Value | Description |
|----|----|----|
| ACK | 0x00 | Command executed with success. |
| DATA | 0x01 | Data response. |
| CHK_FAILED | 0xC0 | Checksum failed. |
| NOT_ENOUGH | 0xD0 | Payload did not contain enough data |
| ERROR | 0xE0 | Command failed. |
| UNKNOWN_CMD | 0xFC | Unknown command |
| UNKNOWN_GRP | 0xFE | Unknown group |
| UNKNOWN | 0xFF | Unexpected issue occurred. |