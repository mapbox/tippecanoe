// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: GNOMONIC
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Gnomonic
 *    projection coordinates (easting and northing in meters).  This projection 
 *    employs a spherical Earth model.  The spherical radius used is the radius 
 *    of the sphere having the same area as the ellipsoid.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       GNOM_NO_ERROR           : No errors occurred in function
 *       GNOM_LAT_ERROR          : Latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *       GNOM_LON_ERROR          : Longitude outside of valid range
 *                                     (-180 to 360 degrees)
 *       GNOM_EASTING_ERROR      : Easting outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       GNOM_NORTHING_ERROR     : Northing outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       GNOM_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *       GNOM_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                     (-180 to 360 degrees)
 *       GNOM_A_ERROR            : Semi-major axis less than or equal to zero
 *       GNOM_INV_F_ERROR        : Inverse flattening outside of valid range
 *									                   (250 to 350)
 *
 *
 * REUSE NOTES
 *
 *    GNOMONIC is intended for reuse by any application that 
 *    performs a Gnomonic projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on GNOMONIC can be found in the Reuse Manual.
 *
 *    GNOMONIC originated from:     U.S. Army Topographic Engineering Center
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
 *    GNOMONIC has no restrictions.
 *
 * ENVIRONMENT
 *
 *    GNOMONIC was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. MSDOS with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    05-22-00          Original Code
 *    03-05-07          Original C++ Code
 *    05-31-11          Fixed dicsontinuitty at south pole. DR 27958
 *    
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "Gnomonic.h"
#include "MapProjection4Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h                          - Standard C math library
 *    Gnomonic.h                      - Is for prototype error checking
 *    MapProjectionCoordinates.h      - defines map projection coordinates
 *    GeodeticCoordinates.h           - defines geodetic coordinates
 *    CoordinateConversionException.h - Exception handler
 *    ErrorMessages.h                 - Contains exception messages
 */

using namespace MSP::CCS;

/***************************************************************************/
/*                               DEFINES 
 *
 */

const double PI = 3.14159265358979323e0;  /* PI */
const double PI_OVER_2 = ( PI / 2.0);                 
const double TWO_PI = ( 2.0 * PI);                 


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

Gnomonic::Gnomonic(
   double ellipsoidSemiMajorAxis,
   double ellipsoidFlattening,
   double centralMeridian,
   double originLatitude,
   double falseEasting,
   double falseNorthing ) :
  CoordinateSystem(),
  Ra( 6371007.1810824 ),
  Sin_Gnom_Origin_Lat( 0.0 ),
  Cos_Gnom_Origin_Lat( 1.0 ),
  Gnom_Origin_Long( 0.0 ),
  Gnom_Origin_Lat( 0.0 ),
  Gnom_False_Easting( 0.0 ),
  Gnom_False_Northing( 0.0 ),
  abs_Gnom_Origin_Lat( 0.0 ),
  Gnom_Delta_Northing( 40000000 ),
  Gnom_Delta_Easting( 40000000 )
{
/*
 * The constructor receives the ellipsoid parameters and
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

  es2 = 2 * flattening - flattening * flattening;
  es4 = es2 * es2;
  es6 = es4 * es2;
  /* spherical radius */
  Ra = semiMajorAxis *
     (1.0 - es2 / 6.0 - 17.0 * es4 / 360.0 - 67.0 * es6 / 3024.0);
  Gnom_Origin_Lat = originLatitude;
  Sin_Gnom_Origin_Lat = sin(Gnom_Origin_Lat);
  Cos_Gnom_Origin_Lat = cos(Gnom_Origin_Lat);
  abs_Gnom_Origin_Lat = fabs(Gnom_Origin_Lat);
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Gnom_Origin_Long = centralMeridian;
  Gnom_False_Northing = falseNorthing;
  Gnom_False_Easting = falseEasting;
}


Gnomonic::Gnomonic( const Gnomonic &g )
{
  semiMajorAxis = g.semiMajorAxis;
  flattening    = g.flattening;
  Ra            = g.Ra;
  Sin_Gnom_Origin_Lat = g.Sin_Gnom_Origin_Lat;
  Cos_Gnom_Origin_Lat = g.Cos_Gnom_Origin_Lat;
  Gnom_Origin_Long    = g.Gnom_Origin_Long;
  Gnom_Origin_Lat     = g.Gnom_Origin_Lat;
  Gnom_False_Easting  = g.Gnom_False_Easting;
  Gnom_False_Northing = g.Gnom_False_Northing;
  abs_Gnom_Origin_Lat = g.abs_Gnom_Origin_Lat;
  Gnom_Delta_Northing = g.Gnom_Delta_Northing;
  Gnom_Delta_Easting  = g.Gnom_Delta_Easting;
}


Gnomonic::~Gnomonic()
{
}


Gnomonic& Gnomonic::operator=( const Gnomonic &g )
{
  if( this != &g )
  {
    semiMajorAxis = g.semiMajorAxis;
    flattening    = g.flattening;
    Ra            = g.Ra;
    Sin_Gnom_Origin_Lat = g.Sin_Gnom_Origin_Lat;
    Cos_Gnom_Origin_Lat = g.Cos_Gnom_Origin_Lat;
    Gnom_Origin_Long    = g.Gnom_Origin_Long;
    Gnom_Origin_Lat     = g.Gnom_Origin_Lat;
    Gnom_False_Easting  = g.Gnom_False_Easting;
    Gnom_False_Northing = g.Gnom_False_Northing;
    abs_Gnom_Origin_Lat = g.abs_Gnom_Origin_Lat;
    Gnom_Delta_Northing = g.Gnom_Delta_Northing;
    Gnom_Delta_Easting  = g.Gnom_Delta_Easting;
  }

  return *this;
}


