/* Copyright (C) 2012-2017 Ultraleap Limited. All rights reserved.
 *
 * Use of this code is subject to the terms of the Ultraleap SDK agreement
 * available at https://central.leapmotion.com/agreements/SdkAgreement unless
 * Ultraleap has signed a separate license agreement with you or your
 * organisation.
 *
 */

#include "ExampleConnection.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if defined(_MSC_VER)
  #include <Windows.h>
  #include <process.h>
  #define LockMutex EnterCriticalSection
  #define UnlockMutex LeaveCriticalSection
#else
  #include <unistd.h>
  #include <pthread.h>
  #define LockMutex pthread_mutex_lock
  #define UnlockMutex pthread_mutex_unlock
#endif


//Forward declarations
#if defined(_MSC_VER)
static void serviceMessageLoop(void * unused);
#else
static void* serviceMessageLoop(void * unused);
#endif
static void setFrame(const LEAP_TRACKING_EVENT *frame);
static void setDevice(const LEAP_DEVICE, const LEAP_DEVICE_INFO *deviceProps);

//External state
bool IsConnected = false;

//Internal state
static volatile bool _isRunning = false;
static LEAP_CONNECTION connectionHandle = NULL;
static LEAP_TRACKING_EVENT *lastFrame = NULL;
static LEAP_DEVICE_INFO *lastDevice = NULL;
static LEAP_DEVICE lastDeviceHandle = NULL;

//Callback function pointers
struct Callbacks ConnectionCallbacks;

//Threading variables
#if defined(_MSC_VER)
static HANDLE pollingThread;
static CRITICAL_SECTION dataLock;
#else
static pthread_t pollingThread;
static pthread_mutex_t dataLock;
#endif

/**
 * Creates the connection handle and opens a connection to the Leap Motion
 * service. On success, creates a thread to service the LeapC message pump.
 */
LEAP_CONNECTION* OpenConnection(void){
  if(_isRunning){
    return &connectionHandle;
  }
  if(connectionHandle || LeapCreateConnection(NULL, &connectionHandle) == eLeapRS_Success){
    eLeapRS result = LeapOpenConnection(connectionHandle);
    if(result == eLeapRS_Success){
      _isRunning = true;
#if defined(_MSC_VER)
      InitializeCriticalSection(&dataLock);
      pollingThread = (HANDLE)_beginthread(serviceMessageLoop, 0, NULL);
#else
      pthread_mutex_init(&dataLock, NULL);
      pthread_create(&pollingThread, NULL, serviceMessageLoop, NULL);
#endif
    }
  }
  return &connectionHandle;
}

void CloseConnection(void){
  if(!_isRunning){
    return;
  }
  _isRunning = false;
  LeapCloseConnection(connectionHandle);
#if defined(_MSC_VER)
  WaitForSingleObject(pollingThread, INFINITE);
  CloseHandle(pollingThread);
#else
  pthread_join(pollingThread, NULL);
  pthread_mutex_destroy(&dataLock);
#endif
}

void DestroyConnection(void){
  CloseConnection();
  LeapDestroyConnection(connectionHandle);
}


/** Close the connection and let message thread function end. */
void CloseConnectionHandle(LEAP_CONNECTION* connectionHandle){
  LeapDestroyConnection(*connectionHandle);
  _isRunning = false;
}

/** Called by serviceMessageLoop() when a connection event is returned by LeapPollConnection(). */
static void handleConnectionEvent(const LEAP_CONNECTION_EVENT *connection_event){
  IsConnected = true;
  if(ConnectionCallbacks.on_connection){
    ConnectionCallbacks.on_connection();
  }
}

/** Called by serviceMessageLoop() when a connection lost event is returned by LeapPollConnection(). */
static void handleConnectionLostEvent(const LEAP_CONNECTION_LOST_EVENT *connection_lost_event){
  IsConnected = false;
  if(ConnectionCallbacks.on_connection_lost){
    ConnectionCallbacks.on_connection_lost();
  }
}

