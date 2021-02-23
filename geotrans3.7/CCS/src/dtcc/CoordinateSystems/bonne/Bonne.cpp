// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: BONNE
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Bonne projection coordinates
 *    (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          BONN_NO_ERROR           : No errors occurred in function
 *          BONN_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          BONN_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          BONN_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~20,500,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          BONN_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~23,500,000 m,
 *                                       depending on ellipsoid parameters
 *                                       and Origin_Latitude)
 *          BONN_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          BONN_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          BONN_A_ERROR            : Semi-major axis less than or equal to zero
 *          BONN_INV_F_ERROR        : Inverse flattening outside of valid range
 *									                    (250 to 350)
 *
 * REUSE NOTES
 *
 *    BONNE is intended for reuse by any application that performs a
 *    Bonne projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on BONNE can be found in the Reuse Manual.
 *
 *    BONNE originated from :  U.S. Army Topographic Engineering Center
 *                             Geospatial Information Division
 *                             7701 Telegraph Road
 *                             Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    BONNE has no restrictions.
 *
 * ENVIRONMENT
 *
 *    BONNE was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC 2.8.1
 *    2. MS Windows 95 with MS Visual C++ 6
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
#include "Bonne.h"
#include "Sinusoidal.h"
#include "MapProjection4Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h  - Is needed to call the math functions (sqrt, pow, exp, log,
 *                 sin, cos, tan, and atan).
 *    Bonne.h - Is for prototype error checking.
 *    Sinusoidal.h - Is called when the origin latitude is zero.
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
const double PI_OVER_2 = (PI / 2.0);                 
const double TWO_PI = (2.0 * PI);

                 
double bonnCoeffTimesSine( double coeff, double x, double latit )
{
  return coeff * (sin(x * latit));
}


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

