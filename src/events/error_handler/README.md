# Error event handler
This document describes the different error levels and how it is handeled.

## Error modules

Sender  | Value
------------- | -------------
ERR_FW_UPGRADE          | 0  
ERR_AMC                 | 1
ERR_STORAGE_CONTROLLER  | 2
ERR_ENV_SENSOR          | 3
ERR_EP_MODULE           | 4
ERR_PWR_MODULE          | 5
ERR_GNSS_CONTROLLER     | 6
ERR_SOUND_CONTROLLER    | 7
ERR_MESSAGING           | 8
ERR_EEPROM              | 9
ERR_DIAGNOSTIC          |10
ERR_BLE_MODULE          |11
ERR_CELLULAR_CONTROLLER |12

## Error levels

We currently use three different error levels. All errors using the pre-defined functions below, will store information about sender, error code and severity to external flash. 

**1. Fatal error:**
All fatal errors is so severe that the entire system shuts down after 5 seconds.
Here is one example sending a cellular controller error:
```c
char *e_msg = "Out of memory!";
nf_app_fatal(ERR_CELLULAR_CONTROLLER, -ENOMEM, e_msg, strlen(e_msg));
```

**2. Normal error:**
A normal error is a non-fatal error caused by some module/logic that must
be handled. May not cause total system failure, but should still be processed
```c
char *e_msg = "BLE module is busy ";
nf_app_error(ERR_BLE_MODULE, -EBUSY, e_msg, strlen(e_msg));
```

**3. Warning error:**
A warning caused by a module must not necessarily be handled. May be ignored, but should be treated if possible.
```c
char *e_msg = "Date & Time is not available";
nf_app_error(ERR_GNSS_CONTROLLER, -EIO, e_msg, strlen(e_msg));
```