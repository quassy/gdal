/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Mainline for Imagine to TIFF translation.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  1999/01/27 16:22:19  warmerda
 * Added RGB Support
 *
 * Revision 1.1  1999/01/22 17:45:13  warmerda
 * New
 */

#include "hfa_p.h"

#include "tiffiop.h"
#include "xtiffio.h"

CPL_C_START
static void ImagineToGeoTIFF( HFAHandle, HFABand *, HFABand *, HFABand *,
                              const char * );
CPLErr ImagineToGeoTIFFProjection( HFAHandle hHFA, TIFF * hTIFF );
CPLErr CopyPyramidsToTiff( HFAHandle, HFABand *, TIFF * );
static CPLErr RGBComboValidate( HFAHandle, int, int, int );
CPL_C_END

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

void Usage()

{
    printf(
      "Usage: img2tif ...\n" );
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    int		i, nBandCount, nBand, nRed=0, nGreen=0, nBlue=0;
    const char	*pszSrcFilename = NULL;
    const char	*pszDstBasename = NULL;
    HFAHandle   hHFA;

/* -------------------------------------------------------------------- */
/*      Parse commandline options.                                      */
/* -------------------------------------------------------------------- */
    for( i = 1; i < nArgc; i++ )
    {
        if( EQUAL(papszArgv[i],"-i") && i+1 < nArgc )
        {
            pszSrcFilename = papszArgv[i+1];
            i++;
        }
        else if( EQUAL(papszArgv[i],"-o") && i+1 < nArgc )
        {
            pszDstBasename = papszArgv[i+1];
            i++;
        }
        else if( EQUAL(papszArgv[i],"-rgb") && i+3 < nArgc )
        {
            nRed = atoi(papszArgv[++i]);
            nGreen = atoi(papszArgv[++i]);
            nBlue = atoi(papszArgv[++i]);
        }
        else
        {
            printf( "Unexpected argument: %s\n\n", papszArgv[i] );
            Usage();
        }
    }

    if( pszSrcFilename == NULL )
    {
        printf( "No source file provided.\n\n" );
        Usage();
    }
    
    if( pszDstBasename == NULL )
    {
        printf( "No destination file provided.\n\n" );
        Usage();
    }
    
/* -------------------------------------------------------------------- */
/*      Open the imagine file.                                          */
/* -------------------------------------------------------------------- */
    hHFA = HFAOpen( pszSrcFilename, "r" );

    if( hHFA == NULL )
        exit( 100 );

/* -------------------------------------------------------------------- */
/*      Loop over all bands, generating each TIFF file.                 */
/* -------------------------------------------------------------------- */
    HFAGetRasterInfo( hHFA, NULL, NULL, &nBandCount );

/* -------------------------------------------------------------------- */
/*      Has the user requested an RGB image?                            */
/* -------------------------------------------------------------------- */
    if( nRed > 0 )
    {
        char	szFilename[512];
        
        if( RGBComboValidate( hHFA, nRed, nGreen, nBlue ) == CE_Failure )
            exit( 1 );

        if( strstr(pszDstBasename,".") == NULL )
            sprintf( szFilename, "%s.tif", pszDstBasename );
        else
            sprintf( szFilename, "%s", pszDstBasename );

        ImagineToGeoTIFF( hHFA,
                          hHFA->papoBand[nRed-1],
                          hHFA->papoBand[nGreen-1],
                          hHFA->papoBand[nBlue-1],
                          szFilename );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we translate each band.                               */
/* -------------------------------------------------------------------- */
    else
    {
        for( nBand = 1; nBand <= nBandCount; nBand++ )
        {
            char	szFilename[512];

            if( nBandCount == 1 )
                sprintf( szFilename, "%s.tif", pszDstBasename );
            else
                sprintf( szFilename, "%s%d.tif", pszDstBasename, nBand );
        
            ImagineToGeoTIFF( hHFA, hHFA->papoBand[nBand-1], NULL, NULL,
                              szFilename );
        }
    }

    HFAClose( hHFA );

    return 0;
}

/************************************************************************/
/*                      ImagineToGeoTIFFPalette()                       */
/************************************************************************/

static
void ImagineToGeoTIFFPalette( HFABand *poBand, TIFF * hTIFF )

{
    unsigned short	anTRed[256], anTGreen[256], anTBlue[256];
    double	*padfRed, *padfGreen, *padfBlue;
    int		nColors, i;

    poBand->GetPCT( &nColors, &padfRed, &padfGreen, &padfBlue );
    CPLAssert( nColors > 0 );

    for( i = 0; i < 256; i++ )
    {
        if( i < nColors )
        {
            anTRed[i] = (unsigned short) (65535 * padfRed[i]);
            anTGreen[i] = (unsigned short) (65535 * padfGreen[i]);
            anTBlue[i] = (unsigned short) (65535 * padfBlue[i]);
        }
        else
        {
            anTRed[i] = 0;
            anTGreen[i] = 0;
            anTBlue[i] = 0;
        }
    }

    TIFFSetField( hTIFF, TIFFTAG_COLORMAP, anTRed, anTGreen, anTBlue );
}

/************************************************************************/
/*                     ImagineToGeoTIFFDataRange()                      */
/************************************************************************/

static CPLErr ImagineToGeoTIFFDataRange( HFABand * poBand, TIFF *hTIFF)

{
    double		dfMin, dfMax;
    unsigned short	nTMin, nTMax;
    HFAEntry		*poBinInfo;
    
    poBinInfo = poBand->poNode->GetNamedChild("Statistics" );

    if( poBinInfo == NULL )
        return( CE_Failure );

    dfMin = poBinInfo->GetDoubleField( "minimum" );
    dfMax = poBinInfo->GetDoubleField( "maximum" );

    if( dfMax > dfMin )
        return CE_None;
    else
        return CE_Failure;
    
    if( dfMin < 0 || dfMin > 65536 || dfMax < 0 || dfMax > 65535
        || dfMin >= dfMax )
        return( CE_Failure );

    nTMin = (unsigned short) dfMin;
    nTMax = (unsigned short) dfMax;
    
    TIFFSetField( hTIFF, TIFFTAG_MINSAMPLEVALUE, nTMin );
    TIFFSetField( hTIFF, TIFFTAG_MAXSAMPLEVALUE, nTMax );

    return( CE_None );
}

/************************************************************************/
/*                            CopyOneBand()                             */
/*                                                                      */
/*      copy just the imagery tiles from an Imagine band (full res,     */
/*      or overview) to a sample of a TIFF file.                        */
/************************************************************************/

static CPLErr CopyOneBand( HFABand * poBand, TIFF * hTIFF, int nSample )

{
    void	*pData;
    int		nTileSize;
    
/* -------------------------------------------------------------------- */
/*	Allocate a block buffer.					*/
/* -------------------------------------------------------------------- */
    nTileSize = TIFFTileSize( hTIFF );
    pData = VSIMalloc(nTileSize);
    if( pData == NULL )
    {
        printf( "Out of memory allocating working tile of %d bytes.\n",
                nTileSize );
        return( CE_Failure );
    }

/* -------------------------------------------------------------------- */
/*      Write each of the tiles.                                        */
/* -------------------------------------------------------------------- */
    int		iBlockX, iBlockY;
     
    for( iBlockY = 0; iBlockY < poBand->nBlocksPerColumn; iBlockY++ )
    {
        for( iBlockX = 0; iBlockX < poBand->nBlocksPerRow; iBlockX++ )
        {
            int	iTile;

            if( poBand->GetRasterBlock( iBlockX, iBlockY, pData )
                != CE_None )
                return( CE_Failure );

            if( HFAGetDataTypeBits(poBand->nDataType) == 16 )
            {
                int		ii;

                for( ii = 0;
                     ii < poBand->nBlockXSize*poBand->nBlockYSize;
                     ii++ )
                {
                    unsigned char *pabyData = (unsigned char *) pData;
                    int		nTemp;

                    nTemp = pabyData[ii*2];
                    pabyData[ii*2] = pabyData[ii*2+1];
                    pabyData[ii*2+1] = nTemp;
                }
            }

            iTile = TIFFComputeTile( hTIFF,
                                     iBlockX*poBand->nBlockXSize, 
                                     iBlockY*poBand->nBlockYSize,
                                     0, nSample );
            
            if( TIFFWriteEncodedTile( hTIFF, iTile, pData, nTileSize ) < 1 )
                return( CE_Failure );
        }
    }

    VSIFree( pData );

    return( CE_None );
}

#ifdef notdef
/************************************************************************/
/*                        ImagineBandToGeoTIFF()                        */
/************************************************************************/

static void ImagineBandToGeoTIFF( HFAHandle hHFA, int nBand,
                                  const char * pszDstBasename )

{
    TIFF	*hTIFF;
    char	szDstFilename[1024];
    int		nXSize, nYSize, nBlockXSize, nBlockYSize, nDataType;
    int		iBlockX, iBlockY, nBlocksPerRow, nBlocksPerColumn, nTileSize;
    void	*pData;
    double	*padfRed, *padfGreen, *padfBlue;
    int		nColors;

    HFAGetRasterInfo( hHFA, &nXSize, &nYSize, NULL );
    HFAGetBandInfo( hHFA, nBand, &nDataType, &nBlockXSize, &nBlockYSize );
    HFAGetPCT( hHFA, nBand, &nColors, &padfRed, &padfGreen, &padfBlue );

    nBlocksPerRow = (nXSize + nBlockXSize - 1) / nBlockXSize;
    nBlocksPerColumn = (nYSize + nBlockYSize - 1) / nBlockYSize;
    
/* -------------------------------------------------------------------- */
/*      Create the new file.                                            */
/* -------------------------------------------------------------------- */
    sprintf( szDstFilename, "%s%d.tif", pszDstBasename, nBand );

    hTIFF = XTIFFOpen( szDstFilename, "w+" );

/* -------------------------------------------------------------------- */
/*      Write standard header fields.                                   */
/* -------------------------------------------------------------------- */
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, nXSize );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, nYSize );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE,
                  HFAGetDataTypeBits(nDataType) );

    /* notdef: should error on illegal types */

    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    TIFFSetField( hTIFF, TIFFTAG_SUBFILETYPE, 0 );

    TIFFSetField( hTIFF, TIFFTAG_TILEWIDTH, nBlockXSize );
    TIFFSetField( hTIFF, TIFFTAG_TILELENGTH, nBlockYSize );

    if( nColors > 0 )
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
    else
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
    
