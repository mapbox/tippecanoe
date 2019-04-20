// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: EQUIDISTANT CYLINDRICAL
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Equidistant Cylindrical projection coordinates
 *    (easting and northing in meters).  The Equidistant Cylindrical
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
 *          EQCY_NO_ERROR           : No errors occurred in function
 *          EQCY_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          EQCY_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          EQCY_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~20,000,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Standard Parallel)
 *          EQCY_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- 0 to ~10,000,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Standard Parallel)
 *          EQCY_STDP_ERROR         : Standard parallel outside of valid range
 *                                      (-90 to 90 degrees)
 *          EQCY_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          EQCY_A_ERROR            : Semi-major axis less than or equal to zero
 *          EQCY_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                      (250 to 350)
 *
 * REUSE NOTES
 *
 *    EQUIDISTANT CYLINDRICAL is intended for reuse by any application that performs a
 *    Equidistant Cylindrical projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on EQUIDISTANT CYLINDRICAL can be found in the Reuse Manual.
 *
 *    EQUIDISTANT CYLINDRICAL originated from :  U.S. Army Topographic Engineering Center
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
 *    EQUIDISTANT CYLINDRICAL has no restrictions.
 *
 * ENVIRONMENT
 *
 *    EQUIDISTANT CYLINDRICAL was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC 2.8.1
 *    2. MS Windows with MS Visual C++ 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    04-16-99          Original Code
 *    03-06-07          Original C++ Code
 *
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "EquidistantCylindrical.h"
#include "EquidistantCylindricalParameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h   - Standard C math library
 *    EquidistantCylindrical.h - Is for prototype error checking
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

const double PI = 3.14159265358979323e0;   /* PI                            */
const double PI_OVER_2 = ( PI / 2.0);
const double TWO_PI = (2.0 * PI);
const double ONE = (1.0 * PI / 180);       /* 1 degree in radians           */


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

EquidistantCylindrical::EquidistantCylindrical( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double standardParallel, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
  es2( 0.0066943799901413800 ),
  es4( 4.4814723452405e-005 ),
  es6( 3.0000678794350e-007 ),
  Ra( 6371007.1810824 ),
  Cos_Eqcy_Std_Parallel( 1.0 ),
  Eqcy_Origin_Long( 0.0 ),
  Eqcy_Std_Parallel( 0.0 ),
  Eqcy_False_Easting( 0.0 ),
  Eqcy_False_Northing( 0.0 ),
  Eqcy_Delta_Northing( 10007555.0 ),
  Eqcy_Max_Easting(  20015110.0 ),
  Eqcy_Min_Easting(  -20015110.0 ),
  Ra_Cos_Eqcy_Std_Parallel( 6371007.1810824 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * projection parameters as inputs, and sets the corresponding state
 * variables.  It also calculates the spherical radius of the sphere having
 * the same area as the ellipsoid.  If any errors occur, an exception is 
 * thrown with a description of the error.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters  (input)
 *    ellipsoidFlattening     : Flattening of ellipsoid                  (input)
 *    centralMeridian         : Longitude in radians at the center of    (input)
 *                              the projection
 *    standardParallel        : Latitude in radians at which the         (input)
 *                              point scale factor is 1.0
 *    falseEasting            : A coordinate value in meters assigned to the
 *                              central meridian of the projection.      (input)
 *    falseNorthing           : A coordinate value in meters assigned to the
 *                              standard parallel of the projection      (input)
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
  if ((standardParallel < -PI_OVER_2) || (standardParallel > PI_OVER_2))
  { /* standard parallel out of range */
    throw CoordinateConversionException( ErrorMessages::originLatitude  );
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
  Eqcy_Std_Parallel = standardParallel;
  Cos_Eqcy_Std_Parallel = cos(Eqcy_Std_Parallel);
  Ra_Cos_Eqcy_Std_Parallel = Ra * Cos_Eqcy_Std_Parallel;
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Eqcy_Origin_Long = centralMeridian;
  Eqcy_False_Easting = falseEasting;
  Eqcy_False_Northing = falseNorthing;
  if (Eqcy_Origin_Long > 0)
  {
    GeodeticCoordinates gcMax( CoordinateType::geodetic, Eqcy_Origin_Long - PI - ONE, PI_OVER_2 );
    MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcMax );
    Eqcy_Max_Easting = tempCoordinates->easting();
    delete tempCoordinates;

    GeodeticCoordinates gcMin( CoordinateType::geodetic, Eqcy_Origin_Long - PI, PI_OVER_2 );
    tempCoordinates = convertFromGeodetic( &gcMin );
    Eqcy_Min_Easting = tempCoordinates->easting();
    delete tempCoordinates;
  }
  else if (Eqcy_Origin_Long < 0)
  {
    GeodeticCoordinates gcMax( CoordinateType::geodetic, Eqcy_Origin_Long + PI, PI_OVER_2 );
    MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcMax );
    Eqcy_Max_Easting = tempCoordinates->easting();
    delete tempCoordinates;

    GeodeticCoordinates gcMin( CoordinateType::geodetic, Eqcy_Origin_Long + PI + ONE, PI_OVER_2 );
    tempCoordinates = convertFromGeodetic( &gcMin );
    Eqcy_Min_Easting = tempCoordinates->easting();
    delete tempCoordinates;
  }
  else
  {
    GeodeticCoordinates gcMax( CoordinateType::geodetic, PI, PI_OVER_2 );
    MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcMax );
    Eqcy_Max_Easting = tempCoordinates->easting();
    delete tempCoordinates;

    Eqcy_Min_Easting = -Eqcy_Max_Easting;
  }
  Eqcy_Delta_Northing = Ra * PI_OVER_2 + 1.0e-2;

  if(Eqcy_False_Easting)
  {
    Eqcy_Min_Easting -= Eqcy_False_Easting;
    Eqcy_Max_Easting -= Eqcy_False_Easting;
  }
}


