// CLASSIFICATION: UNCLASSIFIED

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


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "Eckert6.h"
#include "MapProjection3Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h     - Is needed to call the math functions (sqrt, pow, exp, log,
 *                   sin, cos, tan, and atan).
 *    Eckert6.h - Is for prototype error checking.
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

const double PI = 3.14159265358979323e0;       /* PI                            */
const double PI_OVER_2 = ( PI / 2.0);                
const double MAX_LAT = ( (PI * 90) / 180.0 );  /* 90 degrees in radians    */
const double TWO_PI = (2.0 * PI);                  
const double one_PLUS_PI_OVER_2 = (1.0 + PI / 2.0);


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

Eckert6::Eckert6( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
  es2( 0.0066943799901413800 ),
  es4( 4.4814723452405e-005 ),
  es6( 3.0000678794350e-007 ),
  Ra_Over_Sqrt_Two_Plus_PI( 2809695.5356062 ),
  Inv_Ra_Over_Sqrt_Two_Plus_PI( 3.5591044913137e-007 ),
  Eck6_Origin_Long( 0.0 ),
  Eck6_False_Easting( 0.0 ),
  Eck6_False_Northing( 0.0 ),
  Eck6_Delta_Northing( 8826919.0 ),
  Eck6_Max_Easting( 17653838.0 )
{
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

  double Ra;                      /* Spherical radius */
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
     (1.0 - es2 / 6.0 - 17.0 * es4 / 360.0 - 67.0 * es6 /3024.0);
  Ra_Over_Sqrt_Two_Plus_PI = Ra / (sqrt(2.0 + PI));
  Inv_Ra_Over_Sqrt_Two_Plus_PI = 1 / Ra_Over_Sqrt_Two_Plus_PI;
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Eck6_Origin_Long = centralMeridian;
  Eck6_False_Easting = falseEasting;
  Eck6_False_Northing = falseNorthing;
  if (Eck6_Origin_Long > 0)
  {
    Eck6_Max_Easting = 17555761.0;
    Eck6_Min_Easting = -17653839.0;
  }
  else if (Eck6_Origin_Long < 0)
  {
    Eck6_Max_Easting = 17653838.0;
    Eck6_Min_Easting = -17555761.0;
  }
  else
  {
    Eck6_Max_Easting = 17653838.0;
    Eck6_Min_Easting = -17653838.0;
  }
}


Eckert6::Eckert6( const Eckert6 &e )
{
  semiMajorAxis = e.semiMajorAxis;
  flattening = e.flattening;
  es2 = e.es2;
  es4 = e.es4;
  es6 = e.es6;
  Ra_Over_Sqrt_Two_Plus_PI = e.Ra_Over_Sqrt_Two_Plus_PI;
  Inv_Ra_Over_Sqrt_Two_Plus_PI = e.Inv_Ra_Over_Sqrt_Two_Plus_PI;
  Eck6_Origin_Long = e.Eck6_Origin_Long;
  Eck6_False_Easting = e.Eck6_False_Easting;
  Eck6_False_Northing = e.Eck6_False_Northing;
  Eck6_Delta_Northing = e.Eck6_Delta_Northing;
  Eck6_Max_Easting = e.Eck6_Max_Easting;
}


Eckert6::~Eckert6()
{
}


Eckert6& Eckert6::operator=( const Eckert6 &e )
{
  if( this != &e )
  {
    semiMajorAxis = e.semiMajorAxis;
    flattening = e.flattening;
    es2 = e.es2;
    es4 = e.es4;
    es6 = e.es6;
    Ra_Over_Sqrt_Two_Plus_PI = e.Ra_Over_Sqrt_Two_Plus_PI;
    Inv_Ra_Over_Sqrt_Two_Plus_PI = e.Inv_Ra_Over_Sqrt_Two_Plus_PI;
    Eck6_Origin_Long = e.Eck6_Origin_Long;
    Eck6_False_Easting = e.Eck6_False_Easting;
    Eck6_False_Northing = e.Eck6_False_Northing;
    Eck6_Delta_Northing = e.Eck6_Delta_Northing;
    Eck6_Max_Easting = e.Eck6_Max_Easting;
  }

  return *this;
}


MapProjection3Parameters* Eckert6::getParameters() const
{
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

  return new MapProjection3Parameters(
     CoordinateType::eckert6,
     Eck6_Origin_Long, Eck6_False_Easting, Eck6_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* Eckert6::convertFromGeodetic(
   MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
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

  double dlam;     /* Longitude - Central Meridan */
  double delta_theta = 1.0;
  double dt_tolerance = 4.85e-10;        /* approximately 1/1000th of
                                             an arc second or 1/10th meter */
  int count = 60;

  double longitude = geodeticCoordinates->longitude();
  double latitude  = geodeticCoordinates->latitude();
  double slat  = sin(latitude);
  double theta = latitude;

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  {  /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  {  /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  dlam = longitude - Eck6_Origin_Long;
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
    delta_theta = -(theta + sin(theta) - one_PLUS_PI_OVER_2 *
                    slat) / (1.0 + cos(theta));
    theta += delta_theta;
    count --;
  }

  if(!count)
  throw CoordinateConversionException( ErrorMessages::northing);

  double easting =
     Ra_Over_Sqrt_Two_Plus_PI * dlam * (1.0 + cos(theta)) + Eck6_False_Easting;
  double northing = 2.0 * Ra_Over_Sqrt_Two_Plus_PI  * theta + Eck6_False_Northing;

  return new MapProjectionCoordinates(
     CoordinateType::eckert6, easting, northing );
}


MSP::CCS::GeodeticCoordinates* Eckert6::convertToGeodetic(
   MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
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

  double dx, dy;
  double theta;
  double i;
  double longitude, latitude;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Eck6_False_Easting + Eck6_Min_Easting))
      || (easting > (Eck6_False_Easting + Eck6_Max_Easting)))
  { /* Easting out of range  */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < (Eck6_False_Northing - Eck6_Delta_Northing))
      || (northing > (Eck6_False_Northing + Eck6_Delta_Northing)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy = northing - Eck6_False_Northing;
  dx = easting - Eck6_False_Easting;
  theta = Inv_Ra_Over_Sqrt_Two_Plus_PI * dy / 2.0;
  i = (theta + sin(theta)) / one_PLUS_PI_OVER_2;
  if (i > 1.0)
    latitude = MAX_LAT;
  else if (i < -1.0)
    latitude = -MAX_LAT;
  else
    latitude = asin(i);
  longitude  =
     Eck6_Origin_Long + Inv_Ra_Over_Sqrt_Two_Plus_PI * dx / (1 + cos(theta));

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

  return new GeodeticCoordinates(
     CoordinateType::geodetic, longitude, latitude );
}



// CLASSIFICATION: UNCLASSIFIED
