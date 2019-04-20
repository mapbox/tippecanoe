
// CLASSIFICATION: UNCLASSIFIED

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//   File name: egm2008_aoi_grid_package.cpp                                  //
//                                                                            //
//   Description of this module:                                              //
//      Utility software that interpolates EGM 2008 geoid                     //
//      heights from one of NGA's reformatted geoid height grids.             //
//                                                                            //
//      This interpolator uses Area of Interest (AOI)                         //
//      geoid height grids, not the worldwide geoid height grid.              //
//                                                                            //
//      This interpolator does not load an Area of Interest (AOI)             //
//      geoid height grid until a user first requests a geoid height.         //
//      The interpolator then loads an AOI grid centered near the point of    // 
//      interest, and it interpolates local geoid height from the AOI grid.   //
//      This interpolator re-uses the AOI grid until a subsequent point of    //
//      interest lies outside the AOI.  The interpolator then loads a         //
//      new AOI grid centered at the new horizontal location of interest.     //
//                                                                            //
//      This interpolator gives exactly the same results as                   //
//      the companion egm2008_full_grid_package's interpolator.               //
//      However, the AOI interpolator requires far less computer memory.      //
//                                                                            //
//   Revision History:                                                        //
//   Date         Name          Description                                   //
//   -----------  ------------  ----------------------------------------------//
//   19 Nov 2010  RD Craig      Release                                       //
//   27 Jan 2011  S. Gillis     BAEts28121, Terrain Service rearchitecture    //
//   11 Feb 2011  RD Craig      Updates following code review                 //
//   30 May 2013  RD Craig      MSP 1.3: ER29758                              //
//                              Added second constructor to                   //
//                              permit multiple geoid-height grids            //
//                              when assessing relative interpolation errors. //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
  
// This file contains definitions
// for functions in the Egm2008AoiGrid class.

#include <fstream>

#ifdef IRIXN32
#include <math.h>
#else
#include <cmath>
#endif

#include "CCSThreadLock.h"
#include "CoordinateConversionException.h"

#include "egm2008_aoi_grid_package.h"

namespace 
{
   const int    BYTES_PER_DOUBLE = sizeof( double );
   const int    BYTES_PER_FLOAT  = sizeof( float );
   const int    BYTES_PER_INT    = sizeof( int );

   const int    BYTES_IN_HEADER  = 
                   ( 3 * BYTES_PER_INT + 2 * BYTES_PER_DOUBLE );

   const int    NOM_AOI_COLS1    = 100;  // nominal # columns in 1' x 1' AOI grid 
   const int    NOM_AOI_ROWS1    = 100;  // nominal # rows    in 1' x 1' AOI grid
   const int    NOM_AOI_COLS2_5  =  50;  // nominal # columns in 2.5' x 2.5' AOI grid 
   const int    NOM_AOI_ROWS2_5  =  50;  // nominal # rows    in 2.5' x 2.5' AOI grid

   const double PI               = 3.14159265358979323;

   const double PIDIV2           = PI / 2.0;
   const double TWOPI            = 2.0 * PI;

   const double DEG_PER_RAD      = 180.0 / PI;
   const double RAD_PER_DEG      = PI / 180.0;

   const double MTR_PER_NM       = 1852.0;                 // meters per International nautical mile

   const double EPSILON          = 1.0e-15;  // ~4 times machine epsilon

   const double SEMI_MAJOR_AXIS  = 6378137.0;              // WGS-84
   const double FLATTENING       = 1.0 / 298.257223563;    // WGS-84
}

using namespace MSP;

// ***************************
// ** Public user functions **
// ***************************

// **************************************
// * Default Egm2008AoiGrid constructor *
// **************************************

Egm2008AoiGrid::Egm2008AoiGrid( void )

