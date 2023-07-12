#include "zejf_api.h"
#include "data_request.h"
#include "zejf_data.h"
#include "zejf_network.h"
#include "zejf_routing.h"

zejf_err zejf_init(void)
{
    zejf_err rv = data_init();

    if(rv != ZEJF_OK){
        return rv;
    }

    rv = data_requests_init();

    if(rv != ZEJF_OK){
        return rv;
    }

    rv = routing_init();

    if(rv != ZEJF_OK){
        return rv;
    }

    rv = network_init();

    if(rv != ZEJF_OK){
        return rv;
    }

    return ZEJF_OK;
}

void zejf_destroy(void)
{
    data_save();
    network_destroy();
    routing_destroy();
    data_requests_destroy();
    data_destroy();
}
