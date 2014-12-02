
#include "apiHandler.hpp"
#include "rodsErrorTable.hpp"
#include "initServer.hpp"

#include "irods_ms_home.hpp"
#include "irods_network_home.hpp"
#include "irods_auth_home.hpp"
#include "irods_resources_home.hpp"
#include "irods_api_home.hpp"
#include "irods_database_home.hpp"
#include "irods_lookup_table.hpp"
#include "irods_home_directory.hpp"
#include "irods_log.hpp"
#include "grid_server_properties.hpp"
#include "irods_plugin_name_generator.hpp"
#include "irods_resource_manager.hpp"
#include "irods_get_full_path_for_config_file.hpp"
#include "readServerConfig.hpp"

#include "server_report_403.hpp"
#include "gather_resources.hpp"

#include <jansson.h>

#include <fstream>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

#include <sys/utsname.h>


extern "C" {
#ifdef RODS_SERVER
    const std::string HOST_ACCESS_CONTROL_FILE( "HostAccessControl" );


    int _rsServerReport( 
        rsComm_t*    _comm,
        bytesBuf_t** _bbuf );

    int rsServerReport( 
        rsComm_t*    _comm,
        bytesBuf_t** _bbuf ) {

        // always execute this locally 
        int status = _rsServerReport( 
                        _comm,
                        _bbuf );
        if ( status < 0 ) {
            rodsLog( 
                LOG_ERROR,
                "rsServerReport: rcServerReport failed, status = %d",
                status );
        }

        return status;

    } // rsServerReport



    irods::error make_file_set( 
        const std::string& _files,
        json_t*&           _object ) {

        if( _files.empty() ) {
            return SUCCESS();
        }

        if( _object ) {
            return ERROR(
                       SYS_INVALID_INPUT_PARAM,
                       "json object is not null" );
        }

        std::vector<std::string> file_set;
        boost::split( file_set, _files, boost::is_any_of( "," ) );


        _object = json_array();
        if( !_object ) {
            return ERROR(
                       SYS_MALLOC_ERR,
                       "allocation of json object failed" );
        }

        for( size_t i = 0;
             i < file_set.size();
             ++i ) {
            json_t* obj = json_object();
            if( !obj ) {
                return ERROR( 
                           SYS_MALLOC_ERR,
                           "failed to allocate object" );
            }

            json_object_set( obj, "filename", json_string( file_set[ i ].c_str() ) ); 
            json_array_append( _object, obj );

        } // for i


        return SUCCESS();

    } // make_file_set



    irods::error make_federation_set( 
        const std::vector< std::string >& _feds,
        json_t*&                          _object ) {
        if( _feds.empty() ) {
            return SUCCESS();
        }

        if( _object ) {
            return ERROR(
                       SYS_INVALID_INPUT_PARAM,
                       "json object is not null" );
        }

        _object = json_array();
        if( !_object ) {
            return ERROR(
                       SYS_MALLOC_ERR,
                       "allocation of json object failed" );
        }

        for( size_t i = 0;
             i < _feds.size();
             ++i ) {

            std::vector<std::string> zone_sid_vals;
            boost::split( zone_sid_vals, _feds[ i ], boost::is_any_of( "-" ) );

            if( zone_sid_vals.size() > 2 ) {
                rodsLog( 
                    LOG_ERROR,
                    "multiple hyphens found in RemoteZoneSID [%s]",
                    _feds[ i ].c_str() );
                continue;
            }

            json_t* fed_obj = json_object();
            json_object_set( fed_obj, "zone_name",       json_string( zone_sid_vals[ 0 ].c_str() ) );
            json_object_set( fed_obj, "zone_id",         json_string( zone_sid_vals[ 1 ].c_str() ) );
            json_object_set( fed_obj, "negotiation_key", json_string( "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" ) );
            
            json_array_append( _object, fed_obj );

        } // for i

        return SUCCESS();

    } // make_federation_set



    irods::error convert_server_config( 
        json_t*& _svr_cfg ) {

        irods::grid_server_properties& props = irods::grid_server_properties::getInstance();
        props.capture_if_needed();

        _svr_cfg = json_object();
        if( !_svr_cfg ) {
            return ERROR(
                       SYS_MALLOC_ERR,
                       "json_object() failed" );
        }
            
        std::string s_val;
        irods::error ret = props.get_property< std::string >( "icatHost", s_val );
        if( ret.ok() ) {
            json_object_set( _svr_cfg, "icat_host", json_string( s_val.c_str() ) );
        } 

        ret = props.get_property< std::string >( "KerberosName", s_val );
        if( ret.ok() ) {
            json_object_set( _svr_cfg, "kerberos_name", json_string( s_val.c_str() ) );
        }

        bool b_val = false;
        ret = props.get_property< bool >( "run_server_as_root", b_val );
        if( ret.ok() ) {
            if( b_val ) {
                json_object_set( _svr_cfg, "run_server_as_root", json_string( "true" ) );
            } else {
                json_object_set( _svr_cfg, "run_server_as_root", json_string( "false" ) );
            }
        }

        ret = props.get_property< bool >( "pam_no_extend", b_val );
        if( ret.ok() ) {
            if( b_val ) {
                json_object_set( _svr_cfg, "pam_no_extend", json_string( "true" ) );
            } else {
                json_object_set( _svr_cfg, "pam_no_extend", json_string( "false" ) );
            }
        }

        ret = props.get_property< std::string >( "pam_password_min_time", s_val );
        if( ret.ok() ) {
            int min_time = boost::lexical_cast< int >( s_val );
            json_object_set( _svr_cfg, "pam_password_min_time", json_integer( min_time ) );
        }

        ret = props.get_property< std::string >( "pam_password_max_time", s_val );
        if( ret.ok() ) {
            int max_time = boost::lexical_cast< int >( s_val );
            json_object_set( _svr_cfg, "pam_password_max_time", json_integer( max_time ) );
        }

        size_t st_val = 0;
        ret = props.get_property< size_t>( "pam_password_length", st_val );
        if( ret.ok() ) {
            json_object_set( _svr_cfg, "pam_password_length", json_integer( st_val ) );
        }


        int i_val = 0;
        ret = props.get_property< int >( "default_dir_mode", i_val );
        if( ret.ok() ) {
            std::string mode = boost::lexical_cast< std::string >( i_val );
            json_object_set( _svr_cfg, "default_dir_mode", json_string( mode.c_str() ) );
        }

        ret = props.get_property< int >( "default_file_mode", i_val );
        if( ret.ok() ) {
            std::string mode = boost::lexical_cast< std::string >( i_val );
            json_object_set( _svr_cfg, "default_file_mode", json_string( mode.c_str() ) );
        }

        ret = props.get_property< std::string >( "default_hash_scheme", s_val );
        if( ret.ok() ) {
            json_object_set( _svr_cfg, "default_hash_scheme", json_string( s_val.c_str() ) );
        }

        ret = props.get_property< std::string >( "LocalZoneSID", s_val );
        if( ret.ok() ) {
            json_object_set( _svr_cfg, "zone_id", json_string( s_val.c_str() ) );
        }

        ret = props.get_property< std::string >( "agent_key", s_val );
        if( ret.ok() ) {
            json_object_set( _svr_cfg, "negotiation_key", json_string( s_val.c_str() ) );
        }

        ret = props.get_property< std::string >( "match_hash_policy", s_val );
        if( ret.ok() ) {
            json_object_set( _svr_cfg, "match_hash_policy", json_string( s_val.c_str() ) );
        } else {
            json_object_set( _svr_cfg, "match_hash_policy", json_string( "" ) );
        }

        ret = props.get_property< std::string >( "reRuleSet", s_val );
        if( ret.ok() ) {
            json_t* arr = 0;
            ret = make_file_set( s_val, arr );
            if( ret.ok() ) {
                json_object_set( _svr_cfg, "re_rulebase_set", arr );
            }
        }

        ret = props.get_property< std::string >( "reFuncMapSet", s_val );
        if( ret.ok() ) {
            json_t* arr = 0;
            ret = make_file_set( s_val, arr );
            if( ret.ok() ) {
                json_object_set( _svr_cfg, "re_function_name_mapping_set", arr );
            }
        }

        ret = props.get_property< std::string >( "reVariableMapSet", s_val );
        if( ret.ok() ) {
            json_t* arr = 0;
            ret = make_file_set( s_val, arr );
            if( ret.ok() ) {
                json_object_set( _svr_cfg, "re_data_variable_mapping_set", arr );
            }
        }

        std::vector< std::string > rem_sids;
        ret = props.get_property< std::vector< std::string > >( "RemoteZoneSID", rem_sids );
        if( ret.ok() ) {
            json_t* arr = 0;
            ret = make_federation_set( rem_sids, arr );
            if( !ret.ok() ) {
                irods::log( PASS( ret ) );
            }

            json_object_set( _svr_cfg, "federation", arr );

        } else {
            // may not be federeated, but it is required by the spec 
            json_t* arr = json_array();
            json_object_set( _svr_cfg, "federation", arr );
        }

        json_t* env_obj = json_object();
        json_object_set( _svr_cfg, "environment_variables", env_obj );

        // handle move from irods env to server config
        rodsEnv env;
        int status = getRodsEnv( &env );
        if( status < 0 ) {
            return ERROR(
                       status,
                       "failed in getRodsEnv" );
        }

        json_object_set( _svr_cfg, "zone_name",        json_string( env.rodsZone ) );
        json_object_set( _svr_cfg, "zone_user",        json_string( env.rodsUserName ) );
        json_object_set( _svr_cfg, "zone_auth_scheme", json_string( env.rodsAuthScheme ) );
        json_object_set( _svr_cfg, "zone_port",        json_integer( env.rodsPort ) );


        return SUCCESS();

    } // convert_server_config



    irods::error convert_host_access_control( 
        json_t*& _host_ctrl ) {

        _host_ctrl = json_object();
        if( !_host_ctrl ) {
            return ERROR(
                       SYS_MALLOC_ERR,
                       "json_object() failed" );
        }

        std::string cfg_file;
        irods::error ret = irods::get_full_path_for_config_file( HOST_ACCESS_CONTROL_FILE, cfg_file );
        if ( !ret.ok() ) {
            return PASS( ret );
        }

        json_t* cfg_arr = json_array();
        if( !cfg_arr ) {
            return ERROR( 
                       SYS_MALLOC_ERR,
                       "failed to allocate array" );
        }

        std::ifstream file( cfg_file.c_str(), std::ios::in );
        if( file.is_open() ) {
           
            std::string line; 
            while( getline( file, line ) ) {

                size_t pos = line.find_first_not_of( "\t " );
                if( line[ pos ] == '#' ) {
                    continue;
                }
       
                boost::trim( line );

                std::vector< std::string > vals;
                boost::split( vals, line, boost::is_any_of( "\t " ),boost::token_compress_on );
                if( vals.size() != 4 ) {
                    rodsLog( 
                        LOG_ERROR,
                        "convert_host_access_control - invalid line [%s]",
                        line.c_str() );
                    continue;
                }

                json_t* obj = json_object();
                if( !obj ) {
                    return ERROR( 
                               SYS_MALLOC_ERR,
                               "failed to allocate object" );
                }

                json_object_set( obj, "user",    json_string( vals[0].c_str() ) );
                json_object_set( obj, "group",   json_string( vals[1].c_str() ) );
                json_object_set( obj, "address", json_string( vals[2].c_str() ) );
                json_object_set( obj, "mask",    json_string( vals[3].c_str() ) );
      
                json_array_append( cfg_arr, obj );
                 
           } // while 

           file.close();

        } else {
            std::string msg( "failed to open file [" );
            msg += cfg_file;
            msg += "]";
            return ERROR(
                       SYS_INVALID_INPUT_PARAM,
                       msg.c_str() );
        }

        json_object_set( _host_ctrl, "access_entries", cfg_arr );

        return SUCCESS();

    } // convert_host_access_control



    irods::error convert_irods_host( 
        json_t*& _irods_host ) {

        _irods_host = json_object();
        if( !_irods_host ) {
            return ERROR(
                       SYS_MALLOC_ERR,
                       "json_object() failed" );
        }

        std::string cfg_file;
        irods::error ret = irods::get_full_path_for_config_file( HOST_CONFIG_FILE, cfg_file );
        if ( !ret.ok() ) {
            return PASS( ret );
        }

        json_t* host_arr = json_array();
        if( !host_arr ) {
            return ERROR( 
                       SYS_MALLOC_ERR,
                       "failed to allocate array" );
        }

        std::ifstream file( cfg_file.c_str(), std::ios::in );
        if( file.is_open() ) {
           
            std::string line; 
            while( getline( file, line ) ) {

                size_t pos = line.find_first_not_of( "\t " );
                if( line[ pos ] == '#' ) {
                    continue;
                }

                boost::trim( line );

                std::vector< std::string > vals;
                boost::split( vals, line, boost::is_any_of( "\t " ),boost::token_compress_on );
                
                json_t* host_obj = json_object();
                if( !host_obj ) {
                    return ERROR( 
                               SYS_MALLOC_ERR,
                               "failed to allocate object" );
                }

                json_t* addr_arr = json_array();
                if( !addr_arr ) {
                    return ERROR( 
                               SYS_MALLOC_ERR,
                               "failed to allocate object" );
                }

                if( "localhost" == vals[0] ) {
                    json_object_set( host_obj, "address_type", json_string( "local" ) );
                } else {
                    json_object_set( host_obj, "address_type", json_string( "remote" ) );
                }

                for( size_t i = 1;
                     i < vals.size();
                     ++i ) {
                    json_t* addr_obj = json_object();
                    if( !addr_obj ) {
                        return ERROR( 
                                   SYS_MALLOC_ERR,
                                   "failed to allocate object" );
                    }
                    
                    json_object_set( addr_obj, "address", json_string( vals[i].c_str() ) );
                    json_array_append( addr_arr, addr_obj );
               
                } // for i
                
                json_object_set( host_obj, "addresses", addr_arr );
                json_array_append( host_arr, host_obj );
                 
           } // while 

           file.close();

        } else {
            std::string msg( "failed to open file [" );
            msg += cfg_file;
            msg += "]";
            return ERROR(
                       SYS_INVALID_INPUT_PARAM,
                       msg.c_str() );
        }

        json_object_set( _irods_host, "host_entries", host_arr );

        return SUCCESS();

    } // convert_irods_host

    irods::error convert_service_account( 
        json_t*& _svc_acct ) {

        _svc_acct = json_object();
        if( !_svc_acct ) {
            return ERROR(
                       SYS_MALLOC_ERR,
                       "json_object() failed" );
        }
        
        rodsEnv my_env;
        int status = getRodsEnv( &my_env ); 
        if( status < 0 ) {
            return ERROR(
                       status,
                       "failed in getRodsEnv" );    
        }

        json_object_set( _svc_acct, "irods_host", json_string( my_env.rodsHost ) );
        
        json_object_set( _svc_acct, "irods_port", json_integer( my_env.rodsPort ) );

        json_object_set( _svc_acct, "irods_default_resource", json_string( my_env.rodsDefResource ) );

        if( my_env.rodsServerDn ) {
            json_object_set( _svc_acct, "irods_server_dn", json_string( my_env.rodsServerDn ) );
        } else {
            json_object_set( _svc_acct, "irods_server_dn", json_string( "" ) );
        }

        json_object_set( _svc_acct, "irods_log_level", json_integer( my_env.rodsPort  ) );

        json_object_set( _svc_acct, "irods_authentication_file_name", json_string( my_env.rodsAuthFileName ) );

        json_object_set( _svc_acct, "irods_debug", json_string( my_env.rodsDebug) );

        json_object_set( _svc_acct, "irods_home", json_string( my_env.rodsHome ) );

        json_object_set( _svc_acct, "irods_cwd", json_string( my_env.rodsCwd ) );

        json_object_set( _svc_acct, "irods_authentication_scheme", json_string( my_env.rodsAuthScheme ) );

        json_object_set( _svc_acct, "irods_user_name", json_string( my_env.rodsUserName ) );

        json_object_set( _svc_acct, "irods_zone", json_string( my_env.rodsZone ) );

        json_object_set( _svc_acct, "irods_client_server_negotiation", json_string( my_env.rodsClientServerNegotiation ) );

        json_object_set( _svc_acct, "irods_client_server_policy", json_string( my_env.rodsClientServerPolicy ) );

        json_object_set( _svc_acct, "irods_encryption_key_size", json_integer( my_env.rodsEncryptionKeySize ) );

        json_object_set( _svc_acct, "irods_encryption_salt_size", json_integer( my_env.rodsEncryptionSaltSize ) );

        json_object_set( _svc_acct, "irods_encryption_num_hash_rounds", json_integer( my_env.rodsEncryptionNumHashRounds ) );
        
        json_object_set( _svc_acct, "irods_encryption_algorithm", json_string( my_env.rodsEncryptionAlgorithm ) );
        json_object_set( _svc_acct, "irods_default_hash_scheme", json_string( my_env.rodsDefaultHashScheme ) );
        json_object_set( _svc_acct, "irods_match_hash_policy", json_string( my_env.rodsMatchHashPolicy ) );

        return SUCCESS();

    } // convert_service_account

    irods::error add_plugin_type_to_json_array(
        const std::string& dir_name,
        const char* plugin_type,
        json_t*& json_array) {

        irods::plugin_name_generator name_gen;
        irods::plugin_name_generator::plugin_list_t plugin_list;
        irods::error ret = name_gen.list_plugins( dir_name, plugin_list );
        if ( !ret.ok() ) {
            return PASS( ret );
        }

        for ( irods::plugin_name_generator::plugin_list_t::iterator itr = plugin_list.begin();
              itr != plugin_list.end();
              ++itr ) {

            json_t* plug = json_object();
            json_object_set( plug, "name",     json_string( itr->c_str() ) );
            json_object_set( plug, "type",     json_string( plugin_type ) );
            json_object_set( plug, "version",  json_string( "" ) );
            json_object_set( plug, "checksum_sha256", json_string( "" ) );

            json_array_append( json_array, plug );
        }

        return SUCCESS();
    } // add_plugin_type_to_json_array

    irods::error get_plugin_array(
        json_t*& _plugins ) {

        _plugins = json_array();
        if ( !_plugins ) {
            return ERROR(
                         SYS_MALLOC_ERR,
                         "json_object() failed" );
        }

        irods::error ret = add_plugin_type_to_json_array( irods::RESOURCES_HOME, "resource", _plugins );
        if ( !ret.ok() ) {
            return PASS( ret );
        }

        ret = add_plugin_type_to_json_array( irods::IRODS_DATABASE_HOME, "database", _plugins );
        if ( !ret.ok() ) {
            return PASS( ret );
        }

        ret = add_plugin_type_to_json_array( irods::AUTH_HOME, "auth", _plugins );
        if ( !ret.ok() ) {
            return PASS( ret );
        }

        ret = add_plugin_type_to_json_array( irods::NETWORK_HOME, "network", _plugins );
        if ( !ret.ok() ) {
            return PASS( ret );
        }

        ret = add_plugin_type_to_json_array( irods::API_HOME, "api", _plugins );
        if ( !ret.ok() ) {
            return PASS( ret );
        }

        ret = add_plugin_type_to_json_array( irods::MS_HOME, "microservice", _plugins );
        if ( !ret.ok() ) {
            return PASS( ret );
        }

        return SUCCESS();

    } // get_plugin_array

    irods::error get_uname_string( 
        std::string& _str ) {

        struct utsname os_name;
        memset( &os_name, 0, sizeof( os_name ) );
        const int status = uname( &os_name );
        if( status != 0 ) {
            return ERROR( 
                         status,
                         "uname failed" );
        }

        _str.clear();
        _str += "SYS_NAME=" ;
        _str += os_name.sysname;
        _str += ";NODE_NAME="; 
        _str += os_name.nodename;
        _str += ";RELEASE="; 
        _str += os_name.release;
        _str += ";VERSION="; 
        _str += os_name.version;
        _str += ";MACHINE="; 
        _str += os_name.machine;

        return SUCCESS();

    } // get_uname_string

    irods::error get_script_output_single_line(const std::string& script_language, const std::string& script_name, const std::vector<std::string>& args, std::string& output) {
        output.clear();
        std::stringstream exec;
        exec << script_language << " " << irods::IRODS_HOME_DIRECTORY << "/iRODS/scripts/" << script_language << "/" << script_name;
        for (std::vector<std::string>::size_type i=0; i<args.size(); ++i) {
            exec << " " << args[i];
        }

        FILE *fp = popen(exec.str().c_str(), "r");
        if (fp == NULL) {
            return ERROR( SYS_FORK_ERROR, "popen() failed" );
        }

        std::vector<char> buf(1000);
        const char* fgets_ret = fgets(&buf[0], buf.size(), fp);
        if (fgets_ret == NULL) {
            const int pclose_ret = pclose(fp);
            std::stringstream msg;
            msg << "fgets() failed. feof[" << std::feof(fp) << "] ferror[" << std::ferror(fp) << "] pclose[" << pclose_ret << "]";
            return ERROR( FILE_READ_ERR,
                          msg.str() );
        }

        const int pclose_ret = pclose(fp);
        if (pclose_ret == -1) {
            return ERROR( SYS_FORK_ERROR,
                          "pclose() failed." );
        }

        output = &buf[0];
        // Remove trailing newline
        const std::string::size_type size = output.size();
        if (size > 0 && output[size-1] == '\n') {
            output.resize(size-1);
        }

        return SUCCESS();

    }  // get_script_output

    irods::error get_host_system_information(json_t*& _host_system_information ) {

        _host_system_information = json_object();
        if ( !_host_system_information ) {
            return ERROR(
                         SYS_MALLOC_ERR,
                         "json_object() failed" );
        }


        std::string uname_string;
        irods::error ret = get_uname_string( uname_string );
        if( !ret.ok() ) {
            irods::log( PASS( ret ) );
        }
        json_object_set( _host_system_information, "uname", json_string( uname_string.c_str() ) );

        std::vector<std::string> args;
        args.push_back("os_distribution_name");
        std::string os_distribution_name;
        ret = get_script_output_single_line("python", "system_identification.py", args, os_distribution_name);
        if( !ret.ok() ) {
            irods::log( PASS( ret ) );
        }
        json_object_set( _host_system_information, "os_distribution_name", json_string( os_distribution_name.c_str() ) );

        args.clear();
        args.push_back("os_distribution_version");
        std::string os_distribution_version;
        ret = get_script_output_single_line("python", "system_identification.py", args, os_distribution_version);
        if( !ret.ok() ) {
            irods::log( PASS( ret ) );
        }
        json_object_set( _host_system_information, "os_distribution_version", json_string( os_distribution_version.c_str() ) );

        return SUCCESS();

    } // get_host_system_information


    irods::error get_resource_array( 
        rsComm_t* _comm,
        json_t*&  _resources ) {

        _resources = json_array();
        if( !_resources ) {
            return ERROR(
                       SYS_MALLOC_ERR,
                       "json_array() failed" );
        }

        resc_results_t rescs;
        irods::error ret = gather_resources(
                               LOCAL_HOST,
                               _comm,
                               rescs );
        if( !ret.ok() ) {
            return PASS( ret );
        }

        for( size_t i = 0; 
             i < rescs.size();
             ++i ) {

            irods::lookup_table< std::string >& resc = rescs[ i ];
           
            std::string name;
            ret = resc.get( irods::RESOURCE_NAME, name );
            if( !ret.ok() ) {
                irods::log( PASS( ret ) );
                continue;
            }
     
            std::string host_name;
            ret = resc.get( irods::RESOURCE_LOCATION, host_name );
            if( !ret.ok() ) {
                irods::log( PASS( ret ) );
                continue;
            }

            std::string type;
            ret = resc.get( irods::RESOURCE_TYPE, type );
            if( !ret.ok() ) {
                irods::log( PASS( ret ) );
                continue;
            }
     
            std::string vault;
            ret = resc.get( irods::RESOURCE_PATH, vault );
            if( !ret.ok() ) {
                irods::log( PASS( ret ) );
                continue;
            }
     
            std::string context;
            ret = resc.get( irods::RESOURCE_CONTEXT, context );
            if( !ret.ok() ) {
                irods::log( PASS( ret ) );
                continue;
            }
     
            std::string parent;
            ret = resc.get( irods::RESOURCE_PARENT, parent );
            if( !ret.ok() ) {
                irods::log( PASS( ret ) );
                continue;
            }
     
            std::string children;
            ret = resc.get( irods::RESOURCE_CHILDREN, children );
            if( !ret.ok() ) {
                irods::log( PASS( ret ) );
                continue;
            }
     
            std::string object_count;
            ret = resc.get( irods::RESOURCE_OBJCOUNT, object_count );
            if( !ret.ok() ) {
                irods::log( PASS( ret ) );
                continue;
            }

            std::string freespace; 
            ret = resc.get( irods::RESOURCE_FREESPACE, freespace );
            if( !ret.ok() ) {
                irods::log( PASS( ret ) );
                continue;
            }
     
            std::string status; 
            ret = resc.get( irods::RESOURCE_STATUS, status );
            if( !ret.ok() ) {
                irods::log( PASS( ret ) );
                continue;
            }

            json_t* entry = json_object();
            if( !entry ) {
                return ERROR(
                           SYS_MALLOC_ERR,
                           "failed to alloc entry" );
            }

            json_object_set( entry, "name",            json_string( name.c_str() ) );
            json_object_set( entry, "type",            json_string( type.c_str() ) );
            json_object_set( entry, "host",            json_string( host_name.c_str() ) );
            json_object_set( entry, "vault_path",      json_string( vault.c_str() ) );
            json_object_set( entry, "context_string",  json_string( context.c_str() ) );
            json_object_set( entry, "parent_resource", json_string( parent.c_str() ) );
            
            int count = boost::lexical_cast< int >( object_count );
            json_object_set( entry, "object_count", json_integer( count ) );
            json_object_set( entry, "free_space",   json_string( freespace.c_str() ) );
            json_object_set( entry, "status",       json_string( status == "1" ? "up" : "down" ) );

            json_array_append( _resources, entry );

        } // for itr
       
        return SUCCESS(); 

    } // get_resource_array



    irods::error get_file_contents( 
        const std::string& _fn,
        std::string&       _cont ) {
        
        std::ifstream f( _fn.c_str() );
        std::stringstream ss;
        ss << f.rdbuf();
        f.close();

        std::string in_s = ss.str();
        
        namespace bitr = boost::archive::iterators;
        std::stringstream o_str;
        typedef 
            //bitr::insert_linebreaks<
                bitr::base64_from_binary< // convert binary values to base64 characters
                bitr::transform_width<    // retrieve 6 bit integers from a sequence of 8 bit bytes
                const char *,
                    6,
                    8
                      >
            //    >,72
                
            > 
            base64_text; // compose all the above operations in to a new iterator

        std::copy(
                base64_text(in_s.c_str()),
                base64_text(in_s.c_str() + in_s.size()),
                bitr::ostream_iterator<char>( o_str )
                );

        _cont = o_str.str();
        
        size_t pad = in_s.size() % 3;
        _cont.insert( _cont.size(), ( 3 - pad ) % 3, '=' );

        return SUCCESS();
         
    } // get_file_contents



    irods::error get_config_dir( 
        json_t*& _cfg_dir ) {
        namespace fs = boost::filesystem;

        _cfg_dir = json_object();
        if( !_cfg_dir ) {
            return ERROR(
                       SYS_MALLOC_ERR,
                       "json_object() failed" );
        }

        json_t* file_arr = json_array();
        if( !file_arr ) {
            return ERROR(
                       SYS_MALLOC_ERR,
                       "json_array() failed" );
        }

        std::string cfg_file;
        irods::error ret = irods::get_full_path_for_config_file( SERVER_CONFIG_FILE, cfg_file );
        if ( !ret.ok() ) {
            return PASS( ret );
        }

        fs::path p( cfg_file );
        std::string config_dir = p.parent_path().string();

        json_object_set( _cfg_dir, "path", json_string( config_dir.c_str() ) );
        
        for( fs::directory_iterator itr( config_dir );
             itr != fs::directory_iterator();
             ++itr ) {
            if( fs::is_regular_file( itr->path() ) ) {
                const fs::path& p = itr->path();
                const std::string& name = p.string();

                if( std::string::npos != name.find( SERVER_CONFIG_FILE ) 
                    ) {
                    continue;
                }

                json_t* f_obj = json_object();
                if( !f_obj ) {
                    return ERROR(
                               SYS_MALLOC_ERR,
                               "failed to allocate f_obj" );
                }

                json_object_set( f_obj, "name", json_string( name.c_str() ) );

                std::string contents;
                ret = get_file_contents( name, contents );
                if( !ret.ok() ) {
                    irods::log( PASS( ret ) );
                    continue;
                }

                json_object_set( f_obj, "contents", json_string( contents.c_str() ) );

                json_array_append( file_arr, f_obj );
            }

        } // for itr

        json_object_set( _cfg_dir, "files", file_arr );

        return SUCCESS();

    } // get_config_dir



    irods::error get_database_config( 
        json_t*& _db_cfg ) {

        irods::grid_server_properties& props = irods::grid_server_properties::getInstance();
        props.capture_if_needed();

        _db_cfg = json_object();
        if( !_db_cfg ) {
            return ERROR(
                       SYS_MALLOC_ERR,
                       "allocation of json_object failed" );
        }

        std::string s_val;
        irods::error ret = props.get_property< std::string >( 
                               "catalog_database_type", 
                               s_val );
        if( ret.ok() ) {
            json_object_set( 
                _db_cfg, 
                "catalog_database_type", 
                json_string( s_val.c_str() ) );
        } 

        ret = props.get_property< std::string >( "DBUsername", s_val );
        if( ret.ok() ) {
            json_object_set( _db_cfg, "db_username", json_string( s_val.c_str() ) );
        } 
            
        json_object_set( _db_cfg, "db_password", json_string( "XXXXX" ) );

        return SUCCESS();

    } // get_database_config


    irods::error get_version(
        rsComm_t* _comm,
        json_t*&  _version ) {
        namespace fs = boost::filesystem;
        if( !_comm ) {
            return ERROR( 
                       SYS_INVALID_INPUT_PARAM,
                       "comm is null" );
        }

        _version = json_object();
        if( !_version ) {
            return ERROR(
                       SYS_MALLOC_ERR,
                       "json_object() failed" );

        }

        std::string version_path = irods::IRODS_HOME_DIRECTORY + "VERSION";
        std::ifstream f( version_path.c_str(), std::ios::in );
        if( !f.is_open() ) {
            std::string msg( "failed to open [" );
            msg += version_path;
            msg += "]";
            return ERROR(
                       -1,
                       msg );
        }

        std::string line;
        while( getline( f, line ) ) {
            std::vector< std::string > tok;
            boost::split( 
                tok, 
                line, 
                boost::is_any_of( "=" ) );

            if( "IRODSVERSION" == tok[0] ) {
                json_object_set( 
                    _version, 
                    "irods_version", 
                    json_string( tok[1].c_str() ) );

            } else if( "CATALOG_SCHEMA_VERSION" == tok[0] ) {
                int csv = boost::lexical_cast< int >( tok[1] );
                json_object_set( 
                    _version, 
                    "catalog_schema_version", 
                    json_integer( csv ) );
                    
            }

        }

        f.close();

        json_object_set( 
            _version,
            "commit_id",
            json_string( "0000000000000000000000000000000000000000" ) );

        json_object_set(
            _version,
            "installation_time",
            json_string( "2014-01-01T12:00:00Z" ) );
                    
        json_object_set(
            _version,
            "build_system_information",
            json_string( "not provided" ) );

        json_object_set(
            _version,
            "compiler_version",
            json_string( "not provided" ) );
        
        json_object_set(
            _version,
            "compile_time",
            json_string( "2014-01-01T12:00:00Z" ) );

        return SUCCESS();

    } // get_version




    int _rsServerReport( 
        rsComm_t*    _comm,
        bytesBuf_t** _bbuf ) {

        if( !_comm || !_bbuf ) {
            rodsLog( 
                LOG_ERROR,
                "_rsServerReport: null comm or bbuf" );
            return SYS_INVALID_INPUT_PARAM;
        }

        (*_bbuf) = ( bytesBuf_t* ) malloc( sizeof( **_bbuf ) );
        if( !(*_bbuf) ) {
            rodsLog( 
                LOG_ERROR,
                "_rsServerReport: failed to allocate _bbuf" );
            return SYS_MALLOC_ERR;

        }

        json_t* resc_svr = json_object();
        if( !resc_svr ) {
            rodsLog( 
                LOG_ERROR,
                "_rsServerReport :: json_object() failed" );
            return SYS_MALLOC_ERR;
                       
        }

        json_t* host_system_information = 0;
        irods::error ret = get_host_system_information( host_system_information );
        if ( !ret.ok() ) {
            irods::log( PASS( ret ) );
        }
        json_object_set( resc_svr, "host_system_information", host_system_information );

        json_t* version = 0;
        ret = get_version(
                  _comm,
                  version );
        if( !ret.ok() ) {
            irods::log( PASS( ret ) );
        }
        json_object_set( resc_svr, "version", version );

        json_t* svr_cfg = 0;
        ret = convert_server_config( svr_cfg );
        if( !ret.ok() ) {
            irods::log( PASS( ret ) );
        }
        json_object_set( resc_svr, "server_config", svr_cfg );

        json_t* host_ctrl = 0;
        ret = convert_host_access_control( host_ctrl );
        if( !ret.ok() ) {
            irods::log( PASS( ret ) );
        }
        json_object_set( resc_svr, "host_access_control_config", host_ctrl );

        json_t* irods_host = 0;
        ret = convert_irods_host( irods_host );
        if( !ret.ok() ) {
            irods::log( PASS( ret ) );
        }
        json_object_set( resc_svr, "hosts_config", irods_host );

        json_t* svc_acct = 0;
        ret = convert_service_account( svc_acct );
        if( !ret.ok() ) {
            irods::log( PASS( ret ) );
        }
        json_object_set( resc_svr, "service_account_environment", svc_acct );

        json_t* plugins = 0;
        ret = get_plugin_array( plugins );
        if( !ret.ok() ) {
            irods::log( PASS( ret ) );
        }
        json_object_set( resc_svr, "plugins", plugins );

        json_t* resources = 0;
        ret = get_resource_array( _comm, resources );
        if( !ret.ok() ) {
            irods::log( PASS( ret ) );
        }
        json_object_set( resc_svr, "resources", resources );

        json_t* cfg_dir = 0;
        ret = get_config_dir( cfg_dir );
        if( !ret.ok() ) {
            irods::log( PASS( ret ) );
        }
        json_object_set( resc_svr, "configuration_directory", cfg_dir );

        irods::error is_icat = server_is_icat();
        if( is_icat.ok() ) {
            json_t* db_cfg = 0;
            ret = get_database_config( db_cfg );
            if( !ret.ok() ) {
                irods::log( PASS( ret ) );
            }
            json_object_set( resc_svr, "database_config", db_cfg );
        }

        char* tmp_buf = json_dumps( resc_svr, JSON_INDENT(4) );

        // *SHOULD* free All The Things...
        json_decref( resc_svr );

        (*_bbuf)->buf = tmp_buf;
        (*_bbuf)->len = strlen( tmp_buf );

        return 0;
        
    } // _rsServerReport

#endif // RODS_SERVER

    // =-=-=-=-=-=-=-
    // factory function to provide instance of the plugin
    irods::api_entry* plugin_factory(
        const std::string&,     //_inst_name
        const std::string& ) { // _context
        // =-=-=-=-=-=-=-
        // create a api def object
        irods::apidef_t def = { SERVER_REPORT_AN,
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
        api->fcn_name_ = "rsServerReport";
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




}; // extern "C"




