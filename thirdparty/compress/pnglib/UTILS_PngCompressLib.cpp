/*----------------Copyright(C) 2007 Shenyang Familiar software Co.,Ltd. ALL RIGHTS RESERVED-------------------------*/

/*********************************************************************************************************************
*	FILE NAME	: UTILS_PngCompressLib.cpp																             *
*	CREATE DATE	: 2009-12-01																						 *
*	MODULE		: UTILS																								 *																			 *
*--------------------------------------------------------------------------------------------------------------------*
*	MEMO		:																									 *
*********************************************************************************************************************/

/*********************************************************************************************************************
*									Include File Section															 *
*********************************************************************************************************************/
#include <stdio.h>
#if defined(UPF_OS_IS_WINNT) || defined(UPF_OS_IS_WINCE)
#include <windows.h>
#endif
#include "upf/upf_dubhe/UPF_Dubhe_FileIO.h"
#include "UPF_Malloc.h"
#include <string.h>
/*********************************************************************************************************************
*									Include File Section															 *
*********************************************************************************************************************/
extern "C"
{
	#include "png.h"
};

/*********************************************************************************************************************
*									Include File Section															 *
*********************************************************************************************************************/
#include "UTILS_PngCompressLib.h"

/*********************************************************************************************************************
*									Macro Definition Section														 *
*********************************************************************************************************************/
#ifndef UPF_OS_IS_WINCE
#define ASSERT(_PAR_) \
{\
	char acPrint[128];\
	if(!_PAR_)\
	{\
		sprintf(acPrint, "PNG Assert Posline:%d\n",__LINE__);\
		DebugBreak();\
	}\
}
#else
#define ASSERT(_PAR_)
#endif

//#define PNG_CAPABILITY
//#define PNGMEMTEST
/*********************************************************************************************************************
*									Type Definition Section															 *
*********************************************************************************************************************/
typedef unsigned char byte;

typedef struct {
	char* data;
	int size;
	int offset;
}ImageSource;
/*********************************************************************************************************************
*									Global Variables Definition Section												 *
*********************************************************************************************************************/
#ifdef PNGMEMTEST
static int		s_iMemSize = 0;
static int		s_iMaxSize = 0;
#endif
/*********************************************************************************************************************
*									Semaphore definition															 *
*********************************************************************************************************************/

/*********************************************************************************************************************
*									Functions Prototype Declaration Section											 *
*********************************************************************************************************************/
void UTILS_PngReadFile(png_structp, png_bytep, png_size_t);
void UTILS_PngReadFileNoEncrypto(png_structp, png_bytep, png_size_t);

BOOL PngLoadImage (FileHandle_t fileHandle, png_byte **ppbImageData, int *piWidth, int *piHeight, 
				   int *piChannels, png_color *pBkgColor, png_structpp ppng_ptr, png_infopp ppng_info, png_voidp png_alloctor, bool bIsEncrypto);

png_voidp UTILS_alloc(png_structp png_ptr, png_size_t size);

