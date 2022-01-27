# Unit tests for animal monitor control
This unit test contains two tests at this time. The unit test here is basically set up to be able to simulate fence data as well as gnss data (if those modules are mocked away), and then subscribe to sound/zap events and see that dummy fence/gnss data provide the sound/zap events that we expect.

## First test
The first test creates a request fencedata event, and submits it to the queue. What would happen is then that the storage module that subscribes to this will write its fencedata to the given pointer area that is given in the event. Once that is done, it sends an ack. In this test, we do not have this module (or it is mocked away), and we instead subscribe to the event ourselves and writes any dummy data that we want (here we can test specific data arrays against GNSS for unit testing). \
So, we're sending request fencedata event, it triggers in our event_handler and we write dummy data and submits a new ACK event, we subscribe to this event as well and see that contents of the pointer we passed earlier has been changed to what we expect.

## Second test
The second test does the same, only that it requests GNSS data instead. What this mean is that we have both fencedata from previous test available, as well as GNSS data (since we write our own dummy data here aswell). our amc_handler module has its own process ACK event function, i.e using its calculating thread, which is at the moment just playing a SND_WELCOME everytime GNSS data is updated. This is just to see that everything works as it should.