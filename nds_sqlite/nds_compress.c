/*
** This version of SQLite is specially prepared for the
** Navigation Data Standard e.V.  Use by license only.
**
** This file contains code use to:
**
**   (1) Implement compression and decompression of database pages
**   (2) Implement encryption and decryption of database pages
**   (3) Determine which compression and encryption algorithms are to
**       be used for a particular database.
**
** The key component of this file is the nds_compression_algorithm_detector()
** function implemented near the bottom. All the other routines in this file
** merely support the nds_compression_algorithm_detector() function. If you
** are reading this file for the first time, and trying to understand what
** is happening, it is suggested that you begin at the bottom with the
** nds_compression_algorithm_detector() function and then work your way
** upward.
**
** Here are the compression methods supported by this module:
**
**    zlib    This method uses the world-famous zLib compressor - the
**            same compression technology used in ZIP archives and in gzip.
**            The compression and decompression routines are in an external
**            library. The code in this file merely invokes the external
**            library. This compression method is only included if this
**            file is compiled with the NDS_ENABLE_ZLIB macro defined.
**
**    lz4     This method uses the famous LZ4 compression. The code
**            in this file merely invokes the external library. This compression
**            method is only included if this file is compiled with the
**            NDS_ENABLE_LZ4 macro defined. If this file is compiled with the
**            NDS_ENABLE_LZ4_DECOMPRESS_FAST, LZ4_decompress_fast() method
**            is used instead of LZ4_decompress_safe() method.
**
**    lz4hc   This method is high compression alternative of LZ4. It uses
**            the same decompression routine as the lz4 compression method.
**            The code in this file merely invokes the external library. This
**            compression method is only included if this file is compiled
**            with the NDS_ENABLE_LZ4 macro defined. If this file is compiled
**            with the NDS_ENABLE_LZ4_DECOMPRESS_FAST, LZ4_decompress_fast()
**            method is used instead of LZ4_decompress_safe() method.
**
**    ndsc    This method uses NDS internal compression algorithm. The code
**            in this file merely invokes the external library. This compression
**            method is only included if this file is compiled with the
**            NDS_ENABLE_NDSC macro defined.
**
**    bsr     "Blank Space Removal". Many SQLite pages contain large spans
**            of zero bytes. This compression method searches for the single
**            longest span of zeros within each page and removes it. This
**            compression method is implemented in this module.
**
**    bsr2    "Black Space Removal" with safer decryption. First two bytes
**            of compressed data (length of the largest spans of zero bytes)
**            are not encrypted. In this way, possible memory corruption is
**            avoided if wrong password during decryption is applied. This
**            compression method is implemented in this module.
**
**    none    No compression. This method actually does not do any compression
**            but allows to use encryption on uncompressed data.
**
**    brotli  This method uses the brotli compression. The code in this file
**            merely invokes the external library. This method is only enabled
**            if this file is compiled with the NDS_ENABLE_BROTLI defined.
**
**    zstd    This method uses the Zstandard compression. The code in this file
**            merely invokes the external library. This method is only enabled
**            if this file is compiled with the NDS_ENABLE_ZSTD defined.
**
**    zstd_d  This method uses the Zstandard compression using external
**            dictionary. The code in this file merely invokes the external
**            library. This method is only enabled if this file is compiled
**            with the NDS_ENABLE_ZSTD_DICT defined.
**
** The ZipVFS extension is not compelled to do compression on the database.
** It can also simply pass through the file content, resulting in an
** uncompressed database file that can be read and written by ordinary
** unextended versions of SQLite. Some people think of the no-compression
** option as a fifth form of compression. But are there no compression
** and decompression routines associated with the no-compression option.
**
** In addition to doing compression/decompression, the ZipVFS module is
** also able to do encryption/decryption using Rijndael AES encryption
** algorithm. This encryption is only included if this file is compiled with
** the NDS_ENABLE_AES macro defined. Additionally, If the encryption/decryption
** algorithm is provided by crypto shared library, the NDS_ENABLE_CRYPTO_LIB
** should be defined.
*/
#include "nds_sqlite3.h"
#include "nds_extensions.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <limits.h>
#ifdef NDS_ENABLE_AES
# include <rijndael.h>
#endif /* ifdef NDS_ENABLE_AES */
#ifdef NDS_ENABLE_CRYPTO_LIB
# include "crypto.h"
#endif /* ifdef NDS_ENABLE_CRYPTO_LIB */
#ifdef NDS_ENABLE_ZLIB
# include <zlib.h>
#endif /* ifdef NDS_ENABLE_ZLIB */
#ifdef NDS_ENABLE_LZ4
# include <lz4.h>
# include <lz4hc.h>
#endif /* ifdef NDS_ENABLE_LZ4 */
#ifdef NDS_ENABLE_NDSC
# include <packerNDSC.h>
#endif /* ifdef NDS_ENABLE_NDSC */
#ifdef NDS_ENABLE_BROTLI
# include <brotli.h>
#endif  /* ifdef NDS_ENABLE_BROTLI */
#if defined NDS_ENABLE_ZSTD || defined NDS_ENABLE_ZSTD_DICT
# include <zstd.h>
#endif /* if defined NDS_ENABLE_ZSTD || defined NDS_ENABLE_ZSTD_DICT */

/*
** Forward declarations of structures
*/
typedef struct ZipvfsInst ZipvfsInst;
typedef struct ZipvfsAlgorithm ZipvfsAlgorithm;

/*
** Each open connection to a ZIPVFS database has an instance of the following
** structure which holds context information for that connection.
**
** This structure is automatically allocated and initialized when the database
** connection is opened and automatically freed when the database connection
** is closed.
**
** The pComprContext and pDecmprContext fields hold information needed by the
** compressor and decompressor functions.
**
** The pCryptoContex field holds context information for the encryption/decryption
** logic.
**
** Callback routines for doing encryption/decryption and compression and
** uncompression are always passed a pointer to the complete ZipvfsInst
** structure. Those routines should then access the particular fields of
** ZipvfsInst structure that are relevant to them.
**
** The setup and breakdown routines (xComprSetup(), xComprCleanup(),
** xDecmprSetup() and xDecmprCleanup()) are also passed a pointer to the
** complete ZipvfsInst object. Once again, these routines should setup
** or breakdown only those fields of the ZipvfsInst object that are relevant.
*/
struct ZipvfsInst {
  void  *pComprContext;         /* Context ptr for compression */
  void  *pDecmprContext;        /* Context ptr for decompression */
  void  *pCryptoContex;         /* Context ptr for encryption and decryption */
  const ZipvfsAlgorithm *pAlg;  /* Corresponding algorithm object */
  ZipvfsMethods *pMethods;      /* Corresponding zipvfs object */
  int   iLevel;                 /* Compression level */
  char  zHdr[16];               /* Complete header */
};

/*
** An instance of the following structure describes a single ZIPVFS
** compression and encryption algorithm.
**
** There is an array of instances of this object further down in this file
** that contains descriptions of all supported algorithms.
**
** zName                This is the name of the algorithm.  When creating
**                      a new database, use the "zv=NAME" query parameter
**                      to select an algorithm, where NAME matches this field.
**                      The name will written into the database header and
**                      so when reopening the database, the same algorithm
**                      will be used.
**
** xBound(X,N)          This function returns the maximum size of the output
**                      buffer for xCompr() assuming that the input is
**                      N bytes in size.  X is a pointer to the ZipvfsInst
**                      object for the open connection.
**
** xComprSetup(X,F)     This function is called once when the database is
**                      opened.  Its job is to setup the X->pEncode field
**                      appropriately.  F is the name of the database file
**                      and is suitable for passing to sqlite3_uri_parameter().
**                      Return SQLITE_OK on success or another
**                      error code (ex: SQLITE_NOMEM) upon error.
**
** xCompr(X,O,N,I,M)    This function does compression on input buffer I
**                      of size M bytes, and writes the output into buffer O
**                      and the output buffer size into N.
**
** xComprCleanup(X)     This function is called once when the database is
**                      closing.  Its job is to cleanup the X->pEncode field.
**                      This routine undoes the work of xComprSetup().
**
** xDecmprSetup(X,F)    This function is called once when the database is
**                      opened.  It should setup the X->pDecode field as
**                      is appropriate.  F is the name of the database file,
**                      suitable for passing to sqlite3_uri_parameter().
**
** xDecmpr(X,O,N,I,M)   This routine does decompression.  The input in buffer
**                      I which is M bytes in size is decompressed into an
**                      output buffer O.  The number of bytes of decompressed
**                      content should be written into N.
**
** xDecmprCleanup(X)    This function is called once when the database is
**                      closed in order to cleanup the X->pDecode field.
**                      This undoes the work of xDecmprSetup().
**
** xCryptoSetup(X,F)    This function is invoked once when the database is
**                      first opened, for the purpose of setting up the
**                      X->pCryptoContex field.  The F argument is the name
**                      of the file that is being opened.  This routine can use
**                      sqlite3_uri_parameter(F, "password") to determine
**                      the value of the password query parameter, if desired.
**
** xEncrypt(X,O,I,M)    This routine does content encryption.  The input
**                      buffer I of size M bytes is encrypted and the results
**                      written into output buffer O.  Note that this routine
**                      must be invoked by the xCompr() method.  See the
**                      example implementations below for details.
**
** xDecrypt(X,0,I,M)    This routine does content decryption.  The input
**                      buffer I of size M bytes is decrypted into the output
**                      buffer O.  Note that this routine must be invoked
**                      by the xDecmpr() method.  See the examples below for
**                      a demonstration.
**
** xCryptoCleanup(X)    This routine is called when the database is closing
**                      in order to cleanup the X->pCryptoContex field.
*/
struct ZipvfsAlgorithm {
  const char zName[12];
  int (*xBound)(void*,int);
  int (*xComprSetup)(ZipvfsInst*,const char*);
  int (*xCompr)(void*,char*,int*,const char*,int);
  int (*xComprCleanup)(ZipvfsInst*);
  int (*xDecmprSetup)(ZipvfsInst*,const char*);
  int (*xDecmpr)(void*,char*,int*,const char*,int);
  int (*xDecmprCleanup)(ZipvfsInst*);
  int (*xCryptoSetup)(ZipvfsInst*, const char *zFile);
  int (*xEncrypt)(ZipvfsInst*,char*,const char*,int);
  int (*xDecrypt)(ZipvfsInst*,char*,const char*,int);
  int (*xCryptoCleanup)(ZipvfsInst*);
};

