#include "zcs.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "multicast.h"

#define APP_SEND_CHANNEL_IP "224.1.1.1"
#define SERVICE_LPORT 1100
#define SERVICE_SEND_CHANNEL_IP "225.1.1.1"
#define APP_LPORT 1700
#define UNUSED_PORT 1000

#define MAX_SERVICE_NUM 20
#define MAX_ATTR_NUM 10           // max number of attributes for each service
#define HEARTBEAT_EXPIRE_TIME 10  // in seconds
#define APP_MAX_WAIT_TIME 20      // max seconds app waits messages from service
#define MAX_MESSAGE_LENGTH 256
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
int table_index = 0;  // Table index

bool isInitialized = false;
bool isStarted = false;
bool isTerminating = false;
// assuming for now that app terminates after all the service nodes terminate
bool appTerminated = false;
char *global_service_name = NULL;

pthread_t messageListener;    // listen for notification, heartbeat, and ad (app)
pthread_t heartbeatChecker;   // check if heartbeat is expire (app)
pthread_t heartbeatSender;    // send heartbeat (service)
pthread_t discoveryListener;  // listen for discovery from the app (service)

void decode_type_name(char *message, char **type, char **serviceName) {
  char *saveptr1;
  char *token = strtok_r(message, "&", &saveptr1);
  // char *token = strtok(message, "&");
  while (token != NULL) {
    if (*type != NULL && *serviceName != NULL) {
      break;
    }
    char *saveptr2;
    char *key = strtok_r(token, "=", &saveptr2);
    char *value = strtok_r(NULL, "=", &saveptr2);
    if (strcmp(key, "message_type") == 0) {
      *type = value;
    } else if (strcmp(key, "name") == 0) {
      *serviceName = value;
    }
    token = strtok_r(NULL, "&", &saveptr1);
  }
}

