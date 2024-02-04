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
#define MAX_ATTR_NUM 10
#define HEARTBEAT_STALE_TIME 5

typedef struct localTableEntry {
  char *serviceName;
  bool status;
  int lastHeartbeat;  // timestamp of the last heartbeat
  zcs_attribute_t attributes[MAX_ATTR_NUM];
  zcs_cb_f callback;  // callback function for the service
} LocalTableEntry;

typedef struct heartbeatSenderArgs {
  mcast_t *channel;
  char *serviceName;
} HeartbeatSenderArgs;

typedef struct discoveryListenerArgs {
  mcast_t *receivingChannel;
  mcast_t *sendingChannel;
  char *serviceName;
  int attrNum;
  zcs_attribute_t *attr;
} DiscoveryListenerArgs;

LocalTableEntry localTable[MAX_SERVICE_NUM] = {0};
pthread_mutex_t localTableLock = PTHREAD_MUTEX_INITIALIZER;

bool isInitialized = false;

void decode_type_name(char *message, char **type, char **serviceName) {
  char *token = strtok(message, "&");
  while (token != NULL) {
    char *key = strtok(token, "=");
    char *value = strtok(NULL, "=");
    if (strcmp(key, "type") == 0) {
      *type = value;
    } else if (strcmp(key, "serviceName") == 0) {
      *serviceName = value;
    }
    token = strtok(NULL, "&");
  }
}

void decode_whole_message(char *message, LocalTableEntry *entry) {
  // tokenize the message based on '&'
  char *token = strtok(message, "&");

  while (token != NULL) {
    // split each token into attribute name and value based on '='
    char *equalSign = strchr(token, '=');
    if (equalSign != NULL) {
      *equalSign = '\0';  // null-terminate to separate name and value
      char *attr_name = token;
      char *value = equalSign + 1;
      if (strcmp(attr_name, "type") == 0) {
        // ignore the type field
      } else if (strcmp(attr_name, "name") == 0) {
        entry->serviceName = strdup(value);
      } else {
        // handle other attributes
        int i;
        for (i = 0; i < MAX_ATTR_NUM; i++) {
          if (entry->attributes[i].attr_name == NULL) {
            entry->attributes[i].attr_name = strdup(attr_name);
            entry->attributes[i].value = strdup(value);
            break;
          }
        }
        if (i == MAX_ATTR_NUM) {
          // reached the maximum number of attributes, handle accordingly
          fprintf(stderr, "Max number of attributes exceeded.\n");
        }
      }
    }
    // move to the next token
    token = strtok(NULL, "&");
  }
}

void send_notification(mcast_t *channel, const char *name,
                       zcs_attribute_t attr[], int num) {
  char message[256];
  snprintf(message, sizeof(message), "type=NOTIFICATION&name=%s", name);

  for (int i = 0; i < num && i < MAX_ATTR_NUM; i++) {
    if (attr[i].attr_name != NULL && attr[i].value != NULL) {
      snprintf(message + strlen(message), sizeof(message) - strlen(message),
               "&%s=%s", attr[i].attr_name, attr[i].value);
    }
  }

  multicast_send(channel, message, strlen(message));
}