void UTILS_free(png_structp png_ptr, png_voidp png_buffer);
/*********************************************************************************************************************
*									Functions SourceCode Implement Section											 *
*********************************************************************************************************************/
int UTILS_ViewMisc_DecodePngFile_Do(FileHandle_t fileHandle, int* piChannels, int* piWidth, int* piHeight, fun_png callBack, void** ppvParam, UPF_Allocator_Handle palloctor, bool bIsEncrypto)
{
	unsigned char	*pcImageData = NULL, *src = NULL, *dst = NULL;
	int				iWidth = 0, iHeight = 0;
	png_color		stBkgColor = {0};
	BOOL			bLoadRet = FALSE;
	int				yImg, xImg;
	unsigned long	ulImgRowBytes = 0, ulDIRowBytes = 0;
	unsigned char	a,r,g,b;
	png_structp		png_ptr = NULL;
	png_infop		info_ptr = NULL;
	png_voidp		png_alloctor = (png_voidp)palloctor;

	if (NULL == piChannels
		|| NULL == piWidth
		|| NULL == piHeight)
	{
		return UTILS_PNG_FAILURE;
	}

	*piChannels		= 0; 
	
#ifdef PNG_CAPABILITY
	DWORD			dwSysTime;
	FILETIME		ftCreateTime1, ftExitTime1, ftKernelTime1, ftUserTime1;
	FILETIME		ftCreateTime2, ftExitTime2, ftKernelTime2, ftUserTime2;
	
	dwSysTime = GetTickCount();
	GetThreadTimes(GetCurrentThread(), &ftCreateTime1, &ftExitTime1, &ftKernelTime1, &ftUserTime1);
#endif
	bLoadRet = PngLoadImage(fileHandle, &pcImageData, &iWidth, &iHeight, piChannels, &stBkgColor, &png_ptr, &info_ptr, png_alloctor, bIsEncrypto);
	if (bLoadRet)
	{
		ulImgRowBytes = (*piChannels) * iWidth;
//		ulDIRowBytes = (unsigned long) ((iChannels * iWidth + 3L) >> 2) << 2;
		dst = (uchar* )callBack((iWidth) * (iHeight) * sizeof(uchar) * (*piChannels), ppvParam);
		if (dst == (uchar*)NULL)
		{
			UTILS_free(png_ptr,pcImageData);
			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
			return UTILS_PNG_FAILURE;
		}

		for (yImg = 0; yImg < iHeight; yImg++)
		{
			src = pcImageData + yImg * ulImgRowBytes;
//			dst = pDiData + yWin * wDIRowBytes + iWidth * iChannels;

			for (xImg = 0; xImg < iWidth; xImg++)
			{
				r = *src++;
				g = *src++;
				b = *src++;
				*dst++ = r; /* note the reverse order */
				*dst++ = g;
				*dst++ = b;
				if ((*piChannels) == 4)
				{
					a = *src++;
					*dst++ = a;
				}
			}
		}

		*piWidth = iWidth;
		*piHeight = iHeight;
		UTILS_free(png_ptr,pcImageData);
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
#ifdef PNG_CAPABILITY
#ifndef UPF_OS_IS_WINCE
		mos_trace("[UTILS_PNG]\t[Capability]\tSystem Time : %d\n", (GetTickCount() - dwSysTime));
#else
		mos_trace_to_file("[UTILS_PNG]\t[Capability]\tSystem Time : %d\n", (GetTickCount() - dwSysTime));
#endif
		GetThreadTimes(GetCurrentThread(), &ftCreateTime2, &ftExitTime2, &ftKernelTime2, &ftUserTime2);
#ifndef UPF_OS_IS_WINCE
		mos_trace("[UTILS_PNG]\t[Capability]\tThread Time : C:%d, E:%d, K:%d, U:%d\n", 
			(ftCreateTime2.dwLowDateTime - ftCreateTime1.dwLowDateTime)/10000,
			(ftExitTime2.dwLowDateTime - ftExitTime2.dwLowDateTime)/10000,
			(ftKernelTime2.dwLowDateTime - ftKernelTime1.dwLowDateTime)/10000,
			(ftUserTime2.dwLowDateTime - ftUserTime1.dwLowDateTime)/10000 );
#else
		mos_trace_to_file("[UTILS_PNG]\t[Capability]\tThread Time : C:%d, E:%d, K:%d, U:%d\n", 
			(ftCreateTime2.dwLowDateTime - ftCreateTime1.dwLowDateTime)/10000,
			(ftExitTime2.dwLowDateTime - ftExitTime2.dwLowDateTime)/10000,
			(ftKernelTime2.dwLowDateTime - ftKernelTime1.dwLowDateTime)/10000,
			(ftUserTime2.dwLowDateTime - ftUserTime1.dwLowDateTime)/10000 );
#endif
#endif
#ifdef PNGMEMTEST
		mos_trace("[UTILS_PNG]\t[Memory]\tMax Size : %d\n", s_iMaxSize);
#endif
		return UTILS_PNG_OK;
	}
	
	return UTILS_PNG_FAILURE;
}

