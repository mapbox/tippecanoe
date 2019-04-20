// CLASSIFICATION: UNCLASSIFIED

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//   File name: egm2008_geoid_grid.cpp                                        //
//                                                                            //
//   Description of this module:                                              //
//                                                                            //
//      Utility software that interpolates EGM 2008                           //
//      geoid heights from one of NGA's geoid height grids.                   //
//                                                                            //
//      This base class supports two geoid height interpolators:              //
//                                                                            //
//      1) the first one reads the entire worldwide EGM2008 grid              //
//         before interpolating any geoid heights from the worldwide grid;    //
//      2) the second one does not read geoid height data from                //
//         file until a user requests that a geoid height be computed;        //
//         the 2nd interpolator then reads an Area of Interest                //
//         (AOI) grid from file before interpolating from the AOI grid;       //
//         the Area of Interest grid is refreshed, if needed,                 //
//         when users submit subsequent geoid height requests.                //
//                                                                            //
//   Revision History:                                                        //
//   Date         Name          Description                                   //
//   -----------  ------------  ----------------------------------------------//
//   19 Nov 2010  RD Craig      Release                                       //
//   27 Jan 2011  S. Gillis     BAEts28121, Terrain Service rearchitecture    //
//   17 May 2011  T. Thompson   BAEts27393, let user know when problem        //
//                              is due to undefined MSPCCS_DATA               //
//   30 May 2013  RD Craig      MSP 1.3: ER29758                              //
//                              Added second constructor to                   //
//                              permit multiple geoid-height grids            //
//                              when assessing relative interpolation errors. //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
  
// THIS PACKAGE REQUIRES NGA's WORLDWIDE GEOID HEIGHT GRID.  THIS GRID'S
// DIRECTORY PATH MUST BE SPECIFIED BY ENVIRONMENT VARIABLE "MSPCCS_DATA".

// Note: Following common usage, this class uses the term 
//       "geoid height" to mean heights of the geoid above or
//       below the earth ellipsoid.  The GeoidLibrary class confuses
//       the term "geoid height" with height of a point above or below the
//       geoid: these heights are properly called elevations or heights-above-MSL.
 
#include <fstream>
#include <iomanip>            // DEBUG
#include <iostream>           // DEBUG
#include <stdlib.h>
#include <string>

#ifdef IRIXN32
#include <math.h>
#else
#include <cmath>
#endif

/* DEBUG

using std::ios;

using std::cin;
using std::cout;
using std::endl;
using std::setprecision;
using std::setw;

END DEBUG */


#include "CoordinateConversionException.h"

#include "egm2008_geoid_grid.h"

namespace {

   const double EPSILON         = 1.0e-15;  // ~4 times machine epsilon

   const double PI              = 3.14159265358979323;

   const double PIDIV2          = PI / 2.0;
   const double TWOPI           = 2.0 * PI; 

   const double DEG_PER_RAD     = 180.0 / PI;
   const double RAD_PER_DEG     = PI / 180.0;

   const double SEMI_MAJOR_AXIS = 6378137.0;              // WGS-84
   const double FLATTENING      = 1.0 / 298.257223563;    // WGS-84
}

using namespace MSP;

// ***************************
// ** Public user functions **
// ***************************

// ****************************************
// * Default Egm2008GeoidGrid constructor *
// ****************************************