void *app_listen_messages(void *channel) {
  mcast_t *m = (mcast_t *)channel;
  char buffer[100];
  multicast_setup_recv(m);
  int index = 0;
  while (1) {
    while (multicast_check_receive(m) == 0) {
      printf("repeat..app is checking messages .. \n");
    }
    char *type = NULL;
    char *serviceName = NULL;
    multicast_receive(m, buffer, 100);
    char *bufferCopy = strdup(buffer);
    decode_type_name(buffer, &type, &serviceName);
    // check if it is NOTIFICATION message or HEARTBEAT message
    if (strcmp(type, "NOTIFICATION") == 0) {
      bool serviceExists = false;
      // if there is already an entry for the service, skip
      for (int i = 0; i < MAX_SERVICE_NUM; i++) {
        if (localTable[i].serviceName != NULL &&
            strcmp(localTable[i].serviceName, serviceName) == 0) {
          serviceExists = true;
          break;
        }
      }
      if (!serviceExists) {
        pthread_mutex_lock(&localTableLock);
        LocalTableEntry entry;
        // actually no need to assign serviceName here, it would be done inside
        // the decode_whole_message
        entry.status = true;
        entry.lastHeartbeat = time(NULL);
        decode_whole_message(bufferCopy, &entry);
        free(bufferCopy);
        if (index < MAX_SERVICE_NUM) {
          localTable[index++] = entry;
        }
        pthread_mutex_unlock(&localTableLock);
      }
    } else if (strcmp(type, "HEARTBEAT") == 0) {
      // listen for HEARTBEAT message
      pthread_mutex_lock(&localTableLock);
      // set the lastHeartbeat to the current time
      for (int i = 0; i < MAX_SERVICE_NUM; i++) {
        if (localTable[i].serviceName != NULL &&
            strcmp(localTable[i].serviceName, serviceName) == 0) {
          localTable[i].lastHeartbeat = time(NULL);
          break;
        }
      }
      pthread_mutex_unlock(&localTableLock);
    }
  }
  return NULL;
}
// check if the service is still alive by checking the lastHeartbeat
void *app_check_heartbeat(void *channel) {
  while (1) {
    usleep(1000000);  // check every second
    pthread_mutex_lock(&localTableLock);
    for (int i = 0; i < MAX_SERVICE_NUM; i++) {
      if (localTable[i].serviceName != NULL) {
        // heartbeat is stale, if the time difference is greater than
        // HEARTBEAT_STALE_TIME
        if (time(NULL) - localTable[i].lastHeartbeat > HEARTBEAT_STALE_TIME) {
          localTable[i].status = false;
        }
      }
    }
    pthread_mutex_unlock(&localTableLock);
  }
  return NULL;
}

void *service_send_heartbeat(void *args) {
  HeartbeatSenderArgs *heartbeatArgs = (HeartbeatSenderArgs *)args;
  mcast_t *channel = heartbeatArgs->channel;
  char *serviceName = heartbeatArgs->serviceName;
  char message[256];
  snprintf(message, sizeof(message), "type=HEARTBEAT&name=%s", serviceName);
  free(heartbeatArgs);
  // send HEARTBEAT message every second
  while (1) {
    usleep(1000000);
    multicast_send(channel, message, strlen(message));
  }
  return NULL;
}

void *service_listen_discovery(void *args) {
  DiscoveryListenerArgs *discoveryArgs = (DiscoveryListenerArgs *)args;
  mcast_t *receiveChannel = discoveryArgs->receivingChannel;
  mcast_t *sendingChannel = discoveryArgs->sendingChannel;

  char *serviceName = discoveryArgs->serviceName;
  int attrNum = discoveryArgs->attrNum;
  zcs_attribute_t *attr = discoveryArgs->attr;
  free(discoveryArgs);
  char buffer[100];
  multicast_setup_recv(receiveChannel);
  while (1) {
    while (multicast_check_receive(receiveChannel) == 0) {
      printf("repeat..service is checking messages .. \n");
    }
    multicast_receive(receiveChannel, buffer, 100);
    char *type = NULL;
    char *unusedName = NULL;
    decode_type_name(buffer, &type, &unusedName);
    if (strcmp(type, "DISCOVERY") == 0) {
      // send NOTIFICATION message
      send_notification(sendingChannel, serviceName, attr, attrNum);
    }
  }
  return NULL;
};

int zcs_init(int type) {
  if (type != ZCS_APP_TYPE && type != ZCS_SERVICE_TYPE) {
    return -1;
  }

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

    // listen for messages in another thread
    pthread_t messageListener;
    pthread_create(&messageListener, NULL, app_listen_messages,
                   appReceivingChannel);

    // validate the service status in another thread
    pthread_t heartbeatChecker;
    pthread_create(&heartbeatChecker, NULL, app_check_heartbeat, NULL);
  } else if (type == ZCS_SERVICE_TYPE) {
    // do nothing? set up in zcs_start
  }
  isInitialized = true;
  return 0;
}

