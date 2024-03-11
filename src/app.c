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
  // argument should be apps IP address and then services IP address
  if (argc != 2) {
    printf(
        "Please pass the correct number of args. \nUsage: ./app <apps IP "
        "address> <services IP address>");
    return 1;
  }
  int rv;
  // receive the addresses from the args
  rv = zcs_init(ZCS_APP_TYPE, argv[1], argv[2]);
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