// CLASSIFICATION: UNCLASSIFIED

#ifndef UTM_H
#define UTM_H

/***************************************************************************/
/* RSC IDENTIFIER: UTM
 *
 * ABSTRACT
 *
 *    This component provides conversions between geodetic coordinates 
 *    (latitude and longitudes) and Universal Transverse Mercator (UTM)
 *    projection (zone, hemisphere, easting, and northing) coordinates.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          UTM_NO_ERROR           : No errors occurred in function
 *          UTM_LAT_ERROR          : Latitude outside of valid range
 *                                    (-80.5 to 84.5 degrees)
 *          UTM_LON_ERROR          : Longitude outside of valid range
 *                                    (-180 to 360 degrees)
 *          UTM_EASTING_ERROR      : Easting outside of valid range
 *                                    (100,000 to 900,000 meters)
 *          UTM_NORTHING_ERROR     : Northing outside of valid range
 *                                    (0 to 10,000,000 meters)
 *          UTM_ZONE_ERROR         : Zone outside of valid range (1 to 60)
 *          UTM_HEMISPHERE_ERROR   : Invalid hemisphere ('N' or 'S')
 *          UTM_ZONE_OVERRIDE_ERROR: Zone outside of valid range
 *                                    (1 to 60) and within 1 of 'natural' zone
 *          UTM_A_ERROR            : Semi-major axis less than or equal to zero
 *          UTM_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                    (250 to 350)
 *
 * REUSE NOTES
 *
 *    UTM is intended for reuse by any application that performs a Universal
 *    Transverse Mercator (UTM) projection or its inverse.
 *    
 * REFERENCES
 *
 *    Further information on UTM can be found in the Reuse Manual.
 *
 *    UTM originated from :  U.S. Army Topographic Engineering Center
 *                           Geospatial Information Division
 *                           7701 Telegraph Road
 *                           Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    UTM has no restrictions.
 *
 * ENVIRONMENT
 *
 *    UTM was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. MSDOS with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date        Description
 *    ----        -----------
 *    2-27-07     Original C++ Code
 *    5-02-11     DR 28872, interface added to allow setting zone override
 *                for each fromGeodetic call.  The override in the call has
 *                precedence over what is set in the class constructor.
 *    5-09-11     DR 28908, add default constructor
 * 
 *    1/16/2016   A. Layne MSP_DR30125 Updated constructor to receive ellipsoid 
 *				  code from callers
 */


#include <map>
#include "CoordinateSystem.h"


namespace MSP
{
   namespace CCS
   {
      class UTMParameters;
      class TransverseMercator;
      class UTMCoordinates;
      class GeodeticCoordinates;


      /***********************************************************************/
      /*
       *                              DEFINES
       */

      class MSP_DTCC_API UTM : public CoordinateSystem
      {
         public:

            /*
             * The constructor receives the ellipsoid parameters and
             * UTM zone override parameter as inputs, and sets the
             * corresponding state variables.  If any errors occur,
             * an exception is thrown with a description of the error.
             *
             *    ellipsoidSemiMajorAxis : Semi-major axis in meters (input)
             *    ellipsoidFlattening  : Flattening of ellipsoid     (input)
             *    override  : UTM override zone, 0 indicates no override (input)
             */

            UTM();
            
            UTM(
               double ellipsoidSemiMajorAxis,
               double ellipsoidFlattening,
               char   *ellipsoidCode,
               long   override = 0
               );


            UTM( const UTM &u );


	    ~UTM( void );


            UTM& operator=( const UTM &u );


            /*
             * The function getParameters returns the current ellipsoid
             * parameters and UTM zone override parameter.
             *
             * ellipsoidSemiMajorAxis : Semi-major axis    (meters) (output)
             * ellipsoidFlattening    : Flattening of ellipsoid	    (output)
             * override : UTM override zone, zero indicates no override (output)
             */

            UTMParameters* getParameters() const;


            /*
             * The function convertFromGeodetic converts geodetic (latitude and
             * longitude) coordinates to UTM projection (zone, hemisphere,
             * easting and northing) coordinates according to the current
             * ellipsoid and UTM zone override parameters.  If any errors occur,
             * an exception is thrown with a description of the error.
             *
             *    longitude       : Longitude in radians              (input)
             *    latitude        : Latitude in radians               (input)
             *    utmZoneOverride : zone override                     (input)
             *    zone            : UTM zone                          (output)
             *    hemisphere      : North or South hemisphere         (output)
             *    easting         : Easting (X) in meters             (output)
             *    northing        : Northing (Y) in meters            (output)
             */

            MSP::CCS::UTMCoordinates* convertFromGeodetic(
               MSP::CCS::GeodeticCoordinates* geodeticCoordinates,
               int                            utmZoneOverride = 0 );


            /*
             * The function convertToGeodetic converts UTM projection (zone, 
             * hemisphere, easting and northing) coordinates to geodetic
             * (latitude and  longitude) coordinates, according to the 
             * current ellipsoid parameters.  If any errors occur,
             * an exception is thrown with a description of the error.
             *
             *    zone          : UTM zone                             (input)
             *    hemisphere    : North or South hemisphere            (input)
             *    easting       : Easting (X) in meters                (input)
             *    northing      : Northing (Y) in meters               (input)
             *    longitude     : Longitude in radians                 (output)
             *    latitude      : Latitude in radians                  (output)
             */

            MSP::CCS::GeodeticCoordinates* convertToGeodetic(
               MSP::CCS::UTMCoordinates* utmCoordinates );

         private:
            char   ellipsCode[3];

            std::map< int, TransverseMercator* > transverseMercatorMap;

            long UTM_Override;          /* Zone override flag */
      };
   }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