int zcs_start(char *name, zcs_attribute_t attr[], int num) {
  if (!isInitialized) {
    printf("ZCS not initialized.\n");
    return -1;
  }
  // create a sending multicast channel for service
  mcast_t *serviceSendingChannel =
      multicast_init(SERVICE_SEND_CHANNEL_IP, APP_LPORT, UNUSED_PORT);

  // send NOTIFICATION message
  send_notification(serviceSendingChannel, name, attr, num);

  // send HEARTBEAT message periodically in another thread
  pthread_t heartbeatSender;
  HeartbeatSenderArgs *heartbeatArgs =
      (HeartbeatSenderArgs *)malloc(sizeof(HeartbeatSenderArgs));
  heartbeatArgs->channel = serviceSendingChannel;
  heartbeatArgs->serviceName = name;
  pthread_create(&heartbeatSender, NULL, service_send_heartbeat, heartbeatArgs);

  // create a receiving multicast channel for service
  mcast_t *serviceReceivingChannel =
      multicast_init(APP_SEND_CHANNEL_IP, UNUSED_PORT, SERVICE_LPORT);

  // listen for discovery in another thread periodically
  pthread_t messageListener;
  DiscoveryListenerArgs *discoveryArgs =
      (DiscoveryListenerArgs *)malloc(sizeof(DiscoveryListenerArgs));
  discoveryArgs->receivingChannel = serviceReceivingChannel;
  discoveryArgs->sendingChannel = serviceSendingChannel;
  discoveryArgs->serviceName = name;
  discoveryArgs->attrNum = num;
  discoveryArgs->attr = attr;
  pthread_create(&messageListener, NULL, service_listen_discovery,
                 discoveryArgs);

  return 0;
}

// Assuming a simple global state for demonstration purposes
// static int initialized = 0;
static char *service_name = NULL;
static zcs_attribute_t *service_attributes = NULL;
static int service_attr_count = 0;

// int zcs_init(int type) {
//   if (initialized) {
//     printf("ZCS already initialized.\n");
//     return -1;
//   }
//   // Initialize network, logging, etc., based on type
//   initialized = 1;
//   printf("ZCS initialized with type %d.\n", type);
//   return 0;
// }

// int zcs_start(char *name, zcs_attribute_t attr[], int num) {
//   if (!isInitialized) {
//     printf("ZCS not initialized.\n");
//     return -1;
//   }
//   // Store or advertise the service name and attributes
//   service_name = strdup(name);
//   service_attributes = (zcs_attribute_t *)malloc(num * sizeof(zcs_attribute_t));
//   for (int i = 0; i < num; ++i) {
//     service_attributes[i].attr_name = strdup(attr[i].attr_name);
//     service_attributes[i].value = strdup(attr[i].value);
//   }
//   service_attr_count = num;
//   printf("Service %s started.\n", name);
//   return 0;
// }

// int zcs_post_ad(char *ad_name, char *ad_value) {
//   // Post advertisement to the network
//   printf("Posting ad: %s = %s\n", ad_name, ad_value);
//   return 0;
// }

// int zcs_post_ad(char *ad_name, char *ad_value) {
//   char message[256];  // Adjust size based on expected message length
//   // Format the advertisement message
//   snprintf(message, sizeof(message), "%s:%s", ad_name, ad_value);
//   mcast_t *channel =
//       (zcs_type == ZCS_APP_TYPE) ? appSendingChannel : serviceSendingChannel;

//   // Check if the channel is initialized properly
//   if (channel == NULL) {
//     printf("Error: Multicast channel is not initialized.\n");
//     return -1;
//   }

//   // Use multicast_send to post the advertisement
//   if (multicast_send(channel, message, strlen(message)) == -1) {
//     perror("multicast_send failed");
//     return -1;
//   }
// }

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
  isInitialized = 0;
  printf("ZCS shutdown.\n");
  return 0;
}

void zcs_log() {
  // Implement logging functionality
  printf("ZCS log function called.\n");
}