/*
** This routine returns pointer to encryption password or NULL if no encryption
** is requested.
*/
static const char *aesGetEncryptionPassword(const char *zFilename) {
  return sqlite3_uri_parameter(zFilename, "password");
}

#ifdef NDS_ENABLE_AES
/*****************************************************************************
** Encryption routines.
**
** There are three possibilities how to use encryption:
**
** 1. If the ZIPVFS module is started using a URI filename that contains
**    "ndsSupplierId=", "keyStoreId" and "keyId" parameters, then external
**    crypto shared library is used for encryption and decryption.
**
** 2. If the ZIPVFS module is started using a URI filename that contains
**    "keyStore=" and "keyId" parameters, then specified key store file
**    is used to get 16-bytes key for Rijndael AES encryption algorithm.
**
** 3. If the ZIPVFS module is started using a URI filename that contains
**    a "password=" query parameter, then the password is used as 16-bytes key
**    for Rijndael AES encryption algorithm.
**
** The aesEncryptionSetup() and aesEncryptionCleanup() routines are setup and
** breakdown routines for the encryption logic. The actual encryption
** and decryption of content is performed by aesEncrypt() and aesDecrypt()
** routines.
*/

#define AES_ENCRYPTION_KEY_BITS    128
#define AES_ENCRYPTION_NUM_BLOCKS  4
#define AES_ENCRYPTION_BLOCK_SIZE  (KEYLENGTH(AES_ENCRYPTION_KEY_BITS))

/* This structure is filled in aesEncryptionSetup() and passed to aesEncrypt()
** and aesDecrypt() routines and holds necessary input data for AES
** Rijndael encryption algorithm.
*/
struct CryptoContex {
  /* Crypto shared library session id or NULL if shread library is not used. */
  void          *pCryptoSessionId;
  char          *pDecryptTempBuffer;    /* Temp buffer used during decryption */
  int           DecryptTempBufferSize;  /* Temp buffer size in bytes */
  unsigned long EncryptBuffer[RKLENGTH(AES_ENCRYPTION_KEY_BITS)];
  int           EncryptRounds;
  unsigned long DecryptBuffer[RKLENGTH(AES_ENCRYPTION_KEY_BITS)];
  int           DecryptRounds;
};

typedef struct CryptoContex CryptoContex;

#define MAX_URI_LENGTH    512

static int aesGetKeyFromStatement(int keyId, sqlite3_stmt *pStmt,
        unsigned char *key, int keySize) {
  int result;
  const char *keyFromDb;
  int keyFromDbSize;
  int i;

  result = sqlite3_bind_int(pStmt, 1, keyId);
  if (result != SQLITE_OK)
    return result;

  result = sqlite3_step(pStmt);
  if (result != SQLITE_ROW)
    return SQLITE_ERROR;

  keyFromDb = sqlite3_column_blob(pStmt, 0);
  keyFromDbSize = sqlite3_column_bytes(pStmt, 0);
  if (!keyFromDb || keyFromDbSize != keySize)
    return SQLITE_ERROR;

  for (i = 0; i < keySize; ++i) {
    const char curKeyFromDb = keyFromDb[i];
    // "terminated" zero character in the middle of the key is forbidden
    if (curKeyFromDb == '\0')
      return SQLITE_ERROR;
    key[i] = curKeyFromDb;
  }

  return SQLITE_OK;
}

/*
** This routine reads AES key from keystore specified in URI.
*/
static int aesGetKeyFromKeystore(int keyId, const char *keyStore,
                const char *keyStorePassword, unsigned char *key, int keySize) {
  sqlite3 *pDb;
  sqlite3_stmt *pStmt;
  char *snprintf_result;
  int result;
  char uri[MAX_URI_LENGTH];

  snprintf_result = sqlite3_snprintf(MAX_URI_LENGTH, uri, "file:%s?zv=zlib&password=%s",
                       keyStore, keyStorePassword);
  if (snprintf_result == NULL)
      return SQLITE_ERROR;

  result = sqlite3_open_v2(uri, &pDb, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, NULL);
  if (result != SQLITE_OK) {
      sqlite3_close(pDb);
      return result;
  }

  /* CREATE TABLE encryptionKeyTable(keyIndex INTEGER PRIMARY KEY, key BLOB NOT NULL); */
  result = sqlite3_prepare_v2(pDb,
              "SELECT key FROM encryptionKeyTable WHERE keyIndex = ? LIMIT 1",
              -1, &pStmt, NULL);
  if (result != SQLITE_OK) {
    sqlite3_close(pDb);
    return result;
  }

  result = aesGetKeyFromStatement(keyId, pStmt, key, keySize);
  if (result != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    sqlite3_close(pDb);
    return result;
  }

  result = sqlite3_finalize(pStmt);
  if (result != SQLITE_OK) {
    sqlite3_close(pDb);
    return result;
  }

  result = sqlite3_close(pDb);

  return result;
}

/*
** This routine determines whether or not encryption
** should be used for the database file. This routine
** is called directly from ZIPVFS if ZIPVFS was registered using
** zipvfs_create_vfs_v2().  If ZIPVFS was registered using the newer
** zipvfs_create_vfs_v3() interface then ZIPVFS will never invoke this
** routine itself; the xAutoDetect callback supplied by the application
** should invoke this routine instead.
*/
static int aesEncryptionSetup(ZipvfsInst *pInst, const char *zFilename) {
  /* by default encryption and decryption is not used */
  pInst->pCryptoContex = NULL;

  if (pInst->pAlg->xEncrypt != 0 && pInst->pAlg->xDecrypt != 0) {
    const char *zPasswd = aesGetEncryptionPassword(zFilename);
    const char *keyId = sqlite3_uri_parameter(zFilename, "keyId");
    void* pCryptoSessionId = NULL;
    int isAesKeyValid = 0;
    unsigned char aesKey[KEYLENGTH(AES_ENCRYPTION_KEY_BITS)];

#ifdef NDS_ENABLE_CRYPTO_LIB
    const char *ndsSupplierId = sqlite3_uri_parameter(zFilename, "ndsSupplierId");
    const char *keyStoreId = sqlite3_uri_parameter(zFilename, "keyStoreId");
    if (ndsSupplierId && keyStoreId && keyId)
    {
      /* external crypto shared library will be used */
      const char *keyStoreDir = sqlite3_uri_parameter(zFilename, "keyStoreDir");
      CryptoSQLiteFunctionPtrs funcs;

      if (zPasswd == NULL)
        return SQLITE_ERROR;
      funcs.snprintf = sqlite3_snprintf;
      funcs.open_v2 = sqlite3_open_v2;
      funcs.prepare_v2 = sqlite3_prepare_v2;
      funcs.bind_int = sqlite3_bind_int;
      funcs.step = sqlite3_step;
      funcs.column_blob = sqlite3_column_blob;
      funcs.column_bytes = sqlite3_column_bytes;
      funcs.close = sqlite3_close;
      funcs.finalize = sqlite3_finalize;
      pCryptoSessionId = crypto_initialize(atoi(ndsSupplierId), atoi(keyStoreId),
              atoi(keyId), keyStoreDir, zPasswd, &funcs, NULL);
      if (pCryptoSessionId == NULL)
        return SQLITE_ERROR;
    }
    else
#endif /* ifdef NDS_ENABLE_CRYPTO_LIB */
    {
      const char *keyStore = sqlite3_uri_parameter(zFilename, "keyStore");
      if (keyStore && keyId) {
        /* AES key will be read from specified key store */
        int result;
        if (zPasswd == NULL)
          return SQLITE_ERROR;
        result = aesGetKeyFromKeystore(atoi(keyId), keyStore, zPasswd, aesKey,
                        (int)(sizeof(aesKey) / sizeof(aesKey[0])));
        if (result != SQLITE_OK)
          return result;
        isAesKeyValid = 1;
      }
      else if (zPasswd) {
        /* AES key will be specified password */
        int i;
        const char *zCurPasswd = zPasswd;
        for (i = 0; i < (int)(sizeof(aesKey) / sizeof(aesKey[0])); i++)
          aesKey[i] = *zCurPasswd != 0 ? *zCurPasswd++ : 0;
        isAesKeyValid = 1;
      }
    }

    if (pCryptoSessionId != NULL || isAesKeyValid) {
      CryptoContex *pCryptoContex = (CryptoContex*)
                            sqlite3_malloc(sizeof(CryptoContex));
      if (pCryptoContex == NULL)
          return SQLITE_ERROR;

      pCryptoContex->pCryptoSessionId = pCryptoSessionId;
      pCryptoContex->pDecryptTempBuffer = NULL;
      pCryptoContex->DecryptTempBufferSize = 0;
      if (isAesKeyValid) {
        pCryptoContex->EncryptRounds = rijndaelSetupEncrypt(
                    pCryptoContex->EncryptBuffer, aesKey, AES_ENCRYPTION_KEY_BITS);
        pCryptoContex->DecryptRounds = rijndaelSetupDecrypt(
                    pCryptoContex->DecryptBuffer, aesKey, AES_ENCRYPTION_KEY_BITS);
      }
      pInst->pCryptoContex = pCryptoContex;
    }
  }

  return SQLITE_OK;
}

