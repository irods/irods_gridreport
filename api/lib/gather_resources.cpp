#ifndef RODS_SERVER
#define RODS_SERVER
#endif
#include "gather_resources.hpp"
#include "irods_resource_manager.hpp"
#include "grid_server_properties.hpp"
#include "initServer.hpp"
#include "rcMisc.hpp"
#include "readServerConfig.hpp"



irods::error server_is_icat() {
    irods::grid_server_properties& props = irods::grid_server_properties::getInstance();
    props.capture_if_needed();

    std::string user, pass;
    irods::error has_dbuser = props.get_property< std::string >( DB_USERNAME_KW, user );
    irods::error has_dbpass = props.get_property< std::string >( DB_PASSWORD_KW, pass );

    if( has_dbuser.ok() && has_dbpass.ok() ) {
        return SUCCESS();
    } 
    else {
        return ERROR( -1, "" );
    }

} // server_is_icat



irods::error process_gather_resources_results( 
    int             _local_remote_flag, 
    genQueryOut_t*  _result,
    resc_results_t& _rescs ) {
    // =-=-=-=-=-=-=-
    // extract results from query
    if ( !_result ) {
        return ERROR( SYS_INVALID_INPUT_PARAM, "_result parameter is null" );
    }

    // =-=-=-=-=-=-=-
    // values to extract from query
    sqlResult_t *rescId       = 0, *rescName      = 0, *zoneName   = 0, *rescType   = 0, *rescClass = 0;
    sqlResult_t *rescLoc      = 0, *rescVaultPath = 0, *freeSpace  = 0, *rescInfo   = 0;
    sqlResult_t *rescComments = 0, *rescCreate    = 0, *rescModify = 0, *rescStatus = 0;
    sqlResult_t *rescChildren = 0, *rescContext   = 0, *rescParent = 0, *rescObjCount = 0;

    // =-=-=-=-=-=-=-
    // extract results from query
    if ( ( rescId = getSqlResultByInx( _result, COL_R_RESC_ID ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_RESC_ID failed" );
    }

    if ( ( rescName = getSqlResultByInx( _result, COL_R_RESC_NAME ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_RESC_NAME failed" );
    }

    if ( ( zoneName = getSqlResultByInx( _result, COL_R_ZONE_NAME ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_ZONE_NAME failed" );
    }

    if ( ( rescType = getSqlResultByInx( _result, COL_R_TYPE_NAME ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_TYPE_NAME failed" );
    }

    if ( ( rescClass = getSqlResultByInx( _result, COL_R_CLASS_NAME ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_CLASS_NAME failed" );
    }

    if ( ( rescLoc = getSqlResultByInx( _result, COL_R_LOC ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_LOC failed" );
    }

    if ( ( rescVaultPath = getSqlResultByInx( _result, COL_R_VAULT_PATH ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_VAULT_PATH failed" );
    }

    if ( ( freeSpace = getSqlResultByInx( _result, COL_R_FREE_SPACE ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_FREE_SPACE failed" );
    }

    if ( ( rescInfo = getSqlResultByInx( _result, COL_R_RESC_INFO ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_RESC_INFO failed" );
    }

    if ( ( rescComments = getSqlResultByInx( _result, COL_R_RESC_COMMENT ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_RESC_COMMENT failed" );
    }

    if ( ( rescCreate = getSqlResultByInx( _result, COL_R_CREATE_TIME ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_CREATE_TIME failed" );
    }

    if ( ( rescModify = getSqlResultByInx( _result, COL_R_MODIFY_TIME ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_MODIFY_TIME failed" );
    }

    if ( ( rescStatus = getSqlResultByInx( _result, COL_R_RESC_STATUS ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_RESC_STATUS failed" );
    }

    if ( ( rescChildren = getSqlResultByInx( _result, COL_R_RESC_CHILDREN ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_RESC_CHILDREN failed" );
    }

    if ( ( rescContext = getSqlResultByInx( _result, COL_R_RESC_CONTEXT ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_RESC_CONTEXT failed" );
    }

    if ( ( rescParent = getSqlResultByInx( _result, COL_R_RESC_PARENT ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_RESC_PARENT failed" );
    }

    if ( ( rescObjCount = getSqlResultByInx( _result, COL_R_RESC_OBJCOUNT ) ) == NULL ) {
        return ERROR( UNMATCHED_KEY_OR_INDEX, "getSqlResultByInx for COL_R_RESC_OBJCOUNT failed" );
    }

    rodsEnv my_env;
    int status = getRodsEnv( &my_env );
    if( status < 0 ) {
        return ERROR( 
            status,
            "getRodEnv failed" );
    }

    // =-=-=-=-=-=-=-
    // iterate through the rows, initialize a resource for each entry
    for ( int i = 0; i < _result->rowCnt; ++i ) {
        // =-=-=-=-=-=-=-
        // extract row values
        std::string tmpRescId        = &rescId->value[ rescId->len * i ];
        std::string tmpRescLoc       = &rescLoc->value[ rescLoc->len * i ];
        std::string tmpRescName      = &rescName->value[ rescName->len * i ];
        std::string tmpZoneName      = &zoneName->value[ zoneName->len * i ];
        std::string tmpRescType      = &rescType->value[ rescType->len * i ];
        std::string tmpRescInfo      = &rescInfo->value[ rescInfo->len * i ];
        std::string tmpFreeSpace     = &freeSpace->value[ freeSpace->len * i ];
        std::string tmpRescClass     = &rescClass->value[ rescClass->len * i ];
        std::string tmpRescCreate    = &rescCreate->value[ rescCreate->len * i ];
        std::string tmpRescModify    = &rescModify->value[ rescModify->len * i ];
        std::string tmpRescStatus    = &rescStatus->value[ rescStatus->len * i ];
        std::string tmpRescComments  = &rescComments->value[ rescComments->len * i ];
        std::string tmpRescVaultPath = &rescVaultPath->value[ rescVaultPath->len * i ];
        std::string tmpRescChildren  = &rescChildren->value[ rescChildren->len * i ];
        std::string tmpRescContext   = &rescContext->value[ rescContext->len * i ];
        std::string tmpRescParent    = &rescParent->value[ rescParent->len * i ];
        std::string tmpRescObjCount  = &rescObjCount->value[ rescObjCount->len * i ];
        // =-=-=-=-=-=-=-
        // resolve the host name into a rods server host structure
        if ( tmpRescLoc != irods::EMPTY_RESC_HOST ) {
            if( _local_remote_flag >= 0 ) {
                if( _local_remote_flag == LOCAL_HOST ) {
                    if( tmpRescLoc != my_env.rodsHost ) {
                        continue;
                    }
                    
                } else if( _local_remote_flag == REMOTE_HOST ) {
                    if( tmpRescLoc == my_env.rodsHost ) {
                        continue;
                    }

                } else {
                    rodsLog( 
                        LOG_ERROR, 
                        "process_gather_resources_results :: invalid local_remote flag %d", 
                        _local_remote_flag );
                }
            }

        } 

        irods::lookup_table< std::string > resc;
        resc.set( irods::RESOURCE_ID,        tmpRescId );
        resc.set( irods::RESOURCE_FREESPACE, tmpFreeSpace );
        resc.set( irods::RESOURCE_QUOTA,     "-1" );
        resc.set( irods::RESOURCE_ZONE,      tmpZoneName );
        resc.set( irods::RESOURCE_NAME,      tmpRescName );
        resc.set( irods::RESOURCE_LOCATION,  tmpRescLoc );
        resc.set( irods::RESOURCE_TYPE,      tmpRescType );
        resc.set( irods::RESOURCE_CLASS,     tmpRescClass );
        resc.set( irods::RESOURCE_PATH,      tmpRescVaultPath );
        resc.set( irods::RESOURCE_INFO,      tmpRescInfo );
        resc.set( irods::RESOURCE_COMMENTS,  tmpRescComments );
        resc.set( irods::RESOURCE_CREATE_TS, tmpRescCreate );
        resc.set( irods::RESOURCE_MODIFY_TS, tmpRescModify );
        resc.set( irods::RESOURCE_CHILDREN,  tmpRescChildren );
        resc.set( irods::RESOURCE_PARENT,    tmpRescParent );
        resc.set( irods::RESOURCE_CONTEXT,   tmpRescContext );
        resc.set( irods::RESOURCE_OBJCOUNT,  tmpRescObjCount ); 

        if ( tmpRescStatus == std::string( RESC_DOWN ) ) {
            resc.set( irods::RESOURCE_STATUS, "1" );
        }
        else {
            resc.set( irods::RESOURCE_STATUS, "1" );
        }

        _rescs.push_back( resc );

    } // for i

    return SUCCESS();

} // process_gather_resources_results


irods::error gather_resources( 
    int             _local_remote_flag,
    rsComm_t*       _comm,
    resc_results_t& _rescs ) {
    // =-=-=-=-=-=-=-
    // clear existing resource map and initialize
    _rescs.clear();

    // =-=-=-=-=-=-=-
    // set up data structures for a gen query
    genQueryInp_t  genQueryInp;
    genQueryOut_t* genQueryOut = NULL;

    irods::error proc_ret;

    memset( &genQueryInp, 0, sizeof( genQueryInp ) );

    addInxIval( &genQueryInp.selectInp, COL_R_RESC_ID,       1 );
    addInxIval( &genQueryInp.selectInp, COL_R_RESC_NAME,     1 );
    addInxIval( &genQueryInp.selectInp, COL_R_ZONE_NAME,     1 );
    addInxIval( &genQueryInp.selectInp, COL_R_TYPE_NAME,     1 );
    addInxIval( &genQueryInp.selectInp, COL_R_CLASS_NAME,    1 );
    addInxIval( &genQueryInp.selectInp, COL_R_LOC,           1 );
    addInxIval( &genQueryInp.selectInp, COL_R_VAULT_PATH,    1 );
    addInxIval( &genQueryInp.selectInp, COL_R_FREE_SPACE,    1 );
    addInxIval( &genQueryInp.selectInp, COL_R_RESC_INFO,     1 );
    addInxIval( &genQueryInp.selectInp, COL_R_RESC_COMMENT,  1 );
    addInxIval( &genQueryInp.selectInp, COL_R_CREATE_TIME,   1 );
    addInxIval( &genQueryInp.selectInp, COL_R_MODIFY_TIME,   1 );
    addInxIval( &genQueryInp.selectInp, COL_R_RESC_STATUS,   1 );
    addInxIval( &genQueryInp.selectInp, COL_R_RESC_CHILDREN, 1 );
    addInxIval( &genQueryInp.selectInp, COL_R_RESC_CONTEXT,  1 );
    addInxIval( &genQueryInp.selectInp, COL_R_RESC_PARENT,   1 );
    addInxIval( &genQueryInp.selectInp, COL_R_RESC_OBJCOUNT, 1 );

    genQueryInp.maxRows = MAX_SQL_ROWS;

    // =-=-=-=-=-=-=-
    // init continueInx to pass for first loop
    int continueInx = 1;

    // =-=-=-=-=-=-=-
    // loop until continuation is not requested
    while ( continueInx > 0 ) {

        // =-=-=-=-=-=-=-
        // perform the general query
        int status = rsGenQuery( _comm, &genQueryInp, &genQueryOut );

        // =-=-=-=-=-=-=-
        // perform the general query
        if ( status < 0 ) {
            if ( status != CAT_NO_ROWS_FOUND ) {
                rodsLog( LOG_NOTICE, "initResc: rsGenQuery error, status = %d",
                         status );
            }

            clearGenQueryInp( &genQueryInp );
            return ERROR( status, "genqery failed." );

        } // if

        // =-=-=-=-=-=-=-
        // given a series of rows, each being a resource, create a resource 
        // and add it to the table
        proc_ret = process_gather_resources_results( 
                       _local_remote_flag,
                       genQueryOut, 
                       _rescs );

        // =-=-=-=-=-=-=-
        // if error is not valid, clear query and bail
        if ( !proc_ret.ok() ) {
            irods::log( PASS( proc_ret ) );
            freeGenQueryOut( &genQueryOut );
            break;
        }
        else {
            if ( genQueryOut != NULL ) {
                continueInx = genQueryInp.continueInx = genQueryOut->continueInx;
                freeGenQueryOut( &genQueryOut );
            }
            else {
                continueInx = 0;
            }

        } // else

    } // while

    clearGenQueryInp( &genQueryInp );

    return SUCCESS();

} // gather_local_resources