// : Egm2008GeoidGrid()                            // base class initializer
{
   // November  19, 2010: Version 1.00
   // February  11, 2011: Version 1.01

   int     status;

   // This function implements the
   // default Egm2008AoiGrid constructor.

   // The base class constructor
   // initialized most data members.

   // Initialize AOI grid parameters .....

   _minAoiRowIndex =  10000000; // these invalid values
   _maxAoiRowIndex = -10000000; // will force the first 
   _minAoiColIndex =  10000000; // geoid height request
   _maxAoiColIndex = -10000000; // to load a new AOI grid

   _nAoiCols       = 0;
   _nAoiRows       = 0;

   _nomAoiCols     = 0;
   _nomAoiRows     = 0;

   _heightGrid     = NULL;

   // Get worldwide grid's metadata .....

   status          = this->loadGridMetadata();

   if ( status != 0 )
   {
      throw MSP::CCS::CoordinateConversionException(
         "Error: Egm2008AoiGrid: constructor failed." );
   }

}  // End of Egm2008AoiGrid constuctor


// ******************************************
// * Non-default Egm2008AoiGrid constructor *
// ******************************************

Egm2008AoiGrid::Egm2008AoiGrid( 
   const std::string  &gridFname )              // input

: Egm2008GeoidGrid( gridFname )                 // base class initializer
{
   // November  19, 2010: Version 1.00
   // February  11, 2011: Version 1.01
   // May       30, 2013: Version 2.00

   // Definition:

   // gridFname:             The support geoid-height grid's file name; this
   //                        file name should not contain the directory path;
   //                        the base-class constructor will prepend the 
   //                        path specified by environment variable MSPCCS_DATA.

   int     status;

   // This function implements the
   // non-default Egm2008AoiGrid constructor.

   // The base class constructor
   // initialized most data members.

   // Initialize AOI grid parameters .....

   _minAoiRowIndex =  10000000; // these invalid values
   _maxAoiRowIndex = -10000000; // will force the first 
   _minAoiColIndex =  10000000; // geoid height request
   _maxAoiColIndex = -10000000; // to load a new AOI grid

   _nAoiCols       = 0;
   _nAoiRows       = 0;

   _nomAoiCols     = 0;
   _nomAoiRows     = 0;

   _heightGrid     = NULL;

   // Get worldwide grid's metadata .....

   status          = this->loadGridMetadata();

   if ( status != 0 )
   {
      throw MSP::CCS::CoordinateConversionException(
         "Error: Egm2008AoiGrid: constructor failed." );
   }

}  // End of non-default Egm2008AoiGrid constuctor


// ***********************************
// * Egm2008AoiGrid copy constructor *
// ***********************************

Egm2008AoiGrid::Egm2008AoiGrid 
   ( const Egm2008AoiGrid&  oldGrid )       // input

: Egm2008GeoidGrid( oldGrid )               // base class initializer

{
   // November  19, 2010: Version 1.00

   // This function implements 
   // the Egm2008AoiGrid copy constructor.

   // Definition:

   // oldGrid:            The Egm2008AoiGrid object being copied.

   int     i;
   int     kount;

   // Copy AOI grid's parameters .....

   _maxAoiColIndex   = oldGrid._maxAoiColIndex;
   _minAoiColIndex   = oldGrid._minAoiColIndex;

   _maxAoiRowIndex   = oldGrid._maxAoiRowIndex;
   _minAoiRowIndex   = oldGrid._minAoiRowIndex;

   _nAoiCols         = oldGrid._nAoiCols;
   _nAoiRows         = oldGrid._nAoiRows;

   _nomAoiCols       = oldGrid._nomAoiCols;
   _nomAoiRows       = oldGrid._nomAoiRows;

   _heightGrid       = NULL;

   // Copy AOI grid's geoid separations .....

   // (the AOI geoid height grid 
   //  will not have been loaded if copying
   //  precedes first geoid height interpolation)

   try
   {
      if ( NULL != oldGrid._heightGrid )
      {
         kount          = _nAoiRows * _nAoiCols;

         _heightGrid    = new float[ kount ];

         for ( i = 0; i < kount; i++ )
         {
            _heightGrid[ i ] = oldGrid._heightGrid[ i ];
         }
      }
   }
   catch (...)
   {
      oldGrid._mutex.unlock();

      throw MSP::CCS::CoordinateConversionException(
         "Error: Egm2008AoiGrid: copy contructor failed");
   }

   oldGrid._mutex.unlock();  // Use CCSThreadMutex in copy constructors

}  // End of Egm2008AoiGrid copy constuctor


// *****************************
// * Egm2008AoiGrid destructor *
// *****************************