Egm2008GeoidGrid::Egm2008GeoidGrid( void )
{
   // November  19, 2010: Version 1.00

   // This function implements the
   // default Egm2008GeoidGrid constructor.

   //_mutex.lock();  // Use CCSThreadMutex function in constructors

   int     i        =    0;
   int     length   =    0;

   char*   pathName = NULL;

   // Get reformatted geoid height grid's file name .....
#ifdef NDK_BUILD
   pathName = "/data/data/com.baesystems.msp.geotrans/lib/";
   _gridFname += pathName;
   _gridFname += "libegm2008grd.so";
#else
   pathName        = getenv( "MSPCCS_DATA" ); 

   if ( NULL == pathName )
   {
      strcpy( pathName, "../../data" );
   }

   length          = strlen( pathName );

   for (i = 0; i < length; i++) _gridFname += pathName[i];

   _gridFname     +=  
      "/Und_min2.5x2.5_egm2008_WGS84_TideFree_reformatted";
#endif

   // BAEts27393 Reverse the change for this DR for plug-in backward 
   // compatible with MSP 1.1, i.e. do not throw error when plug-in is
   // dropped into MSP 1.1 where EGM2008 data file is not available.

   //int     undefEnvVar = 0;
   //FILE*   fp = NULL;                    /* File pointer to file */
   //
   //// Set the reformatted geoid height grid file name
   //char*   fileName = new char[50];
   //strcpy( fileName, "Und_min2.5x2.5_egm2008_WGS84_TideFree_reformatted" );
   //
   //// Determine the path to the MSP CCS data
   //char* dataPath = getenv( "MSPCCS_DATA" ); 
   //if ( NULL == dataPath )
   //{
   //   undefEnvVar = 1;
   //   strcpy( dataPath, "../../data" );
   //}
   //
   //// Assemble the path and file name
   //std::string pathName = dataPath;
   //pathName.append("/");
   //pathName.append(fileName);
   //
   //// Check if the reformatted geoid height grid file exists
   //if ( ( fp = fopen( pathName.c_str(), "r" ) ) == NULL )
   //{
   //   ErrorControl errCtrl;
   //   if ( 1 == undefEnvVar )
   //   {
   //      errCtrl.issueError( "Environment variable undefined: MSPCCS_DATA.",
   //                          "Egm2008GeoidGrid::Egm2008GeoidGrid");
   //   }
   //   else
   //   {
   //      std::string errMsg = "Unable to open ";
   //      errMsg.append(pathName);
   //      errCtrl.issueError( errMsg, "Egm2008GeoidGrid::Egm2008GeoidGrid" );
   //   }
   //}
   //fclose( fp );

   //// Update the class string variable with the path and name of the file
   //_gridFname = pathName.c_str();


   // Initialize base grid parameters .....

   MAX_WSIZE       =  20;

   _nGridPad       =   0;
   _nGridCols      =   0;
   _nGridRows      =   0;
   _nOrigCols      =   0;
   _nOrigRows      =   0;

   _baseLatitude   = 0.0;
   _baseLongitude  = 0.0;

   _dLat           = 0.0;
   _dLon           = 0.0;

   // The thread will be unlocked 
   // by a derived class constructor.

}  // End of default Egm2008GeoidGrid constuctor


// ********************************************
// * Non-default Egm2008GeoidGrid constructor *
// ********************************************

Egm2008GeoidGrid::Egm2008GeoidGrid( const std::string  &gridFname )
{
   // 30 May 2013: Version 1.00

   // This function implements a
   // non-default Egm2008GeoidGrid constructor.

   // Definition:

   // gridFname:             The geoid height grid's file name; this
   //                        file name should not contain the directory path;
   //                        this function will pre-pend the path
   //                        specified by environment variable MSPCCS_DATA.

   int     i        =    0;
   int     length   =    0;

   char*   pathName = NULL;

   // Get reformatted geoid height grid's file name .....
#ifdef NDK_BUILD
   pathName = "/data/data/com.baesystems.msp.geotrans/lib/";
   _gridFname += pathName;
   _gridFname += "libegm2008grd.so";
#else
   pathName        = getenv( "MSPCCS_DATA" ); 

   if ( NULL == pathName )
   {
      strcpy( pathName, "../../data" );
   }

   length          = strlen( pathName );

   for( i = 0; i < length; i++ ) _gridFname += pathName[ i ];

   _gridFname     += '/';

   _gridFname     += gridFname; 
#endif

   // Initialize base grid parameters .....

   MAX_WSIZE       =  20;

   _nGridPad       =   0;
   _nGridCols      =   0;
   _nGridRows      =   0;
   _nOrigCols      =   0;
   _nOrigRows      =   0;

   _baseLatitude   = 0.0;
   _baseLongitude  = 0.0;

   _dLat           = 0.0;
   _dLon           = 0.0;

}  // End of non-default Egm2008GeoidGrid constructor


// *************************************
// * Egm2008GeoidGrid copy constructor *
// *************************************