/* This routine is called by ZIPVFS when a database connection is shutting
** down in order to deallocate resources previously allocated by
** aesEncryptionSetup().
*/
static int aesEncryptionCleanup(ZipvfsInst *pInst) {
  if (pInst->pCryptoContex != NULL) {
    const CryptoContex *pCryptoContex = (CryptoContex*)(pInst->pCryptoContex);
#ifdef NDS_ENABLE_CRYPTO_LIB
    if (pCryptoContex->pCryptoSessionId != NULL)
      crypto_shutdown(pCryptoContex->pCryptoSessionId);
#endif /* ifdef NDS_ENABLE_CRYPTO_LIB */
    sqlite3_free(pCryptoContex->pDecryptTempBuffer);
    sqlite3_free(pInst->pCryptoContex);
    pInst->pCryptoContex = NULL;
  }

  return SQLITE_OK;
}

/* This enum is used to distinguish AES descryption or encryption in helper
** routine aesEncryptDecrypt().
*/
enum AesMethod{
  AES_ENCRYPTION,
  AES_DECRYPTION
};

/* This is the helper routine for AES encryption and decryption.
*/
static void aesEncryptDecrypt(const CryptoContex *pCryptoContex, const char *zIn,
                              int nIn, char *zOut, enum AesMethod eAesMethod) {
  const unsigned char *zCurIn = (const unsigned char*)zIn;
  unsigned char *zCurOut = (unsigned char*)zOut;
  int nRestIn = nIn;
  const int nMaxEncryptionSize = AES_ENCRYPTION_NUM_BLOCKS *
                                 AES_ENCRYPTION_BLOCK_SIZE;
  const int nNumBlocks = (nIn >= nMaxEncryptionSize) ?
                                 AES_ENCRYPTION_NUM_BLOCKS :
                                 nIn / AES_ENCRYPTION_BLOCK_SIZE;
  int i = 0;
  for (; i < nNumBlocks; ++i) {
    if (eAesMethod == AES_ENCRYPTION)
      rijndaelEncrypt(pCryptoContex->EncryptBuffer,
                      pCryptoContex->EncryptRounds, zCurIn, zCurOut);
    else
      rijndaelDecrypt(pCryptoContex->DecryptBuffer,
                      pCryptoContex->DecryptRounds, zCurIn, zCurOut);
    zCurIn += AES_ENCRYPTION_BLOCK_SIZE;
    zCurOut += AES_ENCRYPTION_BLOCK_SIZE;
    nRestIn -= AES_ENCRYPTION_BLOCK_SIZE;
  }
  if (zIn != zOut)
    memcpy(zCurOut, zCurIn, nRestIn);
}

/* This is the routine that does the AES encryption.
**
** ZIPVFS does *not* call this routine directly.  Instead, the
** various compression and decompression routines must call this
** routine themselves.
**
** The first paramater pInst->pCryptoContex contain pointer to CryptoContex
** structure allocated by aesEncryptionSetup() routine or NULL if encryption
** is not required.
*/
static int aesEncryption(ZipvfsInst *pInst, char *zOut, const char *zIn, int nIn) {

  if (pInst->pCryptoContex != NULL) {
    const CryptoContex *pCryptoContex = (CryptoContex*)(pInst->pCryptoContex);
#ifdef NDS_ENABLE_CRYPTO_LIB
    if (pCryptoContex->pCryptoSessionId != NULL)
    {
      crypto_encrypt(pCryptoContex->pCryptoSessionId, zIn, nIn, zOut);
    }
  else
#endif /* ifdef NDS_ENABLE_CRYPTO_LIB */
    {
      aesEncryptDecrypt(pCryptoContex, zIn, nIn, zOut, AES_ENCRYPTION);
    }
  }

  return SQLITE_OK;
}

/* This is the routine that does the AES decryption.
**
** ZIPVFS does *not* call this routine directly.  Instead, the
** various compression and decompression routines must call this
** routine themselves.
**
** The first paramater pInst contain pointer to CryptoContex structure
** allocated by aesEncryptionSetup() routine or NULL if encryption is not
** required.
*/
static int aesDecryption(ZipvfsInst *pInst, char *zOut, const char *zIn, int nIn) {

  if (pInst->pCryptoContex != NULL) {
    const CryptoContex *pCryptoContex = (CryptoContex*)(pInst->pCryptoContex);
#ifdef NDS_ENABLE_CRYPTO_LIB
    if (pCryptoContex->pCryptoSessionId != NULL)
    {
      crypto_decrypt(pCryptoContex->pCryptoSessionId, zIn, nIn, zOut);
    }
    else
#endif /* ifdef NDS_ENABLE_CRYPTO_LIB */
    {
      aesEncryptDecrypt(pCryptoContex, zIn, nIn, zOut, AES_DECRYPTION);
    }
  }

  return SQLITE_OK;
}

/*
** This is a wrapper routine around xDecrypt().  It allocates space to do
** the decryption so that the input, aIn[], is not altered.   A pointer to
** the decrypted text is returned.  NULL is returned if there is a memory
** allocation error.
*/
static const char *aesDecryptWrapper(ZipvfsInst *pInst, const char *aIn,
        const int nIn) {
  const char *ret = aIn;
  if (pInst->pCryptoContex != NULL) {
    CryptoContex *pCryptoContex = (CryptoContex*)(pInst->pCryptoContex);
    if (pCryptoContex->DecryptTempBufferSize < nIn) {
      int nNew = nIn;
      char *aNew = sqlite3_realloc(pCryptoContex->pDecryptTempBuffer, nNew);
      if (aNew == 0)
        return NULL;
      pCryptoContex->pDecryptTempBuffer = aNew;
      pCryptoContex->DecryptTempBufferSize = nNew;
    }
    pInst->pAlg->xDecrypt(pInst, pCryptoContex->pDecryptTempBuffer, aIn, nIn);
    ret = pCryptoContex->pDecryptTempBuffer;
  }

  return ret;
}

/* End encryption logic
******************************************************************************/

#endif /* ifdef NDS_ENABLE_AES */

#define DISCARD_PARAMETER(param) (void)param

#ifdef NDS_ENABLE_ZLIB

/*****************************************************************************
** ZLIB compression for ZipVFS.
**
** These routines implement compression using the external ZLIB library.
*/
static int zlibBound(void *pLocalCtx, int nByte) {
  DISCARD_PARAMETER(pLocalCtx);

  return (int)compressBound((uLong)nByte);
}

static int zlibCompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc) {
  uLongf nDest = *pnDest;         /* In/out buffer size for compress() */
  int result;                     /* compress() return code */
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
  int iLevel = pInst->iLevel;

  if (iLevel < Z_NO_COMPRESSION || iLevel > Z_BEST_COMPRESSION)
    iLevel = Z_DEFAULT_COMPRESSION;
  result = compress2((Bytef*)aDest, &nDest, (const Bytef*)aSrc,
          (uLong)nSrc, iLevel);
  if (result != Z_OK)
      return SQLITE_ERROR;

#ifdef NDS_ENABLE_AES
  if (pInst->pCryptoContex != NULL) {
    pInst->pAlg->xEncrypt(pInst, aDest, aDest, (int)nDest);
  }
#endif /* ifdef NDS_ENABLE_AES */
  *pnDest = (int)nDest;

  return SQLITE_OK;
}

static int zlibUncompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc) {
  uLongf nDest = *pnDest;         /* In/out buffer size for uncompress() */
  int result;                     /* uncompress() return code */

#ifdef NDS_ENABLE_AES
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
  aSrc = aesDecryptWrapper(pInst, aSrc, nSrc);
  if (aSrc == NULL)
    return SQLITE_NOMEM;
#else
  DISCARD_PARAMETER(pLocalCtx);
#endif /* ifdef NDS_ENABLE_AES */

  result = uncompress((Bytef*)aDest, &nDest, (const Bytef*)aSrc, (uLong)nSrc);
  if (result != Z_OK)
      return SQLITE_ERROR;

  *pnDest = (int)nDest;

  return SQLITE_OK;
}
/* End ZLIB compression
******************************************************************************/

#endif /* NDS_ENABLE_ZLIB */

#ifdef NDS_ENABLE_LZ4

#define LZ4_ACCELERATION_DEFAULT    1

/*****************************************************************************
** LZ4 compression for ZipVFS.
**
** These routines implement compression using the external LZ4 library.
*/
#ifdef NDS_ENABLE_LZ4_PREALLOCATION
static int lz4ComprSetup(ZipvfsInst* pInst, const char* zFile) {
  DISCARD_PARAMETER(zFile);
  pInst->pComprContext = sqlite3_malloc(LZ4_sizeofState());

  return (pInst->pComprContext != NULL) ? SQLITE_OK : SQLITE_ERROR;
}
#endif /* NDS_ENABLE_LZ4_PREALLOCATION */

static int lz4Bound(void *pLocalCtx, int nByte) {
  DISCARD_PARAMETER(pLocalCtx);

  return LZ4_compressBound(nByte);
}