Egm2008AoiGrid::~Egm2008AoiGrid (void)
{
   // November  19, 2010: Version 1.00

   // This function implements 
   // the Egm2008AoiGrid destructor. 

   delete[] _heightGrid;

}  // End of Egm2008AoiGrid destructor


// **************************************
// * Egm2008AoiGrid assignment operator *
// **************************************

Egm2008AoiGrid&
Egm2008AoiGrid::operator= ( const Egm2008AoiGrid&  oldGrid )
{
   // November  19, 2010: Version 1.00

   // Definition:

   // oldGrid:            The Egm2008AoiGrid object being assigned.

   int     i;
   int     kount;

   // This function implements 
   // the Egm2008AoiGrid assignment operator.

   if ( this == & oldGrid )                      return ( *this );

   // Assign most base class data members .....

   Egm2008GeoidGrid::operator= ( oldGrid );

   // Assign AOI grid parameters .....

   _maxAoiColIndex   = oldGrid._maxAoiColIndex;
   _minAoiColIndex   = oldGrid._minAoiColIndex;

   _maxAoiRowIndex   = oldGrid._maxAoiRowIndex;
   _minAoiRowIndex   = oldGrid._minAoiRowIndex;

   _nAoiCols         = oldGrid._nAoiCols;
   _nAoiRows         = oldGrid._nAoiRows;

   _nomAoiCols       = oldGrid._nomAoiCols;
   _nomAoiRows       = oldGrid._nomAoiRows;

   // Assign AOI grid's geoid separations .....

   // (the AOI geoid height grid will
   //  not have been loaded if assignment 
   //  precedes first geoid height interpolation)

   try
   {
      delete[] _heightGrid;    _heightGrid = NULL;

      if ( NULL != oldGrid._heightGrid )
      {
         kount          = _nAoiRows * _nAoiCols;

         _heightGrid    = new float[ kount ];

         for ( i = 0; i < kount; i++ )
         {
            _heightGrid[ i ] = oldGrid._heightGrid[ i ];
         }
      }
   }
   catch (...)
   {
      _mutex.unlock();
      oldGrid._mutex.unlock();

      throw MSP::CCS::CoordinateConversionException(
         "Error: Egm2008AoiGrid: assignment operator failed");
   }

   _mutex.unlock();  // Use CCSThreadMutex function in assignment operations
   oldGrid._mutex.unlock();

   return( *this );

}  // End of Egm2008AoiGrid assignment operator


// *************************************************
// * Find geoid height via 2D spline interpolation *
// *************************************************