Egm2008GeoidGrid::Egm2008GeoidGrid 
   ( const Egm2008GeoidGrid&  oldGrid )          // input
{
   // November  19, 2010: Version 1.00

   // This function implements the
   // Egm2008GeoidGrid copy constructor.

   // Definition:

   // oldGrid:            The Egm2008GeoidGrid object being copied.

   oldGrid._mutex.lock();  //  Use CCSThreadMutex function in copy constructors

   // Copy worldwide grid's file name .....

   _gridFname        =  oldGrid._gridFname;

   // Copy grid's parameters .....

   MAX_WSIZE         = oldGrid.MAX_WSIZE;

   _nGridPad         = oldGrid._nGridPad;

   _nGridRows        = oldGrid._nGridRows;
   _nGridCols        = oldGrid._nGridCols; 

   _nOrigRows        = oldGrid._nOrigRows;
   _nOrigCols        = oldGrid._nOrigCols;

   _baseLatitude     = oldGrid._baseLatitude;
   _baseLongitude    = oldGrid._baseLongitude;

   _dLat             = oldGrid._dLat;
   _dLon             = oldGrid._dLon;

   // The oldGrid object will be unlocked 
   // by a derived class copy constructor.

}  // End of Egm2008GeoidGrid copy constuctor


// *******************************
// * Egm2008GeoidGrid destructor *
// *******************************

Egm2008GeoidGrid::~Egm2008GeoidGrid (void)
{
   // November  19, 2010: Version 1.00

   // This function implements 
   // the Egm2008GeoidGrid destructor. 

   ;

}  // End of Egm2008GeoidGrid destructor


// ****************************************
// * Egm2008GeoidGrid assignment operator *
// ****************************************

Egm2008GeoidGrid&
Egm2008GeoidGrid::operator= ( const Egm2008GeoidGrid&  oldGrid ) 
{
   // November  19, 2010: Version 1.00

   // Definition:

   // oldGrid:            The Egm2008GeoidGrid object being assigned.

   // This function implements the
   // Egm2008GeoidGrid assignment operator.

   _mutex.lock(); // Use CCSThreadMutex function in assignments
   oldGrid._mutex.lock();

   if ( this == & oldGrid )                      return ( *this );

   // Assign worldwide grid's file name .....

   _gridFname        = oldGrid._gridFname;

   // Assign grids' parameters .....

   MAX_WSIZE         = oldGrid.MAX_WSIZE;

   _nGridPad         = oldGrid._nGridPad; 

   _nGridRows        = oldGrid._nGridRows;
   _nGridCols        = oldGrid._nGridCols;

   _nOrigRows        = oldGrid._nOrigRows;
   _nOrigCols        = oldGrid._nOrigCols;

   _baseLatitude     = oldGrid._baseLatitude;
   _baseLongitude    = oldGrid._baseLongitude;

   _dLat             = oldGrid._dLat;
   _dLon             = oldGrid._dLon;

   // The object will be unlocked by a 
   // derived class assignment operator.

   return( *this );

}  // End of Egm2008GeoidGrid assignment operator


// **********************
// ** Hidden functions **
// **********************

// **************************************
// * Retrieve user-specified grid point *
// **************************************

int
Egm2008GeoidGrid::loadGridCoords(
   int      i,                              // input
   int      j,                              // input
   double&  latitude,                       // output
   double&  longitude )                     // output
{
   // November  19, 2010: Version 1.00

   // This function retrieves geodetic
   // coordinates corresponding to geoid height posts.

   // This function allows users to supply 
   // indices refering to the worldwide grid's pad region.

   // Definitions:

   // i:                     Worldwide grid post's row    index. 
   // j:                     Worldwide grid post's column index.
   // latitude:              Grid point's geodetic latitude  (radians).
   // longitude:             Grid point's geodetic longitude (radians).

   // Thread locks are not needed here, because this function
   // can only be invoked from a bicubic spline geoidHeight function; the
   // thread locks, if any, reside in the bicubic spline's geoidHeight function.

   try {

      const int     LIMIT1 = _nGridPad;
      const int     LIMIT2 = _nGridRows - _nGridPad - 1;

      int           k;

      // EDIT THE INPUT AND INITIALIZE .....

      if (( i < 0 ) || ( i >= _nGridRows ))                return( 1 );

      while ( j <           0 ) j += _nGridCols;
      while ( j >= _nGridCols ) j -= _nGridCols;

      // COMPUTE GEODETIC COORDINATES .....

      // Indices refer to Southern pad region .....

      if ( i < LIMIT1 )
      {
         latitude  = 
            ( -PIDIV2 - _dLat * double( i - LIMIT1 ));

         k         = j + ( _nOrigCols / 2 );

         if ( k >= _nGridCols ) k -= _nOrigCols;

         longitude = 
            ( _baseLongitude + _dLon * double( k ));
      }

      // Indices refer to NGA grid region .....

      if (( i >= LIMIT1 ) && ( i <= LIMIT2 ))
      {
         latitude  = 
            ( _baseLatitude  + _dLat * double( i ));

         longitude = 
            ( _baseLongitude + _dLon * double( j ));
      }

      // Indices refer to Northern pad region .....

      if ( i > LIMIT2 )
      {
         latitude  = 
            ( PIDIV2 - _dLat * double( i - LIMIT2 )); 

         k         = j + ( _nOrigCols / 2 );

         if ( k >= _nGridCols ) k -= _nOrigCols;

         longitude = 
            ( _baseLongitude + _dLon * double( k ));
      }

   }  // End of exceptions' try block

   catch ( ... ) {                                         return( 1 ); }

   return( 0 );  // Normal-return flag

}  // End of function Egm2008GeoidGrid::loadGridCoords


