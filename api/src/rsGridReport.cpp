
#include "rodsErrorTable.hpp"
#include "initServer.hpp"
#include "miscServerFunct.hpp"

#include "irods_log.hpp"
#include "grid_report_403.hpp"
#include "server_report.hpp"
#include "gather_resources.hpp"
#include "irods_resource_manager.hpp"
#include "irods_resource_backport.hpp"
#include "irods_server_api_call.hpp"

#include <jansson.h>

#ifdef RODS_SERVER

#ifdef __cplusplus
extern "C" 
#endif
int _rsGridReport( 
    rsComm_t*    _comm,
    bytesBuf_t** _bbuf );

#ifdef __cplusplus
extern "C" 
#endif
int rsGridReport( 
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
            status = _rsGridReport( 
                         _comm,
                         _bbuf );
        } else {
            status = SYS_NO_RCAT_SERVER_ERR;
        }
    }
    else {
        status = rcGridReport( rods_host->conn,
                                 _bbuf );
    }

    if ( status < 0 ) {
        rodsLog( 
            LOG_ERROR,
            "rsGridReport: rcGridReport failed, status = %d",
            status );
    }

    return status;

} // rsGridReport



irods::error get_server_reports( 
    rsComm_t* _comm,
    json_t*&  _resc_arr ) {
    _resc_arr = json_array();
    if( !_resc_arr ) {
        return ERROR(
                   SYS_MALLOC_ERR,
                   "json_object() failed" );
    }
 
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
       
        rodsServerHost_t* tmp_host;
        ret = resc_ptr->get_property< rodsServerHost_t* >(
                  irods::RESOURCE_HOST,
                  tmp_host ); 
        if( !ret.ok() ) {
            return PASS( ret );
        }
         
        int status = svrToSvrConnect( _comm, tmp_host );
        if( status < 0 ) {
            return ERROR(
                       status,
                       "failed in svrToSvrConnect" );
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
        }

        json_error_t j_err;
        json_t* j_resc = json_loads(
                             (char*)bbuf->buf,
                             bbuf->len,
                             &j_err );
        if( !j_resc ) {
            std::string msg( "json_loads failed [" );
            msg += j_err.text;
            msg += "]";
            return ERROR(
                ACTION_FAILED_ERR,
                msg );
        }

        json_array_append( _resc_arr, j_resc );

    } // for itr

    return SUCCESS();

} // get_server_reports


extern "C" int _rsGridReport( 
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
            "_rsGridReport - rsServerReport failed %d",
            status );
        return status;
    }

    json_error_t j_err;
    json_t* cat_svr = json_loads( (char*)bbuf->buf, bbuf->len, &j_err );
    if( !cat_svr ) {
        rodsLog( 
            LOG_ERROR,
            "_rsGridReport - json_loads failed [%s]",
            j_err.text );
        return ACTION_FAILED_ERR;
    }


    json_t* svr_arr = 0;
    irods::error ret = get_server_reports( _comm, svr_arr );
    if( !ret.ok() ) {
        rodsLog( 
            LOG_ERROR,
            "_rsGridReport - get_server_reports failed, status = %d",
            ret.code() );
        return ret.code(); 
    }

    json_t* grid_obj = json_object();
    if( !grid_obj ) {
        rodsLog(
            LOG_ERROR,
            "failed to allocat json_object" );
        return SYS_MALLOC_ERR;
    }

    json_object_set( grid_obj, "icat_server", cat_svr );
    json_object_set( grid_obj, "resource_servers", svr_arr );

    json_t* grid_arr = json_array();
    if( !grid_arr ) {
        rodsLog(
            LOG_ERROR,
            "failed to allocat json_array" );
        return SYS_MALLOC_ERR;
    }

    json_array_append( grid_arr, grid_obj );

    json_t* grid = json_object();
    if( !grid ) {
        rodsLog(
            LOG_ERROR,
            "failed to allocat json_object" );
        return SYS_MALLOC_ERR;
    }

    json_object_set( grid, "schema_version", json_string( "1" ) );
    json_object_set( grid, "zones", grid_arr );

    char* tmp_buf = json_dumps( grid, JSON_INDENT(4) );

    // *SHOULD* free All The Things...
    json_decref( grid );

    (*_bbuf) = ( bytesBuf_t* ) malloc( sizeof( bytesBuf_t ) );
    if( !(*_bbuf) ) {
        rodsLog( 
            LOG_ERROR,
            "_rsGridReport: failed to allocate _bbuf" );
        return SYS_MALLOC_ERR;

    }

    (*_bbuf)->buf = tmp_buf;
    (*_bbuf)->len = strlen( tmp_buf );

    return 0;

} // _rsGridReport

#endif // RODS_SERVER

// =-=-=-=-=-=-=-
// factory function to provide instance of the plugin
extern "C" irods::api_entry* plugin_factory(
    const std::string&,     //_inst_name
    const std::string& ) { // _context
    // =-=-=-=-=-=-=-
    // create a api def object
    irods::apidef_t def = { GRID_REPORT_AN,
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
    api->fcn_name_ = "rsGridReport";
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



