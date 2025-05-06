/*----------------Copyright(C) 2008 Shenyang Familiar software Co.,Ltd. ALL RIGHTS RESERVED-------------------------*/

/*********************************************************************************************************************
*    FILE NAME      : compress.cpp                                                                                   *
*    CREATE DATE    : 2008-11-24                                                                                     *
*    MODULE         : compress                                                                                       *
*    AUTHOR         : mansiontech                                                                                    *
*--------------------------------------------------------------------------------------------------------------------*
*    MEMO           : if you use tab to indent, you will be fined!                                                   *
*********************************************************************************************************************/

/*********************************************************************************************************************
*                                    Include File Section                                                            *
*********************************************************************************************************************/
#include <stdio.h>
#include <memory.h>
#include "UPF_Malloc.h"
#include "compress.h"
#include "zlib.h"


/*********************************************************************************************************************
*                                    Macro Definition Section                                                        *
*********************************************************************************************************************/

/*********************************************************************************************************************
*                                    Type Definition Section                                                         *
*********************************************************************************************************************/

/*********************************************************************************************************************
*                                    Global Variables Definition Section                                             *
*********************************************************************************************************************/

/*********************************************************************************************************************
*                                    Functions SourceCode Implement Section                                          *
*********************************************************************************************************************/
extern "C" {


typedef struct _UTILS_INFLATE_CTX
{
    void           *upf_Allocator;
    int             sliceSize;
    unsigned char  *in;
    unsigned char  *out;
    z_stream        strm;
}UTILS_INFLATE_CTX;

static voidpf c_upf_alloc(voidpf opaque, uInt items, uInt size, void* upf_Allocator);
static void c_upf_free(voidpf opaque, voidpf address, void* upf_Allocator);

///////////////////////////////////////////////////////////////////////////////////
int utils_c_compress( void* upf_Allocator, void* dst,unsigned long *dlen,const void* src,unsigned long slen)
{
    return compress((Bytef*)dst,dlen,(const Bytef*)src,slen); 
}

unsigned long utils_c_compressBound(unsigned long sourceLen)
{
    return compressBound(sourceLen);
}

int utils_c_uncompress( void* upf_Allocator, void* dst,unsigned long *dlen,const void* src,unsigned long slen)
{
    z_stream stream;
    int err;

    stream.next_in = (Bytef*)src;
    stream.avail_in = (uInt)slen;
    /* Check for source > 64K on 16-bit machine: */
    if ((uLong)stream.avail_in != slen) return Z_BUF_ERROR;

    stream.next_out = (Bytef*)dst;
    stream.avail_out = (uInt)*dlen;
    stream.upf_Allocator = upf_Allocator;
    if ((uLong)stream.avail_out != *dlen) return Z_BUF_ERROR;

    stream.zalloc = (alloc_func)c_upf_alloc;
    stream.zfree = (free_func)c_upf_free;

    err = inflateInit(&stream);
    if (err != Z_OK) return err;

    err = inflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        inflateEnd(&stream);
        if (err == Z_NEED_DICT || (err == Z_BUF_ERROR && stream.avail_in == 0))
            return Z_DATA_ERROR;
        return err;
    }
    *dlen = stream.total_out;

    err = inflateEnd(&stream);
    return err;
    //return uncompress((Bytef*)dst,dlen,(const Bytef*)src,slen); 
}


