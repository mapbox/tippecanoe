// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: CYLINDRICAL EQUAL AREA
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Cylindrical Equal Area projection
 *    coordinates (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          CYEQ_NO_ERROR           : No errors occurred in function
 *          CYEQ_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          CYEQ_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          CYEQ_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~20,000,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          CYEQ_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~6,000,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          CYEQ_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          CYEQ_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          CYEQ_A_ERROR            : Semi-major axis less than or equal to zero
 *          CYEQ_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                      (250 to 350)
 *
 * REUSE NOTES
 *
 *    CYLINDRICAL EQUAL AREA is intended for reuse by any application that performs a
 *    Cylindrical Equal Area projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on CYLINDRICAL EQUAL AREA can be found in the Reuse Manual.
 *
 *    CYLINDRICAL EQUAL AREA originated from :
 *                                U.S. Army Topographic Engineering Center
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
 *    CYLINDRICAL EQUAL AREA has no restrictions.
 *
 * ENVIRONMENT
 *
 *    CYLINDRICAL EQUAL AREA was tested and certified in the following environments:
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
#include "CylindricalEqualArea.h"
#include "MapProjection4Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h   - Standard C math library
 *    CylindricalEqualArea.h - Is for prototype error checking
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
const double TWO_PI = (2.0 * PI);
const double ONE = (1.0 * PI / 180);      /* 1 degree in radians           */


double cyeqCoeffTimesSine( double coeff, double c, double Beta )
{
  return coeff * sin(c * Beta);
}


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

CylindricalEqualArea::CylindricalEqualArea( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
  es( .081819190842622 ),
  es2( 0.0066943799901413800 ),
  es4( 4.4814723452405e-005 ),
  es6( 3.0000678794350e-007 ),
  k0( 1.0 ),
  Cyeq_a_k0( 6378137.0 ),
  two_k0( 2.0 ),
  c0( .0022392088624809 ),
  c1( 2.8830839728915e-006 ),
  c2( 5.0331826636906e-009 ),
  Cyeq_Origin_Long( 0.0 ),
  Cyeq_Origin_Lat( 0.0 ),
  Cyeq_False_Easting( 0.0 ),
  Cyeq_False_Northing( 0.0 ),
  Cyeq_Max_Easting( 20037509.0 ),
  Cyeq_Min_Easting( -20037509.0 ),
  Cyeq_Delta_Northing( 6363886.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * Cylindrical Equal Area projcetion parameters as inputs, and sets the corresponding
 * state variables.  If any errors occur, an exception is thrown with a description 
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

  double Sin_Cyeq_Origin_Lat;
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
  flattening = ellipsoidFlattening;

  Cyeq_Origin_Lat = originLatitude;
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Cyeq_Origin_Long = centralMeridian;
  Cyeq_False_Northing = falseNorthing;
  Cyeq_False_Easting = falseEasting;
  es2 = 2 * flattening - flattening * flattening;
  es4 = es2 * es2;
  es6 = es4 * es2;
  es = sqrt(es2);
  c0 = es2 / 3.0 + 31.0 * es4 / 180.0 + 517.0 * es6 / 5040.0;
  c1 = 23.0 * es4 / 360.0 + 251.0 * es6 / 3780.0;
  c2 = 761.0 * es6 / 45360.0;
  Sin_Cyeq_Origin_Lat = sin(Cyeq_Origin_Lat);
  k0 = cos(Cyeq_Origin_Lat) / sqrt(1.0 - es2 * Sin_Cyeq_Origin_Lat * Sin_Cyeq_Origin_Lat);
  Cyeq_a_k0 = semiMajorAxis * k0;
  two_k0 = 2.0 * k0;

  if (Cyeq_Origin_Long > 0)
  {
    GeodeticCoordinates gcMax( CoordinateType::geodetic, Cyeq_Origin_Long - PI - ONE, PI_OVER_2 );
    MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcMax );
    Cyeq_Max_Easting = tempCoordinates->easting();
    delete tempCoordinates;

    GeodeticCoordinates gcMin( CoordinateType::geodetic, Cyeq_Origin_Long - PI, PI_OVER_2 );
    tempCoordinates = convertFromGeodetic( &gcMin );
    Cyeq_Min_Easting = tempCoordinates->easting();
    delete tempCoordinates;

    GeodeticCoordinates gcDelta( CoordinateType::geodetic, PI, PI_OVER_2 );
    tempCoordinates = convertFromGeodetic( &gcDelta );
    Cyeq_Delta_Northing = tempCoordinates->northing();
    delete tempCoordinates;
  }
  else if (Cyeq_Origin_Long < 0)
  {
    GeodeticCoordinates gcMax( CoordinateType::geodetic, Cyeq_Origin_Long + PI, PI_OVER_2 );
    MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcMax );
    Cyeq_Max_Easting = tempCoordinates->easting();
    delete tempCoordinates;

    GeodeticCoordinates gcMin( CoordinateType::geodetic, Cyeq_Origin_Long + PI + ONE, PI_OVER_2 );
    tempCoordinates = convertFromGeodetic( &gcMin );
    Cyeq_Min_Easting = tempCoordinates->easting();
    delete tempCoordinates;

    GeodeticCoordinates gcDelta( CoordinateType::geodetic, PI, PI_OVER_2 );
    tempCoordinates = convertFromGeodetic( &gcDelta );
    Cyeq_Delta_Northing = tempCoordinates->northing();
    delete tempCoordinates;
  }
  else
  {
    GeodeticCoordinates gcTemp( CoordinateType::geodetic, PI, PI_OVER_2 );
    MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcTemp );
    Cyeq_Max_Easting = tempCoordinates->easting();
    Cyeq_Delta_Northing = tempCoordinates->northing();
    delete tempCoordinates;
    Cyeq_Min_Easting = -Cyeq_Max_Easting;
  }

  if(Cyeq_False_Northing)
    Cyeq_Delta_Northing -= Cyeq_False_Northing;
  if (Cyeq_Delta_Northing < 0)
    Cyeq_Delta_Northing = -Cyeq_Delta_Northing;

  if(Cyeq_False_Easting)
  {
    Cyeq_Min_Easting -= Cyeq_False_Easting;
    Cyeq_Max_Easting -= Cyeq_False_Easting;
  }
}


