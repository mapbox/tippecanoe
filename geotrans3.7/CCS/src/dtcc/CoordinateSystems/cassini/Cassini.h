// CLASSIFICATION: UNCLASSIFIED

#ifndef Cassini_H
#define Cassini_H

/***************************************************************************/
/* RSC IDENTIFIER: CASSINI
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Cassini projection coordinates
 *    (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          CASS_NO_ERROR           : No errors occurred in function
 *          CASS_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          CASS_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          CASS_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~20,000,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          CASS_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~57,000,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          CASS_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          CASS_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          CASS_A_ERROR            : Semi-major axis less than or equal to zero
 *          CASS_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                      (250 to 350)
 *      CASS_LON_WARNING        : Distortion will result if longitude is more
 *                                      than 4 degrees from the Central Meridian
 *
 * REUSE NOTES
 *
 *    CASSINI is intended for reuse by any application that performs a
 *    Cassini projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on CASSINI can be found in the Reuse Manual.
 *
 *    CASSINI originated from :  U.S. Army Topographic Engineering Center
 *                               Geospatial Information Division
 *                               7701 Telegraph Road
 *                               Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    CASSINI has no restrictions.
 *
 * ENVIRONMENT
 *
 *    CASSINI was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC 2.8.1
 *    2. MS Windows 95 with MS Visual C++ 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    04-16-99          Original Code
 *    03-07-07          Original C++ Code
 *
 */


#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class MapProjection4Parameters;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class Cassini : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * Cassini projection parameters as inputs, and sets the corresponding state
       * variables.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
       *    ellipsoidFlattening     : Flattening of ellipsoid                   (input)
       *    centralMeridian         : Longitude in radians at the center of     (input)
       *                              the projection
       *    originLatitude          : Latitude in radians at which the          (input)
       *                              point scale factor is 1.0
       *    falseEasting            : A coordinate value in meters assigned to the
       *                              central meridian of the projection.       (input)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                              origin latitude of the projection         (input)
       */

	    Cassini( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing );


      Cassini( const Cassini &c );


	    ~Cassini( void );


      Cassini& operator=( const Cassini &c );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters, Cassini projection parameters.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
       *    ellipsoidFlattening     : Flattening of ellipsoid                   (output)
       *    centralMeridian         : Longitude in radians at the center of     (output)
       *                              the projection
       *    originLatitude          : Latitude in radians at which the          (output)
       *                              point scale factor is 1.0
       *    falseEasting            : A coordinate value in meters assigned to the
       *                              central meridian of the projection.       (output)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                              origin latitude of the projection         (output)
       */

      MapProjection4Parameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts geodetic (latitude and
       * longitude) coordinates to Cassini projection (easting and northing)
       * coordinates, according to the current ellipsoid and Cassini projection
       * parameters.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    longitude         : Longitude (lambda) in radians       (input)
       *    latitude          : Latitude (phi) in radians           (input)
       *    easting           : Easting (X) in meters               (output)
       *    northing          : Northing (Y) in meters              (output)
       */

      MSP::CCS::MapProjectionCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates );


      /*
       * The function convertToGeodetic converts Cassini projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid and Cassini projection
       * coordinates.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    easting           : Easting (X) in meters                  (input)
       *    northing          : Northing (Y) in meters                 (input)
       *    longitude         : Longitude (lambda) in radians          (output)
       *    latitude          : Latitude (phi) in radians              (output)
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates );

    private:
    
      /* Ellipsoid Parameters, default to WGS 84 */
      double es2;              /* Eccentricity (0.08181919084262188000) squared  */
      double es4;              /* es2 * es2 */
      double es6;              /* es4 * es2 */
      double M0;
      double c0;               /* 1 - es2 / 4.0 - 3.0 * es4 / 64.0 - 5.0 * es6 / 256.0 */
      double c1;               /* 3.0 * es2 / 8.0 + 3.0 * es4 / 32.0 + 45.0 * es6 / 1024.0 */
      double c2;               /* 15.0 * es4 / 256.0 + 45.0 * es6 / 1024.0 */
      double c3;               /* 35.0 * es6 / 3072.0 */
      double One_Minus_es2;    /* 1.0 - es2 */
      double a0;               /* 3.0 * e1 / 2.0 - 27.0 * e3 / 32.0 */
      double a1;               /* 21.0 * e2 / 16.0 - 55.0 * e4 / 32.0 */
      double a2;               /* 151.0 * e3 / 96.0 */
      double a3;               /* 1097.0 * e4 /512.0 */

      /* Cassini projection Parameters */
      double Cass_Origin_Lat;                   /* Latitude of origin in radians     */
      double Cass_Origin_Long;                  /* Longitude of origin in radians    */
      double Cass_False_Northing;               /* False northing in meters          */
      double Cass_False_Easting;                /* False easting in meters           */

      /* Maximum variance for easting and northing values for WGS 84.*/
      double Cass_Min_Easting;
      double Cass_Max_Easting;
      double Cass_Min_Northing;
      double Cass_Max_Northing;


      double cassM( double c0lat, double c1s2lat, double c2s4lat, double c3s6lat );

      double cassRd( double sinlat );

      double floatEq( double x, double v, double epsilon );
    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