/**
 * Called by serviceMessageLoop() when a device event is returned by LeapPollConnection()
 * Demonstrates how to access device properties.
 */
static void handleDeviceEvent(const LEAP_DEVICE_EVENT *device_event){
  LEAP_DEVICE deviceHandle;
  //Open device using LEAP_DEVICE_REF from event struct.
  eLeapRS result = LeapOpenDevice(device_event->device, &deviceHandle);
  if(result != eLeapRS_Success){
    printf("Could not open device %s.\n", ResultString(result));
    return;
  }

  //Create a struct to hold the device properties, we have to provide a buffer for the serial string
  LEAP_DEVICE_INFO deviceProperties = { sizeof(deviceProperties) };
  // Start with a length of 1 (pretending we don't know a priori what the length is).
  // Currently device serial numbers are all the same length, but that could change in the future
  deviceProperties.serial_length = 1;
  deviceProperties.serial = malloc(deviceProperties.serial_length);
  //This will fail since the serial buffer is only 1 character long
  // But deviceProperties is updated to contain the required buffer length
  result = LeapGetDeviceInfo(deviceHandle, &deviceProperties);
  if(result == eLeapRS_InsufficientBuffer){
    //try again with correct buffer size
    deviceProperties.serial = realloc(deviceProperties.serial, deviceProperties.serial_length);
    result = LeapGetDeviceInfo(deviceHandle, &deviceProperties);
    if(result != eLeapRS_Success){
      printf("Failed to get device info %s.\n", ResultString(result));
      free(deviceProperties.serial);
      return;
    }
  }
  setDevice(deviceHandle, &deviceProperties);
  if(ConnectionCallbacks.on_device_found){
    ConnectionCallbacks.on_device_found(&deviceProperties);
  }

  free(deviceProperties.serial);
  LeapCloseDevice(deviceHandle);
}

/** Called by serviceMessageLoop() when a device lost event is returned by LeapPollConnection(). */
static void handleDeviceLostEvent(const LEAP_DEVICE_EVENT *device_event){
  if(ConnectionCallbacks.on_device_lost){
    ConnectionCallbacks.on_device_lost();
  }
}

/** Called by serviceMessageLoop() when a device failure event is returned by LeapPollConnection(). */
static void handleDeviceFailureEvent(const LEAP_DEVICE_FAILURE_EVENT *device_failure_event){
  if(ConnectionCallbacks.on_device_failure){
    ConnectionCallbacks.on_device_failure(device_failure_event->status, device_failure_event->hDevice);
  }
}

/** Called by serviceMessageLoop() when a tracking event is returned by LeapPollConnection(). */
static void handleTrackingEvent(const LEAP_TRACKING_EVENT *tracking_event){
  setFrame(tracking_event); //support polling tracking data from different thread
  if(ConnectionCallbacks.on_frame){
    ConnectionCallbacks.on_frame(tracking_event);
  }
}

/** Called by serviceMessageLoop() when a policy event is returned by LeapPollConnection(). */
static void handlePolicyEvent(const LEAP_POLICY_EVENT *policy_event){
  if(ConnectionCallbacks.on_policy){
    ConnectionCallbacks.on_policy(policy_event->current_policy);
  }
}

/** Called by serviceMessageLoop() when an image event is returned by LeapPollConnection(). */
static void handleImageEvent(const LEAP_IMAGE_EVENT *image_event) {
  if(ConnectionCallbacks.on_image){
    ConnectionCallbacks.on_image(image_event);
  }
}

/** Called by serviceMessageLoop() when an IMU event is returned by LeapPollConnection(). */
static void handleImuEvent(const LEAP_IMU_EVENT *imu_event) {
  if(ConnectionCallbacks.on_imu){
    ConnectionCallbacks.on_imu(imu_event);
  }
}

static void handleTrackingModeEvent(const LEAP_TRACKING_MODE_EVENT *mode_event) {
  if(ConnectionCallbacks.on_tracking_mode){
    ConnectionCallbacks.on_tracking_mode(mode_event);
  }
}

