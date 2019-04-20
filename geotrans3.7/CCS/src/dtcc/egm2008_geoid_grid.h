
// CLASSIFICATION: UNCLASSIFIED
 
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//   File name: egm2008_geoid_grid.h                                          //
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
//   11 Feg 2011  RD Craig      Upgrades following code review                //
//   30 May 2013  RD Craig      MSP 1.3: ER29758                              //
//                              Added second constructor to                   //
//                              permit multiple geoid-height grids            //
//                              when assessing relative interpolation errors. //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef EGM2008_GEOID_GRID_H
#define EGM2008_GEOID_GRID_H

// This file declares a base C++ class
// that supports geoid height interpolation
// from one of NGA's EGM 2008 geoid height files.  

// EGM2008_GEOID_GRID IS A PURE VIRTUAL CLASS.

// Note: Following common usage, this class uses the term 
//       "geoid height" to mean heights of the geoid above or
//       below the earth ellipsoid.  The GeoidLibrary class confuses
//       the term "geoid height" with height of a point above or below
//       the geoid: these heights are properly called "orthometric heights".

#include <string>

#include "CCSThreadMutex.h"

namespace MSP
{

   class Egm2008GeoidGrid {
      
      protected:

      // MAX_WSIZE:          The maximum number of rows and
      //                     columns in a local interpolation window.

      int                    MAX_WSIZE;

      // gridFname:          A string containing
      //                     the file name for one of
      //                     NGA's reformatted geoid height grids.

      std::string            _gridFname;

      // nGridPad:           The number of grid cells added to 
      //                     the left, right, top, and bottom sides 
      //                     of the worldwide EGM2008 geoid height grid;
      //                     interpolation window size is related to _nGridPad;
      //                     an ExE grid, where E is an even number, requires at
      //                     least _nGridPad = int( E - 2 ) / 2 ) rows/cols of pad;
      //                     an OxO grid, where O is an odd number, requires at
      //                     least _nGridPad = int( O / 2 ) rows/cols of pad on each edge.

      int                    _nGridPad;

      // nGridRows:          The number of latitude rows contained in
      //                     the padded, memory-resident world-wide grid.

      int                    _nGridRows;

      // nGridCols:          The number of longitude entries per
      //                     row in the padded, memory-resident grid.

      int                    _nGridCols;

      // nOrigRows:          The number of latitude rows 
      //                     contained in the world-wide grid file.

      int                    _nOrigRows;

      // nOrigCols:          The number of longitude entries per
      //                     latitude row in the world-wide grid file.

      int                    _nOrigCols;

      // baseLatitude:       The padded grid's base latitude 
      //                     (radians); baseLatitude will be less than
      //                     -PI/2 radians for the world-wide EGM 2008 grid.

      double                 _baseLatitude;

      // baseLongitude:      The padded grid's base longitude
      //                     (radians); baseLongitude will be less than
      //                     zero radians for the world-wide EGM 2008 grid.

      double                 _baseLongitude;

      // dLat:               Grid's latitude spacing (radians).

      double                 _dLat;

      // dLon:               Grid's longitude spacing (radians).

      double                 _dLon;

      // mutex:              A CCSThreadMutex object used to ensure thread safety.

      MSP::CCSThreadMutex    _mutex;

      public:

      // Basic functions .....

      Egm2008GeoidGrid( void );

      Egm2008GeoidGrid( const std::string  &gridFname );  // new 5/30/2013
 
      Egm2008GeoidGrid( const Egm2008GeoidGrid& oldGrid );

      virtual 
      ~Egm2008GeoidGrid( void );

      Egm2008GeoidGrid&
      operator = ( const Egm2008GeoidGrid& oldGrid );

      // User functions .....
 
      /*
       * Public function geoidHeight implements
       * a bicubic spline geoid separation interpolator.
       *
       *    wSize              : Number of rows (= # columns)      ( input )
       *                         in the local interpolation window;
       *                         the function supports 2 <= wSize <= 20,
       *                         but EGM 2008 interpolations use wSize = 6.
       *    latitude           : Geodetic latitude                 ( input  - radians )
       *    longitude          : Geodetic longitude                ( input  - radians )
       *    gHeight            : Geoid height                      ( output - meters  )
       *
       *    return value       : The function's error flag;
       *                         errors = 0 ..... no errors encountered,
       *                         errors = 1 ..... at least one error encountered.
       *
       */

      virtual int
      geoidHeight(
         int     wSize,                     // input
         double  latitude,                  // input
         double  longitude,                 // input
         double& gHeight ) = 0;             // output

      protected:
 
