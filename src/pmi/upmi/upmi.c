#include "upmi.h"
#include <stdlib.h>

struct PMI_keyval_t;
int _size, _rank, _appnum;
pthread_mutex_t umpi_lock;

void UPMI_lock_init(void) {
    pthread_mutex_init(&umpi_lock, NULL);
}

void UPMI_lock_destroy(void) {
    pthread_mutex_destroy(&umpi_lock);
}

void UPMI_lock(void) {
    pthread_mutex_lock(&umpi_lock);
}

void UPMI_unlock(void) {
    pthread_mutex_unlock(&umpi_lock);
}

int UPMI_INIT( int *spawned ) {
    #ifdef USE_PMI2_API
    return PMI2_Init( spawned, &_size, &_rank, &_appnum );
    #else
    UPMI_lock_init();
    return PMI_Init( spawned );
    #endif
}

int UPMI_INITIALIZED( int *initialized ) { 
    #ifdef USE_PMI2_API
    *initialized = PMI2_Initialized();
    return UPMI_SUCCESS;
    #else
    return PMI_Initialized( initialized );
    #endif
}

int UPMI_FINALIZE( void ) { 
    int pmi_ret_val;
    #ifdef USE_PMI2_API
    pmi_ret_val = PMI2_Finalize();
    #else
    UPMI_lock();
    pmi_ret_val = PMI_Finalize();
    UPMI_unlock();
    UPMI_lock_destroy();
    #endif
    return pmi_ret_val;
}

int UPMI_GET_SIZE( int *size ) { 
    #ifdef USE_PMI2_API
    *size = _size;
    return UPMI_SUCCESS;
    #else
    return PMI_Get_size( size );
    #endif
}

int UPMI_GET_RANK( int *rank ) { 
    #ifdef USE_PMI2_API
    *rank = _rank;
    return UPMI_SUCCESS;
    #else
    return PMI_Get_rank( rank );
    #endif
}

int UPMI_GET_APPNUM( int *appnum ) { 
    int pmi_ret_val;
    #ifdef USE_PMI2_API
    *appnum = _appnum;
    pmi_ret_val = UPMI_SUCCESS;
    #else
    UPMI_lock();
    pmi_ret_val = PMI_Get_appnum( appnum );
    UPMI_unlock();
    #endif
    return pmi_ret_val;
}

int UPMI_GET_UNIVERSE_SIZE( int *size ) { 
    int pmi_ret_val;
    #ifdef USE_PMI2_API
    char name[] = "universeSize";
    int outlen, found;
    PMI2_Info_GetJobAttrIntArray( name, size, sizeof (int), &outlen, &found );
    if( found && outlen==1 ) {
        pmi_ret_val = UPMI_SUCCESS;
    } else {
        pmi_ret_val = UPMI_FAIL;
    }
    #else
    UPMI_lock();
    pmi_ret_val = PMI_Get_universe_size( size );
    UPMI_unlock();
    #endif
    return pmi_ret_val;
}

int UPMI_BARRIER( void ) { 
    int pmi_ret_val;
    #ifdef USE_PMI2_API
    pmi_ret_val = PMI2_KVS_Fence();
    #else
    UPMI_lock();
    pmi_ret_val = PMI_Barrier();
    UPMI_unlock();
    #endif
    return pmi_ret_val;
}

int UPMI_ABORT( int exit_code, const char error_msg[] ) { 
    #ifdef USE_PMI2_API
    return PMI2_Abort( 1, error_msg );    //flag = 1, abort all processes
    #else
    return PMI_Abort( exit_code, error_msg );
    #endif
}

int UPMI_KVS_GET_KEY_LENGTH_MAX( int *length ) { 
    #ifdef USE_PMI2_API
    *length = PMI2_MAX_KEYLEN;
    return UPMI_SUCCESS;
    #else
    return PMI_KVS_Get_key_length_max( length );
    #endif
}

int UPMI_KVS_GET_NAME_LENGTH_MAX( int *length ) { 
    #ifdef USE_PMI2_API
    *length = PMI2_MAX_KEYLEN; //TODO is this correct?
    return UPMI_SUCCESS;
    #else
    return PMI_KVS_Get_name_length_max( length );
    #endif
}

int UPMI_KVS_GET_VALUE_LENGTH_MAX( int *length ) { 
    #ifdef USE_PMI2_API
    *length = PMI2_MAX_VALLEN;
    return UPMI_SUCCESS;
    #else
    return PMI_KVS_Get_value_length_max( length );
    #endif
}

int UPMI_KVS_GET_MY_NAME( char kvsname[], int length ) {
    int pmi_ret_val;
    #ifdef USE_PMI2_API
    pmi_ret_val = PMI2_Job_GetId( kvsname, length );
    #else
    UPMI_lock();
    pmi_ret_val = PMI_KVS_Get_my_name( kvsname, length );
    UPMI_unlock();
    #endif
    return pmi_ret_val;
}

