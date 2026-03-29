#include <iostream>

#include <args-parser/all.hpp>

#include "utils/Config.h"
#include "gateway/SbeGateway.h"


int main(int argc, char* argv[]) {

    std::string config_file_name;

    auto logger = spdlog::daily_logger_mt("gateway_logger", "/var/log/gateway.log", 2, 30, false, 7);

    try {
        // get command line args
        Args::CmdLine cmd(argc, argv, Args::CmdLine::CommandIsRequired);
        cmd.addArgWithFlagAndName( 'b', "bool", false, false, "Boolean flag",
                "Boolean flag, used without value" )
            .addArgWithFlagAndName( 'v', "value", true, false, "With value",
                "Argument with value", "", "VAL" )
            .addArgWithFlagAndName( 'f', "filename", true, true, "config file name",
                "Config file name - should be json", "", "VAL" )
            .addHelp( true, argv[ 0 ], "CLI with boolean and value." );

        cmd.parse();

        if(cmd.isDefined("-f")) {
            config_file_name = cmd.value("-f");
        }
    }
    catch(const Args::HelpHasBeenPrintedException&)
    {
        return 0;
    }
    catch(const Args::BaseException& x)
    {
        logger->error("Command line error: {}", x.desc());
        return 1;
    }

    // get config
    libconfig::Config config;

    try
    {
        config.readFile(config_file_name);
    }
    catch(const libconfig::FileIOException& fioex)
    {
        logger->warn("I/O error while reading config file: {}, err: {}", config_file_name, fioex.what());
        return(EXIT_FAILURE);
    }
    catch(const libconfig::ParseException& pex)
    {
        logger->warn("Parse error at {}: {} - {}", pex.getFile(), pex.getLine(), pex.getError());
        return(EXIT_FAILURE);
    }

    // start up gateway
    try {
        gateway::SbeGateway gateway(common::CommonComponents{ config, *logger.get() });
        gateway.run();
    }
    catch (std::exception& e)
    {
        logger->error("Encountered error during runtime: {}", e.what());
        return 1;
    }

    return 0;
}