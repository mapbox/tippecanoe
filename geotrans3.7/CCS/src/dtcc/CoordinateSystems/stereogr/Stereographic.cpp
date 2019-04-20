// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: STEREOGRAPHIC
 *
 *
 * ABSTRACT
 *
 *    This component provides conversions between geodetic (latitude and
 *    longitude) coordinates and Stereographic (easting and northing)
 *    coordinates.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid
 *    value is found the error code is combined with the current error code
 *    using the bitwise or.  This combining allows multiple error codes to
 *    be returned. The possible error codes are:
 *
 *          STEREO_NO_ERROR           : No errors occurred in function
 *          STEREO_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          STEREO_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          STEREO_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          STEREO_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          STEREO_EASTING_ERROR      : Easting outside of valid range,
 *                                      (False_Easting +/- ~1,460,090,226 m,
 *                                       depending on ellipsoid and projection
 *                                       parameters)
 *          STEREO_NORTHING_ERROR     : Northing outside of valid range,
 *                                      (False_Northing +/- ~1,460,090,226 m,
 *                                       depending on ellipsoid and projection
 *                                       parameters)
 *          STEREO_A_ERROR            : Semi-major axis less than or equal to zero
 *          STEREO_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                      (250 to 350)
 *
 *
 * REUSE NOTES
 *
 *    STEREOGRAPHIC is intended for reuse by any application that
 *    performs a Stereographic projection.
 *
 *
 * REFERENCES
 *
 *    Further information on STEREOGRAPHIC can be found in the
 *    Reuse Manual.
 *
 *
 *    STEREOGRAPHIC originated from :
 *                                U.S. Army Topographic Engineering Center
 *                                Geospatial Information Division
 *                                7701 Telegraph Road
 *                                Alexandria, VA  22310-3864
 *
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 *
 * RESTRICTIONS
 *
 *    STEREOGRAPHIC has no restrictions.
 *
 *
 * ENVIRONMENT
 *
 *    STEREOGRAPHIC was tested and certified in the following
 *    environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Window 95 with MS Visual C++, version 6
 *
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    3-1-07            Original Code
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "Stereographic.h"
#include "MapProjection4Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h      - Standard C math library
 *    Stereographic.h  - Is for prototype error checking
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

const double PI = 3.14159265358979323e0;       /* PI     */
const double PI_OVER_2 = (PI / 2.0);
const double PI_OVER_4 = (PI / 4.0);
const double TWO_PI = (2.0 * PI);
const double ONE = (1.0 * PI / 180.0);         /* One degree */


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

Stereographic::Stereographic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing ) :
  Stereo_Ra( 6371007.1810824 ),             
  Two_Stereo_Ra( 12742014.3621648 ),        
  Stereo_At_Pole( 0 ),                        
  Stereo_Origin_Long( 0.0 ),                
  Stereo_Origin_Lat( 0.0 ),                 
  Stereo_False_Easting( 0.0 ),              
  Stereo_False_Northing( 0.0 ),             
  Sin_Stereo_Origin_Lat( 0.0 ),            
  Cos_Stereo_Origin_Lat( 1.0 ),             
  Stereo_Delta_Easting( 1460090226.0 ),
  Stereo_Delta_Northing( 1460090226.0 )
{
/*
 * The constructor receives the ellipsoid
 * parameters and Stereograpic projection parameters as inputs, and
 * sets the corresponding state variables.  If any errors occur, an 
 * exception is thrown with a description of the error.
 *
 *  ellipsoidSemiMajorAxis    : Semi-major axis of ellipsoid, in meters         (input)
 *  ellipsoidFlattening       : Flattening of ellipsoid                         (input)
 *  centralMeridian           : Longitude, in radians, at the center of         (input)
 *                              the projection
 *  originLatitude            : Latitude, in radians, at the center of          (input)
 *                              the projection
 *  falseEasting              : Easting (X) at center of projection, in meters  (input)
 *  falseNorthing             : Northing (Y) at center of projection, in meters (input)
 */

  double es2, es4, es6;
  double temp = 0;
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

  es2 = 2 * flattening - flattening * flattening;
  es4 = es2 * es2;
  es6 = es4 * es2;
  Stereo_Ra = semiMajorAxis *
     (1.0 - es2 / 6.0 - 17.0 * es4 / 360.0 - 67.0 * es6 /3024.0);
  Two_Stereo_Ra = 2.0 * Stereo_Ra;
  Stereo_Origin_Lat = originLatitude;
  Sin_Stereo_Origin_Lat = sin(Stereo_Origin_Lat);
  Cos_Stereo_Origin_Lat = cos(Stereo_Origin_Lat);
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Stereo_Origin_Long = centralMeridian;
  Stereo_False_Easting = falseEasting;
  Stereo_False_Northing = falseNorthing;
  if(fabs(fabs(Stereo_Origin_Lat) - PI_OVER_2) < 1.0e-10)
    Stereo_At_Pole = 1;
  else
    Stereo_At_Pole = 0;

  if ((Stereo_At_Pole) || (fabs(Stereo_Origin_Lat) < 1.0e-10))
  {
    Stereo_Delta_Easting = 1460090226.0;
  }
  else
  {
    MapProjectionCoordinates* tempCoordinates;
    if (Stereo_Origin_Long <= 0)
    {
      GeodeticCoordinates gcTemp( CoordinateType::geodetic, PI + Stereo_Origin_Long - ONE, -Stereo_Origin_Lat );
      tempCoordinates = convertFromGeodetic( &gcTemp );
    }
    else
    {
      GeodeticCoordinates gcTemp( CoordinateType::geodetic, Stereo_Origin_Long - PI - ONE, -Stereo_Origin_Lat );
      tempCoordinates = convertFromGeodetic( &gcTemp );
    }
    Stereo_Delta_Easting = tempCoordinates->easting();
    delete tempCoordinates;

    if(Stereo_False_Easting)
      Stereo_Delta_Easting -= Stereo_False_Easting;
    if (Stereo_Delta_Easting < 0)
      Stereo_Delta_Easting = -Stereo_Delta_Easting;
  }
}


