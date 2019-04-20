// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: OBLIQUE MERCATOR
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Oblique Mercator
 *    projection coordinates (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       OMERC_NO_ERROR           : No errors occurred in function
 *       OMERC_LAT_ERROR          : Latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *       OMERC_LON_ERROR          : Longitude outside of valid range
 *                                     (-180 to 360 degrees)
 *       OMERC_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                     (-89 to 89 degrees)
 *       OMERC_LAT1_ERROR         : First latitude outside of valid range
 *                                     (-89 to 89 degrees, excluding 0)
 *       OMERC_LAT2_ERROR         : First latitude outside of valid range
 *                                     (-89 to 89 degrees)
 *       OMERC_LON1_ERROR         : First longitude outside of valid range
 *                                     (-180 to 360 degrees)
 *       OMERC_LON2_ERROR         : Second longitude outside of valid range
 *                                     (-180 to 360 degrees)
 *       OMERC_LAT1_LAT2_ERROR    : First and second latitudes can not be equal
 *       OMERC_DIFF_HEMISPHERE_ERROR: First and second latitudes can not be
 *                                     in different hemispheres
 *       OMERC_EASTING_ERROR      : Easting outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       OMERC_NORTHING_ERROR     : Northing outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       OMERC_A_ERROR            : Semi-major axis less than or equal to zero
 *       OMERC_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                     (250 to 350)
 *       OMERC_SCALE_FACTOR_ERROR : Scale factor outside of valid
 *                                     range (0.3 to 3.0)
 *       OMERC_LON_WARNING        : Distortion will result if longitude is 90 degrees or more
 *                                     from the Central Meridian
 *
 * REUSE NOTES
 *
 *    OBLIQUE MERCATOR is intended for reuse by any application that 
 *    performs an Oblique Mercator projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on OBLIQUE MERCATOR can be found in the Reuse Manual.
 *
 *    OBLIQUE MERCATOR originated from:     U.S. Army Topographic Engineering Center
 *                                          Geospatial Information Division
 *                                          7701 Telegraph Road
 *                                          Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    OBLIQUE MERCATOR has no restrictions.
 *
 * ENVIRONMENT
 *
 *    OBLIQUE MERCATOR was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. MSDOS with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    06-07-00          Original Code
 *    03-02-07          Original C++ Code
 *    05-11-11          BAEts28017 - Fix Oblique Mercator near poles
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "ObliqueMercator.h"
#include "ObliqueMercatorParameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"
#include "WarningMessages.h"

/*
 *    math.h     - Standard C math library
 *    ObliqueMercator.h   - Is for prototype error checking
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

const double PI = 3.14159265358979323e0;  /* PI                            */
const double PI_OVER_2 = ( PI / 2.0);                 
const double PI_OVER_4 = ( PI / 4.0);                 
const double TWO_PI = ( 2.0 * PI);                 
const double MIN_SCALE_FACTOR = 0.3;
const double MAX_SCALE_FACTOR = 3.0;


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

ObliqueMercator::ObliqueMercator( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double originLatitude, double longitude1, double latitude1, double longitude2, double latitude2, double falseEasting, double falseNorthing, double scaleFactor ) :
  CoordinateSystem(),
  es( 0.08181919084262188000 ),
  es_OVER_2( .040909595421311 ),
  OMerc_A( 6383471.9177251 ),
  OMerc_B( 1.0008420825413 ),
  OMerc_E( 1.0028158089754 ),
  OMerc_gamma( .41705894983580 ),
  OMerc_azimuth( .60940407333533 ),
  OMerc_Origin_Long( -.46732023406900 ),
  cos_gamma( .91428423352628 ),
  sin_gamma( .40507325303611 ),
  sin_azimuth( .57237890829911 ),  
  cos_azimuth( .81998925927985 ),
  A_over_B( 6378101.0302010 ),
  B_over_A( 1.5678647849335e-7 ),
  OMerc_u( 5632885.2272051 ),
  OMerc_Origin_Lat( ((45.0 * PI) / 180.0) ),
  OMerc_Lon_1( ((-5.0 * PI) / 180.0) ),
  OMerc_Lat_1( ((40.0 * PI) / 180.0) ),
  OMerc_Lon_2( ((5.0 * PI) / 180.0) ),
  OMerc_Lat_2( ((50.0 * PI) / 180.0) ),
  OMerc_False_Easting( 0.0 ),
  OMerc_False_Northing( 0.0 ),
  OMerc_Scale_Factor( 1.0 ),
  OMerc_Delta_Northing( 40000000.0 ),
  OMerc_Delta_Easting(  40000000.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * projection parameters as inputs, and sets the corresponding state
 * variables.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    ellipsoidSemiMajorAxis   : Semi-major axis of ellipsoid, in meters  (input)
 *    ellipsoidFlattening      : Flattening of ellipsoid                  (input)
 *    originLatitude           : Latitude, in radians, at which the       (input)
 *                               point scale factor is 1.0
 *    longitude1               : Longitude, in radians, of first point lying on
 *                               central line                             (input)
 *    latitude1                : Latitude, in radians, of first point lying on
 *                               central line                             (input)
 *    longitude2               : Longitude, in radians, of second point lying on
 *                               central line                             (input)
 *    latitude2                : Latitude, in radians, of second point lying on
 *                               central line                             (input)
 *    falseEasting             : A coordinate value, in meters, assigned to the
 *                               central meridian of the projection       (input)
 *    falseNorthing            : A coordinate value, in meters, assigned to the
 *                               origin latitude of the projection        (input)
 *    scaleFactor              : Multiplier which reduces distances in the
 *                               projection to the actual distance on the
 *                               ellipsoid                                (input)
 */

  double inv_f = 1 / ellipsoidFlattening;
  double es2, one_MINUS_es2;
  double cos_olat, cos_olat2;
  double sin_olat, sin_olat2, es2_sin_olat2;
  double t0, t1, t2;
  double D, D2, D2_MINUS_1, sqrt_D2_MINUS_1;
  double H, L, LH;
  double E2;
  double F, G, J, P;
  double dlon;

  if (ellipsoidSemiMajorAxis <= 0.0)
  { /* Semi-major axis must be greater than zero */
    throw CoordinateConversionException( ErrorMessages::semiMajorAxis );
  }
  if ((inv_f < 250) || (inv_f > 350))
  { /* Inverse flattening must be between 250 and 350 */
    throw CoordinateConversionException( ErrorMessages::ellipsoidFlattening );
  }
  if ((originLatitude <= -PI_OVER_2) || (originLatitude >= PI_OVER_2))
  { /* origin latitude out of range -  can not be at a pole */
    throw CoordinateConversionException( ErrorMessages::originLatitude );
  }
  if ((latitude1 <= -PI_OVER_2) || (latitude1 >= PI_OVER_2))
  { /* first latitude out of range -  can not be at a pole */
    throw CoordinateConversionException( ErrorMessages::latitude1 );
  }
  if ((latitude2 <= -PI_OVER_2) || (latitude2 >= PI_OVER_2))
  { /* second latitude out of range -  can not be at a pole */
    throw CoordinateConversionException( ErrorMessages::latitude2 );
  }
  if (latitude1 == 0.0)
  { /* first latitude can not be at the equator */
    throw CoordinateConversionException( ErrorMessages::latitude1 );
  }
  if (latitude1 == latitude2)
  { /* first and second latitudes can not be equal */
    throw CoordinateConversionException( ErrorMessages::latitude2 );
  }
  if (((latitude1 < 0.0) && (latitude2 > 0.0)) ||
      ((latitude1 > 0.0) && (latitude2 < 0.0)))
  { /*first and second points can not be in different hemispheres */
    throw CoordinateConversionException( ErrorMessages::omercHemisphere );
  }
  if ((longitude1 < -PI) || (longitude1 > TWO_PI))
  { /* first longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude1 );
  }
  if ((longitude2 < -PI) || (longitude2 > TWO_PI))
  { /* first longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude2 );
  }
  if ((scaleFactor < MIN_SCALE_FACTOR) || (scaleFactor > MAX_SCALE_FACTOR))
  { /* scale factor out of range */
    throw CoordinateConversionException( ErrorMessages::scaleFactor );
  }

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening = ellipsoidFlattening;

  OMerc_Origin_Lat = originLatitude;
  OMerc_Lon_1 = longitude1;
  OMerc_Lat_1 = latitude1;
  OMerc_Lon_2 = longitude2;
  OMerc_Lat_2 = latitude2;
  OMerc_False_Northing = falseNorthing;
  OMerc_False_Easting = falseEasting;
  OMerc_Scale_Factor = scaleFactor;

  es2 = 2 * flattening - flattening * flattening;
  es = sqrt(es2);
  one_MINUS_es2 = 1 - es2;
  es_OVER_2 = es / 2.0;

  cos_olat = cos(OMerc_Origin_Lat);
  cos_olat2 = cos_olat * cos_olat;
  sin_olat = sin(OMerc_Origin_Lat);
  sin_olat2 = sin_olat * sin_olat;
  es2_sin_olat2 = es2 * sin_olat2;

  OMerc_B = sqrt(1 + (es2 * cos_olat2 * cos_olat2) / one_MINUS_es2);
  OMerc_A = (semiMajorAxis * OMerc_B * OMerc_Scale_Factor * sqrt(one_MINUS_es2)) / (1.0 - es2_sin_olat2);  
  A_over_B = OMerc_A / OMerc_B;
  B_over_A = OMerc_B / OMerc_A;

  t0 = omercT(OMerc_Origin_Lat, es * sin_olat, es_OVER_2);
  t1 = omercT(OMerc_Lat_1, es * sin(OMerc_Lat_1), es_OVER_2);  
  t2 = omercT(OMerc_Lat_2, es * sin(OMerc_Lat_2), es_OVER_2);  

  D = (OMerc_B * sqrt(one_MINUS_es2)) / (cos_olat * sqrt(1.0 - es2_sin_olat2)); 
  D2 = D * D;
  if (D2 < 1.0)
    D2 = 1.0;
  D2_MINUS_1 = D2 - 1.0;
  sqrt_D2_MINUS_1 = sqrt(D2_MINUS_1);
  if (D2_MINUS_1 > 1.0e-10)
  {
    if (OMerc_Origin_Lat >= 0.0)
      OMerc_E = (D + sqrt_D2_MINUS_1) * pow(t0, OMerc_B);
    else
      OMerc_E = (D - sqrt_D2_MINUS_1) * pow(t0, OMerc_B);
  }
  else
    OMerc_E = D * pow(t0, OMerc_B);
  H = pow(t1, OMerc_B);
  L = pow(t2, OMerc_B);
  F = OMerc_E / H;
  G = (F - 1.0 / F) / 2.0;
  E2 = OMerc_E * OMerc_E;
  LH = L * H;
  J = (E2 - LH) / (E2 + LH);
  P = (L - H) / (L + H);

  dlon = OMerc_Lon_1 - OMerc_Lon_2;
  if (dlon < -PI )
    OMerc_Lon_2 -= TWO_PI;
  if (dlon > PI)
    OMerc_Lon_2 += TWO_PI;
  dlon = OMerc_Lon_1 - OMerc_Lon_2;
  OMerc_Origin_Long = (OMerc_Lon_1 + OMerc_Lon_2) / 2.0 - (atan(J * tan(OMerc_B * dlon / 2.0) / P)) / OMerc_B;

  dlon = OMerc_Lon_1 - OMerc_Origin_Long;
  if (dlon < -PI )
    OMerc_Origin_Long -= TWO_PI;
  if (dlon > PI)
    OMerc_Origin_Long += TWO_PI;
 
  dlon = OMerc_Lon_1 - OMerc_Origin_Long;
  OMerc_gamma = atan(sin(OMerc_B * dlon) / G);
  cos_gamma = cos(OMerc_gamma);
  sin_gamma = sin(OMerc_gamma);

  OMerc_azimuth = asin(D * sin_gamma);
  cos_azimuth = cos(OMerc_azimuth);
  sin_azimuth = sin(OMerc_azimuth);

  if (OMerc_Origin_Lat >= 0)
    OMerc_u =  A_over_B * atan(sqrt_D2_MINUS_1/cos_azimuth);
  else
    OMerc_u = -A_over_B * atan(sqrt_D2_MINUS_1/cos_azimuth);
}


ObliqueMercator::ObliqueMercator( const ObliqueMercator &om )
{
  semiMajorAxis = om.semiMajorAxis;
  flattening = om.flattening;
  es = om.es;     
  es_OVER_2 = om.es_OVER_2;     
  OMerc_A = om.OMerc_A;     
  OMerc_B = om.OMerc_B;     
  OMerc_E = om.OMerc_E;     
  OMerc_gamma = om.OMerc_gamma; 
  OMerc_azimuth = om.OMerc_azimuth; 
  OMerc_Origin_Long = om.OMerc_Origin_Long; 
  cos_gamma = om.cos_gamma; 
  sin_gamma = om.sin_gamma; 
  sin_azimuth = om.sin_azimuth; 
  cos_azimuth = om.cos_azimuth; 
  A_over_B = om.A_over_B; 
  B_over_A = om.B_over_A; 
  OMerc_u = om.OMerc_u; 
  OMerc_Origin_Lat = om.OMerc_Origin_Lat; 
  OMerc_Lon_1 = om.OMerc_Lon_1; 
  OMerc_Lat_1 = om.OMerc_Lat_1; 
  OMerc_Lon_2 = om.OMerc_Lon_2; 
  OMerc_Lat_2 = om.OMerc_Lat_2; 
  OMerc_False_Easting = om.OMerc_False_Easting; 
  OMerc_False_Northing = om.OMerc_False_Northing; 
  OMerc_Scale_Factor = om.OMerc_Scale_Factor; 
  OMerc_Delta_Northing = om.OMerc_Delta_Northing; 
  OMerc_Delta_Easting = om.OMerc_Delta_Easting; 
}


ObliqueMercator::~ObliqueMercator()
{
}


ObliqueMercator& ObliqueMercator::operator=( const ObliqueMercator &om )
{
  if( this != &om )
  {
    semiMajorAxis = om.semiMajorAxis;
    flattening = om.flattening;
    es = om.es;     
    es_OVER_2 = om.es_OVER_2;     
    OMerc_A = om.OMerc_A;     
    OMerc_B = om.OMerc_B;     
    OMerc_E = om.OMerc_E;     
    OMerc_gamma = om.OMerc_gamma; 
    OMerc_azimuth = om.OMerc_azimuth; 
    OMerc_Origin_Long = om.OMerc_Origin_Long; 
    cos_gamma = om.cos_gamma; 
    sin_gamma = om.sin_gamma; 
    sin_azimuth = om.sin_azimuth; 
    cos_azimuth = om.cos_azimuth; 
    A_over_B = om.A_over_B; 
    B_over_A = om.B_over_A; 
    OMerc_u = om.OMerc_u; 
    OMerc_Origin_Lat = om.OMerc_Origin_Lat; 
    OMerc_Lon_1 = om.OMerc_Lon_1; 
    OMerc_Lat_1 = om.OMerc_Lat_1; 
    OMerc_Lon_2 = om.OMerc_Lon_2; 
    OMerc_Lat_2 = om.OMerc_Lat_2; 
    OMerc_False_Easting = om.OMerc_False_Easting; 
    OMerc_False_Northing = om.OMerc_False_Northing; 
    OMerc_Scale_Factor = om.OMerc_Scale_Factor; 
    OMerc_Delta_Northing = om.OMerc_Delta_Northing; 
    OMerc_Delta_Easting = om.OMerc_Delta_Easting; 
  }

  return *this;
}


ObliqueMercatorParameters* ObliqueMercator::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters and Oblique Mercator projection parameters.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters  (output)
 *    ellipsoidFlattening     : Flattening of ellipsoid                  (output)
 *    originLatitude          : Latitude, in radians, at which the       (output)
 *                              point scale factor is 1.0
 *    longitude1              : Longitude, in radians, of first point lying on
 *                              central line                           (output)
 *    latitude1               : Latitude, in radians, of first point lying on
 *                              central line                           (output)
 *    longitude2              : Longitude, in radians, of second point lying on
 *                              central line                           (output)
 *    latitude2               : Latitude, in radians, of second point lying on
 *                              central line                           (output)
 *    falseEasting            : A coordinate value, in meters, assigned to the
 *                              central meridian of the projection     (output)
 *    falseNorthing           : A coordinate value, in meters, assigned to the
 *                              origin latitude of the projection      (output)
 *    scaleFactor             : Multiplier which reduces distances in the
 *                              projection to the actual distance on the
 *                              ellipsoid                              (output)
 */

  return new ObliqueMercatorParameters( CoordinateType::obliqueMercator, OMerc_Origin_Lat, OMerc_Lon_1, OMerc_Lat_1, OMerc_Lon_2, OMerc_Lat_2, OMerc_False_Easting, OMerc_False_Northing, OMerc_Scale_Factor );
}