int UTILS_ViewMisc_DecodePngFile(FileHandle_t fileHandle, int* piChannels, int* piWidth, int* piHeight, fun_png callBack, void** ppvParam, UPF_Allocator_Handle palloctor)
{
	return UTILS_ViewMisc_DecodePngFile_Do(fileHandle, piChannels, piWidth, piHeight, callBack, ppvParam, palloctor,true);
}

int UTILS_ViewMisc_DecodePngFileNoEncrypto(FileHandle_t fileHandle, int* piChannels, int* piWidth, int* piHeight, fun_png callBack, void** ppvParam, UPF_Allocator_Handle palloctor)
{
	return UTILS_ViewMisc_DecodePngFile_Do(fileHandle, piChannels, piWidth, piHeight, callBack, ppvParam, palloctor, false);
}

BOOL PngLoadImage (FileHandle_t fileHandle, png_byte **ppbImageData, int *piWidth, int *piHeight, 
				   int *piChannels, png_color *pBkgColor, png_structpp ppng_ptr, png_infopp ppng_info, png_voidp png_alloctor, bool bIsEncrypto)
{
	png_byte            pbSig[8];
	int                 iBitDepth;
	int                 iColorType;
	double              dGamma;
	png_color_16		*pBackground;
	png_uint_32         ulChannels;
	png_uint_32         ulRowBytes;
	png_byte			*pbImageData = *ppbImageData;
	png_byte			**ppbRowPointers = NULL;
	int                 i;
	png_structp			png_ptr = NULL;
	png_infop			info_ptr = NULL;

	if (!fileHandle)
	{
		pbImageData = NULL;
		return FALSE;
	}

	// first check the eight byte PNG signature
	if (bIsEncrypto)
	{
		UPF_Dubhe_read(fileHandle, (void*)pbSig, 8);	//fread(pbSig, 1, 8, pfFile);
	}
	else
	{
		fread((void*)pbSig, 1, 8, (FILE *)fileHandle);
	}
	if (!png_check_sig(pbSig, 8))
	{
		*ppbImageData = pbImageData = NULL;
		return FALSE;
	}

	// create the two png(-info) structures
	//png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,NULL, NULL);
	png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,0,0,0, png_alloctor, UTILS_alloc, UTILS_free);
	if (!png_ptr)
	{
		*ppbImageData = pbImageData = NULL;
		return FALSE;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		*ppbImageData = pbImageData = NULL;
		return FALSE;
	}

