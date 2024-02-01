#include "zcs.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "multicast.h"
#include "zcs.h"

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
    // if the message is a HEARTBEAT, update the local table for corresponding
    // service
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
   // do nothing?
  }
}

// Assuming a simple global state for demonstration purposes
static int initialized = 0;
static char *service_name = NULL;
static zcs_attribute_t *service_attributes = NULL;
static int service_attr_count = 0;

int zcs_init(int type) {
  if (initialized) {
    printf("ZCS already initialized.\n");
    return -1;
  }
  // Initialize network, logging, etc., based on type
  initialized = 1;
  printf("ZCS initialized with type %d.\n", type);
  return 0;
}

int zcs_start(char *name, zcs_attribute_t attr[], int num) {
  if (!initialized) {
    printf("ZCS not initialized.\n");
    return -1;
  }
  // Store or advertise the service name and attributes
  service_name = strdup(name);
  service_attributes = (zcs_attribute_t *)malloc(num * sizeof(zcs_attribute_t));
  for (int i = 0; i < num; ++i) {
    service_attributes[i].attr_name = strdup(attr[i].attr_name);
    service_attributes[i].value = strdup(attr[i].value);
  }
  service_attr_count = num;
  printf("Service %s started.\n", name);
  return 0;
}

int zcs_post_ad(char *ad_name, char *ad_value) {
  // Post advertisement to the network
  printf("Posting ad: %s = %s\n", ad_name, ad_value);
  return 0;
}

int zcs_query(char *attr_name, char *attr_value, char *node_names[],
              int namelen) {
  // Query the network for services matching attr_name and attr_value
  printf("Querying for %s = %s\n", attr_name, attr_value);
  return 0;  // Return number of matching nodes found (up to namelen)
}

int zcs_get_attribs(char *name, zcs_attribute_t attr[], int *num) {
  // Retrieve attributes for a service by name
  if (strcmp(service_name, name) == 0) {
    int count = (*num < service_attr_count) ? *num : service_attr_count;
    for (int i = 0; i < count; ++i) {
      attr[i].attr_name = strdup(service_attributes[i].attr_name);
      attr[i].value = strdup(service_attributes[i].value);
    }
    *num = count;
    return 0;
  }
  return -1;  // Service not found
}

int zcs_listen_ad(char *name, zcs_cb_f cback) {
  // Listen for advertisements matching 'name', and call cback upon match
  printf("Listening for ads with name %s.\n", name);
  return 0;
}

int zcs_shutdown() {
  // Clean up resources
  free(service_name);
  for (int i = 0; i < service_attr_count; ++i) {
    free(service_attributes[i].attr_name);
    free(service_attributes[i].value);
  }
  free(service_attributes);
  initialized = 0;
  printf("ZCS shutdown.\n");
  return 0;
}

void zcs_log() {
  // Implement logging functionality
  printf("ZCS log function called.\n");
}
