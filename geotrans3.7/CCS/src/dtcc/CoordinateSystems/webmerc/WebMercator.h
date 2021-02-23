// CLASSIFICATION: UNCLASSIFIED

#ifndef WEBM_H
#define WEBM_H
/***************************************************************************/
/* RSC IDENTIFIER: Web Mercator
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates 
 *    (latitude and longitude) and Web Mercator coordinates
 *    (easting and northing).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       WEBM_NO_ERROR              : No errors occurred in function
 *       WEBM_LAT_ERROR             : Latitude outside of valid range
 *                                      (-33.5 to -48.5 degrees)
 *       WEBM_ELLIPSOID_ERROR       : Invalid ellipsoid - must be WGS84
 *
 * REFERENCES
 *
 *    Further information on Web Mercator can be found in the NGA document 
 *    "Implementation Practice Web Mercator Map Projection", 2014-02-18.
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    06-14-14          Original Code
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

    /*
     *                              DEFINES
     */

      class WebMercator : public CoordinateSystem
      {
      public:

         /*
          * The constructor receives the ellipsoid code and sets
          * the corresponding state variables. If any errors occur,
          * an exception is thrown with a description of the error.
          *
          *   ellipsoidCode : 2-letter code for ellipsoid           (input)
          */

         WebMercator( char* ellipsoidCode );

         /*
          * The function getParameters returns the current ellipsoid code.
          *
          *   ellipsoidCode : 2-letter code for ellipsoid          (output)
          */

         EllipsoidParameters* getParameters() const;


         /*
          * The function convertFromGeodetic converts geodetic (latitude and
          * longitude) coordinates to Web Mercator projection
          * (easting and northing) coordinates, according to the current
          * ellipsoid.
          * If any errors occur, an exception is thrown with a 
          * description of the error.
          *
          *    longitude     : Longitude (lambda), in radians       (input)
          *    latitude      : Latitude (phi), in radians           (input)
          *    easting       : Easting (X), in meters               (output)
          *    northing      : Northing (Y), in meters              (output)
          */

         MSP::CCS::MapProjectionCoordinates* convertFromGeodetic(
            MSP::CCS::GeodeticCoordinates* geodeticCoordinates );

         /*
          * The function convertToGeodetic converts Web Mercator projection
          * (easting and northing) coordinates to geodetic
          * (latitude and longitude) coordinates.
          * If any errors occur, an exception is thrown with a description 
          * of the error.
          *
          *    easting       : Easting (X), in meters                  (input)
          *    northing      : Northing (Y), in meters                 (input)
          *    longitude     : Longitude (lambda), in radians          (output)
          *    latitude      : Latitude (phi), in radians              (output)
       */

         MSP::CCS::GeodeticCoordinates* convertToGeodetic(
            MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates );

      private:
         
      /* Ellipsoid Parameters, must be WGS84  */
         char WebMEllipsoidCode[3];

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
