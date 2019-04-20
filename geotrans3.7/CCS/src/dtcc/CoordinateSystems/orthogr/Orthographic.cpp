// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: ORTHOGRAPHIC
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Orthographic projection 
 *    coordinates (easting and northing in meters).  The Orthographic
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
 *          ORTH_NO_ERROR           : No errors occurred in function
 *          ORTH_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          ORTH_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          ORTH_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~6,500,000 m,
 *                                       depending on ellipsoid parameters)
 *          ORTH_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~6,500,000 m,
 *                                       depending on ellipsoid parameters)
 *          ORTH_RADIUS_ERROR       : Coordinates too far from pole,
 *                                      depending on ellipsoid and
 *                                      projection parameters
 *          ORTH_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          ORTH_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          ORTH_A_ERROR            : Semi-major axis less than or equal to zero
 *          ORTH_INV_F_ERROR        : Inverse flattening outside of valid range
 *								  	                  (250 to 350)
 *
 * REUSE NOTES
 *
 *    ORTHOGRAPHIC is intended for reuse by any application that performs a
 *    Orthographic projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on ORTHOGRAPHIC can be found in the Reuse Manual.
 *
 *    ORTHOGRAPHIC originated from :  U.S. Army Topographic Engineering Center
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
 *    ORTHOGRAPHIC has no restrictions.
 *
 * ENVIRONMENT
 *
 *    ORTHOGRAPHIC was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    06-15-99          Original Code
 *    03-05-07          Original C++ Code
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "Orthographic.h"
#include "MapProjection4Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h    - Standard C math library
 *    Orthographic.h - Is for prototype error checking
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

const double PI = 3.14159265358979323e0;       /* PI                     */
const double PI_OVER_2 = ( PI / 2.0);                 
const double MAX_LAT = ( (PI * 90) / 180.0 );  /* 90 degrees in radians  */
const double TWO_PI = (2.0 * PI);                  


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

Orthographic::Orthographic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
  es2( 0.0066943799901413800 ),
  es4( 4.4814723452405e-005 ),
  es6( 3.0000678794350e-007 ),
  Ra( 6371007.1810824 ),
  Orth_Origin_Long( 0.0 ),
  Orth_Origin_Lat( 0.0 ),
  Orth_False_Easting( 0.0 ),
  Orth_False_Northing( 0.0 ),
  Sin_Orth_Origin_Lat( 0.0 ),
  Cos_Orth_Origin_Lat( 1.0 )     
{
/*
 * The constructor receives the ellipsoid parameters and
 * projection parameters as inputs, and sets the corresponding state
 * variables.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
 *    ellipsoidFlattening     : Flattening of ellipsoid						        (input)
 *    centralMeridian         : Longitude in radians at the center of     (input)
 *                              the projection
 *    originLatitude          : Latitude in radians at which the          (input)
 *                              point scale factor is 1.0
 *    falseEasting            : A coordinate value in meters assigned to the
 *                              central meridian of the projection.       (input)
 *    falseNorthing           : A coordinate value in meters assigned to the
 *                              origin latitude of the projection         (input)                          
 */

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
  Ra = semiMajorAxis * (1.0 - es2 / 6.0 - 17.0 * es4 / 360.0 - 67.0 * es6 /3024.0);
  Orth_Origin_Lat = originLatitude;
  Sin_Orth_Origin_Lat = sin(Orth_Origin_Lat);
  Cos_Orth_Origin_Lat = cos(Orth_Origin_Lat);
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Orth_Origin_Long = centralMeridian;
  Orth_False_Easting = falseEasting;
  Orth_False_Northing = falseNorthing;
}


Orthographic::Orthographic( const Orthographic &o )
{
  semiMajorAxis = o.semiMajorAxis;
  flattening = o.flattening;
  es2 = o.es2;     
  es4 = o.es4;     
  es6 = o.es6;     
  Ra = o.Ra; 
  Orth_Origin_Long = o.Orth_Origin_Long; 
  Orth_Origin_Lat = o.Orth_Origin_Lat; 
  Orth_False_Easting = o.Orth_False_Easting; 
  Orth_False_Northing = o.Orth_False_Northing; 
  Sin_Orth_Origin_Lat = o.Sin_Orth_Origin_Lat; 
  Cos_Orth_Origin_Lat = o.Cos_Orth_Origin_Lat; 
}


Orthographic::~Orthographic()
{
}


Orthographic& Orthographic::operator=( const Orthographic &o )
{
  if( this != &o )
  {
    semiMajorAxis = o.semiMajorAxis;
    flattening = o.flattening;
    es2 = o.es2;     
    es4 = o.es4;     
    es6 = o.es6;     
    Ra = o.Ra; 
    Orth_Origin_Long = o.Orth_Origin_Long; 
    Orth_Origin_Lat = o.Orth_Origin_Lat; 
    Orth_False_Easting = o.Orth_False_Easting; 
    Orth_False_Northing = o.Orth_False_Northing; 
    Sin_Orth_Origin_Lat = o.Sin_Orth_Origin_Lat; 
    Cos_Orth_Origin_Lat = o.Cos_Orth_Origin_Lat; 
  }

  return *this;
}


