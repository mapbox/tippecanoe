// CLASSIFICATION: UNCLASSIFIED

#ifndef Geocentric_H
#define Geocentric_H

/***************************************************************************/
/* RSC IDENTIFIER:  GEOCENTRIC
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates (latitude,
 *    longitude in radians and height in meters) and Geocentric coordinates
 *    (X, Y, Z) in meters.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *      GEOCENT_NO_ERROR        : No errors occurred in function
 *      GEOCENT_LAT_ERROR       : Latitude out of valid range
 *                                 (-90 to 90 degrees)
 *      GEOCENT_LON_ERROR       : Longitude out of valid range
 *                                 (-180 to 360 degrees)
 *      GEOCENT_A_ERROR         : Semi-major axis less than or equal to zero
 *      GEOCENT_INV_F_ERROR     : Inverse flattening outside of valid range
 *								                 (250 to 350)
 *
 *
 * REUSE NOTES
 *
 *    GEOCENTRIC is intended for reuse by any application that performs
 *    coordinate conversions between geodetic coordinates and geocentric
 *    coordinates.
 *    
 *
 * REFERENCES
 *    
 *    An Improved Algorithm for Geocentric to Geodetic Coordinate Conversion,
 *    Ralph Toms, February 1996  UCRL-JC-123138.
 *    
 *    Further information on GEOCENTRIC can be found in the Reuse Manual.
 *
 *    GEOCENTRIC originated from : U.S. Army Topographic Engineering Center
 *                                 Geospatial Information Division
 *                                 7701 Telegraph Road
 *                                 Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    GEOCENTRIC has no restrictions.
 *
 * ENVIRONMENT
 *
 *    GEOCENTRIC was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC version 2.8.1
 *    2. Windows 95 with MS Visual C++ version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    25-02-97          Original Code
 *    3-02-07           Original C++ Code
 *    01/24/11          I. Krinsky    BAEts28121   
 *                      Terrain Service rearchitecture
 *
 */


#include "DtccApi.h"
#include "CoordinateSystem.h"


namespace MSP
{
   namespace CCS
   {
      class CartesianCoordinates;
      class GeodeticCoordinates;


      /***************************************************************************/
      /*
       *                              DEFINES
       */

      class MSP_DTCC_API Geocentric : public CoordinateSystem
      {
         public:

            /*
             * The constructor receives the ellipsoid parameters
             * as inputs and sets the corresponding state variables.
             *
             *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters.       (input)
             *    ellipsoidFlattening     : Flattening of ellipsoid.						           (input)
             */

	    Geocentric( double ellipsoidSemiMajorAxis, double ellipsoidFlattening );


            Geocentric( const Geocentric &g );


	    ~Geocentric( void );


            Geocentric& operator=( const Geocentric &g );


            /*
             * The function convertFromGeodetic converts geodetic coordinates
             * (latitude, longitude, and height) to geocentric coordinates (X, Y, Z),
             * according to the current ellipsoid parameters.
             *
             *   longitude : Geodetic longitude in radians                   (input)
             *   latitude  : Geodetic latitude in radians                    (input)
             *   height    : Geodetic height, in meters                      (input)
             *   X         : Calculated Geocentric X coordinate, in meters   (output)
             *   Y         : Calculated Geocentric Y coordinate, in meters   (output)
             *   Z         : Calculated Geocentric Z coordinate, in meters   (output)
             *
             */

            MSP::CCS::CartesianCoordinates* convertFromGeodetic(
               const MSP::CCS::GeodeticCoordinates* geodeticCoordinates );


            /*
             * The function convertToGeodetic converts geocentric
             * coordinates (X, Y, Z) to geodetic coordinates (latitude, longitude, 
             * and height), according to the current ellipsoid parameters.
             *
             *    X         : Geocentric X coordinate, in meters.         (input)
             *    Y         : Geocentric Y coordinate, in meters.         (input)
             *    Z         : Geocentric Z coordinate, in meters.         (input)
             *    longitude : Calculated longitude value in radians.      (output)
             *    latitude  : Calculated latitude value in radians.       (output)
             *    height    : Calculated height value, in meters.         (output)
             *
             */

            MSP::CCS::GeodeticCoordinates* convertToGeodetic(
               MSP::CCS::CartesianCoordinates* cartesianCoordinates );

         private:
    
            void geocentricToGeodetic(
               const double x,
               const double y,
               const double z,
               double      &lat,
               double      &lon,
               double      &ht );

            /* Ellipsoid parameters, default to WGS 84 */
            double Geocent_e2;   /* Eccentricity squared  */
            double Geocent_ep2;  /* 2nd eccentricity squared */

            enum AlgEnum { UNDEFINED, ITERATIVE, GEOTRANS };
            AlgEnum    Geocent_algorithm; 
      };
   }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