CylindricalEqualArea::CylindricalEqualArea( const CylindricalEqualArea &cea )
{
  semiMajorAxis = cea.semiMajorAxis;
  flattening = cea.flattening;
  es = cea.es;
  es2 = cea.es2;
  es4 = cea.es4;
  es6 = cea.es6;
  k0 = cea.k0;
  Cyeq_a_k0 = cea.Cyeq_a_k0;
  two_k0 = cea.two_k0;
  c0 = cea.c0;
  c1 = cea.c1;
  c2 = cea.c2;
  Cyeq_Origin_Long = cea.Cyeq_Origin_Long;
  Cyeq_Origin_Lat = cea.Cyeq_Origin_Lat;
  Cyeq_False_Easting = cea.Cyeq_False_Easting;
  Cyeq_False_Northing = cea.Cyeq_False_Northing;
  Cyeq_Max_Easting = cea.Cyeq_Max_Easting;
  Cyeq_Min_Easting = cea.Cyeq_Min_Easting;
  Cyeq_Delta_Northing = cea.Cyeq_Delta_Northing;
}


CylindricalEqualArea::~CylindricalEqualArea()
{
}


CylindricalEqualArea& CylindricalEqualArea::operator=( const CylindricalEqualArea &cea )
{
  if( this != &cea )
  {
    semiMajorAxis = cea.semiMajorAxis;
    flattening = cea.flattening;
    es = cea.es;
    es2 = cea.es2;
    es4 = cea.es4;
    es6 = cea.es6;
    k0 = cea.k0;
    Cyeq_a_k0 = cea.Cyeq_a_k0;
    two_k0 = cea.two_k0;
    c0 = cea.c0;
    c1 = cea.c1;
    c2 = cea.c2;
    Cyeq_Origin_Long = cea.Cyeq_Origin_Long;
    Cyeq_Origin_Lat = cea.Cyeq_Origin_Lat;
    Cyeq_False_Easting = cea.Cyeq_False_Easting;
    Cyeq_False_Northing = cea.Cyeq_False_Northing;
    Cyeq_Max_Easting = cea.Cyeq_Max_Easting;
    Cyeq_Min_Easting = cea.Cyeq_Min_Easting;
    Cyeq_Delta_Northing = cea.Cyeq_Delta_Northing;
  }

  return *this;
}