MSP::CCS::MapProjectionCoordinates* ObliqueMercator::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Oblique Mercator projection (easting and
 * northing) coordinates, according to the current ellipsoid and Oblique Mercator 
 * projection parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda), in radians       (input)
 *    latitude          : Latitude (phi), in radians           (input)
 *    easting           : Easting (X), in meters               (output)
 *    northing          : Northing (Y), in meters              (output)
 */

  double dlam, B_dlam, cos_B_dlam;
  double t, S, T, V, U;
  double Q, Q_inv;
  /* Coordinate axes defined with respect to the azimuth of the center line */
  /* Natural origin*/
  double v = 0;
  double u = 0;

  double longitude = geodeticCoordinates->longitude();
  double latitude = geodeticCoordinates->latitude();

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  { /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  { /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude );
  }

  dlam = longitude - OMerc_Origin_Long;

  char warning[256];
  warning[0] = '\0';
  if (fabs(dlam) >= PI_OVER_2)
  { /* Distortion will result if Longitude is 90 degrees or more from the Central Meridian */
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

  if (fabs(fabs(latitude) - PI_OVER_2) > 1.0e-10)
  {
    t = omercT(latitude, es * sin(latitude), es_OVER_2);  
    Q = OMerc_E / pow(t, OMerc_B);
    Q_inv = 1.0 / Q;
    S = (Q - Q_inv) / 2.0;
    T = (Q + Q_inv) / 2.0;
    B_dlam = OMerc_B * dlam;
    V = sin(B_dlam);
    U = ((-1.0 * V * cos_gamma) + (S * sin_gamma)) / T;
    if (fabs(fabs(U) - 1.0) < 1.0e-10)
    { /* Point projects into infinity */
      throw CoordinateConversionException( ErrorMessages::longitude );
    }
    else
    {
      v = A_over_B * log((1.0 - U) / (1.0 + U)) / 2.0;
      cos_B_dlam = cos(B_dlam);
      if (fabs(cos_B_dlam) < 1.0e-10)
        u = OMerc_A * B_dlam;
      // Check for longitude span > 90 degrees
      else if (fabs(B_dlam) > PI_OVER_2)
      {
        double temp = atan(((S * cos_gamma) + (V * sin_gamma)) / cos_B_dlam);
        if (temp < 0.0)
          u = A_over_B * (temp + PI);
        else
          u = A_over_B * (temp - PI);
      }
      else
        u = A_over_B * atan(((S * cos_gamma) + (V * sin_gamma)) / cos_B_dlam);
    }
  }
  else
  {
    if (latitude > 0.0)
      v = A_over_B * log(tan(PI_OVER_4 - (OMerc_gamma / 2.0)));
    else
      v = A_over_B * log(tan(PI_OVER_4 + (OMerc_gamma / 2.0)));
    u = A_over_B * latitude;
  }


  u = u - OMerc_u;

  double easting = OMerc_False_Easting + v * cos_azimuth + u * sin_azimuth;
  double northing = OMerc_False_Northing + u * cos_azimuth - v * sin_azimuth;

  return new MapProjectionCoordinates(
     CoordinateType::obliqueMercator, warning, easting, northing );
}