/**
 * Services the LeapC message pump by calling LeapPollConnection().
 * The average polling time is determined by the framerate of the Ultraleap Tracking service.
 */
#if defined(_MSC_VER)
static void serviceMessageLoop(void * unused){
#else
static void* serviceMessageLoop(void * unused){
#endif
  eLeapRS result;
  LEAP_CONNECTION_MESSAGE msg;
  while(_isRunning){
    unsigned int timeout = 1000;
    result = LeapPollConnection(connectionHandle, timeout, &msg);

    if(result != eLeapRS_Success){
      printf("LeapC PollConnection call was %s.\n", ResultString(result));
      continue;
    }

    switch (msg.type){
      case eLeapEventType_Connection:
        handleConnectionEvent(msg.connection_event);
        break;
      case eLeapEventType_ConnectionLost:
        handleConnectionLostEvent(msg.connection_lost_event);
        break;
      case eLeapEventType_Device:
        handleDeviceEvent(msg.device_event);
        break;
      case eLeapEventType_DeviceLost:
        handleDeviceLostEvent(msg.device_event);
        break;
      case eLeapEventType_DeviceFailure:
        handleDeviceFailureEvent(msg.device_failure_event);
        break;
      case eLeapEventType_Tracking:
        handleTrackingEvent(msg.tracking_event);
        break;
      case eLeapEventType_ImageComplete:
        // Ignore
        break;
      case eLeapEventType_ImageRequestError:
        // Ignore
        break;
      case eLeapEventType_Policy:
        handlePolicyEvent(msg.policy_event);
        break;
      case eLeapEventType_Image:
        handleImageEvent(msg.image_event);
        break;
      case eLeapEventType_TrackingMode:
        handleTrackingModeEvent(msg.tracking_mode_event);
        break;
      case eLeapEventType_IMU:
        handleImuEvent(msg.imu_event);
        break;
      default:
        //discard unknown message types
        printf("Unhandled message type %i.\n", msg.type);
    } //switch on msg.type
  }
#if !defined(_MSC_VER)
  return NULL;
#endif
}

/* Used in Polling Example: */

void deepCopyTrackingEvent(LEAP_TRACKING_EVENT* dst, const LEAP_TRACKING_EVENT* src)
{
  memcpy(&dst->info, &src->info, sizeof(LEAP_FRAME_HEADER));
  dst->tracking_frame_id = src->tracking_frame_id;
  dst->nHands = src->nHands;
  dst->framerate = src->framerate;
  memcpy(dst->pHands, src->pHands, src->nHands * sizeof(LEAP_HAND));
}

/**
 * Caches the newest frame by copying the tracking event struct returned by
 * LeapC.
 */
void setFrame(const LEAP_TRACKING_EVENT *frame){
  LockMutex(&dataLock);
  if(!lastFrame)
  {
    lastFrame = malloc(sizeof(LEAP_TRACKING_EVENT));
    lastFrame->pHands = malloc(2 * sizeof(LEAP_HAND));
  }

  if (frame != NULL)
  {
    deepCopyTrackingEvent(lastFrame, frame);
  }

  UnlockMutex(&dataLock);
}

/** Returns a pointer to a cached tracking frame which remains valid until the next call. */
LEAP_TRACKING_EVENT* GetFrame(){
  static LEAP_TRACKING_EVENT *currentFrame = NULL;
  if(currentFrame == NULL)
  {
    currentFrame = malloc(sizeof(LEAP_TRACKING_EVENT));
    currentFrame->pHands = malloc(2 * sizeof(LEAP_HAND));
  }

  LockMutex(&dataLock);
  if (lastFrame == NULL)
  {
    UnlockMutex(&dataLock);
    return NULL;
  }

  deepCopyTrackingEvent(currentFrame, lastFrame);
  UnlockMutex(&dataLock);

  return currentFrame;
}

/**
 * Caches the last device found by copying the device info struct returned by
 * LeapC.
 */
static void setDevice(LEAP_DEVICE device, const LEAP_DEVICE_INFO *deviceProps){
  LockMutex(&dataLock);
  lastDeviceHandle = device;
  if(lastDevice){
    free(lastDevice->serial);
  } else {
    lastDevice = malloc(sizeof(*deviceProps));
  }
  *lastDevice = *deviceProps;
  lastDevice->serial = malloc(deviceProps->serial_length);
  memcpy(lastDevice->serial, deviceProps->serial, deviceProps->serial_length);
  UnlockMutex(&dataLock);
}

/** Returns a pointer to the cached device info. */
LEAP_DEVICE_INFO* GetDeviceProperties(){
  LEAP_DEVICE_INFO *currentDevice;
  LockMutex(&dataLock);
  currentDevice = lastDevice;
  UnlockMutex(&dataLock);
  return currentDevice;
}

//End of polling example-specific code

/* Used in DeviceTransform Example: */
bool GetDeviceTransform(float buffer[16])
{
  if(!buffer)
  {
    printf("Failed to get device transform, invalid argument: output buffer was NULL.\n");
    return false;
  }

  bool available = false;
  eLeapRS result = eLeapRS_UnknownError;
  LockMutex(&dataLock);
  available = LeapDeviceTransformAvailable(lastDeviceHandle);
  if(available)
  {
    result = LeapGetDeviceTransform(lastDeviceHandle, buffer);
  }
  UnlockMutex(&dataLock);

  if(!available)
  {
    printf("Device transform not available for this device\n");
    return false;
  }

  if(result != eLeapRS_Success)
  {
    printf("Failed to get device transform: %s.\n", ResultString(result));
    return false;
  }
  
  return true;
}
//End of DeviceTransform example-specific code

/** Translates eLeapRS result codes into a human-readable string. */
const char* ResultString(eLeapRS r) {
  switch(r){
    case eLeapRS_Success:                  return "eLeapRS_Success";
    case eLeapRS_UnknownError:             return "eLeapRS_UnknownError";
    case eLeapRS_InvalidArgument:          return "eLeapRS_InvalidArgument";
    case eLeapRS_InsufficientResources:    return "eLeapRS_InsufficientResources";
    case eLeapRS_InsufficientBuffer:       return "eLeapRS_InsufficientBuffer";
    case eLeapRS_Timeout:                  return "eLeapRS_Timeout";
    case eLeapRS_NotConnected:             return "eLeapRS_NotConnected";
    case eLeapRS_HandshakeIncomplete:      return "eLeapRS_HandshakeIncomplete";
    case eLeapRS_BufferSizeOverflow:       return "eLeapRS_BufferSizeOverflow";
    case eLeapRS_ProtocolError:            return "eLeapRS_ProtocolError";
    case eLeapRS_InvalidClientID:          return "eLeapRS_InvalidClientID";
    case eLeapRS_UnexpectedClosed:         return "eLeapRS_UnexpectedClosed";
    case eLeapRS_UnknownImageFrameRequest: return "eLeapRS_UnknownImageFrameRequest";
    case eLeapRS_UnknownTrackingFrameID:   return "eLeapRS_UnknownTrackingFrameID";
    case eLeapRS_RoutineIsNotSeer:         return "eLeapRS_RoutineIsNotSeer";
    case eLeapRS_TimestampTooEarly:        return "eLeapRS_TimestampTooEarly";
    case eLeapRS_ConcurrentPoll:           return "eLeapRS_ConcurrentPoll";
    case eLeapRS_NotAvailable:             return "eLeapRS_NotAvailable";
    case eLeapRS_NotStreaming:             return "eLeapRS_NotStreaming";
    case eLeapRS_CannotOpenDevice:         return "eLeapRS_CannotOpenDevice";
    default:                               return "unknown result type.";
  }
}
/** Cross-platform sleep function */
void millisleep(int milliseconds){
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds*1000);
#endif
  }
//End-of-ExampleConnection.c