/* -------------------------------------------------------------------- */
/*	Do we have min/max value information?				*/
/* -------------------------------------------------------------------- */
    ImagineToGeoTIFFDataRange( hHFA, nBand, hTIFF );

/* -------------------------------------------------------------------- */
/*	Allocate a block buffer.					*/
/* -------------------------------------------------------------------- */
    nTileSize = TIFFTileSize( hTIFF );
    pData = VSIMalloc(nTileSize);
    if( pData == NULL )
    {
        printf( "Out of memory allocating working tile of %d bytes.\n",
                nTileSize );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Write each of the tiles.                                        */
/* -------------------------------------------------------------------- */
    for( iBlockY = 0; iBlockY < nBlocksPerColumn; iBlockY++ )
    {
        for( iBlockX = 0; iBlockX < nBlocksPerRow; iBlockX++ )
        {
            int	iBlock = iBlockX + iBlockY * nBlocksPerRow;

            if( HFAGetRasterBlock( hHFA, nBand, iBlockX, iBlockY, pData )
                != CE_None )
                return;

            if( HFAGetDataTypeBits(nDataType) == 16 )
            {
                int		ii;

                for( ii = 0; ii < nBlockXSize*nBlockYSize; ii++ )
                {
                    unsigned char *pabyData = (unsigned char *) pData;
                    int		nTemp;

                    nTemp = pabyData[ii*2];
                    pabyData[ii*2] = pabyData[ii*2+1];
                    pabyData[ii*2+1] = nTemp;
                }
            }

            if( TIFFWriteEncodedTile( hTIFF, iBlock, pData, nTileSize ) < 1 )
                return;
        }
    }

    VSIFree( pData );

/* -------------------------------------------------------------------- */
/*      Write Geotiff information.                                      */
/* -------------------------------------------------------------------- */
    ImagineToGeoTIFFProjection( hHFA, hTIFF );

/* -------------------------------------------------------------------- */
/*      Write Palette                                                   */
/* -------------------------------------------------------------------- */
    if( nColors > 0 )
        ImagineToGeoTIFFPalette( poRedBand, hTIFF );

/* -------------------------------------------------------------------- */
/*      Write overviews                                                 */
/* -------------------------------------------------------------------- */
    CopyPyramidsToTiff( hHFA, nBand, hTIFF );

    XTIFFClose( hTIFF );
}
#endif

