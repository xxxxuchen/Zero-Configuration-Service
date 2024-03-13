#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "multicast.h"
#include "zcs.h"

typedef struct listenLAN1Args {
  mcast_t *receivingChannel;
  mcast_t *sendingChannel;
} ListenLAN1Args;

typedef struct listenLAN2Args {
  mcast_t *receivingChannel;
  mcast_t *sendingChannel;
} ListenLAN2Args;

void *relay_listen_LAN1(void *args) {
  ListenLAN1Args *listenLAN1Args = (ListenLAN1Args *)args;
  mcast_t *relayReceivingChannel1 = listenLAN1Args->receivingChannel;
  mcast_t *relaySendingChannel2 = listenLAN1Args->sendingChannel;
  char message[MAX_MESSAGE_LENGTH];
  multicast_setup_recv(relayReceivingChannel1);
  while (1) {
    multicast_receive(relayReceivingChannel1, message, MAX_MESSAGE_LENGTH);
    printf("Received:  %s\r\n", message);
    fflush(stdout);
    // forward the message to the other LAN
    multicast_send(relaySendingChannel2, message, strlen(message));
  }
}

void *relay_listen_LAN2(void *args) {
  ListenLAN2Args *listenLAN2Args = (ListenLAN2Args *)args;
  mcast_t *relayReceivingChannel2 = listenLAN2Args->receivingChannel;
  mcast_t *relaySendingChannel1 = listenLAN2Args->sendingChannel;
  char message[MAX_MESSAGE_LENGTH];
  multicast_setup_recv(relayReceivingChannel2);
  while (1) {
    multicast_receive(relayReceivingChannel2, message, MAX_MESSAGE_LENGTH);
    printf("Received:  %s\r\n", message);
    fflush(stdout);
    // forward the message to the other LAN
    multicast_send(relaySendingChannel1, message, strlen(message));
  printf("SENTTTTT\n");
    //   // send discovery message to both LANs
  // multicast_send(relaySendingChannel1, "message_type=DISCOVERY",
  //                strlen("message_type=DISCOVERY"));
  // multicast_send(relaySendingChannel2, "message_type=DISCOVERY",
  //                strlen("message_type=DISCOVERY"));

  
  
  
  }
}

int main(int argc, char *argv[]) {
  if (argc != 5) {
    printf(
        "Usage: %s <Recv_IP_LAN1> <Send_IP_LAN1> <Recv_IP_LAN2> "
        "<Send_IP_LAN2>\n",
        argv[0]);
  }

  // create a relay receiving multicast channel for LAN1
  mcast_t *relayReceivingChannelServices1 =
      multicast_init(argv[1], UNUSED_PORT, APP_LPORT);

  // create a relay sending multicast channel for LAN1
  mcast_t *relaySendingChannelServices1 =
      multicast_init(argv[2], SERVICE_LPORT, UNUSED_PORT);

  // create a relay receiving multicast channel for LAN2
  mcast_t *relayReceivingChannelServices2 =
      multicast_init(argv[3], UNUSED_PORT, APP_LPORT);

  // create a relay sending multicast channel for LAN2
  mcast_t *relaySendingChannelServices2 =
      multicast_init(argv[4], SERVICE_LPORT, UNUSED_PORT);

  // create a relay receiving multicast channel for LAN1
  mcast_t *relayReceivingChannelApps1 =
      multicast_init(argv[1], UNUSED_PORT, SERVICE_LPORT);

  // create a relay sending multicast channel for LAN1
  mcast_t *relaySendingChannelApps1 =
      multicast_init(argv[2], APP_LPORT, UNUSED_PORT);

  // create a relay receiving multicast channel for LAN2
  mcast_t *relayReceivingChannelApps2 =
      multicast_init(argv[3], UNUSED_PORT, SERVICE_LPORT);

  // create a relay sending multicast channel for LAN2
  mcast_t *relaySendingChannelApps2 =
      multicast_init(argv[4], APP_LPORT, UNUSED_PORT);

  // start the relay's listener thread for LAN1
  pthread_t listenerThread1;
  ListenLAN1Args *listenLAN1Args = malloc(sizeof(ListenLAN1Args));
  listenLAN1Args->receivingChannel = relayReceivingChannel1;
  listenLAN1Args->sendingChannel = relaySendingChannel2;

  pthread_create(&listenerThread1, NULL, relay_listen_LAN1,
                 (void *)listenLAN1Args);

  // start the relay's listener thread for LAN2
  pthread_t listenerThread2;
  ListenLAN2Args *listenLAN2Args = malloc(sizeof(ListenLAN2Args));
  listenLAN2Args->receivingChannel = relayReceivingChannel2;
  listenLAN2Args->sendingChannel = relaySendingChannel1;

  pthread_create(&listenerThread2, NULL, relay_listen_LAN2,
                 (void *)listenLAN2Args);

  // send discovery message to both LANs
  multicast_send(relaySendingChannel1, "message_type=DISCOVERY",
                 strlen("message_type=DISCOVERY"));
  multicast_send(relaySendingChannel2, "message_type=DISCOVERY",
                 strlen("message_type=DISCOVERY"));
  pthread_join(listenerThread2, NULL);
  pthread_join(listenerThread1, NULL);

}