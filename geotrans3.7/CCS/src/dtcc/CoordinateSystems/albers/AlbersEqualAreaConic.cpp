// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: ALBERS
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Albers Equal Area Conic
 *    projection coordinates (easting and northing in meters) defined
 *    by two standard parallels.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       ALBERS_NO_ERROR           : No errors occurred in function
 *       ALBERS_LAT_ERROR          : Latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *       ALBERS_LON_ERROR          : Longitude outside of valid range
 *                                     (-180 to 360 degrees)
 *       ALBERS_EASTING_ERROR      : Easting outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       ALBERS_NORTHING_ERROR     : Northing outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       ALBERS_FIRST_STDP_ERROR   : First standard parallel outside of valid
 *                                     range (-90 to 90 degrees)
 *       ALBERS_SECOND_STDP_ERROR  : Second standard parallel outside of valid
 *                                     range (-90 to 90 degrees)
 *       ALBERS_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *       ALBERS_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                     (-180 to 360 degrees)
 *       ALBERS_A_ERROR            : Semi-major axis less than or equal to zero
 *       ALBERS_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                     (250 to 350)
 *       ALBERS_HEMISPHERE_ERROR   : Standard parallels cannot be opposite
 *                                     latitudes
 *       ALBERS_FIRST_SECOND_ERROR : The 1st & 2nd standard parallels cannot
 *                                   both be 0
 *
 *
 * REUSE NOTES
 *
 *    ALBERS is intended for reuse by any application that performs an Albers
 *    Equal Area Conic projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on ALBERS can be found in the Reuse Manual.
 *
 *    ALBERS originated from:     U.S. Army Topographic Engineering Center
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
 *    ALBERS has no restrictions.
 *
 * ENVIRONMENT
 *
 *    ALBERS was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. MSDOS with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    07-09-99          Original Code
 *    03-08-07          Original C++ Code
 *    
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "AlbersEqualAreaConic.h"
#include "MapProjection6Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h     - Standard C math library
 *    AlbersEqualAreaConic.h   - Is for prototype error checking
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

                 
double oneMinusSqr( double x )
{
  return 1.0 - x * x;
}


double albersM( double clat, double oneminussqressin )
{
  return clat / sqrt(oneminussqressin);
}


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

