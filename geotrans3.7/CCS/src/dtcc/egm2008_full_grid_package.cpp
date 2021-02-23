
// CLASSIFICATION: UNCLASSIFIED

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//   File name: egm2008_full_grid_package.cpp                                 //
//                                                                            //
//   Description of this module:                                              //
//      Utility software that interpolates EGM 2008                           //
//      geoid heights from one of NGA's geoid height grids.                   //
//                                                                            //
//      This interpolator loads the worldwide EGM 2008 grid upon              //
//      instantiation, and it interpolates from the worldwide grid.           //
//                                                                            //
//      This interpolator gives exactly the same results as                   //
//      the companion egm2008_aoi_grid_package's interpolator.                //
//      However, this interpolator is faster when                             //
//      users are requesting tens of thousands of geoid                       //
//      heights at widely dispersed horizontal locations.                     //
//                                                                            //
//   Revision History:                                                        //
//   Date         Name          Description                                   //
//   -----------  ------------  ----------------------------------------------//
//   19 Nov 2010  RD Craig      Release                                       //
//   27 Jan 2011  S. Gillis     BAEts28121, Terrain Service rearchitecture    //
//   30 May 2013  RD Craig      MSP 1.3: ER29758                              //
//                              Added second constructor to                   //
//                              permit multiple geoid-height grids            //
//                              when assessing relative interpolation errors. //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
  
// This file contains definitions
// for functions in the Egm2008FullGrid class.

#include <fstream>

#ifdef IRIXN32
#include <math.h>
#else
#include <cmath>
#endif

#include "CCSThreadLock.h"
#include "CoordinateConversionException.h"

#include "egm2008_full_grid_package.h"

using namespace MSP;

namespace 
{
   const int    BYTES_PER_DOUBLE = sizeof( double );
   const int    BYTES_PER_FLOAT  = sizeof( float );
   const int    BYTES_PER_INT    = sizeof( int );

   const double EPSILON          = 1.0e-15;  // ~4 times machine epsilon

   const double PI               = 3.14159265358979323;

   const double PIDIV2           = PI / 2.0;
   const double TWOPI            = 2.0 * PI; 

   const double DEG_PER_RAD      = 180.0 / PI;
   const double RAD_PER_DEG      = PI / 180.0;
}

// ***************************
// ** Public user functions **
// ***************************

// ***************************************
// * Default Egm2008FullGrid constructor *
// ***************************************

Egm2008FullGrid::Egm2008FullGrid( void )

// : Egm2008GeoidGrid()                            // base class initializer
{
   // November  19, 2010: Version 1.00
   // February  11, 2011: Version 1.01

   // This function implements the 
   // default Egm2008FullGrid constructor.

   int     status;

   // The base class constructor
   // initialized most data members.

   // Initialize grid pointer for proper
   // operation of derived-class functions .....

   _heightGrid     = NULL; 

   // Read entire worldwide EGM 2008 grid here .....

   status          = this->loadGrid();

   if ( status != 0 )
   {
      throw MSP::CCS::CoordinateConversionException(
         "Error: Egm2008GeoidGrid: constructor failed.");
   }

}  // End of default Egm2008FullGrid constuctor


// *******************************************
// * Non-default Egm2008FullGrid constructor *
// *******************************************

Egm2008FullGrid::Egm2008FullGrid( 
   const std::string  &gridFname )              // input

: Egm2008GeoidGrid( gridFname )                 // base class initializer
{
   // November  19, 2010: Version 1.00
   // February  11, 2011: Version 1.01
   // May       30, 2013: Version 2.00

   // This function implements a 
   // non-default Egm2008FullGrid constructor.

   // Definition:

   // gridFname:             The support geoid-height grid's file name; this
   //                        file name should not contain the directory path;
   //                        the base-class constructor will prepend the 
   //                        path specified by environment variable MSPCCS_DATA.

   int     status;

   // The base class constructor
   // initialized most data members.

   // Initialize grid pointer for proper
   // operation of derived-class functions .....

   _heightGrid     = NULL; 

   // Read entire worldwide EGM 2008 grid here .....

   status          = this->loadGrid();

   if ( status != 0 )
   {
      throw MSP::CCS::CoordinateConversionException(
         "Error: Egm2008GeoidGrid: constructor failed.");
   }

}  // End of default Egm2008FullGrid constuctor


// ************************************
// * Egm2008FullGrid copy constructor *
// ************************************

Egm2008FullGrid::Egm2008FullGrid 
   ( const Egm2008FullGrid&  oldGrid )          // input

: Egm2008GeoidGrid( oldGrid )                   // base class initializer

