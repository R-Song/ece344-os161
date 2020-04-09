/*
 * Global enable flags for features added in vm functions
 *
 */

#include <vm_features_enable.h>

/* Loading on demand enabling flag */
extern int LOAD_ON_DEMAND_ENABLE;

void flag_bootstrap(int load_on_demand){
    LOAD_ON_DEMAND_ENABLE = load_on_demand;    
}

