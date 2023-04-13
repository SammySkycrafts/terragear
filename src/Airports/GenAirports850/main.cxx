// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.


#include <chrono>
#include <memory>
#include <string>
#include <iostream>

#include <boost/thread.hpp>

#include <simgear/debug/logstream.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/io/iostreams/sgstream.hxx>
#include <simgear/misc/strutils.hxx>

#include <Include/version.h>

#include "scheduler.hxx"
#include "beznode.hxx"
#include "closedpoly.hxx"
#include "linearfeature.hxx"
#include "parser.hxx"
#include "scheduler.hxx"

using namespace std;

// Display usage
static void usage( int argc, char **argv ) {
    TG_LOG(SG_GENERAL, SG_ALERT, "Usage: " << argv[0] << "\n--input=<apt_file>"
    << "\n--work=<work_dir>\n[ --start-id=abcd ] [ --nudge=n ] "
    << "[--min-lon=<deg>] [--max-lon=<deg>] [--min-lat=<deg>] [--max-lat=<deg>] "
    << "[ --airport=abcd ] [--max-slope=<decimal>] [--threads] [--threads=x]"
    << "[--clear-dem-path] [--dem-path=<path>] [--verbose] [--help] [--log-level=bulk|info|debug|warn|alert]");
}


void setup_default_elevation_sources(string_list& elev_src) {
    elev_src.push_back( "SRTM2-Africa-3" );
    elev_src.push_back( "SRTM2-Australia-3" );
    elev_src.push_back( "SRTM2-Eurasia-3" );
    elev_src.push_back( "SRTM2-Islands-3" );
    elev_src.push_back( "SRTM2-North_America-3" );
    elev_src.push_back( "SRTM2-South_America-3" );
    elev_src.push_back( "DEM-USGS-3" );
    elev_src.push_back( "SRTM-1" );
    elev_src.push_back( "SRTM-3" );
    elev_src.push_back( "SRTM-30" );
    elev_src.push_back( "SRTMGL1" );
    elev_src.push_back( "SRTMGL3" );
}

// Display help and usage
static void help( int argc, char **argv, const string_list& elev_src ) {
    cout << "genapts generates airports for use in generating scenery for the FlightGear flight simulator.  \n";
    cout << "Airport, runway, and taxiway vector data and attributes are input, and generated 3D airports \n";
    cout << "are output for further processing by the TerraGear scenery creation tools.  \n";
    cout << "\n\n";
    cout << "The standard input file is apt.dat.gz which is found in $FG_ROOT/Airports.  \n";
    cout << "This file is periodically generated by Robin Peel, who maintains  \n";
    cout << "the airport database for both the X-Plane and FlightGear simulators.  \n";
    cout << "The format of this file is documented at  \n";
    cout << "http://data.x-plane.com/designers.html#Formats   \n";
    cout << "Any other input file corresponding to this format may be used as input to genapts.  \n";
    cout << "Input files may be gzipped or left as plain text as required.  \n";
    cout << "\n\n";
    cout << "Processing all the world's airports takes a *long* time.  To cut down processing time \n";
    cout << "when only some airports are required, you may refine the input selection either by airport \n";
    cout << "or by area.  By airport, either one airport can be specified using --airport=abcd, where abcd is \n";
    cout << "a valid airport code eg. --airport-id=KORD, or a starting airport can be specified using --start-id=abcd \n";
    cout << "where once again abcd is a valid airport code.  In this case, all airports in the file subsequent to the \n";
    cout << "start-id are done.  This is convenient when re-starting after a previous error.  \n";
    cout << "\nAn input area may be specified by lat and lon extent using min and max lat and lon.  \n";
    cout << "\nAn input file containing only a subset of the world's \n";
    cout << "airports may of course be used.\n";
    cout << "\n\n";
    cout << "It is necessary to generate the elevation data for the area of interest PRIOR TO GENERATING THE AIRPORTS.  \n";
    cout << "Failure to do this will result in airports being generated with an elevation of zero.  \n";
    cout << "The following subdirectories of the work-dir will be searched for elevation files:\n\n";
    
    string_list::const_iterator elev_src_it;
    for (elev_src_it = elev_src.begin(); elev_src_it != elev_src.end(); ++elev_src_it) {
    	    cout << *elev_src_it << "\n";
    }
    cout << "\n";
    usage( argc, argv );
}

void
setLoggingPriority (const string & priority )
{
  SG_LOG(SG_ALL,SG_ALERT,"setting log level to " << priority );
  if (priority == "bulk") {
    sglog().set_log_priority(SG_BULK);
  } else if (priority == "debug") {
    sglog().set_log_priority(SG_DEBUG);
  } else if (priority.empty() || priority == "info") { // default
    sglog().set_log_priority(SG_INFO);
  } else if (priority == "warn") {
    sglog().set_log_priority(SG_WARN);
  } else if (priority == "alert") {
    sglog().set_log_priority(SG_ALERT);
  } else {
    SG_LOG(SG_GENERAL, SG_WARN, "Unknown logging priority " << priority);
  }
}


