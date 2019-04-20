// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: POLYCONIC
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Polyconic projection coordinates
 *    (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          POLY_NO_ERROR           : No errors occurred in function
 *          POLY_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          POLY_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          POLY_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~20,500,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          POLY_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~15,500,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          POLY_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          POLY_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          POLY_A_ERROR            : Semi-major axis less than or equal to zero
 *          POLY_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                      (250 to 350)
 *          POLY_LON_WARNING        : Distortion will result if longitude is more
 *                                     than 90 degrees from the Central Meridian
 *
 * REUSE NOTES
 *
 *    POLYCONIC is intended for reuse by any application that performs a
 *    Polyconic projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on POLYCONIC can be found in the Reuse Manual.
 *
 *    POLYCONIC originated from :  U.S. Army Topographic Engineering Center
 *                                 Geospatial Information Division
 *                                 7701 Telegraph Road
 *                                 Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    POLYCONIC has no restrictions.
 *
 * ENVIRONMENT
 *
 *    POLYCONIC was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    10-06-99          Original Code
 *    03-05-07          Original C++ Code
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "Polyconic.h"
#include "MapProjection4Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"
#include "WarningMessages.h"

/*
 *    math.h     - Standard C math library
 *    Polyconic.h - Is for prototype error checking
 *    MapProjectionCoordinates.h   - defines map projection coordinates
 *    GeodeticCoordinates.h   - defines geodetic coordinates
 *    CoordinateConversionException.h - Exception handler
 *    ErrorMessages.h  - Contains exception messages
 *    WarningMessages.h  - Contains warning messages
 */


using namespace MSP::CCS;


/***************************************************************************/
/*                               DEFINES 
 *
 */

const double PI = 3.14159265358979323e0;       /* PI                    */
const double PI_OVER_2 = ( PI / 2.0);
const double TWO_PI = (2.0 * PI);
const double FOURTY_ONE (41.0 * PI / 180);     /* 41 degrees in radians */


double polyCoeffTimesSine( double coeff,  double x,  double latit )
{
  return coeff * (sin (x * latit));
}


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

Polyconic::Polyconic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
  es2( 0.0066943799901413800 ),
  es4( 4.4814723452405e-005 ),
  es6( 3.0000678794350e-007 ),
  M0( 0.0 ),
  c0( .99832429845280 ),
  c1( .0025146070605187 ),
  c2( 2.6390465943377e-006 ),
  c3( 3.4180460865959e-009 ),
  Poly_Origin_Long( 0.0 ),
  Poly_Origin_Lat( 0.0 ),
  Poly_False_Easting( 0.0 ),                
  Poly_False_Northing( 0.0 ),
  Poly_Max_Easting( 20037509.0 ),
  Poly_Max_Northing( 15348215.0 ),
  Poly_Min_Easting( -20037509.0 ),
  Poly_Min_Northing( -15348215.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * Polyconic projection parameters as inputs, and sets the corresponding state
 * variables.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
 *    ellipsoidFlattening     : Flattening of ellipsoid                   (input)
 *    centralMeridian         : Longitude in radians at the center of     (input)
 *                              the projection
 *    originLatitude          : Latitude in radians at which the          (input)
 *                              point scale factor is 1.0
 *    falseEasting            : A coordinate value in meters assigned to the
 *                              central meridian of the projection.       (input)
 *    falseNorthing           : A coordinate value in meters assigned to the
 *                              origin latitude of the projection         (input)
 */

  double j, three_es4;
  double lat, sin2lat, sin4lat, sin6lat;
  double inv_f = 1 / ellipsoidFlattening;

  if (ellipsoidSemiMajorAxis <= 0.0)
  { /* Semi-major axis must be greater than zero */
    throw CoordinateConversionException( ErrorMessages::semiMajorAxis );
  }
  if ((inv_f < 250) || (inv_f > 350))
  { /* Inverse flattening must be between 250 and 350 */
    throw CoordinateConversionException( ErrorMessages::ellipsoidFlattening );
  }
  if ((originLatitude < -PI_OVER_2) || (originLatitude > PI_OVER_2))
  { /* origin latitude out of range */
    throw CoordinateConversionException( ErrorMessages::originLatitude );
  }
  if ((centralMeridian < -PI) || (centralMeridian > TWO_PI))
  { /* origin longitude out of range */
    throw CoordinateConversionException( ErrorMessages::centralMeridian );
  }

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening = ellipsoidFlattening;

  Poly_Origin_Lat = originLatitude;
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Poly_Origin_Long = centralMeridian;
  Poly_False_Northing = falseNorthing;
  Poly_False_Easting = falseEasting;
  es2 = 2 * flattening - flattening * flattening;
  es4 = es2 * es2;
  es6 = es4 * es2;

  j = 45.0 * es6 / 1024.0;
  three_es4 = 3.0 * es4;
  c0 = 1.0 - es2 / 4.0 - three_es4 / 64.0 - 5.0 * es6 / 256.0;
  c1 = 3.0 * es2 / 8.0 + three_es4 / 32.0 + j;
  c2 = 15.0 * es4 / 256.0 + j;
  c3 = 35.0 * es6 / 3072.0;

  lat = c0 * Poly_Origin_Lat;
  sin2lat = polyCoeffTimesSine(c1, 2.0, Poly_Origin_Lat);
  sin4lat = polyCoeffTimesSine(c2, 4.0, Poly_Origin_Lat);
  sin6lat = polyCoeffTimesSine(c3, 6.0, Poly_Origin_Lat);
  M0 = polyM(lat, sin2lat, sin4lat, sin6lat);

  if (Poly_Origin_Long > 0)
  {
    GeodeticCoordinates gcMax( CoordinateType::geodetic, Poly_Origin_Long - PI, FOURTY_ONE );
    MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcMax );
    Poly_Max_Northing = tempCoordinates->northing();
    delete tempCoordinates;

    GeodeticCoordinates gcMin( CoordinateType::geodetic, Poly_Origin_Long - PI, -FOURTY_ONE );
    tempCoordinates = convertFromGeodetic( &gcMin );
    Poly_Min_Northing = tempCoordinates->northing();
    delete tempCoordinates;

    Poly_Max_Easting = 19926189.0;
    Poly_Min_Easting = -20037509.0;
  }
  else if (Poly_Origin_Long < 0)
  {
    GeodeticCoordinates gcMax( CoordinateType::geodetic, Poly_Origin_Long + PI, FOURTY_ONE );
    MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcMax );
    Poly_Max_Northing = tempCoordinates->northing();
    delete tempCoordinates;

    GeodeticCoordinates gcMin( CoordinateType::geodetic, Poly_Origin_Long + PI, -FOURTY_ONE );
    tempCoordinates = convertFromGeodetic( &gcMin );
    Poly_Min_Northing = tempCoordinates->northing();
    delete tempCoordinates;

    Poly_Max_Easting = 20037509.0;
    Poly_Min_Easting = -19926189.0;
  }
  else
  {
    GeodeticCoordinates gcMax( CoordinateType::geodetic, PI, FOURTY_ONE );
    MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcMax );
    Poly_Max_Northing = tempCoordinates->northing();
    delete tempCoordinates;

    GeodeticCoordinates gcMin( CoordinateType::geodetic, PI, -FOURTY_ONE );
    tempCoordinates = convertFromGeodetic( &gcMin );    Poly_Min_Northing = tempCoordinates->northing();
    delete tempCoordinates;

    Poly_Max_Easting = 20037509.0;
    Poly_Min_Easting = -20037509.0;
  }

  if(Poly_False_Northing)
  {
    Poly_Max_Northing -= Poly_False_Northing;
    Poly_Min_Northing -= Poly_False_Northing;
  }
}


