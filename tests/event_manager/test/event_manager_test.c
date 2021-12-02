// ! Commented out since we need to write the unit tests mentioned in
// the readme file, using event_manager modules and twister !
// Difficulties by unit testing event-driven architecture is how to expect a return value
// from the given ztest assertion with passed function return value
//
//
//#include <ztest.h>
//#include "publisher.h"
//#include "listener.h"
//
//static void test_retrieve_publisher_data(void)
//{
//	int value1 = 1;
//	int value2 = 2;
//	int value3 = 3;
//
//	update_sensor(value1, value2, value3);
//	/* Add so that we can get listener callback somehow into the ztest asserert... */
//}
