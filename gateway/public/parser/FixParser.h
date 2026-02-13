#pragma once

#include <string>

namespace gateway {

    /**
     * This class is responsible for parsing FIX messages.
     * 
     * It provides an interface to parse a given FIX message
     * string and extract relevant information from it.
     */
    class FixParser {
    public:
        FixParser();
        ~FixParser();

        void parse(const std::string& message);
    };
}