// initialize the png structure
// #if !defined(PNG_NO_STDIO)
// 		png_init_io(png_ptr, pfFile);
// #else
// 		png_set_read_fn(png_ptr, (png_voidp)pfFile, png_read_data);
// #endif
	if (bIsEncrypto)
	{
		png_set_read_fn(png_ptr, (png_voidp)fileHandle, UTILS_PngReadFile);
	}
	else
	{
		png_set_read_fn(png_ptr, (png_voidp)fileHandle, UTILS_PngReadFileNoEncrypto);
	}

	png_set_sig_bytes(png_ptr, 8);

	// read all PNG info up to image data
	png_read_info(png_ptr, info_ptr);

	// get width, height, bit-depth and color-type
	png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)piWidth, (png_uint_32*)piHeight, &iBitDepth,&iColorType, NULL, NULL, NULL);

	// expand images of all color-type and bit-depth to 3x8 bit RGB images
	// let the library process things like alpha, transparency, background
	if (iBitDepth == 16)
		png_set_strip_16(png_ptr);
	if (iColorType == PNG_COLOR_TYPE_PALETTE)
		png_set_expand(png_ptr);
	if (iBitDepth < 8)
		png_set_expand(png_ptr);
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_expand(png_ptr);
	if (iColorType == PNG_COLOR_TYPE_GRAY || iColorType == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	// set the background color to draw transparent and alpha images over.
	if (png_get_bKGD(png_ptr, info_ptr, &pBackground))
	{
		pBkgColor->red   = (byte) pBackground->red;
		pBkgColor->green = (byte) pBackground->green;
		pBkgColor->blue  = (byte) pBackground->blue;
	}
	else
	{
		pBkgColor = NULL;
	}

	// if required set gamma conversion
	if (png_get_gAMA(png_ptr, info_ptr, &dGamma))	png_set_gamma(png_ptr, (double) 2.2, dGamma);

	// after the transformations have been registered update info_ptr data
	png_read_update_info(png_ptr, info_ptr);

	// get again width, height and the new bit-depth and color-type
	png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)piWidth, (png_uint_32*)piHeight, &iBitDepth, &iColorType, NULL, NULL, NULL);


	// row_bytes is the width x number of channels
	ulRowBytes = png_get_rowbytes(png_ptr, info_ptr);
	ulChannels = png_get_channels(png_ptr, info_ptr);

	*piChannels = ulChannels;

	// now we can allocate memory to store the image
	if (pbImageData)
	{
		//free (pbImageData);
		UTILS_free(png_ptr, pbImageData);		// modify by zhoub
		pbImageData = NULL;
	}
	//if ((pbImageData = (png_byte *) malloc(ulRowBytes * (*piHeight) * sizeof(png_byte))) == NULL)	ASSERT(0);
	if( (pbImageData = (png_byte*)UTILS_alloc(png_ptr, ulRowBytes * (*piHeight) * sizeof(png_byte))) == NULL ) 
		return FALSE;		// modify by zhoub
	*ppbImageData = pbImageData;

	// and allocate memory for an array of row-pointers
	//if ((ppbRowPointers = (png_bytepp) malloc((*piHeight) * sizeof(png_bytep))) == NULL)	ASSERT(0);
	if((ppbRowPointers = (png_bytepp)UTILS_alloc(png_ptr, (*piHeight) * sizeof(png_bytep))) == NULL) 
		return FALSE;		// modify by zhoub

	// set the individual row-pointers to point at the correct offsets
	for (i = 0; i < (*piHeight); i++)
		ppbRowPointers[i] = pbImageData + i * ulRowBytes;

	// now we can go ahead and just read the whole image
	png_read_image(png_ptr, ppbRowPointers);

	// read the additional chunks in the PNG file (not really needed)
	png_read_end(png_ptr, NULL);

	// and we're done
	//free (ppbRowPointers);
	UTILS_free(png_ptr, ppbRowPointers);
	ppbRowPointers = NULL;
	// yepp, done

	if (0)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		pbImageData = NULL;
		if(ppbRowPointers)
			//free (ppbRowPointers);
			UTILS_free(png_ptr, ppbRowPointers);
//		fclose(pfFile);
		return FALSE;
	}
//	fclose (pfFile);
	*ppng_ptr = png_ptr;
	*ppng_info = info_ptr;
	return TRUE;
}

void UTILS_PngReadFile(png_structp png_ptr, png_bytep pbData, png_size_t iSize)
{
	if(png_ptr == NULL)		return;
	if(pbData == NULL)		return;
	
	UPF_Dubhe_read((long)(png_ptr->io_ptr), pbData, iSize);
}

void UTILS_PngReadMem(png_structp png_ptr, png_bytep pbData, png_size_t iSize)
{
	PngImageSource_t* isource = (PngImageSource_t*)png_get_io_ptr(png_ptr);

	{
		memcpy(pbData, isource->data + isource->offset, iSize);
		isource->offset += iSize;
	}

	return ;
}

void UTILS_PngReadFileNoEncrypto(png_structp png_ptr, png_bytep pbData, png_size_t iSize)
{
	if(png_ptr == NULL)		return;
	if(pbData == NULL)		return;

	fread(pbData, 1, iSize, (FILE *)(png_ptr->io_ptr));
}
//////////////////////////////////////////////////////////////////////
png_voidp UTILS_alloc(png_structp png_ptr, png_size_t size)
{
#ifdef PNGMEMTEST
	void *pbuffer = NULL;
	mos_trace("[UTILS_PNG]\talloc size : %d\n", size);
	s_iMemSize += size;
	if (s_iMemSize > s_iMaxSize)
	{
		s_iMaxSize = s_iMemSize;
	}
	size += 4;
	pbuffer = UPF_Malloc( (UPF_Allocator_Handle)png_ptr->mem_ptr, size );
	*(int *)(pbuffer) = size - 4;
	return (png_voidp)((char*)(pbuffer) + 4);
#else
	return (png_voidp)UPF_Malloc( (UPF_Allocator_Handle)png_ptr->mem_ptr, size );
#endif
}

