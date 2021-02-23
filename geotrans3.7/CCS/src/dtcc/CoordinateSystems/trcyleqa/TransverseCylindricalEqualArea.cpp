// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: TRANSVERSE CYLINDRICAL EQUAL AREA
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Transverse Cylindrical Equal Area
 *    projection coordinates (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          TCEA_NO_ERROR           : No errors occurred in function
 *          TCEA_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          TCEA_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          TCEA_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~6,500,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          TCEA_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~20,000,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          TCEA_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          TCEA_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          TCEA_A_ERROR            : Semi-major axis less than or equal to zero
 *          TCEA_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                      (250 to 350)
 *          TCEA_SCALE_FACTOR_ERROR : Scale factor outside of valid
 *                                      range (0.3 to 3.0)
 *          TCEA_LON_WARNING       : Distortion will result if longitude is more
 *                                    than 90 degrees from the Central Meridian
 *
 * REUSE NOTES
 *
 *    TRANSVERSE CYLINDRICAL EQUAL AREA is intended for reuse by any application that
 *    performs a Transverse Cylindrical Equal Area projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on TRANSVERSE CYLINDRICAL EQUAL AREA can be found in the Reuse Manual.
 *
 *    TRANSVERSE CYLINDRICAL EQUAL AREA originated from :
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
 *    TRANSVERSE CYLINDRICAL EQUAL AREA has no restrictions.
 *
 * ENVIRONMENT
 *
 *    TRANSVERSE CYLINDRICAL EQUAL AREA was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    3-1-07            Original C++ Code
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "TransverseCylindricalEqualArea.h"
#include "MapProjection5Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"
#include "WarningMessages.h"

/*
 *    math.h     - Standard C math library
 *    TransverseCylindricalEqualArea.h - Is for prototype error checking
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
const double MIN_SCALE_FACTOR = 0.3;
const double MAX_SCALE_FACTOR = 3.0;


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

TransverseCylindricalEqualArea::TransverseCylindricalEqualArea(
   double ellipsoidSemiMajorAxis,
   double ellipsoidFlattening,
   double centralMeridian,
   double latitudeOfTrueScale,
   double falseEasting,
   double falseNorthing,
   double scaleFactor ) :
  CoordinateSystem(),
  es2( 0.0066943799901413800 ),      
  es4( 4.4814723452405e-005 ),        
  es6( 3.0000678794350e-007 ),        
  es( .081819190842622 ),             
  M0( 0.0 ),
  qp( 1.9955310875028 ),
  One_MINUS_es2( .99330562000986 ),  
  One_OVER_2es( 6.1110357466348 ),    
  a0( .0022392088624809 ),     
  a1( 2.8830839728915e-006 ),         
  a2( 5.0331826636906e-009 ),         
  b0( .0025188265843907 ),            
  b1( 3.7009490356205e-006 ),        
  b2( 7.4478137675038e-009 ),         
  b3( 1.7035993238596e-011 ),         
  c0( .99832429845280 ),              
  c1( .0025146070605187 ),            
  c2( 2.6390465943377e-006 ),         
  c3( 3.4180460865959e-009 ),         
  Tcea_Origin_Long( 0.0 ),            
  Tcea_Origin_Lat( 0.0 ),             
  Tcea_False_Easting( 0.0 ),          
  Tcea_False_Northing( 0.0 ),         
  Tcea_Scale_Factor( 1.0 ),           
  Tcea_Min_Easting( -6398628.0 ),
  Tcea_Max_Easting( 6398628.0 ),
  Tcea_Min_Northing( -20003931.0 ),
  Tcea_Max_Northing( 20003931.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * Transverse Cylindrical Equal Area projection parameters as inputs,
 * and sets the corresponding state variables.
 * If any errors occur, an exception is thrown with a description of the error.
 *
 *    ellipsoidSemiMajorAxis : Semi-major axis of ellipsoid, in meters   (input)
 *    ellipsoidFlattening    : Flattening of ellipsoid                   (input)
 *    centralMeridian        : Longitude in radians at the center of     (input)
 *                             the projection
 *    latitudeOfTrueScale    : Latitude in radians at which the          (input)
 *                             point scale factor is 1.0
 *    falseEasting           : A coordinate value in meters assigned to the
 *                             central meridian of the projection.       (input)
 *    falseNorthing          : A coordinate value in meters assigned to the
 *                             origin latitude of the projection         (input)
 *    scaleFactor            : Multiplier which reduces distances in the
 *                             projection to the actual distance on the
 *                             ellipsoid                                 (input)
 */

  double sin_lat_90 = sin(PI_OVER_2);
  double x, j, three_es4;
  double Sqrt_One_MINUS_es2;
  double e1, e2, e3, e4;
  double lat, sin2lat, sin4lat, sin6lat;
  double temp, temp_northing;
  double inv_f = 1 / ellipsoidFlattening;

  if (ellipsoidSemiMajorAxis <= 0.0)
  { /* Semi-major axis must be greater than zero */
    throw CoordinateConversionException( ErrorMessages::semiMajorAxis  );
  }
  if ((inv_f < 250) || (inv_f > 350))
  { /* Inverse flattening must be between 250 and 350 */
    throw CoordinateConversionException( ErrorMessages::ellipsoidFlattening  );
  }
  if ((latitudeOfTrueScale < -PI_OVER_2) || (latitudeOfTrueScale > PI_OVER_2))
  { /* origin latitude out of range */
    throw CoordinateConversionException( ErrorMessages::originLatitude  );
  }
  if ((centralMeridian < -PI) || (centralMeridian > TWO_PI))
  { /* origin longitude out of range */
    throw CoordinateConversionException( ErrorMessages::centralMeridian  );
  }
  if ((scaleFactor < MIN_SCALE_FACTOR) || (scaleFactor > MAX_SCALE_FACTOR))
  {
    throw CoordinateConversionException( ErrorMessages::scaleFactor  );
  }

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening    = ellipsoidFlattening;

  Tcea_Origin_Lat = latitudeOfTrueScale;
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Tcea_Origin_Long = centralMeridian;
  Tcea_False_Northing = falseNorthing;
  Tcea_False_Easting = falseEasting;
  Tcea_Scale_Factor = scaleFactor;

  es2 = 2 * flattening - flattening * flattening;
  es = sqrt(es2);
  One_MINUS_es2 = 1.0 - es2;
  Sqrt_One_MINUS_es2 = sqrt(One_MINUS_es2);
  One_OVER_2es = 1.0 / (2.0 * es);
  es4 = es2 * es2;
  es6 = es4 * es2;
  x = es * sin_lat_90;
  qp = TCEA_Q(sin_lat_90,x);

  a0 = es2 / 3.0 + 31.0 * es4 / 180.0 + 517.0 * es6 / 5040.0;
  a1 = 23.0 * es4 / 360.0 + 251.0 * es6 / 3780.0;
  a2 = 761.0 * es6 / 45360.0;

  e1 = (1.0 - Sqrt_One_MINUS_es2) / (1.0 + Sqrt_One_MINUS_es2);
  e2 = e1 * e1;
  e3 = e2 * e1;
  e4 = e3 * e1;
  b0 = 3.0 * e1 / 2.0 - 27.0 * e3 / 32.0;
  b1 = 21.0 * e2 / 16.0 - 55.0 * e4 / 32.0;
  b2 = 151.0 * e3 / 96.0;
  b3 = 1097.0 * e4 / 512.0;

  j = 45.0 * es6 / 1024.0;
  three_es4 = 3.0 * es4;
  c0 = 1.0 - es2 / 4.0 - three_es4 / 64.0 - 5.0 * es6 / 256.0;
  c1 = 3.0 * es2 / 8.0 + three_es4 / 32.0 + j;
  c2 = 15.0 * es4 / 256.0 + j;
  c3 = 35.0 * es6 / 3072.0;
  lat = c0 * Tcea_Origin_Lat;
  sin2lat = TCEA_COEFF_TIMES_SIN(c1, 2.0, Tcea_Origin_Lat);
  sin4lat = TCEA_COEFF_TIMES_SIN(c2, 4.0, Tcea_Origin_Lat);
  sin6lat = TCEA_COEFF_TIMES_SIN(c3, 6.0, Tcea_Origin_Lat);
  M0 = TCEA_M(lat, sin2lat, sin4lat, sin6lat);

  GeodeticCoordinates gcTemp( CoordinateType::geodetic, PI, PI_OVER_2 );
  MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcTemp );
  temp = tempCoordinates->easting();
  temp_northing = tempCoordinates->northing();
  delete tempCoordinates;

  if (temp_northing > 0)
  {
    Tcea_Min_Northing = temp_northing - 20003931.458986;
    Tcea_Max_Northing = temp_northing;

    if(Tcea_False_Northing)
    {
      Tcea_Min_Northing -= Tcea_False_Northing;
      Tcea_Max_Northing -= Tcea_False_Northing;
    }
  }
  else if (temp_northing < 0)
  {
    Tcea_Max_Northing = temp_northing + 20003931.458986;
    Tcea_Min_Northing = temp_northing;

    if(Tcea_False_Northing)
    {
      Tcea_Min_Northing -= Tcea_False_Northing;
      Tcea_Max_Northing -= Tcea_False_Northing;
    }
  }
}