AlbersEqualAreaConic::AlbersEqualAreaConic(
   double ellipsoidSemiMajorAxis,
   double ellipsoidFlattening,
   double centralMeridian,
   double originLatitude,
   double standardParallel1,
   double standardParallel2,
   double falseEasting,
   double falseNorthing ) :
  CoordinateSystem(),
  es( 0.08181919084262188000 ),
  es2( 0.0066943799901413800 ),
  C( 1.4896626908850 ),
  rho0( 6388749.3391064 ),
  n( .70443998701755 ),
  Albers_a_OVER_n( 9054194.9882824 ),
  one_MINUS_es2( .99330562000986 ),
  two_es( .16363838168524 ),
  Albers_Origin_Long( 0.0 ),
  Albers_Origin_Lat( (45 * PI / 180) ),
  Albers_Std_Parallel_1( 40 * PI / 180 ),
  Albers_Std_Parallel_2( 50 * PI / 180 ),
  Albers_False_Easting( 0.0 ),
  Albers_False_Northing( 0.0 ),
  Albers_Delta_Northing( 40000000 ),
  Albers_Delta_Easting( 40000000 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * projection parameters as inputs, and sets the corresponding state
 * variables.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *   ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
 *   ellipsoidFlattening     : Flattening of ellipsoid                   (input)
 *   centralMeridian         : Longitude in radians at the center of     (input)
 *                              the projection
 *   originLatitude          : Latitude in radians at which the          (input)
 *                              point scale factor is 1.0
 *   standardParallel1       : First standard parallel                   (input)
 *   standardParallel2       : Second standard parallel                  (input)
 *   falseEasting            : A coordinate value in meters assigned to the
 *                              central meridian of the projection.      (input)
 *   falseNorthing           : A coordinate value in meters assigned to the
 *                              origin latitude of the projection        (input)
 */

  double sin_lat, sin_lat_1, cos_lat;
  double m1, m2, SQRm1;
  double q0, q1, q2;
  double es_sin, one_MINUS_SQRes_sin;
  double nq0;
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
  if ((standardParallel1 < -PI_OVER_2) || (standardParallel1 > PI_OVER_2))
  { /* First Standard Parallel out of range */
    throw CoordinateConversionException( ErrorMessages::standardParallel1  );
  }
  if ((standardParallel2 < -PI_OVER_2) || (standardParallel2 > PI_OVER_2))
  { /* Second Standard Parallel out of range */
    throw CoordinateConversionException( ErrorMessages::standardParallel2  );
  }
  if ((standardParallel1 == 0.0) && (standardParallel2 == 0.0))
  { /* First & Second Standard Parallels equal 0 */
    throw CoordinateConversionException( ErrorMessages::standardParallel1_2  );
  }
  if (standardParallel1 == -standardParallel2)
  { /* Parallels are opposite latitudes */
    throw CoordinateConversionException( ErrorMessages::standardParallelHemisphere  );
  }


  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening = ellipsoidFlattening;

  Albers_Origin_Lat = originLatitude;
  Albers_Std_Parallel_1 = standardParallel1;
  Albers_Std_Parallel_2 = standardParallel2;
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Albers_Origin_Long = centralMeridian;
  Albers_False_Easting = falseEasting;
  Albers_False_Northing = falseNorthing;

  es2 = 2 * flattening - flattening * flattening;
  es = sqrt(es2);
  one_MINUS_es2 = 1 - es2;
  two_es = 2 * es;

  sin_lat = sin(Albers_Origin_Lat);
  es_sin = esSine(sin_lat);
  one_MINUS_SQRes_sin = oneMinusSqr( es_sin );
  q0 = albersQ( sin_lat, one_MINUS_SQRes_sin, es_sin );

  sin_lat_1 = sin(Albers_Std_Parallel_1);
  cos_lat = cos(Albers_Std_Parallel_1);
  es_sin = esSine(sin_lat_1);
  one_MINUS_SQRes_sin = oneMinusSqr( es_sin );
  m1 = albersM( cos_lat, one_MINUS_SQRes_sin );
  q1 = albersQ( sin_lat_1, one_MINUS_SQRes_sin, es_sin );

  SQRm1 = m1 * m1;
  if (fabs(Albers_Std_Parallel_1 - Albers_Std_Parallel_2) > 1.0e-10)
  {
    sin_lat = sin(Albers_Std_Parallel_2);
    cos_lat = cos(Albers_Std_Parallel_2);
    es_sin = esSine(sin_lat);
    one_MINUS_SQRes_sin = oneMinusSqr( es_sin );
    m2 = albersM( cos_lat, one_MINUS_SQRes_sin );
    q2 = albersQ( sin_lat, one_MINUS_SQRes_sin, es_sin );
    n = (SQRm1 - m2 * m2) / (q2 - q1);
  }
  else
    n = sin_lat_1;

  C = SQRm1 + n * q1;
  Albers_a_OVER_n = semiMajorAxis / n;
  nq0 = n * q0;
  if (C < nq0)
    rho0 = 0;
  else
    rho0 = Albers_a_OVER_n * sqrt(C - nq0);
}


AlbersEqualAreaConic::AlbersEqualAreaConic( const AlbersEqualAreaConic &aeac )
{
  semiMajorAxis = aeac.semiMajorAxis;
  flattening = aeac.flattening;
  es = aeac.es;
  es2 = aeac.es2;
  C = aeac.C;
  rho0 = aeac.rho0;
  n = aeac.n;
  Albers_a_OVER_n = aeac.Albers_a_OVER_n;
  one_MINUS_es2 = aeac.one_MINUS_es2;
  two_es = aeac.two_es;
  Albers_Origin_Long = aeac.Albers_Origin_Long;
  Albers_Origin_Lat = aeac.Albers_Origin_Lat;
  Albers_Std_Parallel_1 = aeac.Albers_Std_Parallel_1;
  Albers_Std_Parallel_2 = aeac.Albers_Std_Parallel_2;
  Albers_False_Easting = aeac.Albers_False_Easting;
  Albers_False_Northing = aeac.Albers_False_Northing;
  Albers_Delta_Northing = aeac.Albers_Delta_Northing;
  Albers_Delta_Easting = aeac.Albers_Delta_Easting;
}


