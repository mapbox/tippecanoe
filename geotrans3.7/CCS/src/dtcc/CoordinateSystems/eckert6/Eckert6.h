// CLASSIFICATION: UNCLASSIFIED

#ifndef Eckert6_H
#define Eckert6_H

/***************************************************************************/
/* RSC IDENTIFIER: ECKERT6
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Eckert VI projection coordinates
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
 *          ECK6_NO_ERROR           : No errors occurred in function
 *          ECK6_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          ECK6_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          ECK6_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~18,000,000 m,
 *                                       depending on ellipsoid parameters)
 *          ECK6_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- 0 to ~8,000,000 m,
 *                                       depending on ellipsoid parameters)
 *          ECK6_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          ECK6_A_ERROR            : Semi-major axis less than or equal to zero
 *          ECK6_INV_F_ERROR        : Inverse flattening outside of valid range
 *									                    (250 to 350)
 *
 * REUSE NOTES
 *
 *    ECKERT6 is intended for reuse by any application that performs a
 *    Eckert VI projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on ECKERT6 can be found in the Reuse Manual.
 *
 *    ECKERT6 originated from :  U.S. Army Topographic Engineering Center
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
 *    ECKERT6 has no restrictions.
 *
 * ENVIRONMENT
 *
 *    ECKERT6 was tested and certified in the following environments:
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

    class Eckert6 : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * Eckert VI projection parameters as inputs, and sets the corresponding state
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

	    Eckert6( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double falseEasting, double falseNorthing );


      Eckert6( const Eckert6 &e );


	    ~Eckert6( void );


      Eckert6& operator=( const Eckert6 &e );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters and Eckert VI projection parameters.
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
       * longitude) coordinates to Eckert VI projection (easting and northing)
       * coordinates, according to the current ellipsoid and Eckert VI projection
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
       * The function convertToGeodetic converts Eckert VI projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid and Eckert VI projection
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
      double es2;                             /* Eccentricity (0.08181919084262188000) squared */
      double es4;                             /* es2 * es2	*/
      double es6;                             /* es4 * es2  */
      double Ra_Over_Sqrt_Two_Plus_PI;        /* Ra(6371007.1810824)/Sqrt(2.0 + PI) */
      double Inv_Ra_Over_Sqrt_Two_Plus_PI;    /* Sqrt(2.0 + PI)/Ra(6371007.1810824) */

      /* Eckert6 projection Parameters */
      double Eck6_Origin_Long;                /* Longitude of origin in radians    */
      double Eck6_False_Easting;
      double Eck6_False_Northing;
      double Eck6_Delta_Northing;
      double Eck6_Max_Easting;
      double Eck6_Min_Easting;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