Polyconic::Polyconic( const Polyconic &p )
{
  semiMajorAxis = p.semiMajorAxis;
  flattening = p.flattening;
  es2 = p.es2;     
  es4 = p.es4; 
  es6 = p.es6; 
  M0 = p.M0; 
  c0 = p.c0; 
  c1 = p.c1; 
  c2 = p.c2; 
  c3 = p.c3; 
  Poly_Origin_Long = p.Poly_Origin_Long; 
  Poly_Origin_Lat = p.Poly_Origin_Lat; 
  Poly_False_Easting = p.Poly_False_Easting; 
  Poly_False_Northing = p.Poly_False_Northing; 
  Poly_Max_Easting = p.Poly_Max_Easting; 
  Poly_Max_Northing = p.Poly_Max_Northing; 
  Poly_Min_Easting = p.Poly_Min_Easting; 
  Poly_Min_Northing = p.Poly_Min_Northing; 
}


Polyconic::~Polyconic()
{
}


Polyconic& Polyconic::operator=( const Polyconic &p )
{
  if( this != &p )
  {
    semiMajorAxis = p.semiMajorAxis;
    flattening = p.flattening;
    es2 = p.es2;     
    es4 = p.es4; 
    es6 = p.es6; 
    M0 = p.M0; 
    c0 = p.c0; 
    c1 = p.c1; 
    c2 = p.c2; 
    c3 = p.c3; 
    Poly_Origin_Long = p.Poly_Origin_Long; 
    Poly_Origin_Lat = p.Poly_Origin_Lat; 
    Poly_False_Easting = p.Poly_False_Easting; 
    Poly_False_Northing = p.Poly_False_Northing; 
    Poly_Max_Easting = p.Poly_Max_Easting; 
    Poly_Max_Northing = p.Poly_Max_Northing; 
    Poly_Min_Easting = p.Poly_Min_Easting; 
    Poly_Min_Northing = p.Poly_Min_Northing; 
  }

  return *this;
}