void UTILS_free(png_structp png_ptr, png_voidp png_buffer)
{
#ifdef PNGMEMTEST
	void	*pbuffer = NULL;
	int		ibufSize = 0;
	pbuffer = (void*)((char*)(png_buffer) - 4);
	ibufSize = *(int*)pbuffer;
	mos_trace("[UTILS_PNG]\tfree size : %d\n", ibufSize);
	s_iMemSize -= ibufSize;
	UPF_Free( (UPF_Allocator_Handle)png_ptr->mem_ptr, pbuffer );
#else
	UPF_Free( (UPF_Allocator_Handle)png_ptr->mem_ptr, png_buffer);
#endif
}

int UTILS_StartDecordPngFile(bool bEncryp, FileHandle_t fileHandle, int* piWidth, int* piHeight, int *piChannels, png_structpp ppng_ptr, png_infopp ppng_info, UPF_Allocator_Handle png_alloctor)
{
	png_byte            pbSig[8];
	int                 iBitDepth;
	int                 iColorType;
	double              dGamma;
	png_color_16		*pBackground;
	png_uint_32         ulChannels;
	png_byte			**ppbRowPointers = NULL;
	png_structp			png_ptr = NULL;
	png_infop			info_ptr = NULL;

	if (!fileHandle)
	{
		return FAILURE;
	}

	// first check the eight byte PNG signature
	if (bEncryp)
	{
		UPF_Dubhe_read(fileHandle, (void*)pbSig, 8);	//fread(pbSig, 1, 8, pfFile);
	}
	else
	{
		fread((void*)pbSig, 1, 8, (FILE *)fileHandle);
	}
	
	if (!png_check_sig(pbSig, 8))
	{
		return FAILURE;
	}

	// create the two png(-info) structures
	//png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,NULL, NULL);
	png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,0,0,0, png_alloctor, UTILS_alloc, UTILS_free);
	if (!png_ptr)
	{
		return FAILURE;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return FAILURE;
	}

	// initialize the png structure
	// #if !defined(PNG_NO_STDIO)
	// 		png_init_io(png_ptr, pfFile);
	// #else
	// 		png_set_read_fn(png_ptr, (png_voidp)pfFile, png_read_data);
	// #endif
	if (bEncryp)
	{
		png_set_read_fn(png_ptr, (png_voidp)fileHandle, UTILS_PngReadFile);
	}
	else
	{
		png_set_read_fn(png_ptr, (png_voidp)fileHandle, UTILS_PngReadFileNoEncrypto);
	}
	png_set_sig_bytes(png_ptr, 8);

	// read all PNG info up to image data
	png_read_info(png_ptr, info_ptr);

	// get width, height, bit-depth and color-type
	png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)piWidth, (png_uint_32*)piHeight, &iBitDepth,&iColorType, NULL, NULL, NULL);

	// expand images of all color-type and bit-depth to 3x8 bit RGB images
	// let the library process things like alpha, transparency, background
	if (iBitDepth == 16)
		png_set_strip_16(png_ptr);
	if (iColorType == PNG_COLOR_TYPE_PALETTE)
		png_set_expand(png_ptr);
	if (iBitDepth < 8)
		png_set_expand(png_ptr);
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_expand(png_ptr);
	if (iColorType == PNG_COLOR_TYPE_GRAY || iColorType == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);