/************************************************************************/
/*                          ImagineToGeoTIFF()                          */
/************************************************************************/

static void ImagineToGeoTIFF( HFAHandle hHFA,
                              HFABand * poRedBand,
                              HFABand * poGreenBand,
                              HFABand * poBlueBand,
                              const char * pszDstFilename )

{
    TIFF	*hTIFF;
    int		nXSize, nYSize, nBlockXSize, nBlockYSize, nDataType;
    int		nBlocksPerRow, nBlocksPerColumn;
    double	*padfRed, *padfGreen, *padfBlue;
    int		nColors;

    HFAGetRasterInfo( hHFA, &nXSize, &nYSize, NULL );

    nDataType = poRedBand->nDataType;
    nBlockXSize = poRedBand->nBlockXSize;
    nBlockYSize = poRedBand->nBlockYSize;
    
/* -------------------------------------------------------------------- */
/*      Verify some conditions of similarity on the bands.  These       */
/*      should be checked before calling this function with a user      */
/*      error.  This is just an extra check.                            */
/* -------------------------------------------------------------------- */

    CPLAssert( poBlueBand == NULL
               || (poBlueBand->nDataType == nDataType
                   && poGreenBand->nDataType == nDataType) );

    CPLAssert( poBlueBand == NULL
               || (poBlueBand->nBlockXSize == nBlockXSize
                   && poGreenBand->nBlockXSize == nBlockXSize
                   && poGreenBand->nBlockYSize == nBlockYSize
                   && poGreenBand->nBlockYSize == nBlockYSize) );

    if( poBlueBand == NULL )
        poRedBand->GetPCT( &nColors, &padfRed, &padfGreen, &padfBlue );
    else
        nColors = 0;

    nBlocksPerRow = (nXSize + nBlockXSize - 1) / nBlockXSize;
    nBlocksPerColumn = (nYSize + nBlockYSize - 1) / nBlockYSize;
    
/* -------------------------------------------------------------------- */
/*      Create the new file.                                            */
/* -------------------------------------------------------------------- */
    hTIFF = XTIFFOpen( pszDstFilename, "w+" );

/* -------------------------------------------------------------------- */
/*      Write standard header fields.                                   */
/* -------------------------------------------------------------------- */
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, nXSize );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, nYSize );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE,
                  HFAGetDataTypeBits(nDataType) );

    /* notdef: should error on illegal types */

    if( poBlueBand == NULL )
    {
        TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
        TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    }
    else
    {
        TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 3 );
        TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_SEPARATE );
    }
        
    TIFFSetField( hTIFF, TIFFTAG_SUBFILETYPE, 0 );

    TIFFSetField( hTIFF, TIFFTAG_TILEWIDTH, nBlockXSize );
    TIFFSetField( hTIFF, TIFFTAG_TILELENGTH, nBlockYSize );

    if( nColors > 0 )
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
    else if( poBlueBand == NULL )
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
    else
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
    
