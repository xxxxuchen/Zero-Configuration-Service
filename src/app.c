#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "zcs.h"

void hello(char *s, char *r) {
  // printf("Ad successfully received from speaker!!!!!!\n");
  printf("Ad received: %s, with value: %s\n", s, r);
  zcs_log();
}

int main(int argc, char *argv[]) {
  if (argc < 2) { // Expect at least one multicast address
    printf("Usage: %s <multicast_address_1> [multicast_address_2] ...\n", argv[0]);
    return 1;
  }
  char *app_send_channel_ip = argv[1];
  char *service_send_channel_ip = argv[2];
  int rv;
  rv = zcs_init(ZCS_APP_TYPE,app_send_channel_ip,service_send_channel_ip);
  char *names[10];
  rv = zcs_query("type", "speaker", names, 10);
  if (rv > 0) {
    zcs_attribute_t attrs[5];
    int anum = 5;
    rv = zcs_get_attribs(names[0], attrs, &anum);
    if ((strcmp(attrs[1].attr_name, "location") == 0) &&
        (strcmp(attrs[1].value, "kitchen") == 0)) {
      rv = zcs_listen_ad(names[0], hello);
    }
  }
  rv = zcs_shutdown();
}