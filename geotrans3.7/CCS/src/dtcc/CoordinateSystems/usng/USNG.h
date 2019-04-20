// CLASSIFICATION: UNCLASSIFIED

#ifndef USNG_H
#define USNG_H

/***************************************************************************/
/* RSC IDENTIFIER:  USNG
 *
 * ABSTRACT
 *
 *    This component converts between geodetic coordinates (latitude and
 *    longitude) and United States National Grid (USNG) coordinates.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          USNG_NO_ERROR          : No errors occurred in function
 *          USNG_LAT_ERROR         : Latitude outside of valid range
 *                                    (-90 to 90 degrees)
 *          USNG_LON_ERROR         : Longitude outside of valid range
 *                                    (-180 to 360 degrees)
 *          USNG_STR_ERROR         : An USNG string error: string too long,
 *                                    too short, or badly formed
 *          USNG_PRECISION_ERROR   : The precision must be between 0 and 5
 *                                    inclusive.
 *          USNG_A_ERROR           : Semi-major axis less than or equal to zero
 *          USNG_INV_F_ERROR       : Inverse flattening outside of valid range
 *                                    (250 to 350)
 *          USNG_EASTING_ERROR     : Easting outside of valid range
 *                                    (100,000 to 900,000 meters for UTM)
 *                                    (0 to 4,000,000 meters for UPS)
 *          USNG_NORTHING_ERROR    : Northing outside of valid range
 *                                    (0 to 10,000,000 meters for UTM)
 *                                    (0 to 4,000,000 meters for UPS)
 *          USNG_ZONE_ERROR        : Zone outside of valid range (1 to 60)
 *          USNG_HEMISPHERE_ERROR  : Invalid hemisphere ('N' or 'S')
 *
 * REUSE NOTES
 *
 *    USNG is intended for reuse by any application that does conversions
 *    between geodetic coordinates and USNG coordinates.
 *
 * REFERENCES
 *
 *    Further information on USNG can be found in the Reuse Manual.
 *
 *    USNG originated from : Federal Geographic Data Committee
 *                           590 National Center
 *                           12201 Sunrise Valley Drive
 *                           Reston, VA  22092
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *
 * ENVIRONMENT
 *
 *    USNG was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC version 2.8.1
 *    2. Windows XP with MS Visual C++ version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    3-1-07          Original Code (cloned from MGRS)
 */


