// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: VAN DER GRINTEN
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Van Der Grinten projection
 *    coordinates (easting and northing in meters).  The Van Der Grinten
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
 *          GRIN_NO_ERROR           : No errors occurred in function
 *          GRIN_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          GRIN_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          GRIN_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~20,000,000 m,
 *                                       depending on ellipsoid parameters)
 *          GRIN_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~20,000,000 m,
 *                                       depending on ellipsoid parameters)
 *          GRIN_RADIUS_ERROR       : Coordinates too far from pole,
 *                                      depending on ellipsoid and
 *                                      projection parameters
 *          GRIN_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          GRIN_A_ERROR            : Semi-major axis less than or equal to zero
 *          GRIN_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                      (250 to 350)
 *
 * REUSE NOTES
 *
 *    VAN DER GRINTEN is intended for reuse by any application that performs a
 *    Van Der Grinten projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on VAN DER GRINTEN can be found in the Reuse Manual.
 *
 *    VAN DER GRINTEN originated from : U.S. Army Topographic Engineering Center
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
 *    VAN DER GRINTEN has no restrictions.
 *
 * ENVIRONMENT
 *
 *    VAN DER GRINTEN was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    3-1-07          Original Code
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "VanDerGrinten.h"
#include "MapProjection3Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h    - Standard C math library
 *    VanDerGrinten.h - Is for prototype error checking
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

const double PI = 3.14159265358979323e0;  /* PI                            */
const double PI_OVER_2 = ( PI / 2.0);
const double MAX_LAT = ( 90 * (PI / 180.0) );  /* 90 degrees in radians   */
const double TWO_PI = (2.0 * PI);
const double TWO_OVER_PI = (2.0 / PI);
const double PI_OVER_3 = (PI / 3.0);
const double ONE_THIRD  = (1.0 / 3.0);



/************************************************************************/
/*                              FUNCTIONS     
 *
 */