MapProjection4Parameters* Gnomonic::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters and Gnomonic projection parameters.
 *
 *  ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
 *  ellipsoidFlattening     : Flattening of ellipsoid		        (output)
 *  centralMeridian         : Longitude in radians at the center of     (output)
 *                            the projection
 *  originLatitude          : Latitude in radians at which the          (output)
 *                            point scale factor is 1.0
 *  falseEasting            : A coordinate value in meters assigned to the
 *                            central meridian of the projection.       (output)
 *  falseNorthing           : A coordinate value in meters assigned to the
 *                              origin latitude of the projection       (output)
 */

  return new MapProjection4Parameters(
     CoordinateType::gnomonic, Gnom_Origin_Long, Gnom_Origin_Lat,
     Gnom_False_Easting, Gnom_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* Gnomonic::convertFromGeodetic(
   MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Gnomonic projection (easting and northing)
 * coordinates, according to the current ellipsoid and Gnomonic projection
 * parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda) in radians       (input)
 *    latitude          : Latitude (phi) in radians           (input)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  double dlam;       /* Longitude - Central Meridan */
  double cos_c;      
  double k_prime;    /* scale factor */
  double Ra_kprime;
  double Ra_cotlat;
  double sin_dlam, cos_dlam;
  double temp_Easting, temp_Northing;
  double easting, northing;

  double longitude = geodeticCoordinates->longitude();
  double latitude  = geodeticCoordinates->latitude();
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
  dlam = longitude - Gnom_Origin_Long;
  sin_dlam = sin(dlam);
  cos_dlam = cos(dlam);
  cos_c = Sin_Gnom_Origin_Lat * slat + Cos_Gnom_Origin_Lat * clat * cos_dlam;
  if (cos_c <= 1.0e-10)
  {  /* Point is out of view.  Will return longitude out of range message
    since no point out of view is implemented.  */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  if (dlam > PI)
  {
    dlam -= TWO_PI;
  }
  if (dlam < -PI)
  {
    dlam += TWO_PI;
  }

  if (fabs(abs_Gnom_Origin_Lat - PI_OVER_2) < 1.0e-10)
  {
    Ra_cotlat = Ra * (clat / slat);
    temp_Easting = Ra_cotlat * sin_dlam;
    temp_Northing = Ra_cotlat * cos_dlam;
    if (Gnom_Origin_Lat >= 0.0)
    {
      easting  = temp_Easting + Gnom_False_Easting;
      northing = -1.0 * temp_Northing + Gnom_False_Northing;
    }
    else
    {
      easting  = -1.0 * temp_Easting + Gnom_False_Easting;
      northing = -1.0 * temp_Northing + Gnom_False_Northing;
    }
  }
  else if (abs_Gnom_Origin_Lat <= 1.0e-10)
  {
    easting  = Ra * tan(dlam) + Gnom_False_Easting;
    northing = Ra * tan(latitude) / cos_dlam + Gnom_False_Northing;
  }
  else
  {
    k_prime = 1 / cos_c;
    Ra_kprime = Ra * k_prime;
    easting  = Ra_kprime * clat * sin_dlam + Gnom_False_Easting;
    northing = Ra_kprime * 
       (Cos_Gnom_Origin_Lat * slat - Sin_Gnom_Origin_Lat * clat * cos_dlam)
       + Gnom_False_Northing;
  }

  return new MapProjectionCoordinates(
     CoordinateType::gnomonic, easting, northing );
}


MSP::CCS::GeodeticCoordinates* Gnomonic::convertToGeodetic(
   MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Gnomonic projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid and Gnomonic projection
 * coordinates.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    easting           : Easting (X) in meters                  (input)
 *    northing          : Northing (Y) in meters                 (input)
 *    longitude         : Longitude (lambda) in radians          (output)
 *    latitude          : Latitude (phi) in radians              (output)
 */

  double dx, dy;
  double rho;
  double c;
  double sin_c, cos_c;
  double dy_sinc;
  double longitude, latitude;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Gnom_False_Easting - Gnom_Delta_Easting)) 
      || (easting > (Gnom_False_Easting + Gnom_Delta_Easting)))
  { /* Easting out of range  */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < (Gnom_False_Northing - Gnom_Delta_Northing)) 
      || (northing > (Gnom_False_Northing + Gnom_Delta_Northing)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy = northing - Gnom_False_Northing;
  dx = easting - Gnom_False_Easting;
  rho = sqrt(dx * dx + dy * dy);
  if (fabs(rho) <= 1.0e-10)
  {
    latitude  = Gnom_Origin_Lat;
    longitude = Gnom_Origin_Long;
  }
  else
  {
    c = atan(rho / Ra);
    sin_c = sin(c);
    cos_c = cos(c);
    dy_sinc = dy * sin_c;
    latitude = asin((cos_c * Sin_Gnom_Origin_Lat)
       + ((dy_sinc * Cos_Gnom_Origin_Lat) / rho));

    if (fabs(abs_Gnom_Origin_Lat - PI_OVER_2) < 1.0e-10)
    {
      if (Gnom_Origin_Lat >= 0.0)
        longitude = Gnom_Origin_Long + atan2(dx, -dy);
      else
        longitude = Gnom_Origin_Long + atan2(dx, dy);
    }
    else
      longitude = Gnom_Origin_Long
         + atan2((dx * sin_c),
            (rho * Cos_Gnom_Origin_Lat * cos_c - dy_sinc *Sin_Gnom_Origin_Lat));
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

  return new GeodeticCoordinates(
     CoordinateType::geodetic, longitude, latitude );
}



// CLASSIFICATION: UNCLASSIFIED