int   
Egm2008AoiGrid::geoidHeight(
   int     wSize,                           // input
   double  latitude,                        // input
   double  longitude,                       // input
   double& gHeight )                        // output
{
   // November 19, 2010: Version 1.00
   // February 11, 2011: Version 1.01

   // This function interpolates 
   // geoid heights from an AOI grid,
   // which, in turn, was extracted from a
   // reformatted version of NGA's worldwide grid.
   // It primarily uses BICUBIC-SPLINE INTERPOLATION, 
   // but it uses bilinear interpolation for small windows.
 
   // Definitions:

   // gHeight:               Interpolated geoid height (meters).
   // latitude:              Geodetic latitude (radians) at 
   //                        which geoid height is to be interpolated. 
   // longitude:             Geodetic longitude (radians) at
   //                        which geoid height is to be interpolated.
   // wSize:                 The number of grid points 
   //                        along each edge of the interpolation
   //                        window that surrounds the point of interest;
   //                        the interpolation window has wSize**2 grid points;
   //                        if wSize < 3, then the geoid height 
   //                        is computed using bilinear interpolation; the
   //                        maximum allowable wSize is twenty grid points;
   //                        typical window sizes are 4x4, 6x6, 8,8 etc.

   // i:                     Worldwide grid's latitude  index. 
   // j:                     Worldwide grid's longitude index.
   // oddSize:               A flag indicating whether the 
   //                        local interpolation grid's number
   //                        of (rows = column) is even or odd;
   //                        oddSize = true  ..... 
   //                           the number of rows = columns is odd,
   //                           where 3x3, 5x5, 7x7, 9x9, etc are
   //                           examples of grids with oddSize = true,
   //                        oddSize = false ..... 
   //                           the number of rows = columns is even,
   //                           where 4x4, 6x6, 8x8, 10x10, etc are
   //                           examples of grids with oddSize = false.
   //                        The associated indexing logic has two goals:
   //                        2) if oddSize = true, select the 
   //                           the local grid so that the point of
   //                           interest lies near the grid's center,
   //                        2) if oddSize = false, select the 
   //                           the local grid so that the point of
   //                           interest lies in the grid's central cell.
   //                        When oddSize = false, the interpolator will
   //                        obtain a numerically-most-trustworthy solution.

   try {

      MSP::CCSThreadLock  aLock( &_mutex );  // copy constructor locks thread

      const int           TWENTY = 20;

      bool                oddSize;

      int                 i;
      int                 i0;
      int                 iIndex;
      int                 iMax;
      int                 iMin;
      int                 j;
      int                 j0;
      int                 jIndex;
      int                 jMax;
      int                 jMin;
      int                 kIndex;
      int                 offset;
      int                 status;

      double              latIndex;
      double              lonIndex;
      double              temp;

      double              latSupport[ TWENTY ];
      double              lonSupport[ TWENTY ];
      double              moments   [ TWENTY ];

      // EDIT THE INPUT AND INITIALIZE .....

      gHeight = 0.0;

      if ( wSize > MAX_WSIZE ) wSize = MAX_WSIZE;

      if (( latitude < -PIDIV2 ) || ( latitude > PIDIV2 )) return( 1 );

      while ( longitude <   0.0 ) longitude += TWOPI;
      while ( longitude > TWOPI ) longitude -= TWOPI;

      if ( TWENTY != MAX_WSIZE )                           return( 1 );

      for (i = 0; i < TWENTY; i++)
      {
         latSupport[ i ] = lonSupport[ i ] = moments[ i ] = 0.0;
      }

      // Determine point indices
      // (relative to worldwide grid) .....

      latIndex = 
         double( _nGridPad ) + 
            ( latitude  + PIDIV2 ) / _dLat;
      lonIndex = 
         double( _nGridPad ) + 
            ( longitude -    0.0 ) / _dLon;

      // Determine interpolation window 
      // indices (relative to worldwide grid) .....

      oddSize  = ( wSize != (( wSize / 2 ) * 2 ));

      if ( oddSize ) {
        i0   = int( latIndex + 0.5 );
        j0   = int( lonIndex + 0.5 );

        iMin = i0 - ( wSize / 2 );
        jMin = j0 - ( wSize / 2 );
      }
      else { 
        i0   = int( latIndex );
        j0   = int( lonIndex );

        iMin = i0 - ( wSize / 2 ) + 1;
        jMin = j0 - ( wSize / 2 ) + 1;
      }

     iMax      = iMin + wSize - 1;
     jMax      = jMin + wSize - 1;

     // Compare interpolation 
     // window's limits to AOI grid's 
     // limits, and reload AOI grid if required .....

     if (( iMin < _minAoiRowIndex ) ||
         ( iMax > _maxAoiRowIndex ) ||
         ( jMin < _minAoiColIndex ) ||
         ( jMax > _maxAoiColIndex ))
     {
        status = this->loadAoiParms( i0, j0 );

        if ( status != 0 )                                 return( 1 ); 

        status = this->loadGrid();

        if ( status != 0 )                                 return( 1 );
     }

     // COMPUTE BILINEAR INTERPOLATION .....

     // (executes only when the interpolation window
     //  is too small for bicubic spline interpolation)

     if ( wSize < 3 )        
     {
        status = 
           this->geoidHeight( latitude, longitude, gHeight );

        if ( status != 0 )                                 return( 1 );

        ;    /* Normal bilinear interpolation return */    return( 0 );
     }

     // COMPUTE BI-CUBIC SPLINE INTERPOLATION .....

     // Compute interpolation window's 
     // relative column coordinate at which the
     // first set of geoid heights will be interpolated ....

     temp    = 
        lonIndex - double( jMin ); // 0 <= temp <= (wSize - 1)

     // Interpolate a synthetic geoid height 
     // for each row within the interpolation window .....

     for ( i = 0; i < wSize; i++ ) 
     {
        iIndex = iMin + i - _minAoiRowIndex;              // AOI referenced

        offset = ( _nAoiRows - iIndex - 1 ) * _nAoiCols;

        // Load interpolation window's 
        // i-th row of tabulated geoid heights ..... 

        for ( j = 0; j < wSize; j++ ) 
        {
           jIndex          = jMin + j - _minAoiColIndex;  // AOI referenced

           kIndex          = jIndex + offset;  

           lonSupport[ j ] = _heightGrid[ kIndex ];
        }

        // Compute moments, 
        // interpolate the i-th row's geoid 
        // height at the longitude of interest, 
        // then load the i-th latitude support point .....

        status          = 
           this->initSpline( wSize, lonSupport, moments );

        if ( status != 0 )                                 return( 1 ); 

        latSupport[ i ] = 
           this->spline( wSize, temp, lonSupport, moments ); 
     }

     // Compute interpolation window's 
     // relative row coordinate at which the
     // final geoid height will be interpolated .....

     temp     = 
        latIndex - double( iMin );  // 0 <= temp <= (wSize - 1)

     // Compute moments, and interpolate final 
     // geoid height at the latitude of interest .....

     status   = 
        this->initSpline( wSize, latSupport, moments ); 

     if ( status != 0 )                                    return( 1 ); 

     gHeight  = 
        this->spline( wSize, temp, latSupport, moments );

     // Thread is unlocked by aLock's destructor;
     // this occurs even when exceptions are thrown.

   } // End of exceptions' try block

   catch ( ... ) {                                         return( 1 ); }

   return ( 0 );  // Normal-return flag

}  // End of function Egm2008AoiGrid::geoidHeight