Bonne::Bonne( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
  sinusoidal( 0 ),        
  es2( 0.0066943799901413800 ),        
  es4( 4.4814723452405e-005 ),
  es6( 3.0000678794350e-007 ),
  M1( 4984944.3782319 ),
  m1( .70829317069372 ),
  c0( .99832429845280 ),
  c1( .0025146070605187 ),
  c2( 2.6390465943377e-006 ),
  c3( 3.4180460865959e-009 ),
  a0( .0025188265843907 ),
  a1( 3.7009490356205e-006 ),
  a2( 7.4478137675038e-009 ),
  a3( 1.7035993238596e-011 ),
  Bonn_Origin_Long( 0.0 ),
  Bonn_Origin_Lat( ((45 * PI) / 180.0) ),
  Bonn_False_Easting( 0.0 ),
  Bonn_False_Northing( 0.0 ),
  Sin_Bonn_Origin_Lat( .70710678118655 ), 
  Bonn_am1sin( 6388838.2901211 ),
  Bonn_Max_Easting( 20027474.0 ),
  Bonn_Min_Easting( -20027474.0 ),
  Bonn_Delta_Northing( 20003932.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * Bonne projection parameters as inputs, and sets the corresponding state
 * variables. If any errors occur, an exception is thrown with a description 
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

  double j, three_es4;
  double x,e1,e2,e3,e4;
  double clat; 
  double sin2lat, sin4lat, sin6lat, lat;
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

  Bonn_Origin_Lat = originLatitude;
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Bonn_Origin_Long = centralMeridian;
  Bonn_False_Northing = falseNorthing;
  Bonn_False_Easting = falseEasting;
  if (Bonn_Origin_Lat == 0.0)
	{
		if (Bonn_Origin_Long > 0)
		{
			Bonn_Max_Easting = 19926189.0;
			Bonn_Min_Easting = -20037509.0;
		}
		else if (Bonn_Origin_Long < 0)
		{
			Bonn_Max_Easting = 20037509.0;
			Bonn_Min_Easting = -19926189.0;
		}
		else
		{
			Bonn_Max_Easting = 20037509.0;
			Bonn_Min_Easting = -20037509.0;
		}
		Bonn_Delta_Northing = 10001966.0;

    sinusoidal = new Sinusoidal( semiMajorAxis, flattening, Bonn_Origin_Long, Bonn_False_Easting, Bonn_False_Northing );
	}
	else
	{
		Sin_Bonn_Origin_Lat = sin(Bonn_Origin_Lat);

		es2 = 2 * flattening - flattening * flattening;
		es4 = es2 * es2;
		es6 = es4 * es2;
		j = 45.0 * es6 / 1024.0;
		three_es4 = 3.0 * es4;
		c0 = 1 - es2 / 4.0 - three_es4 / 64.0 - 5.0 * es6 / 256.0;
		c1 = 3.0 * es2 / 8.0 + three_es4 / 32.0 + j;
		c2 = 15.0 * es4 / 256.0 + j;
		c3 = 35.0 * es6 / 3072.0;

		clat = cos(Bonn_Origin_Lat);
		m1 = bonnm(clat, Sin_Bonn_Origin_Lat);

		lat = c0 * Bonn_Origin_Lat;
		sin2lat = bonnCoeffTimesSine(c1, 2.0, Bonn_Origin_Lat);
		sin4lat = bonnCoeffTimesSine(c2, 4.0, Bonn_Origin_Lat);
		sin6lat = bonnCoeffTimesSine(c3, 6.0, Bonn_Origin_Lat);
		M1 = bonnM(lat, sin2lat, sin4lat, sin6lat);

		x = sqrt (1.0 - es2);
		e1 = (1.0 - x) / (1.0 + x);
		e2 = e1 * e1;
		e3 = e2 * e1;
		e4 = e3 * e1;
		a0 = 3.0 * e1 / 2.0 - 27.0 * e3 / 32.0;
		a1 = 21.0 * e2 / 16.0 - 55.0 * e4 / 32.0;
		a2 = 151.0 * e3 / 96.0;
		a3 = 1097.0 * e4 / 512.0;
		if (Sin_Bonn_Origin_Lat == 0.0)
			Bonn_am1sin = 0.0;
		else
			Bonn_am1sin = semiMajorAxis * m1 / Sin_Bonn_Origin_Lat;

		Bonn_Max_Easting = 20027474.0;
		Bonn_Min_Easting = -20027474.0;
		Bonn_Delta_Northing = 20003932.0;
	}
}


Bonne::Bonne( const Bonne &b )
{
  sinusoidal = new Sinusoidal( *( b.sinusoidal ) );
  semiMajorAxis = b.semiMajorAxis;
  flattening = b.flattening;
  es2 = b.es2;     
  es4 = b.es4; 
  es6 = b.es6; 
  M1 = b.M1; 
  m1 = b.m1; 
  c0 = b.c0; 
  c1 = b.c1; 
  c2 = b.c2; 
  c3 = b.c3; 
  a0 = b.a0; 
  a1 = b.a1; 
  a2 = b.a2; 
  a3 = b.a3; 
  Bonn_Origin_Long = b.Bonn_Origin_Long; 
  Bonn_Origin_Lat = b.Bonn_Origin_Lat; 
  Bonn_False_Easting = b.Bonn_False_Easting; 
  Bonn_False_Northing = b.Bonn_False_Northing; 
  Sin_Bonn_Origin_Lat = b.Sin_Bonn_Origin_Lat; 
  Bonn_am1sin = b.Bonn_am1sin; 
  Bonn_Max_Easting = b.Bonn_Max_Easting; 
  Bonn_Min_Easting = b.Bonn_Min_Easting; 
  Bonn_Delta_Northing = b.Bonn_Delta_Northing; 
}


Bonne::~Bonne()
{
  if( sinusoidal )
  {
    delete sinusoidal;
    sinusoidal = 0;
  }
}


Bonne& Bonne::operator=( const Bonne &b )
{
  if( this != &b )
  {
    sinusoidal->operator=( *b.sinusoidal );
    semiMajorAxis = b.semiMajorAxis;
    flattening = b.flattening;
    es2 = b.es2;     
    es4 = b.es4; 
    es6 = b.es6; 
    M1 = b.M1; 
    m1 = b.m1; 
    c0 = b.c0; 
    c1 = b.c1; 
    c2 = b.c2; 
    c3 = b.c3; 
    a0 = b.a0; 
    a1 = b.a1; 
    a2 = b.a2; 
    a3 = b.a3; 
    Bonn_Origin_Long = b.Bonn_Origin_Long; 
    Bonn_Origin_Lat = b.Bonn_Origin_Lat; 
    Bonn_False_Easting = b.Bonn_False_Easting; 
    Bonn_False_Northing = b.Bonn_False_Northing; 
    Sin_Bonn_Origin_Lat = b.Sin_Bonn_Origin_Lat; 
    Bonn_am1sin = b.Bonn_am1sin; 
    Bonn_Max_Easting = b.Bonn_Max_Easting; 
    Bonn_Min_Easting = b.Bonn_Min_Easting; 
    Bonn_Delta_Northing = b.Bonn_Delta_Northing; 
  }

  return *this;
}


MapProjection4Parameters* Bonne::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters and Bonne projection parameters.
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

  return new MapProjection4Parameters( CoordinateType::bonne, Bonn_Origin_Long, Bonn_Origin_Lat, Bonn_False_Easting, Bonn_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* Bonne::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Bonne projection (easting and northing)
 * coordinates, according to the current ellipsoid and Bonne projection
 * parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda) in radians       (input)
 *    latitude          : Latitude (phi) in radians           (input)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  double dlam; /* Longitude - Central Meridan */
  double mm;
  double MM;
  double rho;
  double EE;
  double lat, sin2lat, sin4lat, sin6lat;
  double easting, northing;

  double longitude = geodeticCoordinates->longitude();
  double latitude = geodeticCoordinates->latitude();
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

  if (Bonn_Origin_Lat == 0.0)
     return sinusoidal->convertFromGeodetic( geodeticCoordinates );
  else
  {
     dlam = longitude - Bonn_Origin_Long;
     if (dlam > PI)
     {
        dlam -= TWO_PI;
     }
     if (dlam < -PI)
     {
        dlam += TWO_PI;
     }
     if ((latitude - Bonn_Origin_Lat) == 0.0 && floatEq(fabs(latitude),PI_OVER_2,.00001))
     {
        easting = 0.0;
        northing = 0.0;
     }
     else
     {
        mm = bonnm(clat, slat);
        lat = c0 * latitude;
        sin2lat = bonnCoeffTimesSine(c1, 2.0, latitude);
        sin4lat = bonnCoeffTimesSine(c2, 4.0, latitude);
        sin6lat = bonnCoeffTimesSine(c3, 6.0, latitude);
        MM = bonnM(lat, sin2lat, sin4lat, sin6lat);         

        rho = Bonn_am1sin + M1 - MM;
        if (rho == 0)
           EE = 0;
        else
           EE = semiMajorAxis * mm * dlam / rho;
        easting  = rho * sin(EE) + Bonn_False_Easting;
        northing = Bonn_am1sin - rho * cos(EE) + Bonn_False_Northing;
     }
     
     return new MapProjectionCoordinates(
        CoordinateType::bonne, easting, northing );
  }
}


