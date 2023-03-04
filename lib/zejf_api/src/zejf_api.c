#include "zejf_api.h"
#include "data_request.h"
#include "zejf_data.h"
#include "zejf_network.h"
#include "zejf_routing.h"

void zejf_init(void)
{
    data_init();
    data_requests_init();
    routing_init();
    network_init();
}

void zejf_destroy(void)
{
    data_save();
    network_destroy();
    routing_destroy();
    data_requests_destroy();
    data_destroy();
}