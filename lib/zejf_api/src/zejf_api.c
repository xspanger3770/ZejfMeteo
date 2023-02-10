#include "zejf_api.h"
#include "zejf_data.h"
#include "zejf_routing.h"
#include "zejf_network.h"

void zejf_init(void) {
    data_init();
    routing_init();
    network_init();
}

void zejf_destroy(void) {
    network_destroy();
    routing_destroy();
    data_destroy();
}