// CLASSIFICATION: UNCLASSIFIED

#ifndef Mollweide_H
#define Mollweide_H

/***************************************************************************/
/* RSC IDENTIFIER: MOLLWEIDE
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Mollweide projection coordinates
 *    (easting and northing in meters).  The Mollweide Pseudocylindrical
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
 *          MOLL_NO_ERROR           : No errors occurred in function
 *          MOLL_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          MOLL_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          MOLL_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~18,000,000 m,
 *                                       depending on ellipsoid parameters)
 *          MOLL_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~9,000,000 m,
 *                                       depending on ellipsoid parameters)
 *          MOLL_ORIGIN_LON_ERROR   : Origin longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          MOLL_A_ERROR            : Semi-major axis less than or equal to zero
 *          MOLL_INV_F_ERROR        : Inverse flattening outside of valid range
 *								  	                  (250 to 350)
 *
 * REUSE NOTES
 *
 *    MOLLWEID is intended for reuse by any application that performs a
 *    Mollweide projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on MOLLWEID can be found in the Reuse Manual.
 *
 *    MOLLWEID originated from :  U.S. Army Topographic Engineering Center
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
 *    MOLLWEID has no restrictions.
 *
 * ENVIRONMENT
 *
 *    MOLLWEID was tested and certified in the following environments:
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

    class Mollweide : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * Mollweide projcetion parameters as inputs, and sets the corresponding state
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

	    Mollweide( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double falseEasting, double falseNorthing );


      Mollweide( const Mollweide &m );


	    ~Mollweide( void );


      Mollweide& operator=( const Mollweide &m );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters and Mollweide projection parameters.
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
       * longitude) coordinates to Mollweide projection (easting and northing)
       * coordinates, according to the current ellipsoid and Mollweide projection
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
       * The function convertToGeodetic converts Mollweide projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid and Mollweide projection
       * coordinates. If any errors occur, an exception is thrown with a description 
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
      double es2;             /* Eccentricity (0.08181919084262188000) squared */
      double es4;             /* es2 * es2	*/
      double es6;             /* es4 * es2  */
      double Sqrt2_Ra;        /* sqrt(2.0) * Spherical Radius(6371007.1810824) */
      double Sqrt8_Ra;        /* sqrt(8.0) * Spherical Radius(6371007.1810824) */

      /* Mollweide projection Parameters */
      double Moll_Origin_Long;            /* Longitude of origin in radians    */
      double Moll_False_Easting;
      double Moll_False_Northing;
      double Moll_Delta_Northing;
      double Moll_Max_Easting;
      double Moll_Min_Easting;

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
