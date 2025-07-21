#ifndef MEANREVERSIONSTRATEGY_H
#define MEANREVERSIONSTRATEGY_H

#pragma once

#include "IStrategy.h"

class MeanReversionStrategy : public Strategy {
public:
    MeanReversionStrategy() : Strategy("MeanReversion") {}
};

#endif // MEANREVERSIONSTRATEGY_H