MapProjection4Parameters* CylindricalEqualArea::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters, and Cylindrical Equal Area projection parameters.
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

  return new MapProjection4Parameters( CoordinateType::cylindricalEqualArea, Cyeq_Origin_Long, Cyeq_Origin_Lat, Cyeq_False_Easting, Cyeq_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* CylindricalEqualArea::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Cylindrical Equal Area projection (easting and northing)
 * coordinates, according to the current ellipsoid and Cylindrical Equal Area projection
 * parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda) in radians       (input)
 *    latitude          : Latitude (phi) in radians           (input)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  double dlam;                      /* Longitude - Central Meridan */
  double qq;
  double x;

  double longitude = geodeticCoordinates->longitude();
  double latitude = geodeticCoordinates->latitude();
  double sin_lat = sin(latitude);

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  { /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  { /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  dlam = longitude - Cyeq_Origin_Long;
  if (dlam > PI)
  {
    dlam -= TWO_PI;
  }
  if (dlam < -PI)
  {
    dlam += TWO_PI;
  }
  x = es * sin_lat;
  qq = cyleqarQ( sin_lat, x );

  double easting = Cyeq_a_k0 * dlam + Cyeq_False_Easting;
  double northing = semiMajorAxis * qq / two_k0 + Cyeq_False_Northing;

  return new MapProjectionCoordinates( CoordinateType::cylindricalEqualArea, easting, northing );
}


MSP::CCS::GeodeticCoordinates* CylindricalEqualArea::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts
 * Cylindrical Equal Area projection (easting and northing) coordinates
 * to geodetic (latitude and longitude) coordinates, according to the
 * current ellipsoid and Cylindrical Equal Area projection
 * coordinates.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    easting           : Easting (X) in meters                  (input)
 *    northing          : Northing (Y) in meters                 (input)
 *    longitude         : Longitude (lambda) in radians          (output)
 *    latitude          : Latitude (phi) in radians              (output)
 */

  double sin2beta, sin4beta, sin6beta;
  double dx;     /* Delta easting - Difference in easting (easting-FE)      */
  double dy;     /* Delta northing - Difference in northing (northing-FN)   */
  double qp;
  double beta;
  double sin_lat = sin(PI_OVER_2);
  double i;
  double x;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Cyeq_False_Easting + Cyeq_Min_Easting))
      || (easting > (Cyeq_False_Easting + Cyeq_Max_Easting)))
  { /* Easting out of range */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < (Cyeq_False_Northing - fabs(Cyeq_Delta_Northing)))
      || (northing > (Cyeq_False_Northing + fabs(Cyeq_Delta_Northing))))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy = northing - Cyeq_False_Northing;
  dx = easting - Cyeq_False_Easting;
  x  = es * sin_lat;
  qp = cyleqarQ( sin_lat, x );
  i  = two_k0 * dy / (semiMajorAxis * qp);
  if (i > 1.0)
    i = 1.0;
  else if (i < -1.0)
    i = -1.0;
  beta = asin(i);
  sin2beta = cyeqCoeffTimesSine(c0, 2.0, beta);
  sin4beta = cyeqCoeffTimesSine(c1, 4.0, beta);
  sin6beta = cyeqCoeffTimesSine(c2, 6.0, beta);

  double latitude = beta + sin2beta + sin4beta + sin6beta;
  double longitude = Cyeq_Origin_Long + dx / Cyeq_a_k0;

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


double CylindricalEqualArea::cyleqarQ( double slat, double x )
{
  return (1.0-es2)*(slat/(1.0-x*x)-(1.0/(2.0*es)) * log((1.0-x)/(1.0+x)));
}



// CLASSIFICATION: UNCLASSIFIED
