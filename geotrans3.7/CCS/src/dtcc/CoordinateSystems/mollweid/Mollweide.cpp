// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: MOLLWEIDE
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Mollweide projection coordinates
 *    (easting and northing in meters).  The Mollweide Pseudocylindrical
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
 *          MOLL_NO_ERROR           : No errors occurred in function
 *          MOLL_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          MOLL_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          MOLL_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~18,000,000 m,
 *                                       depending on ellipsoid parameters)
 *          MOLL_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~9,000,000 m,
 *                                       depending on ellipsoid parameters)
 *          MOLL_ORIGIN_LON_ERROR   : Origin longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          MOLL_A_ERROR            : Semi-major axis less than or equal to zero
 *          MOLL_INV_F_ERROR        : Inverse flattening outside of valid range
 *								  	                  (250 to 350)
 *
 * REUSE NOTES
 *
 *    MOLLWEID is intended for reuse by any application that performs a
 *    Mollweide projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on MOLLWEID can be found in the Reuse Manual.
 *
 *    MOLLWEID originated from :  U.S. Army Topographic Engineering Center
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
 *    MOLLWEID has no restrictions.
 *
 * ENVIRONMENT
 *
 *    MOLLWEID was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC 2.8.1
 *    2. Windows 95 with MS Visual C++ 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    04-16-99          Original Code
 *    03-05-07          Original C++ Code
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "Mollweide.h"
#include "MapProjection3Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h     - Standard C math library
 *    Mollweide.h - Is for prototype error checking
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

const double PI = 3.14159265358979323e0;      /* PI                     */
const double PI_OVER_2 = ( PI / 2.0);                 
const double MAX_LAT = ( (PI * 90) / 180.0 ); /* 90 degrees in radians  */
const double TWO_PI = (2.0 * PI);                  


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

