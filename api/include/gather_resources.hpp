#ifndef GATHER_RESOURCES_HPP
#define GATHER_RESOURCES_HPP

#include <vector>
#include <string>
#include "irods_log.hpp"
#include "irods_lookup_table.hpp"

#include "genQuery.hpp"

typedef std::vector< irods::lookup_table< std::string > > resc_results_t;

irods::error process_gather_resources_results(
    int,                // flag for local or remote resources
    genQueryOut_t*,     // genquery results 
    resc_results_t& );  // list of resource properties

irods::error gather_resources( 
    int,               // flag for local or remote resources
    rsComm_t*,         // server connection handle
    resc_results_t& ); // list of resource properties

irods::error server_is_icat();

#endif // GATHER_RESOURCES_HPP



