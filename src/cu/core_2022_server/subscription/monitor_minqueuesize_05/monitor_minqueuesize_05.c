#include "monitor_minqueuesize_05.h"
#include "muc_opcua/capacities.h"

_Static_assert(MU_INTERN_MONITORED_QUEUE_DEPTH >= MU_CU_MONITOR_MINQUEUESIZE_05_REQUIRED_DEPTH,
               "Monitor MinQueueSize_05 requires monitored-item queue depth >= 5");

const unsigned char mu_cu_monitor_minqueuesize_05_enabled = 1u;