Stereographic::Stereographic( const Stereographic &s )
{
  semiMajorAxis = s.semiMajorAxis;
  flattening = s.flattening;
  Stereo_Ra = s.Stereo_Ra;     
  Two_Stereo_Ra = s.Two_Stereo_Ra; 
  Stereo_At_Pole = s.Stereo_At_Pole; 
  Stereo_Origin_Long = s.Stereo_Origin_Long; 
  Stereo_Origin_Lat = s.Stereo_Origin_Lat; 
  Stereo_False_Easting = s.Stereo_False_Easting; 
  Stereo_False_Northing = s.Stereo_False_Northing; 
  Sin_Stereo_Origin_Lat = s.Sin_Stereo_Origin_Lat; 
  Cos_Stereo_Origin_Lat = s.Cos_Stereo_Origin_Lat; 
  Stereo_Delta_Easting = s.Stereo_Delta_Easting; 
  Stereo_Delta_Northing = s.Stereo_Delta_Northing; 
}


Stereographic::~Stereographic()
{
}


Stereographic& Stereographic::operator=( const Stereographic &s )
{
  if( this != &s )
  {
    semiMajorAxis = s.semiMajorAxis;
    flattening = s.flattening;
    Stereo_Ra = s.Stereo_Ra;     
    Two_Stereo_Ra = s.Two_Stereo_Ra; 
    Stereo_At_Pole = s.Stereo_At_Pole; 
    Stereo_Origin_Long = s.Stereo_Origin_Long; 
    Stereo_Origin_Lat = s.Stereo_Origin_Lat; 
    Stereo_False_Easting = s.Stereo_False_Easting; 
    Stereo_False_Northing = s.Stereo_False_Northing; 
    Sin_Stereo_Origin_Lat = s.Sin_Stereo_Origin_Lat; 
    Cos_Stereo_Origin_Lat = s.Cos_Stereo_Origin_Lat; 
    Stereo_Delta_Easting = s.Stereo_Delta_Easting; 
    Stereo_Delta_Northing = s.Stereo_Delta_Northing; 
  }

  return *this;
}