// **********************************************
// * Compute a one-dimensional spline's moments *
// **********************************************

int 
Egm2008GeoidGrid::initSpline(
   int           n,                         // input
   const double  posts[],                   // input
   double        moments[] )                // output
{
   // November  4, 2010: Version 1.00
   // February 12, 2011: Version 2.00:
   // new variable names following code review

   // Using equally-spaced abscissae, this
   // function computes a one-dimensional spline's moments.

   // This function assumes that the endmost 
   // support points' second derivatives are zero.

   // Definitions:

   // moments:               Spline moments (2nd derivatives) 
   //                        that are evaluated at the spline's support 
   //                        points; this array contains at least n elements.
   // n:                     The spline's number of support points; the
   //                        calling function ensures that 3 <= n <= 20.
   // posts:                 An array containing the support points' 
   //                        ordinates; this array contains at least n elements.

   // w:                     An array that, on completion of the Gauss
   //                        elimination step, contains the triangularized
   //                        coefficient matrix' superdiagonal. Element w[0] 
   //                        DOES NOT contain a superdiagonal matrix element.

   // Thread locks are not needed here, because this function
   // can only be invoked from a bicubic spline geoidHeight function;
   // the thread locks reside in the bicubic spline's geoidHeight function.

   // This C++ function 
   // refines FORTRAN subroutine INITSP, 
   // which was originally written (July 1983) 
   // by Rene Forsberg, Institut Fur Erdmessung, 
   // Universitat Hannover, Federal Republic of Germany. 

   // NGA used this FORTRAN function in 
   // its internal EGM 2008 interpolation software.

   // References:

   // Einfuhrung in die Numerische Mathematik, Band I,
   // Josef Stoer,
   // Springer-Verlag, Heidelberg, 1972, pp 82 und 86

   // Introduction to Numerical Analysis,
   // Josef Stoer and Roland Bulirsch,
   // Springer-Verlag, New York 1980, 
   // Sections 2.4.1 - 2.4.2 (pp 93 - 102)

   // Algorithm:
 
   // For the special case of equally-
   // spaced abscissae, where the first and
   // last moments are zero, the moments are found by
   // solving the linear tridiagonal system of equations
 
   //  ..                                    .. ..    ..   ..    ..
   //  :                                      : :      :   :      :
   //  : 2.0    0.0                           : : m    :   :  0   :
   //  :                                      : :  0   :   :      :
   //  : 0.5    2.0    0.5                    : : m    :   : rhs  :
   //  :                                      : :  1   :   :    1 :
   //  :        0.5    2.0  0.5               : : m    :   : rhs  :
   //  :                                      : :  2   : = :    2 :,
   //  :                     .                : :      :   :      :
   //  :                        .             : :      :   :   .  :
   //  :                           .          : :      :   :   .  :
   //  :                      0.5   2.0   0.5 : :      :   :   .  :
   //  :                                      : :      :   :      :
   //  :                            0.0   2.0 : : m    :   :  0   :
   //  :.                                    .: :. n-1.:   :.    .:

   // where 

   // 1) n  is the number of support data points, 
 
   // 2) m  is the j-th support point's unknown second derivative, 
   //     j
 
   //           .. 
   //           : 0.0,                            j = 0, 
   //           : 
   //           : 
   // 3) rhs  = : 3 * (y    - 2 y  + y   ),  1 <= j <= (n-2), 
   //       j   :       j+1      j    j-1
   //           : 
   //           : 0.0,                            j = n - 1.
   //           :. 

   // This system of equations is never singular, 
   // so tests for singularity are not necessary.

   // The coefficient matrix is tridiagonal,
   // so system solution time is proportional 
   // to the number of moments being computed.

   try {

      const int  TWENTY = 20;

      int        k;

      double     p;

      double     w[ TWENTY ];

      // EDIT THE INPUT AND INITIALIZE .....

      if ( TWENTY != MAX_WSIZE )                           return( 1 );

      if ( n < 3 )                                         return( 1 );

      w[ 0 ]       = 0.0;
      moments[ 0 ] = 0.0;

      // APPLY GAUSS ELIMINATION TO THE 
      // MOMENTS' TRI-DIAGONAL SYSTEM OF EQUATIONS .....

      for (k = 2; k < n; k++) 
      {
         p              = ( w[ k-2 ] / 2.0 ) + 2.0;
         w[ k-1 ]       = -0.5 / p;
         moments[ k-1 ] = 
           (3.0 * 
              ( posts[ k ] - ( 2.0 * posts[ k-1 ]) + 
                  posts[ k-2 ]) - ( moments[ k-2 ] / 2.0 )) / p;
      }

      // SOLVE THE RESULTING BI-DIAGONAL SYSTEM .....

      moments[ n-1 ] = 0.0;

      for (k = n - 1; k >= 2; k--) 
      {
         moments[ k-1 ] += ( w[ k-1 ] * moments[ k ]);
      }

   }  // End of exceptions' try block

   catch ( ... ) {                                         return( 1 ); }

   return( 0 );  // Normal-return flag

}  // End of function Egm2008GeoidGrid::initSpline  