MSP::CCS::GeodeticCoordinates* Bonne::convertToGeodetic(
   MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Bonne projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid and Bonne projection
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
  double mu;
  double MM;
  double mm;
  double am1sin_dy;
  double rho;
  double sin2mu, sin4mu, sin6mu, sin8mu;
  double clat, slat;
  double longitude, latitude;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Bonn_False_Easting + Bonn_Min_Easting))
      || (easting > (Bonn_False_Easting + Bonn_Max_Easting)))
  { /* Easting out of range */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < (Bonn_False_Northing - Bonn_Delta_Northing))
      || (northing > (Bonn_False_Northing + Bonn_Delta_Northing)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  if (Bonn_Origin_Lat == 0.0)
     return sinusoidal->convertToGeodetic( mapProjectionCoordinates );
  else
  {
     dy = northing - Bonn_False_Northing;
     dx = easting - Bonn_False_Easting;
     am1sin_dy = Bonn_am1sin - dy;
     rho = sqrt(dx * dx + am1sin_dy * am1sin_dy);
     if (Bonn_Origin_Lat < 0.0)
        rho = -rho;
     MM = Bonn_am1sin + M1 - rho;
     
     mu = MM / (semiMajorAxis * c0); 
     sin2mu = bonnCoeffTimesSine(a0, 2.0, mu);
     sin4mu = bonnCoeffTimesSine(a1, 4.0, mu);
     sin6mu = bonnCoeffTimesSine(a2, 6.0, mu);
     sin8mu = bonnCoeffTimesSine(a3, 8.0, mu);
     latitude = mu + sin2mu + sin4mu + sin6mu + sin8mu;

     if (floatEq(fabs(latitude),PI_OVER_2,.00001))
     {
        longitude = Bonn_Origin_Long;
     }
     else
     {
        clat = cos(latitude);
        slat = sin(latitude);
        mm = bonnm(clat, slat);

        if (Bonn_Origin_Lat < 0.0)
        {
           dx = -dx;
           am1sin_dy = -am1sin_dy;
        }
        longitude = Bonn_Origin_Long + rho * (atan2(dx, am1sin_dy)) /
           (semiMajorAxis * mm);
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
}


double Bonne::bonnm( double coslat, double sinlat )
{
  return coslat/sqrt(1.0 - es2*sinlat*sinlat);
}


double Bonne::bonnM( double c0lat, double c1s2lat, double c2s4lat, double c3s6lat )
{
  return semiMajorAxis*(c0lat-c1s2lat+c2s4lat-c3s6lat);
}


double Bonne::floatEq( double x, double v, double epsilon )
{
  return ((v - epsilon) < x) && (x < (v + epsilon));
}



// CLASSIFICATION: UNCLASSIFIED