// **********************
// ** Hidden functions **
// **********************

// ************************************** 
// * Interpolate EGM 2008 geoid heights * 
// ************************************** 
 
int
Egm2008AoiGrid::geoidHeight(
   double  latitude,                        // input
   double  longitude,                       // input
   double& gHeight )                        // output
{ 
   // November  19, 2010: Version 1.00
 
   // This function, which is
   // exercised only when wSize < 3, uses
   // bilinear interpolation to find geoid heights.

   // Definitions:  

   // gHeight:               Interpolated geoid height (meters).
   // latitude:              GEODETIC latitude at which 
   //                        nominal geoid height is to be computed. 
   // longitude:             GEODETIC longitude at which 
   //                        nominal geoid height is to be computed. 

   // wSize:                 The local interpolation window's size:
   //                        normally wSize = 6 for EGM 2008 interpolations.

   // Thread locks are not needed here, because this function
   // can only be invoked by the bicubic spline's geoidHeight function; 
   // the thread locks reside in the bicubic spline's geoidHeight function.

   try {

      int           i1;
      int           i1ww;
      int           i2;
      int           i3;
      int           i4;
      int           index;
      int           j1;
      int           j1ww;
      int           j2;
      int           j3;
      int           j4;
      int           status;

      double        a0;
      double        a1;
      double        a2;
      double        a3;
      double        lat1;
      double        lon1;
      double        n1;
      double        n2;
      double        n3;
      double        n4;
      double        x;
      double        y;

      // EDIT THE INPUT AND INITIALIZE .....

      gHeight = 0.0;

      if (( latitude < -PIDIV2 ) || ( latitude > PIDIV2 )) return( 1 );

      while ( longitude <   0.0 ) longitude += TWOPI;
      while ( longitude > TWOPI ) longitude -= TWOPI;

      // Compute the surrounding 
      // points' worldwide grid indices .....

      status  = 
         this->swGridIndices
            ( latitude, longitude, i1, j1 );

      if ( status != 0 )                                   return( 1 ); 

      i1ww    = i1;  // save worldwide index
      j1ww    = j1;  // save worldwide index

      i2      = i1;
      j2      = j1 + 1;

      i3      = i1 + 1;
      j3      = j2;

      i4      = i3;
      j4      = j1;

      // Compute the 
      // corresponding AOI grid indices .....

      i1     -= _minAoiRowIndex;
      j1     -= _minAoiColIndex;

      i2     -= _minAoiRowIndex;
      j2     -= _minAoiColIndex;

      i3     -= _minAoiRowIndex;
      j3     -= _minAoiColIndex;

      i4     -= _minAoiRowIndex;
      j4     -= _minAoiColIndex;

      // GET THE SURROUNDING GRID POINTS' GEOID HEIGHTS .....

      index   = 
         j1 + ( _nAoiRows - i1 - 1 ) * _nAoiCols;
      n1      = _heightGrid[ index ];

      index   = 
         j2 + ( _nAoiRows - i2 - 1 ) * _nAoiCols;
      n2      = _heightGrid[ index ];

      index   = 
         j3 + ( _nAoiRows - i3 - 1 ) * _nAoiCols;
      n3      = _heightGrid[ index ];

      index   = 
         j4 + ( _nAoiRows - i4 - 1 ) * _nAoiCols;
      n4      = _heightGrid[ index ];

      // INTERPOLATE GEOID HEIGHT AT THE POINT OF INTEREST .....

      // Set bilinear coefficients .....

      a0      = n1;
      a1      = n2 - n1;
      a2      = n4 - n1;
      a3      = n1 + n3 - n2 - n4;

      // Set first ( S/W ) corner's geodetic coordinates .....

      lat1    = 
         _baseLatitude  + _dLat * double( i1ww ); // radians
      lon1    = 
         _baseLongitude + _dLon * double( j1ww ); // radians

      // Set point coordinates relative
      // to the grid square's S/W corner .....

      // 0 <= x <= 1;             0 <= y <= 1.

      x       = ( longitude - lon1 ) / _dLon;
      y       = ( latitude  - lat1 ) / _dLat;

      // Perform the interpolation .....

      gHeight = a0 + a1 * x + a2 * y + a3 * x * y;   // meters

   }  // End of exceptions' try block

   catch ( ... ) {                                         return( 1 ); }

   return( 0 );  // Normal-return flag
 
} //  End of function Egm2008AoiGrid::geoidHeight 