EquidistantCylindrical::EquidistantCylindrical(
   const EquidistantCylindrical &ec )
{
  semiMajorAxis = ec.semiMajorAxis;
  flattening = ec.flattening;
  es2 = ec.es2;
  es4 = ec.es4;
  es6 = ec.es6;
  Ra = ec.Ra;
  Cos_Eqcy_Std_Parallel = ec.Cos_Eqcy_Std_Parallel;
  Eqcy_Origin_Long = ec.Eqcy_Origin_Long;
  Eqcy_Std_Parallel = ec.Eqcy_Std_Parallel;
  Eqcy_False_Easting = ec.Eqcy_False_Easting;
  Eqcy_False_Northing = ec.Eqcy_False_Northing;
  Eqcy_Delta_Northing = ec.Eqcy_Delta_Northing;
  Eqcy_Max_Easting = ec.Eqcy_Max_Easting;
  Eqcy_Min_Easting = ec.Eqcy_Min_Easting;
  Ra_Cos_Eqcy_Std_Parallel = ec.Ra_Cos_Eqcy_Std_Parallel;
}


EquidistantCylindrical::~EquidistantCylindrical()
{
}


EquidistantCylindrical& EquidistantCylindrical::operator=( const EquidistantCylindrical &ec )
{
  if( this != &ec )
  {
    semiMajorAxis = ec.semiMajorAxis;
    flattening = ec.flattening;
    es2 = ec.es2;
    es4 = ec.es4;
    es6 = ec.es6;
    Ra = ec.Ra;
    Cos_Eqcy_Std_Parallel = ec.Cos_Eqcy_Std_Parallel;
    Eqcy_Origin_Long = ec.Eqcy_Origin_Long;
    Eqcy_Std_Parallel = ec.Eqcy_Std_Parallel;
    Eqcy_False_Easting = ec.Eqcy_False_Easting;
    Eqcy_False_Northing = ec.Eqcy_False_Northing;
    Eqcy_Delta_Northing = ec.Eqcy_Delta_Northing;
    Eqcy_Max_Easting = ec.Eqcy_Max_Easting;
    Eqcy_Min_Easting = ec.Eqcy_Min_Easting;
    Ra_Cos_Eqcy_Std_Parallel = ec.Ra_Cos_Eqcy_Std_Parallel;
  }

  return *this;
}


EquidistantCylindricalParameters* EquidistantCylindrical::getParameters( ) const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters and Equidistant Cylindrical projection parameters.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
 *    ellipsoidFlattening     : Flattening of ellipsoid                   (output)
 *    centralMeridian         : Longitude in radians at the center of     (output)
 *                              the projection
 *    standardParallel        : Latitude in radians at which the          (output)
 *                              point scale factor is 1.0
 *    falseEasting            : A coordinate value in meters assigned to the
 *                              central meridian of the projection.       (output)
 *    falseNorthing           : A coordinate value in meters assigned to the
 *                              standard parallel of the projection       (output)
 */

  return new EquidistantCylindricalParameters( CoordinateType::equidistantCylindrical, Eqcy_Origin_Long, Eqcy_Std_Parallel, Eqcy_False_Easting, Eqcy_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* EquidistantCylindrical::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Equidistant Cylindrical projection (easting and northing)
 * coordinates, according to the current ellipsoid, spherical radiius
 * and Equidistant Cylindrical projection parameters.
 * If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda) in radians       (input)
 *    latitude          : Latitude (phi) in radians           (input)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  double dlam;     /* Longitude - Central Meridan */

  double longitude = geodeticCoordinates->longitude();
  double latitude = geodeticCoordinates->latitude();

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  {  /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  {  /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  dlam = longitude - Eqcy_Origin_Long;
  if (dlam > PI)
  {
    dlam -= TWO_PI;
  }
  if (dlam < -PI)
  {
    dlam += TWO_PI;
  }

  double easting = Ra_Cos_Eqcy_Std_Parallel * dlam + Eqcy_False_Easting;
  double northing = Ra * latitude + Eqcy_False_Northing;

  return new MapProjectionCoordinates( CoordinateType::equidistantCylindrical, easting, northing );
}


MSP::CCS::GeodeticCoordinates* EquidistantCylindrical::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Equidistant Cylindrical projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid, spherical radius
 * and Equidistant Cylindrical projection coordinates.
 * If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    easting           : Easting (X) in meters                  (input)
 *    northing          : Northing (Y) in meters                 (input)
 *    longitude         : Longitude (lambda) in radians          (output)
 *    latitude          : Latitude (phi) in radians              (output)
 */

  double dx, dy;
  double longitude, latitude;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Eqcy_False_Easting + Eqcy_Min_Easting))
      || (easting > (Eqcy_False_Easting + Eqcy_Max_Easting)))
  { /* Easting out of range */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < (Eqcy_False_Northing - Eqcy_Delta_Northing))
      || (northing > (Eqcy_False_Northing + Eqcy_Delta_Northing)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy = northing - Eqcy_False_Northing;
  dx = easting - Eqcy_False_Easting;
  latitude = dy / Ra;

  if (Ra_Cos_Eqcy_Std_Parallel == 0)
    longitude = 0;
  else
    longitude = Eqcy_Origin_Long + dx / Ra_Cos_Eqcy_Std_Parallel;

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