MapProjection4Parameters* Orthographic::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters and Orthographic projection parameters.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
 *    ellipsoidFlattening     : Flattening of ellipsoid						        (output)
 *    centralMeridian         : Longitude in radians at the center of     (output)
 *                              the projection
 *    originLatitude          : Latitude in radians at which the          (output)
 *                              point scale factor is 1.0
 *    falseEasting            : A coordinate value in meters assigned to the
 *                              central meridian of the projection.       (output)
 *    falseNorthing           : A coordinate value in meters assigned to the
 *                              origin latitude of the projection         (output) 
 */

  return new MapProjection4Parameters( CoordinateType::orthographic, Orth_Origin_Long, Orth_Origin_Lat, Orth_False_Easting, Orth_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* Orthographic::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Orthographic projection (easting and northing)
 * coordinates, according to the current ellipsoid and Orthographic projection
 * parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda) in radians       (input)
 *    latitude          : Latitude (phi) in radians           (input)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  double dlam;  /* Longitude - Central Meridan */
  double clat_cdlam;
  double cos_c; /* Value used to determine whether the point is beyond
                   viewing.  If zero or positive, the point is within view.  */

  double longitude = geodeticCoordinates->longitude();
  double latitude  = geodeticCoordinates->latitude();
  double slat = sin(latitude);
  double clat = cos(latitude);

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  {  /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  {  /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude );
  }

  dlam = longitude - Orth_Origin_Long;
  clat_cdlam = clat * cos(dlam);
  cos_c = Sin_Orth_Origin_Lat * slat + Cos_Orth_Origin_Lat * clat_cdlam;
  if (cos_c < 0.0)
  {  /* Point is out of view.  Will return longitude out of range message
  since no point out of view is implemented.  */
    throw CoordinateConversionException( ErrorMessages::longitude );
  }

  if (dlam > PI)
  {
    dlam -= TWO_PI;
  }
  if (dlam < -PI)
  {
    dlam += TWO_PI;
  }
  double easting = Ra * clat * sin(dlam) + Orth_False_Easting;
  double northing = Ra * (Cos_Orth_Origin_Lat * slat - Sin_Orth_Origin_Lat * clat_cdlam) +
              Orth_False_Northing;

  return new MapProjectionCoordinates( CoordinateType::orthographic, easting, northing );
}


MSP::CCS::GeodeticCoordinates* Orthographic::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Orthographic projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid and Orthographic projection
 * coordinates.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    easting           : Easting (X) in meters                  (input)
 *    northing          : Northing (Y) in meters                 (input)
 *    longitude         : Longitude (lambda) in radians          (output)
 *    latitude          : Latitude (phi) in radians              (output)
 */

  double cc;
  double cos_cc, sin_cc;
  double rho;
  double dx, dy;
  double temp;
  double rho_OVER_Ra;
  double longitude, latitude;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting > (Orth_False_Easting + Ra)) ||
      (easting < (Orth_False_Easting - Ra)))
  { /* Easting out of range */
    throw CoordinateConversionException( ErrorMessages::easting );
  }
  if ((northing > (Orth_False_Northing + Ra)) ||
      (northing < (Orth_False_Northing - Ra)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing );
  }

  temp = sqrt(easting * easting + northing * northing);     

  if((temp > (Orth_False_Easting + Ra)) ||
     (temp > (Orth_False_Northing + Ra)) ||
     (temp < (Orth_False_Easting - Ra)) ||
     (temp < (Orth_False_Northing - Ra)))
  { /* Point is outside of projection area */
    throw CoordinateConversionException( ErrorMessages::radius );
  }

  dx = easting  - Orth_False_Easting;
  dy = northing - Orth_False_Northing;
  rho = sqrt(dx * dx + dy * dy);
  if (rho == 0.0)
  {
    latitude = Orth_Origin_Lat;
    longitude = Orth_Origin_Long;
  }
  else
  {
    rho_OVER_Ra = rho / Ra;

    if (rho_OVER_Ra > 1.0)
      rho_OVER_Ra = 1.0;
    else if (rho_OVER_Ra < -1.0)
      rho_OVER_Ra = -1.0;

    cc = asin(rho_OVER_Ra);
    cos_cc = cos(cc);
    sin_cc = sin(cc);
    latitude = asin(cos_cc * Sin_Orth_Origin_Lat + (dy * sin_cc * Cos_Orth_Origin_Lat / rho));

    if (Orth_Origin_Lat == MAX_LAT)
      longitude = Orth_Origin_Long + atan2(dx, -dy);
    else if (Orth_Origin_Lat == -MAX_LAT)
     longitude = Orth_Origin_Long + atan2(dx, dy);
    else
      longitude = Orth_Origin_Long + atan2(dx * sin_cc, (rho *
                  Cos_Orth_Origin_Lat * cos_cc - dy * Sin_Orth_Origin_Lat * sin_cc));

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

  return new GeodeticCoordinates( CoordinateType::geodetic, longitude, latitude );
}



// CLASSIFICATION: UNCLASSIFIED