AlbersEqualAreaConic::~AlbersEqualAreaConic()
{
}


AlbersEqualAreaConic& AlbersEqualAreaConic::operator=(
   const AlbersEqualAreaConic &aeac )
{
  if( this != &aeac )
  {
    semiMajorAxis = aeac.semiMajorAxis;
    flattening = aeac.flattening;
    es = aeac.es;
    es2 = aeac.es2;
    C = aeac.C;
    rho0 = aeac.rho0;
    n = aeac.n;
    Albers_a_OVER_n = aeac.Albers_a_OVER_n;
    one_MINUS_es2 = aeac.one_MINUS_es2;
    two_es = aeac.two_es;
    Albers_Origin_Long = aeac.Albers_Origin_Long;
    Albers_Origin_Lat = aeac.Albers_Origin_Lat;
    Albers_Std_Parallel_1 = aeac.Albers_Std_Parallel_1;
    Albers_Std_Parallel_2 = aeac.Albers_Std_Parallel_2;
    Albers_False_Easting = aeac.Albers_False_Easting;
    Albers_False_Northing = aeac.Albers_False_Northing;
    Albers_Delta_Northing = aeac.Albers_Delta_Northing;
    Albers_Delta_Easting = aeac.Albers_Delta_Easting;
  }

  return *this;
}


MapProjection6Parameters* AlbersEqualAreaConic::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters, and Albers projection parameters.
 *
 *  ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
 *  ellipsoidFlattening     : Flattening of ellipsoid                   (output)
 *  centralMeridian         : Longitude in radians at the center of     (output)
 *                            the projection
 *  originLatitude          : Latitude in radians at which the          (output)
 *                            point scale factor is 1.0
 *  standardParallel1       : First standard parallel                   (output)
 *  standardParallel2       : Second standard parallel                  (output)
 *  falseEasting            : A coordinate value in meters assigned to the
 *                            central meridian of the projection.       (output)
 *  falseNorthing           : A coordinate value in meters assigned to the
 *                            origin latitude of the projection         (output)
 */

  return new MapProjection6Parameters(
     CoordinateType::albersEqualAreaConic,
     Albers_Origin_Long,
     Albers_Origin_Lat,
     Albers_Std_Parallel_1,
     Albers_Std_Parallel_2,
     Albers_False_Easting,
     Albers_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* AlbersEqualAreaConic::convertFromGeodetic(
   MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Albers projection (easting and northing)
 * coordinates, according to the current ellipsoid and Albers projection
 * parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda) in radians       (input)
 *    latitude          : Latitude (phi) in radians           (input)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  double dlam;                      /* Longitude - Central Meridan */
  double sin_lat, cos_lat;
  double es_sin, one_MINUS_SQRes_sin;
  double q;
  double rho;
  double theta;
  double nq;

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

  dlam = longitude - Albers_Origin_Long;
  if (dlam > PI)
  {
    dlam -= TWO_PI;
  }
  if (dlam < -PI)
  {
    dlam += TWO_PI;
  }
  sin_lat = sin(latitude);
  cos_lat = cos(latitude);
  es_sin = esSine( sin_lat );
  one_MINUS_SQRes_sin = oneMinusSqr( es_sin );
  q = albersQ( sin_lat, one_MINUS_SQRes_sin, es_sin );
  nq = n * q;
  if (C < nq)
    rho = 0;
  else
    rho = Albers_a_OVER_n * sqrt(C - nq);


  theta = n * dlam;
  double easting = rho * sin(theta) + Albers_False_Easting;
  double northing = rho0 - rho * cos(theta) + Albers_False_Northing;

  return new MapProjectionCoordinates(
     CoordinateType::albersEqualAreaConic, easting, northing );
}


