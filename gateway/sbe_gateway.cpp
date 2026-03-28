#include <iostream>

#include <args-parser/all.hpp>

#include "utils/Config.h"
#include "gateway/FixSbeGateway.h"


int main(int argc, char* argv[]) {

    std::string config_file_name;

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

        if(cmd.isDefined("-v")) {
            config_file_name = cmd.value("-v");
        }
    } catch(const Args::HelpHasBeenPrintedException&)
    {
        return 0;
    } catch(const Args::BaseException& x )
    {
        std::cout << x.desc() << "\n";
        return 1;
    }

    // get config
    libconfig::Config config;

    try
    {
        config.readFile(config_file_name);
    }
    catch(const libconfig::FileIOException &fioex)
    {
        std::cerr << "I/O error while reading file." << std::endl;
        return(EXIT_FAILURE);
    }

    auto logger = spdlog::daily_logger_mt("gateway_logger", "/var/log/gateway.log", 2, 30, false, 7);

    // start up gateway
    try {
        gateway::FixSbeGateway gateway(common::CommonComponents{ config, *logger.get() });
        gateway.run();
    } catch (std::exception& e) {
        std::cout << std::format("Encountered error during runtime: {}", e.what());
    }
    //...profit?
    return 0;
}