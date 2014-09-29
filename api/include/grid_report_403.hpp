#ifndef GRID_REPORT_HPP
#define GRID_REPORT_HPP

// =-=-=-=-=-=-=-
// irods includes
#include "rods.hpp"
#include "rcMisc.hpp"
#include "procApiRequest.hpp"
#include "apiNumber.hpp"
#include "initServer.hpp"

#define GRID_REPORT_AN 10205

#ifdef RODS_SERVER
    #define RS_GRID_REPORT rsGridReport

#ifdef __cplusplus
extern "C"
#endif
int rsGridReport(
    rsComm_t*,      // server comm ptr
    bytesBuf_t** ); // json response
#else
    #define RS_GRID_REPORT NULL
#endif

// =-=-=-=-=-=-=-
// prototype for client
#ifdef __cplusplus
extern "C"
#endif
int rcGridReport(
        rcComm_t*,      // server comm ptr
        bytesBuf_t** ); // json response

#endif //  GRID_REPORT_HPP