// ************************************
// * Compute new AOI's new parameters *
// ************************************

int
Egm2008AoiGrid::loadAoiParms(
   int     i0,                              // input
   int     j0 )                             // input
{
   // November 19, 2010: Version 1.00
   // February 11, 2011: Version 2.00:
   // the function's name was changed following code review

   // This function determines 
   // current AOI grid parameters.

   // Definitions;

   // i0:                    Worldwide row index
   //                        for geoid height post that's
   //                        near the ground point of interest.
   // j0:                    Worldwide column index
   //                        for geoid height post that's
   //                        near the ground point of interest.

   // THRESHOLD:             Compute the E/W horizontal 
   //                        distance between grid columns
   //                        a) at the equator (ewDelta0) and
   //                        b) at the latitude of interest (ewDelta);
   //                        then form the ratio ( ewDelta / ewDelta0 );
   //                        THRESHOLD is the minimum allowable E/W ratio,
   //                        where ratio controls the # of AOI grid columns.

   // Thread locks are not needed here, because this function
   // can only be invoked by the bicubic spline geoidHeight function;
   // the thread locks reside in the bicubic spline's geoidHeight function.

   try {

      const double  THRESHOLD = 0.05;  // unitless

      int           status;

      double        cLat;
      double        eSquared; 
      double        ewDelta;
      double        ewDelta0;
      double        latitude;
      double        longitude;
      double        nRadius;
      double        ratio;
      double        sLat;
      double        temp;

      // INITIALIZE .....

      // Compute latitude & longitude
      // corresponding to indices i0 & j0 .....

      status     = 
         this->loadGridCoords(
            i0, j0, latitude, longitude );

      if ( status != 0 )                                   return( 1 );

      cLat       = cos( latitude );
      sLat       = sin( latitude );

      // Compute earth ellipsoid's
      // radius of curvature in the prime vertical .....

      eSquared   = FLATTENING * ( 2.0 - FLATTENING );

      temp       = 1.0 - eSquared * sLat * sLat;

      nRadius    = SEMI_MAJOR_AXIS / sqrt( temp );

      // Set AOI grid's number of rows and columns .....

      ewDelta0   = SEMI_MAJOR_AXIS * _dLon;

      ewDelta    = nRadius * _dLon * cLat;

      ratio      = ewDelta / ewDelta0;

      if ( ratio < THRESHOLD ) ratio = THRESHOLD;

      _nAoiCols  = int( double( _nomAoiCols ) / ratio ); 

      _nAoiCols  = 2 * ( _nAoiCols / 2 );    // These both
      _nAoiRows  = 2 * ( _nomAoiRows / 2 );  // become even numbers

      // SET AOI GRID'S ABSOLUTE LOCATION .....

      // (indices are referenced to the worldwide grid)

      _minAoiRowIndex = i0 - (( _nAoiRows - 2 ) / 2 ) + 1;
      _maxAoiRowIndex = _minAoiRowIndex + _nAoiRows - 1;

      _minAoiColIndex = j0 - (( _nAoiCols - 2 ) / 2 ) + 1;
      _maxAoiColIndex = _minAoiColIndex + _nAoiCols - 1;

      // MOVE AOI GRID IF IT EXTENDS
      // BEYOND A WORLDWIDE GRID BOUNDARY .....

      if ( _minAoiRowIndex < 0 ) 
      {
         _minAoiRowIndex = 0;
         _maxAoiRowIndex = _minAoiRowIndex + _nAoiRows - 1;
      }

      if ( _maxAoiRowIndex >= _nGridRows ) 
      {
         _maxAoiRowIndex = _nGridRows - 1;
         _minAoiRowIndex = _maxAoiRowIndex - _nAoiRows + 1;
      }

      if ( _minAoiColIndex < 0 )
      {
         _minAoiColIndex = 0;
         _maxAoiColIndex = _minAoiColIndex + _nAoiCols - 1;
      }

      if ( _maxAoiColIndex >= _nGridCols ) 
      {
         _maxAoiColIndex = _nGridCols - 1;
         _minAoiColIndex = _maxAoiColIndex - _nAoiCols + 1;
      }

   }  // End of exception's try block

   catch ( ... ) {                                         return( 1 ); }

   return( 0 );  // Normal return flag

}  // End of function Egm2008AoiGrid::loadAoiParms


