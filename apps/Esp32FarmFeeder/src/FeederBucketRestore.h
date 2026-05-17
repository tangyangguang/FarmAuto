#pragma once

#include "FeederBucket.h"

FeederBucketResult restoreFeederBucketParts(FeederBucketService& buckets,
                                            const FeederBucketSnapshot& calibration,
                                            const FeederBucketSnapshot& dynamicState);