MSP::CCS::GeodeticCoordinates* ObliqueMercator::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Oblique Mercator projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid and Oblique Mercator projection
 * coordinates.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    easting           : Easting (X), in meters                  (input)
 *    northing          : Northing (Y), in meters                 (input)
 *    longitude         : Longitude (lambda), in radians          (output)
 *    latitude          : Latitude (phi), in radians              (output)
 */

  double dx, dy;
  /* Coordinate axes defined with respect to the azimuth of the center line */
  /* Natural origin*/
  double u, v;
  double Q_prime, Q_prime_inv;
  double S_prime, T_prime, V_prime, U_prime;
  double t;
  double es_sin;
  double u_B_over_A;
  double phi;
  double temp_phi = 0.0;
  int count = 60;
  double longitude, latitude;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (OMerc_False_Easting - OMerc_Delta_Easting)) 
      || (easting > (OMerc_False_Easting + OMerc_Delta_Easting)))
  { /* Easting out of range  */
    throw CoordinateConversionException( ErrorMessages::easting );
  }
  if ((northing < (OMerc_False_Northing - OMerc_Delta_Northing)) 
      || (northing > (OMerc_False_Northing + OMerc_Delta_Northing)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing );
  }

  dy = northing - OMerc_False_Northing;
  dx = easting - OMerc_False_Easting;
  v = dx * cos_azimuth - dy * sin_azimuth;
  u = dy * cos_azimuth + dx * sin_azimuth;
  u = u + OMerc_u;
  Q_prime = exp(-1.0 * (v * B_over_A ));
  Q_prime_inv = 1.0 / Q_prime;
  S_prime = (Q_prime - Q_prime_inv) / 2.0;
  T_prime = (Q_prime + Q_prime_inv) / 2.0;
  u_B_over_A = u * B_over_A;
  V_prime = sin(u_B_over_A);
  U_prime = (V_prime * cos_gamma + S_prime * sin_gamma) / T_prime;
  if (fabs(fabs(U_prime) - 1.0) < 1.0e-10)
  {
    if (U_prime > 0)
      latitude = PI_OVER_2;
    else
      latitude = -PI_OVER_2;
    longitude = OMerc_Origin_Long;
  }
  else
  {
    t = pow(OMerc_E / sqrt((1.0 + U_prime) / (1.0 - U_prime)), 1.0 / OMerc_B);
    phi = PI_OVER_2 - 2.0 * atan(t);
    while (fabs(phi - temp_phi) > 1.0e-10 && count)
    {
      temp_phi = phi;
      es_sin = es * sin(phi);
      phi = PI_OVER_2 - 2.0 * atan(t * pow((1.0 - es_sin) / (1.0 + es_sin), es_OVER_2));
      count --;
    }

    if(!count)
      throw CoordinateConversionException( ErrorMessages::northing );

    latitude = phi;
    longitude = OMerc_Origin_Long - atan2((S_prime * cos_gamma - V_prime * sin_gamma), cos(u_B_over_A)) / OMerc_B;
  }

  if (fabs(latitude) < 2.0e-7)  /* force lat to 0 to avoid -0 degrees */
    latitude = 0.0;
  if (latitude > PI_OVER_2)  /* force distorted values to 90, -90 degrees */
    latitude = PI_OVER_2;
  else if (latitude < -PI_OVER_2)
    latitude = -PI_OVER_2;

  if (longitude > PI)
    longitude -= TWO_PI;
  if (longitude < -PI)
    longitude += TWO_PI;

  if (fabs(longitude) < 2.0e-7)  /* force lon to 0 to avoid -0 degrees */
    longitude = 0.0;
  if (longitude > PI)  /* force distorted values to 180, -180 degrees */
    longitude = PI;
  else if (longitude < -PI)
    longitude = -PI;

  char warning[256];
  warning[0] = '\0';
  if (fabs(longitude - OMerc_Origin_Long) >= PI_OVER_2)
  { /* Distortion results if Longitude > 90 degrees from the Central Meridian */
    strcat( warning, MSP::CCS::WarningMessages::longitude );
  }

  return new GeodeticCoordinates(
     CoordinateType::geodetic, warning, longitude, latitude );
}


double ObliqueMercator::omercT( double lat, double e_sinlat, double e_over_2 )
{  
  return (tan(PI_OVER_4 - lat / 2.0)) / (pow((1 - e_sinlat) / (1 + e_sinlat), e_over_2));
}


// CLASSIFICATION: UNCLASSIFIED