static void lz4Encrypt(ZipvfsInst *pInst, char *aDest, int nDest) {
  if (pInst->pCryptoContex != NULL)
    pInst->pAlg->xEncrypt(pInst, aDest, aDest, nDest);
}

static int lz4Compress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc) {
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;

#ifdef NDS_ENABLE_LZ4_PREALLOCATION
  int nDest = LZ4_compress_fast_extState(pInst->pComprContext, aSrc, aDest, nSrc,
          *pnDest, LZ4_ACCELERATION_DEFAULT);
#else
  int nDest = LZ4_compress_fast(aSrc, aDest, nSrc, *pnDest,
		  LZ4_ACCELERATION_DEFAULT);
#endif /* NDS_ENABLE_LZ4_PREALLOCATION */
  if (nDest == 0)
    return SQLITE_ERROR;

#ifdef NDS_ENABLE_AES
  lz4Encrypt(pInst, aDest, nDest);
#endif /* ifdef NDS_ENABLE_AES */
  *pnDest = nDest;

  return SQLITE_OK;
}

#ifdef NDS_ENABLE_LZ4_PREALLOCATION
static int lz4ComprCleanup(ZipvfsInst* pInst) {
  sqlite3_free(pInst->pComprContext);

  return SQLITE_OK;
}

static int lz4hcComprSetup(ZipvfsInst* pInst, const char* zFile) {
  DISCARD_PARAMETER(zFile);
  pInst->pComprContext = sqlite3_malloc(LZ4_sizeofStateHC());

  return (pInst->pComprContext != NULL) ? SQLITE_OK : SQLITE_ERROR;
}
#endif /* NDS_ENABLE_LZ4_PREALLOCATION */

static int lz4hcCompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc) {
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
  int iLevel = pInst->iLevel;
  int nDest;
  if (iLevel < LZ4HC_CLEVEL_MIN || iLevel > LZ4HC_CLEVEL_MAX)
    iLevel = LZ4HC_CLEVEL_DEFAULT;

#ifdef NDS_ENABLE_LZ4_PREALLOCATION
  nDest = LZ4_compress_HC_extStateHC(pInst->pComprContext, aSrc, aDest, nSrc,
          *pnDest, iLevel);
#else
  nDest = LZ4_compress_HC(aSrc, aDest, nSrc, *pnDest, iLevel);
#endif /* NDS_ENABLE_LZ4_PREALLOCATION */
  if (nDest == 0)
    return SQLITE_ERROR;

#ifdef NDS_ENABLE_AES
  lz4Encrypt(pInst, aDest, nDest);
#endif /* ifdef NDS_ENABLE_AES */
  *pnDest = nDest;

  return SQLITE_OK;
}

static int lz4Uncompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc) {
  int nDest;

#ifdef NDS_ENABLE_AES
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
  aSrc = aesDecryptWrapper(pInst, aSrc, nSrc);
  if (aSrc == NULL)
    return SQLITE_NOMEM;
#else
  DISCARD_PARAMETER(pLocalCtx);
#endif /* ifdef NDS_ENABLE_AES */

#ifndef NDS_ENABLE_LZ4_DECOMPRESS_FAST
  nDest = LZ4_decompress_safe(aSrc, aDest, nSrc, *pnDest);
#else
  DISCARD_PARAMETER(nSrc);
  nDest = LZ4_decompress_fast(aSrc, aDest, *pnDest);
#endif /* ifndef NDS_ENABLE_LZ4_DECOMPRESS_FAST */

  if (nDest < 0)
    return SQLITE_ERROR;

#ifndef NDS_ENABLE_LZ4_DECOMPRESS_FAST
  *pnDest = nDest;
#endif /* ifndef NDS_ENABLE_LZ4_DECOMPRESS_FAST */

  return SQLITE_OK;
}
/* End LZ4 compression
******************************************************************************/

#endif /* NDS_ENABLE_LZ4 */

#ifdef NDS_ENABLE_NDSC

/*****************************************************************************
** NDSC compression for ZipVFS.
**
** These routines implement compression using the external NDSC library.
*/

#define NDSC_MIN_COMPRESSION_LEVEL      0
#define NDSC_DEFAULT_COMPRESSION_LEVEL  4
#define NDSC_MAX_COMPRESSION_LEVEL      8

int ndscBound(void *pLocalCtx, int nByte)
{
   return CalcNDSC(pLocalCtx, nByte, 0);
}

int ndscCompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char* aSrc,  int nSrc) {
   ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
   unsigned int outLen = (unsigned int)*pnDest;
   int iLevel = pInst->iLevel;
   int result;
   if (iLevel < NDSC_MIN_COMPRESSION_LEVEL || iLevel > NDSC_MAX_COMPRESSION_LEVEL)
     iLevel = NDSC_DEFAULT_COMPRESSION_LEVEL;
   result = PackNDSC((unsigned char*)aSrc, (unsigned int)nSrc, (unsigned char*)aDest,
           outLen, &outLen, iLevel);
   if (result != 0)
       return SQLITE_ERROR;

   *pnDest = (int)outLen;
#ifdef NDS_ENABLE_AES
   if (pInst->pCryptoContex != NULL) {
     pInst->pAlg->xEncrypt(pInst, aDest, aDest, *pnDest);
   }
#endif /* ifdef NDS_ENABLE_AES */

   return SQLITE_OK;
}

int ndscUncompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc) {
   int result;

#ifdef NDS_ENABLE_AES
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
  aSrc = aesDecryptWrapper(pInst, aSrc, nSrc);
  if (aSrc == NULL)
    return SQLITE_NOMEM;
#else
  DISCARD_PARAMETER(pLocalCtx);
#endif /* ifdef NDS_ENABLE_AES */

   result = UnpackNDSC((unsigned char*)aSrc, (unsigned int)nSrc, (unsigned char*)aDest,
           (unsigned int)*pnDest);

   return (result == 0) ? SQLITE_OK : SQLITE_ERROR;
}
/* End NDSC compression
******************************************************************************/

#endif /* NDS_ENABLE_NDSC */

/******************************************************************************
** Blank-space removal compression routines for use with ZIPVFS
**
** Many SQLite database pages contain a large span of zero bytes.  This
** compression algorithm attempts to reduce the page size by simply not
** storing the single largest span of zeros within each page.
**
** The compressed format consists of a single 2-byte big-endian number X
** followed by N bytes of content.  To decompress, copy the first X
** bytes of content, followed by Pagesize-N zeros, followed by the final
** N-X bytes of content.
*/

/* This enum is used to distinguish BSR and BSR2 compression methods. */
enum bsr_method {
  BSR_COMPRESSION,
  BSR2_COMPRESSION
};

static int bsrBoundImpl(int nByte) {
  return nByte + 2;
}

static int bsrCompressImpl(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc, enum bsr_method eBsrMethod) {
  const char *p = aSrc;           /* Loop pointer */
  const char *pEnd = &aSrc[nSrc]; /* Ptr to end of data */
  const char *pBestStart = p;     /* Ptr to first byte of longest run of zeros */
  const char *pN = pEnd;  /* End of data to search (reduces for longer runs) */
  int X;                  /* Length of the current run of zeros */
  int bestLen = 0;        /* Length of the longest run of zeros */
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
  int skipEncryptBytes;   /* Number of bytes to skip during encryption */

  /* Find the longest run of zeros */
  while( p<pN ){
    if( *p==0 ){
      const char *pS = p;  /* save start */
      p++;
      while( p<pEnd && *p==0 ) p++;
      X = (int)(p - pS);
      if( X>bestLen ){
        bestLen = X;
        pBestStart = pS;
        pN = &aSrc[nSrc - bestLen]; /* reduce search space based on longest run */
      }
    }
    p++;
  }

  /* Do the compression */
  X = (int)(pBestStart-aSrc);          /* length of first data block */
  aDest[0] = (X>>8)&0xff;
  aDest[1] = X & 0xff;
  memcpy(&aDest[2], aSrc, X);    /* copy first data block */
  memcpy(&aDest[2+X], &pBestStart[bestLen], nSrc-X-bestLen);
  *pnDest = 2+nSrc-bestLen;
#ifdef NDS_ENABLE_AES
  if (pInst->pCryptoContex != NULL) {
    skipEncryptBytes = (eBsrMethod == BSR2_COMPRESSION) ? 2 : 0;
    pInst->pAlg->xEncrypt(pInst, aDest+skipEncryptBytes, aDest+skipEncryptBytes,
            (*pnDest)-skipEncryptBytes);
  }
#endif /* ifdef NDS_ENABLE_AES */

  return SQLITE_OK;
}

static int bsrUncompressImpl(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc,  int nSrc, enum bsr_method eBsrMethod) {
  int X;                     /* Initial number of bytes to copy */
  int szPage;                /* Size of a page */
  int nTail;                 /* Byte of content to write to tail of page */
  const unsigned char *a;    /* Input unsigned */
  int skipDecryptBytes;      /* Number of decrypted bytes to skip */
#ifdef NDS_ENABLE_AES
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
#else
  DISCARD_PARAMETER(pLocalCtx);
#endif /* ifdef NDS_ENABLE_AES */

  if (eBsrMethod == BSR2_COMPRESSION) {
    /* first two bytes have not been encrypted */
    a = (unsigned char*)aSrc;
    X = (a[0]<<8) + a[1];
    nSrc -= 2;
    aSrc += 2;
#ifdef NDS_ENABLE_AES
    aSrc = aesDecryptWrapper(pInst, aSrc, nSrc);
    if (aSrc == NULL)
      return SQLITE_NOMEM;
#endif /* ifdef NDS_ENABLE_AES */
    skipDecryptBytes = 0;
  } else {
#ifdef NDS_ENABLE_AES
   aSrc = aesDecryptWrapper(pInst, aSrc, nSrc);
   if (aSrc == NULL)
       return SQLITE_NOMEM;
#endif /* ifdef NDS_ENABLE_AES */

    a = (unsigned char*)aSrc;
    X = (a[0]<<8) + a[1];
    nSrc -= 2;
    skipDecryptBytes = 2;
  }
  nTail = nSrc - X;
  szPage = *pnDest;
  if (X > 0) {
    memcpy(aDest, &aSrc[skipDecryptBytes], X);
  }
  memset(&aDest[X], 0, szPage-nSrc);
  memcpy(&aDest[szPage-nTail], &aSrc[skipDecryptBytes+X], nTail);

  return SQLITE_OK;
}