MapProjection4Parameters* Polyconic::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters, and Polyconic projection parameters.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
 *    ellipsoidFlattening     : Flattening of ellipsoid                   (output)
 *    centralMeridian         : Longitude in radians at the center of     (output)
 *                              the projection
 *    originLatitude          : Latitude in radians at which the          (output)
 *                              point scale factor is 1.0
 *    falseEasting            : A coordinate value in meters assigned to the
 *                              central meridian of the projection.       (output)
 *    falseNorthing           : A coordinate value in meters assigned to the
 *                              origin latitude of the projection         (output)
 */

  return new MapProjection4Parameters( CoordinateType::polyconic, Poly_Origin_Long, Poly_Origin_Lat, Poly_False_Easting, Poly_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* Polyconic::convertFromGeodetic(
   MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Polyconic projection (easting and northing)
 * coordinates, according to the current ellipsoid and Polyconic projection
 * parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda) in radians       (input)
 *    latitude          : Latitude (phi) in radians           (input)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  double lat, sin2lat, sin4lat, sin6lat;
  double dlam;                      /* Longitude - Central Meridan */
  double NN;
  double NN_OVER_tlat;
  double MM;
  double EE;
  double easting, northing;

  double longitude = geodeticCoordinates->longitude();
  double latitude = geodeticCoordinates->latitude();
  double slat = sin(latitude);

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  { /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  { /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude );
  }

  char warning[256];
  warning[0] = '\0';
  dlam = longitude - Poly_Origin_Long;
  if (fabs(dlam) > (PI / 2))
  { /* Distortion results if Longitude is > 90 deg from the Central Meridian */
    strcat( warning, MSP::CCS::WarningMessages::longitude );
  }
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
    easting = semiMajorAxis * dlam + Poly_False_Easting;
    northing = -M0 + Poly_False_Northing;
  }
  else
  {
    NN = semiMajorAxis / sqrt(1.0 - es2 * (slat * slat));
    NN_OVER_tlat = NN  / tan(latitude);
    lat = c0 * latitude;
    sin2lat = polyCoeffTimesSine(c1, 2.0, latitude);
    sin4lat = polyCoeffTimesSine(c2, 4.0, latitude);
    sin6lat = polyCoeffTimesSine(c3, 6.0, latitude);
    MM = polyM(lat, sin2lat, sin4lat, sin6lat);
    EE = dlam * slat;
    easting = NN_OVER_tlat * sin(EE) + Poly_False_Easting;
    northing = MM - M0 + NN_OVER_tlat * (1.0 - cos(EE)) +
               Poly_False_Northing;
  }

  return new MapProjectionCoordinates(
     CoordinateType::polyconic, warning, easting, northing );
}