MapProjection4Parameters* Stereographic::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters and Stereographic projection parameters.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
 *    ellipsoidFlattening     : Flattening of ellipsoid                   (output)
 *    centralMeridian         : Longitude, in radians, at the center of   (output)
 *                              the projection
 *    originLatitude          : Latitude, in radians, at the center of    (output)
 *                              the projection
 *    falseEasting            : A coordinate value, in meters, assigned to the
 *                              central meridian of the projection.       (output)
 *    falseNorthing           : A coordinate value, in meters, assigned to the
 *                              origin latitude of the projection         (output)
 */

  return new MapProjection4Parameters( CoordinateType::stereographic, Stereo_Origin_Long, Stereo_Origin_Lat, Stereo_False_Easting, Stereo_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* Stereographic::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic
 * coordinates (latitude and longitude) to Stereographic coordinates
 * (easting and northing), according to the current ellipsoid
 * and Stereographic projection parameters. If any errors occur, 
 * an exception is thrown with a description of the error.
 *
 *    longitude  :  Longitude, in radians                     (input)
 *    latitude   :  Latitude, in radians                      (input)
 *    easting    :  Easting (X), in meters                    (output)
 *    northing   :  Northing (Y), in meters                   (output)
 */

  double g, k;
  double num = 0;
  double Ra_k = 0;
  double dlam;                        /* Longitude - Central Meridan */
  double cos_dlam;
  double easting, northing;

  double longitude = geodeticCoordinates->longitude();
  double latitude = geodeticCoordinates->latitude();
  double slat = sin(latitude);
  double clat = cos(latitude);

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  {  /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  {  /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  dlam = longitude - Stereo_Origin_Long;
  if (dlam > PI)
  {
    dlam -= TWO_PI;
  }
  if (dlam < -PI)
  {
    dlam += TWO_PI;
  }

  cos_dlam = cos(dlam);
  g = 1.0 + Sin_Stereo_Origin_Lat * slat + Cos_Stereo_Origin_Lat * clat * cos_dlam;
  if (fabs(g) <= 1.0e-10)
  {  /* Point is out of view.  Will return longitude out of range message
        since no point out of view is implemented.  */
    throw CoordinateConversionException( ErrorMessages::longitude );
  }
  else
  {
    if (Stereo_At_Pole)
    {
      if (fabs(fabs(latitude) - PI_OVER_2) < 1.0e-10)
      {
        easting = Stereo_False_Easting;
        northing = Stereo_False_Northing;
      }
      else
      {
        if (Stereo_Origin_Lat > 0)
        {
          num = Two_Stereo_Ra * tan(PI_OVER_4 - latitude / 2.0);
          easting = Stereo_False_Easting + num * sin(dlam);
          northing = Stereo_False_Northing + (-num * cos_dlam);
        }
        else
        {
          num = Two_Stereo_Ra * tan(PI_OVER_4 + latitude / 2.0);
          easting = Stereo_False_Easting + num * sin(dlam);
          northing = Stereo_False_Northing + num * cos_dlam;
        }
      }
    }
    else
    {
      if (fabs(Stereo_Origin_Lat) <= 1.0e-10)
      {
        k = 2.0 / (1.0 + clat * cos_dlam);
        Ra_k = Stereo_Ra * k;
        northing = Stereo_False_Northing + Ra_k * slat;
      }
      else
      {
        k = 2.0 / g;
        Ra_k = Stereo_Ra * k;
        northing = Stereo_False_Northing + Ra_k * (Cos_Stereo_Origin_Lat * slat - Sin_Stereo_Origin_Lat * clat * cos_dlam);
      }
      easting = Stereo_False_Easting + Ra_k * clat * sin(dlam);
    }
  }

  return new MapProjectionCoordinates( CoordinateType::stereographic, easting, northing );
}


MSP::CCS::GeodeticCoordinates* Stereographic::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Stereographic projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid and Stereographic projection
 * coordinates.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    easting           : Easting (X), in meters              (input)
 *    northing          : Northing (Y), in meters             (input)
 *    longitude         : Longitude (lambda), in radians      (output)
 *    latitude          : Latitude (phi), in radians          (output)
 */

  double dx, dy;
  double rho, c;
  double sin_c, cos_c;
  double dy_sin_c;
  double longitude, latitude;

  double easting = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Stereo_False_Easting - Stereo_Delta_Easting))
      ||(easting > (Stereo_False_Easting + Stereo_Delta_Easting)))
  { /* Easting out of range  */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < (Stereo_False_Northing - Stereo_Delta_Northing))
      || (northing > (Stereo_False_Northing + Stereo_Delta_Northing)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy = northing - Stereo_False_Northing;
  dx = easting - Stereo_False_Easting;
  rho = sqrt(dx * dx + dy * dy);
  if (fabs(rho) <= 1.0e-10)
  {
    latitude  = Stereo_Origin_Lat;
    longitude = Stereo_Origin_Long;
  }
  else
  {
    c = 2.0 * atan(rho / (Two_Stereo_Ra));
    sin_c = sin(c);
    cos_c = cos(c);
    dy_sin_c = dy * sin_c;
    if (Stereo_At_Pole)
    {
      if (Stereo_Origin_Lat > 0)
        longitude = Stereo_Origin_Long + atan2(dx, -dy);
      else
        longitude = Stereo_Origin_Long + atan2(dx, dy);
    }
    else
      longitude = Stereo_Origin_Long + atan2(dx * sin_c, (rho * Cos_Stereo_Origin_Lat * cos_c - dy_sin_c * Sin_Stereo_Origin_Lat));
    latitude = asin(cos_c * Sin_Stereo_Origin_Lat + ((dy_sin_c * Cos_Stereo_Origin_Lat) / rho));
  }

  if (fabs(latitude) < 2.2e-8)  /* force lat to 0 to avoid -0 degrees */
    latitude = 0.0;
  if (latitude > PI_OVER_2)  /* force distorted values to 90, -90 degrees */
    latitude = PI_OVER_2;
  else if (latitude < -PI_OVER_2)
    latitude = -PI_OVER_2;

  if (longitude > PI)
  {
    if (longitude - PI < 3.5e-6)
      longitude = PI;
    else
      longitude -= TWO_PI;
  }
  if (longitude < -PI)
  {
    if (fabs(longitude + PI) < 3.5e-6)
      longitude = -PI;
    else
      longitude += TWO_PI;
  }

  if (fabs(longitude) < 2.0e-7)  /* force lon to 0 to avoid -0 degrees */
    longitude = 0.0;
  if (longitude > PI)  /* force distorted values to 180, -180 degrees */
    longitude = PI;
  else if (longitude < -PI)
    longitude = -PI;

  return new GeodeticCoordinates( CoordinateType::geodetic, longitude, latitude );
}



// CLASSIFICATION: UNCLASSIFIED
