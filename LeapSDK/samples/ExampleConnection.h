/* Copyright (C) 2012-2017 Ultraleap Limited. All rights reserved.
 *
 * Use of this code is subject to the terms of the Ultraleap SDK agreement
 * available at https://central.leapmotion.com/agreements/SdkAgreement unless
 * Ultraleap has signed a separate license agreement with you or your
 * organisation.
 *
 */

#ifndef ExampleConnection_h
#define ExampleConnection_h

#include "LeapC.h"

/* Client functions */
LEAP_CONNECTION* OpenConnection(void);
void CloseConnection(void);
void DestroyConnection(void);
LEAP_TRACKING_EVENT* GetFrame(void); //Used in polling example
LEAP_DEVICE_INFO* GetDeviceProperties(void); //Used in polling example
bool GetDeviceTransform(float[16]); //Used in device transform example
const char* ResultString(eLeapRS r);

/* State */
extern bool IsConnected;

/* Callback function pointers */
typedef void (*connection_callback)     (void);
typedef void (*device_callback)         (const LEAP_DEVICE_INFO *device);
typedef void (*device_lost_callback)    (void);
typedef void (*device_failure_callback) (const eLeapDeviceStatus failure_code,
                                         const LEAP_DEVICE failed_device);
typedef void (*policy_callback)         (const uint32_t current_policies);
typedef void (*tracking_callback)       (const LEAP_TRACKING_EVENT *tracking_event);
typedef void (*image_callback)          (const LEAP_IMAGE_EVENT *image_event);
typedef void (*imu_callback)(const LEAP_IMU_EVENT *imu_event);
typedef void (*tracking_mode_callback)(const LEAP_TRACKING_MODE_EVENT *mode_event);

struct Callbacks{
  connection_callback      on_connection;
  connection_callback      on_connection_lost;
  device_callback          on_device_found;
  device_lost_callback     on_device_lost;
  device_failure_callback  on_device_failure;
  policy_callback          on_policy;
  tracking_callback        on_frame;
  image_callback           on_image;
  imu_callback             on_imu;
  tracking_mode_callback   on_tracking_mode;
};
extern struct Callbacks ConnectionCallbacks;
extern void millisleep(int milliseconds);
#endif /* ExampleConnection_h */
