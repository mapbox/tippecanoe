// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: SINUSOIDAL
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates 
 *    (latitude and longitude in radians) and Sinusoid projection coordinates
 *    (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          SINU_NO_ERROR           : No errors occurred in function
 *          SINU_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          SINU_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          SINU_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~20,000,000 m,
 *                                       depending on ellipsoid parameters)
 *          SINU_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~10,000,000 m,
 *                                       depending on ellipsoid parameters)
 *          SINU_CENT_MER_ERROR     : Origin longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          SINU_A_ERROR            : Semi-major axis less than or equal to zero
 *          SINU_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                      (250 to 350)
 *
 * REUSE NOTES
 *
 *    SINUSOIDAL is intended for reuse by any application that performs a
 *    Sinusoid projection or its inverse.
 *    
 * REFERENCES
 *
 *    Further information on SINUSOIDAL can be found in the Reuse Manual.
 *
 *    SINUSOIDAL originated from :  U.S. Army Topographic Engineering Center
 *                                  Geospatial Information Division
 *                                  7701 Telegraph Road
 *                                  Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    SINUSOIDAL has no restrictions.
 *
 * ENVIRONMENT
 *
 *    SINUSOIDAL was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    07-15-99          Original Code
 *    03-05-07          Original C++ Code
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "Sinusoidal.h"
#include "MapProjection3Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h     - Standard C math library
 *    Sinusoidal.h - Is for prototype error checking
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

const double PI = 3.14159265358979323e0;           /* PI                            */
const double PI_OVER_2 =  (PI / 2.0);                 
const double TWO_PI = (2.0 * PI);                  


double sinuCoeffTimesSine( double coeff, double x, double latit )
{
  return coeff * sin(x * latit);
}


double floatEq( double x, double  v, double epsilon )
{
  return ((v - epsilon) < x) && (x < (v + epsilon));
}


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

