// CLASSIFICATION: UNCLASSIFIED

#ifndef LocalCartesian_H
#define LocalCartesian_H

/***************************************************************************/
/* RSC IDENTIFIER:  LOCAL CARTESIAN
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates (latitude,
 *    longitude in radians and height in meters) and Local Cartesian coordinates 
 *    (X, Y, Z).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *      LOCCART_NO_ERROR            : No errors occurred in function
 *      LOCCART_LAT_ERROR           : Latitude out of valid range
 *                                      (-90 to 90 degrees)
 *      LOCCART_LON_ERROR           : Longitude out of valid range
 *                                      (-180 to 360 degrees)
 *      LOCCART_A_ERROR             : Semi-major axis less than or equal to zero
 *      LOCCART_INV_F_ERROR         : Inverse flattening outside of valid range
 *									                    (250 to 350)
 *      LOCCART_ORIGIN_LAT_ERROR    : Origin Latitude out of valid range
 *                                      (-90 to 90 degrees)
 *      LOCCART_ORIGIN_LON_ERROR    : Origin Longitude out of valid range
 *                                      (-180 to 360 degrees)
 *		  LOCCART_ORIENTATION_ERROR   : Orientation angle out of valid range
 *									                    (-360 to 360 degrees)
 *
 *
 * REUSE NOTES
 *
 *    LOCCART is intended for reuse by any application that performs
 *    coordinate conversions between geodetic coordinates or geocentric
 *    coordinates and local cartesian coordinates..
 *    
 *
 * REFERENCES
 *    
 *    Further information on LOCAL CARTESIAN can be found in the Reuse Manual.
 *
 *    LOCCART originated from : U.S. Army Topographic Engineering Center
 *                              Geospatial Inforamtion Division
 *                              7701 Telegraph Road
 *                              Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    LOCCART has no restrictions.
 *
 * ENVIRONMENT
 *
 *    LOCCART was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC version 2.8.1
 *    2. Windows 95 with MS Visual C++ version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *	  07-16-99			    Original Code
 *	  03-2-07			      Original C++ Code
 *
 */


