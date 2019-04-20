// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: CASSINI
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Cassini projection coordinates
 *    (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          CASS_NO_ERROR          : No errors occurred in function
 *          CASS_LAT_ERROR         : Latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *          CASS_LON_ERROR         : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          CASS_EASTING_ERROR     : Easting outside of valid range
 *                                     (False_Easting +/- ~20,000,000 m,
 *                                      depending on ellipsoid parameters
 *                                      and Origin_Latitude)
 *          CASS_NORTHING_ERROR    : Northing outside of valid range
 *                                     (False_Northing +/- ~57,000,000 m,
 *                                      depending on ellipsoid parameters
 *                                      and Origin_Latitude)
 *          CASS_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          CASS_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          CASS_A_ERROR           : Semi-major axis less than or equal to zero
 *          CASS_INV_F_ERROR       : Inverse flattening outside of valid range
 *                                      (250 to 350)
 *          CASS_LON_WARNING       : Distortion will result if longitude is more
 *                                     than 4 degrees from the Central Meridian
 *
 * REUSE NOTES
 *
 *    CASSINI is intended for reuse by any application that performs a
 *    Cassini projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on CASSINI can be found in the Reuse Manual.
 *
 *    CASSINI originated from :  U.S. Army Topographic Engineering Center
 *                               Geospatial Information Division
 *                               7701 Telegraph Road
 *                               Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    CASSINI has no restrictions.
 *
 * ENVIRONMENT
 *
 *    CASSINI was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC 2.8.1
 *    2. MS Windows 95 with MS Visual C++ 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    04-16-99          Original Code
 *    03-07-07          Original C++ Code
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "Cassini.h"
#include "MapProjection4Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"
#include "WarningMessages.h"

/*
 *    math.h    - Standard C math library
 *    Cassini.h - Is for prototype error checking.
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
const double TWO_PI = (2.0 * PI);
const double THIRTY_ONE = (31.0 * PI / 180);   /* 31 degrees in radians */

double cassCoeffTimesSine( double coeff, double x, double latit )
{
  return coeff * (sin (x * latit));
}


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

Cassini::Cassini( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
  es2( 0.0066943799901413800 ),
  es4( 4.4814723452405e-005 ),
  es6( 3.0000678794350e-007 ),
  M0( 0.0 ),
  c0( .99832429845280 ),
  c1( .0025146070605187 ),
  c2( 2.6390465943377e-006 ),
  c3( 3.4180460865959e-009 ),
  One_Minus_es2( .99330562000986 ),
  a0( .0025188265843907 ),
  a1( 3.7009490356205e-006 ),
  a2( 7.4478137675038e-009 ),
  a3( 1.7035993238596e-011 ),
  Cass_Origin_Long( 0.0 ),
  Cass_Origin_Lat( 0.0 ),
  Cass_False_Easting( 0.0 ),
  Cass_False_Northing( 0.0 ),
  Cass_Min_Easting( -20037508.4 ),
  Cass_Max_Easting( 20037508.4 ),
  Cass_Min_Northing( -56575846.0 ),
  Cass_Max_Northing( 56575846.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * Cassini projection parameters as inputs, and sets the corresponding state
 * variables.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters  (input)
 *    ellipsoidFlattening     : Flattening of ellipsoid                  (input)
 *    centralMeridian         : Longitude in radians at the center of    (input)
 *                              the projection
 *    originLatitude          : Latitude in radians at which the         (input)
 *                              point scale factor is 1.0
 *    falseEasting            : A coordinate value in meters assigned to the
 *                              central meridian of the projection.      (input)
 *    falseNorthing           : A coordinate value in meters assigned to the
 *                              origin latitude of the projection        (input)
 */

  double j,three_es4;
  double x, e1, e2, e3, e4;
  double lat, sin2lat, sin4lat, sin6lat;
  double inv_f = 1 / ellipsoidFlattening;

  if (ellipsoidSemiMajorAxis <= 0.0)
  { /* Semi-major axis must be greater than zero */
    throw CoordinateConversionException( ErrorMessages::semiMajorAxis  );
  }
  if ((inv_f < 250) || (inv_f > 350))
  { /* Inverse flattening must be between 250 and 350 */
    throw CoordinateConversionException( ErrorMessages::ellipsoidFlattening  );
  }
  if ((originLatitude < -PI_OVER_2) || (originLatitude > PI_OVER_2))
  { /* origin latitude out of range */
    throw CoordinateConversionException( ErrorMessages::originLatitude  );
  }
  if ((centralMeridian < -PI) || (centralMeridian > TWO_PI))
  { /* origin longitude out of range */
    throw CoordinateConversionException( ErrorMessages::centralMeridian  );
  }

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening    = ellipsoidFlattening;

  Cass_Origin_Lat = originLatitude;
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Cass_Origin_Long = centralMeridian;
  Cass_False_Northing = falseNorthing;
  Cass_False_Easting = falseEasting;
  es2 = 2 * flattening - flattening * flattening;
  es4 = es2 * es2;
  es6 = es4 * es2;
  j = 45.0 * es6 / 1024.0;
  three_es4 = 3.0 * es4;
  c0 = 1 - es2 / 4.0 - three_es4 / 64.0 - 5.0 * es6 / 256.0;
  c1 = 3.0 * es2 /8.0 + three_es4 / 32.0 + j;
  c2 = 15.0 * es4 / 256.0 + j;
  c3 = 35.0 * es6 / 3072.0;
  lat = c0 * Cass_Origin_Lat;
  sin2lat = cassCoeffTimesSine(c1, 2.0, Cass_Origin_Lat);
  sin4lat = cassCoeffTimesSine(c2, 4.0, Cass_Origin_Lat);
  sin6lat = cassCoeffTimesSine(c3, 6.0, Cass_Origin_Lat);
  M0 = cassM( lat, sin2lat, sin4lat, sin6lat );

  One_Minus_es2 = 1.0 - es2;
  x = sqrt (One_Minus_es2);
  e1 = (1 - x) / (1 + x);
  e2 = e1 * e1;
  e3 = e2 * e1;
  e4 = e3 * e1;
  a0 = 3.0 * e1 / 2.0 - 27.0 * e3 / 32.0;
  a1 = 21.0 * e2 / 16.0 - 55.0 * e4 / 32.0;
  a2 = 151.0 * e3 / 96.0;
  a3 = 1097.0 * e4 /512.0;

  if (Cass_Origin_Long > 0)
  {
    GeodeticCoordinates gcMax( CoordinateType::geodetic, Cass_Origin_Long - PI, THIRTY_ONE );
    MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcMax );
    Cass_Max_Northing = tempCoordinates->northing();
    delete tempCoordinates;

    GeodeticCoordinates gcMin( CoordinateType::geodetic, Cass_Origin_Long - PI, -THIRTY_ONE );
    tempCoordinates = convertFromGeodetic( &gcMin );
    Cass_Min_Northing = tempCoordinates->northing();
    delete tempCoordinates;

    Cass_Max_Easting = 19926188.9;
    Cass_Min_Easting = -20037508.4;
  }
  else if (Cass_Origin_Long < 0)
  {
    GeodeticCoordinates gcMax( CoordinateType::geodetic, PI + Cass_Origin_Long, THIRTY_ONE );
    MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcMax );
    Cass_Max_Northing = tempCoordinates->northing();
    delete tempCoordinates;

    GeodeticCoordinates gcMin( CoordinateType::geodetic, PI + Cass_Origin_Long, -THIRTY_ONE );
    tempCoordinates = convertFromGeodetic( &gcMin );
    Cass_Min_Northing = tempCoordinates->northing();
    delete tempCoordinates;

    Cass_Max_Easting = 20037508.4;
    Cass_Min_Easting = -19926188.9;
  }
  else
  {
    GeodeticCoordinates gcMax( CoordinateType::geodetic, PI, THIRTY_ONE );
    MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcMax );
    Cass_Max_Northing = tempCoordinates->northing();
    delete tempCoordinates;

    GeodeticCoordinates gcMin( CoordinateType::geodetic, PI, -THIRTY_ONE );
    tempCoordinates = convertFromGeodetic( &gcMin );
    Cass_Min_Northing = tempCoordinates->northing();
    delete tempCoordinates;

    Cass_Max_Easting = 20037508.4;
    Cass_Min_Easting = -20037508.4;
  }

  if(Cass_False_Northing)
  {
    Cass_Min_Northing -= Cass_False_Northing;
    Cass_Max_Northing -= Cass_False_Northing;
  }
}


