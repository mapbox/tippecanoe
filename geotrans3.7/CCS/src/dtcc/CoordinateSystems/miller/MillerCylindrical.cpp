// CLASSIFICATION: UNCLASSIFIED

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


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "MillerCylindrical.h"
#include "MapProjection3Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h   - Standard C math library
 *    MillerCylindrical.h - Is for prototype error checking
 *    MapProjectionCoordinates.h   - defines map projection coordinates
 *    GeodeticCoordinates.h   - defines geodetic coordinates
 *    CoordinateConversionException.h - Exception handler
 *    ErrorMessages.h  - Contains exception messages
 */


using namespace MSP::CCS;


/***************************************************************************/
/*                               DEFINES 
 *
 */

const double PI = 3.14159265358979323e0;     /* PI     */
const double PI_OVER_2 = ( PI / 2.0);                
const double TWO_PI = (2.0 * PI);                  


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

MillerCylindrical::MillerCylindrical( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
   es2( 0.0066943799901413800 ),
   es4( 4.4814723452405e-005 ),
   es6( 3.0000678794350e-007 ),
   Ra( 6371007.1810824 ),
   Mill_Origin_Long( 0.0 ),
   Mill_False_Easting( 0.0 ),
   Mill_False_Northing( 0.0 ),
   Mill_Delta_Northing( 14675058.0 ),
   Mill_Max_Easting( 20015110.0 ),
   Mill_Min_Easting( -20015110.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * Miller Cylindrical projcetion parameters as inputs, and sets the corresponding
 * state variables.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *  ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
 *  ellipsoidFlattening     : Flattening of ellipsoid                   (input)
 *  centralMeridian         : Longitude in radians at the center of     (input)
 *                            the projection
 *  falseEasting            : A coordinate value in meters assigned to the
 *                            central meridian of the projection.       (input)
 *  falseNorthing           : A coordinate value in meters assigned to the
 *                            origin latitude of the projection         (input)
 */

  double inv_f = 1 / ellipsoidFlattening;

  if (ellipsoidSemiMajorAxis <= 0.0)
  { /* Semi-major axis must be greater than zero */
    throw CoordinateConversionException( ErrorMessages::semiMajorAxis  );
  }
  if ((inv_f < 250) || (inv_f > 350))
  { /* Inverse flattening must be between 250 and 350 */
    throw CoordinateConversionException( ErrorMessages::ellipsoidFlattening  );
  }
  if ((centralMeridian < -PI) || (centralMeridian > TWO_PI))
  { /* origin longitude out of range */
    throw CoordinateConversionException( ErrorMessages::centralMeridian  );
  }

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening = ellipsoidFlattening;

  es2 = 2 * flattening - flattening * flattening;
  es4 = es2 * es2;
  es6 = es4 * es2;
  /* spherical radius */
  Ra = semiMajorAxis * (1.0 - es2 / 6.0 - 17.0 * es4 / 360.0 - 67.0 * es6 /3024.0);
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Mill_Origin_Long = centralMeridian;
  Mill_False_Easting = falseEasting;
  Mill_False_Northing = falseNorthing;
  if (Mill_Origin_Long > 0)
  {
    Mill_Max_Easting = 19903915.0;
    Mill_Min_Easting = -20015110.0;
  }
  else if (Mill_Origin_Long < 0)
  {
    Mill_Max_Easting = 20015110.0;
    Mill_Min_Easting = -19903915.0;
  }
  else
  {
    Mill_Max_Easting = 20015110.0;
    Mill_Min_Easting = -20015110.0;
  }
}


MillerCylindrical::MillerCylindrical( const MillerCylindrical &mc )
{
  semiMajorAxis = mc.semiMajorAxis;
  flattening = mc.flattening;
  es2 = mc.es2;
  es4 = mc.es4;
  es6 = mc.es6;
  Ra = mc.Ra;
  Mill_Origin_Long = mc.Mill_Origin_Long;
  Mill_False_Easting = mc.Mill_False_Easting;
  Mill_False_Northing = mc.Mill_False_Northing;
  Mill_Delta_Northing = mc.Mill_Delta_Northing;
  Mill_Max_Easting = mc.Mill_Max_Easting;
  Mill_Min_Easting = mc.Mill_Min_Easting;
}


MillerCylindrical::~MillerCylindrical()
{
}


MillerCylindrical& MillerCylindrical::operator=( const MillerCylindrical &mc )
{
  if( this != &mc )
  {
    semiMajorAxis = mc.semiMajorAxis;
    flattening = mc.flattening;
    es2 = mc.es2;
    es4 = mc.es4;
    es6 = mc.es6;
    Ra = mc.Ra;
    Mill_Origin_Long = mc.Mill_Origin_Long;
    Mill_False_Easting = mc.Mill_False_Easting;
    Mill_False_Northing = mc.Mill_False_Northing;
    Mill_Delta_Northing = mc.Mill_Delta_Northing;
    Mill_Max_Easting = mc.Mill_Max_Easting;
    Mill_Min_Easting = mc.Mill_Min_Easting;
  }

  return *this;
}


MapProjection3Parameters* MillerCylindrical::getParameters() const
{
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

  return new MapProjection3Parameters( CoordinateType::millerCylindrical, Mill_Origin_Long, Mill_False_Easting, Mill_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* MillerCylindrical::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
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

  double dlam;     /* Longitude - Central Meridan */
  double longitude = geodeticCoordinates->longitude();
  double latitude = geodeticCoordinates->latitude();
  double slat = sin(0.8 * latitude);

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  {  /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  {  /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  dlam = longitude - Mill_Origin_Long;
  if (dlam > PI)
  {
    dlam -= TWO_PI;
  }
  if (dlam < -PI)
  {
    dlam += TWO_PI;
  }
  double easting = Ra * dlam + Mill_False_Easting;
  double northing = (Ra / 1.6) * log((1.0 + slat) /
                               (1.0 - slat)) + Mill_False_Northing;

  return new MapProjectionCoordinates( CoordinateType::millerCylindrical, easting, northing );
}


MSP::CCS::GeodeticCoordinates* MillerCylindrical::convertToGeodetic(
   MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
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

  double dx, dy;

  double easting = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Mill_False_Easting + Mill_Min_Easting))
      || (easting > (Mill_False_Easting + Mill_Max_Easting)))
  { /* Easting out of range  */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < (Mill_False_Northing - Mill_Delta_Northing)) || 
      (northing > (Mill_False_Northing + Mill_Delta_Northing) ))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy = northing - Mill_False_Northing;
  dx = easting  - Mill_False_Easting;
  double latitude = atan(sinh(0.8 * dy / Ra)) / 0.8;
  double longitude = Mill_Origin_Long + dx / Ra;

  if (latitude > PI_OVER_2)  /* force distorted values to 90, -90 degrees */
    latitude = PI_OVER_2;
  else if (latitude < -PI_OVER_2)
    latitude = -PI_OVER_2;

  if (longitude > PI)
    longitude -= TWO_PI;
  if (longitude < -PI)
    longitude += TWO_PI;

  if (longitude > PI)  /* force distorted values to 180, -180 degrees */
    longitude = PI;
  else if (longitude < -PI)
    longitude = -PI;

  return new GeodeticCoordinates( CoordinateType::geodetic, longitude, latitude );
}



// CLASSIFICATION: UNCLASSIFIED