static int bsrBound(void *pLocalCtx, int nByte) {
  DISCARD_PARAMETER(pLocalCtx);

  return bsrBoundImpl(nByte);
}

static int bsrCompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc,  int nSrc) {
  return bsrCompressImpl(pLocalCtx, aDest, pnDest, aSrc, nSrc, BSR_COMPRESSION);
}

static int bsrUncompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc,  int nSrc) {
  return bsrUncompressImpl(pLocalCtx, aDest, pnDest, aSrc, nSrc, BSR_COMPRESSION);
}

static int bsr2Bound(void *pLocalCtx, int nByte) {
  DISCARD_PARAMETER(pLocalCtx);

  return bsrBoundImpl(nByte);
}

static int bsr2Compress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc,  int nSrc) {
  return bsrCompressImpl(pLocalCtx, aDest, pnDest, aSrc, nSrc, BSR2_COMPRESSION);
}

static int bsr2Uncompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc,  int nSrc) {
  return bsrUncompressImpl(pLocalCtx, aDest, pnDest, aSrc, nSrc, BSR2_COMPRESSION);
}
/* End BSR compression routines
******************************************************************************/

/******************************************************************************
** No compression routines for use with ZIPVFS
**
** No compression is necessary for encrypted uncompressed databases.
*/
static int noneBound(void *pLocalCtx, int nByte) {
  DISCARD_PARAMETER(pLocalCtx);
  return nByte;
}

static int noneCompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc) {
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
  if (pInst->pCryptoContex != NULL)
    pInst->pAlg->xEncrypt(pInst, aDest, aSrc, nSrc);
  else
    memcpy(aDest, aSrc, nSrc);
  *pnDest = nSrc;

  return SQLITE_OK;
}

static int noneUncompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc) {
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
  if (pInst->pCryptoContex != NULL)
    pInst->pAlg->xDecrypt(pInst, aDest, aSrc, nSrc);
  else
    memcpy(aDest, aSrc, nSrc);
  *pnDest = nSrc;

  return SQLITE_OK;
}
/* End NONE compression routines
******************************************************************************/

#ifdef NDS_ENABLE_BROTLI

/*****************************************************************************
** BROTLI compression for ZipVFS.
**
** These routines implement compression using the external BROTLI library.
*/
static int brotliBound(void *pLocalCtx, int nByte) {
  DISCARD_PARAMETER(pLocalCtx);

  return (int)brotli_bound((size_t)nByte);
}

static int brotliCompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc) {
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
  size_t compressedSize = (size_t)*pnDest;
  int rc;

  int level = pInst->iLevel;
  if (level < BROTLI_MIN_COMPRESSION_LEVEL || level > BROTLI_MAX_COMPRESSION_LEVEL)
    level = BROTLI_DEFAULT_COMPRESSION_LEVEL;

  rc = brotli_compress((size_t)nSrc, (const unsigned char*)aSrc, &compressedSize,
          (unsigned char*)aDest, level);
  if (rc != BROTLI_OK)
      return SQLITE_ERROR;

#ifdef NDS_ENABLE_AES
  if (pInst->pCryptoContex != NULL) {
    pInst->pAlg->xEncrypt(pInst, aDest, aDest, (int)compressedSize);
  }
#endif /* ifdef NDS_ENABLE_AES */
  *pnDest = (int)compressedSize;

  return SQLITE_OK;
}

static int brotliUncompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc) {
  size_t decompressedSize = (size_t)*pnDest;
  int rc;

#ifdef NDS_ENABLE_AES
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
  aSrc = aesDecryptWrapper(pInst, aSrc, nSrc);
  if (aSrc == NULL)
    return SQLITE_NOMEM;
#else
  DISCARD_PARAMETER(pLocalCtx);
#endif /* ifdef NDS_ENABLE_AES */

  rc = brotli_decompress((size_t)nSrc, (const unsigned char*)aSrc, &decompressedSize,
          (unsigned char*)aDest);
  if (rc != BROTLI_OK)
      return SQLITE_ERROR;
  *pnDest = (int)decompressedSize;

  return SQLITE_OK;
}

/* End BROTLI compression
******************************************************************************/

#endif /* NDS_ENABLE_BROTLI */

#if defined NDS_ENABLE_ZSTD || defined NDS_ENABLE_ZSTD_DICT
  #define ZSTD_MIN_COMPRESSION_LEVEL     (-22)
#endif /* if defined NDS_ENABLE_ZSTD || defined NDS_ENABLE_ZSTD_DICT */

#ifdef NDS_ENABLE_ZSTD

/*****************************************************************************
** ZSTD compression for ZipVFS.
**
** These routines implement compression using the external ZSTD library.
*/
#ifdef NDS_ENABLE_ZSTD_PREALLOCATION
static int zstdComprSetup(ZipvfsInst* pInst, const char* zFile) {
  DISCARD_PARAMETER(zFile);
  pInst->pComprContext = ZSTD_createCCtx();

  return (pInst->pComprContext != NULL) ? SQLITE_OK : SQLITE_ERROR;
}
#endif /* ifdef NDS_ENABLE_ZSTD_PREALLOCATION */

static int zstdBound(void *pLocalCtx, int nByte) {
  DISCARD_PARAMETER(pLocalCtx);

  return (int)ZSTD_compressBound((size_t)nByte);
}

static int zstdCompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc) {
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
  size_t compressedSize;

  int level = pInst->iLevel;
  int maxLevel = ZSTD_maxCLevel();
  if (level < ZSTD_MIN_COMPRESSION_LEVEL || level > maxLevel)
    level = maxLevel / 2;

#ifdef NDS_ENABLE_ZSTD_PREALLOCATION
  compressedSize = ZSTD_compressCCtx(pInst->pComprContext, aDest, (size_t)*pnDest,
          aSrc, (size_t)nSrc, level);
#else
  compressedSize = ZSTD_compress(aDest, (size_t)*pnDest, aSrc, (size_t)nSrc,
		  level);
#endif /* ifdef NDS_ENABLE_ZSTD_PREALLOCATION */
  if (ZSTD_isError(compressedSize))
      return SQLITE_ERROR;
#ifdef NDS_ENABLE_AES
  if (pInst->pCryptoContex != NULL) {
    pInst->pAlg->xEncrypt(pInst, aDest, aDest, (int)compressedSize);
  }
#endif /* ifdef NDS_ENABLE_AES */
  *pnDest = (int)compressedSize;

  return SQLITE_OK;
}

#ifdef NDS_ENABLE_ZSTD_PREALLOCATION
static int zstdComprCleanup(ZipvfsInst* pInst) {
  ZSTD_freeCCtx(pInst->pComprContext);

  return SQLITE_OK;
}

static int zstdDecmprSetup(ZipvfsInst* pInst, const char* zFile) {
  DISCARD_PARAMETER(zFile);
  pInst->pDecmprContext = ZSTD_createDCtx();

  return (pInst->pDecmprContext != NULL) ? SQLITE_OK : SQLITE_ERROR;
}
#endif /* ifdef NDS_ENABLE_ZSTD_PREALLOCATION */

static int zstdUncompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc) {
  size_t decompressedSize;
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;

#ifdef NDS_ENABLE_AES
  aSrc = aesDecryptWrapper(pInst, aSrc, nSrc);
  if (aSrc == NULL)
    return SQLITE_NOMEM;
#endif /* ifdef NDS_ENABLE_AES */

#ifdef NDS_ENABLE_ZSTD_PREALLOCATION
  decompressedSize = ZSTD_decompressDCtx(pInst->pDecmprContext, aDest, (size_t)*pnDest,
          aSrc, (size_t)nSrc);
#else
  decompressedSize = ZSTD_decompress(aDest, (size_t)*pnDest, aSrc,
		  (size_t)nSrc);
#endif /* ifdef NDS_ENABLE_ZSTD_PREALLOCATION */
  if (ZSTD_isError(decompressedSize))
      return SQLITE_ERROR;
  *pnDest = (int)decompressedSize;

  return SQLITE_OK;
}

#ifdef NDS_ENABLE_ZSTD_PREALLOCATION
static int zstdDecmprCleanup(ZipvfsInst* pInst) {
  ZSTD_freeDCtx(pInst->pDecmprContext);

  return SQLITE_OK;
}
#endif /* ifdef NDS_ENABLE_ZSTD_PREALLOCATION */

/* End ZSTD compression
******************************************************************************/

#endif /* ifdef NDS_ENABLE_ZSTD */

#ifdef NDS_ENABLE_ZSTD_DICT

typedef struct ZstdDictionary ZstdDictionary;
typedef struct ZstdDictComprContext ZstdDictComprContext;
typedef struct ZstdDictDecmprContext ZstdDictDecmprContext;

struct ZstdDictionary {
  int   iId;
  void  *pDict;
};

