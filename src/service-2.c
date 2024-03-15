#include <stdio.h>
#include <unistd.h>

#include "zcs.h"

int main(int argc, char *argv[]) {
  // argument should be apps IP address and then services IP address
  if (argc != 3) {
    printf(
        "Please pass the correct number of args. \nUsage: ./app <apps IP "
        "address> <services IP address>");
    return 1;
  }
  int rv;
  rv = zcs_init(ZCS_SERVICE_TYPE, argv[1], argv[2]);
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