TransverseCylindricalEqualArea::TransverseCylindricalEqualArea(
   const TransverseCylindricalEqualArea &tcea )
{
  semiMajorAxis = tcea.semiMajorAxis;
  flattening = tcea.flattening;
  es2 = tcea.es2;     
  es4 = tcea.es4; 
  es6 = tcea.es6; 
  es = tcea.es; 
  M0 = tcea.M0; 
  qp = tcea.qp; 
  One_MINUS_es2 = tcea.One_MINUS_es2; 
  One_OVER_2es = tcea.One_OVER_2es; 
  a0 = tcea.a0; 
  a1 = tcea.a1; 
  a2 = tcea.a2; 
  b0 = tcea.b0; 
  b1 = tcea.b1; 
  b2 = tcea.b2; 
  b3 = tcea.b3; 
  c0 = tcea.c0; 
  c1 = tcea.c1; 
  c2 = tcea.c2; 
  c3 = tcea.c3; 
  Tcea_Origin_Long = tcea.Tcea_Origin_Long; 
  Tcea_Origin_Lat = tcea.Tcea_Origin_Lat; 
  Tcea_False_Easting = tcea.Tcea_False_Easting; 
  Tcea_False_Northing = tcea.Tcea_False_Northing; 
  Tcea_Scale_Factor = tcea.Tcea_Scale_Factor; 
  Tcea_Min_Easting = tcea.Tcea_Min_Easting; 
  Tcea_Max_Easting = tcea.Tcea_Max_Easting; 
  Tcea_Min_Northing = tcea.Tcea_Min_Northing; 
  Tcea_Max_Northing = tcea.Tcea_Max_Northing; 
}