int UPMI_KVS_PUT( const char kvsname[], const char key[], const char value[] ) { 
    int pmi_ret_val;
    #ifdef USE_PMI2_API
    pmi_ret_val = PMI2_KVS_Put( key, value );
    #else
    UPMI_lock();
    pmi_ret_val = PMI_KVS_Put( kvsname, key, value );
    UPMI_unlock();
    #endif
    return pmi_ret_val;
}

int UPMI_KVS_GET( const char kvsname[], const char key[], char value[], int length ) { 
    int pmi_ret_val;
    #ifdef USE_PMI2_API
    int vallen;
    pmi_ret_val = PMI2_KVS_Get( kvsname, PMI2_ID_NULL, key, value, length, &vallen );
    #else
    UPMI_lock();
    pmi_ret_val = PMI_KVS_Get( kvsname, key, value, length );
    UPMI_unlock();
    #endif
    return pmi_ret_val;
}

int UPMI_KVS_COMMIT( const char kvsname[] ) { 
    #ifdef USE_PMI2_API
    //return PMI2_KVS_Fence();
    return UPMI_SUCCESS;
    #else
    return PMI_KVS_Commit( kvsname );
    #endif
}

int UPMI_PUBLISH_NAME( const char service_name[], const char port[], const struct MPID_Info *info_ptr ) { 
    int pmi_ret_val;
    #ifdef USE_PMI2_API
    pmi_ret_val = PMI2_Nameserv_publish( service_name, info_ptr, port );
    #else
    UPMI_lock();
    pmi_ret_val = PMI_Publish_name( service_name, port );
    UPMI_unlock();
    #endif
    return pmi_ret_val;
}

int UPMI_UNPUBLISH_NAME( const char service_name[], const struct MPID_Info *info_ptr ) { 
    int pmi_ret_val;
    #ifdef USE_PMI2_API
    pmi_ret_val = PMI2_Nameserv_unpublish( service_name, info_ptr );
    #else
    UPMI_lock();
    pmi_ret_val = PMI_Unpublish_name( service_name );
    UPMI_unlock();
    #endif
    return pmi_ret_val;
}

int UPMI_LOOKUP_NAME( const char service_name[], char port[], const struct MPID_Info *info_ptr ) { 
    int pmi_ret_val;
    #ifdef USE_PMI2_API
    pmi_ret_val = PMI2_Nameserv_lookup( service_name, info_ptr, port, sizeof port );  
    #else
    UPMI_lock();
    pmi_ret_val = PMI_Lookup_name( service_name, port );
    UPMI_unlock();
    #endif
    return pmi_ret_val;
}

int UPMI_GET_NODE_ATTR( const char name[], char value[], int valuelen, int *found, int waitfor ) {
    #ifdef USE_PMI2_API
    return PMI2_Info_GetNodeAttr( name, value, valuelen, found, waitfor );
    #else
    return UPMI_FAIL;
    #endif
}

int UPMI_GET_NODE_ATTR_INT_ARRAY( const char name[], int array[], int arraylen, int *outlen, int *found ) {
    #ifdef USE_PMI2_API
    return PMI2_Info_GetNodeAttrIntArray( name, array, arraylen, outlen, found );
    #else
    return UPMI_FAIL;
    #endif
}

int UPMI_PUT_NODE_ATTR( const char name[], const char value[] ) {
    #ifdef USE_PMI2_API
    return PMI2_Info_PutNodeAttr( name, value );
    #else
    return UPMI_FAIL;
    #endif
}

int UPMI_GET_JOB_ATTR( const char name[], char value[], int valuelen, int *found ) {
    #ifdef USE_PMI2_API
    return PMI2_Info_GetJobAttr( name, value, valuelen, found );
    #else
    return UPMI_FAIL;
    #endif
}

int UPMI_GET_JOB_ATTR_INT_ARRAY( const char name[], int array[], int arraylen, int *outlen, int *found ) {
    #ifdef USE_PMI2_API
    return PMI2_Info_GetJobAttrIntArray( name, array, arraylen, outlen, found );
    #else
    return UPMI_FAIL;
    #endif
}

int UPMI_JOB_SPAWN(int count,
                   const char * cmds[],
                   int argcs[],
                   const char ** argvs[],
                   const int maxprocs[],
                   const int info_keyval_sizes[],
                   const void *info_keyval_vectors[],
                   int preput_keyval_size,
                   const void *preput_keyval_vector[],
                   char jobId[],
                   int jobIdSize,
                   int errors[])
{
    int pmi_ret_val;
    #ifdef USE_PMI2_API
    pmi_ret_val = PMI2_Job_Spawn( count, cmds, argcs, argvs, maxprocs,
                           info_keyval_sizes, (const struct MPID_Info**)info_keyval_vectors,
                           preput_keyval_size, (const struct MPID_Info**)preput_keyval_vector,
                           jobId, jobIdSize, errors );
    #else
    UPMI_lock();
    pmi_ret_val = PMI_Spawn_multiple( count, cmds, argvs, maxprocs,
                               info_keyval_sizes, (const struct PMI_keyval_t**)info_keyval_vectors,
                               preput_keyval_size, (const struct PMI_keyval_t*)preput_keyval_vector[0],
                               errors );
    UPMI_unlock();
    #endif
    return pmi_ret_val;
}

