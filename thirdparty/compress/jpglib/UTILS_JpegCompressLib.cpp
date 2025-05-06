/*----------------Copyright(C) 2007 Shenyang Familiar software Co.,Ltd. ALL RIGHTS RESERVED-------------------------*/

/*********************************************************************************************************************
*	FILE NAME	: DTM_JpegLib.c																			             *
*	CREATE DATE	: 2009-06-24																						 *
*	MODULE		: DTM																								 *
*	AUTHOR		: [CN]朱小莹[CN]																					 *
*--------------------------------------------------------------------------------------------------------------------*
*	MEMO		:																									 *
*********************************************************************************************************************/

/*********************************************************************************************************************
*									Include File Section															 *
*********************************************************************************************************************/
#include <stdio.h>
#include "UPF_Malloc.h"
/*********************************************************************************************************************
*									Include File Section															 *
*********************************************************************************************************************/
extern "C"
{
#include "jpeglib.h"
};

/*********************************************************************************************************************
*									Include File Section															 *
*********************************************************************************************************************/
#include "UTILS_JpegCompressLib.h"

/*********************************************************************************************************************
*									Macro Definition Section														 *
*********************************************************************************************************************/

/*********************************************************************************************************************
*									Type Definition Section															 *
*********************************************************************************************************************/

/*********************************************************************************************************************
*									Global Variables Definition Section												 *
*********************************************************************************************************************/

/*********************************************************************************************************************
*									Semaphore definition															 *
*********************************************************************************************************************/

/*********************************************************************************************************************
*									Functions Prototype Declaration Section											 *
*********************************************************************************************************************/

/*********************************************************************************************************************
*									Functions SourceCode Implement Section											 *
*********************************************************************************************************************/
void utils_Init_Global_Var()
{

}

int UTILS_ViewMisc_DecodeJpegFile(FileHandle_t fileHandle, int* piChannels, int* piWidth, int* piHeight, fun callBack, void** ppvParam, UPF_Allocator_Handle palloctor)
{
	return UTILS_ViewMisc_DecodeJpegFile_Do(fileHandle, piChannels, piWidth, piHeight, callBack, ppvParam, palloctor,true);
}

int UTILS_ViewMisc_DecodeJpegFileNoEncrypto(FileHandle_t fileHandle, int* piChannels, int* piWidth, int* piHeight, fun callBack, void** ppvParam, UPF_Allocator_Handle palloctor)
{
	return UTILS_ViewMisc_DecodeJpegFile_Do(fileHandle, piChannels, piWidth, piHeight, callBack, ppvParam, palloctor, false);
}
int UTILS_ViewMisc_DecodeJpegFile_Do(FileHandle_t fileHandle, int* piChannels, int* piWidth, int* piHeight, fun callBack, void** pvParam, UPF_Allocator_Handle pvAllocator, bool bIsEncrypto)
{
	FileHandle_t						fp;
	unsigned char*						line;
	int									left;
	int									bpl;
	//int									iJPG_Width = 0;
	//int									iJPG_Height = 0;
	jpeg_decompress_struct				cinfo = {0};
	jpeg_error_mgr						jerr = {0};
	uchar								*pucImageAddr;

	if (NULL == piChannels
		|| NULL == piWidth
		|| NULL == piHeight)
	{
		return DTM_JPEG_FAILURE;
	}

	*piChannels = 3;
	
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo, pvAllocator);
	cinfo.NoEncryp = !false;
	jpeg_stdio_src(&cinfo, fileHandle);
	jpeg_read_header(&cinfo, TRUE);

	*piWidth=(int)cinfo.image_width;
	*piHeight=(int)cinfo.image_height;

	pucImageAddr = (uchar* )callBack((*piWidth) * (*piHeight) * sizeof(uchar) * 3, pvParam);

	// open file
	fp = fileHandle;
	if(!fp) {
		return DTM_JPEG_FAILURE;
	}
#if defined(UPF_OS_IS_WINNT) || defined(UPF_OS_IS_WINCE)
	__try
#endif	
	{
		// check image width & height
		if( (*piWidth == (int)cinfo.image_width) && (*piHeight == (int)cinfo.image_height) )
		{
			// make for performance up
			cinfo.dct_method = JDCT_IFAST;
			cinfo.do_fancy_upsampling = FALSE;
			cinfo.do_block_smoothing = FALSE;
			cinfo.dither_mode = JDITHER_NONE;

			// jpeg_start_decompress
			if(jpeg_start_decompress(&cinfo))
			{
				line = pucImageAddr;
				left = *piHeight;
				bpl = *piWidth * 3;	// RGB888
				while(left > 0)
				{
					JDIMENSION lines_read = jpeg_read_scanlines(&cinfo, (JSAMPARRAY)&line, 1);
					if(lines_read > 0)
					{
						line += bpl;
						left -= lines_read;
					}
				}
			}

			jpeg_finish_decompress(&cinfo);
		}
	}