{
   // November 19, 2010: Version 1.00

   int     i;
   int     kount;

   // This function implements the
   // Egm2008FullGrid copy constructor.

   // The base class copy 
   // constructor handles many data members .....

   // Definition:

   // oldGrid:            The Egm2008FullGrid object being copied.

   // Copy the worldwide geoid separations .....

   try
   {
      _heightGrid       = NULL;

      kount             = _nGridRows * _nGridCols;

      _heightGrid       = new float[ kount ];

      for ( i = 0; i < kount; i++ )
      {
         _heightGrid[ i ] = oldGrid._heightGrid[ i ];
      }
   }
   catch (...)
   {
      oldGrid._mutex.unlock();

      throw MSP::CCS::CoordinateConversionException(
         "Error: Egm2008GeoidGrid: copy contructor failed");
   }

   oldGrid._mutex.unlock();  // Use CCSThreadMutex function in copy constructors

}  // End of Egm2008FullGrid copy constuctor


// ******************************
// * Egm2008FullGrid destructor *
// ******************************

Egm2008FullGrid::~Egm2008FullGrid (void)
{
   // November 19, 2010: Version 1.00

   // This function implements 
   // the Egm2008FullGrid destructor. 

   delete[] _heightGrid;

}  // End of Egm2008FullGrid destructor


// ***************************************
// * Egm2008FullGrid assignment operator *
// ***************************************

Egm2008FullGrid&
Egm2008FullGrid::operator= ( const Egm2008FullGrid&  oldGrid )
{
   // November 19, 2010: Version 1.00

   // This function implements the
   // Egm2008FullGrid assignment operator.

   // Definition:

   // oldGrid:            The Egm2008FullGrid object being assigned.

   int     i;
   int     kount;

   // This function implements 
   // the Egm2008FullGrid assignment operator.

   if ( this == & oldGrid )                                return ( *this );

   // Assign base class data members .....

   Egm2008GeoidGrid::operator= ( oldGrid );

   // Assign the worldwide geoid separations .....

   try
   {
      delete[] _heightGrid;   _heightGrid = NULL;

      kount             = _nGridRows * _nGridCols;

      _heightGrid       = new float[ kount ];

      for ( i = 0; i < kount; i++ )
      {
         _heightGrid[ i ] = oldGrid._heightGrid[ i ];
      }
   }
   catch (...)
   {
      _mutex.unlock();
      oldGrid._mutex.unlock();

      throw MSP::CCS::CoordinateConversionException(
         "Error: Egm2008GeoidGrid: assignment operator failed");
   }

   _mutex.unlock();  // Use CCSThreadMutex function in assignment operations
   oldGrid._mutex.unlock();

   return( *this );

}  // End of Egm2008FullGrid assignment operator


// *************************************************
// * Find geoid height via 2D spline interpolation *
// *************************************************

int   
Egm2008FullGrid::geoidHeight(
   int     wSize,                           // input
   double  latitude,                        // input
   double  longitude,                       // input
   double& gHeight )                        // output
{
   // November 19, 2010: Version 1.00

   // This function computes 
   // geoid heights from a reformatted 
   // version of NGA's worldwide geoid-height grid.
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
   //                        typical window sizes are 4x4, 6x6, 8,8 etc;
   //                        EGM 2008 interpolations normally use wSize = 6.

   // i:                     Grid's latitude  index. 
   // j:                     Grid's longitude index.
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

//    MSP::CCSThreadLock  aLock( &_mutex );  // copy constructor locks thread

      const int           TWENTY = 20;

      bool                oddSize;

      int                 i;
      int                 i0;
      int                 iIndex;
      int                 iMin;
      int                 j;
      int                 j0;
      int                 jIndex;
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

      if ( TWENTY != MAX_WSIZE )                           return( 1 );

      if ( wSize > MAX_WSIZE ) wSize = MAX_WSIZE;

      if (( latitude < -PIDIV2 ) || ( latitude > PIDIV2 )) return( 1 );

      // Rationalize the longitude .....

      while ( longitude <   0.0 ) longitude += TWOPI;
      while ( longitude > TWOPI ) longitude -= TWOPI;

      for (i = 0; i < TWENTY; i++)
      {
         latSupport[ i ] = lonSupport[ i ] = moments[ i ] = 0.0;
      }

      // If window size is less than three, compute
      // the geoid height using bilinear interpolation .....

      // (executes only when the interpolation window 
      //  is too small for bicubic spline interpolation)

      if ( wSize < 3 ) 
      {
         status = 
            this->geoidHeight( latitude, longitude, gHeight );

         if ( status != 0 )                                return( 1 ); 

         ;   /* Normal bilinear interpolation return */    return( 0 );
      }

      // Compute indices to the window's grid points .....

      // Roughly center the 
      // interpolation window at the station .....

      latIndex = 
         double( _nGridPad ) + ( latitude  + PIDIV2 ) / _dLat;
      lonIndex = 
         double( _nGridPad ) + ( longitude -    0.0 ) / _dLon;

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
         iIndex = iMin + i;

         offset = 
            ( _nGridRows - iIndex - 1 ) * _nGridCols;

         // Load interpolation window's 
         // i-th row of tabulated geoid heights ..... 

         for ( j = 0; j < wSize; j++ ) 
         {
            jIndex        = jMin + j;

            kIndex        = jIndex + offset;  

            lonSupport[j] = _heightGrid[ kIndex ];
         }

         // Compute moments, 
         // interpolate the i-th row's geoid 
         // height at the longitude of interest, 
         // then load the i-th latitude support point .....

         status        = 
            this->initSpline( wSize, lonSupport, moments );

         if ( status != 0 )                                return( 1 ); 

         latSupport[i] = 
            this->spline( wSize, temp, lonSupport, moments ); 
      }

      // Compute interpolation window's 
      // relative row coordinate at which the
      // final geoid height will be interpolated .....

      temp    = 
         latIndex - double( iMin );  // 0 <= temp <= (wSize - 1)

      // Compute moments, and interpolate final 
      // geoid height at the latitude of interest .....

      status  = 
         this->initSpline( wSize, latSupport, moments ); 

      if ( status != 0 )                                   return( 1 ); 

      gHeight = 
         this->spline( wSize, temp, latSupport, moments );

      // The thread is unlocked by aLock's destructor;
      // this occurs even when exceptions are thrown.

   }  // End of exceptions' try block

   catch ( ... ) { gHeight = 0.0;                          return( 1 ); }

   return( 0 );  // Normal-return flag

}  // End of function Egm2008FullGrid::geoidHeight


