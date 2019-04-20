// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: AZIMUTHAL EQUIDISTANT
 *
 * ABSTRACT
 *
 *   This component provides conversions between Geodetic coordinates
 *   (latitude and longitude in radians) and Azimuthal Equidistant
 *   projection coordinates (easting and northing in meters).  This projection 
 *   employs a spherical Earth model. The spherical radius used is the radius of
 *   the sphere having the same area as the ellipsoid.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       AZEQ_NO_ERROR           : No errors occurred in function
 *       AZEQ_LAT_ERROR          : Latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *       AZEQ_LON_ERROR          : Longitude outside of valid range
 *                                     (-180 to 360 degrees)
 *       AZEQ_EASTING_ERROR      : Easting outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       AZEQ_NORTHING_ERROR     : Northing outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       AZEQ_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *       AZEQ_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                     (-180 to 360 degrees)
 *       AZEQ_A_ERROR            : Semi-major axis less than or equal to zero
 *       AZEQ_INV_F_ERROR        : Inverse flattening outside of valid range
 *									                   (250 to 350)
 *       AZEQ_PROJECTION_ERROR   : Point is plotted as a circle of radius PI * Ra
 *
 *
 * REUSE NOTES
 *
 *    AZIMUTHAL EQUIDISTANT is intended for reuse by any application that 
 *    performs an Azimuthal Equidistant projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on AZIMUTHAL EQUIDISTANT can be found in the Reuse Manual.
 *
 *    AZIMUTHAL EQUIDISTANT originated from:
 *                U.S. Army Topographic Engineering Center
 *                Geospatial Information Division
 *                7701 Telegraph Road
 *                Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    AZIMUTHAL EQUIDISTANT has no restrictions.
 *
 * ENVIRONMENT
 *
 *    AZIMUTHAL EQUIDISTANT was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. MSDOS with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    05-19-00          Original Code
 *    03-08-07          Original C++ Code
 *    
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "AzimuthalEquidistant.h"
#include "MapProjection4Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h     - Standard C math library
 *    AzimuthalEquidistant.h   - Is for prototype error checking
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

const double PI = 3.14159265358979323e0;   /* PI                  */
const double PI_OVER_2 = ( PI / 2.0);                 
const double TWO_PI = ( 2.0 * PI);                 
const double ONE = (1.0 * PI / 180);       /* 1 degree in radians */


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

AzimuthalEquidistant::AzimuthalEquidistant(
   double ellipsoidSemiMajorAxis,
   double ellipsoidFlattening,
   double centralMeridian,
   double originLatitude,
   double falseEasting,
   double falseNorthing ) :
  CoordinateSystem(),
  Ra( 6371007.1810824 ),
  Sin_Azeq_Origin_Lat( 0.0 ),
  Cos_Azeq_Origin_Lat( 1.0 ),
  Azeq_Origin_Long( 0.0 ),
  Azeq_Origin_Lat( 0.0 ),
  Azeq_False_Easting( 0.0 ),
  Azeq_False_Northing( 0.0 ),
  abs_Azeq_Origin_Lat( 0.0 ),
  Azeq_Delta_Northing( 19903915.0 ),
  Azeq_Delta_Easting( 19903915.0 )
{
/*
 * The function setParameters receives the ellipsoid parameters and
 * projection parameters as inputs, and sets the corresponding state
 * variables.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *  ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
 *  ellipsoidFlattening     : Flattening of ellipsoid                   (input)
 *  centralMeridian         : Longitude in radians at the center of     (input)
 *                            the projection
 *  originLatitude          : Latitude in radians at which the          (input)
 *                            point scale factor is 1.0
 *  falseEasting            : A coordinate value in meters assigned to the
 *                            central meridian of the projection.       (input)
 *  falseNorthing           : A coordinate value in meters assigned to the
 *                            origin latitude of the projection         (input)
 */

  double es2, es4, es6;
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

  es2 = 2 * flattening - flattening * flattening;
  es4 = es2 * es2;
  es6 = es4 * es2;
  /* spherical radius */
  Ra = semiMajorAxis * 
     (1.0 - es2 / 6.0 - 17.0 * es4 / 360.0 - 67.0 * es6 / 3024.0);
  Azeq_Origin_Lat = originLatitude;
  Sin_Azeq_Origin_Lat = sin(Azeq_Origin_Lat);
  Cos_Azeq_Origin_Lat = cos(Azeq_Origin_Lat);
  abs_Azeq_Origin_Lat = fabs(Azeq_Origin_Lat);
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Azeq_Origin_Long = centralMeridian;
  Azeq_False_Northing = falseNorthing;
  Azeq_False_Easting = falseEasting;

  if (fabs(abs_Azeq_Origin_Lat - PI_OVER_2) < 1.0e-10)
  {
    Azeq_Delta_Northing = 20015110.0;
    Azeq_Delta_Easting = 20015110.0;
  }
  else if (abs_Azeq_Origin_Lat >= 1.0e-10)
  {
    if (Azeq_Origin_Long > 0.0)
    {
      GeodeticCoordinates gcDelta( CoordinateType::geodetic, Azeq_Origin_Long - PI + ONE, -Azeq_Origin_Lat );
      MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcDelta );
      Azeq_Delta_Easting = tempCoordinates->easting();
      delete tempCoordinates;
    }
    else
    {
      GeodeticCoordinates gcDelta(CoordinateType::geodetic, Azeq_Origin_Long + PI - ONE, -Azeq_Origin_Lat );
      MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcDelta );
      Azeq_Delta_Easting = tempCoordinates->easting();
      delete tempCoordinates;
    }

    if(Azeq_False_Easting)
      Azeq_Delta_Easting -= Azeq_False_Easting;
    if (Azeq_Delta_Easting < 0)
      Azeq_Delta_Easting = -Azeq_Delta_Easting;

    Azeq_Delta_Northing = 19903915.0;
  }
  else
  {
    Azeq_Delta_Northing = 19903915.0;
    Azeq_Delta_Easting = 19903915.0;
  }
}