// 	if (iColorType == PNG_COLOR_TYPE_RGB_ALPHA)
// 		png_set_bgr(png_ptr);

	// if required set gamma conversion
	if (png_get_gAMA(png_ptr, info_ptr, &dGamma))	png_set_gamma(png_ptr, (double) 2.2, dGamma);

	// after the transformations have been registered update info_ptr data
	png_read_update_info(png_ptr, info_ptr);

	// get again width, height and the new bit-depth and color-type
	png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)piWidth, (png_uint_32*)piHeight, &iBitDepth, &iColorType, NULL, NULL, NULL);


	// row_bytes is the width x number of channels
	ulChannels = png_get_channels(png_ptr, info_ptr);

	*piChannels = ulChannels;

	//	fclose (pfFile);
	*ppng_ptr = png_ptr;
	*ppng_info = info_ptr;
	return SUCCESS;
}

int UTILS_StartDecordPngMem(char* pcBuffer, int* piWidth, int* piHeight, int *piChannels, png_structpp ppng_ptr, png_infopp ppng_info, PngImageSource_t* pImageSource, UPF_Allocator_Handle png_alloctor, bool IsUseBkgdInfo)
{
	png_byte            pbSig[8];
	int                 iBitDepth;
	int                 iColorType;
	double              dGamma;
	png_color_16		*pBackground;
	png_uint_32         ulChannels;
	png_byte			**ppbRowPointers = NULL;
	png_structp			png_ptr = NULL;
	png_infop			info_ptr = NULL;

	if (!pcBuffer)
	{
		return FAILURE;
	}

	// first check the eight byte PNG signature
	memcpy(pbSig, pcBuffer, 8);
	if (!png_check_sig(pbSig, 8))
	{
		return FAILURE;
	}

	// create the two png(-info) structures
	//png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,NULL, NULL);
	png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,0,0,0, png_alloctor, UTILS_alloc, UTILS_free);
	if (!png_ptr)
	{
		return FAILURE;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return FAILURE;
	}

	// initialize the png structure
	// #if !defined(PNG_NO_STDIO)
	// 		png_init_io(png_ptr, pfFile);
	// #else
	// 		png_set_read_fn(png_ptr, (png_voidp)pfFile, png_read_data);
	// #endif
	pImageSource->data = pcBuffer;
	pImageSource->offset = 8;
	png_set_read_fn(png_ptr, (png_voidp)(pImageSource), UTILS_PngReadMem);
	png_set_sig_bytes(png_ptr, 8);

	// read all PNG info up to image data
	png_read_info(png_ptr, info_ptr);

	// get width, height, bit-depth and color-type
	png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)piWidth, (png_uint_32*)piHeight, &iBitDepth,&iColorType, NULL, NULL, NULL);

	// expand images of all color-type and bit-depth to 3x8 bit RGB images
	// let the library process things like alpha, transparency, background
	if (iBitDepth == 16)
		png_set_strip_16(png_ptr);
	if (iColorType == PNG_COLOR_TYPE_PALETTE)
		png_set_expand(png_ptr);
	if (iBitDepth < 8)
		png_set_expand(png_ptr);
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_expand(png_ptr);
	if (iColorType == PNG_COLOR_TYPE_GRAY || iColorType == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);
	// 	if (iColorType == PNG_COLOR_TYPE_RGB_ALPHA)
	// 		png_set_bgr(png_ptr);

	// set the background color to draw transparent and alpha images over.
	if (IsUseBkgdInfo)
	{
		if (png_get_bKGD(png_ptr, info_ptr, &pBackground))
		{
			png_set_background(png_ptr, pBackground, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
		}
	}

	// if required set gamma conversion
	if (png_get_gAMA(png_ptr, info_ptr, &dGamma))	png_set_gamma(png_ptr, (double) 2.2, dGamma);

	// after the transformations have been registered update info_ptr data
	png_read_update_info(png_ptr, info_ptr);

	// get again width, height and the new bit-depth and color-type
	png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)piWidth, (png_uint_32*)piHeight, &iBitDepth, &iColorType, NULL, NULL, NULL);


	// row_bytes is the width x number of channels
	ulChannels = png_get_channels(png_ptr, info_ptr);

	*piChannels = ulChannels;

	//	fclose (pfFile);
	*ppng_ptr = png_ptr;
	*ppng_info = info_ptr;
	return SUCCESS;
}

