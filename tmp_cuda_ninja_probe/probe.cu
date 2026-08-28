#include <cuda_runtime.h>
__global__ void k() {}
void probe_host() { k<<<1,1>>>(); }