// ******************************
// *  Read reformatted version  * 
// * of NGA's geoid-height grid *
// ******************************

int
Egm2008AoiGrid::loadGrid( void )
{
   // November  19, 2010: Version 1.00
   // February  11, 2011: Version 2.00:
   // the function's name was changed following code review

   // This function reads 
   // AOI geoid heights from NGA's
   // reformatted EGM 2008 geoid height file.

   // The grid data is arranged in latitude rows, 
   // with the northernmost rows first, and with the
   // geoid heights arranged west-to-east within each row. 

   // Thread locks are not needed here, because this function
   // can only be invoked by the bicubic spline geoidHeight function;
   // the thread locks reside in the bicubic spline's geoidHeight function.

   try {

      int            endRow;
      int            i;
      int            index0;
      int            index1;
      int            index2;
      int            kount;
      int            startCol;
      int            startRow;

      std::ifstream  fin;

      // INITIALIZE .....

      fin.open( 
         _gridFname.c_str(), 
            std::ifstream::binary | std::ifstream::in ); 

      if ( fin.fail() )                                    return( 1 );

      delete[] _heightGrid;    _heightGrid = NULL;

      _heightGrid = new float[ _nAoiRows * _nAoiCols ];

      if ( NULL == _heightGrid )                           return ( 1 );  

      // READ THE AOI GRID's GEOID HEIGHTS .....

      index0      = 0;       // Starting AOI grid locaion

      startRow    = _maxAoiRowIndex;  
      endRow      = _minAoiRowIndex;

      index1      =          // Starting worldwide file location (bytes)
         BYTES_IN_HEADER + 
            BYTES_PER_FLOAT *
               ( _minAoiColIndex +
                  ( _nGridRows - startRow - 1 ) * _nGridCols );

      for ( i = startRow; i >= endRow; i-- ) 
      {
         // Read row's geoid heights .....

         fin.seekg( index1 );

         fin.read( 
            (char*) ( &_heightGrid[ index0 ] ), 
               ( _nAoiCols * BYTES_PER_FLOAT ));

         if ( fin.fail() ) { fin.close();                  return( 1 ); }

         // If needed, convert to LITTLE-ENDIAN format .....

         #if LITTLE_ENDIAN

            this->swapBytes( 
               &_heightGrid[ index0 ], 
               BYTES_PER_FLOAT, _nAoiCols ); 

         #endif
         
         index0 += _nAoiCols;
         index1 += ( BYTES_PER_FLOAT * _nGridCols );
      }

      fin.close();

   }  // End of exceptions' try block

   catch ( ... ) {                                         return( 1 ); }

   return ( 0 );  // Normal-return flag

}  // End of function Egm2008AoiGrid::loadGrid


