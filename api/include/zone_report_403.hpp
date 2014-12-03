#ifndef ZONE_REPORT_HPP
#define ZONE_REPORT_HPP

// =-=-=-=-=-=-=-
// irods includes
#include "rods.hpp"
#include "rcMisc.hpp"
#include "procApiRequest.hpp"
#include "apiNumber.hpp"
#include "initServer.hpp"

#define ZONE_REPORT_AN 10205

#ifdef RODS_SERVER
    #define RS_ZONE_REPORT rsZoneReport

#ifdef __cplusplus
extern "C"
#endif
int rsZoneReport(
    rsComm_t*,      // server comm ptr
    bytesBuf_t** ); // json response
#else
    #define RS_ZONE_REPORT NULL
#endif

// =-=-=-=-=-=-=-
// prototype for client
#ifdef __cplusplus
extern "C"
#endif
int rcZoneReport(
        rcComm_t*,      // server comm ptr
        bytesBuf_t** ); // json response

#endif //  ZONE_REPORT_HPP