/* Decompress from file source to file dest until stream ends or EOF.
   utils_c_inflateFile() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
#define BLOCK_SIZE 1*1024

int utils_c_inflateFile(void* upf_Allocator, FILE *source, FILE *dest)
{
    void *handle = utils_c_createInflateHandle(upf_Allocator, BLOCK_SIZE);
    if (NULL == handle)
    {
        return Z_ERRNO;
    }

    int hasMore = 1;
    while (hasMore)
    {
        int ret = utils_c_inflateFileSlice(handle, source, dest, &hasMore);
        if (ret != Z_OK)
        {
            utils_c_destroyInflateHandle(handle);
            return ret;
        }
    }

    utils_c_destroyInflateHandle(handle);
    return Z_OK;
}

void *utils_c_createInflateHandle(void* upf_Allocator, int sliceSize)
{
    UTILS_INFLATE_CTX *ctx = (UTILS_INFLATE_CTX *)UPF_Malloc( (UPF_Allocator_Handle)upf_Allocator, sizeof(UTILS_INFLATE_CTX));
    if (ctx)
    {
        memset(ctx, 0, sizeof(UTILS_INFLATE_CTX));
        ctx->sliceSize = sliceSize;
        ctx->upf_Allocator = upf_Allocator;
        ctx->in = (unsigned char *)UPF_Malloc( (UPF_Allocator_Handle)upf_Allocator, ctx->sliceSize * sizeof(unsigned char ));
        if (ctx->in)
        {
            ctx->out = (unsigned char *)UPF_Malloc( (UPF_Allocator_Handle)upf_Allocator, ctx->sliceSize * sizeof(unsigned char ));
            if (ctx->out)
            {
                ctx->strm.zalloc = (alloc_func)c_upf_alloc;
                ctx->strm.zfree = (free_func)c_upf_free;
                ctx->strm.opaque = Z_NULL;
                ctx->strm.avail_in = 0;
                ctx->strm.next_in = Z_NULL;
                ctx->strm.upf_Allocator = upf_Allocator;
                int ret = inflateInit(&(ctx->strm));
                if (ret == Z_OK)
                {
                    return  (void *)ctx;
                }
            }
        }
    }

    utils_c_destroyInflateHandle(ctx);
    return NULL;
}

int utils_c_inflateFileSlice(void *handle, FILE *source, FILE *dest, int *hasMore)
{
	if (NULL == handle)
	{return Z_ERRNO;}
    int ret;
    unsigned have;

    *hasMore = 0;

    UTILS_INFLATE_CTX *ctx = (UTILS_INFLATE_CTX *)handle;
  
    ctx->strm.avail_in = fread(ctx->in, 1, ctx->sliceSize, source);
    if (ferror(source)) {
        return Z_ERRNO;
    }
    if (ctx->strm.avail_in == 0)
    {
        return Z_ERRNO;
    }
    ctx->strm.next_in = ctx->in;

    /* run inflate() on input until output buffer not full */
    do {
        ctx->strm.avail_out = ctx->sliceSize;
        ctx->strm.next_out = ctx->out;
        ret = inflate(&ctx->strm, Z_NO_FLUSH);
        switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR;     /* and fall through */
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            return ret;
        }
        have = ctx->sliceSize - ctx->strm.avail_out;
        if (fwrite(ctx->out, 1, have, dest) != have || ferror(dest)) {
            return Z_ERRNO;
        }
    } while (ctx->strm.avail_out == 0);

    if (ret != Z_STREAM_END)
    {
        *hasMore = 1;
    }

    return Z_OK;
}

void utils_c_destroyInflateHandle(void *handle)
{
    UTILS_INFLATE_CTX *ctx = (UTILS_INFLATE_CTX *)handle;
    if (ctx)
    {
        (void)inflateEnd(&(ctx->strm));
        if (ctx->in)
        {
            UPF_Free(ctx->upf_Allocator, ctx->in);
        }
        if (ctx->out)
        {
            UPF_Free(ctx->upf_Allocator, ctx->out);
        }

        UPF_Free(ctx->upf_Allocator, ctx);
    }
}

//////////////////////////////////////////////////////////////////////
static voidpf c_upf_alloc(voidpf opaque, uInt items, uInt size, void* upf_Allocator)
{
    return (voidpf)UPF_Malloc( (UPF_Allocator_Handle)upf_Allocator, items * size );
}

static void c_upf_free(voidpf opaque, voidpf address, void* upf_Allocator)
{
    UPF_Free( (UPF_Allocator_Handle) upf_Allocator, address );
}

} // C linkage

