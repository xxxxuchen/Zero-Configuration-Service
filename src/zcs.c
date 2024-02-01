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

typedef struct localTableEntry {
  char *serviceName;
  bool status;
  int lastHeartbeat;
  zcs_attribute_t attributes[MAX_ATTR_NUM];
} LocalTableEntry;

LocalTableEntry localTable[MAX_SERVICE_NUM] = {0};
pthread_mutex_t localTableLock = PTHREAD_MUTEX_INITIALIZER;

bool isInitialized = false;

void encode_message();

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
        if (localTable[i].serviceName != NULL && strcmp(localTable[i].serviceName, serviceName) == 0) {
          serviceExists = true;
          break;
        }
      }
      if (!serviceExists) {
        pthread_mutex_lock(&localTableLock);
        LocalTableEntry entry;
        entry.serviceName = strdup(serviceName); // Allocate new memory for serviceName
        entry.status = true;
        entry.lastHeartbeat = time(NULL);
        decode_whole_message(bufferCopy, &entry);
        if (index < MAX_SERVICE_NUM) {
          localTable[index++] = entry;
        }
        pthread_mutex_unlock(&localTableLock);
      }
    } else if (strcmp(type, "HEARTBEAT") == 0) { 
      //CONFUSED BY THIS CHECK TO BE HONEST; ISNT THE OTHER THREAD DOING THE HEARTBEAT CHECK?
      pthread_mutex_lock(&localTableLock);
      // set the lastHeartbeat to the current time
      for (int i = 0; i < MAX_SERVICE_NUM; i++) {
        if (localTable[i].serviceName != NULL && strcmp(localTable[i].serviceName, serviceName) == 0) {
          localTable[i].lastHeartbeat = time(NULL);
          break;
        }
      }
      pthread_mutex_unlock(&localTableLock);
    }
  }
}

// check if the service is still alive by checking the lastHeartbeat
void *app_check_heartbeat(void *channel) {
  while (1) {
    sleep(1);
    pthread_mutex_lock(&localTableLock);
    for (int i = 0; i < MAX_SERVICE_NUM; i++) {
      if (localTable[i].serviceName != NULL) {
        // if lastHeartbeat is more than 5 seconds ago, set the status to down
        if (time(NULL) - localTable[i].lastHeartbeat > 5) {
          localTable[i].status = false;
        }
      }
    }
    pthread_mutex_unlock(&localTableLock);
  }
}

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
