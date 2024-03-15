#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "multicast.h"
#include "zcs.h"

#define FORWARD_TAG "forward=true&"
typedef struct listenLANArgs {
  mcast_t *receivingChannel;
  mcast_t *sendingChannel;
} ListenThreadArgs;

// add the forward=true& tag to the front of the message
void encodeForwardMessage(char message[]) {
  const char prefix[] = FORWARD_TAG;

  // Calculate the length of the new message
  int newLength = strlen(prefix) + strlen(message) + 1;

  // Create a temporary buffer to hold the new message
  char temp[MAX_MESSAGE_LENGTH];

  // Copy the prefix and existing message into the temporary buffer
  strcpy(temp, prefix);
  strcat(temp, message);

  // Copy the new message back into the original message buffer
  strncpy(message, temp, newLength);

  // Ensure the new message is null-terminated
  message[newLength - 1] = '\0';
}

// listen for messages from services in LAN1 and forward them to apps in LAN2
void *relay_listen_services1(void *args) {
  ListenThreadArgs *listenLAN1Args = (ListenThreadArgs *)args;
  mcast_t *relayReceivingChannel1 = listenLAN1Args->receivingChannel;
  mcast_t *relaySendingChannel2 = listenLAN1Args->sendingChannel;
  char message[MAX_MESSAGE_LENGTH];
  multicast_setup_recv(relayReceivingChannel1);
  while (1) {
    if (multicast_check_receive(relayReceivingChannel1) == 0) {
      continue;
    }
    multicast_receive(relayReceivingChannel1, message, MAX_MESSAGE_LENGTH);
    // check if the message is a forward message, if so, discard it
    if (strstr(message, FORWARD_TAG) != NULL) {
      memset(message, 0, MAX_MESSAGE_LENGTH);
      continue;
    }

    // add the forward=true tag to the front of the message
    encodeForwardMessage(message);
    printf("Received1:  %s\r\n", message);
    fflush(stdout);
    // forward the message to the other LAN
    multicast_send(relaySendingChannel2, message, strlen(message));
    memset(message, 0, MAX_MESSAGE_LENGTH);
  }
}

// listen for messages from services in LAN2 and forward them to apps in LAN1
void *relay_listen_services2(void *args) {
  ListenThreadArgs *listenLAN2Args = (ListenThreadArgs *)args;
  mcast_t *relayReceivingChannel2 = listenLAN2Args->receivingChannel;
  mcast_t *relaySendingChannel1 = listenLAN2Args->sendingChannel;
  char message[MAX_MESSAGE_LENGTH];
  multicast_setup_recv(relayReceivingChannel2);
  while (1) {
    if (multicast_check_receive(relayReceivingChannel2) == 0) {
      continue;
    }
    multicast_receive(relayReceivingChannel2, message, MAX_MESSAGE_LENGTH);
    // check if the message is a forward message, if so, discard it
    if (strstr(message, FORWARD_TAG) != NULL) {
      memset(message, 0, MAX_MESSAGE_LENGTH);
      continue;
    }
    // add the forward=true tag to the front of the message
    encodeForwardMessage(message);
    printf("Received2:  %s\r\n", message);
    fflush(stdout);
    // forward the message to the other LAN
    multicast_send(relaySendingChannel1, message, strlen(message));
    memset(message, 0, MAX_MESSAGE_LENGTH);
  }
}

// listen for messages from apps in LAN1 and forward them to services in LAN2
void *relay_listen_apps1(void *args) {
  ListenThreadArgs *listenLAN1Args = (ListenThreadArgs *)args;
  mcast_t *relayReceivingChannel1 = listenLAN1Args->receivingChannel;
  mcast_t *relaySendingChannel2 = listenLAN1Args->sendingChannel;
  char message[MAX_MESSAGE_LENGTH];
  multicast_setup_recv(relayReceivingChannel1);
  while (1) {
    if (multicast_check_receive(relayReceivingChannel1) == 0) {
      continue;
    }
    multicast_receive(relayReceivingChannel1, message, MAX_MESSAGE_LENGTH);
    // check if the message is a forward message, if so, discard it
    if (strstr(message, FORWARD_TAG) != NULL) {
      memset(message, 0, MAX_MESSAGE_LENGTH);
      continue;
    }
    // add the forward=true tag to the front of the message
    encodeForwardMessage(message);
    printf("Received3:  %s\r\n", message);
    fflush(stdout);
    // forward the message to the other LAN
    multicast_send(relaySendingChannel2, message, strlen(message));
    memset(message, 0, MAX_MESSAGE_LENGTH);
  }
}

