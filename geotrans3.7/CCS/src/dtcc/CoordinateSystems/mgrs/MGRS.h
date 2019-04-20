// CLASSIFICATION: UNCLASSIFIED

#ifndef MGRS_H
#define MGRS_H

/***************************************************************************/
/* RSC IDENTIFIER:  MGRS
 *
 * ABSTRACT
 *
 *    This component converts between geodetic coordinates (latitude and 
 *    longitude) and Military Grid Reference System (MGRS) coordinates. 
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          MGRS_NO_ERROR          : No errors occurred in function
 *          MGRS_LAT_ERROR         : Latitude outside of valid range 
 *                                    (-90 to 90 degrees)
 *          MGRS_LON_ERROR         : Longitude outside of valid range
 *                                    (-180 to 360 degrees)
 *          MGRS_STR_ERROR         : An MGRS string error: string too long,
 *                                    too short, or badly formed
 *          MGRS_PRECISION_ERROR   : The precision must be between 0 and 5 
 *                                    inclusive.
 *          MGRS_A_ERROR           : Semi-major axis less than or equal to zero
 *          MGRS_INV_F_ERROR       : Inverse flattening outside of valid range
 *			              (250 to 350)
 *          MGRS_EASTING_ERROR     : Easting outside of valid range
 *                                    (100,000 to 900,000 meters for UTM)
 *                                    (0 to 4,000,000 meters for UPS)
 *          MGRS_NORTHING_ERROR    : Northing outside of valid range
 *                                    (0 to 10,000,000 meters for UTM)
 *                                    (0 to 4,000,000 meters for UPS)
 *          MGRS_ZONE_ERROR        : Zone outside of valid range (1 to 60)
 *          MGRS_HEMISPHERE_ERROR  : Invalid hemisphere ('N' or 'S')
 *
 * REUSE NOTES
 *
 *    MGRS is intended for reuse by any application that does conversions
 *    between geodetic coordinates and MGRS coordinates.
 *
 * REFERENCES
 *
 *    Further information on MGRS can be found in the Reuse Manual.
 *
 *    MGRS originated from : U.S. Army Topographic Engineering Center
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
 *
 * ENVIRONMENT
 *
 *    MGRS was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC version 2.8.1
 *    2. Windows 95 with MS Visual C++ version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    2-27-07          Original Code
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

    #define MGRS_LETTERS 3

    /**********************************************************************/
    /*
     *                        DEFINES
     */

     class MSP_DTCC_API MGRS : public CoordinateSystem
     {
        public:

      /*
       * The constructor receives the ellipsoid parameters and sets
       * the corresponding state variables. If any errors occur,
       * an exception is thrown with a description of the error.
       *
       *   ellipsoidSemiMajorAxis : Semi-major axis of ellipsoid (m)    (input)
       *   ellipsoidFlattening    : Flattening of ellipsoid             (input)
       *   ellipsoid_Code         : 2-letter code for ellipsoid         (input)
       */

           MGRS(
              double ellipsoidSemiMajorAxis,
              double ellipsoidFlattening,
              char*  ellipsoidCode );
           

           MGRS( const MGRS &m );


           ~MGRS( void );


           MGRS& operator=( const MGRS &m );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters.
       *
       *  ellipsoidSemiMajorAxis : Semi-major axis of ellipsoid, (m)   (output)
       *  ellipsoidFlattening    : Flattening of ellipsoid             (output)
       *  ellipsoidCode          : 2-letter code for ellipsoid         (output)
       */

           EllipsoidParameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts Geodetic (latitude and
       * longitude) coordinates to an MGRS coordinate string, according to the 
       * current ellipsoid parameters. If any errors occur,
       * an exception is thrown with a description of the error.
       *
       *    latitude      : Latitude in radians              (input)
       *    longitude     : Longitude in radians             (input)
       *    precision     : Precision level of MGRS string   (input)
       *    MGRSString    : MGRS coordinate string           (output)
       *  
       */

           MSP::CCS::MGRSorUSNGCoordinates* convertFromGeodetic(
              MSP::CCS::GeodeticCoordinates* geodeticCoordinates,
              long precision );

      /*
       * The function convertToGeodetic converts an MGRS coordinate string
       * to Geodetic (latitude and longitude) coordinates 
       * according to the current ellipsoid parameters.  If any errors occur,
       * an exception is thrown with a description of the error.
       *
       *    MGRS       : MGRS coordinate string           (input)
       *    latitude   : Latitude in radians              (output)
       *    longitude  : Longitude in radians             (output)
       *  
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic(
         MSP::CCS::MGRSorUSNGCoordinates* mgrsCoordinates );


      /*
       * The function convertFromUTM converts UTM (zone, easting, and
       * northing) coordinates to an MGRS coordinate string, according to the 
       * current ellipsoid parameters.  If any errors occur,
       * an exception is thrown with a description of the error.
       *
       *    zone       : UTM zone                         (input)
       *    hemisphere : North or South hemisphere        (input)
       *    easting    : Easting (X) in meters            (input)
       *    northing   : Northing (Y) in meters           (input)
       *    precision  : Precision level of MGRS string   (input)
       *    MGRSString : MGRS coordinate string           (output)
       */

      MSP::CCS::MGRSorUSNGCoordinates* convertFromUTM(
         UTMCoordinates* utmCoordinates, long precision );


      /*
       * The function convertToUTM converts an MGRS coordinate string
       * to UTM projection (zone, hemisphere, easting and northing) coordinates
       * according to the current ellipsoid parameters.  If any errors occur,
       * an exception is thrown with a description of the error.
       *
       *    MGRSString : MGRS coordinate string           (input)
       *    zone       : UTM zone                         (output)
       *    hemisphere : North or South hemisphere        (output)
       *    easting    : Easting (X) in meters            (output)
       *    northing   : Northing (Y) in meters           (output)
       */

      MSP::CCS::UTMCoordinates* convertToUTM(
         MSP::CCS::MGRSorUSNGCoordinates* mgrsorUSNGCoordinates );

      /*
       * The function convertFromUPS converts UPS (hemisphere, easting, 
       * and northing) coordinates to an MGRS coordinate string according to 
       * the current ellipsoid parameters.  If any errors occur,
       * an exception is thrown with a description of the error.
       *
       *    hemisphere    : Hemisphere either 'N' or 'S'     (input)
       *    easting       : Easting/X in meters              (input)
       *    northing      : Northing/Y in meters             (input)
       *    precision     : Precision level of MGRS string   (input)
       *    MGRSString    : MGRS coordinate string           (output)
       */

      MSP::CCS::MGRSorUSNGCoordinates* convertFromUPS(
         MSP::CCS::UPSCoordinates* upsCoordinates, long precision );


      /*
       * The function convertToUPS converts an MGRS coordinate string
       * to UPS (hemisphere, easting, and northing) coordinates, according 
       * to the current ellipsoid parameters. If any errors occur,
       * an exception is thrown with a description of the error.
       *
       *    MGRSString    : MGRS coordinate string           (input)
       *    hemisphere    : Hemisphere either 'N' or 'S'     (output)
       *    easting       : Easting/X in meters              (output)
       *    northing      : Northing/Y in meters             (output)
       */

      MSP::CCS::UPSCoordinates* convertToUPS(
         MSP::CCS::MGRSorUSNGCoordinates* mgrsorUSNGCoordinates );

    private:

      UPS* ups;
      UTM* utm;
    
      char MGRSEllipsoidCode[3];


      /*
       * The function fromUTM calculates an MGRS coordinate string
       * based on the zone, latitude, easting and northing.
       *
       *    zone       : Zone number             (input)
       *    hemisphere : Hemisphere              (input)
       *    longitude  : Longitude in radians    (input)
       *    latitude   : Latitude in radians     (input)
       *    easting    : Easting                 (input)
       *    northing   : Northing                (input)
       *    precision  : Precision               (input)
       *    MGRSString : MGRS coordinate string  (output)
       */

      MSP::CCS::MGRSorUSNGCoordinates* fromUTM(
         MSP::CCS::UTMCoordinates* utmCoordinates,
         double longitude,
         double latitude,
         long precision );

      /*
       * The function toUTM converts an MGRS coordinate string
       * to UTM projection (zone, hemisphere, easting and northing) coordinates
       * according to the current ellipsoid parameters.  If any errors occur,
       * an exception is thrown with a description of the error.
       *
       *    MGRSString : MGRS coordinate string           (input)
       *    zone       : UTM zone                         (output)
       *    hemisphere : North or South hemisphere        (output)
       *    easting    : Easting (X) in meters            (output)
       *    northing   : Northing (Y) in meters           (output)
       */

      MSP::CCS::UTMCoordinates* toUTM(
         long zone,
         long letters[MGRS_LETTERS],
         double easting,
         double northing,
         long in_precision );


      /*
       * The function fromUPS converts UPS (hemisphere, easting, 
       * and northing) coordinates to an MGRS coordinate string according to 
       * the current ellipsoid parameters.
       *
       *    hemisphere    : Hemisphere either 'N' or 'S'     (input)
       *    easting       : Easting/X in meters              (input)
       *    northing      : Northing/Y in meters             (input)
       *    precision     : Precision level of MGRS string   (input)
       *    MGRSString    : MGRS coordinate string           (output)
       */

      MSP::CCS::MGRSorUSNGCoordinates* fromUPS(
         MSP::CCS::UPSCoordinates* upsCoordinates,
         long precision );

      /*
       * The function toUPS converts an MGRS coordinate string
       * to UPS (hemisphere, easting, and northing) coordinates, according
       * to the current ellipsoid parameters. If any errors occur, an
       * exception is thrown with a description of the error.
       *
       *    MGRSString    : MGRS coordinate string           (input)
       *    hemisphere    : Hemisphere either 'N' or 'S'     (output)
       *    easting       : Easting/X in meters              (output)
       *    northing      : Northing/Y in meters             (output)
       */

      MSP::CCS::UPSCoordinates* toUPS(
         long letters[MGRS_LETTERS],
         double easting, double northing );

      /*
       * The function getGridValues sets the letter range used for 
       * the 2nd letter in the MGRS coordinate string, based on the set 
       * number of the utm zone. It also sets the pattern offset using a
       * value of A for the second letter of the grid square, based on 
       * the grid pattern and set number of the utm zone.
       *
       *    zone            : Zone number             (input)
       *    ltr2_low_value  : 2nd letter low number   (output)
       *    ltr2_high_value : 2nd letter high number  (output)
       *    pattern_offset  : Pattern offset          (output)
       */

      void getGridValues(
         long zone,
         long* ltr2_low_value,
         long* ltr2_high_value,
         double* pattern_offset );

    
      /*
       * The function getLatitudeBandMinNorthing receives a latitude band 
       * letter and uses the Latitude_Band_Table to determine the 
       * minimum northing and northing offset for that latitude band letter.
       *
       *   letter          : Latitude band letter               (input)
       *   min_northing    : Minimum northing for that letter	(output)
       *   northing_offset : Latitude band northing offset  	(output)
       */

      void getLatitudeBandMinNorthing(
         long letter, double* min_northing, double* northing_offset );


      /*
       * The function inLatitudeRange receives a latitude band letter
       * and uses the Latitude_Band_Table to determine if the latitude
       * falls within the band boundaries for that latitude band letter.  
       *
       *   letter   : Latitude band letter                        (input)
       *   latitude : Latitude to test                            (input)
       *   border   : Border added to band in radians             (input)
       */

      bool inLatitudeRange( long letter, double latitude, double border );
   

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
