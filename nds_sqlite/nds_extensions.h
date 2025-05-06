#ifndef NDS_EXTENSIONS_H
#define NDS_EXTENSIONS_H

#include "nds_sqlite3.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * The NDS SQLite release version. This is the version of NDS SQLite extensions as well.
 *
 * Please note that there are another two versions which define the NDS SQLite release:
 *
 * 1. NDSev DevKit version defined in nds_sqlite3.h.
 * 2. SQLite version defined in nds_sqlite3.h.
 *
 * Versions dependencies:
 *
 * 1. All three versions are the same in case of the first release containing public SQLite release.
 * 2. If SQLite version is the same but NDSev DevKit contains some more patches, the NDSeV DevKit version
 *    and NDS SQLite release versions will have versions with postfix "-1".
 * 3. If SQLite version and NDSev DevKit are the same but NDS SQLite extensions contain some more patches,
 *    the NDS SQLite release version will have version with postfix "-1".
 * 4. In case of multiple releases without changed SQLite version, versions postfixes will be incremented.
 */
#define NDS_SQLITE_VERSION "3.46.0"

/**
 * The function registers all NDS extensions.
 *
 * \param p_db The pointer to database for which to register the extensions.
 *
 * \return Returns SQLITE_OK in case of success, otherwise returns SQLite error code.
 */
int nds_extensions_init(sqlite3* p_db);

/* NDS Extensions API */

#ifdef _WIN32
    #if defined(NDS_EXTENSIONS_EXPORT)
        #define NDS_EXTENSIONS_API __declspec(dllexport)
    #elif defined(NDS_EXTENSIONS_IMPORT)
        #define NDS_EXTENSIONS_API __declspec(dllimport)
    #else
        #define NDS_EXTENSIONS_API
    #endif
#else
    #ifdef __GNUC__
        #define NDS_EXTENSIONS_API __attribute__((visibility("default")))
    #else
        #define NDS_EXTENSIONS_API
    #endif
#endif

/**
 * Gets NDS SQLite release version.
 *
 * \return A const char pointer to NDS SQLite release version.
 */
NDS_EXTENSIONS_API const char* nds_sqlite_version();

/**
 * An opaque structure containing the actual description of a collation.
 *
 * This is returned by nds_lookup_collation_data() and used in nds_localized_compare().
 */
struct tagNdsCollationData;
typedef struct tagNdsCollationData nds_collation;

/**
 * Look up collation data by name.
 *
 * \note The returned pointer must not be freed. It's valid throughout the lifecycle of the application.
 *
 * \return A pointer to collation data (to be used with nds_localized_compare()) or NULL if not found.
 */
NDS_EXTENSIONS_API nds_collation const* nds_lookup_collation_data(char const* collation_name);

/**
 * Compares two utf8-encoded strings using given collation data.
 *
 * \param[in] collation_data Collation data to use for the comparison returned by nds_lookup_collation_data().
 * \param[in] string1        Pointer to the first string to compare.
 * \param[in] string1_len    Length (in bytes) of the first string.
 * \param[in] string2        Pointer to the second string to compare.
 * \param[in] string2_len    Length (in bytes) of the second string.
 *
 * \return 0 if the strings are equal, a number lower than 0 if the first string is lower or
 * a number greater than 0 if the second string is lower.
 */
NDS_EXTENSIONS_API int nds_localized_compare(nds_collation const* collation_data, char const* string1,
        int string1_len, char const* string2, int string2_len);

/*
 * NDS xAutoDetect callback for ZipVFS.
 *
 * The job of this function is to figure out which compression and encryption algorithm to use for a database
 * and fill out the methods object with pointers to the appropriate compression and decompression routines.
 *
 * \param[in]  ctx     Copy of pCtx from zipvfs_create_vfs_v3().
 * \param[in]  file    Name of file being opened.
 * \param[in]  header  Algorithm name in the database header.
 * \param[out] methods Write new pCtx and function pointers here.
 */
NDS_EXTENSIONS_API int nds_compression_algorithm_detector(void* ctx, const char* file, const char* header,
        ZipvfsMethods* methods);

#ifdef __cplusplus
}
#endif

#endif /* NDS_EXTENSIONS_H */
