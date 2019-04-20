// CLASSIFICATION: UNCLASSIFIED

#ifndef Bonne_H
#define Bonne_H

/***************************************************************************/
/* RSC IDENTIFIER: BONNE
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Bonne projection coordinates
 *    (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          BONN_NO_ERROR           : No errors occurred in function
 *          BONN_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          BONN_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          BONN_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~20,500,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          BONN_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~23,500,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          BONN_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          BONN_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          BONN_A_ERROR            : Semi-major axis less than or equal to zero
 *          BONN_INV_F_ERROR        : Inverse flattening outside of valid range
 *									                    (250 to 350)
 *
 * REUSE NOTES
 *
 *    BONNE is intended for reuse by any application that performs a
 *    Bonne projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on BONNE can be found in the Reuse Manual.
 *
 *    BONNE originated from :  U.S. Army Topographic Engineering Center
 *                             Geospatial Information Division
 *                             7701 Telegraph Road
 *                             Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    BONNE has no restrictions.
 *
 * ENVIRONMENT
 *
 *    BONNE was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC 2.8.1
 *    2. MS Windows 95 with MS Visual C++ 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    04-16-99          Original Code
 *    03-05-07          Original C++ Code
 *
 */


#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class Sinusoidal;
    class MapProjection4Parameters;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class Bonne : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * Bonne projection parameters as inputs, and sets the corresponding state
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

	    Bonne( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing );


      Bonne( const Bonne &b );


	    ~Bonne( void );


      Bonne& operator=( const Bonne &b );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters and Bonne projection parameters.
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
       * longitude) coordinates to Bonne projection (easting and northing)
       * coordinates, according to the current ellipsoid and Bonne projection
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
       * The function convertToGeodetic converts Bonne projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid and Bonne projection
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
    
      Sinusoidal* sinusoidal;

      /* Ellipsoid Parameters, default to WGS 84 */
      double es2;         /* Eccentricity (0.08181919084262188000) squared  */
      double es4;         /* es2 * es2 */
      double es6;         /* es4 * es2 */
      double M1;          /* Bonn_M(Bonna,c0lat,c1s2lat,c2s4lat,c3s6lat) */
      double m1;          /* Bonn_m(coslat,sinlat,es2) */
      double c0;          /* 1 - es2 / 4.0 - 3.0 * es4 / 64.0 - 5.0 * es6 / 256.0 */
      double c1;          /* 3.0 * es2 / 8.0 + 3.0 * es4 / 32.0 + 45.0 * es6 / 1024.0 */
      double c2;          /* 15.0 * es4 / 256.0 + 45.0 * es6 / 1024.0 */
      double c3;          /* 35.0 * es6 / 3072.0 */
      double a0;          /* 3.0 * e1 / 2.0 - 27.0 * e3 / 32.0 */
      double a1;          /* 21.0 * e2 / 16.0 - 55.0 * e4 / 32.0 */
      double a2;          /* 151.0 * e3 / 96.0 */
      double a3;          /* 1097.0 * e4 / 512.0 */

      /* Bonne projection Parameters */
      double  Bonn_Origin_Lat;               /* Latitude of origin in radians     */
      double  Bonn_Origin_Long;              /* Longitude of origin in radians    */
      double  Bonn_False_Northing;           /* False northing in meters          */
      double  Bonn_False_Easting;            /* False easting in meters           */
      double  Sin_Bonn_Origin_Lat;           /* sin(Bonn_Origin_Lat)              */
      double  Bonn_am1sin;                   /* Bonn_a * m1 / Sin_Bonn_Origin_Lat */

      /* Maximum variance for easting and northing values for WGS 84. */
      double  Bonn_Max_Easting;
      double  Bonn_Min_Easting;
      double  Bonn_Delta_Northing;


      double bonnm( double coslat, double sinlat );

      double bonnM( double c0lat, double c1s2lat, double c2s4lat, double c3s6lat );

      double floatEq( double x, double v, double epsilon );
    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