AzimuthalEquidistant::AzimuthalEquidistant( const AzimuthalEquidistant &ae )
{
  semiMajorAxis = ae.semiMajorAxis;
  flattening = ae.flattening;
  Ra = ae.Ra;
  Sin_Azeq_Origin_Lat = ae.Sin_Azeq_Origin_Lat;
  Cos_Azeq_Origin_Lat = ae.Cos_Azeq_Origin_Lat;
  Azeq_Origin_Long = ae.Azeq_Origin_Long;
  Azeq_Origin_Lat = ae.Azeq_Origin_Lat;
  Azeq_False_Easting = ae.Azeq_False_Easting;
  Azeq_False_Northing = ae.Azeq_False_Northing;
  abs_Azeq_Origin_Lat = ae.abs_Azeq_Origin_Lat;
  Azeq_Delta_Northing = ae.Azeq_Delta_Northing;
  Azeq_Delta_Easting = ae.Azeq_Delta_Easting;
}


AzimuthalEquidistant::~AzimuthalEquidistant()
{
}


AzimuthalEquidistant& AzimuthalEquidistant::operator=( const AzimuthalEquidistant &ae )
{
  if( this != &ae )
  {
    semiMajorAxis = ae.semiMajorAxis;
    flattening = ae.flattening;
    Ra = ae.Ra;
    Sin_Azeq_Origin_Lat = ae.Sin_Azeq_Origin_Lat;
    Cos_Azeq_Origin_Lat = ae.Cos_Azeq_Origin_Lat;
    Azeq_Origin_Long = ae.Azeq_Origin_Long;
    Azeq_Origin_Lat = ae.Azeq_Origin_Lat;
    Azeq_False_Easting = ae.Azeq_False_Easting;
    Azeq_False_Northing = ae.Azeq_False_Northing;
    abs_Azeq_Origin_Lat = ae.abs_Azeq_Origin_Lat;
    Azeq_Delta_Northing = ae.Azeq_Delta_Northing;
    Azeq_Delta_Easting = ae.Azeq_Delta_Easting;
  }

  return *this;
}


