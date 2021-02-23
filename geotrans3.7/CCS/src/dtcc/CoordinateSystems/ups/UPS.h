// CLASSIFICATION: UNCLASSIFIED

#ifndef UPS_H
#define UPS_H

/********************************************************************/
/* RSC IDENTIFIER: UPS
 *
 *
 * ABSTRACT
 *
 *    This component provides conversions between geodetic (latitude
 *    and longitude) coordinates and Universal Polar Stereographic (UPS)
 *    projection (hemisphere, easting, and northing) coordinates.
 *
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an 
 *    invalid value is found the error code is combined with the 
 *    current error code using the bitwise or.  This combining allows  
 *    multiple error codes to be returned. The possible error codes 
 *    are:
 *
 *         UPS_NO_ERROR           : No errors occurred in function
 *         UPS_LAT_ERROR          : Latitude outside of valid range
 *                                   (North Pole: 83.5 to 90,
 *                                    South Pole: -79.5 to -90)
 *         UPS_LON_ERROR          : Longitude outside of valid range
 *                                   (-180 to 360 degrees)
 *         UPS_HEMISPHERE_ERROR   : Invalid hemisphere ('N' or 'S')
 *         UPS_EASTING_ERROR      : Easting outside of valid range,
 *                                   (0 to 4,000,000m)
 *         UPS_NORTHING_ERROR     : Northing outside of valid range,
 *                                   (0 to 4,000,000m)
 *         UPS_A_ERROR            : Semi-major axis less than or equal to zero
 *         UPS_INV_F_ERROR        : Inverse flattening outside of valid range
 *								  	               (250 to 350)
 *
 *
 * REUSE NOTES
 *
 *    UPS is intended for reuse by any application that performs a Universal
 *    Polar Stereographic (UPS) projection.
 *
 *
 * REFERENCES
 *
 *    Further information on UPS can be found in the Reuse Manual.
 *
 *    UPS originated from :  U.S. Army Topographic Engineering Center
 *                           Geospatial Information Division
 *                           7701 Telegraph Road
 *                           Alexandria, VA  22310-3864
 *
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 *
 * RESTRICTIONS
 *
 *    UPS has no restrictions.
 *
 *
 * ENVIRONMENT
 *
 *    UPS was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC version 2.8.1
 *    2. Windows 95 with MS Visual C++ version 6
 *
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    2-27-07          Original Code
 *
 *
 */


#include <map>
#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class PolarStereographic;
    class UPSCoordinates;
    class GeodeticCoordinates;


    /**********************************************************************/
    /*
     *                        DEFINES
     */

    class UPS : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and sets
       * the corresponding state variables. If any errors occur, an exception 
       * is thrown with a description of the error.
       *
       *   ellipsoidSemiMajorAxis     : Semi-major axis of ellipsoid in meters (input)
       *   ellipsoidFlattening        : Flattening of ellipsoid					       (input)
       */

	    UPS( double ellipsoidSemiMajorAxis, double ellipsoidFlattening );


      UPS( const UPS &u );


	    ~UPS( void );


      UPS& operator=( const UPS &u );


      /*
       * The function convertFromGeodetic converts geodetic (latitude and
       * longitude) coordinates to UPS (hemisphere, easting, and northing)
       * coordinates, according to the current ellipsoid parameters. 
       * If any errors occur, an exception is thrown with a description of the error.
       *
       *    latitude      : Latitude in radians                       (input)
       *    longitude     : Longitude in radians                      (input)
       *    hemisphere    : Hemisphere either 'N' or 'S'              (output)
       *    easting       : Easting/X in meters                       (output)
       *    northing      : Northing/Y in meters                      (output)
       */

      MSP::CCS::UPSCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates );


      /*
       * The function convertToGeodetic converts UPS (hemisphere, easting, 
       * and northing) coordinates to geodetic (latitude and longitude) coordinates
       * according to the current ellipsoid parameters.  If any errors occur, 
       * an exception is thrown with a description of the error.
       *
       *    hemisphere    : Hemisphere either 'N' or 'S'              (input)
       *    easting       : Easting/X in meters                       (input)
       *    northing      : Northing/Y in meters                      (input)
       *    latitude      : Latitude in radians                       (output)
       *    longitude     : Longitude in radians                      (output)
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::UPSCoordinates* upsCoordinates );

    private:

      std::map< char, PolarStereographic* > polarStereographicMap;

      double UPS_Origin_Latitude;  /*set default = North Hemisphere */
    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
