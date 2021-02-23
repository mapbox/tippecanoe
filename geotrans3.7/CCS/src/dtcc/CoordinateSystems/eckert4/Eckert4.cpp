// CLASSIFICATION: UNCLASSIFIED

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


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "Eckert4.h"
#include "MapProjection3Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h    - Standard C math library
 *    Eckert4.h - Is for prototype error checking.
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

const double PI = 3.14159265358979323e0;      /* PI  */
const double PI_OVER_2 = ( PI / 2.0);                 
const double TWO_PI = (2.0 * PI) ;      
const double two_PLUS_PI_OVER_2 = (2.0 + PI / 2.0);

             
double calculateNum( double theta, double sinTheta, double cosTheta )
{
  return theta + sinTheta * cosTheta + 2.0 * sinTheta;
}


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

Eckert4::Eckert4( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
  es2( 0.0066943799901413800 ),
  es4( 4.4814723452405e-005 ),
  es6( 3.0000678794350e-007 ),
  Ra0( 2690082.6043273 ),
  Ra1( 8451143.5741087 ),
  Eck4_Origin_Long( 0.0 ),
  Eck4_False_Easting( 0.0 ),
  Eck4_False_Northing( 0.0 ),
  Eck4_Delta_Northing( 8451144.0 ),
  Eck4_Max_Easting( 16902288.0 ),
  Eck4_Min_Easting( -16902288.0 )
{
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

  double Ra;              /* Spherical radius */
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
  Ra = semiMajorAxis *
     (1.0 - es2 / 6.0 - 17.0 * es4 / 360.0 - 67.0 * es6 / 3024.0);
  Ra0 = 0.4222382 * Ra;
  Ra1 = 1.3265004 * Ra;
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Eck4_Origin_Long = centralMeridian;
  Eck4_False_Easting = falseEasting;
  Eck4_False_Northing = falseNorthing;
  if (Eck4_Origin_Long > 0)
  {
    Eck4_Max_Easting = 16808386.0;
    Eck4_Min_Easting = -16902288.0;
  }
  else if (Eck4_Origin_Long < 0)
  {
    Eck4_Max_Easting = 16902288.0;
    Eck4_Min_Easting = -16808386.0;
  }
  else
  {
    Eck4_Max_Easting = 16902288.0;
    Eck4_Min_Easting = -16902288.0;
  }
}


Eckert4::Eckert4( const Eckert4 &e )
{
  semiMajorAxis = e.semiMajorAxis;
  flattening = e.flattening;
  es2 = e.es2;
  es4 = e.es4;
  es6 = e.es6;
  Ra0 = e.Ra0;
  Ra1 = e.Ra1;
  Eck4_Origin_Long = e.Eck4_Origin_Long;
  Eck4_False_Easting = e.Eck4_False_Easting;
  Eck4_False_Northing = e.Eck4_False_Northing;
  Eck4_Delta_Northing = e.Eck4_Delta_Northing;
  Eck4_Max_Easting = e.Eck4_Max_Easting;
  Eck4_Min_Easting = e.Eck4_Min_Easting;
}


Eckert4::~Eckert4()
{
}


Eckert4& Eckert4::operator=( const Eckert4 &e )
{
  if( this != &e )
  {
    semiMajorAxis = e.semiMajorAxis;
    flattening = e.flattening;
    es2 = e.es2;
    es4 = e.es4;
    es6 = e.es6;
    Ra0 = e.Ra0;
    Ra1 = e.Ra1;
    Eck4_Origin_Long = e.Eck4_Origin_Long;
    Eck4_False_Easting = e.Eck4_False_Easting;
    Eck4_False_Northing = e.Eck4_False_Northing;
    Eck4_Delta_Northing = e.Eck4_Delta_Northing;
    Eck4_Max_Easting = e.Eck4_Max_Easting;
    Eck4_Min_Easting = e.Eck4_Min_Easting;
  }

  return *this;
}


MapProjection3Parameters* Eckert4::getParameters() const
{
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

  return new MapProjection3Parameters( CoordinateType::eckert4, Eck4_Origin_Long, Eck4_False_Easting, Eck4_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* Eckert4::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
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

  double sin_theta, cos_theta;
  double num;
  double dlam;     /* Longitude - Central Meridan */
  double delta_theta = 1.0;
  double dt_tolerance = 4.85e-10;        /* approximately 1/1000th of
                                            an arc second or 1/10th meter */
  int count = 200;

  double longitude = geodeticCoordinates->longitude();
  double latitude  = geodeticCoordinates->latitude();
  double slat  = sin(latitude);
  double theta = latitude / 2.0;

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  {  /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  {  /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  dlam = longitude - Eck4_Origin_Long;
  if (dlam > PI)
  {
    dlam -= TWO_PI;
  }
  if (dlam < -PI)
  {
    dlam += TWO_PI;
  }
  while (fabs(delta_theta) > dt_tolerance && count)
  {
    sin_theta = sin(theta);
    cos_theta = cos(theta);
    num = calculateNum(theta, sin_theta, cos_theta);
    delta_theta = -(num - two_PLUS_PI_OVER_2 * slat) /
                  (2.0 * cos_theta * (1.0 + cos_theta));
    theta += delta_theta;
    count --;
  }

  if(!count)
    throw CoordinateConversionException( ErrorMessages::northing );

  double easting = Ra0 * dlam * (1.0 + cos(theta)) + Eck4_False_Easting;
  double northing = Ra1 * sin(theta) + Eck4_False_Northing;

  return new MapProjectionCoordinates( CoordinateType::eckert4, easting, northing );
}


MSP::CCS::GeodeticCoordinates* Eckert4::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
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

  double theta;
  double sin_theta, cos_theta;
  double num;
  double dx, dy;
  double i;

  double easting = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Eck4_False_Easting + Eck4_Min_Easting))
      || (easting > (Eck4_False_Easting + Eck4_Max_Easting)))
  { /* Easting out of range  */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < (Eck4_False_Northing - Eck4_Delta_Northing)) 
      || (northing > (Eck4_False_Northing + Eck4_Delta_Northing)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy = northing - Eck4_False_Northing;
  dx = easting - Eck4_False_Easting;
  i = dy / Ra1;
  if (i > 1.0)
    i = 1.0;
  else if (i < -1.0)
    i = -1.0;

  theta = asin(i);
  sin_theta = sin(theta);
  cos_theta = cos(theta);
  num = calculateNum(theta, sin_theta, cos_theta);

  double latitude = asin(num / two_PLUS_PI_OVER_2);
  double longitude = Eck4_Origin_Long + dx / (Ra0 * (1 + cos_theta));

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