TransverseCylindricalEqualArea::~TransverseCylindricalEqualArea()
{
}


TransverseCylindricalEqualArea& TransverseCylindricalEqualArea::operator=( const TransverseCylindricalEqualArea &tcea )
{
  if( this != &tcea )
  {
    semiMajorAxis = tcea.semiMajorAxis;
    flattening = tcea.flattening;
    es2 = tcea.es2;     
    es4 = tcea.es4; 
    es6 = tcea.es6; 
    es = tcea.es; 
    M0 = tcea.M0; 
    qp = tcea.qp; 
    One_MINUS_es2 = tcea.One_MINUS_es2; 
    One_OVER_2es = tcea.One_OVER_2es; 
    a0 = tcea.a0; 
    a1 = tcea.a1; 
    a2 = tcea.a2; 
    b0 = tcea.b0; 
    b1 = tcea.b1; 
    b2 = tcea.b2; 
    b3 = tcea.b3; 
    c0 = tcea.c0; 
    c1 = tcea.c1; 
    c2 = tcea.c2; 
    c3 = tcea.c3; 
    Tcea_Origin_Long = tcea.Tcea_Origin_Long; 
    Tcea_Origin_Lat = tcea.Tcea_Origin_Lat; 
    Tcea_False_Easting = tcea.Tcea_False_Easting; 
    Tcea_False_Northing = tcea.Tcea_False_Northing; 
    Tcea_Scale_Factor = tcea.Tcea_Scale_Factor; 
    Tcea_Min_Easting = tcea.Tcea_Min_Easting; 
    Tcea_Max_Easting = tcea.Tcea_Max_Easting; 
    Tcea_Min_Northing = tcea.Tcea_Min_Northing; 
    Tcea_Max_Northing = tcea.Tcea_Max_Northing; 
  }

  return *this;
}