Mollweide::Mollweide( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
  es2( 0.0066943799901413800 ),
  es4( 4.4814723452405e-005 ),
  es6( 3.0000678794350e-007 ),
  Sqrt2_Ra( 9009964.7614632 ),
  Sqrt8_Ra( 18019929.522926 ),
  Moll_Origin_Long( 0.0 ),
  Moll_False_Easting( 0.0 ),
  Moll_False_Northing( 0.0 ),
  Moll_Delta_Northing( 9009965.0 ),
  Moll_Max_Easting( 18019930.0 ),
  Moll_Min_Easting( -18019930.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * Mollweide projcetion parameters as inputs, and sets the corresponding state
 * variables.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters (input)
 *    ellipsoidFlattening     : Flattening of ellipsoid                 (input)
 *    centralMeridian         : Longitude in radians at the center of   (input)
 *                              the projection
 *    falseEasting            : A coordinate value in meters assigned to the
 *                              central meridian of the projection.     (input)
 *    falseNorthing           : A coordinate value in meters assigned to the
 *                              origin latitude of the projection       (input)
 */

  double Ra;                       /* Spherical Radius */
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
  Ra = semiMajorAxis * (1.0 - es2 / 6.0 - 17.0 * es4 / 360.0 - 67.0 * es6 / 3024.0);
  Sqrt2_Ra = sqrt(2.0) * Ra;
  Sqrt8_Ra = sqrt(8.0) * Ra;
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Moll_Origin_Long = centralMeridian;
  Moll_False_Easting = falseEasting;
  Moll_False_Northing = falseNorthing;

  if (Moll_Origin_Long > 0)
  {
    Moll_Max_Easting = 17919819.0;
    Moll_Min_Easting = -18019930.0;
  }
  else if (Moll_Origin_Long < 0)
  {
    Moll_Max_Easting = 18019930.0;
    Moll_Min_Easting = -17919819.0;
  }
  else
  {
    Moll_Max_Easting = 18019930.0;
    Moll_Min_Easting = -18019930.0;
  }
}


Mollweide::Mollweide( const Mollweide &m )
{
  semiMajorAxis = m.semiMajorAxis;
  flattening = m.flattening;
  es2 = m.es2;
  es4 = m.es4;
  es6 = m.es6;
  Sqrt2_Ra = m.Sqrt2_Ra;
  Sqrt8_Ra = m.Sqrt8_Ra;
  Moll_Origin_Long = m.Moll_Origin_Long;
  Moll_False_Easting = m.Moll_False_Easting;
  Moll_False_Northing = m.Moll_False_Northing;
  Moll_Delta_Northing = m.Moll_Delta_Northing;
  Moll_Max_Easting = m.Moll_Max_Easting;
  Moll_Min_Easting = m.Moll_Min_Easting;
}


Mollweide::~Mollweide()
{
}


Mollweide& Mollweide::operator=( const Mollweide &m )
{
  if( this != &m )
  {
    semiMajorAxis = m.semiMajorAxis;
    flattening = m.flattening;
    es2 = m.es2;
    es4 = m.es4;
    es6 = m.es6;
    Sqrt2_Ra = m.Sqrt2_Ra;
    Sqrt8_Ra = m.Sqrt8_Ra;
    Moll_Origin_Long = m.Moll_Origin_Long;
    Moll_False_Easting = m.Moll_False_Easting;
    Moll_False_Northing = m.Moll_False_Northing;
    Moll_Delta_Northing = m.Moll_Delta_Northing;
    Moll_Max_Easting = m.Moll_Max_Easting;
    Moll_Min_Easting = m.Moll_Min_Easting;
  }

  return *this;
}


MapProjection3Parameters* Mollweide::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters and Mollweide projection parameters.
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

  return new MapProjection3Parameters( CoordinateType::mollweide, Moll_Origin_Long, Moll_False_Easting, Moll_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* Mollweide::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Mollweide projection (easting and northing)
 * coordinates, according to the current ellipsoid and Mollweide projection
 * parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda) in radians       (input)
 *    latitude          : Latitude (phi) in radians           (input)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  double dlam;                                  /* Longitude - Central Meridan */
  double theta;
  double delta_theta_primed = 0.1745329;        /* arbitrarily initialized to 10 deg */
  double dtp_tolerance = 4.85e-10;              /* approximately 1/1000th of
                                                 an arc second or 1/10th meter */
  int count = 60;

  double longitude = geodeticCoordinates->longitude();
  double latitude  = geodeticCoordinates->latitude();
  double PI_Sin_Latitude = PI * sin(latitude);
  double theta_primed = latitude;

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  {  /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  {  /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  dlam = longitude - Moll_Origin_Long;
  if (dlam > PI)
  {
    dlam -= TWO_PI;
  }
  if (dlam < -PI)
  {
    dlam += TWO_PI;
  }
  while (fabs(delta_theta_primed) > dtp_tolerance && count)
  {
    delta_theta_primed = -(theta_primed + sin(theta_primed) -
                           PI_Sin_Latitude) / (1.0 + cos(theta_primed));
    theta_primed += delta_theta_primed;
    count --;
  }

  if(!count)
    throw CoordinateConversionException( ErrorMessages::northing );

  theta = theta_primed / 2.0;
  double easting = (Sqrt8_Ra / PI ) * dlam * cos(theta) +
             Moll_False_Easting;
  double northing = Sqrt2_Ra * sin(theta) + Moll_False_Northing;

  return new MapProjectionCoordinates( CoordinateType::mollweide, easting, northing );
}


MSP::CCS::GeodeticCoordinates* Mollweide::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Mollweide projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid and Mollweide projection
 * coordinates.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    easting           : Easting (X) in meters                  (input)
 *    northing          : Northing (Y) in meters                 (input)
 *    longitude         : Longitude (lambda) in radians          (output)
 *    latitude          : Latitude (phi) in radians              (output)
 */

  double dx, dy;
  double theta = 0.0;
  double two_theta;
  double i;
  double longitude, latitude;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if(   (easting < (Moll_False_Easting + Moll_Min_Easting))
     || (easting > (Moll_False_Easting + Moll_Max_Easting)))
  { /* Easting out of range  */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if((northing < (Moll_False_Northing - Moll_Delta_Northing)) || 
     (northing > (Moll_False_Northing + Moll_Delta_Northing) ))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy = northing - Moll_False_Northing;
  dx = easting - Moll_False_Easting;
  i = dy / Sqrt2_Ra;
  if (fabs(i) > 1.0)
  {
    latitude = MAX_LAT;
    if (northing < 0.0)
      latitude *= -1.0;
  }

  else
  {
    theta = asin(i);
    two_theta = 2.0 * theta;
    latitude = asin((two_theta + sin(two_theta)) / PI);

    if (latitude > PI_OVER_2)  /* force distorted values to 90, -90 degrees */
      latitude = PI_OVER_2;
    else if (latitude < -PI_OVER_2)
      latitude = -PI_OVER_2;

  }
  if (fabs(fabs(latitude) - MAX_LAT) < 1.0e-10)
    longitude = Moll_Origin_Long;
  else
    longitude = Moll_Origin_Long + PI * dx /
                 (Sqrt8_Ra * cos(theta));

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