// TODO: where do these belong
int nudge = 10;
double gSnap = 0.00000001;      // approx 1 mm
double slope_max = 0.02;
double slope_eps = 0.00001;

int main(int argc, char **argv)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    SGGeod min = SGGeod::fromDeg( -180, -90 );
    SGGeod max = SGGeod::fromDeg( 180, 90 );

    // Setup elevation directories
    string_list elev_src;
    elev_src.clear();
    setup_default_elevation_sources(elev_src);

    std::string debug_dir = ".";
    vector<std::string> debug_runway_defs;
    vector<std::string> debug_pavement_defs;
    vector<std::string> debug_taxiway_defs;
    vector<std::string> debug_feature_defs;

    // Set Normal logging
    sglog().setLogLevels( SG_GENERAL, SG_INFO );

    // parse arguments
    std::string work_dir = "";
    std::string input_file = "";
    std::string summary_file = "./genapt850.csv";
    std::string start_id = "";
    std::string airport_id = "";
    int         num_threads    =  1;

    int arg_pos;
    for (arg_pos = 1; arg_pos < argc; arg_pos++)
    {
        string arg = argv[arg_pos];
        if (arg.compare(0, 12, "--log-level=") == 0)
        {
            setLoggingPriority(arg.substr(12));
        }
        else if (arg.compare(0, 7, "--work=") == 0)
        {
            work_dir = arg.substr(7);
        }
        else if (arg.compare(0, 8, "--input=") == 0)
        {
            input_file = arg.substr(8);
        }
        else if (arg.compare(0, 11, "--start-id=") == 0)
        {
            start_id = arg.substr(11);
        }
        else if (arg.compare(0, 8, "--nudge=") == 0)
        {
            nudge = atoi( arg.substr(8).c_str() );
        }
        else if (arg.compare(0, 7, "--snap=") == 0)
        {
            gSnap = atof( arg.substr(7).c_str() );
        }
        else if (arg.compare(0, 10, "--min-lon=") == 0)
        {
            min.setLongitudeDeg(atof( arg.substr(10).c_str() ));
        }
        else if (arg.compare(0, 10, "--max-lon=") == 0)
        {
            max.setLongitudeDeg(atof( arg.substr(10).c_str() ));
        }
        else if (arg.compare(0, 10, "--min-lat=") == 0)
        {
            min.setLatitudeDeg(atof( arg.substr(10).c_str() ));
        }
        else if (arg.compare(0, 10, "--max-lat=") == 0)
        {
            max.setLatitudeDeg(atof( arg.substr(10).c_str() ));
        }
        else if (arg.compare(0, 10, "--airport=") == 0)
        {
            airport_id = simgear::strutils::uppercase( arg.substr(10).c_str() );
    	} 
        else if (arg.compare(0, 16, "--clear-dem-path") == 0)
        {
    	    elev_src.clear();
    	} 
        else if (arg.compare(0, 11, "--dem-path=") == 0)
        {
    	    elev_src.push_back( arg.substr(11) );
    	} 
        else if (arg.compare(0, 9, "--verbose") == 0 || arg.compare(0, 2, "-v") == 0) 
        {
    	    sglog().setLogLevels( SG_GENERAL, SG_BULK );
    	}
        else if (arg.compare(0, 12, "--max-slope=") == 0)
        {
    	    slope_max = atof( arg.substr(12).c_str() );
        }
        else if (arg.compare(0, 10, "--threads=") == 0)
        {
            num_threads = atoi( arg.substr(10).c_str() );
        }
        else if (arg.compare(0, 9, "--threads") == 0)
        {
            num_threads = std::thread::hardware_concurrency();
        }
        else if (arg.compare(0, 12, "--debug-dir=") == 0)
        {
            debug_dir = arg.substr(12);
        }
        else if (arg.compare(0, 16, "--debug-runways=") == 0)
        {
            debug_runway_defs.push_back( arg.substr(16) );
        }
        else if (arg.compare(0, 18, "--debug-pavements=") == 0)
        {
            debug_pavement_defs.push_back( arg.substr(18) );
        }
        else if (arg.compare(0, 17, "--debug-taxiways=") == 0)
        {
            TG_LOG(SG_GENERAL, SG_INFO, "add debug taxiway " << arg.substr(17) );
            debug_taxiway_defs.push_back( arg.substr(17) );
        }
        else if (arg.compare(0, 17, "--debug-features=") == 0)
        {
            debug_feature_defs.push_back( arg.substr(17) );
        }
        else if (arg.compare(0, 6, "--help") == 0 || arg.compare(0, 2, "-h") == 0)
        {
    	    help( argc, argv, elev_src );
    	    exit(-1);
    	} 
        else 
        {
    	    usage( argc, argv );
    	    return EXIT_FAILURE;
    	}
    }

    std::string airportareadir=work_dir+"/AirportArea";

    // this is the main program -
    TG_LOG(SG_GENERAL, SG_INFO, "Genapts850 version " << getTGVersion() << " running with " << num_threads << " threads" );
    TG_LOG(SG_GENERAL, SG_INFO, "Launch command was " << argv[0] );
    TG_LOG(SG_GENERAL, SG_INFO, "Input file = " << input_file);
    TG_LOG(SG_GENERAL, SG_INFO, "Work directory = " << work_dir);
    TG_LOG(SG_GENERAL, SG_INFO, "Longitude = " << min.getLongitudeDeg() << ':' << max.getLongitudeDeg());
    TG_LOG(SG_GENERAL, SG_INFO, "Latitude = " << min.getLatitudeDeg() << ':' << max.getLatitudeDeg());
    TG_LOG(SG_GENERAL, SG_INFO, "Terrain sources = ");
    for ( unsigned int i = 0; i < elev_src.size(); ++i ) {
        TG_LOG(SG_GENERAL, SG_INFO, "  " << work_dir << "/" << elev_src[i] );
    }
    TG_LOG(SG_GENERAL, SG_INFO, "Nudge = " << nudge);

    if (!max.isValid() || !min.isValid())
    {
        TG_LOG(SG_GENERAL, SG_ALERT, "Bad longitude or latitude");
        exit(1);
    }

    // make work directory
    TG_LOG(SG_GENERAL, SG_DEBUG, "Creating AirportArea directory");

    SGPath sgp( airportareadir );
    sgp.append( "dummy" );
    sgp.create_dir( 0755 );

    tgRectangle boundingBox(min, max);
    boundingBox.sanify();

    if ( work_dir == "" ) 
    {
    	TG_LOG( SG_GENERAL, SG_ALERT, "Error: no work directory specified." );
    	usage( argc, argv );
	    return EXIT_FAILURE;
    }

    if ( input_file == "" ) 
    {
    	TG_LOG( SG_GENERAL, SG_ALERT,  "Error: no input file." );
    	return EXIT_FAILURE;
    }

    sg_gzifstream in( input_file );
    if ( !in.is_open() ) 
    {
        TG_LOG( SG_GENERAL, SG_ALERT, "Cannot open file: " << input_file );
        return EXIT_FAILURE;
    }

    // try to figure out which apt.dat version we are reading and bail if version is unsupported
    in >> skipeol;
    char line[20];
    in.getline(line, 20);
    in.close();
    int code = atoi(line);
    if (code == 810 || code > 1200) {
        TG_LOG(SG_GENERAL, SG_ALERT, "ERROR: This genapts version does not support apt.data version " << code << " files.");
        return EXIT_FAILURE;
    }

    // Create the scheduler
    auto scheduler = std::make_unique<Scheduler>(input_file, work_dir, elev_src);

    // Add any debug 
    scheduler->set_debug( debug_dir, debug_runway_defs, debug_pavement_defs, debug_taxiway_defs, debug_feature_defs );

    // just one airport 
    if ( airport_id != "" )
    {
        // just find and add the one airport
        scheduler->AddAirport( airport_id );

        TG_LOG(SG_GENERAL, SG_INFO, "Finished Adding airport - now parse");
        
        // and schedule parsers
        scheduler->Schedule( num_threads, summary_file );
    }

    else if ( start_id != "" )
    {
        TG_LOG(SG_GENERAL, SG_INFO, "move forward to " << start_id );

        // scroll forward in datafile
        long position = scheduler->FindAirport( start_id );

        // add remaining airports within boundary
        if ( scheduler->AddAirports( position, &boundingBox ) )
        {
            // parse all the airports that were found
            scheduler->Schedule( num_threads, summary_file );
        }
    }
    else
    {
        // find all airports within given boundary
        if ( scheduler->AddAirports( 0, &boundingBox ) )
        {
            // and parse them
            scheduler->Schedule( num_threads, summary_file );
        }
    }

    TG_LOG(SG_GENERAL, SG_INFO, "Genapts finished successfully");

    auto finish_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = finish_time - start_time;
    std::cout << std::endl << "Elapsed time: " << elapsed.count() << " seconds" << std::endl << std::endl;

    return EXIT_SUCCESS;
}