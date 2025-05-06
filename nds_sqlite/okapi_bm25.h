#ifndef _OKAPI_BM25_H
#define _OKAPI_BM25_H

#ifdef __cplusplus
extern "C"
{
#endif

//#include <sqlite3ext.h>
#include <assert.h>
#include <stdio.h>
#include "nds_sqlite3.h"

typedef struct sqlite3_api_routines sqlite3_api_routines;

int sqlite3_extension_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);

#ifdef __cplusplus
}
#endif

#endif			//_OKAPI_BM25_H