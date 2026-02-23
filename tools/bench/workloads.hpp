#pragma once

#include "bench_types.hpp"

// Each function returns a fully configured Workload for that benchmark type.
Workload make_workload_seqread();
Workload make_workload_seqwrite();
Workload make_workload_randread();
Workload make_workload_randwrite();
Workload make_workload_meta();
Workload make_workload_mixed();