// *******************************
// * Read header's metadata from *
// *     reformatted version     * 
// * of NGA's  geoid-height grid *
// *******************************

int
Egm2008AoiGrid::loadGridMetadata( void )
{
   // November  19, 2010: Version 1.00
   // February  11, 2011: Version 2.00:
   //    the function's name was changed following code review
   // June       4, 2013: Version 2.01

   // This function reads 
   // the grid metadata from NGA's 
   // reformatted EGM 2008 geoid height file. 

   // THE GRID METADATA OCCUPIES 28 BYTES AT THE
   // BEGINNING OF THE REFORMATTED WORLDWIDE FILE.

   // Thread locks are not needed here, because this
   // function can only be invoked by the constructor;
   // thread locks / unlocks reside in the constructors.

   try {

      double         ewDelta;

      std::ifstream  fin;

      // INITIALIZE .....

      fin.open( 
         _gridFname.c_str(), 
            std::ifstream::binary | std::ifstream::in ); 

      if ( fin.fail() ) {                                  return( 1 ); }

      // READ AND STORE HEADER .....

      fin.read((char*) &_nGridPad,  BYTES_PER_INT );

      if ( fin.fail() ) { fin.close();                     return( 1 ); }

      fin.read((char*) &_nOrigRows, BYTES_PER_INT );

      if ( fin.fail() ) { fin.close();                     return( 1 ); }

      fin.read((char*) &_nOrigCols, BYTES_PER_INT );

      if ( fin.fail() ) { fin.close();                     return( 1 ); }

      fin.read((char*) &_dLat,      BYTES_PER_DOUBLE );

      if ( fin.fail() ) { fin.close();                     return( 1 ); }

      fin.read((char*) &_dLon,      BYTES_PER_DOUBLE );

      if ( fin.fail() ) { fin.close();                     return( 1 ); }

      fin.close();

      #if LITTLE_ENDIAN

         this->swapBytes( &_nGridPad,  BYTES_PER_INT,    1 );
         this->swapBytes( &_nOrigRows, BYTES_PER_INT,    1 );
         this->swapBytes( &_nOrigCols, BYTES_PER_INT,    1 );
         this->swapBytes( &_dLat,      BYTES_PER_DOUBLE, 1 );
         this->swapBytes( &_dLon,      BYTES_PER_DOUBLE, 1 );

      #endif

      _dLat         *= RAD_PER_DEG;  // grid file stores these in degrees
      _dLon         *= RAD_PER_DEG;  // grid file stores these in degrees

      // SET DERIVED METADATA .....

      _nGridRows     = _nOrigRows + ( 2 * _nGridPad );
      _nGridCols     = _nOrigCols + ( 2 * _nGridPad ) + 1;

      ewDelta        = _dLon * SEMI_MAJOR_AXIS;

      _nomAoiCols    = 1 + int( 150.0 * MTR_PER_NM / ewDelta );

      if( _nomAoiCols <            25 ) _nomAoiCols =  25;
      if( _nomAoiCols > _nOrigCols/40 ) _nomAoiCols = _nOrigCols / 40;

      _nomAoiRows    = _nomAoiCols;

      _baseLatitude  =
         -PIDIV2 - _dLat * double( _nGridPad );     // radians
      _baseLongitude =
             0.0 - _dLon * double( _nGridPad );     // radians

   }  // End of exceptions' try block

   catch ( ... ) {                                         return( 1 ); }

   return( 0 );  // Normal-return flag

}  // End of function Egm2008AoiGrid::loadGridMetadata

// CLASSIFICATION: UNCLASSIFIED

