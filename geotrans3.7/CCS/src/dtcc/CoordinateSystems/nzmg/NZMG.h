// CLASSIFICATION: UNCLASSIFIED

#ifndef NZMG_H
#define NZMG_H

/***************************************************************************/
/* RSC IDENTIFIER: NEW ZEALAND MAP GRID
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates 
 *    (latitude and longitude) and New Zealand Map Grid coordinates
 *    (easting and northing).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       NZMG_NO_ERROR              : No errors occurred in function
 *       NZMG_LAT_ERROR             : Latitude outside of valid range
 *                                      (-33.5 to -48.5 degrees)
 *       NZMG_LON_ERROR             : Longitude outside of valid range
 *                                      (165.5 to 180.0 degrees)
 *       NZMG_EASTING_ERROR         : Easting outside of valid range
 *                                      (depending on ellipsoid and
 *                                       projection parameters)
 *       NZMG_NORTHING_ERROR        : Northing outside of valid range
 *                                      (depending on ellipsoid and
 *                                       projection parameters)
 *       NZMG_ELLIPSOID_ERROR       : Invalid ellipsoid - must be International
 *
 * REUSE NOTES
 *
 *    NEW ZEALAND MAP GRID is intended for reuse by any application that 
 *    performs a New Zealand Map Grid projection or its inverse.
 *    
 * REFERENCES
 *
 *    Further information on NEW ZEALAND MAP GRID can be found in the 
 *    Reuse Manual.
 *
 *    NEW ZEALAND MAP GRID originated from :  
 *                      U.S. Army Topographic Engineering Center
 *                      Geospatial Information Division
 *                      7701 Telegraph Road
 *                      Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    NEW ZEALAND MAP GRID has no restrictions.
 *
 * ENVIRONMENT
 *
 *    NEW ZEALAND MAP GRID was tested and certified in the following 
 *    environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    09-14-00          Original Code
 *    03-2-07           Original C++ Code
 *
 *
 */

#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class EllipsoidParameters;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class NZMG : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid code and sets
       * the corresponding state variables. If any errors occur, an exception is 
       * thrown with a description of the error.
       *
       *   ellipsoidCode : 2-letter code for ellipsoid           (input)
       */

	    NZMG( char* ellipsoidCode );


      NZMG( const NZMG &n );


	    ~NZMG( void );


      NZMG& operator=( const NZMG &n );


      /*                         
       * The function getParameters returns the current ellipsoid
       * code.
       *
       *   ellipsoidCode : 2-letter code for ellipsoid          (output)
       */

      EllipsoidParameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts geodetic (latitude and
       * longitude) coordinates to New Zealand Map Grid projection (easting and northing)
       * coordinates, according to the current ellipsoid and New Zealand Map Grid 
       * projection parameters.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    longitude         : Longitude (lambda), in radians       (input)
       *    latitude          : Latitude (phi), in radians           (input)
       *    easting           : Easting (X), in meters               (output)
       *    northing          : Northing (Y), in meters              (output)
       */

      MSP::CCS::MapProjectionCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates );


      /*
       * The function convertToGeodetic converts New Zealand Map Grid projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid and New Zealand Map Grid projection
       * coordinates.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    easting           : Easting (X), in meters                  (input)
       *    northing          : Northing (Y), in meters                 (input)
       *    longitude         : Longitude (lambda), in radians          (output)
       *    latitude          : Latitude (phi), in radians              (output)
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates );

    private:
    
      /* Ellipsoid Parameters, must be International  */
       char NZMGEllipsoidCode[3];

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