#if defined(UPF_OS_IS_WINNT) || defined(UPF_OS_IS_WINCE)
	__except( 1 )	// catch all expcetion
	{
		// jpeg_destroy_decompress
		jpeg_destroy_decompress(&cinfo);

		return DTM_JPEG_FAILURE;
	}
#endif

	// jpeg_destroy_decompress
	jpeg_destroy_decompress(&cinfo);

	return DTM_JPEG_OK;
}


int UTILS_StartDecordJpegFile(FileHandle_t stFileHandle, void* pvAllocator, int* piWidth, int* piHeight, jpeg_decompress_struct* cinfo, jpeg_error_mgr* jerr, int* piLeftLines)
{
	cinfo->err = jpeg_std_error(jerr);

	jpeg_create_decompress(cinfo, pvAllocator);
	jpeg_stdio_src(cinfo, stFileHandle);
	jpeg_read_header(cinfo, TRUE);

	// make for performance up
	cinfo->dct_method = JDCT_ISLOW;
	cinfo->do_fancy_upsampling = FALSE;
	cinfo->do_block_smoothing = FALSE;

	if(!jpeg_start_decompress(cinfo))
	{
		jpeg_destroy_decompress(cinfo);
		return FAILURE;
	}
	*piWidth = cinfo->image_width;
	*piHeight = *piLeftLines = cinfo->image_height;
	return SUCCESS;
}

 //dgq
int UTILS_StartDecordJpegMem(char* pcDataSrc, int iSize, void* pvAllocator, int* piWidth, int* piHeight, jpeg_decompress_struct* cinfo, jpeg_error_mgr* jerr, int* piLeftLines)
{
	if(NULL == pcDataSrc)
	{
		return FAILURE;
	}
	
	cinfo->err = jpeg_std_error(jerr);
	jpeg_create_decompress(cinfo, pvAllocator);
	jpeg_stdio_src(cinfo, NULL);

	cinfo->src->next_input_byte	= (JOCTET *)pcDataSrc;
	cinfo->src->bytes_in_buffer	= iSize;
	
	jpeg_read_header(cinfo, TRUE);

	// make for performance up
	cinfo->dct_method = JDCT_IFAST;
	cinfo->do_fancy_upsampling = FALSE;
	cinfo->do_block_smoothing = FALSE;

	if(!jpeg_start_decompress(cinfo))
	{
		jpeg_destroy_decompress(cinfo);
		return FAILURE;
	}
	*piWidth = cinfo->image_width;
	*piHeight = *piLeftLines = cinfo->image_height;
	return SUCCESS;
}

int UTILS_NextDecordJpegLine(jpeg_decompress_struct* cinfo, int* piLeftLines, unsigned char* pucLineBuf)
{
	if(*piLeftLines > 0) {
		JDIMENSION lines_read = jpeg_read_scanlines(cinfo, (JSAMPARRAY)&pucLineBuf, 1);
		if(lines_read > 0) {
			return SUCCESS;
		}
	}
	return FAILURE;
}

void UTILS_EndDecordJpegFile(jpeg_decompress_struct* cinfo)
{
	jpeg_destroy_decompress(cinfo);
}

BOOL UTILS_decodeJpegPackageFromMbuf(char* pDataBuf,unsigned long ulDataBufSize,UPF_Allocator_Handle pvAllocator,int* piWidth, int* piHeight,  void* pvParam)
{
	//FILE*								fp;
	unsigned char*						line;
	int									left;
	int									bpl;
	jpeg_decompress_struct				cinfo = {0};
	jpeg_error_mgr						jerr = {0};
	uchar								*pucRead, *pucWrite;			/* 24 bit ===> 16 bit read and write pointer */
	unsigned short						clr;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo,pvAllocator);
	jpeg_mbuf_src(&cinfo, pDataBuf,ulDataBufSize);
	jpeg_read_header(&cinfo, TRUE);

	*piWidth=(int)cinfo.image_width;
	*piHeight=(int)cinfo.image_height;

	//pucImageAddr = (uchar* )callBack((*piWidth) * (*piHeight) * sizeof(uchar) * 3, pvParam);

	// open file
	
	
#if defined(UPF_OS_IS_WINNT) || defined(UPF_OS_IS_WINCE)
	__try
