// CLASSIFICATION: UNCLASSIFIED

#ifndef Sinusoidal_H
#define Sinusoidal_H

/***************************************************************************/
/* RSC IDENTIFIER: SINUSOIDAL
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates 
 *    (latitude and longitude in radians) and Sinusoid projection coordinates
 *    (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          SINU_NO_ERROR           : No errors occurred in function
 *          SINU_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          SINU_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          SINU_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~20,000,000 m,
 *                                       depending on ellipsoid parameters)
 *          SINU_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~10,000,000 m,
 *                                       depending on ellipsoid parameters)
 *          SINU_CENT_MER_ERROR     : Origin longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          SINU_A_ERROR            : Semi-major axis less than or equal to zero
 *          SINU_INV_F_ERROR        : Inverse flattening outside of valid range
 *								  	                  (250 to 350)
 *
 * REUSE NOTES
 *
 *    SINUSOIDAL is intended for reuse by any application that performs a
 *    Sinusoid projection or its inverse.
 *    
 * REFERENCES
 *
 *    Further information on SINUSOIDAL can be found in the Reuse Manual.
 *
 *    SINUSOIDAL originated from :  U.S. Army Topographic Engineering Center
 *                                  Geospatial Information Division
 *                                  7701 Telegraph Road
 *                                  Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    SINUSOIDAL has no restrictions.
 *
 * ENVIRONMENT
 *
 *    SINUSOIDAL was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    07-15-99          Original Code
 *    03-05-07          Original C++ Code
 *
 */


#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class MapProjection3Parameters;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class Sinusoidal : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * Sinusoidal projection parameters as inputs, and sets the corresponding state
       * variables.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    ellipsoidSemiMajorAxis   : Semi-major axis of ellipsoid, in meters   (input)
       *    ellipsoidFlattening      : Flattening of ellipsoid						       (input)
       *    centralMeridian          : Longitude in radians at the center of     (input)
       *                               the projection
       *    falseEasting             : A coordinate value in meters assigned to the
       *                               central meridian of the projection.       (input)
       *    falseNorthing            : A coordinate value in meters assigned to the
       *                               origin latitude of the projection         (input)
       */

	    Sinusoidal( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double falseEasting, double falseNorthing );


      Sinusoidal( const Sinusoidal &s );


	    ~Sinusoidal( void );


      Sinusoidal& operator=( const Sinusoidal &s );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters, and Sinusoidal projection parameters.
       *
       *    ellipsoidSemiMajorAxis : Semi-major axis of ellipsoid, in meters   (output)
       *    ellipsoidFlattening    : Flattening of ellipsoid						       (output)
       *    centralMeridian        : Longitude in radians at the center of     (output)
       *                             the projection
       *    falseEasting           : A coordinate value in meters assigned to the
       *                             central meridian of the projection.       (output)
       *    falseNorthing          : A coordinate value in meters assigned to the
       *                             origin latitude of the projection         (output)
       */

      MapProjection3Parameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts geodetic (latitude and
       * longitude) coordinates to Sinusoidal projection (easting and northing)
       * coordinates, according to the current ellipsoid and Sinusoidal projection
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
       * The function convertToGeodetic converts Sinusoidal projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid and Sinusoidal projection
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
      double es2;              /* Eccentricity (0.08181919084262188000) squared         */
      double es4;              /* es2 * es2 */
      double es6;              /* es4 * es2 */
      double c0;               /* 1 - es2 / 4.0 - 3.0 * es4 / 64.0 - 5.0 * es6 / 256.0 */
      double c1;               /* 3.0 * es2 / 8.0 + 3.0 * es4 / 32.0 + 45.0 * es6 / 1024.0 */
      double c2;               /* 15.0 * es4 / 256.0 + 45.0 * es6 / 1024.0 */
      double c3;               /* 35.0 * es6 / 3072.0 */
      double a0;               /* 3.0 * e1 / 2.0 - 27.0 * e3 / 32.0 */
      double a1;               /* 21.0 * e2 / 16.0 - 55.0 * e4 / 32.0 */
      double a2;               /* 151.0 * e3 / 96.0 */
      double a3;               /* 1097.0 * e4 / 512.0 */

      /* Sinusoid projection Parameters */
      double Sinu_Origin_Long;                  /* Longitude of origin in radians    */
      double Sinu_False_Northing;               /* False northing in meters          */
      double Sinu_False_Easting;                /* False easting in meters           */

      /* Maximum variance for easting and northing values for WGS 84. */
      double Sinu_Max_Easting;
      double Sinu_Min_Easting;
      double Sinu_Delta_Northing;

    };
  }
}
#endif 


// CLASSIFICATION: UNCLASSIFIED
