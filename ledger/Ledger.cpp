#include "ledger/Ledger.h"

namespace ledger {

    Ledger::Ledger(common::CommonComponents&& components) {

    }
    void Ledger::start() {
        // we want to spin up a poll runner and run in a worker thread.
        
    }

    void Ledger::run() {

    }

    void Ledger::stop() {
        // quit polling the queue
    }
}