#endif	
	{
		// check image width & height
		if( (*piWidth == (int)cinfo.image_width) && (*piHeight == (int)cinfo.image_height) )
		{
			// make for performance up
			cinfo.dct_method = JDCT_IFAST;
			cinfo.do_fancy_upsampling = FALSE;
			cinfo.do_block_smoothing = FALSE;

			// jpeg_start_decompress
			if(jpeg_start_decompress(&cinfo))
			{
				line = (unsigned char*)pvParam;
				left = *piHeight;
				bpl = *piWidth * 2;	// RGB888
				while(left > 0)
				{
					JDIMENSION lines_read = jpeg_read_scanlines(&cinfo, (JSAMPARRAY)&line, 1);
					if(lines_read > 0)
					{
						for(pucRead = line, pucWrite = line; pucRead < line + *piWidth * 3; pucRead += 3, pucWrite += 2)
						{
							/* RGB888 ====> RGB565 */
							clr = ((pucRead[0] << 8) & 0xF800) | ((pucRead[1] << 3) & 0x07E0) | (pucRead[2] >> 3 & 0x001F);
							*((unsigned short*)pucWrite) = clr;
						}
						line += bpl;
						left -= lines_read;
					}
				}
			}

			jpeg_finish_decompress(&cinfo);
		}
	}
#if defined(UPF_OS_IS_WINNT) || defined(UPF_OS_IS_WINCE)
	__except( 1 )	// catch all expceti
	{
		// jpeg_destroy_decompress
		jpeg_destroy_decompress(&cinfo);

		return FALSE;
	}
#endif
	// jpeg_destroy_decompress
	jpeg_destroy_decompress(&cinfo);

	return TRUE;
}

BOOL UTILS_decodeJpegPackageFromMbufEx(char* pDataBuf,unsigned long ulDataBufSize,UPF_Allocator_Handle pvAllocator, int* piWidth, int* piHeight,  void** ppvParam)
{
//	FILE*								fp;
	unsigned char*						line;
	int									left;
	int									bpl;
	jpeg_decompress_struct				cinfo = {0};
	jpeg_error_mgr						jerr = {0};
//	uchar								*pucRead, *pucWrite;			/* 24 bit ===> 16 bit read and write pointer */
//	unsigned short						clr;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo,pvAllocator);
	jpeg_mbuf_src(&cinfo, pDataBuf,ulDataBufSize);
	jpeg_read_header(&cinfo, TRUE);

	*piWidth=(int)cinfo.image_width;
	*piHeight=(int)cinfo.image_height;

	//pucImageAddr = (uchar* )callBack((*piWidth) * (*piHeight) * sizeof(uchar) * 3, pvParam);

	// open file


#if defined(UPF_OS_IS_WINNT) || defined(UPF_OS_IS_WINCE)
	__try
#endif	
	{
		// check image width & height
		if( (*piWidth == (int)cinfo.image_width) && (*piHeight == (int)cinfo.image_height) )
		{
			// make for performance up
			cinfo.dct_method = JDCT_IFAST;
			cinfo.do_fancy_upsampling = FALSE;
			cinfo.do_block_smoothing = FALSE;

			// jpeg_start_decompress
			if(jpeg_start_decompress(&cinfo))
			{
				*ppvParam = UPF_Malloc( pvAllocator, (*piWidth)*(*piHeight)*3 );
 				line = (unsigned char*)*ppvParam;
				left = *piHeight;
 				bpl = *piWidth * 3;	// RGB888
 				while(left > 0)
 				{
 					JDIMENSION lines_read = jpeg_read_scanlines(&cinfo, (JSAMPARRAY)&line, 1);
 					if(lines_read > 0)
					{
// 						for(pucRead = line, pucWrite = line; pucRead < line + *piWidth * 3; pucRead += 3, pucWrite += 2)
// 						{
// 							/* RGB888 ====> RGB1555 */
// 							clr = ((pucRead[0] << 7) & 0x7C00) | ((pucRead[1] << 2) & 0x03E0) | (pucRead[2] >> 3 & 0x001F) | (0x0000);
// 							*((unsigned short*)pucWrite) = clr;
// 						}
 						line += bpl;
						left -= lines_read;
 					}
				}
			}

			jpeg_finish_decompress(&cinfo);
		}
	}
#if defined(UPF_OS_IS_WINNT) || defined(UPF_OS_IS_WINCE)
	__except( 1 )	// catch all expceti
	{
		// jpeg_destroy_decompress
		jpeg_destroy_decompress(&cinfo);

		return FALSE;
	}
#endif

	// jpeg_destroy_decompress
	jpeg_destroy_decompress(&cinfo);

	return TRUE;
}

/*************************************************end****************************************************************/
