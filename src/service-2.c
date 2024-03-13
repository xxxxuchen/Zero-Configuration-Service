#include <stdio.h>
#include <unistd.h>

#include "zcs.h"

int main(int argc, char *argv[]) {
  if (argc < 2) { // Expect at least one multicast address
  printf("Usage: %s <multicast_address_1> [multicast_address_2] ...\n", argv[0]);
  return 1;
  }
  char *app_send_channel_ip = argv[2];
  char *service_send_channel_ip = argv[1];
  int rv;
  rv = zcs_init(ZCS_SERVICE_TYPE, app_send_channel_ip, service_send_channel_ip);
  zcs_attribute_t attribs[] = {{.attr_name = "type", .value = "lamp"},
                               {.attr_name = "location", .value = "bedroom"},
                               {.attr_name = "make", .value = "phillips"}};
  rv = zcs_start("Lamp-Pro", attribs,
                 sizeof(attribs) / sizeof(zcs_attribute_t));
  for (int i = 0; i < 1000; i++) {
    rv = zcs_post_ad("light", "on");
    sleep(10);
    rv = zcs_post_ad("light", "off");
    sleep(10);
  }
  rv = zcs_shutdown();
}
