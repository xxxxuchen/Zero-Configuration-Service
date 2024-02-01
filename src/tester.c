#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "multicast.h"

char buffer[100];

int main(int argc, char *argv[]) {
  if (argc <= 3) {
    printf("Usage: tester src-port dest-port message \n");
    exit(0);
  }

  printf("Source port: %d\n", atoi(argv[1]));
  printf("Destination port: %d\n", atoi(argv[2]));

  mcast_t *m = multicast_init("224.1.1.1", atoi(argv[1]), atoi(argv[2]));

  char *msg = argv[3];
  multicast_setup_recv(m);
  // printf("==============\n");
  // multicast_send(m, msg, strlen(msg));
  // printf("==============\n");
  while (1) {
    printf("next iteration\n");
    while (multicast_check_receive(m) == 0) {
          printf("repeat.. \n");
    }
    multicast_send(m, msg, strlen(msg));
    multicast_receive(m, buffer, 100);
    sleep(10);
    printf("Received:  %s\r\n", buffer);
    fflush(stdout);
  }
}