/* -------------------------------------------------------------------- */
/*	Do we have min/max value information?				*/
/* -------------------------------------------------------------------- */
    if( poBlueBand == NULL )
        ImagineToGeoTIFFDataRange( poRedBand, hTIFF );

/* -------------------------------------------------------------------- */
/*      Copy over one, or three bands of raster data.                   */
/* -------------------------------------------------------------------- */
    CopyOneBand( poRedBand, hTIFF, 0 );

    if( poBlueBand != NULL )
    {
        CopyOneBand( poGreenBand, hTIFF, 1 );
        CopyOneBand( poBlueBand, hTIFF, 2 );
    }
    
/* -------------------------------------------------------------------- */
/*      Write Geotiff information.                                      */
/* -------------------------------------------------------------------- */
    ImagineToGeoTIFFProjection( hHFA, hTIFF );

/* -------------------------------------------------------------------- */
/*      Write Palette                                                   */
/* -------------------------------------------------------------------- */
    if( nColors > 0 )
        ImagineToGeoTIFFPalette( poRedBand, hTIFF );

/* -------------------------------------------------------------------- */
/*      Write overviews                                                 */
/* -------------------------------------------------------------------- */
    CopyPyramidsToTiff( hHFA, poRedBand, hTIFF );

    XTIFFClose( hTIFF );
}