MSP::CCS::GeodeticCoordinates* AlbersEqualAreaConic::convertToGeodetic(
   MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Albers projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid and Albers projection
 * coordinates.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    easting           : Easting (X) in meters                  (input)
 *    northing          : Northing (Y) in meters                 (input)
 *    latitude          : Latitude (phi) in radians              (output)
 *    longitude         : Longitude (lambda) in radians          (output)
 */

  double dy, dx;
  double rho0_MINUS_dy;
  double q, qconst, q_OVER_2;
  double rho, rho_n;
  double PHI, Delta_PHI = 1.0;
  double sin_phi;
  double es_sin, one_MINUS_SQRes_sin;
  double theta = 0.0;
  int count = 60;
  double longitude, latitude;
  double tolerance = 4.85e-10; /* approximately 1/1000th of
                                  an arc second or 1/10th meter */

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if(   (easting < (Albers_False_Easting - Albers_Delta_Easting)) 
     || (easting >  Albers_False_Easting + Albers_Delta_Easting))
  { /* Easting out of range  */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if(   (northing < (Albers_False_Northing - Albers_Delta_Northing)) 
     || (northing >  Albers_False_Northing + Albers_Delta_Northing))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy = northing - Albers_False_Northing;
  dx = easting - Albers_False_Easting;
  rho0_MINUS_dy = rho0 - dy;
  rho = sqrt(dx * dx + rho0_MINUS_dy * rho0_MINUS_dy);

  if (n < 0)
  {
    rho *= -1.0;
    dy  *= -1.0;
    dx  *= -1.0;
    rho0_MINUS_dy *= -1.0;
  }

  if (rho != 0.0)
    theta = atan2(dx, rho0_MINUS_dy);
  rho_n = rho * n;
  q = (C - (rho_n * rho_n) / (semiMajorAxis * semiMajorAxis)) / n;
  qconst = 1 - ((one_MINUS_es2) / (two_es)) * log((1.0 - es) / (1.0 + es));
  if (fabs(fabs(qconst) - fabs(q)) > 1.0e-6)
  {
    q_OVER_2 = q / 2.0;
    if (q_OVER_2 > 1.0)
      latitude = PI_OVER_2;
    else if (q_OVER_2 < -1.0)
      latitude = -PI_OVER_2;
    else
    {
      PHI = asin(q_OVER_2);
      if (es < 1.0e-10)
        latitude = PHI;
      else
      {
        while ((fabs(Delta_PHI) > tolerance) && count)
        {
          sin_phi = sin(PHI);
          es_sin = esSine( sin_phi );
          one_MINUS_SQRes_sin = oneMinusSqr( es_sin );
          Delta_PHI = 
             (one_MINUS_SQRes_sin * one_MINUS_SQRes_sin) / (2.0 * cos(PHI)) *
             (q / (one_MINUS_es2) - sin_phi / one_MINUS_SQRes_sin +
                (log((1.0 - es_sin) / (1.0 + es_sin)) / (two_es)));
          PHI += Delta_PHI;
          count --;
        }

        if(!count)
          throw CoordinateConversionException( ErrorMessages::northing );

        latitude = PHI;
      }

      if (latitude > PI_OVER_2)  /* force distorted values to 90, -90 degrees */
        latitude = PI_OVER_2;
      else if (latitude < -PI_OVER_2)
        latitude = -PI_OVER_2;

    }
  }
  else
  {
    if (q >= 0.0)
      latitude = PI_OVER_2;
    else
      latitude = -PI_OVER_2;
  }
  longitude = Albers_Origin_Long + theta / n;

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


double AlbersEqualAreaConic::esSine( double sinlat )
{
  return es * sinlat;
}


double AlbersEqualAreaConic::albersQ(
   double slat, double oneminussqressin, double essin )
{
  return (one_MINUS_es2)*(slat / (oneminussqressin) -   
     (1 / (two_es)) *log((1 - essin) / (1 + essin)));
}

// CLASSIFICATION: UNCLASSIFIED