#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class LocalCartesianParameters;
    class Geocentric;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class LocalCartesian : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters
       * and local origin parameters as inputs and sets the corresponding state variables.
       *
       *    ellipsoidSemiMajorAxis   : Semi-major axis of ellipsoid, in meters           (input)
       *    ellipsoidFlattening      : Flattening of ellipsoid					                 (input)
       *    originLongitude          : Longitude of the local origin, in radians         (input)
       *    originLatitude           : Latitude of the local origin, in radians          (input)
       *    originHeight             : Ellipsoid height of the local origin, in meters   (input)
       *    orientation              : Orientation angle of the local cartesian coordinate system,
       *                               in radians                                        (input)
       */

	    LocalCartesian( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double originLongitude, double originLatitude, double originHeight, double orientation );


      LocalCartesian( const LocalCartesian &lc );


	    ~LocalCartesian( void );


      LocalCartesian& operator=( const LocalCartesian &lc );


      /*
       * The function getParameters returns the ellipsoid parameters
       * and local origin parameters.
       *
       *    ellipsoidSemiMajorAxis    : Semi-major axis of ellipsoid, in meters           (output)
       *    ellipsoidFlattening       : Flattening of ellipsoid					                  (output)
       *    originLongitude           : Longitude of the local origin, in radians         (output)
       *    originLatitude            : Latitude of the local origin, in radians          (output)
       *    originHeight              : Ellipsoid height of the local origin, in meters   (output)
       *    orientation               : Orientation angle of the local cartesian coordinate system,
       *                                in radians                                        (output)
       */

      LocalCartesianParameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts geodetic coordinates
       * (latitude, longitude, and height) to local cartesian coordinates (X, Y, Z),
       * according to the current ellipsoid and local origin parameters.
       *
       *    longitude : Geodetic longitude, in radians                       (input)
       *    latitude  : Geodetic latitude, in radians                        (input)
       *    Height    : Geodetic height, in meters                           (input)
       *    X         : Calculated local cartesian X coordinate, in meters   (output)
       *    Y         : Calculated local cartesian Y coordinate, in meters   (output)
       *    Z         : Calculated local cartesian Z coordinate, in meters   (output)
       *
       */

      MSP::CCS::CartesianCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates );


      /*
       * The function convertToGeodetic converts local cartesian
       * coordinates (X, Y, Z) to geodetic coordinates (latitude, longitude, 
       * and height), according to the current ellipsoid and local origin parameters.
       *
       *    X         : Local cartesian X coordinate, in meters    (input)
       *    Y         : Local cartesian Y coordinate, in meters    (input)
       *    Z         : Local cartesian Z coordinate, in meters    (input)
       *    longitude : Calculated longitude value, in radians     (output)
       *    latitude  : Calculated latitude value, in radians      (output)
       *    Height    : Calculated height value, in meters         (output)
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::CartesianCoordinates* cartesianCoordinates );

      /*
       * The function convertFromGeocentric converts geocentric
       * coordinates according to the current ellipsoid and local origin parameters.
       *
       *    U         : Geocentric latitude, in meters                       (input)
       *    V         : Geocentric longitude, in meters                      (input)
       *    W         : Geocentric height, in meters                         (input)
       *    X         : Calculated local cartesian X coordinate, in meters   (output)
       *    Y         : Calculated local cartesian Y coordinate, in meters   (output)
       *    Z         : Calculated local cartesian Z coordinate, in meters   (output)
       *
       */

      MSP::CCS::CartesianCoordinates* convertFromGeocentric( const MSP::CCS::CartesianCoordinates* cartesianCoordinates );

      /*
       * The function Convert_Local_Cartesian_To_Geocentric converts local cartesian
       * coordinates (x, y, z) to geocentric coordinates (X, Y, Z) according to the 
       * current ellipsoid and local origin parameters.
       *
       *    X         : Local cartesian X coordinate, in meters    (input)
       *    Y         : Local cartesian Y coordinate, in meters    (input)
       *    Z         : Local cartesian Z coordinate, in meters    (input)
       *    U         : Calculated U value, in meters              (output)
       *    V         : Calculated v value, in meters              (output)
       *    W         : Calculated w value, in meters              (output)
       */

      MSP::CCS::CartesianCoordinates* convertToGeocentric( const MSP::CCS::CartesianCoordinates* cartesianCoordinates );

    private:
    
      Geocentric* geocentric;

      /* Ellipsoid Parameters, default to WGS 84 */
      double es2;                       /* Eccentricity (0.08181919084262188000) squared         */
      double u0;                        /* Geocentric origin coordinates in */
      double v0;                        /* terms of Local Cartesian origin  */
      double w0;                        /* parameters                       */

      /* Local Cartesian Projection Parameters */
      double LocalCart_Origin_Lat;      /* Latitude of origin in radians     */
      double LocalCart_Origin_Long;     /* Longitude of origin in radians    */
      double LocalCart_Origin_Height;   /* Height of origin in meters        */
      double LocalCart_Orientation;     /* Orientation of Y axis in radians  */

      double Sin_LocalCart_Origin_Lat;  /* sin(LocalCart_Origin_Lat)         */
      double Cos_LocalCart_Origin_Lat;  /* cos(LocalCart_Origin_Lat)         */
      double Sin_LocalCart_Origin_Lon;  /* sin(LocalCart_Origin_Lon)         */
      double Cos_LocalCart_Origin_Lon;  /* cos(LocalCart_Origin_Lon)         */
      double Sin_LocalCart_Orientation; /* sin(LocalCart_Orientation)        */
      double Cos_LocalCart_Orientation; /* cos(LocalCart_Orientation)        */

      double Sin_Lat_Sin_Orient; /* sin(LocalCart_Origin_Lat) * sin(LocalCart_Orientation) */
      double Sin_Lat_Cos_Orient; /* sin(LocalCart_Origin_Lat) * cos(LocalCart_Orientation) */
      double Cos_Lat_Cos_Orient; /* cos(LocalCart_Origin_Lat) * cos(LocalCart_Orientation) */
      double Cos_Lat_Sin_Orient; /* cos(LocalCart_Origin_Lat) * sin(LocalCart_Orientation) */

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
