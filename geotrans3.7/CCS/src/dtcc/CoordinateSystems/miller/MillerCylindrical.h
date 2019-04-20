// CLASSIFICATION: UNCLASSIFIED

#ifndef Miller_H
#define Miller_H

/***************************************************************************/
/* RSC IDENTIFIER: MILLER
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Miller Cylindrical projection 
 *    coordinates (easting and northing in meters).  The Miller Cylindrical
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
 *          MILL_NO_ERROR           : No errors occurred in function
 *          MILL_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          MILL_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          MILL_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~20,000,000 m,
 *                                       depending on ellipsoid parameters)
 *          MILL_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~14,000,000 m,
 *                                       depending on ellipsoid parameters)
 *          MILL_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          MILL_A_ERROR            : Semi-major axis less than or equal to zero
 *          MILL_INV_F_ERROR        : Inverse flattening outside of valid range
 *									                    (250 to 350)
 *
 * REUSE NOTES
 *
 *    MILLER is intended for reuse by any application that performs a
 *    Miller Cylindrical projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on MILLER can be found in the Reuse Manual.
 *
 *    MILLER originated from :  U.S. Army Topographic Engineering Center
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
 *    MILLER has no restrictions.
 *
 * ENVIRONMENT
 *
 *    MILLER was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC 2.8.1
 *    2. Windows 95 with MS Visual C++ 6
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
    class MapProjection3Parameters;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class MillerCylindrical : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * Miller Cylindrical projcetion parameters as inputs, and sets the corresponding
       * state variables.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
       *    ellipsoidFlattening     : Flattening of ellipsoid						        (input)
       *    centralMeridian         : Longitude in radians at the center of     (input)
       *                              the projection
       *    falseEasting            : A coordinate value in meters assigned to the
       *                              central meridian of the projection.       (input)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                              origin latitude of the projection         (input)
       */

	    MillerCylindrical( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double falseEasting, double falseNorthing );


      MillerCylindrical( const MillerCylindrical &mc );


	    ~MillerCylindrical( void );


      MillerCylindrical& operator=( const MillerCylindrical &mc );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters and Miller Cylindrical projection parameters.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
       *    ellipsoidFlattening     : Flattening of ellipsoid						        (output)
       *    centralMeridian         : Longitude in radians at the center of     (output)
       *                              the projection
       *    falseEasting            : A coordinate value in meters assigned to the
       *                              central meridian of the projection.       (output)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                              origin latitude of the projection         (output)
       */

      MapProjection3Parameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts geodetic (latitude and
       * longitude) coordinates to Miller Cylindrical projection (easting and northing)
       * coordinates, according to the current ellipsoid and Miller Cylindrical projection
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
       * The function convertToGeodetic converts Miller Cylindrical projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid and Miller Cylindrical projection
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
      double es4;              /* es2 * es2	*/
      double es6;              /* es4 * es2   */
      double Ra;               /* Spherical Radius */

      /* Miller projection Parameters */
      double Mill_Origin_Long;                  /* Longitude of origin in radians    */
      double Mill_False_Easting;
      double Mill_False_Northing;
      double Mill_Delta_Northing;
      double Mill_Max_Easting;
      double Mill_Min_Easting;

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
