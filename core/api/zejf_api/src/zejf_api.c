#include "zejf_api.h"
#include "data_request.h"
#include "zejf_data.h"
#include "zejf_network.h"
#include "zejf_routing.h"

bool zejf_init(void)
{
    bool res = true;
    res &= data_init();
    res &= data_requests_init();
    res &= routing_init();
    res &= network_init();
    return res;
}

void zejf_destroy(void)
{
    data_save();
    network_destroy();
    routing_destroy();
    data_requests_destroy();
    data_destroy();
}