Sinusoidal::Sinusoidal( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
  es2( 0.0066943799901413800 ),             
  es4( 4.4814723452405e-005 ),              
  es6( 3.0000678794350e-007 ),              
  c0( .99832429845280 ),           
  c1( .0025146070605187 ),
  c2( 2.6390465943377e-006 ),
  c3( 3.4180460865959e-009 ),
  a0( .0025188265843907 ),
  a1( 3.7009490356205e-006 ),
  a2( 7.4478137675038e-009 ),
  a3( 1.7035993238596e-011 ),
  Sinu_Origin_Long( 0.0 ),
  Sinu_False_Easting( 0.0 ),
  Sinu_False_Northing( 0.0 ),
  Sinu_Max_Easting( 20037509.0 ),
  Sinu_Min_Easting( -20037509.0 ),
  Sinu_Delta_Northing( 10001966.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * Sinusoidal projection parameters as inputs, and sets the corresponding state
 * variables.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    ellipsoidSemiMajorAxis   : Semi-major axis of ellipsoid, in meters (input)
 *    ellipsoidFlattening      : Flattening of ellipsoid                 (input)
 *    centralMeridian          : Longitude in radians at the center of   (input)
 *                               the projection
 *    falseEasting             : A coordinate value in meters assigned to the
 *                               central meridian of the projection.     (input)
 *    falseNorthing            : A coordinate value in meters assigned to the
 *                               origin latitude of the projection       (input)
 */

  double j;
  double One_MINUS_es2, Sqrt_One_MINUS_es2, e1, e2, e3, e4;
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
  j   = 45.0 * es6 / 1024.0;
  c0  = 1.0 - es2 / 4.0 - 3.0 * es4 / 64.0 - 5.0 * es6 / 256.0;
  c1  = 3.0 * es2 / 8.0 + 3.0 * es4 / 32.0 + j;
  c2  = 15.0 * es4 / 256.0 + j;
  c3  = 35.0 * es6 / 3072.0;
  One_MINUS_es2 = 1.0 - es2;
  Sqrt_One_MINUS_es2 = sqrt(One_MINUS_es2);
  e1  = (1.0 - Sqrt_One_MINUS_es2) / (1.0 + Sqrt_One_MINUS_es2);
  e2  = e1 * e1;
  e3  = e2 * e1;
  e4  = e3 * e1;
  a0  = 3.0 * e1 / 2.0 - 27.0 * e3 / 32.0 ;
  a1  = 21.0 * e2 / 16.0 - 55.0 * e4 / 32.0;
  a2  = 151.0 * e3 / 96.0;
  a3  = 1097.0 * e4 / 512.0;
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Sinu_Origin_Long    = centralMeridian;
  Sinu_False_Northing = falseNorthing;
  Sinu_False_Easting  = falseEasting;

  if (Sinu_Origin_Long > 0)
  {
    Sinu_Max_Easting = 19926189.0;
    Sinu_Min_Easting = -20037509.0;
  }
  else if (Sinu_Origin_Long < 0)
  {
    Sinu_Max_Easting = 20037509.0;
    Sinu_Min_Easting = -19926189.0;
  }
  else
  {
    Sinu_Max_Easting = 20037509.0;
    Sinu_Min_Easting = -20037509.0;
  }
}


Sinusoidal::Sinusoidal( const Sinusoidal &s )
{
  semiMajorAxis = s.semiMajorAxis;
  flattening = s.flattening;
  es2 = s.es2;     
  es4 = s.es4; 
  es6 = s.es6; 
  c0 = s.c0; 
  c1 = s.c1; 
  c2 = s.c2; 
  c3 = s.c3; 
  a0 = s.a0; 
  a1 = s.a1; 
  a2 = s.a2; 
  a3 = s.a3; 
  Sinu_Origin_Long = s.Sinu_Origin_Long; 
  Sinu_False_Easting = s.Sinu_False_Easting; 
  Sinu_False_Northing = s.Sinu_False_Northing; 
  Sinu_Max_Easting = s.Sinu_Max_Easting; 
  Sinu_Min_Easting = s.Sinu_Min_Easting; 
  Sinu_Delta_Northing = s.Sinu_Delta_Northing; 
}


Sinusoidal::~Sinusoidal()
{
}


Sinusoidal& Sinusoidal::operator=( const Sinusoidal &s )
{
  if( this != &s )
  {
    semiMajorAxis = s.semiMajorAxis;
    flattening    = s.flattening;
    es2 = s.es2;     
    es4 = s.es4; 
    es6 = s.es6; 
    c0  = s.c0; 
    c1  = s.c1; 
    c2  = s.c2; 
    c3  = s.c3; 
    a0  = s.a0; 
    a1  = s.a1; 
    a2  = s.a2; 
    a3  = s.a3; 
    Sinu_Origin_Long    = s.Sinu_Origin_Long; 
    Sinu_False_Easting  = s.Sinu_False_Easting; 
    Sinu_False_Northing = s.Sinu_False_Northing; 
    Sinu_Max_Easting    = s.Sinu_Max_Easting; 
    Sinu_Min_Easting    = s.Sinu_Min_Easting; 
    Sinu_Delta_Northing = s.Sinu_Delta_Northing; 
  }

  return *this;
}


MapProjection3Parameters* Sinusoidal::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters, and Sinusoidal projection parameters.
 *
 *  ellipsoidSemiMajorAxis : Semi-major axis of ellipsoid, in meters   (output)
 *  ellipsoidFlattening    : Flattening of ellipsoid                   (output)
 *    centralMeridian        : Longitude in radians at the center of   (output)
 *                             the projection
 *    falseEasting           : A coordinate value in meters assigned to the
 *                             central meridian of the projection.     (output)
 *    falseNorthing          : A coordinate value in meters assigned to the
 *                             origin latitude of the projection       (output)
 */

  return new MapProjection3Parameters(
     CoordinateType::sinusoidal,
     Sinu_Origin_Long,
     Sinu_False_Easting,
     Sinu_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* Sinusoidal::convertFromGeodetic(
   MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Sinusoidal projection (easting and northing)
 * coordinates, according to the current ellipsoid and Sinusoidal projection
 * parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda) in radians       (input)
 *    latitude          : Latitude (phi) in radians           (input)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  double sin2lat, sin4lat, sin6lat;
  double dlam;                      /* Longitude - Central Meridan */
  double mm;
  double MM;

  double longitude = geodeticCoordinates->longitude();
  double latitude  = geodeticCoordinates->latitude();
  double slat      = sin(latitude);

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  { /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  { /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  dlam = longitude - Sinu_Origin_Long;
  if (dlam > PI)
  {
    dlam -= TWO_PI;
  }
  if (dlam < -PI)
  {
    dlam += TWO_PI;
  }
  mm = sqrt(1.0 - es2 * slat * slat);

  sin2lat = sinuCoeffTimesSine(c1, 2.0, latitude);
  sin4lat = sinuCoeffTimesSine(c2, 4.0, latitude);
  sin6lat = sinuCoeffTimesSine(c3, 6.0, latitude);
  MM = semiMajorAxis * (c0 * latitude - sin2lat + sin4lat - sin6lat);

  double easting = semiMajorAxis * dlam * cos(latitude) / mm + Sinu_False_Easting;
  double northing = MM + Sinu_False_Northing;

  return new MapProjectionCoordinates( CoordinateType::sinusoidal, easting, northing );
}


MSP::CCS::GeodeticCoordinates* Sinusoidal::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Sinusoidal projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid and Sinusoidal projection
 * coordinates.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    easting           : Easting (X) in meters                  (input)
 *    northing          : Northing (Y) in meters                 (input)
 *    longitude         : Longitude (lambda) in radians          (output)
 *    latitude          : Latitude (phi) in radians              (output)
 */

  double dx;     /* Delta easting - Difference in easting (easting-FE)      */
  double dy;     /* Delta northing - Difference in northing (northing-FN)   */
  double mu;
  double sin2mu, sin4mu, sin6mu, sin8mu;
  double sin_lat;
  double longitude, latitude;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Sinu_False_Easting + Sinu_Min_Easting))
      || (easting > (Sinu_False_Easting + Sinu_Max_Easting)))
  { /* Easting out of range */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < (Sinu_False_Northing - Sinu_Delta_Northing))
      || (northing > (Sinu_False_Northing + Sinu_Delta_Northing)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy = northing - Sinu_False_Northing;
  dx = easting  - Sinu_False_Easting;

  mu = dy / (semiMajorAxis * c0);
  sin2mu = sinuCoeffTimesSine(a0, 2.0, mu);
  sin4mu = sinuCoeffTimesSine(a1, 4.0, mu);
  sin6mu = sinuCoeffTimesSine(a2, 6.0, mu);
  sin8mu = sinuCoeffTimesSine(a3, 8.0, mu);
  latitude = mu + sin2mu + sin4mu + sin6mu + sin8mu;

  if (latitude > PI_OVER_2)  /* force distorted values to 90, -90 degrees */
    latitude = PI_OVER_2;
  else if (latitude < -PI_OVER_2)
    latitude = -PI_OVER_2;

  if (floatEq(fabs(latitude),PI_OVER_2,1.0e-8))
    longitude = Sinu_Origin_Long;
  else
  {
    sin_lat = sin(latitude);
    longitude = Sinu_Origin_Long + dx * sqrt(1.0 - es2 *
                                              sin_lat * sin_lat) / (semiMajorAxis * cos(latitude));


    if (longitude > PI)
      longitude -= TWO_PI;
    if (longitude < -PI)
      longitude += TWO_PI;

    if (longitude > PI)  /* force distorted values to 180, -180 degrees */
      longitude = PI;
    else if (longitude < -PI)
      longitude = -PI;
  }

  return new GeodeticCoordinates( CoordinateType::geodetic, longitude, latitude );
}



// CLASSIFICATION: UNCLASSIFIED