int UTILS_NextDecordPngLine(png_structp png_ptr, unsigned char* pbImageData)
{
	int pass, idx;

	if(png_ptr != NULL) 
	{
		//png_debug(1, "in png_read_image\n");
		/* save jump buffer and error functions */
		pass = png_set_interlace_handling(png_ptr);

		for (idx = 0; idx < pass; idx++)
		{
			png_read_row(png_ptr, pbImageData, NULL);
		}
		return SUCCESS;
	}

	return FAILURE;
}

void UTILS_EndDecordPngFile(png_structp png_ptr, png_infop png_info)
{
	if (png_ptr != NULL)
	{
		// read the additional chunks in the PNG file (not really needed)
		//png_read_end(png_ptr, NULL);
		png_destroy_read_struct(&png_ptr, &png_info, NULL);
	}
}

static void pngReaderCallback(png_structp png_ptr, png_bytep data, png_size_t length)
{
	ImageSource* isource = (ImageSource*)png_get_io_ptr(png_ptr);

	if(isource->offset + length <= isource->size)
	{
		memcpy(data, isource->data + isource->offset, length);
		isource->offset += length;
	}
	else
	{
		png_error(png_ptr,"pngReaderCallback failed");
	}
}

bool PngLoadBuffer (char* pixelData, unsigned long size, png_voidp png_alloctor, png_byte **ppbImageData, int *piWidth, int *piHeight,
				   int *piChannels, png_color *pBkgColor, int *pBitDepth, int *pColorType, png_structpp ppng_ptr, png_infopp ppng_info)
{
	int                 iBitDepth;
	int                 iColorType;
	double              dGamma;
	png_color_16		*pBackground;
	png_uint_32         ulChannels;
	png_uint_32         ulRowBytes;
	png_byte			*pbImageData = *ppbImageData;
	png_byte			**ppbRowPointers = NULL;
	png_structp			png_ptr = NULL;
	png_infop			info_ptr = NULL;

    //png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,NULL,NULL);
	png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,0,0,0, png_alloctor, UTILS_alloc, UTILS_free);
    if (!png_ptr)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    }
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    }


    ImageSource imgsource;
    imgsource.data = pixelData;
    imgsource.size = size;
    imgsource.offset = 0;
    //define our own callback function for I/O instead of reading from a file
    png_set_read_fn(png_ptr,&imgsource, pngReaderCallback );


    /* **************************************************
     * The low-level read interface in libpng (http://www.libpng.org/pub/png/libpng-1.2.5-manual.html)
     * **************************************************
     */
	// read all PNG info up to image data
	png_read_info(png_ptr, info_ptr);

	// get width, height, bit-depth and color-type
	png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)piWidth, (png_uint_32*)piHeight, &iBitDepth,&iColorType, NULL, NULL, NULL);
	*pBitDepth = iBitDepth;
	*pColorType = iColorType;

	// expand images of all color-type and bit-depth to 3x8 bit RGB images
	// let the library process things like alpha, transparency, background
	if (iBitDepth == 16)
		png_set_strip_16(png_ptr);
	if (iColorType == PNG_COLOR_TYPE_PALETTE)
		png_set_expand(png_ptr);
	if (iBitDepth < 8)
		png_set_expand(png_ptr);
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_expand(png_ptr);
	if (iColorType == PNG_COLOR_TYPE_GRAY || iColorType == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);
	// 	if (iColorType == PNG_COLOR_TYPE_RGB_ALPHA || iColorType == PNG_COLOR_TYPE_RGB)
	//  		png_set_bgr(png_ptr);

	// set the background color to draw transparent and alpha images over.
	if (png_get_bKGD(png_ptr, info_ptr, &pBackground))
	{
		pBkgColor->red   = (byte) pBackground->red;
		pBkgColor->green = (byte) pBackground->green;
		pBkgColor->blue  = (byte) pBackground->blue;
	}
	else
	{
		pBkgColor = NULL;
	}

	// if required set gamma conversion
	if (png_get_gAMA(png_ptr, info_ptr, &dGamma))	png_set_gamma(png_ptr, (double) 2.2, dGamma);

	// after the transformations have been registered update info_ptr data
	png_read_update_info(png_ptr, info_ptr);
