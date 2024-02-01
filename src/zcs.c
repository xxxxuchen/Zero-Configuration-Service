#include "zcs.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "multicast.h"

#define APP_SEND_CHANNEL_IP "224.1.1.1"
#define SERVICE_LPORT 1100
#define SERVICE_SEND_CHANNEL_IP "223.1.1.1"
#define APP_LPORT 1700
#define UNUSED_PORT 1000

#define MAX_SERVICE_NUM 20

typedef struct localTableEntry {
  char *serviceName;
  //
} LocalTableEntry;

LocalTableEntry localTable[MAX_SERVICE_NUM] = {0};
pthread_mutex_t localTableLock = PTHREAD_MUTEX_INITIALIZER;

bool isInitialized = false;

void encode_message();

void decode_message();

void *app_listen_notification(void *channel) {
  mcast_t *m = (mcast_t *)channel;
  char buffer[100];
  while (1) {
    while (multicast_check_receive(m) == 0) {
      printf("repeat..checking notification.. \n");
    }
    multicast_receive(m, buffer, 100);
    // decoding the received string to get the message type and service name
    // if the message is a NOTIFICATION, update the local table for
    // corresponding service
  }
}

void *app_listen_heartbeat(void *channel) {
  mcast_t *m = (mcast_t *)channel;
  char buffer[100];
  // check if heartbeat is received every 5 seconds
  while (1) {
    usleep(5000000);
    if (multicast_check_receive(m) == 0) {
      // change the status of the service to down
      continue;
    }
    // successfully received the heartbeat, change down to up
    multicast_receive(m, buffer, 100);
    // decoding the received string to get the message type and service name
    // if the message is a HEARTBEAT, update the local table for corresponding service
  }
}

int zcs_init(int type) {
  if (type != ZCS_APP_TYPE && type != ZCS_SERVICE_TYPE) {
    return -1;
  }
  isInitialized = true;

  if (type == ZCS_APP_TYPE) {
    // create a sending multicast channel for app
    mcast_t *appSendingChannel =
        multicast_init(APP_SEND_CHANNEL_IP, SERVICE_LPORT, UNUSED_PORT);
    // send DISCOVERY message
    multicast_send(appSendingChannel, "type=DISCOVERY",
                   strlen("type=DISCOVERY"));

    // create a receiving multicast channel for app
    mcast_t *appReceivingChannel =
        multicast_init(SERVICE_SEND_CHANNEL_IP, UNUSED_PORT, APP_LPORT);

    // listen for NOTIFICATION in another thread
    pthread_t notificationListener;
    pthread_create(&notificationListener, NULL, app_listen_notification,
                   appReceivingChannel);

    // listen for HEARTBEAT in another thread
    pthread_t heartbeatListener;
    pthread_create(&heartbeatListener, NULL, app_listen_heartbeat,
                   appReceivingChannel);

  } else if (type == ZCS_SERVICE_TYPE) {
    // create a sending multicast channel for service
    mcast_t *serviceSendingChannel =
        multicast_init(SERVICE_SEND_CHANNEL_IP, APP_LPORT, UNUSED_PORT);
    
  }
}