// **********************
// ** Hidden functions **
// **********************

// ************************************** 
// * Interpolate EGM 2008 geoid heights * 
// ************************************** 
 
int   
Egm2008FullGrid::geoidHeight(
   double  latitude,                        // input
   double  longitude,                       // input
   double& gHeight )                        // output
{ 
   // November 19, 2010: Version 1.00
 
   // This function, which is
   // exercised only when wSize < 3, uses
   // bilinear interpolation to find geoid heights.

   // Definitions:  

   // gHeight:               Interpolated geoid height (meters).
   // latitude:              GEODETIC latitude at which nominal
   //                        geoid height is to be computed (radians). 
   // longitude:             GEODETIC longitude at which nominal
   //                        geoid height is to be computed (radians). 

   // wSize:                 The local interpolation window's size:
   //                        normally wSize = 6 for EGM 2008 interpolations.

   // Thread locks are not needed here, because this function
   // can only be invoked from the bicubic spline geoidHeight function; the
   // thread locks, if any, reside in the bicubic spline's geoidHeight function.

   try {

      int           i1;
      int           i2;
      int           i3;
      int           i4;
      int           index;
      int           j1;
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

      // COMPUTE THE SURROUNDING GRID POINTS' INDICES .....

      status  = 
         this->swGridIndices
            ( latitude, longitude, i1, j1 );

      if ( status != 0 )                                   return( 1 ); 

      i2      = i1;
      j2      = j1 + 1;

      i3      = i1 + 1;
      j3      = j2;

      i4      = i3;
      j4      = j1;

      // GET THE SURROUNDING GRID POINTS' GEOID HEIGHTS .....

      index   = 
         j1 + ( _nGridRows - i1 - 1 ) * _nGridCols;
      n1      = _heightGrid[ index ];

      index   = 
         j2 + ( _nGridRows - i2 - 1 ) * _nGridCols;
      n2      = _heightGrid[ index ];

      index   = 
         j3 + ( _nGridRows - i3 - 1 ) * _nGridCols;
      n3      = _heightGrid[ index ];

      index   = 
         j4 + ( _nGridRows - i4 - 1 ) * _nGridCols;
      n4      = _heightGrid[ index ];

      // INTERPOLATE GEOID HEIGHT AT THE POINT OF INTEREST .....

      // Set bilinear coefficients .....

      a0      = n1;
      a1      = n2 - n1;
      a2      = n4 - n1;
      a3      = n1 + n3 - n2 - n4;

      // Set first ( S/W ) corner's geodetic coordinates .....

      lat1    = 
         _baseLatitude  + _dLat * double( i1 ); // radians
      lon1    = 
         _baseLongitude + _dLon * double( j1 ); // radians

      // Set point coordinates relative
      // to the grid square's S/W corner .....

      // 0 <= x <= 1;             0 <= y <= 1.

      x       = ( longitude  - lon1 ) / _dLon;
      y       = ( latitude   - lat1 ) / _dLat;

      // Perform the interpolation .....

      gHeight = a0 + a1 * x + a2 * y + a3 * x * y;   // meters

   }  // End of exceptions' try block

   catch ( ... ) { gHeight = 0.0;                          return( 1 ); }

   return( 0 );  // Normal-return flag
 
} //  End of function Egm2008FullGrid::geoidHeight 


