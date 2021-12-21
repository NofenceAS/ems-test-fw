#include <event_manager.h>

/**
 * @brief Simualtes a change in the sensor values and put the contents/changes
 * on the event bus for the listener module to retrieve
 * @param value1 first simulated value to send *from* the sensor to event bus
 * @param value2 second simulated value to send *from* the sensor to event bus
 * @param value3 third simulated value to send *from* the sensor to event bus
 */
void update_sensor(int value1, int value2, int value3);