// ***************************************************
// * Specialized one-dimensional spline interpolator *
// ***************************************************

double
Egm2008GeoidGrid::spline(
   int           n,                         // input
   double        x,                         // input
   const double  posts[],                   // input
   const double  moments[] )                // input
{
   // November  4, 2010: Version 1.00
   // February 12, 2011: Version 2.00:
   // new variable names following code review

   // Using support points having 
   // equally-spaced abscissae, this function quickly 
   // evaluates a cubic spline at an abscissa of interest.

   // Definitions:

   // moments:               Spline moments (2nd derivatives) 
   //                        that were evaluated at the spline's support 
   //                        points; this array contains at least n elements.
   // n:                     The spline's number of support points; the
   //                        calling function ensures that 3 <= n <= 20.
   // posts:                 An array containing the support points' 
   //                        ordinates; this array has at least n elements.
   // x:                     The interpolation argument;
   //                        (x == 0.0)        ==> x equals the first support abscissa,
   //                        (x == double(n-1) ==> x equals the final support abscissa.

   // Thread locks are not needed here, because this function
   // can only be invoked from a bicubic spline geoidHeight function;
   // the thread locks reside in the bicubic spline's geoidHeight function.

   // This C++ function 
   // refines FORTRAN function SPLINE, 
   // which was originally written (June 1983) 
   // by Rene Forsberg, Institut Fur Erdmessung, 
   // Universitat Hannover, Federal Republic of Germany. 

   // NGA used this FORTRAN function in 
   // its internal EGM 2008 interpolation software.

   // References:

   // Einfuhrung in die Numerische Mathematik, Band I,
   // Josef Stoer,
   // Springer-Verlag, Heidelberg, 1972, p 81

   // Introduction to Numerical Analysis,
   // Josef Stoer and Roland Bulirsch,
   // Springer-Verlag, New York 1980, 
   // Sections 2.4.1 and 2.4.2 (pp 93 - 102)

   // When (x < 0) or (x > n-1), this function linearly
   // extrapolates the dependent variable from the nearest
   // support points.  Recall that the endpoints' moments are zero.

   double  height;

   const double  MIN_ABSCISSA = 0.0;

   int           j;

   double        dx;
   double        maxAbscissa;
   double        mJ;
   double        mJp1;

   // Edit the input and initialize .....

   if ( n < 3 )
   {
      throw MSP::CCS::CoordinateConversionException(
         "Error: Egm2008GeoidGrid::spline: not enough points");
   }

   try {

      maxAbscissa = double( n - 1 );

      // Use one-dimensional extrapolation or
      // interpolation to determine geoid height .....

      if ( x <= MIN_ABSCISSA )        // linear extrapolation
      {
         height = 
            posts[ 0 ] + 
               ( x - MIN_ABSCISSA ) * 
                  ( posts[ 1 ] - posts[ 0 ] - 
                     ( moments[ 1 ] / 6.0 ));
      }
      else if ( x >= maxAbscissa )    // linear extrapolation
      {
         height = 
           posts[ n-1 ] + 
              ( x - maxAbscissa ) *
                 ( posts[ n-1 ] - posts[ n-2 ] + 
                    ( moments[ n-2 ] / 6.0 ));
      }
      else                            // cubic spline interpolation
      {
         j      = int( floor( x ));     

         dx     = x - double( j );
         mJ     = moments[  j  ];
         mJp1   = moments[ j+1 ];

         height =       // compute geoid height using Horner's rule
            posts[ j ] +
               dx * (( posts[ j+1 ] - posts[ j ] - 
                     ( mJ / 3.0 )   - ( mJp1 / 6.0 )) +
               dx * (( mJ / 2.0 ) + 
               dx * ( mJp1 - mJ ) / 6.0 ));
      }

   }  // End of exceptions' try block

   catch ( ... )
   {
      throw MSP::CCS::CoordinateConversionException(
         "Error: Egm2008GeoidGrid::spline");
   }

   return ( height );  

}  // End of function Egm2008GeoidGrid::spline 