#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class UPS;
    class UTM;
    class EllipsoidParameters;
    class MGRSorUSNGCoordinates;
    class GeodeticCoordinates;
    class UPSCoordinates;
    class UTMCoordinates;

    #define USNG_LETTERS 3

    /**********************************************************************/
    /*
     *                        DEFINES
     */

    class USNG : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and sets
       * the corresponding state variables. If any errors occur, an exception 
       * is thrown with a description of the error.
       *
       *   ellipsoidSemiMajorAxis     : Semi-major axis of ellipsoid in meters  (input)
       *   ellipsoidFlattening        : Flattening of ellipsoid					        (input)
       *   ellipsoid_Code             : 2-letter code for ellipsoid             (input)
       */

      USNG( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, char* ellipsoidCode );


      USNG( const USNG &u );


	    ~USNG( void );


      USNG& operator=( const USNG &u );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters.
       *
       *  ellipsoidSemiMajorAxis     : Semi-major axis of ellipsoid, in meters (output)
       *  ellipsoidFlattening        : Flattening of ellipsoid					       (output)
       *  ellipsoidCode              : 2-letter code for ellipsoid             (output)
       */

      EllipsoidParameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts Geodetic (latitude and
       * longitude) coordinates to an USNG coordinate string, according to the 
       * current ellipsoid parameters.  If any errors occur, an exception is 
       * thrown with a description of the error.
       *
       *    latitude      : Latitude in radians              (input)
       *    longitude     : Longitude in radians             (input)
       *    precision     : Precision level of USNG string   (input)
       *    USNGString    : USNG coordinate string           (output)
       *  
       */

      MSP::CCS::MGRSorUSNGCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates, long precision );


      /*
       * The function convertToGeodetic converts an USNG coordinate string
       * to Geodetic (latitude and longitude) coordinates 
       * according to the current ellipsoid parameters.  If any errors occur, 
       * an exception is thrown with a description of the error.
       *
       *    USNG       : USNG coordinate string           (input)
       *    latitude   : Latitude in radians              (output)
       *    longitude  : Longitude in radians             (output)
       *  
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::MGRSorUSNGCoordinates* mgrsCoordinates );


      /*
       * The function convertFromUTM converts UTM (zone, easting, and
       * northing) coordinates to an USNG coordinate string, according to the 
       * current ellipsoid parameters.  If any errors occur, an exception is 
       * thrown with a description of the error.
       *
       *    zone       : UTM zone                         (input)
       *    hemisphere : North or South hemisphere        (input)
       *    easting    : Easting (X) in meters            (input)
       *    northing   : Northing (Y) in meters           (input)
       *    precision  : Precision level of USNG string   (input)
       *    USNGString : USNG coordinate string           (output)
       */

      MSP::CCS::MGRSorUSNGCoordinates* convertFromUTM( UTMCoordinates* utmCoordinates, long precision );


      /*
       * The function convertToUTM converts an USNG coordinate string
       * to UTM projection (zone, hemisphere, easting and northing) coordinates 
       * according to the current ellipsoid parameters.  If any errors occur, an 
       * exception is thrown with a description of the error.
       *
       *    USNGString : USNG coordinate string           (input)
       *    zone       : UTM zone                         (output)
       *    hemisphere : North or South hemisphere        (output)
       *    easting    : Easting (X) in meters            (output)
       *    northing   : Northing (Y) in meters           (output)
       */

      MSP::CCS::UTMCoordinates* convertToUTM( MSP::CCS::MGRSorUSNGCoordinates* mgrsorUSNGCoordinates );


      /*
       * The function convertFromUPS converts UPS (hemisphere, easting, 
       * and northing) coordinates to an USNG coordinate string according to 
       * the current ellipsoid parameters.  If any errors occur, an exception 
       * is thrown with a description of the error.
       *
       *    hemisphere    : Hemisphere either 'N' or 'S'     (input)
       *    easting       : Easting/X in meters              (input)
       *    northing      : Northing/Y in meters             (input)
       *    precision     : Precision level of USNG string   (input)
       *    USNGString    : USNG coordinate string           (output)
       */

      MSP::CCS::MGRSorUSNGCoordinates* convertFromUPS( MSP::CCS::UPSCoordinates* upsCoordinates, long precision );


      /*
       * The function convertToUPS converts an USNG coordinate string
       * to UPS (hemisphere, easting, and northing) coordinates, according 
       * to the current ellipsoid parameters. If any errors occur, an 
       * exception is thrown with a description of the error.
       *
       *    USNGString    : USNG coordinate string           (input)
       *    hemisphere    : Hemisphere either 'N' or 'S'     (output)
       *    easting       : Easting/X in meters              (output)
       *    northing      : Northing/Y in meters             (output)
       */

      MSP::CCS::UPSCoordinates* convertToUPS( MSP::CCS::MGRSorUSNGCoordinates* mgrsorUSNGCoordinates );

    private:
    
      UPS* ups;
      UTM* utm;

      char USNGEllipsoidCode[3];


      /*
       * The function fromUTM calculates an USNG coordinate string
       * based on the zone, latitude, easting and northing.
       *
       *    zone       : Zone number             (input)
       *    latitude   : Latitude in radians     (input)
       *    easting    : Easting                 (input)
       *    northing   : Northing                (input)
       *    precision  : Precision               (input)
       *    USNGString : USNG coordinate string  (output)
       */

      MSP::CCS::MGRSorUSNGCoordinates* fromUTM( MSP::CCS::UTMCoordinates* utmCoordinates, double longitude, double latitude, long precision );


      /*
       * The function toUTM converts an USNG coordinate string
       * to UTM projection (zone, hemisphere, easting and northing) coordinates
       * according to the current ellipsoid parameters.  If any errors occur,
       * an exception is thrown with a description of the error.
       *
       *    USNGString : USNG coordinate string           (input)
       *    zone       : UTM zone                         (output)
       *    hemisphere : North or South hemisphere        (output)
       *    easting    : Easting (X) in meters            (output)
       *    northing   : Northing (Y) in meters           (output)
       */

      UTMCoordinates* toUTM( long zone, long letters[USNG_LETTERS], double easting, double northing, long in_precision );


      /*
       * The function fromUPS converts UPS (hemisphere, easting, 
       * and northing) coordinates to an USNG coordinate string according to 
       * the current ellipsoid parameters.  
       *
       *    hemisphere    : Hemisphere either 'N' or 'S'     (input)
       *    easting       : Easting/X in meters              (input)
       *    northing      : Northing/Y in meters             (input)
       *    precision     : Precision level of USNG string   (input)
       *    USNGString    : USNG coordinate string           (output)
       */

      MSP::CCS::MGRSorUSNGCoordinates* fromUPS( MSP::CCS::UPSCoordinates* upsCoordinates, long precision );

      /*
       * The function toUPS converts an USNG coordinate string
       * to UPS (hemisphere, easting, and northing) coordinates, according
       * to the current ellipsoid parameters. If any errors occur, an
       * exception is thrown with a description of the error.
       *
       *    USNGString    : USNG coordinate string           (input)
       *    hemisphere    : Hemisphere either 'N' or 'S'     (output)
       *    easting       : Easting/X in meters              (output)
       *    northing      : Northing/Y in meters             (output)
       */

      MSP::CCS::UPSCoordinates* toUPS( long letters[USNG_LETTERS], double easting, double northing );

  
      /*
       * The function getGridValues sets the letter range used for 
       * the 2nd letter in the USNG coordinate string, based on the set 
       * number of the utm zone. It also sets the pattern offset using a
       * value of A for the second letter of the grid square, based on 
       * the grid pattern and set number of the utm zone.
       *
       *    zone            : Zone number             (input)
       *    ltr2_low_value  : 2nd letter low number   (output)
       *    ltr2_high_value : 2nd letter high number  (output)
       *    pattern_offset  : Pattern offset          (output)
       */

      void getGridValues( long zone, long* ltr2_low_value, long* ltr2_high_value, double* pattern_offset );


      /*
       * The function getLatitudeBandMinNorthing receives a latitude band letter
       * and uses the Latitude_Band_Table to determine the minimum northing and northing offset
       * for that latitude band letter.
       *
       *   letter          : Latitude band letter             (input)
       *   min_northing    : Minimum northing for that letter	(output)
       *   northing_offset : Latitude band northing offset  	(output)
       */

      void getLatitudeBandMinNorthing( long letter, double* min_northing, double* northing_offset );


      /*
       * The function getLatitudeRange receives a latitude band letter
       * and uses the Latitude_Band_Table to determine the latitude band 
       * boundaries for that latitude band letter.
       *
       *   letter   : Latitude band letter                        (input)
       *   north    : Northern latitude boundary for that letter	(output)
       *   north    : Southern latitude boundary for that letter	(output)
       */

      void getLatitudeRange( long letter, double* north, double* south );

    
      /*
       * The function getLatitudeLetter receives a latitude value
       * and uses the Latitude_Band_Table to determine the latitude band 
       * letter for that latitude.
       *
       *   latitude   : Latitude              (input)
       *   letter     : Latitude band letter  (output)
       */

      void getLatitudeLetter( double latitude, int* letter );
    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
