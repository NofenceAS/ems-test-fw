# Unit tests for GNSS driver
This unit test includes a mock of a Serial driver with support for interrupt based UART, as required by the GNSS driver implementation.

Testing is done by registering a list of expected commands, and which responses to send back to those specific commands. 

The commands are required to come from the MCU in the same order as they appear in the list. 
GNSS commands and responses for M10 are according to the protocol document from U-blox:
- https://www.u-blox.com/en/product/max-m10-series#tab-documentation-resources
- https://www.u-blox.com/en/ubx-viewer/view/u-blox-M10-SPG-5.00_InterfaceDescription_UBX-20053845?url=https%3A%2F%2Fwww.u-blox.com%2Fsites%2Fdefault%2Ffiles%2Fu-blox%2520M10-SPG-5.00_InterfaceDescription_UBX-20053845.pdf


See raw folder for Python script generating a complete message with header and checksum, and binary files with messages from real hardware module. 