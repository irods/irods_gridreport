
#include "rodsErrorTable.hpp"
#include "initServer.hpp"
#include "miscServerFunct.hpp"

#include "irods_log.hpp"
#include "zone_report_403.hpp"
#include "server_report_403.hpp"
#include "gather_resources.hpp"
#include "irods_resource_manager.hpp"
#include "irods_resource_backport.hpp"
#include "irods_server_api_call.hpp"

#include <jansson.h>
#include <map>

#ifdef RODS_SERVER

#ifdef __cplusplus
extern "C" 
#endif
int _rsZoneReport( 
    rsComm_t*    _comm,
    bytesBuf_t** _bbuf );

#ifdef __cplusplus
extern "C" 
#endif
int rsZoneReport( 
    rsComm_t*    _comm,
    bytesBuf_t** _bbuf ) {
    rodsServerHost_t* rods_host;
    int status = getAndConnRcatHost( 
                     _comm, 
                     MASTER_RCAT, 
                     NULL, 
                     &rods_host );
    if( status < 0 ) {
        return status;
    }

    if( rods_host->localFlag == LOCAL_HOST ) {
        irods::error is_icat = server_is_icat();
        if( is_icat.ok() ) {
            status = _rsZoneReport( 
                         _comm,
                         _bbuf );
        } else {
            status = SYS_NO_RCAT_SERVER_ERR;
        }
    }
    else {
        status = rcZoneReport( rods_host->conn,
                                 _bbuf );
    }

    if ( status < 0 ) {
        rodsLog( 
            LOG_ERROR,
            "rsZoneReport: rcZoneReport failed, status = %d",
            status );
    }

    return status;

} // rsZoneReport



irods::error get_server_reports( 
    rsComm_t* _comm,
    json_t*&  _resc_arr ) {
    _resc_arr = json_array();
    if( !_resc_arr ) {
        return ERROR(
                   SYS_MALLOC_ERR,
                   "json_object() failed" );
    }

    std::map< rodsServerHost_t*, int > svr_reported;
    rodsServerHost_t* icat_host = 0;
    char* zone_name = getLocalZoneName(); 
    int status = getRcatHost( MASTER_RCAT, zone_name, &icat_host );
 
    resc_results_t rescs;
    irods::error ret = gather_resources(
                           REMOTE_HOST,
                           _comm,
                           rescs );
    for( size_t i = 0; 
         i < rescs.size();
         ++i ) {

        irods::lookup_table< std::string >& resc = rescs[ i ];

        std::string resc_name;
        irods::error ret = resc.get(
                               irods::RESOURCE_NAME, 
                               resc_name );
        if( !ret.ok() ) {
            irods::log( PASS( ret ) );
            continue;
        }
      
        irods::resource_ptr resc_ptr;
        ret = resc_mgr.resolve( resc_name, resc_ptr );
        if( !ret.ok() ) {
            return PASS( ret );
        }

        rodsServerHost_t* tmp_host = 0;
        ret = resc_ptr->get_property< rodsServerHost_t* >(
                  irods::RESOURCE_HOST,
                  tmp_host ); 
        if( !ret.ok() ) {
            return PASS( ret );
        }

        // skip the icat server as that is done separately
        if( tmp_host == icat_host ) {
            continue;

        }

        // skip previously reported servers
        std::map< rodsServerHost_t*, int >::iterator svr_itr = 
            svr_reported.find( tmp_host );
        if( svr_itr != svr_reported.end() ) {
        rodsLog( LOG_NOTICE, "XXXX - skipping [%s]", resc_name.c_str() );
            continue;

        }

        rodsLog( LOG_NOTICE, "XXXX - processing [%s]", resc_name.c_str() );
        svr_reported[ tmp_host ] = 1;
         
        int status = svrToSvrConnect( _comm, tmp_host );
        if( status < 0 ) {
            irods::log( ERROR( status,
                       "failed in svrToSvrConnect" ) );
            continue;
        }

        bytesBuf_t* bbuf = 0;

        status = procApiRequest( 
                     tmp_host->conn, 
                     SERVER_REPORT_AN, 
                     NULL, NULL,
                     (void**)&bbuf, NULL );
        if( status < 0 ) {
            rodsLog(
                LOG_ERROR,
                "rcServerReport failed for [%s], status = %d",
                "",
                status );
            continue;
        }

        // possible null termination issues
        std::string tmp_str;
        tmp_str.assign( (char*)bbuf->buf, bbuf->len );

        json_error_t j_err;
        json_t* j_resc = json_loads(
                             tmp_str.data(),
                             tmp_str.size(),
                             &j_err );
        if( !j_resc ) {
            std::string msg( "json_loads failed [" );
            msg += j_err.text;
            msg += "]";
            irods::log( ERROR(
                ACTION_FAILED_ERR,
                msg ) );
            continue;
        }

        json_array_append( _resc_arr, j_resc );

    } // for itr

    return SUCCESS();

} // get_server_reports


