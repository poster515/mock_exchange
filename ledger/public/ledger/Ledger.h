#pragma once

#include <memory>

#include <utils/Config.h>
#include "archive/ArchiveSubscription.h"

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

        void start() override final;
        void run() override final;
        void stop() override final;

    private:
        std::unique_ptr<archive::ArchiveSubscription> subscription;
    };
}