/************************************************************************/
/*                              RRD2Tiff()                              */
/*                                                                      */
/*      Copy one reduced resolution layer to a TIFF file.               */
/************************************************************************/

static
CPLErr RRD2Tiff( HFABand * poBand, TIFF * hTIFF,
                 int nPhotometricInterp,
                 int nCompression )

{
    if( poBand->nBlockXSize % 16 != 0 || poBand->nBlockYSize % 16 != 0 )
        return( CE_Failure );

    TIFFWriteDirectory( hTIFF );

    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, poBand->nWidth );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, poBand->nHeight );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE,
                  HFAGetDataTypeBits(poBand->nDataType) );

    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );

    TIFFSetField( hTIFF, TIFFTAG_TILEWIDTH, poBand->nBlockXSize );
    TIFFSetField( hTIFF, TIFFTAG_TILELENGTH, poBand->nBlockYSize );

    TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, nPhotometricInterp );
    TIFFSetField( hTIFF, TIFFTAG_COMPRESSION, nCompression );
   
    TIFFSetField( hTIFF, TIFFTAG_SUBFILETYPE, FILETYPE_REDUCEDIMAGE );
   
    return( CopyOneBand( poBand, hTIFF, 0 ) );
}


/************************************************************************/
/*                         CopyPyramidsToTiff()                         */
/*                                                                      */
/*      Copy reduced resolution layers to the TIFF file as              */
/*      overviews.                                                      */
/************************************************************************/

CPLErr CopyPyramidsToTiff( HFAHandle psInfo, HFABand *poBand, TIFF * hTIFF )

{
    HFAEntry	*poBandNode = poBand->poNode;
    HFAEntry	*poSubNode;
    int		nColors, nPhotometric;
    double	*padfRed, *padfGreen, *padfBlue;

    poBand->GetPCT( &nColors, &padfRed, &padfGreen, &padfBlue );
    if( nColors == 0 )
        nPhotometric = PHOTOMETRIC_MINISBLACK;
    else
        nPhotometric = PHOTOMETRIC_PALETTE;

    for( poSubNode = poBandNode->GetChild();
         poSubNode != NULL;
         poSubNode = poSubNode->GetNext() )
    {
        HFABand		*poOverviewBand;
        
        if( !EQUAL(poSubNode->GetType(),"Eimg_Layer_SubSample") )
            continue;

        poOverviewBand = new HFABand( psInfo, poSubNode );
        
        if( RRD2Tiff( poOverviewBand, hTIFF, nPhotometric, COMPRESSION_NONE )
						            == CE_None
            && nColors > 0 )
            ImagineToGeoTIFFPalette( poBand, hTIFF );
        
        delete poOverviewBand;
    }

    return CE_None;
}

/************************************************************************/
/*                          RGBComboValidate()                          */
/************************************************************************/

static CPLErr RGBComboValidate( HFAHandle hHFA,
                                int nRed, int nBlue, int nGreen )

{
    return CE_None;
}