struct ZstdDictComprContext {
  void            *pCCtx;
  ZstdDictionary  Dict;
};

#define DEFAULT_MAX_NUM_DICTS   16

struct ZstdDictDecmprContext {
  void            *pDCtx;
  int             iMaxNumDicts;
  int             iNumDicts;
  /* here follows array with iNumDicts elements with reserved space for
   * iMaxNumDicts elements */
  ZstdDictionary  Dicts[0];
};

/*****************************************************************************
** ZSTD compression using external dictionary for ZipVFS.
**
** These routines implement compression using the external ZSTD library.
*/
static int zstdDictComprSetup(ZipvfsInst* pInst, const char* zFile) {
  ZstdDictComprContext *context;
  DISCARD_PARAMETER(zFile);

  /* alloc memory for context */
  context = sqlite3_malloc(sizeof(ZstdDictComprContext));
  if (context == NULL)
      return SQLITE_NOMEM;

  /* store context */
  context->Dict.iId = -1;
  context->Dict.pDict = NULL;
  pInst->pComprContext = context;

  /* create compression context */
  context->pCCtx = ZSTD_createCCtx();
  if (context->pCCtx == NULL)
    return SQLITE_ERROR;

  return SQLITE_OK;
}

static int zstdDictBound(void *pLocalCtx, int nByte) {
  DISCARD_PARAMETER(pLocalCtx);

  return (int)sizeof(int) + (int)ZSTD_compressBound((size_t)nByte);
}

static size_t encodeDictId(int dictId, unsigned char* dest) {
  if (dictId < 0x80) {
    *dest = (unsigned char)dictId | 0x80;
    return sizeof(unsigned char);
  }

  *dest = (unsigned char)(dictId >> 24);
  *(dest + 1) = (unsigned char)(dictId >> 16);
  *(dest + 2) = (unsigned char)(dictId >> 8);
  *(dest + 3) = (unsigned char)dictId;

  return sizeof(int);
}

static size_t decodeDictId(const unsigned char* src, int* dictId) {
  const unsigned char firstByte = *src;
  if ( (firstByte & 0x80) != 0 ) {
    *dictId = (int)(firstByte & ~0x80);
    return sizeof(unsigned char);
  }

  *dictId = (int)firstByte;
  *dictId = (*dictId << 8) | ((int)*(src + 1));
  *dictId = (*dictId << 8) | ((int)*(src + 2));
  *dictId = (*dictId << 8) | ((int)*(src + 3));

  return sizeof(int);
}

static int zstdDictCompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc) {
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
  ZstdDictComprContext* context = (ZstdDictComprContext*)pInst->pComprContext;
  int sqliteResult;
  int dictId;
  int dictSize;
  unsigned char *dict;
  int level;
  int maxLevel;
  size_t compressedSize;
  size_t encodedDictIdSize;
  int totalSize;

  /* check if we have dictionary for compression */
  if (context->Dict.pDict == NULL) {
    /* read dict with the largest id value from the dictionary store */
    sqliteResult = zipvfs_dictstore_get(pInst->pMethods, -1, &dictId, &dictSize,
        &dict);
    if (sqliteResult != SQLITE_OK)
      return sqliteResult;
    if (dictId < 0)
      return SQLITE_ERROR;

    /* create dictionary compression context */
    level = pInst->iLevel;
    maxLevel = ZSTD_maxCLevel();
    if (level < ZSTD_MIN_COMPRESSION_LEVEL || level > maxLevel)
      level = maxLevel / 2; // default compression level
    context->Dict.pDict = ZSTD_createCDict(dict, dictSize, level);
    sqlite3_free(dict);
    if (context->Dict.pDict == NULL)
      return SQLITE_ERROR;
    context->Dict.iId = dictId;
  }

  /* encode dictionary id */
  encodedDictIdSize = encodeDictId(context->Dict.iId, (unsigned char*)aDest);
  if ((size_t)*pnDest <= encodedDictIdSize)
    return SQLITE_ERROR;

  /* compress page data */
  compressedSize = ZSTD_compress_usingCDict(context->pCCtx,
      aDest + encodedDictIdSize, (size_t)*pnDest - encodedDictIdSize, aSrc,
      (size_t)nSrc, context->Dict.pDict);
  if (ZSTD_isError(compressedSize))
      return SQLITE_ERROR;

  /* if requested encrypt compressed page data */
  totalSize = (int)(encodedDictIdSize + compressedSize);
#ifdef NDS_ENABLE_AES
  if (pInst->pCryptoContex != NULL) {
    pInst->pAlg->xEncrypt(pInst, aDest, aDest, totalSize);
  }
#endif /* ifdef NDS_ENABLE_AES */
  *pnDest = totalSize;

  return SQLITE_OK;
}

static int zstdDictComprCleanup(ZipvfsInst* pInst) {
  ZstdDictComprContext* context = (ZstdDictComprContext*)pInst->pComprContext;

  if (context != NULL) {
    if (context->pCCtx != NULL)
      ZSTD_freeCCtx(context->pCCtx);
    if (context->Dict.pDict != NULL)
      ZSTD_freeCDict(context->Dict.pDict);
    sqlite3_free(context);
  }

  return SQLITE_OK;
}

static int zstdDictDecmprSetup(ZipvfsInst* pInst, const char* zFile) {
  ZstdDictDecmprContext *context;
  DISCARD_PARAMETER(zFile);

  /* alloc memory for context */
  context = sqlite3_malloc(sizeof(ZstdDictDecmprContext) +
      sizeof(ZstdDictionary) * DEFAULT_MAX_NUM_DICTS);
  if (context == NULL)
      return SQLITE_NOMEM;

  /* store context */
  context->iMaxNumDicts = DEFAULT_MAX_NUM_DICTS;
  context->iNumDicts = 0;
  pInst->pDecmprContext = context;

  /* create decompression context */
  context->pDCtx = ZSTD_createDCtx();
  if (context->pDCtx == NULL)
    return SQLITE_ERROR;

  return SQLITE_OK;
}

static void* getDictionaryContext(int dictId, ZipvfsInst *pInst)
{
  ZstdDictDecmprContext* context = (ZstdDictDecmprContext*)pInst->pDecmprContext;
  ZstdDictionary *zstdDict = context->Dicts;
  const ZstdDictionary *endZstdDict = zstdDict + context->iNumDicts;
  int sqliteResult;
  int dictSize;
  unsigned char *dict;
  void* dictContext;

  /* check if given dictionary id is already known */
  for (; zstdDict != endZstdDict; ++zstdDict)
    if (zstdDict->iId == dictId)
      return zstdDict->pDict;

  /* if necessary realloc context structure */
  if (context->iNumDicts == context->iMaxNumDicts) {
    context->iMaxNumDicts = context->iMaxNumDicts * 2;
    context = sqlite3_realloc(context, sizeof(ZstdDictDecmprContext) +
        sizeof(ZstdDictionary) * context->iMaxNumDicts);
    if (context == NULL)
      return NULL;
    pInst->pDecmprContext = context;
  }

  /* read new dictionary */
  sqliteResult = zipvfs_dictstore_get(pInst->pMethods, dictId, &dictId, &dictSize,
      &dict);
  if (sqliteResult != SQLITE_OK)
    return NULL;

  /* create new dictionary context */
  dictContext = ZSTD_createDDict(dict, dictSize);
  sqlite3_free(dict);
  if (dictContext == NULL)
    return NULL;

  /* store just created new dictionary context */
  zstdDict = context->Dicts + context->iNumDicts;
  zstdDict->iId = dictId;
  zstdDict->pDict = dictContext;
  context->iNumDicts = context->iNumDicts + 1;

  return dictContext;
}

static int zstdDictUncompress(void *pLocalCtx, char *aDest, int *pnDest,
        const char *aSrc, int nSrc) {
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
  ZstdDictDecmprContext* context;
  int dictId;
  size_t encodedDictIdSize;
  size_t decompressedSize;
  void* dictContext;

  /* if requested decrypt page data */
#ifdef NDS_ENABLE_AES
  aSrc = aesDecryptWrapper(pInst, aSrc, nSrc);
  if (aSrc == NULL)
    return SQLITE_NOMEM;
#endif /* ifdef NDS_ENABLE_AES */

  /* decode dictionary id */
  encodedDictIdSize = decodeDictId((const unsigned char*)aSrc, &dictId);
  if ((size_t)nSrc <= encodedDictIdSize)
    return SQLITE_ERROR;

  /* get dictionary context (can reallocate context!!!) */
  dictContext = getDictionaryContext(dictId, pInst);
  if (dictContext == NULL)
    return SQLITE_ERROR;

  /* decompress page data */
  context = (ZstdDictDecmprContext*)pInst->pDecmprContext;
  decompressedSize = ZSTD_decompress_usingDDict(context->pDCtx, aDest,
      (size_t)*pnDest, aSrc + encodedDictIdSize, (size_t)nSrc -
      encodedDictIdSize, dictContext);
  if (ZSTD_isError(decompressedSize))
      return SQLITE_ERROR;
  *pnDest = (int)decompressedSize;

  return SQLITE_OK;
}

static int zstdDictDecmprCleanup(ZipvfsInst* pInst) {
  ZstdDictDecmprContext* context = (ZstdDictDecmprContext*)pInst->pDecmprContext;
  if (context != NULL) {
    ZstdDictionary *zstdDict = context->Dicts;
    const ZstdDictionary *endZstdDict = zstdDict + context->iNumDicts;

    if (context->pDCtx != NULL)
      ZSTD_freeDCtx(context->pDCtx);

    for (; zstdDict != endZstdDict; ++zstdDict)
      ZSTD_freeDDict(zstdDict->pDict);

    sqlite3_free(context);
  }

  return SQLITE_OK;
}

