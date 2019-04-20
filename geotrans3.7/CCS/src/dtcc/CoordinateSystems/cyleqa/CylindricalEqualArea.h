// CLASSIFICATION: UNCLASSIFIED

#ifndef CylindricalEqualArea_H
#define CylindricalEqualArea_H

/***************************************************************************/
/* RSC IDENTIFIER: CYLINDRICAL EQUAL AREA
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Cylindrical Equal Area projection
 *    coordinates (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          CYEQ_NO_ERROR           : No errors occurred in function
 *          CYEQ_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          CYEQ_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          CYEQ_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~20,000,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          CYEQ_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~6,000,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          CYEQ_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          CYEQ_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          CYEQ_A_ERROR            : Semi-major axis less than or equal to zero
 *          CYEQ_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                      (250 to 350)
 *
 * REUSE NOTES
 *
 *    CYLINDRICAL EQUAL AREA is intended for reuse by any application that performs a
 *    Cylindrical Equal Area projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on CYLINDRICAL EQUAL AREA can be found in the Reuse Manual.
 *
 *    CYLINDRICAL EQUAL AREA originated from :
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
 *    CYLINDRICAL EQUAL AREA has no restrictions.
 *
 * ENVIRONMENT
 *
 *    CYLINDRICAL EQUAL AREA was tested and certified in the following environments:
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

    class CylindricalEqualArea : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * Cylindrical Equal Area projcetion parameters as inputs, and sets the corresponding
       * state variables.  If any errors occur, an exception is thrown with a description 
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

	    CylindricalEqualArea( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing );


      CylindricalEqualArea( const CylindricalEqualArea &cea );


	    ~CylindricalEqualArea( void );


      CylindricalEqualArea& operator=( const CylindricalEqualArea &cea );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters, and Cylindrical Equal Area projection parameters.
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
       * longitude) coordinates to Cylindrical Equal Area projection (easting and northing)
       * coordinates, according to the current ellipsoid and Cylindrical Equal Area projection
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
       * The function convertToGeodetic converts
       * Cylindrical Equal Area projection (easting and northing) coordinates
       * to geodetic (latitude and longitude) coordinates, according to the
       * current ellipsoid and Cylindrical Equal Area projection
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
      double es2;             /* Eccentricity (0.08181919084262188000) squared  */
      double es;              /* Sqrt(es2) */
      double es4;             /* es2 * es2 */
      double es6;             /* es4 * es2 */
      double k0;
      double Cyeq_a_k0;       /* Cyeq_a * k0 */
      double two_k0;          /* 2.0 * k0 */
      double c0;              /* es2 / 3.0 + 31.0 * es4 / 180.0 + 517.0 * es6 / 5040.0 */
      double c1;              /* 23.0 es4 / 360.0 + 251.0 * es6 / 3780.0 */
      double c2;              /* 761.0 * es6 / 45360.0 */

      /* Cylindrical Equal Area projection Parameters */
      double Cyeq_Origin_Lat;                  /* Latitude of origin in radians     */
      double Cyeq_Origin_Long;                 /* Longitude of origin in radians    */
      double Cyeq_False_Northing;              /* False northing in meters          */
      double Cyeq_False_Easting;               /* False easting in meters           */

      /* Maximum variance for easting and northing values for WGS 84.*/
      double Cyeq_Max_Easting;
      double Cyeq_Min_Easting;
      double Cyeq_Delta_Northing;


      double cyleqarQ( double slat, double x );
    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