extern "C" int _rsZoneReport( 
    rsComm_t*    _comm,
    bytesBuf_t** _bbuf ) {

    bytesBuf_t* bbuf = 0;
    int status = irods::server_api_call(
                     SERVER_REPORT_AN, 
                     _comm,
                     &bbuf );
    if( status < 0 ) {
        rodsLog( 
            LOG_ERROR,
            "_rsZoneReport - rsServerReport failed %d",
            status );
        return status;
    }

    json_error_t j_err;
    json_t* cat_svr = json_loads( (char*)bbuf->buf, bbuf->len, &j_err );
    if( !cat_svr ) {
        rodsLog( 
            LOG_ERROR,
            "_rsZoneReport - json_loads failed [%s]",
            j_err.text );
        return ACTION_FAILED_ERR;
    }


    json_t* svr_arr = 0;
    irods::error ret = get_server_reports( _comm, svr_arr );
    if( !ret.ok() ) {
        rodsLog( 
            LOG_ERROR,
            "_rsZoneReport - get_server_reports failed, status = %d",
            ret.code() );
        return ret.code(); 
    }

    json_t* zone_obj = json_object();
    if( !zone_obj ) {
        rodsLog(
            LOG_ERROR,
            "failed to allocate json_object" );
        return SYS_MALLOC_ERR;
    }

    json_object_set( zone_obj, "icat_server", cat_svr );
    json_object_set( zone_obj, "resource_servers", svr_arr );

    json_t* zone_arr = json_array();
    if( !zone_arr ) {
        rodsLog(
            LOG_ERROR,
            "failed to allocate json_array" );
        return SYS_MALLOC_ERR;
    }

    json_array_append( zone_arr, zone_obj );

    json_t* zone = json_object();
    if( !zone ) {
        rodsLog(
            LOG_ERROR,
            "failed to allocate json_object" );
        return SYS_MALLOC_ERR;
    }

    json_object_set( 
        zone, 
        "schema_version", 
        json_string( 
            "http://schemas.irods.org/v1/zone_bundle.json" ) );
    json_object_set( zone, "zones", zone_arr );

    char* tmp_buf = json_dumps( zone, JSON_INDENT(4) );

    // *SHOULD* free All The Things...
    json_decref( zone );

    (*_bbuf) = ( bytesBuf_t* ) malloc( sizeof( bytesBuf_t ) );
    if( !(*_bbuf) ) {
        rodsLog( 
            LOG_ERROR,
            "_rsZoneReport: failed to allocate _bbuf" );
        return SYS_MALLOC_ERR;

    }

    (*_bbuf)->buf = tmp_buf;
    (*_bbuf)->len = strlen( tmp_buf );

    return 0;

} // _rsZoneReport

#endif // RODS_SERVER

// =-=-=-=-=-=-=-
// factory function to provide instance of the plugin
extern "C" irods::api_entry* plugin_factory(
    const std::string&,     //_inst_name
    const std::string& ) { // _context
    // =-=-=-=-=-=-=-
    // create a api def object
    irods::apidef_t def = { ZONE_REPORT_AN,
                            RODS_API_VERSION,
                            REMOTE_PRIV_USER_AUTH,
                            REMOTE_PRIV_USER_AUTH,
                            NULL, 0,
                            "BytesBuf_PI", 0,
                            0, // null fcn ptr, handled in delay_load
                            0  // null clear fcn
                          };
    // =-=-=-=-=-=-=-
    // create an api object
    irods::api_entry* api = new irods::api_entry( def );

#ifdef RODS_SERVER
    api->fcn_name_ = "rsZoneReport";
#endif // RODS_SERVER

    // =-=-=-=-=-=-=-
    // assign the pack struct key and value
    // N/A - api->in_pack_key   = "HelloInp_PI";
    // N/A - api->in_pack_value = HelloInp_PI;

    api->out_pack_key   = "BytesBuf_PI";
    api->out_pack_value = BytesBuf_PI;

    // N/A - api->extra_pack_struct[ "OtherOut_PI" ] = OtherOut_PI;

    return api;

} // plugin_factory



