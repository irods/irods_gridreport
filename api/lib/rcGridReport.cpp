
#include "grid_report_403.hpp"

int
rcGridReport( rcComm_t *conn, bytesBuf_t** _bbuf ) {
    int status;
    status = procApiRequest( conn, GRID_REPORT_AN, NULL, NULL,
                             ( void ** ) _bbuf, NULL );

    return status;
}
