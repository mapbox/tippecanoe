// CLASSIFICATION: UNCLASSIFIED

#ifndef TransverseCylindricalEqualArea_H
#define TransverseCylindricalEqualArea_H

/***************************************************************************/
/* RSC IDENTIFIER: TRANSVERSE CYLINDRICAL EQUAL AREA
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Transverse Cylindrical Equal Area
 *    projection coordinates (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          TCEA_NO_ERROR           : No errors occurred in function
 *          TCEA_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          TCEA_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          TCEA_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~6,500,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          TCEA_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~20,000,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          TCEA_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          TCEA_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          TCEA_A_ERROR            : Semi-major axis less than or equal to zero
 *          TCEA_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                      (250 to 350)
 *          TCEA_SCALE_FACTOR_ERROR : Scale factor outside of valid
 *                                      range (0.3 to 3.0)
 *          TCEA_LON_WARNING        : Distortion will result if longitude is more
 *                                     than 90 degrees from the Central Meridian
 *
 * REUSE NOTES
 *
 *    TRANSVERSE CYLINDRICAL EQUAL AREA is intended for reuse by any application that
 *    performs a Transverse Cylindrical Equal Area projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on TRANSVERSE CYLINDRICAL EQUAL AREA can be found in the Reuse Manual.
 *
 *    TRANSVERSE CYLINDRICAL EQUAL AREA originated from :
 *                                U.S. Army Topographic Engineering Center
 *                                Geospatial Information Division
 *                                7701 Telegraph Road
 *                                Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    TRANSVERSE CYLINDRICAL EQUAL AREA has no restrictions.
 *
 * ENVIRONMENT
 *
 *    TRANSVERSE CYLINDRICAL EQUAL AREA was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    3-1-07            Original C++ Code
 *
 */


#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class MapProjection5Parameters;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class TransverseCylindricalEqualArea : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * Transverse Cylindrical Equal Area projection parameters as inputs, and sets the
       * corresponding state variables.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    ellipsoidSemiMajorAxis     : Semi-major axis of ellipsoid, in meters   (input)
       *    ellipsoidFlattening        : Flattening of ellipsoid                   (input)
       *    centralMeridian            : Longitude in radians at the center of     (input)
       *                                 the projection
       *    latitudeOfTrueScale        : Latitude in radians at which the          (input)
       *                                 point scale factor is 1.0
       *    falseEasting               : A coordinate value in meters assigned to the
       *                                 central meridian of the projection.       (input)
       *    falseNorthing              : A coordinate value in meters assigned to the
       *                                 origin latitude of the projection         (input)
       *    scaleFactor                : Multiplier which reduces distances in the
       *                                 projection to the actual distance on the
       *                                 ellipsoid                                 (input)
       *    errorStatus                : Error status                              (output) 
       */

	    TransverseCylindricalEqualArea( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double latitudeOfTrueScale, double falseEasting, double falseNorthing, double scaleFactor );


      TransverseCylindricalEqualArea( const TransverseCylindricalEqualArea &tcea );


	    ~TransverseCylindricalEqualArea( void );


      TransverseCylindricalEqualArea& operator=( const TransverseCylindricalEqualArea &tcea );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters, Transverse Cylindrical Equal Area projection parameters, and scale factor.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
       *    ellipsoidFlattening     : Flattening of ellipsoid                   (output)
       *    centralMeridian         : Longitude in radians at the center of     (output)
       *                            the projection
       *    latitudeOfTrueScale     : Latitude in radians at which the          (output)
       *                            point scale factor is 1.0
       *    falseEasting            : A coordinate value in meters assigned to the
       *                            central meridian of the projection.         (output)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                            origin latitude of the projection           (output)
       *    scaleFactor             : Multiplier which reduces distances in the
       *                            projection to the actual distance on the
       *                            ellipsoid                                   (output)
       */

      MapProjection5Parameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts geodetic (latitude and
       * longitude) coordinates to Transverse Cylindrical Equal Area projection (easting and
       * northing) coordinates, according to the current ellipsoid and Transverse Cylindrical
       * Equal Area projection parameters.  If any errors occur, an exception is thrown with a  
       * description of the error.
       *
       *    longitude         : Longitude (lambda) in radians       (input)
       *    latitude          : Latitude (phi) in radians           (input)
       *    easting           : Easting (X) in meters               (output)
       *    northing          : Northing (Y) in meters              (output)
       */

      MSP::CCS::MapProjectionCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates );


      /*
       * The function convertToGeodetic converts Transverse
       * Cylindrical Equal Area projection (easting and northing) coordinates
       * to geodetic (latitude and longitude) coordinates, according to the
       * current ellipsoid and Transverse Cylindrical Equal Area projection
       * coordinates.  If any errors occur, an exception is thrown with a  
       * description of the error.
       *
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
      double es;              /* sqrt(es2) */
      double M0;
      double qp;
      double One_MINUS_es2;   /* 1.0 - es2 */
      double One_OVER_2es;    /* 1.0 / (2.0 * es) */
      double a0;              /* es2 / 3.0 + 31.0 * es4 / 180.0 + 517.0 * es6 / 5040.0 */
      double a1;              /*  23.0 * es4 / 360.0 + 251.0 * es6 / 3780.0 */
      double a2;              /* 761.0 * es6 / 45360.0 */
      double b0;              /* 3.0 * e1 / 2.0 - 27.0 * e3 / 32.0 */
      double b1;              /* 21.0 * e2 / 16.0 - 55.0 * e4 / 32.0 */
      double b2;              /* 151.0 * e3 / 96.0 */
      double b3;              /* 1097.0 * e4 / 512.0 */
      double c0;              /* 1.0 - es2 / 4.0 - 3.0 * es4 / 64.0 - 5.0 * es6 / 256.0 */
      double c1;              /* 3.0 * es2 / 8.0 + 3.0 * es4 / 32.0 + 45.0 * es6 / 1024.0 */
      double c2;              /* 15.0 * es4 / 256.0 + 45.0 * es6 / 1024.0 */
      double c3;              /* 35.0 * es6 / 3072.0 */

      /* Transverse Cylindrical Equal Area projection Parameters */
      double Tcea_Origin_Lat;             /* Latitude of origin in radians     */
      double Tcea_Origin_Long;            /* Longitude of origin in radians    */
      double Tcea_False_Northing;         /* False northing in meters          */
      double Tcea_False_Easting;          /* False easting in meters           */
      double Tcea_Scale_Factor;           /* Scale factor                      */

      /* Maximum variance for easting and northing values for WGS 84.
       */
      double Tcea_Min_Easting;
      double Tcea_Max_Easting;
      double Tcea_Min_Northing;
      double Tcea_Max_Northing;


      double TCEA_Q( double sinlat, double x );
  
      double TCEA_COEFF_TIMES_SIN( double coeff, double x, double latit );  

      double TCEA_M( double c0lat, double c1lat, double c2lat, double c3lat );  

      double TCEA_L( double Beta, double c0lat, double c1lat, double c2lat );             

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