VanDerGrinten::VanDerGrinten( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
  es2( 0.0066943799901413800 ),             
  es4( 4.4814723452405e-005 ),              
  es6( 3.0000678794350e-007 ),             
  Ra( 6371007.1810824 ),                   
  PI_Ra( 20015109.356056 ),
  Grin_Origin_Long( 0.0 ),                  
  Grin_False_Easting( 0.0 ),
  Grin_False_Northing( 0.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * projection parameters as inputs, and sets the corresponding state
 * variables.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *  ellipsoidSemiMajorAxis   : Semi-major axis of ellipsoid, in meters   (input)
 *  ellipsoidFlattening      : Flattening of ellipsoid                   (input)
 *  centralMeridian          : Longitude in radians at the center of     (input)
 *                             the projection
 *  falseEasting             : A coordinate value in meters assigned to the
 *                             central meridian of the projection.       (input)
 *  falseNorthing            : A coordinate value in meters assigned to the
 *                             origin latitude of the projection         (input)
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
  flattening    = ellipsoidFlattening;

  es2 = 2 * flattening - flattening * flattening;
  es4 = es2 * es2;
  es6 = es4 * es2;
  /* spherical radius */
  Ra = semiMajorAxis * (1.0 - es2 / 6.0 - 17.0 * es4 / 360.0 - 67.0 * es6 /3024.0);
  PI_Ra = PI * Ra;
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Grin_Origin_Long = centralMeridian;
  Grin_False_Easting = falseEasting;
  Grin_False_Northing = falseNorthing;
}


VanDerGrinten::VanDerGrinten( const VanDerGrinten &v )
{
  semiMajorAxis = v.semiMajorAxis;
  flattening = v.flattening;
  es2 = v.es2;     
  es4 = v.es4; 
  es6 = v.es6; 
  Ra = v.Ra; 
  PI_Ra = v.PI_Ra; 
  Grin_Origin_Long = v.Grin_Origin_Long; 
  Grin_False_Easting = v.Grin_False_Easting; 
  Grin_False_Northing = v.Grin_False_Northing; 
}


VanDerGrinten::~VanDerGrinten()
{
}


VanDerGrinten& VanDerGrinten::operator=( const VanDerGrinten &v )
{
  if( this != &v )
  {
    semiMajorAxis = v.semiMajorAxis;
    flattening = v.flattening;
    es2 = v.es2;     
    es4 = v.es4; 
    es6 = v.es6; 
    Ra = v.Ra; 
    PI_Ra = v.PI_Ra; 
    Grin_Origin_Long = v.Grin_Origin_Long; 
    Grin_False_Easting = v.Grin_False_Easting; 
    Grin_False_Northing = v.Grin_False_Northing; 
  }

  return *this;
}


MapProjection3Parameters* VanDerGrinten::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters, and Van Der Grinten projection parameters.
 *
 *    ellipsoidSemiMajorAxis     : Semi-major axis of ellipsoid, in meters   (output)
 *    ellipsoidFlattening        : Flattening of ellipsoid                   (output)
 *    centralMeridian            : Longitude in radians at the center of     (output)
 *                                 the projection
 *    falseEasting               : A coordinate value in meters assigned to the
 *                                 central meridian of the projection.       (output)
 *    falseNorthing              : A coordinate value in meters assigned to the
 *                                 origin latitude of the projection         (output)
 */

  return new MapProjection3Parameters( CoordinateType::vanDerGrinten, Grin_Origin_Long, Grin_False_Easting, Grin_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* VanDerGrinten::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Van Der Grinten projection (easting and northing)
 * coordinates, according to the current ellipsoid and Van Der Grinten projection
 * parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda) in radians       (input)
 *    latitude          : Latitude (phi) in radians           (input)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  double dlam;                      /* Longitude - Central Meridan */
  double aa, aasqr;
  double gg;
  double pp, ppsqr;
  double gg_MINUS_ppsqr, ppsqr_PLUS_aasqr;
  double in_theta;
  double theta;
  double sin_theta, cos_theta;
  double qq;
  double easting, northing;

  double longitude = geodeticCoordinates->longitude();
  double latitude  = geodeticCoordinates->latitude();

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  {  /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  {  /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  dlam = longitude - Grin_Origin_Long;
  if (dlam > PI)
  {
    dlam -= TWO_PI;
  }
  if (dlam < -PI)
  {
    dlam += TWO_PI;
  }

  if (latitude == 0.0)
  {
    easting = Ra * dlam + Grin_False_Easting;
    northing = 0.0;
  }
  else if (dlam == 0.0 || floatEq(latitude,MAX_LAT,.00001)  || floatEq(latitude,-MAX_LAT,.00001))
  {
    in_theta = fabs(TWO_OVER_PI * latitude);

    if (in_theta > 1.0)
      in_theta = 1.0;
    else if (in_theta < -1.0)
      in_theta = -1.0;

    theta = asin(in_theta);
    easting = 0.0;
    northing = PI_Ra * tan(theta / 2) + Grin_False_Northing;
    if (latitude < 0.0)
      northing *= -1.0;
  }
  else
  {
    aa = 0.5 * fabs(PI / dlam - dlam / PI);
    in_theta = fabs(TWO_OVER_PI * latitude);

    if (in_theta > 1.0)
      in_theta = 1.0;
    else if (in_theta < -1.0)
      in_theta = -1.0;

    theta = asin(in_theta);
    sin_theta = sin(theta);
    cos_theta = cos(theta);
    gg = cos_theta / (sin_theta + cos_theta - 1);
    pp = gg * (2 / sin_theta - 1);
    aasqr = aa * aa;
    ppsqr = pp * pp;
    gg_MINUS_ppsqr = gg - ppsqr;
    ppsqr_PLUS_aasqr = ppsqr + aasqr;
    qq = aasqr + gg;
    easting = PI_Ra * (aa * (gg_MINUS_ppsqr) +
                        sqrt(aasqr * (gg_MINUS_ppsqr) * (gg_MINUS_ppsqr) -
                             (ppsqr_PLUS_aasqr) * (gg * gg - ppsqr))) /
               (ppsqr_PLUS_aasqr) + Grin_False_Easting;
    if (dlam < 0.0)
      easting *= -1.0;
    northing = PI_Ra * (pp * qq - aa * sqrt ((aasqr + 1) * (ppsqr_PLUS_aasqr) - qq * qq)) /
                (ppsqr_PLUS_aasqr) + Grin_False_Northing;
    if (latitude < 0.0)
      northing *= -1.0;
  }

  return new MapProjectionCoordinates( CoordinateType::vanDerGrinten, easting, northing );
}


MSP::CCS::GeodeticCoordinates* VanDerGrinten::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Grinten projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid and Grinten projection
 * coordinates.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    easting           : Easting (X) in meters                  (input)
 *    northing          : Northing (Y) in meters                 (input)
 *    longitude         : Longitude (lambda) in radians          (output)
 *    latitude          : Latitude (phi) in radians              (output)
 */

  double dx, dy;
  double xx, xxsqr;
  double yy, yysqr, two_yysqr;
  double xxsqr_PLUS_yysqr;
  double c1;
  double c2;
  double c3, c3sqr;
  double c2_OVER_3c3;
  double dd;
  double a1;
  double m1;
  double i;
  double theta1;
  double temp;
  const double epsilon = 1.0e-2;
  double delta = PI_Ra + epsilon;
  double longitude, latitude;

  double easting = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting > (Grin_False_Easting + delta)) ||
      (easting < (Grin_False_Easting - delta)))
  { /* Easting out of range */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing > (Grin_False_Northing + delta)) ||
      (northing < (Grin_False_Northing - delta)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  temp = sqrt(easting * easting + northing * northing);

  if ((temp > (Grin_False_Easting  + PI_Ra + epsilon)) ||
      (temp > (Grin_False_Northing + PI_Ra + epsilon)) ||
      (temp < (Grin_False_Easting  - PI_Ra - epsilon)) ||
      (temp < (Grin_False_Northing - PI_Ra - epsilon)))
  { /* Point is outside of projection area */
      throw CoordinateConversionException( ErrorMessages::radius );
  }

  dy = northing - Grin_False_Northing;
  dx = easting - Grin_False_Easting;
  xx = dx / PI_Ra;
  yy = dy / PI_Ra;
  xxsqr = xx * xx;
  yysqr = yy * yy;
  xxsqr_PLUS_yysqr = xxsqr + yysqr;
  two_yysqr = 2 * yysqr;

  if (northing == 0.0)
    latitude = 0.0;
  else
  {
    c1 = - fabs(yy) * (1 + xxsqr_PLUS_yysqr);
    c2 = c1 - two_yysqr + xxsqr;
    c3 = - 2 * c1 + 1 + two_yysqr + (xxsqr_PLUS_yysqr) * (xxsqr_PLUS_yysqr);
    c2_OVER_3c3 = c2 / (3.0 * c3);
    c3sqr = c3 * c3;
    dd = yysqr / c3 + ((2 * c2 * c2 * c2) / (c3sqr * c3) - (9 * c1 * c2) / (c3sqr)) / 27;
    a1 = (c1 - c2 * c2_OVER_3c3) /c3;
    m1 = 2 * sqrt(-ONE_THIRD * a1);
    i = 3 * dd/ (a1 * m1);
    if ((i > 1.0)||(i < -1.0))
      latitude = MAX_LAT;
    else
    {
      theta1 = ONE_THIRD * acos(3 * dd / (a1 * m1));
      latitude = PI * (-m1 * cos(theta1 + PI_OVER_3) - c2_OVER_3c3);
    }
  }
  if (northing < 0.0)
    latitude *= -1.0;

  if (xx == 0.0)
    longitude = Grin_Origin_Long;
  else
  {
    longitude = PI * (xxsqr_PLUS_yysqr - 1 +
                       sqrt(1 + (2 * xxsqr - two_yysqr) + (xxsqr_PLUS_yysqr) * (xxsqr_PLUS_yysqr))) /
                 (2 * xx) + Grin_Origin_Long;
  }
  if (latitude > PI_OVER_2)  /* force distorted values to 90, -90 degrees */
    latitude = PI_OVER_2;
  else if (latitude < -PI_OVER_2)
    latitude = -PI_OVER_2;

  if (longitude > PI)
    longitude -= TWO_PI;
  if (longitude < -PI)
    longitude += TWO_PI;

  if (longitude > PI) /* force distorted values to 180, -180 degrees */
    longitude = PI;
  else if (longitude < -PI)
    longitude = -PI;

  return new GeodeticCoordinates( CoordinateType::geodetic, longitude, latitude );
}


double VanDerGrinten::floatEq( double x, double v, double epsilon )
{
  return (((v - epsilon) < x) && (x < (v + epsilon)));
}


// CLASSIFICATION: UNCLASSIFIED