MapProjection4Parameters* AzimuthalEquidistant::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters and Azimuthal Equidistant projection parameters.
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

  return new MapProjection4Parameters( CoordinateType::azimuthalEquidistant, Azeq_Origin_Long, Azeq_Origin_Lat, Azeq_False_Easting, Azeq_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* AzimuthalEquidistant::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Azimuthal Equidistant projection (easting and northing)
 * coordinates, according to the current ellipsoid and Azimuthal Equidistant projection
 * parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda) in radians       (input)
 *    latitude          : Latitude (phi) in radians           (input)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  double dlam;       /* Longitude - Central Meridan */
  double k_prime;    /* scale factor */
  double c;          /* angular distance from center */
  double cos_c;
  double sin_dlam, cos_dlam;
  double Ra_kprime;
  double Ra_PI_OVER_2_Lat;
  double easting, northing;

  double longitude = geodeticCoordinates->longitude();
  double latitude = geodeticCoordinates->latitude();
  double slat = sin(latitude);
  double clat = cos(latitude);

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  { /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  { /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  dlam = longitude - Azeq_Origin_Long;
  if (dlam > PI)
  {
    dlam -= TWO_PI;
  }
  if (dlam < -PI)
  {
    dlam += TWO_PI;
  }

  sin_dlam = sin(dlam);
  cos_dlam = cos(dlam);
  if (fabs(abs_Azeq_Origin_Lat - PI_OVER_2) < 1.0e-10)
  {
    if (Azeq_Origin_Lat >= 0.0)
    {
      Ra_PI_OVER_2_Lat = Ra * (PI_OVER_2 - latitude);
      easting = Ra_PI_OVER_2_Lat * sin_dlam + Azeq_False_Easting;
      northing = -1.0 * (Ra_PI_OVER_2_Lat * cos_dlam) + Azeq_False_Northing;
    }
    else
    {
      Ra_PI_OVER_2_Lat = Ra * (PI_OVER_2 + latitude);
      easting = Ra_PI_OVER_2_Lat * sin_dlam + Azeq_False_Easting;
      northing = Ra_PI_OVER_2_Lat * cos_dlam + Azeq_False_Northing;
    }
  }
  else if (abs_Azeq_Origin_Lat <= 1.0e-10)
  {
    cos_c = clat * cos_dlam;
    if (fabs(fabs(cos_c) - 1.0) < 1.0e-14)
    {
      if (cos_c >= 0.0)
      {
        easting = Azeq_False_Easting;
        northing = Azeq_False_Northing;
      }
      else
      {
        /* if cos_c == -1 */
        throw CoordinateConversionException( MSP::CCS::ErrorMessages::projection );
      }
    }
    else
    {
      c = acos(cos_c);
      k_prime = c / sin(c);
      Ra_kprime = Ra * k_prime;
      easting = Ra_kprime * clat * sin_dlam + Azeq_False_Easting;
      northing = Ra_kprime * slat + Azeq_False_Northing;
    }
  }
  else
  {
    cos_c = (Sin_Azeq_Origin_Lat * slat) + (Cos_Azeq_Origin_Lat * clat * cos_dlam);
    if (fabs(fabs(cos_c) - 1.0) < 1.0e-14)
    {
      if (cos_c >= 0.0)
      {
        easting = Azeq_False_Easting;
        northing = Azeq_False_Northing;
      }
      else
      {
        /* if cos_c == -1 */
        throw CoordinateConversionException( MSP::CCS::ErrorMessages::projection );
      }
    }
    else
    {
      c = acos(cos_c);
      k_prime = c / sin(c);
      Ra_kprime = Ra * k_prime;
      easting = Ra_kprime * clat * sin_dlam + Azeq_False_Easting;
      northing = Ra_kprime * (Cos_Azeq_Origin_Lat * slat - Sin_Azeq_Origin_Lat * clat * cos_dlam) + Azeq_False_Northing;
    }
  }

  return new MapProjectionCoordinates( CoordinateType::azimuthalEquidistant, easting, northing );
}


MSP::CCS::GeodeticCoordinates* AzimuthalEquidistant::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Azimuthal_Equidistant projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid and Azimuthal_Equidistant projection
 * coordinates.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    easting           : Easting (X) in meters                  (input)
 *    northing          : Northing (Y) in meters                 (input)
 *    longitude         : Longitude (lambda) in radians          (output)
 *    latitude          : Latitude (phi) in radians              (output)
 */

  double dx, dy;
  double rho;        /* height above ellipsoid */
  double c;          /* angular distance from center */
  double sin_c, cos_c, dy_sinc;
  double longitude, latitude;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Azeq_False_Easting - Azeq_Delta_Easting)) 
      || (easting > (Azeq_False_Easting + Azeq_Delta_Easting)))
  { /* Easting out of range  */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < (Azeq_False_Northing - Azeq_Delta_Northing)) 
      || (northing > (Azeq_False_Northing + Azeq_Delta_Northing)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy  = northing - Azeq_False_Northing;
  dx  = easting - Azeq_False_Easting;
  rho = sqrt(dx * dx + dy * dy);
  if (fabs(rho) <= 1.0e-10)
  {
    latitude  = Azeq_Origin_Lat;
    longitude = Azeq_Origin_Long;
  }
  else
  {
    c = rho / Ra;
    sin_c = sin(c);
    cos_c = cos(c);
    dy_sinc = dy * sin_c;
    latitude = asin((cos_c * Sin_Azeq_Origin_Lat) + ((dy_sinc * Cos_Azeq_Origin_Lat) / rho));
    if (fabs(abs_Azeq_Origin_Lat - PI_OVER_2) < 1.0e-10)
    {
      if (Azeq_Origin_Lat >= 0.0)
        longitude = Azeq_Origin_Long + atan2(dx, -dy);
      else
        longitude = Azeq_Origin_Long + atan2(dx, dy);
    }
    else
      longitude = Azeq_Origin_Long + atan2((dx * sin_c), ((rho * Cos_Azeq_Origin_Lat * cos_c) - (dy_sinc * Sin_Azeq_Origin_Lat)));
  }

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
