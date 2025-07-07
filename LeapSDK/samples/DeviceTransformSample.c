/* Copyright (C) 2012-2017 Ultraleap Limited. All rights reserved.
 *
 * Use of this code is subject to the terms of the Ultraleap SDK agreement
 * available at https://central.leapmotion.com/agreements/SdkAgreement unless
 * Ultraleap has signed a separate license agreement with you or your
 * organisation.
 *
 */

#include "ExampleConnection.h"
#include "LeapC.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/** Helper function to apply a device transform to a leap vector.
 *
 * This function multiplies the device transform, a 4x4 column
 * major matrix with the LEAP_VECTOR input.
 *
 * It assumes that the LEAP_VECTOR input would have a 'w'
 * component of 1.0
 */
static void ApplyDeviceTransform(
  const float deviceTransform[16],
  const LEAP_VECTOR* vector,
  LEAP_VECTOR* result)
{
  for (int y = 0; y < 3; ++y)
  {
    result->v[y] = 0;
    for (int x = 0; x < 3; ++x)
    {
      result->v[y] += deviceTransform[y + 4 * x] * vector->v[y];
    }
    result->v[y] += deviceTransform[y + 4 * 3];
  }
}

/** Callback for when the connection opens. */
static void OnConnect(void)
{
  printf("Connected.\n");
}

/** Callback for when a device is found. */
static void OnDevice(const LEAP_DEVICE_INFO* props)
{
  printf("Found device %s.\n", props->serial);
}

/** Callback for when a frame of tracking data is available. */
static void OnFrame(const LEAP_TRACKING_EVENT* frame)
{
  printf("Frame %lli with %i hands.\n", (long long int)frame->info.frame_id, frame->nHands);
  float buffer[16];
  /**
   * The device transform may not be available if no default is detected,
   * and a custom one has not been set.
   *
   * Note: If the result of LeapGetDeviceTransform() needs to be cached
   * for any reason, the developer can poll for events of type
   * 'eLeapEventType_NewDeviceTransform' to be aware of when to update
   * their cache.
   */
  if (GetDeviceTransform(buffer))
  {
    for (uint32_t h = 0; h < frame->nHands; h++)
    {
      LEAP_HAND* hand = &frame->pHands[h];
      LEAP_VECTOR* palm = &hand->palm.position;
      LEAP_VECTOR transformedPalm;
      ApplyDeviceTransform(buffer, palm, &transformedPalm);

      /**
       * Assuming the camera is mounted to a headset:
       * After applying the device transform, typically the vector will be relative to the centre
       * point between the user's eyes, in metre scale where from the viewer's perspective:
       * X == Right
       * Y == Up
       * Z == Backwards
       */
      printf(
        "    Hand id %i is a %s hand with position (%f, %f, %f).\n",
        hand->id,
        (hand->type == eLeapHandType_Left ? "left" : "right"),
        transformedPalm.x,
        transformedPalm.y,
        transformedPalm.z);
    }
  }
}

int main(int argc, char** argv)
{
  // Set callback function pointers
  ConnectionCallbacks.on_connection = &OnConnect;
  ConnectionCallbacks.on_device_found = &OnDevice;
  ConnectionCallbacks.on_frame = &OnFrame;

  OpenConnection();

  printf("Press Enter to exit program.\n");
  getchar();
  CloseConnection();
  DestroyConnection();
  return 0;
}
