#include "interface_manager.h"
#include "pthread.h"
#include "stddef.h"
#include "zejf_api.h"
#include "zejf_meteo.h"

Interface usb_interface_1 = {
    .type = USB
};

Interface *all_interfaces[INTERFACES_MAX];
size_t interface_count;
static int next_uid;

void interfaces_init(void)
{
    interface_count = 0;
    next_uid = 0;

    interface_add(&usb_interface_1);
}

void interfaces_destroy(void)
{
    for (size_t i = 0; i < interface_count; i++) {
        all_interfaces[i] = NULL;
    }
}

bool interface_add(Interface *interface)
{
    pthread_mutex_lock(&zejf_lock);
    if (interface_count >= INTERFACES_MAX) {
        pthread_mutex_unlock(&zejf_lock);
        return false;
    }

    interface->uid = next_uid++;
    all_interfaces[interface_count] = interface;

    interface_count++;

    pthread_mutex_unlock(&zejf_lock);
    return true;
}

bool interface_remove(int uid)
{
    bool found = false;
    for (size_t i = 0; i < interface_count; i++) {
        Interface *interface = all_interfaces[i];
        interface_removed(interface);
        found |= interface->uid == uid;
        if (found && i != interface_count - 1) {
            all_interfaces[i] = all_interfaces[i + 1];
        }
    }

    if (found) {
        interface_count--;
        all_interfaces[interface_count] = NULL;
    }
    return false;
}

void get_all_interfaces(Interface ***interfaces, size_t *length)
{
    *interfaces = (Interface **) all_interfaces;
    *length = interface_count;
}
