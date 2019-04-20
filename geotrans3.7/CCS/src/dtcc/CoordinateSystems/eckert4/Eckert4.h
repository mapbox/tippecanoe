// CLASSIFICATION: UNCLASSIFIED

#ifndef Eckert4_H
#define Eckert4_H

/***************************************************************************/
/* RSC IDENTIFIER: ECKERT4
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Eckert IV projection coordinates
 *    (easting and northing in meters).  This projection employs a spherical
 *    Earth model.  The spherical radius used is the radius of the sphere
 *    having the same area as the ellipsoid.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          ECK4_NO_ERROR           : No errors occurred in function
 *          ECK4_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          ECK4_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          ECK4_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~17,000,000 m,
 *										 depending on ellipsoid parameters)
 *          ECK4_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- 0 to 8,000,000 m,
 *										 depending on ellipsoid parameters)
 *          ECK4_CENT_MER_ERROR     : Central_Meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          ECK4_A_ERROR            : Semi-major axis less than or equal to zero
 *          ECK4_INV_F_ERROR        : Inverse flattening outside of valid range
 *									                    (250 to 350)
 *
 * REUSE NOTES
 *
 *    ECKERT4 is intended for reuse by any application that performs a
 *    Eckert IV projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on ECKERT4 can be found in the Reuse Manual.
 *
 *    ECKERT4 originated from :  U.S. Army Topographic Engineering Center
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
 *    ECKERT4 has no restrictions.
 *
 * ENVIRONMENT
 *
 *    ECKERT4 was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC 2.8.1
 *    2. MS Windows 95 with MS Visual C++ 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    04-16-99          Original Code
 *    03-06-07          Original C++ Code
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

    class Eckert4 : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * projection parameters as inputs, and sets the corresponding state
       * variables.  If any errors occur, an exception is thrown with a description 
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

	    Eckert4( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double falseEasting, double falseNorthing );


      Eckert4( const Eckert4 &e );


	    ~Eckert4( void );


      Eckert4& operator=( const Eckert4 &e );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters and Eckert IV projection parameters.
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
       * longitude) coordinates to Eckert IV projection (easting and northing)
       * coordinates, according to the current ellipsoid, spherical radius and
       * Eckert IV projection parameters.
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
       * The function convertToGeodetic converts Eckert IV projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid, spherical radius and
       * Eckert IV projection coordinates.
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
      double es2;              /* Eccentricity (0.08181919084262188000) squared         */
      double es4;              /* es2 * es2  */
      double es6;              /* es4 * es2  */

      double Ra0;              /* 0.4222382 * Sperical Radius (6371007.1810824) */
      double Ra1;              /* 1.3265004 * Sperical Radius (6371007.1810824) */

      /* Eckert4 projection Parameters */
      double Eck4_Origin_Long;                  /* Longitude of origin in radians    */
      double Eck4_False_Easting;
      double Eck4_False_Northing;
      double Eck4_Delta_Northing;
      double Eck4_Max_Easting;
      double Eck4_Min_Easting;

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