// **************
// * Swap bytes *
// **************

void
Egm2008GeoidGrid::swapBytes(
   void*   buffer,                          // input & output
   size_t  size,                            // input
   size_t  count )                          // input
{
   // November  8, 2010: Version 1.00

   // This function swaps 
   // bytes when transforming between
   // BIG-ENDIAN and LITTLE-ENDIAN binary formats.

   // Definitions:

   // buffer:                Buffer containing binary numbers.
   // count:                 Number of data items in the buffer.
   // size:                  Size (bytes) of each buffer data item.

   // Thread locks are not needed here, because this function
   // can only be invoked from a bicubic spline geoidHeight function;
   // the thread locks reside in the bicubic spline's geoidHeight function.

   char   temp;

   char*  b = (char *) buffer;

   for ( size_t c = 0; c < count; c ++ )
   {
      for ( size_t s = 0; s < ( size / 2 ); s++ )
      {
         temp = b[ c * size + s ];
         b[ c * size + s] = b[ c * size + size - s - 1 ];
         b[ c * size + size - s - 1] = temp;
      }
   }

}  // End of function Egm2008GeoidGrid::swapBytes


// *************************************
// * Find Southwest grid-point indices *
// *************************************

int
Egm2008GeoidGrid::swGridIndices(
   double   latitude,                       // input
   double   longitude,                      // input
   int&     i,                              // output
   int&     j )                             // output
{
   // November  19, 2010: Version 1.00

   // Given geodetic latitude and longitude, 
   // this function finds indices to the nearest
   // grid point TO THE SOUTHWEST of the input point.
   // The indices refer to the padded version
   // of NGA's worldwide worldwide geoid-height grid. 

   // Definitions:

   // i:                     Worldwide grid's latitude  index. 
   // j:                     Worldwide grid's longitude index.
   // latitude:              Input geodetic latitude  (radians).
   // longitude:             Input geodetic longitude (radians).

   // Thread locks are not needed here, because this function
   // can only be invoked from a bicubic spline geoidHeight function;
   // the thread locks reside in the bicubic spline's geoidHeight function.

   // Edit the input and initialize .....

   try {

      double  maxLatitude;

      maxLatitude = 
         ( _baseLatitude + double( _nGridRows - 1 ) * _dLat );

      if (( latitude < _baseLatitude ) || 
          ( latitude >   maxLatitude ))                    return( 1 );

      while ( longitude <   0.0 ) longitude += TWOPI;
      while ( longitude > TWOPI ) longitude -= TWOPI;

      // Compute the indices .....

      i = _nGridPad + int(( latitude + PIDIV2 ) / _dLat );

      j = _nGridPad + int( longitude / _dLon );

   }  // End of exceptions' try block

   catch ( ... ) {                                         return( 1 ); }

   return( 0 );  // Normal-return flag

}  // End of function Egm2008GeoidGrid::swGridIndices

// CLASSIFICATION: UNCLASSIFIED
