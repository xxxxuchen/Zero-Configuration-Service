#ifndef __ZCS_H__
#define __ZCS_H__

#define ZCS_APP_TYPE 1
#define ZCS_SERVICE_TYPE 2

#define SERVICE_LPORT 1100
#define APP_LPORT 1700
#define UNUSED_PORT 1000

#define MAX_SERVICE_NUM 20
#define MAX_ATTR_NUM 10          // max number of attributes for each service
#define HEARTBEAT_EXPIRE_TIME 5  // in seconds
#define MAX_MESSAGE_LENGTH 256
#define QUERY_WAIT_TIME 3  // seconds

typedef struct {
  char *attr_name;
  char *value;
} zcs_attribute_t;

typedef void (*zcs_cb_f)(char *, char *);

int zcs_init(int type, char *app_addr, char *service_addr);
int zcs_start(char *name, zcs_attribute_t attr[], int num);
int zcs_post_ad(char *ad_name, char *ad_value);
int zcs_query(char *attr_name, char *attr_value, char *node_names[],
              int namelen);
int zcs_get_attribs(char *name, zcs_attribute_t attr[], int *num);
int zcs_listen_ad(char *name, zcs_cb_f cback);
int zcs_shutdown();
void zcs_log();

#endif
