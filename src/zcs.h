#ifndef __ZCS_H__
#define __ZCS_H__

typedef struct {
    char *attr_name;
    char *value;
} zcs_attribute_t;

typedef void (*zcs_cb_f)(char *, char *);

int zcs_init();
int zcs_start(char *name, zcs_attribute_t attr[], int num);
int zcs_post_ad(char *ad_name, char *ad_value);
int zcs_query(char *attr_name, char *attr_value, char *node_names[], int namelen);
int zcs_get_attribs(char *name, zcs_attribute_t attr[], int *num);
int zcs_listen_ad(char *name, zcs_cb_f cback);
int zcs_shutdown();
void zcs_log();

#endif