MapProjection5Parameters* TransverseCylindricalEqualArea::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters, Transverse Cylindrical Equal Area projection parameters, and scale factor.
 *
 *  ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
 *  ellipsoidFlattening     : Flattening of ellipsoid                   (output)
 *  centralMeridian         : Longitude in radians at the center of     (output)
 *                            the projection
 *  latitudeOfTrueScale     : Latitude in radians at which the          (output)
 *                            point scale factor is 1.0
 *  falseEasting            : A coordinate value in meters assigned to the
 *                            central meridian of the projection.       (output)
 *  falseNorthing           : A coordinate value in meters assigned to the
 *                            origin latitude of the projection         (output)
 *  scaleFactor             : Multiplier which reduces distances in the
 *                            projection to the actual distance on the
 *                            ellipsoid                                 (output)
 */

  return new MapProjection5Parameters( CoordinateType::transverseCylindricalEqualArea, Tcea_Origin_Long, Tcea_Origin_Lat, Tcea_Scale_Factor, Tcea_False_Easting, Tcea_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* TransverseCylindricalEqualArea::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Transverse Cylindrical Equal Area projection (easting and
 * northing) coordinates, according to the current ellipsoid and Transverse Cylindrical
 * Equal Area projection parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda) in radians       (input)
 *    latitude          : Latitude (phi) in radians           (input)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  double x;
  double dlam;                      /* Longitude - Central Meridan */
  double qq, qq_OVER_qp;
  double beta, betac;
  double sin2betac, sin4betac, sin6betac;
  double PHIc;
  double phi, sin2phi, sin4phi, sin6phi;
  double sinPHIc;
  double Mc;

  double longitude = geodeticCoordinates->longitude();
  double latitude  = geodeticCoordinates->latitude();
  double sin_lat   = sin(latitude);

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  { /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  { /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  dlam = longitude - Tcea_Origin_Long;

  char warning[100];
  warning[0] = '\0';
  if (fabs(dlam) >= (PI / 2))
  { /* Distortion resultsif Longitude is > 90 deg from the Central Meridian */
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
  if (latitude == PI_OVER_2)
  {
    qq = qp;
    qq_OVER_qp = 1.0;
  }
  else
  {
    x = es * sin_lat;
    qq = TCEA_Q(sin_lat, x);
    qq_OVER_qp = qq / qp;
  }


  if (qq_OVER_qp > 1.0)
    qq_OVER_qp = 1.0;
  else if (qq_OVER_qp < -1.0)
    qq_OVER_qp = -1.0;

  beta = asin(qq_OVER_qp);
  betac = atan(tan(beta) / cos(dlam));

  if ((fabs(betac) - PI_OVER_2) > 1.0e-8)
    PHIc = betac;
  else
  {
    sin2betac = TCEA_COEFF_TIMES_SIN(a0, 2.0, betac);
    sin4betac = TCEA_COEFF_TIMES_SIN(a1, 4.0, betac);
    sin6betac = TCEA_COEFF_TIMES_SIN(a2, 6.0, betac);
    PHIc = TCEA_L(betac, sin2betac, sin4betac, sin6betac);
  }

  sinPHIc = sin(PHIc);
  double easting = semiMajorAxis * cos(beta) * cos(PHIc) * sin(dlam) /
     (Tcea_Scale_Factor * cos(betac) * sqrt(1.0 - es2 *
        sinPHIc * sinPHIc)) + Tcea_False_Easting;

  phi = c0 * PHIc;
  sin2phi = TCEA_COEFF_TIMES_SIN(c1, 2.0, PHIc);
  sin4phi = TCEA_COEFF_TIMES_SIN(c2, 4.0, PHIc);
  sin6phi = TCEA_COEFF_TIMES_SIN(c3, 6.0, PHIc);
  Mc = TCEA_M(phi, sin2phi, sin4phi, sin6phi);

  double northing = Tcea_Scale_Factor * (Mc - M0) + Tcea_False_Northing;

  return new MapProjectionCoordinates(
     CoordinateType::transverseCylindricalEqualArea,
     warning, easting, northing );
}


MSP::CCS::GeodeticCoordinates* TransverseCylindricalEqualArea::convertToGeodetic(
   MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Transverse
 * Cylindrical Equal Area projection (easting and northing) coordinates
 * to geodetic (latitude and longitude) coordinates, according to the
 * current ellipsoid and Transverse Cylindrical Equal Area projection
 * coordinates.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    easting           : Easting (X) in meters                  (input)
 *    northing          : Northing (Y) in meters                 (input)
 *    longitude         : Longitude (lambda) in radians          (output)
 *    latitude          : Latitude (phi) in radians              (output)
 */

  double x;
  double dx;     /* Delta easting - Difference in easting (easting-FE)      */
  double dy;     /* Delta northing - Difference in northing (northing-FN)   */
  double Mc;
  double MUc;
  double sin2mu, sin4mu, sin6mu, sin8mu;
  double PHIc;
  double Qc;
  double sin_lat;
  double beta, betac, beta_prime;
  double sin2beta, sin4beta, sin6beta;
  double cosbetac;
  double Qc_OVER_qp;
  double temp;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Tcea_False_Easting + Tcea_Min_Easting))
      || (easting > (Tcea_False_Easting + Tcea_Max_Easting)))
  { /* Easting out of range */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if(   (northing < (Tcea_False_Northing + Tcea_Min_Northing))
     || (northing > (Tcea_False_Northing + Tcea_Max_Northing)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy = northing - Tcea_False_Northing;
  dx = easting - Tcea_False_Easting;
  Mc = M0 + dy / Tcea_Scale_Factor;
  MUc = Mc / (semiMajorAxis * c0);

  sin2mu = TCEA_COEFF_TIMES_SIN(b0, 2.0, MUc);
  sin4mu = TCEA_COEFF_TIMES_SIN(b1, 4.0, MUc);
  sin6mu = TCEA_COEFF_TIMES_SIN(b2, 6.0, MUc);
  sin8mu = TCEA_COEFF_TIMES_SIN(b3, 8.0, MUc);
  PHIc = MUc + sin2mu + sin4mu + sin6mu + sin8mu;

  sin_lat = sin(PHIc);
  x = es * sin_lat;
  Qc = TCEA_Q(sin_lat, x);
  Qc_OVER_qp = Qc / qp;

  if (Qc_OVER_qp < -1.0)
    Qc_OVER_qp = -1.0;
  else if (Qc_OVER_qp > 1.0)
    Qc_OVER_qp = 1.0;

  betac = asin(Qc_OVER_qp);
  cosbetac = cos(betac);
  temp = Tcea_Scale_Factor * dx * cosbetac * sqrt(1.0 -
     es2 * sin_lat * sin_lat) / (semiMajorAxis * cos(PHIc));
  if (temp > 1.0)
    temp = 1.0;
  else if (temp < -1.0)
    temp = -1.0;
  beta_prime = -asin(temp);
  beta = asin(cos(beta_prime) * sin(betac));

  sin2beta = TCEA_COEFF_TIMES_SIN(a0, 2.0, beta);
  sin4beta = TCEA_COEFF_TIMES_SIN(a1, 4.0, beta);
  sin6beta = TCEA_COEFF_TIMES_SIN(a2, 6.0, beta);
  double latitude = TCEA_L(beta, sin2beta, sin4beta, sin6beta);

  double longitude = Tcea_Origin_Long - atan(tan(beta_prime) / cosbetac);

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

  return new GeodeticCoordinates(
     CoordinateType::geodetic, longitude, latitude );
}


double TransverseCylindricalEqualArea::TCEA_Q( double sinlat, double x )
{
  return One_MINUS_es2*(sinlat/(1.0-es2*sinlat*sinlat)-One_OVER_2es*log((1-x)/(1+x)));
}


double TransverseCylindricalEqualArea::TCEA_COEFF_TIMES_SIN(
   double coeff, double x, double latit )         
{
  return coeff * sin(x*latit);
}


double TransverseCylindricalEqualArea::TCEA_M(
   double c0lat, double c1lat, double c2lat, double c3lat )             
{
  return semiMajorAxis * (c0lat - c1lat + c2lat - c3lat);
}


double TransverseCylindricalEqualArea::TCEA_L(
   double Beta, double c0lat, double c1lat, double c2lat )              
{
  return Beta + c0lat + c1lat + c2lat;
}

// CLASSIFICATION: UNCLASSIFIED