void decode_notification(char *message, LocalTableEntry *entry) {
  if (message == NULL) {
    return;
  }

  // tokenize the message based on '&'
  char *saveptr1;  // Define save pointer for outer tokenization
  char *token = strtok_r(message, "&", &saveptr1);
  // char *token = strtok(message, "&");

  while (token != NULL) {
    // split each token into attribute name and value based on '='
    char *equalSign = strchr(token, '=');
    if (equalSign != NULL) {
      *equalSign = '\0';  // null-terminate to separate name and value
      char *attr_name = token;
      char *value = equalSign + 1;
      if (strcmp(attr_name, "message_type") == 0) {
        // ignore the message_type field
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
    token = strtok_r(NULL, "&", &saveptr1);
  }
}

void decode_advertisement(char *message, char **adName, char **adValue) {
  char *saveptr1, *saveptr2;
  char *token = strtok_r(message, "&", &saveptr1);
  while (token != NULL) {
    char *key = strtok_r(token, "=", &saveptr2);
    char *value = strtok_r(NULL, "=", &saveptr2);
    if (key != NULL && value != NULL) {
      if (strcmp(key, "name") == 0) {
        *adName = value;
      } else if (strcmp(key, "value") == 0) {
        *adValue = value;
      }
    }
    token = strtok_r(NULL, "&", &saveptr1);
  }
}

void send_notification(mcast_t *channel, const char *name,
                       zcs_attribute_t attr[], int num) {
  char message[MAX_MESSAGE_LENGTH];
  snprintf(message, sizeof(message), "message_type=NOTIFICATION&name=%s", name);
  for (int i = 0; i < num && i < MAX_ATTR_NUM; i++) {
    if (attr[i].attr_name != NULL && attr[i].value != NULL) {
      snprintf(message + strlen(message), sizeof(message) - strlen(message),
               "&%s=%s", attr[i].attr_name, attr[i].value);
    }
  }
  multicast_send(channel, message, strlen(message));
}

// messages are NOTIFICATION, HEARTBEAT, and ADVERTISEMENT.
// all in this one thread
void *app_listen_messages(void *channel) {
  mcast_t *m = (mcast_t *)channel;
  char buffer[MAX_MESSAGE_LENGTH];
  multicast_setup_recv(m);
  // int index = 0;
  while (!appTerminated) {
    int startTime = time(NULL);
    while (multicast_check_receive(m) == 0) {
      printf("repeat..app is checking messages .. \n");
      // if there is no service running within APP_MAX_WAIT_TIME, terminate app
      if (time(NULL) - startTime > APP_MAX_WAIT_TIME) {
        appTerminated = true;
        return NULL;
      }
    }
    char *message_type = NULL;
    char *serviceName = NULL;
    multicast_receive(m, buffer, MAX_MESSAGE_LENGTH);
    char *bufferCopy = strdup(buffer);
    decode_type_name(buffer, &message_type, &serviceName);

    // check if it is NOTIFICATION message or HEARTBEAT message
    if (strcmp(message_type, "NOTIFICATION") == 0) {
      printf("Notification received from %s\n", serviceName);
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
        // the decode_notification
        entry.status = true;
        entry.lastHeartbeat = time(NULL);
        entry.callback = NULL;
        decode_notification(bufferCopy, &entry);
        free(bufferCopy);
        if (table_index < MAX_SERVICE_NUM) {
          printf("ADDING SERVICE\n");
          localTable[table_index] = entry;
          table_index = table_index + 1;
        }
        pthread_mutex_unlock(&localTableLock);
      }
    } else if (strcmp(message_type, "HEARTBEAT") == 0) {
      printf("Heartbeat received from %s\n", serviceName);
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
    } else if (strcmp(message_type, "advertisement") == 0) {
      printf("Advertisement received from %s\n", serviceName);
      char *adName = NULL;
      char *adValue = NULL;
      decode_advertisement(bufferCopy, &adName, &adValue);
      free(bufferCopy);
      pthread_mutex_lock(&localTableLock);
      for (int i = 0; i < MAX_SERVICE_NUM; i++) {
        if (localTable[i].serviceName != NULL &&
            strcmp(localTable[i].serviceName, serviceName) == 0) {
          if (localTable[i].callback != NULL) {
            // call the callback function
            localTable[i].callback(adName, adValue);
          }
          break;
        }
      }
      pthread_mutex_unlock(&localTableLock);
    }
    // clean the buffer
    memset(buffer, 0, sizeof(buffer));
  }
  return NULL;
}

// check if the service is still alive by checking the lastHeartbeat
void *app_check_heartbeat(void *channel) {
  while (!appTerminated) {
    usleep(1000000);  // check every second
    pthread_mutex_lock(&localTableLock);
    for (int i = 0; i < MAX_SERVICE_NUM; i++) {
      if (localTable[i].serviceName != NULL) {
        // heartbeat is stale, if the time difference is greater than
        // HEARTBEAT_EXPIRE_TIME
        if (time(NULL) - localTable[i].lastHeartbeat > HEARTBEAT_EXPIRE_TIME) {
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
  char message[MAX_MESSAGE_LENGTH];
  snprintf(message, sizeof(message), "message_type=HEARTBEAT&name=%s",
           serviceName);
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
  char buffer[MAX_MESSAGE_LENGTH];
  multicast_setup_recv(receiveChannel);
  while (1) {
    while (multicast_check_receive(receiveChannel) == 0) {
      printf("repeat..service is checking messages .. \n");
    }
    multicast_receive(receiveChannel, buffer, MAX_MESSAGE_LENGTH);
    char *message_type = NULL;
    char *unusedName = NULL;
    decode_type_name(buffer, &message_type, &unusedName);
    if (strcmp(message_type, "DISCOVERY") == 0) {
      printf("Service: %s received DISCOVERY message\n", serviceName);
      // send NOTIFICATION message if DISCOVERY message is received
      send_notification(sendingChannel, serviceName, attr, attrNum);
    }
  }
  return NULL;
};

// void *service_send_ads(void *args) {
//   AdSenderArgs *adArgs = (AdSenderArgs *)args;
//   mcast_t *channel = adArgs->sendingChannel;
//   char *serviceName = adArgs->serviceName;
//   char *adName = adArgs->adName;
//   char *adValue = adArgs->adValue;
//   free(adArgs);
//   char message[256];
//   snprintf(message, sizeof(message), "type=AD&name=%s&%s=%s", serviceName,
//            adName, adValue);
//   // send AD message
//   multicast_send(channel, message, strlen(message));
//   return NULL;
// }

int zcs_init(int type) {
  if (type != ZCS_APP_TYPE && type != ZCS_SERVICE_TYPE) {
    return -1;
  }

  if (type == ZCS_APP_TYPE) {
    // create a sending multicast channel for app
    mcast_t *appSendingChannel =
        multicast_init(APP_SEND_CHANNEL_IP, SERVICE_LPORT, UNUSED_PORT);
    // send DISCOVERY message
    multicast_send(appSendingChannel, "message_type=DISCOVERY",
                   strlen("message_type=DISCOVERY"));
    // create a receiving multicast channel for app
    mcast_t *appReceivingChannel =
        multicast_init(SERVICE_SEND_CHANNEL_IP, UNUSED_PORT, APP_LPORT);

    // listen for messages in another thread
    pthread_create(&messageListener, NULL, app_listen_messages,
                   appReceivingChannel);

    // validate the service status in another thread
    pthread_create(&heartbeatChecker, NULL, app_check_heartbeat, NULL);
  } else if (type == ZCS_SERVICE_TYPE) {
  }
  isInitialized = true;
  return 0;
}

int zcs_start(char *name, zcs_attribute_t attr[], int num) {
  assert(name != NULL && strlen(name) < 64);
  if (!isInitialized) {
    printf("ZCS not initialized.\n");
    return -1;
  }
  // set global serviceName
  global_service_name = name;

  // create a sending multicast channel for service
  mcast_t *serviceSendingChannel =
      multicast_init(SERVICE_SEND_CHANNEL_IP, APP_LPORT, UNUSED_PORT);

  // send NOTIFICATION message
  send_notification(serviceSendingChannel, name, attr, num);
  // send HEARTBEAT message periodically in another thread

  HeartbeatSenderArgs *heartbeatArgs =
      (HeartbeatSenderArgs *)malloc(sizeof(HeartbeatSenderArgs));
  heartbeatArgs->channel = serviceSendingChannel;
  heartbeatArgs->serviceName = name;
  pthread_create(&heartbeatSender, NULL, service_send_heartbeat, heartbeatArgs);

  // create a receiving multicast channel for service
  mcast_t *serviceReceivingChannel =
      multicast_init(APP_SEND_CHANNEL_IP, UNUSED_PORT, SERVICE_LPORT);

  // listen for discovery in another thread periodically
  DiscoveryListenerArgs *discoveryArgs =
      (DiscoveryListenerArgs *)malloc(sizeof(DiscoveryListenerArgs));
  discoveryArgs->receivingChannel = serviceReceivingChannel;
  discoveryArgs->sendingChannel = serviceSendingChannel;
  discoveryArgs->serviceName = name;
  discoveryArgs->attrNum = num;
  discoveryArgs->attr = attr;
  pthread_create(&discoveryListener, NULL, service_listen_discovery,
                 discoveryArgs);
  isStarted = true;
  return 0;
}

// assuming that the post duration and attempt are defined by the caller program
int zcs_post_ad(char *ad_name, char *ad_value) {
  assert(ad_name != NULL && ad_value != NULL);
  if (!isStarted) {
    printf("ZCS not started.\n");
    return 0;
  }
  static int postCount = 0;
  // create a service sending multicast channel
  mcast_t *serviceSendingChannel =
      multicast_init(SERVICE_SEND_CHANNEL_IP, APP_LPORT, UNUSED_PORT);

  // send advertisement
  char message[MAX_MESSAGE_LENGTH];
  snprintf(message, sizeof(message), "message_type=advertisement&name=%s&%s=%s",
           global_service_name, ad_name, ad_value);
  multicast_send(serviceSendingChannel, message, strlen(message));
  postCount++;
  return postCount;
}

int zcs_query(char *attr_name, char *attr_value, char *node_names[],
              int namelen) {
  printf("Querying for %s = %s\n", attr_name, attr_value);

  pthread_mutex_lock(
      &localTableLock);  // Ensure thread-safe access to localTable
  int found = 0;         // Counter for found nodes

  for (int i = 0; i < MAX_SERVICE_NUM && found < namelen; i++) {
    for (int j = 0; j < MAX_ATTR_NUM; j++) {
      if (localTable[i].attributes[j].attr_name != NULL) {
        printf("found attribute %s:%s\n", localTable[i].attributes[j].attr_name,
               localTable[i].attributes[j].value);
      }
      if (localTable[i].attributes[j].attr_name != NULL &&
          strcmp(localTable[i].attributes[j].attr_name, attr_name) == 0 &&
          strcmp(localTable[i].attributes[j].value, attr_value) == 0) {
        node_names[found] =
            strdup(localTable[i].serviceName);  // Duplicate string to avoid
        printf("HERE\n");                       // pointing to freed memory
        found++;
        break;  // Found a match, no need to check further attributes for this
                // service
      }
    }
  }

  pthread_mutex_unlock(&localTableLock);  // Release lock after access
  return found;  // Return the number of matching nodes found
}

int zcs_get_attribs(char *name, zcs_attribute_t attr[], int *num) {
  // Ensure the input parameters are valid
  if (name == NULL || attr == NULL || num == NULL || *num <= 0) {
    return -1;  // Invalid arguments
  }

  pthread_mutex_lock(&localTableLock);  // Ensure thread-safe access

  for (int i = 0; i < MAX_SERVICE_NUM; i++) {
    if (localTable[i].serviceName != NULL &&
        strcmp(localTable[i].serviceName, name) == 0) {
      // Found the service, now copy its attributes
      int count = 0;
      for (int j = 0; j < MAX_ATTR_NUM && j < *num; j++) {
        if (localTable[i].attributes[j].attr_name != NULL) {
          attr[count].attr_name = strdup(localTable[i].attributes[j].attr_name);
          attr[count].value = strdup(localTable[i].attributes[j].value);

          // Check for strdup failure
          if (attr[count].attr_name == NULL || attr[count].value == NULL) {
            // Free any allocated strings on error
            for (int k = 0; k <= count; k++) {
              free(attr[k].attr_name);  // It's safe to call free on NULL
              free(attr[k].value);
            }
            pthread_mutex_unlock(&localTableLock);
            return -1;  // Memory allocation error
          }
          count++;
        }
      }
      *num = count;  // Update the actual number of attributes copied
      pthread_mutex_unlock(&localTableLock);
      return 0;  // Successfully retrieved attributes
    }
  }

  pthread_mutex_unlock(&localTableLock);
  return -1;  // Service not found
}

int zcs_listen_ad(char *name, zcs_cb_f cback) {
  assert(name != NULL && cback != NULL && strlen(name) < 64);
  if (!isInitialized) {
    printf("ZCS not initialized.\n");
    return -1;
  }

  // register the callback function in app's localTableEntry
  pthread_mutex_lock(&localTableLock);
  for (int i = 0; i < MAX_SERVICE_NUM; i++) {
    if (localTable[i].serviceName != NULL &&
        strcmp(localTable[i].serviceName, name) == 0) {
      localTable[i].callback = cback;
      break;
    }
  }
  pthread_mutex_unlock(&localTableLock);

  return 0;
}

// TODO: implement the shutdown checking logic inside
// every continuously running thread
int zcs_shutdown() {
  if (!isStarted) {
    printf("ZCS not initialized.\n");
    return -1;
  }
  // set termination flag to true
  isTerminating = true;

  // TODO: this is temporary, haven't implemented the shutdown logic
  pthread_join(messageListener, NULL);
  pthread_join(heartbeatChecker, NULL);
  pthread_join(heartbeatSender, NULL);
  pthread_join(discoveryListener, NULL);

  // free the serviceName, attr_name, and value in localTable
  pthread_mutex_lock(&localTableLock);
  for (int i = 0; i < MAX_SERVICE_NUM; i++) {
    if (localTable[i].serviceName != NULL) {
      free(localTable[i].serviceName);
      for (int j = 0; j < MAX_ATTR_NUM; j++) {
        if (localTable[i].attributes[j].attr_name != NULL) {
          free(localTable[i].attributes[j].attr_name);
          free(localTable[i].attributes[j].value);
        }
      }
    }
  }
  pthread_mutex_unlock(&localTableLock);
  return 0;
}

void zcs_log() {
  // Implement logging functionality
    pthread_mutex_lock(&localTableLock);  

    printf("Current Services Status:\n");
    printf("%-25s %-10s %-20s\n", "Service Name", "Status", "Last Heartbeat");

    for (int i = 0; i < MAX_SERVICE_NUM; i++) {
        if (localTable[i].serviceName != NULL) { // Check if entry is used
            char buffer[26]; // Buffer to hold the formatted date and time
            time_t heartbeatTime = (time_t)localTable[i].lastHeartbeat;
            struct tm* tm_info = localtime(&heartbeatTime);

            strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info); // Format time into string

            printf("%-25s %-10s %-20s\n",
                   localTable[i].serviceName,
                   localTable[i].status ? "UP" : "DOWN",
                   buffer);
        }
    }

    pthread_mutex_unlock(&localTableLock); // Release lock
}