/* End ZSTD_DICT compression
******************************************************************************/

#endif /* NDS_ENABLE_ZSTD_DICT */

/*
** The following is the array of available compression and encryption
** algorithms.  To add new compression or encryption algorithms, make
** new entries to this array.
*/
static const ZipvfsAlgorithm aZipvfs[] = {

#ifdef NDS_ENABLE_ZLIB
  /* ZLib */ {
  /* zName          */  "zlib",
  /* xBound         */  zlibBound,
  /* xComprSetup    */  NULL,
  /* xCompr         */  zlibCompress,
  /* xComprCleanup  */  NULL,
  /* xDecmprSetup   */  NULL,
  /* xDecmpr        */  zlibUncompress,
  /* xDecmprCleanup */  NULL,
  #ifdef NDS_ENABLE_AES
    /* xCryptoSetup   */  aesEncryptionSetup,
    /* xEncrypt       */  aesEncryption,
    /* xDecrypt       */  aesDecryption,
    /* xCryptoCleanup */  aesEncryptionCleanup
  #else
    /* xCryptoSetup   */  NULL,
    /* xEncrypt       */  NULL,
    /* xDecrypt       */  NULL,
    /* xCryptoCleanup */  NULL
  #endif /* ifdef NDS_ENABLE_AES */
  },
#endif /* NDS_ENABLE_ZLIB */

#ifdef NDS_ENABLE_LZ4
  /* LZ4 */ {
  /* zName          */  "lz4",
  /* xBound         */  lz4Bound,
  #ifdef NDS_ENABLE_LZ4_PREALLOCATION
    /* xComprSetup    */  lz4ComprSetup,
  #else
    /* xComprSetup    */  NULL,
  #endif /* ifdef NDS_ENABLE_LZ4_PREALLOCATION */
  /* xCompr         */  lz4Compress,
  #ifdef NDS_ENABLE_LZ4_PREALLOCATION
    /* xComprCleanup  */  lz4ComprCleanup,
  #else
    /* xComprCleanup  */  NULL,
  #endif /* ifdef NDS_ENABLE_LZ4_PREALLOCATION */
  /* xDecmprSetup   */  NULL,
  /* xDecmpr        */  lz4Uncompress,
  /* xDecmprCleanup */  NULL,
  #ifdef NDS_ENABLE_AES
    /* xCryptoSetup   */  aesEncryptionSetup,
    /* xEncrypt       */  aesEncryption,
    /* xDecrypt       */  aesDecryption,
    /* xCryptoCleanup */  aesEncryptionCleanup
  #else
    /* xCryptoSetup   */  NULL,
    /* xEncrypt       */  NULL,
    /* xDecrypt       */  NULL,
    /* xCryptoCleanup */  NULL
  #endif /* ifdef NDS_ENABLE_AES */
  },

  /* LZ4HC */ {
  /* zName          */  "lz4hc",
  /* xBound         */  lz4Bound,
  #ifdef NDS_ENABLE_LZ4_PREALLOCATION
    /* xComprSetup    */  lz4hcComprSetup,
  #else
    /* xComprSetup    */  NULL,
  #endif /* ifdef NDS_ENABLE_LZ4_PREALLOCATION */
  /* xCompr         */  lz4hcCompress,
  #ifdef NDS_ENABLE_LZ4_PREALLOCATION
    /* xComprCleanup  */  lz4ComprCleanup,
  #else
    /* xComprCleanup  */  NULL,
  #endif /* ifdef NDS_ENABLE_LZ4_PREALLOCATION */
  /* xDecmprSetup   */  NULL,
  /* xDecmpr        */  lz4Uncompress,
  /* xDecmprCleanup */  NULL,
  #ifdef NDS_ENABLE_AES
    /* xCryptoSetup   */  aesEncryptionSetup,
    /* xEncrypt       */  aesEncryption,
    /* xDecrypt       */  aesDecryption,
    /* xCryptoCleanup */  aesEncryptionCleanup
  #else
    /* xCryptoSetup   */  NULL,
    /* xEncrypt       */  NULL,
    /* xDecrypt       */  NULL,
    /* xCryptoCleanup */  NULL
  #endif /* ifdef NDS_ENABLE_AES */
  },
#endif /* NDS_ENABLE_LZ4 */

#ifdef NDS_ENABLE_NDSC
  /* NDSC */ {
  /* zName          */  "ndsc",
  /* xBound         */  ndscBound,
  /* xComprSetup    */  NULL,
  /* xCompr         */  ndscCompress,
  /* xComprCleanup  */  NULL,
  /* xDecmprSetup   */  NULL,
  /* xDecmpr        */  ndscUncompress,
  /* xDecmprCleanup */  NULL,
  #ifdef NDS_ENABLE_AES
    /* xCryptoSetup   */  aesEncryptionSetup,
    /* xEncrypt       */  aesEncryption,
    /* xDecrypt       */  aesDecryption,
    /* xCryptoCleanup */  aesEncryptionCleanup
  #else
    /* xCryptoSetup   */  NULL,
    /* xEncrypt       */  NULL,
    /* xDecrypt       */  NULL,
    /* xCryptoCleanup */  NULL
  #endif /* ifdef NDS_ENABLE_AES */
  },

  /* NDSC with an alternative name */ {
  /* zName          */  "ndsc-mux",
  /* xBound         */  ndscBound,
  /* xComprSetup    */  NULL,
  /* xCompr         */  ndscCompress,
  /* xComprCleanup  */  NULL,
  /* xDecmprSetup   */  NULL,
  /* xDecmpr        */  ndscUncompress,
  /* xDecmprCleanup */  NULL,
  #ifdef NDS_ENABLE_AES
    /* xCryptoSetup   */  aesEncryptionSetup,
    /* xEncrypt       */  aesEncryption,
    /* xDecrypt       */  aesDecryption,
    /* xCryptoCleanup */  aesEncryptionCleanup
  #else
    /* xCryptoSetup   */  NULL,
    /* xEncrypt       */  NULL,
    /* xDecrypt       */  NULL,
    /* xCryptoCleanup */  NULL
  #endif /* ifdef NDS_ENABLE_AES */
  },
#endif /* NDS_ENABLE_NDSC */

  /* Blank-space removal */ {
  /* zName          */  "bsr",
  /* xBound         */  bsrBound,
  /* xComprSetup    */  NULL,
  /* xCompr         */  bsrCompress,
  /* xComprCleanup  */  NULL,
  /* xDecmprSetup   */  NULL,
  /* xDecmpr        */  bsrUncompress,
  /* xDecmprCleanup */  NULL,
  #ifdef NDS_ENABLE_AES
    /* xCryptoSetup   */  aesEncryptionSetup,
    /* xEncrypt       */  aesEncryption,
    /* xDecrypt       */  aesDecryption,
    /* xCryptoCleanup */  aesEncryptionCleanup
  #else
    /* xCryptoSetup   */  NULL,
    /* xEncrypt       */  NULL,
    /* xDecrypt       */  NULL,
    /* xCryptoCleanup */  NULL
  #endif /* ifdef NDS_ENABLE_AES */
  },

  /* Blank-space removal with safer decryption */ {
  /* zName          */  "bsr2",
  /* xBound         */  bsr2Bound,
  /* xComprSetup    */  NULL,
  /* xCompr         */  bsr2Compress,
  /* xComprCleanup  */  NULL,
  /* xDecmprSetup   */  NULL,
  /* xDecmpr        */  bsr2Uncompress,
  /* xDecmprCleanup */  NULL,
  #ifdef NDS_ENABLE_AES
    /* xCryptoSetup   */  aesEncryptionSetup,
    /* xEncrypt       */  aesEncryption,
    /* xDecrypt       */  aesDecryption,
    /* xCryptoCleanup */  aesEncryptionCleanup
  #else
    /* xCryptoSetup   */  NULL,
    /* xEncrypt       */  NULL,
    /* xDecrypt       */  NULL,
    /* xCryptoCleanup */  NULL
  #endif /* ifdef NDS_ENABLE_AES */
  },

  /* None compression (only encryption) */ {
  /* zName          */  "none",
  /* xBound         */  noneBound,
  /* xComprSetup    */  NULL,
  /* xCompr         */  noneCompress,
  /* xComprCleanup  */  NULL,
  /* xDecmprSetup   */  NULL,
  /* xDecmpr        */  noneUncompress,
  /* xDecmprCleanup */  NULL,
  #ifdef NDS_ENABLE_AES
    /* xCryptoSetup   */  aesEncryptionSetup,
    /* xEncrypt       */  aesEncryption,
    /* xDecrypt       */  aesDecryption,
    /* xCryptoCleanup */  aesEncryptionCleanup
  #else
    /* xCryptoSetup   */  NULL,
    /* xEncrypt       */  NULL,
    /* xDecrypt       */  NULL,
    /* xCryptoCleanup */  NULL
  #endif /* ifdef NDS_ENABLE_AES */
  },

#ifdef NDS_ENABLE_BROTLI
  /* Brotli */ {
  /* zName          */  "brotli",
  /* xBound         */  brotliBound,
  /* xComprSetup    */  NULL,
  /* xCompr         */  brotliCompress,
  /* xComprCleanup  */  NULL,
  /* xDecmprSetup   */  NULL,
  /* xDecmpr        */  brotliUncompress,
  /* xDecmprCleanup */  NULL,
  #ifdef NDS_ENABLE_AES
    /* xCryptoSetup   */  aesEncryptionSetup,
    /* xEncrypt       */  aesEncryption,
    /* xDecrypt       */  aesDecryption,
    /* xCryptoCleanup */  aesEncryptionCleanup
  #else
    /* xCryptoSetup   */  NULL,
    /* xEncrypt       */  NULL,
    /* xDecrypt       */  NULL,
    /* xCryptoCleanup */  NULL
  #endif /* ifdef NDS_ENABLE_AES */
  },
#endif /* NDS_ENABLE_BROTLI */

#ifdef NDS_ENABLE_ZSTD
  /* ZSTD */ {
  /* zName          */  "zstd",
  /* xBound         */  zstdBound,
  #ifdef NDS_ENABLE_ZSTD_PREALLOCATION
    /* xComprSetup    */  zstdComprSetup,
  #else
    /* xComprSetup    */  NULL,
  #endif /* ifdef NDS_ENABLE_ZSTD_PREALLOCATION */
  /* xCompr         */  zstdCompress,
  #ifdef NDS_ENABLE_ZSTD_PREALLOCATION
    /* xComprCleanup  */  zstdComprCleanup,
    /* xDecmprSetup   */  zstdDecmprSetup,
  #else
    /* xComprCleanup  */  NULL,
    /* xDecmprSetup   */  NULL,
  #endif /* ifdef NDS_ENABLE_ZSTD_PREALLOCATION */
  /* xDecmpr        */  zstdUncompress,
  #ifdef NDS_ENABLE_ZSTD_PREALLOCATION
	/* xDecmprCleanup */  zstdDecmprCleanup,
  #else
    /* xDecmprCleanup */  NULL,
  #endif /* ifdef NDS_ENABLE_ZSTD_PREALLOCATION */
  #ifdef NDS_ENABLE_AES
    /* xCryptoSetup   */  aesEncryptionSetup,
    /* xEncrypt       */  aesEncryption,
    /* xDecrypt       */  aesDecryption,
    /* xCryptoCleanup */  aesEncryptionCleanup
  #else
    /* xCryptoSetup   */  NULL,
    /* xEncrypt       */  NULL,
    /* xDecrypt       */  NULL,
    /* xCryptoCleanup */  NULL
  #endif /* ifdef NDS_ENABLE_AES */
  },
#endif /* NDS_ENABLE_ZSTD */

#ifdef NDS_ENABLE_ZSTD_DICT
  /* ZSTD with dictionary */ {
  /* zName          */  "zstd_d",
  /* xBound         */  zstdDictBound,
  /* xComprSetup    */  zstdDictComprSetup,
  /* xCompr         */  zstdDictCompress,
  /* xComprCleanup  */  zstdDictComprCleanup,
  /* xDecmprSetup   */  zstdDictDecmprSetup,
  /* xDecmpr        */  zstdDictUncompress,
  /* xDecmprCleanup */  zstdDictDecmprCleanup,
  #ifdef NDS_ENABLE_AES
    /* xCryptoSetup   */  aesEncryptionSetup,
    /* xEncrypt       */  aesEncryption,
    /* xDecrypt       */  aesDecryption,
    /* xCryptoCleanup */  aesEncryptionCleanup
  #else
    /* xCryptoSetup   */  NULL,
    /* xEncrypt       */  NULL,
    /* xDecrypt       */  NULL,
    /* xCryptoCleanup */  NULL
  #endif /* ifdef NDS_ENABLE_AES */
  }
#endif /* NDS_ENABLE_ZSTD_DICT */
};

