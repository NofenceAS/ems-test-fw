# Event manager with unit test using twister
This example contains a publisher module and a listener module. The publisher module puts some data on the bus, while the listener will retreive this data. 
The unit tests should contain at least two simple tests (which are not yet implemented): 
1. Use the publisher module api call to put some data on the bus and then expect the listener module to retrieve it.
2. Send a request from listener module, and expect the data in return from the publisher module