// 
// 	// get again width, height and the new bit-depth and color-type
// 	png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)piWidth, (png_uint_32*)piHeight, &iBitDepth, &iColorType, NULL, NULL, NULL);


	// row_bytes is the width x number of channels
	ulRowBytes = png_get_rowbytes(png_ptr, info_ptr);
	ulChannels = png_get_channels(png_ptr, info_ptr);

	*piChannels = ulChannels;

	// now we can allocate memory to store the image
	if (pbImageData)
	{
		free (pbImageData);
		pbImageData = NULL;
	}
    if ((pbImageData = (png_byte *) malloc(ulRowBytes * (*piHeight) * sizeof(png_byte))) == NULL)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return false;
    }
	*ppbImageData = pbImageData;

	// and allocate memory for an array of row-pointers
    if ((ppbRowPointers = (png_bytepp) malloc((*piHeight) * sizeof(png_bytep))) == NULL)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return false;
    }

	// set the individual row-pointers to point at the correct offsets
	for (int i = 0; i < (*piHeight); i++)
		ppbRowPointers[i] = pbImageData + i * ulRowBytes;

	// now we can go ahead and just read the whole image
	png_read_image(png_ptr, ppbRowPointers);

	// read the additional chunks in the PNG file (not really needed)
	png_read_end(png_ptr, NULL);

	// and we're done
	free (ppbRowPointers);
	ppbRowPointers = NULL;
	// yepp, done

	//fclose (fileHandle);
	*ppng_ptr = png_ptr;
	*ppng_info = info_ptr;
	return true;
}

int UTILS_DecodePngBuffer(char* pcBuffer, unsigned long size, UPF_Allocator_Handle palloctor, int* piChannels, int* piWidth, int* piHeight, int *pBitDepth, int *pColorType, unsigned char ** ppvParam)
{
	unsigned char	*pcImageData = NULL, *src = NULL, *dst = NULL;
	int				iWidth = 0, iHeight = 0;
	png_color		stBkgColor = {0};
	bool			bLoadRet = false;
	int				yImg, xImg;
	unsigned long	ulImgRowBytes = 0, ulDIRowBytes = 0;
	unsigned char	a,r,g,b;
	png_structp		png_ptr = NULL;
	png_infop		info_ptr = NULL;
	png_voidp		png_alloctor = (png_voidp)palloctor;
    int ret = FAILURE;
    do
    {
        if (NULL == piChannels
            || NULL == piWidth
            || NULL == piHeight)
        {
            break ;
        }

        *piChannels		= 0;

        bLoadRet = PngLoadBuffer(pcBuffer, size, png_alloctor, &pcImageData, &iWidth, &iHeight, piChannels, &stBkgColor, pBitDepth, pColorType, &png_ptr, &info_ptr);
        if (bLoadRet)
        {
            ulImgRowBytes = (*piChannels) * iWidth;
            *ppvParam = dst = (unsigned char * )UPF_Malloc(palloctor, (iWidth) * (iHeight) * sizeof(unsigned char) * (*piChannels));
            if (dst == (unsigned char*)NULL)
            {
                UPF_Free(palloctor, pcImageData);
                png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
                break ;
            }

            // Y轴翻转
            for (yImg = iHeight - 1; yImg >= 0; yImg--)
            {
                src = pcImageData + yImg * ulImgRowBytes;
                // X轴翻转
                for (xImg = iWidth - 1; xImg >= 0; xImg--)
                {
                    r = *src++;
                    g = *src++;
                    b = *src++;
                    *dst++ = r; /* note the reverse order */
                    *dst++ = g;
                    *dst++ = b;
                    if ((*piChannels) == 4)
                    {
                        a = *src++;
                        *dst++ = a;
                    }
                }
            }

            *piWidth = iWidth;
            *piHeight = iHeight;
            free(pcImageData);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            ret = SUCCESS;
        }

    }while(false);
    return ret;
}

/*************************************************end****************************************************************/
