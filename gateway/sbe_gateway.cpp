#include <iostream>
#include <libconfig.h++>
#include <args-parser/all.hpp>


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

    // start up gateway
    

    //...profit?
    return 0;
}