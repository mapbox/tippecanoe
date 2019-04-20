// CLASSIFICATION: UNCLASSIFIED

#ifndef Polyconic_H
#define Polyconic_H

/***************************************************************************/
/* RSC IDENTIFIER: POLYCONIC
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Polyconic projection coordinates
 *    (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          POLY_NO_ERROR           : No errors occurred in function
 *          POLY_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          POLY_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          POLY_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~20,500,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          POLY_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~15,500,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          POLY_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          POLY_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          POLY_A_ERROR            : Semi-major axis less than or equal to zero
 *          POLY_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                      (250 to 350)
 *          POLY_LON_WARNING        : Distortion will result if longitude is more
 *                                     than 90 degrees from the Central Meridian
 *
 * REUSE NOTES
 *
 *    POLYCONIC is intended for reuse by any application that performs a
 *    Polyconic projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on POLYCONIC can be found in the Reuse Manual.
 *
 *    POLYCONIC originated from :  U.S. Army Topographic Engineering Center
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
 *    POLYCONIC has no restrictions.
 *
 * ENVIRONMENT
 *
 *    POLYCONIC was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    10-06-99          Original Code
 *    03-05-07          Original C++ Code
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

    class Polyconic : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * Polyconic projection parameters as inputs, and sets the corresponding state
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

	    Polyconic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing );


      Polyconic( const Polyconic &p );


	    ~Polyconic( void );


      Polyconic& operator=( const Polyconic &p );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters, and Polyconic projection parameters.
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
       * longitude) coordinates to Polyconic projection (easting and northing)
       * coordinates, according to the current ellipsoid and Polyconic projection
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
       * The function convertToGeodetic converts Polyconic projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid and Polyconic projection
       * coordinates.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *    easting           : Easting (X) in meters                  (input)
       *    northing          : Northing (Y) in meters                 (input)
       *    longitude         : Longitude (lambda) in radians          (output)
       *    latitude          : Latitude (phi) in radians              (output)
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates );

    private:
    
      /* Ellipsoid Parameters, default to WGS 84 */
      double es2;             /* Eccentricity (0.08181919084262188000) squared         */
      double es4;             /* es2 * es2 */
      double es6;             /* es4 * es2 */
      double M0;
      double c0;              /* 1 - es2 / 4.0 - 3.0 * es4 / 64.0 - 5.0 * es6 / 256.0 */
      double c1;              /* 3.0 * es2 / 8.0 + 3.0 * es4 / 32.0 + 45.0 * es6 / 1024.0 */
      double c2;              /* 15.0 * es4 / 256.0 + 45.0 * es6 / 1024.0 */
      double c3;              /* 35.0 * es6 / 3072.0 */

      /* Polyconic projection Parameters */
      double Poly_Origin_Lat;                   /* Latitude of origin in radians     */
      double Poly_Origin_Long;                  /* Longitude of origin in radians    */
      double Poly_False_Northing;               /* False northing in meters          */
      double Poly_False_Easting;                /* False easting in meters           */

      /* Maximum variance for easting and northing values for WGS 84.*/
      double Poly_Max_Easting;
      double Poly_Max_Northing;
      double Poly_Min_Easting;
      double Poly_Min_Northing;


      double polyM( double c0lat, double c1s2lat, double c2s4lat, double c3s6lat );

      double floatEq( double x, double v, double epsilon );

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
