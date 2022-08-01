
#include <zephyr.h>
#ifndef X3_AMC_CACHE_MOCK_H
#define X3_AMC_CACHE_MOCK_H

bool fnc_valid_fence(void);
bool fnc_valid_def(void);

typedef enum _Mode {
	Mode_Mode_UNKNOWN = 0, /* Unset mode */
	Mode_Teach = 1, /* Fence functionality active, teach mode */
	Mode_Fence = 2, /* Fence functionality active, normal mode */
	Mode_Trace = 3 /* Fence functionality inactive, tracing active */
} Mode;

typedef enum _FenceStatus {
	FenceStatus_FenceStatus_UNKNOWN = 0, /* Unset state */
	FenceStatus_FenceStatus_Normal =
		1, /* Collar carrier is within a defined pasture and fence function is turned on */
	FenceStatus_NotStarted =
		2, /* Collar carrier has no fence or has not yet been registered in the fence */
	FenceStatus_MaybeOutOfFence =
		3, /* Collar carrier has moved out of pasture */
	FenceStatus_Escaped =
		4, /* Collar carrier has escaped from the pasture */
	FenceStatus_BeaconContact =
		5, /* Collar has contact with a owner Beacon, it will turn off any fence functionality and GPS */
	/* When entering this state, the fence status was not normal */
	FenceStatus_BeaconContactNormal =
		6, /* Contact with beacon, when entering this state, the fencestatus was normal */
	FenceStatus_FenceStatus_Invalid =
		7, /* Fence stored on collar has invalid CRC, presume broken */
	FenceStatus_TurnedOffByBLE =
		8 /* Fence has been turned off by bluetooth low energy */
} FenceStatus;

typedef enum _CollarStatus {
	CollarStatus_CollarStatus_UNKNOWN = 0, /* Unset state */
	CollarStatus_CollarStatus_Normal =
		1, /* Collar carrier has normal activity */
	CollarStatus_Stuck =
		2, /* Collar carrier has high accelerometer activity but does not move */
	CollarStatus_OffAnimal =
		3, /* Collar has extremely low accelerometer activity */
	CollarStatus_PowerOff =
		4, /* Collar off button has been pushed, shut-down */
	CollarStatus_Sleep =
		5 /* TODO remove // Collar carrier has relatively small activity */
} CollarStatus;

Mode get_mode(void);
FenceStatus get_fence_status(void);
CollarStatus get_collar_status(void);
uint16_t get_total_zap_count(void);

#endif //X3_AMC_CACHE_MOCK_H