// ******************************
// *  Read reformatted version  * 
// * of NGA's geoid-height grid *
// ******************************

int
Egm2008FullGrid::loadGrid( void )
{
   // November 19, 2010: Version 1.00
   // February 11, 2011: Version 2.00:
   // the function name changed after code review

   // This function reads a reformatted version
   // of NGA's EGM 2008 worldwide geoid-height grid.

   // The grid data is arranged in latitude rows, 
   // with the northernmost rows first, and with the
   // geoid heights arranged west-to-east within each row. 

   // Thread locks are not needed here, because this 
   // function can only be invoked from the constructor;
   // the thread locks reside in the constructor.

   try {

      int            endRow;
      int            i;
      int            index0;
      int            index1;
      int            index2;
      int            j;
      int            k;
      int            kount;
      int            startRow;

      std::ifstream  fin;

      // INITIALIZE .....

      fin.open(
         _gridFname.c_str(),
            std::ifstream::binary | std::ifstream::in );

      if ( fin.fail() )                                    return( 1 );

      // READ AND STORE HEADER .....

      fin.read((char*) &_nGridPad,  BYTES_PER_INT );

      if (fin.fail()) { fin.close();                       return( 1 ); }

      fin.read((char*) &_nOrigRows, BYTES_PER_INT );

      if (fin.fail()) { fin.close();                       return( 1 ); }

      fin.read((char*) &_nOrigCols, BYTES_PER_INT );

      if (fin.fail()) { fin.close();                       return( 1 ); }

      fin.read((char*) &_dLat,      BYTES_PER_DOUBLE );

      if (fin.fail()) { fin.close();                       return( 1 ); }

      fin.read((char*) &_dLon,      BYTES_PER_DOUBLE );

      if (fin.fail()) { fin.close();                       return( 1 ); }

      // If needed, convert to LITTLE-ENDIAN representation .....

      #if LITTLE_ENDIAN

         this->swapBytes( &_nGridPad,  BYTES_PER_INT,    1 );
         this->swapBytes( &_nOrigRows, BYTES_PER_INT,    1 );
         this->swapBytes( &_nOrigCols, BYTES_PER_INT,    1 );
         this->swapBytes( &_dLat,      BYTES_PER_DOUBLE, 1 );
         this->swapBytes( &_dLon,      BYTES_PER_DOUBLE, 1 );

      #endif

      _dLat         *= RAD_PER_DEG;  // grid file stores these in degrees
      _dLon         *= RAD_PER_DEG;  // grid file stores these in degrees

      // Set derived parameters .....

      _nGridRows     = _nOrigRows + ( 2 * _nGridPad );
      _nGridCols     = _nOrigCols + ( 2 * _nGridPad ) + 1;

      _baseLatitude  =
         -PIDIV2 - _dLat * double( _nGridPad );     // radians
      _baseLongitude =
             0.0 - _dLon * double( _nGridPad );     // radians

      // READ AND STORE THE REFORMATTED GRID .....

      // (Recall that the reformatted grid
      //  a) doesn't have FORTRAN records' headers and trailers,
      //  b) does have padding around the original grid's periphery)

      _heightGrid    = NULL;

      _heightGrid    = 
         new float[ _nGridRows * _nGridCols ];

      if ( NULL == _heightGrid )                           return( 1 ); 

      index0         = 0;

      startRow       = _nGridRows - 1;
      endRow         = 0;

      for ( i = startRow; i >= endRow; i-- ) 
      {
         // Read row's geoid heights .....

         fin.read( 
            (char*) (&_heightGrid[ index0 ]), 
               ( _nGridCols * BYTES_PER_FLOAT ));

         if ( fin.fail() ) { fin.close();                  return( 1 ); }

         // If needed, convert to LITTLE-ENDIAN representation .....

         #if LITTLE_ENDIAN

            this->swapBytes( 
               &_heightGrid[ index0 ], BYTES_PER_FLOAT, _nGridCols ); 

         #endif
         
         index0 += _nGridCols;
      }

      fin.close();

   }  // End of exceptions' try block

   catch ( ... ) {                                         return( 1 ); }

   return ( 0 );  // Normal-return flag

}  // End of function Egm2008FullGrid::loadGrid

// CLASSIFICATION: UNCLASSIFIED

