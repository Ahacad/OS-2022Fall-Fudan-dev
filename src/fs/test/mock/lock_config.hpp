#pragma once

extern "C" {
#include "common/defines.h"
}

struct MockLockConfig {
    static constexpr bool SpinLockBlocksCPU = false;
    static constexpr bool SpinLockForbidsWait = true;
    static constexpr int WaitTimeoutSeconds = 10;
};

extern MockLockConfig mock_lock;