Cassini::Cassini( const Cassini &c )
{
  semiMajorAxis = c.semiMajorAxis;
  flattening = c.flattening;
  es2 = c.es2;
  es4 = c.es4;
  es6 = c.es6;
  M0 = c.M0;
  c0 = c.c0;
  c1 = c.c1;
  c2 = c.c2;
  c3 = c.c3;
  One_Minus_es2 = c.One_Minus_es2;
  a0 = c.a0;
  a1 = c.a1;
  a2 = c.a2;
  a3 = c.a3;
  Cass_Origin_Long = c.Cass_Origin_Long;
  Cass_Origin_Lat = c.Cass_Origin_Lat;
  Cass_False_Easting = c.Cass_False_Easting;
  Cass_False_Northing = c.Cass_False_Northing;
  Cass_Min_Easting = c.Cass_Min_Easting;
  Cass_Max_Easting = c.Cass_Max_Easting;
  Cass_Min_Northing = c.Cass_Min_Northing;
  Cass_Max_Northing = c.Cass_Max_Northing;
}


Cassini::~Cassini()
{
}


Cassini& Cassini::operator=( const Cassini &c )
{
  if( this != &c )
  {
    semiMajorAxis = c.semiMajorAxis;
    flattening = c.flattening;
    es2 = c.es2;
    es4 = c.es4;
    es6 = c.es6;
    M0 = c.M0;
    c0 = c.c0;
    c1 = c.c1;
    c2 = c.c2;
    c3 = c.c3;
    One_Minus_es2 = c.One_Minus_es2;
    a0 = c.a0;
    a1 = c.a1;
    a2 = c.a2;
    a3 = c.a3;
    Cass_Origin_Long = c.Cass_Origin_Long;
    Cass_Origin_Lat = c.Cass_Origin_Lat;
    Cass_False_Easting = c.Cass_False_Easting;
    Cass_False_Northing = c.Cass_False_Northing;
    Cass_Min_Easting = c.Cass_Min_Easting;
    Cass_Max_Easting = c.Cass_Max_Easting;
    Cass_Min_Northing = c.Cass_Min_Northing;
    Cass_Max_Northing = c.Cass_Max_Northing;
  }

  return *this;
}


