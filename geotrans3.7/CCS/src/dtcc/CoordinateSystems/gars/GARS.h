// CLASSIFICATION: UNCLASSIFIED

#ifndef GARS_H
#define GARS_H

/***************************************************************************/
/* RSC IDENTIFIER: GARS
 *
 * ABSTRACT
 *
 *    This component provides conversions from Geodetic coordinates (latitude
 *    and longitude in radians) to a GARS coordinate string.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          GARS_NO_ERROR          : No errors occurred in function
 *          GARS_LAT_ERROR         : Latitude outside of valid range 
 *                                      (-90 to 90 degrees)
 *          GARS_LON_ERROR         : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          GARS_STR_ERROR         : A GARS string error: string too long,
 *                                      string too short, invalid numbers/letters
 *          GARS_STR_LAT_ERROR     : The latitude part of the GARS string
 *                                     (fourth and fifth  characters) is invalid.
 *          GARS_STR_LON_ERROR     : The longitude part of the GARS string
 *                                     (first three characters) is invalid.
 *          GARS_STR_15_MIN_ERROR  : The 15 minute part of the GARS
 *                                      string is less than 1 or greater than 4.
 *          GARS_STR_5_MIN_ERROR   : The 5 minute part of the GARS
 *                                      string is less than 1 or greater than 9.
 *          GARS_PRECISION_ERROR   : The precision must be between 0 and 5 
 *                                      inclusive.
 *
 *
 * REUSE NOTES
 *
 *    GARS is intended for reuse by any application that performs a 
 *    conversion between Geodetic and GARS coordinates.
 *    
 * REFERENCES
 *
 *    Further information on GARS can be found in the Reuse Manual.
 *
 *    GARS originated from : 
 *                              
 *                   http://earth-info.nga.mil/GandG/coordsys/grids/gars.html           
 *                              
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    GARS has no restrictions.
 *
 * ENVIRONMENT
 *
 *    GARS was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC version 2.8.1
 *    2. Windows XP with MS Visual C++ version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    07-10-06          Original Code
 *    03-02-07          Original C++ Code
 */


#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class GARSCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class GARS : public CoordinateSystem
    {
    public:

	    GARS();


      GARS( const GARS &g );


	    ~GARS( void );


      GARS& operator=( const GARS &g );


      /*   
       *  The function convertFromGeodetic converts Geodetic (latitude and longitude in radians)
       *  coordinates to a GARS coordinate string.  Precision specifies the
       *  number of digits in the GARS string for latitude and longitude:
       *                                  0: 30 minutes (5 characters)
       *                                  1: 15 minutes (6 characters)
       *                                  2: 5 minutes (7 characters)
       *
       *    longitude   : Longitude in radians.                  (input)
       *    latitude    : Latitude in radians.                   (input)
       *    precision   : Precision specified by the user.       (input)
       *    GARSString  : GARS coordinate string.                (output)
       *
       */

      MSP::CCS::GARSCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates, long precision );


      /*
       *  The function convertToGeodetic converts a GARS coordinate string to Geodetic (latitude
       *  and longitude in radians) coordinates.
       *
       *    GARSString  : GARS coordinate string.     (input)
       *    longitude   : Longitude in radians.       (output)
       *    latitude    : Latitude in radians.        (output)
       *
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::GARSCoordinates* garsCoordinates );
    
    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