MSP::CCS::GeodeticCoordinates* Polyconic::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Polyconic projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid and Polyconic projection
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
  double dx_OVER_Poly_a;
  double AA;
  double BB;
  double CC = 0.0;
  double PHIn, Delta_PHI = 1.0;
  double sin_PHIn;
  double PHI, sin2PHI,sin4PHI, sin6PHI;
  double Mn, Mn_prime, Ma;
  double AA_Ma;
  double Ma2_PLUS_BB;
  double AA_MINUS_Ma;
  double tolerance = 1.0e-12;        /* approximately 1/1000th of
                               an arc second or 1/10th meter */
  double longitude, latitude;
  int count = 45000;

  double easting = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Poly_False_Easting + Poly_Min_Easting))
      || (easting > (Poly_False_Easting + Poly_Max_Easting)))
  { /* Easting out of range */
    throw CoordinateConversionException( ErrorMessages::easting );
  }
  if ((northing < (Poly_False_Northing + Poly_Min_Northing))
      || (northing > (Poly_False_Northing + Poly_Max_Northing)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing );
  }

  dy = northing - Poly_False_Northing;
  dx = easting - Poly_False_Easting;
  dx_OVER_Poly_a = dx / semiMajorAxis;
  if (floatEq(dy,-M0,1))
  {
    latitude = 0.0;
    longitude = dx_OVER_Poly_a + Poly_Origin_Long;
  }
  else
  {
    AA = (M0 + dy) / semiMajorAxis;
    BB = dx_OVER_Poly_a * dx_OVER_Poly_a + (AA * AA);
    PHIn = AA;

    while (fabs(Delta_PHI) > tolerance && count)
    {
      sin_PHIn = sin(PHIn);
      CC = sqrt(1.0 - es2 * sin_PHIn * sin_PHIn) * tan(PHIn);
      PHI = c0 * PHIn;
      sin2PHI = polyCoeffTimesSine(c1, 2.0, PHIn);
      sin4PHI = polyCoeffTimesSine(c2, 4.0, PHIn);
      sin6PHI = polyCoeffTimesSine(c3, 6.0, PHIn);
      Mn = polyM(PHI, sin2PHI, sin4PHI, sin6PHI);
      Mn_prime = c0 - 2.0 * c1 * cos(2.0 * PHIn) + 4.0 * c2 * cos(4.0 * PHIn) -
                 6.0 * c3 * cos(6.0 * PHIn);
      Ma = Mn / semiMajorAxis;
      AA_Ma = AA * Ma;
      Ma2_PLUS_BB = Ma * Ma + BB;
      AA_MINUS_Ma = AA - Ma;
      Delta_PHI = (AA_Ma * CC + AA_MINUS_Ma - 0.5 * (Ma2_PLUS_BB) * CC) /
                  (es2 * sin2PHI * (Ma2_PLUS_BB - 2.0 * AA_Ma) /
                   4.0 * CC + (AA_MINUS_Ma) * (CC * Mn_prime - 2.0 / sin2PHI) - Mn_prime);
      PHIn -= Delta_PHI;
      count --;
    }

    if(!count)
      throw CoordinateConversionException( ErrorMessages::northing );

    latitude = PHIn;

    if (latitude > PI_OVER_2)  /* force distorted values to 90, -90 degrees */
      latitude = PI_OVER_2;
    else if (latitude < -PI_OVER_2)
      latitude = -PI_OVER_2;

    if (floatEq(fabs(latitude),PI_OVER_2,.00001) || (latitude == 0))
      longitude = Poly_Origin_Long;

    else
    {
      longitude = (asin(dx_OVER_Poly_a * CC)) / sin(latitude) +
                   Poly_Origin_Long;
    }
  }
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


double Polyconic::polyM( double c0lat, double c1s2lat, double c2s4lat, double c3s6lat )
{
  return semiMajorAxis * (c0lat - c1s2lat + c2s4lat - c3s6lat);
}


double Polyconic::floatEq( double x, double v, double epsilon )
{
  return ((v - epsilon) < x) && (x < (v + epsilon));
}



// CLASSIFICATION: UNCLASSIFIED
