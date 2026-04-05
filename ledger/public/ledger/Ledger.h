#pragma once

#include <utils/Config.h>

namespace ledger {
    /**
     * This class is intended to read all the orders put in via gateways, and maintain the positions
     * until they are matched by the market.
     * 
     * This will simulate orders being actually matched and a chance to evaluate the efficiency of
     * various trading strategies.
     * 
     * This is intended for medium/long term trading; we cannot compete with high frequency trading
     * environments in this setup. MMs have too much of an advantage.
     * 
     */
    class Ledger : public common::IApplicationService {
    public:
        Ledger(common::CommonComponents&& components);

        void start();
    };
}