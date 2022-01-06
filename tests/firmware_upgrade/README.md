# Unit tests for firmware upgrade module
This unit test performs actions on the firmware upgrade module by subscribing to FRAGMENT and STATUS events, and further submit its own dummy fragment data. The steps can be seen below:

## Init unit test
The first test simply initializes the HTTP module (not mocked), the firmware upgrade module itself as well as the event manager. Test fails if either of these do not initialize correctly.

## Apply file DFU test
The next test simply simulates dummy fragment data, and submits them to the event bus. We subscribe to the fragment received event, so we can give a semaphore everytime we get into this event callback. Below are the sequence of the test:
1. Set DFU mock functions to return 0 (success)
2. Calculate how many fragments we want to send since we simulate an entire file
3. Call the submit_fragment function n times based on how many fragments we need to send to simulate an entire file given by file_size parameter.
4. Wait for the event_handler function to give semaphore back, so we can send the next fragment.
5. On the last fragment, we check something called the DFU_STATUS_EVENT, which ultimately updates regularely whether we have errors or finished succesfully. 

If the number of times(and if we did NOT go through each state as expected) the test fails. We check that the firmware module sets the states correctly.

## Fragment size > file_size
If we recieve more fragments than indicates by the file size, we should get an error. We have to create a new test suite, to properly teardown and cleanup from the previous tests.
