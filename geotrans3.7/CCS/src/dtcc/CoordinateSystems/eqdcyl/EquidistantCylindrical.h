// CLASSIFICATION: UNCLASSIFIED

#ifndef EquidistantCylindrical_H
#define EquidistantCylindrical_H

/***************************************************************************/
/* RSC IDENTIFIER: EQUIDISTANT CYLINDRICAL
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Equidistant Cylindrical projection coordinates
 *    (easting and northing in meters).  The Equidistant Cylindrical
 *    projection employs a spherical Earth model.  The Spherical Radius
 *    used is the the radius of the sphere having the same area as the
 *    ellipsoid.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          EQCY_NO_ERROR           : No errors occurred in function
 *          EQCY_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          EQCY_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          EQCY_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~20,000,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Standard Parallel)
 *          EQCY_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- 0 to ~10,000,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Standard Parallel)
 *          EQCY_STDP_ERROR         : Standard parallel outside of valid range
 *                                      (-90 to 90 degrees)
 *          EQCY_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          EQCY_A_ERROR            : Semi-major axis less than or equal to zero
 *          EQCY_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                      (250 to 350)
 *
 * REUSE NOTES
 *
 *    EQUIDISTANT CYLINDRICAL is intended for reuse by any application that performs a
 *    Equidistant Cylindrical projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on EQUIDISTANT CYLINDRICAL can be found in the Reuse Manual.
 *
 *    EQUIDISTANT CYLINDRICAL originated from :  U.S. Army Topographic Engineering Center
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
 *    EQUIDISTANT CYLINDRICAL has no restrictions.
 *
 * ENVIRONMENT
 *
 *    EQUIDISTANT CYLINDRICAL was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC 2.8.1
 *    2. MS Windows with MS Visual C++ 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    04-16-99          Original Code
 *    03-06-07          Original C++ Code
 *
 *
 */


#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class EquidistantCylindricalParameters;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class EquidistantCylindrical : public CoordinateSystem
    {
    public:

      /*
       * The function setParameters receives the ellipsoid parameters and
       * projection parameters as inputs, and sets the corresponding state
       * variables.  It also calculates the spherical radius of the sphere having
       * the same area as the ellipsoid.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
       *    ellipsoidFlattening     : Flattening of ellipsoid                   (input)
       *    centralMeridian         : Longitude in radians at the center of     (input)
       *                              the projection
       *    standardParallel        : Latitude in radians at which the          (input)
       *                              point scale factor is 1.0
       *    falseEasting            : A coordinate value in meters assigned to the
       *                              central meridian of the projection.       (input)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                              standard parallel of the projection       (input)
       */

	    EquidistantCylindrical( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double standardParallel, double falseEasting, double falseNorthing );


      EquidistantCylindrical( const EquidistantCylindrical &ec );


	    ~EquidistantCylindrical( void );


      EquidistantCylindrical& operator=( const EquidistantCylindrical &ec );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters and Equidistant Cylindrical projection parameters.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
       *    ellipsoidFlattening     : Flattening of ellipsoid                   (output)
       *    centralMeridian         : Longitude in radians at the center of     (output)
       *                              the projection
       *    standardParallel        : Latitude in radians at which the          (output)
       *                              point scale factor is 1.0
       *    falseEasting            : A coordinate value in meters assigned to the
       *                              central meridian of the projection.       (output)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                              standard parallel of the projection       (output)
       */

      EquidistantCylindricalParameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts geodetic (latitude and
       * longitude) coordinates to Equidistant Cylindrical projection (easting and northing)
       * coordinates, according to the current ellipsoid, spherical radiius
       * and Equidistant Cylindrical projection parameters.
       * If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    longitude         : Longitude (lambda) in radians       (input)
       *    latitude          : Latitude (phi) in radians           (input)
       *    easting           : Easting (X) in meters               (output)
       *    northing          : Northing (Y) in meters              (output)
       */

      MSP::CCS::MapProjectionCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates );


      /*
       * The function convertToGeodetic converts Equidistant Cylindrical projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid, spherical radius
       * and Equidistant Cylindrical projection coordinates.
       * If any errors occur, an exception is thrown with a description 
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
      double es2;               /* Eccentricity (0.08181919084262188000) squared         */
      double es4;               /* es2 * es2  */
      double es6;               /* es4 * es2  */
      double Ra;                /* Spherical Radius  */

      /* Equidistant Cylindrical projection Parameters */
      double Eqcy_Std_Parallel;             /* Latitude of standard parallel in radians     */
      double Cos_Eqcy_Std_Parallel;         /* cos(Eqcy_Std_Parallel)  */
      double Eqcy_Origin_Long;              /* Longitude of origin in radians    */
      double Eqcy_False_Easting;
      double Eqcy_False_Northing;
      double Eqcy_Delta_Northing;
      double Eqcy_Max_Easting;
      double Eqcy_Min_Easting;
      double Ra_Cos_Eqcy_Std_Parallel;      /* Ra * Cos_Eqcy_Std_Parallel */

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
