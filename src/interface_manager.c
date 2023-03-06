#include "interface_manager.h"
#include "stddef.h"
#include "pthread.h"
#include "zejf_meteo.h"

Interface usb_interface_1 = {
    .type = USB
};

static Interface* all_interfaces[INTERFACES_MAX];
static size_t interface_count;
static int next_uid;

void interfaces_init(void) {
    interface_count = 0;
    next_uid = 0;

    interface_add(&usb_interface_1);
}

void interfaces_destroy(void) {
    for(size_t i = 0; i < interface_count; i++){
        printf("Destroy uid %d\n", all_interfaces[i]->uid);
        all_interfaces[i] = NULL;
    }
}

bool interface_add(Interface* interface) {
    pthread_mutex_lock(&zejf_lock);
    if(interface_count >= INTERFACES_MAX){
        pthread_mutex_unlock(&zejf_lock);
        return false;
    }

    interface->uid = next_uid++;
    all_interfaces[interface_count] = interface;

    interface_count++;

    pthread_mutex_unlock(&zejf_lock);
    return true;
}

bool interface_remove(int uid) {
    pthread_mutex_lock(&zejf_lock);
    
    bool found = false;
    for(size_t i = 0; i < interface_count; i++){
        Interface* interface = all_interfaces[i];
        found |= interface->uid == uid;
        if(found && i != interface_count - 1) {
            all_interfaces[i] = all_interfaces[i + 1];
        }
    }

    if(found){
        interface_count--;
        all_interfaces[interface_count] = NULL;
    }

    pthread_mutex_unlock(&zejf_lock);
    return false;
}

void get_all_interfaces(Interface ***interfaces, size_t *length)
{
    *interfaces = (Interface**)all_interfaces;
    *length = interface_count;
}

int main(){
    interfaces_init();

    Interface ifcs[10];

    for(int i = 0; i < 10; i++){
        Interface* tst = &ifcs[i];
        tst->handle = 5;
        tst->type = USB;

        printf("%p\n", (void*)tst);

        interface_add(tst);
    }

    interface_remove(5);

    interfaces_destroy();

    return 0;
}