/*
** This routine is called when a ZIPVFS database connection is shutting
** down.  Invoke all of the cleanup procedures in the ZipvfsAlgorithm
** object.
*/
static int nds_compression_algorithm_close(void *pLocalCtx) {
  ZipvfsInst *pInst = (ZipvfsInst*)pLocalCtx;
  const ZipvfsAlgorithm *pAlg = pInst->pAlg;
  if (pAlg->xComprCleanup) {
    (void)pAlg->xComprCleanup(pInst);
  }
  if (pAlg->xDecmprCleanup) {
    (void)pAlg->xDecmprCleanup(pInst);
  }
  if (pAlg->xCryptoCleanup) {
    (void)pAlg->xCryptoCleanup(pInst);
  }
  sqlite3_free(pInst);
  return SQLITE_OK;
}

/*
** Check the header zHeader to see if it contains a compression level
** following the zero terminator.  If it does, write the compression level
** value into *pHdrLevel.  If it does not, write -1 into *pHdrLevel.
*/
static void nds_extract_hdr_arg(const char *zHeader, int *pHdrLevel) {
  int i, n, d = -1;
  *pHdrLevel = -1;
  n = (int)strlen(zHeader);
  if( n>=12 ) return;
  if( zHeader[n+1]==0 ) return;
  d = 0;
  for(i=n+1; i<13 && zHeader[i]; i++){
    int c = zHeader[i] - '0';
    if( c<0 || c>9 ) return;
    d = d*10 + c;
  }
  *pHdrLevel = d;
}

/*
** The following routine is the crux of this whole module.
**
** This is the xAutoDetect callback for ZipVFS.  The job of this function
** is to figure out which compression algorithm to use for a database
** and fill out the pMethods object with pointers to the appropriate
** compression and decompression routines.
**
** ZIPVFS calls this routine exactly once for each database file that is
** opened.  The zHeader parameter is a copy of the text found in bytes
** 3 through 16 of the database header.  Or, if a new database is being
** created, zHeader is a NULL pointer.  zFile is the name of the database
** file.  If the database was opened as URI, then sqlite3_uri_parameter()
** can be used with the zFile parameter to extract query parameters from
** the URI.
*/
int nds_compression_algorithm_detector(
  void *pCtx,              /* Copy of pCtx from zipvfs_create_vfs_v3() */
  const char *zFile,       /* Name of file being opened */
  const char *zHeader,     /* Algorithm name in the database header */
  ZipvfsMethods *pMethods  /* OUT: Write new pCtx and function pointers here */
) {
  int noHdr = zHeader==0;
  DISCARD_PARAMETER(pCtx);

  /* If zHeader==0 that means we have a new database file.
  ** Look to the zv query parameter (if there is one) as a
  ** substitute for the database header.
  */
  if( noHdr ){
    const char *zZv = sqlite3_uri_parameter(zFile, "zv");
    if( zZv ) zHeader = zZv;
  }

  /* Look for a compression algorithm that matches zHeader.
  ** If special dummy "none" compression is requested without
  ** encryption then no compression is needed.
  */
  if( zHeader &&
      (strcmp(zHeader, "none")!=0 || aesGetEncryptionPassword(zFile)!=0) ){
    int i;
    unsigned int n;
    for(i=0; i<(int)(sizeof(aZipvfs)/sizeof(aZipvfs[0])); i++){
      if( strcmp(aZipvfs[i].zName, zHeader)==0 ){
        ZipvfsInst *pInst = sqlite3_malloc( sizeof(*pInst) );
        int rc = SQLITE_OK;
        if( pInst==0 ) return SQLITE_NOMEM;

        memset(pInst, 0, sizeof(*pInst));
        pInst->pAlg = &aZipvfs[i];
        pInst->pMethods = pMethods;
        pInst->iLevel = (int)sqlite3_uri_int64(zFile, "level", INT_MIN);
        if( pInst->iLevel<0 && !noHdr ){
          nds_extract_hdr_arg(zHeader, &pInst->iLevel);
        }
        sqlite3_snprintf(sizeof(pInst->zHdr), pInst->zHdr,
                         "%s", aZipvfs[i].zName);
        if( pInst->iLevel>=0
         && (n=(unsigned int)strlen(pInst->zHdr))<sizeof(pInst->zHdr)-2
        ){
          sqlite3_snprintf(sizeof(pInst->zHdr)-n-1, pInst->zHdr+n+1,
                           "%d", pInst->iLevel);
          pMethods->zAuxHdr = pInst->zHdr+n+1;
        }
        pInst->pComprContext = NULL;
        pInst->pDecmprContext = NULL;
        pInst->pCryptoContex = NULL;
        pMethods->zHdr = pInst->zHdr;
        pMethods->xCompressBound = aZipvfs[i].xBound;
        pMethods->xCompress = aZipvfs[i].xCompr;
        pMethods->xUncompress = aZipvfs[i].xDecmpr;
        pMethods->xCompressClose = nds_compression_algorithm_close;
        pMethods->pCtx = pInst;
        if( aZipvfs[i].xCryptoSetup ){
          rc = aZipvfs[i].xCryptoSetup(pInst, zFile);
        }
        if( rc==SQLITE_OK && aZipvfs[i].xComprSetup ){
          rc = aZipvfs[i].xComprSetup(pInst, zFile);
        }
        if( rc==SQLITE_OK && aZipvfs[i].xDecmprSetup ){
          rc = aZipvfs[i].xDecmprSetup(pInst, zFile);
        }
        if( rc!=SQLITE_OK ){
          nds_compression_algorithm_close(pInst);
          memset(pMethods, 0, sizeof(*pMethods));
        }
        return rc;
      }
    }
    /* Unknown compression algorithm. */
    memset(pMethods, 0, sizeof(*pMethods));
    return SQLITE_ERROR;
  }

  /* If no compression algorithm is found whose name matches zHeader,
  ** then assume no compression is desired.  Clearing the pMethods object
  ** causes ZIPVFS to be a no-op so that it reads and writes ordinary
  ** uncompressed SQLite database files.
  */
  memset(pMethods, 0, sizeof(*pMethods));
  return SQLITE_OK;
}