MapProjection4Parameters* Cassini::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters, Cassini projection parameters.
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

  return new MapProjection4Parameters( CoordinateType::cassini, Cass_Origin_Long, Cass_Origin_Lat, Cass_False_Easting, Cass_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* Cassini::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Cassini projection (easting and northing)
 * coordinates, according to the current ellipsoid and Cassini projection
 * parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda) in radians       (input)
 *    latitude          : Latitude (phi) in radians           (input)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  double lat, sin2lat, sin4lat, sin6lat;
  double RD;
  double dlam;                      /* Longitude - Central Meridan */
  double NN;
  double TT;
  double AA, A2, A3, A4, A5;
  double CC;
  double MM;

  double longitude = geodeticCoordinates->longitude();
  double latitude  = geodeticCoordinates->latitude();
  double tlat = tan(latitude);
  double clat = cos(latitude);
  double slat = sin(latitude);

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  { /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  { /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  dlam = longitude - Cass_Origin_Long;

  char warning[100];
  warning[0] = '\0';
  if (fabs(dlam) > (4.0 * PI / 180))
  { /* Distortion results if Longitude is > 4 deg from the Central Meridian */
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
  RD = cassRd( slat );
  NN = semiMajorAxis / RD;
  TT = tlat * tlat;
  AA = dlam * clat;
  A2 = AA * AA;
  A3 = AA * A2;
  A4 = AA * A3;
  A5 = AA * A4;
  CC = es2 * clat * clat / One_Minus_es2;
  lat = c0 * latitude;
  sin2lat = cassCoeffTimesSine(c1, 2.0, latitude);
  sin4lat = cassCoeffTimesSine(c2, 4.0, latitude);
  sin6lat = cassCoeffTimesSine(c3, 6.0, latitude);
  MM = cassM( lat, sin2lat, sin4lat, sin6lat );

  double easting = NN * (AA - (TT * A3 / 6.0) - (8.0 - TT + 8.0 * CC) *
                   (TT * A5 / 120.0)) + Cass_False_Easting;
  double northing = MM - M0 + NN * tlat * ((A2 / 2.0) + (5.0 - TT +
                    6.0 * CC) * A4 / 24.0) + Cass_False_Northing;

  return new MapProjectionCoordinates(
     CoordinateType::cassini, warning, easting, northing );
}


MSP::CCS::GeodeticCoordinates* Cassini::convertToGeodetic(
   MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Cassini projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid and Cassini projection
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
  double mu1;
  double sin2mu, sin4mu, sin6mu, sin8mu;
  double M1;
  double phi1;
  double tanphi1, sinphi1, cosphi1;
  double T1, T;
  double N1;
  double RD, R1;
  double DD, D2, D3, D4, D5;
  const double epsilon = 1.0e-1;
  double longitude, latitude;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Cass_False_Easting + Cass_Min_Easting))
      || (easting > (Cass_False_Easting + Cass_Max_Easting)))
  { /* Easting out of range */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < (Cass_False_Northing + Cass_Min_Northing - epsilon))
      || (northing > (Cass_False_Northing + Cass_Max_Northing + epsilon)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy = northing - Cass_False_Northing;
  dx = easting - Cass_False_Easting;
  M1 = M0 + dy;
  mu1 = M1 / (semiMajorAxis * c0);

  sin2mu = cassCoeffTimesSine(a0, 2.0, mu1);
  sin4mu = cassCoeffTimesSine(a1, 4.0, mu1);
  sin6mu = cassCoeffTimesSine(a2, 6.0, mu1);
  sin8mu = cassCoeffTimesSine(a3, 8.0, mu1);
  phi1 = mu1 + sin2mu + sin4mu + sin6mu + sin8mu;

  if (floatEq(phi1,PI_OVER_2,.00001))
  {
    latitude =  PI_OVER_2;
    longitude = Cass_Origin_Long;
  }
  else if (floatEq(phi1,-PI_OVER_2,.00001))
  {
    latitude = -PI_OVER_2;
    longitude = Cass_Origin_Long;
  }
  else
  {
    tanphi1 = tan(phi1);
    sinphi1 = sin(phi1);
    cosphi1 = cos(phi1);
    T1 = tanphi1 * tanphi1;
    RD = cassRd( sinphi1 );
    N1 = semiMajorAxis / RD;
    R1 = N1 * One_Minus_es2 / (RD * RD);
    DD = dx / N1;
    D2 = DD * DD;
    D3 = D2 * DD;
    D4 = D3 * DD;
    D5 = D4 * DD;
    T = (1.0 + 3.0 * T1);
    latitude = phi1 - (N1 * tanphi1 / R1) * (D2 / 2.0 - T * D4 / 24.0);

    longitude = Cass_Origin_Long + (DD - T1 * D3 / 3.0 + T * T1 * D5 / 15.0) / cosphi1;

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
  }

  char warning[100];
  warning[0] = '\0';
  if (fabs(longitude - Cass_Origin_Long) > (4.0 * PI / 180))
  { /* Distortion results if Longitude is > 4 degr from the Central Meridian */
    strcat( warning, MSP::CCS::WarningMessages::longitude );
  }

  return new GeodeticCoordinates(
     CoordinateType::geodetic, warning, longitude, latitude );
}


double Cassini::cassM(
   double c0lat, double c1s2lat, double c2s4lat, double c3s6lat )
{
   return semiMajorAxis*(c0lat-c1s2lat+c2s4lat-c3s6lat);
}


double Cassini::cassRd( double sinlat )
{
  return sqrt(1.0 - es2 * (sinlat * sinlat));
}


double Cassini::floatEq( double x, double v, double epsilon )
{
  return ((v - epsilon) < x) && (x < (v + epsilon));
}



// CLASSIFICATION: UNCLASSIFIED