      /*
       * Protected function geoidHeight 
       * implements a bilinear geoid separation interpolator.
       * Exercise this function when the requested interpolation
       * window size is too small to support bicubic spline interpolation.
       *
       *    latitude           : Geodetic latitude                 ( input  - radians )
       *    longitude          : Geodetic longitude                ( input  - radians )
       *    gHeight            : Geoid height                      ( output - meters  )
       *
       *    return value       : The function's error flag;
       *                         errors = 0 ..... no errors encountered,
       *                         errors = 1 ..... at least one error encountered.
       */

      virtual int
      geoidHeight(
         double  latitude,                  // input
         double  longitude,                 // input
         double& gHeight ) = 0;             // output
 
      /*
       * Protected function loadGridCoords finds
       * horizontal coordinates corresponding to
       * grid row and column indices. The indices must
       * be referenced to the worldwide geoid height grid.
       *
       *    i                  : An index to a worldwide grid      ( input )
       *                         intersection's row of interest.
       *    j                  : An index to a worldwide grid      ( input )
       *                         intersection's column of interest.
       *    latitude           : Geodetic latitude                 ( output - radians )
       *    longitude          : Geodetic longitude                ( output - radians )
       *
       *    return value       : The function's error flag;
       *                         errors = 0 ..... no errors encountered,
       *                         errors = 1 ..... at least one error encountered.
       */

      int
      loadGridCoords(
         int      i,                        // input
         int      j,                        // input
         double&  latitude,                 // output
         double&  longitude );              // output

      /*
       * Protected function initSpline computes the
       * geoid separation posts' 2nd derivatives.  This 
       * function assumes the posts are arranged in a row, 
       * it assumes the posts are equally spaced, and it also 
       * assumes the spline's 2nd derivatives are zero at the endpoints.
       * The computed 2nd derivatives support the 1D spline interpolator.
       *
       *    n                  : Number of geoid height posts for  ( input )
       *                         which 2nd derivatives are computed.
       *    posts[]            : An array containing               ( input )
       *                         geoid height posts' ordinates.
       *    moments[]          : An array containing geoid
       *                         height posts' 2nd derivatives.    ( output )
       *
       *    return value       : The function's error flag;
       *                         errors = 0 ..... no errors encountered,
       *                         errors = 1 ..... at least one error encountered.
       */

      int 
      initSpline(
         int           n,                   // input
         const double  posts[],             // input
         double        moments[] );         // output

      /*
       * Protected function spline implements a
       * low-level one-dimensional spline interpolator.
       *
       *    n                  : Number of geoid height posts for  ( input )
       *                         which 2nd derivatives are computed.
       *    x                  : Abscissa at which                 ( input )
       *                         interpolation is to occur;
       *                         x is measured in grid intervals,
       *                         and it is measured relative to the
       *                         first geoid separation post's position.
       *    posts[]            : An array containing               ( input )
       *                         geoid separation posts' ordinates.
       *    moments[]          : An array containing geoid
       *                         separation posts' 2nd derivatives.( input )
       *
       *    return value       : The interpolated geoid height     ( meters ).
       */

      double
      spline(
         int           n,                   // input
         double        x,                   // input
         const double  posts[],             // input
         const double  moments[] );         // input

      /*
       * Protected function swapBytes swaps 
       * bytes when transforming binary numbers 
       * between BIG-ENDIAN and LITTLE-ENDIAN formats.
       *
       *    buffer             : Pointer to binary data item(s)    ( input & output )
       *    size               : Size of each binary number        ( input - bytes  )
       *    count              : Number of binary numbers          ( input )
       */

      void
      swapBytes(
         void*   buffer,                    // input & output
         size_t  size,                      // input
         size_t  count );                   // input

      /*
       * Protected function swGridIndices computes
       * grid row & column indices for the intersection
       * immediately to the SOUTHWEST of the input point.
       * The indices refer to the worldwide geoid height grid.
       *
       *    latitude           : Geodetic latitude                 ( input - radians )
       *    longitude          : Geodetic longitude                ( input - radians )
       *    i                  : Row number for the                ( output )
       *                         worldwide grid intersection 
       *                         Southwest of the point of interest.
       *    j                  : Column number for the             ( output )
       *                         worldwide grid intersection 
       *                         Southwest of the point of interest.
       *
       *    return value       : The function's error flag;
       *                         errors = 0 ..... no errors encountered,
       *                         errors = 1 ..... at least one error encountered.
       */

      int
      swGridIndices(
         double   latitude,                 // input
         double   longitude,                // input
         int&     i,                        // output
         int&     j );                      // output

   };  // End of Egm2008GeoidGrid class declaration

}  // End of namespace block

#endif

// CLASSIFICATION: UNCLASSIFIED

