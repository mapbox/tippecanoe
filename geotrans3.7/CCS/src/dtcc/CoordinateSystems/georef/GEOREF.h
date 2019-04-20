// CLASSIFICATION: UNCLASSIFIED

#ifndef GEOREF_H
#define GEOREF_H

/***************************************************************************/
/* RSC IDENTIFIER: GEOREF
 *
 * ABSTRACT
 *
 *    This component provides conversions from Geodetic coordinates (latitude
 *    and longitude in radians) to a GEOREF coordinate string.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          GEOREF_NO_ERROR          : No errors occurred in function
 *          GEOREF_LAT_ERROR         : Latitude outside of valid range 
 *                                      (-90 to 90 degrees)
 *          GEOREF_LON_ERROR         : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          GEOREF_STR_ERROR         : A GEOREF string error: string too long,
 *                                      string too short, or string length
 *                                      not even.
 *          GEOREF_STR_LAT_ERROR     : The latitude part of the GEOREF string
 *                                     (second or fourth character) is invalid.
 *          GEOREF_STR_LON_ERROR     : The longitude part of the GEOREF string
 *                                     (first or third character) is invalid.
 *          GEOREF_STR_LAT_MIN_ERROR : The latitude minute part of the GEOREF
 *                                      string is greater than 60.
 *          GEOREF_STR_LON_MIN_ERROR : The longitude minute part of the GEOREF
 *                                      string is greater than 60.
 *          GEOREF_PRECISION_ERROR   : The precision must be between 0 and 5 
 *                                      inclusive.
 *
 *
 * REUSE NOTES
 *
 *    GEOREF is intended for reuse by any application that performs a 
 *    conversion between Geodetic and GEOREF coordinates.
 *    
 * REFERENCES
 *
 *    Further information on GEOREF can be found in the Reuse Manual.
 *
 *    GEOREF originated from :  U.S. Army Topographic Engineering Center
 *                              Geospatial Information Division
 *                              7701 Telegraph Road
 *                              Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    GEOREF has no restrictions.
 *
 * ENVIRONMENT
 *
 *    GEOREF was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC version 2.8.1
 *    2. Windows 95 with MS Visual C++ version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    02-20-97          Original Code
 *    03-02-07          Original C++ Code
 */


#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class GEOREFCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    const long GEOREF_STR_LAT_MIN_ERROR = 0x0020;
    const long GEOREF_STR_LON_MIN_ERROR = 0x0040;


    class GEOREF : public CoordinateSystem
    {
    public:

	    GEOREF();


      GEOREF( const GEOREF &g );


	    ~GEOREF( void );


      GEOREF& operator=( const GEOREF &g );


      /*   
       *  The function convertFromGeodetic converts Geodetic (latitude and longitude in radians)
       *  coordinates to a GEOREF coordinate string.  Precision specifies the
       *  number of digits in the GEOREF string for latitude and longitude:
       *                                  0 for nearest degree
       *                                  1 for nearest ten minutes
       *                                  2 for nearest minute
       *                                  3 for nearest tenth of a minute
       *                                  4 for nearest hundredth of a minute
       *                                  5 for nearest thousandth of a minute
       *
       *    longitude    : Longitude in radians.                  (input)
       *    latitude     : Latitude in radians.                   (input)
       *    precision    : Precision specified by the user.       (input)
       *    GEOREFString : GEOREF coordinate string.              (output)
       *
       */

      MSP::CCS::GEOREFCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates, long precision );


      /*
       *  The function convertToGeodetic converts a GEOREF coordinate string to Geodetic (latitude
       *  and longitude in radians) coordinates.
       *
       *    GEOREFString : GEOREF coordinate string.     (input)
       *    longitude    : Longitude in radians.         (output)
       *    latitude     : Latitude in radians.          (output)
       *
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::GEOREFCoordinates* GEOREFString );

    private:
    
    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