// listen for messages from apps in LAN2 and forward them to services in LAN1
void *relay_listen_apps2(void *args) {
  ListenThreadArgs *listenLAN2Args = (ListenThreadArgs *)args;
  mcast_t *relayReceivingChannel2 = listenLAN2Args->receivingChannel;
  mcast_t *relaySendingChannel1 = listenLAN2Args->sendingChannel;
  char message[MAX_MESSAGE_LENGTH];
  multicast_setup_recv(relayReceivingChannel2);
  while (1) {
    if (multicast_check_receive(relayReceivingChannel2) == 0) {
      continue;
    }
    multicast_receive(relayReceivingChannel2, message, MAX_MESSAGE_LENGTH);
    // check if the message is a forward message, if so, discard it
    if (strstr(message, FORWARD_TAG) != NULL) {
      memset(message, 0, MAX_MESSAGE_LENGTH);
      continue;
    }
    // add the forward=true tag to the front of the message
    encodeForwardMessage(message);
    printf("Received4:  %s\r\n", message);
    fflush(stdout);
    // forward the message to the other LAN
    multicast_send(relaySendingChannel1, message, strlen(message));
    memset(message, 0, MAX_MESSAGE_LENGTH);
  }
}

int main(int argc, char *argv[]) {
  if (argc != 5) {
    printf(
        "Usage: %s <Recv_IP_LAN1> <Send_IP_LAN1> <Recv_IP_LAN2> "
        "<Send_IP_LAN2>\n",
        argv[0]);
  }

  // create a relay receiving multicast channel from services in LAN1
  mcast_t *relayReceivingChannelServices1 =
      multicast_init(argv[1], UNUSED_PORT, APP_LPORT);

  // create a relay sending multicast channel to services in LAN1
  mcast_t *relaySendingChannelServices1 =
      multicast_init(argv[2], SERVICE_LPORT, UNUSED_PORT);

  // create a relay receiving multicast channel from services in LAN2
  mcast_t *relayReceivingChannelServices2 =
      multicast_init(argv[3], UNUSED_PORT, APP_LPORT);

  // create a relay sending multicast channel to services in LAN2
  mcast_t *relaySendingChannelServices2 =
      multicast_init(argv[4], SERVICE_LPORT, UNUSED_PORT);

  //----------------------------------------------------------------

  // create a relay receiving multicast channel from apps in LAN1
  mcast_t *relayReceivingChannelApps1 =
      multicast_init(argv[2], UNUSED_PORT, SERVICE_LPORT);

  // create a relay sending multicast channel to apps in LAN1
  mcast_t *relaySendingChannelApps1 =
      multicast_init(argv[1], APP_LPORT, UNUSED_PORT);

  // create a relay receiving multicast channel from apps in LAN2
  mcast_t *relayReceivingChannelApps2 =
      multicast_init(argv[4], UNUSED_PORT, SERVICE_LPORT);

  // create a relay sending multicast channel to apps in LAN2
  mcast_t *relaySendingChannelApps2 =
      multicast_init(argv[3], APP_LPORT, UNUSED_PORT);

  pthread_t listenServicesThread1;
  ListenThreadArgs *listenServices1Args = malloc(sizeof(ListenThreadArgs));
  listenServices1Args->receivingChannel = relayReceivingChannelServices1;
  listenServices1Args->sendingChannel = relaySendingChannelApps2;

  pthread_create(&listenServicesThread1, NULL, relay_listen_services1,
                 (void *)listenServices1Args);

  pthread_t listenerServicesThread2;
  ListenThreadArgs *listenServices2Args = malloc(sizeof(ListenThreadArgs));
  listenServices2Args->receivingChannel = relayReceivingChannelServices2;
  listenServices2Args->sendingChannel = relaySendingChannelApps1;

  pthread_create(&listenerServicesThread2, NULL, relay_listen_services2,
                 (void *)listenServices2Args);

  pthread_t listenAppsThread1;
  ListenThreadArgs *listenApps1Args = malloc(sizeof(ListenThreadArgs));
  listenApps1Args->receivingChannel = relayReceivingChannelApps1;
  listenApps1Args->sendingChannel = relaySendingChannelServices2;

  pthread_create(&listenAppsThread1, NULL, relay_listen_apps1,
                 (void *)listenApps1Args);

  pthread_t listenAppsThread2;
  ListenThreadArgs *listenApps2Args = malloc(sizeof(ListenThreadArgs));
  listenApps2Args->receivingChannel = relayReceivingChannelApps2;
  listenApps2Args->sendingChannel = relaySendingChannelServices1;
  pthread_create(&listenAppsThread2, NULL, relay_listen_apps2,
                 (void *)listenApps2Args);

  pthread_join(listenServicesThread1, NULL);
  pthread_join(listenerServicesThread2, NULL);
  pthread_join(listenAppsThread1, NULL);
  pthread_join(listenAppsThread2, NULL);
}