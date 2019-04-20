// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER:  LOCAL CARTESIAN
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates (latitude,
 *    longitude in radians and height in meters) and Local Cartesian coordinates 
 *    (X, Y, Z).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *      LOCCART_NO_ERROR            : No errors occurred in function
 *      LOCCART_LAT_ERROR           : Latitude out of valid range
 *                                      (-90 to 90 degrees)
 *      LOCCART_LON_ERROR           : Longitude out of valid range
 *                                      (-180 to 360 degrees)
 *      LOCCART_A_ERROR             : Semi-major axis less than or equal to zero
 *      LOCCART_INV_F_ERROR         : Inverse flattening outside of valid range
 *									                    (250 to 350)
 *      LOCCART_ORIGIN_LAT_ERROR    : Origin Latitude out of valid range
 *                                      (-90 to 90 degrees)
 *      LOCCART_ORIGIN_LON_ERROR    : Origin Longitude out of valid range
 *                                      (-180 to 360 degrees)
 *		  LOCCART_ORIENTATION_ERROR   : Orientation angle out of valid range
 *									                    (-360 to 360 degrees)
 *
 *
 * REUSE NOTES
 *
 *    LOCCART is intended for reuse by any application that performs
 *    coordinate conversions between geodetic coordinates or geocentric
 *    coordinates and local cartesian coordinates..
 *    
 *
 * REFERENCES
 *    
 *    Further information on LOCAL CARTESIAN can be found in the Reuse Manual.
 *
 *    LOCCART originated from : U.S. Army Topographic Engineering Center
 *                              Geospatial Inforamtion Division
 *                              7701 Telegraph Road
 *                              Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    LOCCART has no restrictions.
 *
 * ENVIRONMENT
 *
 *    LOCCART was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC version 2.8.1
 *    2. Windows 95 with MS Visual C++ version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *	  07-16-99			Original Code
 *	  03-2-07			Original C++ Code
 *    3/23/11           N. Lundgren BAEts28583 Updated for memory leaks in 
 *                      convertFromGeodetic and convertToGeodetic
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "Geocentric.h"
#include "LocalCartesian.h"
#include "LocalCartesianParameters.h"
#include "CartesianCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h    - Standard C math library
 *    Geocentric.h - Is needed to call the convertToGeocentric and
 *                    convertFromGeocentric functions
 *    LocalCartesian.h - Is for prototype error checking.
 *    CartesianCoordinates.h   - defines cartesian coordinates
 *    GeodeticCoordinates.h   - defines geodetic coordinates
 *    CoordinateConversionException.h - Exception handler
 *    ErrorMessages.h  - Contains exception messages
 */


using namespace MSP::CCS;


/***************************************************************************/
/*                               DEFINES 
 *
 */

const double PI = 3.14159265358979323e0;    /* PI */
const double PI_OVER_2 = ( PI / 2.0e0);                 
const double TWO_PI = (2.0 * PI);                    


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

LocalCartesian::LocalCartesian( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double originLongitude, double originLatitude, double originHeight, double orientation ) :
  CoordinateSystem(),
  geocentric( 0 ),        
  es2( 0.0066943799901413800 ),
  u0( 6378137.0 ),
  v0( 0.0 ),
  w0( 0.0 ),
  LocalCart_Origin_Long( 0.0 ),
  LocalCart_Origin_Lat( 0.0 ),
  LocalCart_Origin_Height( 0.0 ),
  LocalCart_Orientation( 0.0 ),
  Sin_LocalCart_Origin_Lat( 0.0 ),
  Cos_LocalCart_Origin_Lat( 1.0 ),
  Sin_LocalCart_Origin_Lon( 0.0 ),
  Cos_LocalCart_Origin_Lon( 1.0 ),
  Sin_LocalCart_Orientation( 0.0 ),
  Cos_LocalCart_Orientation( 1.0 ),
  Sin_Lat_Sin_Orient( 0.0 ),
  Sin_Lat_Cos_Orient( 0.0 )
{
/*
 * The constructor receives the ellipsoid parameters
 * and local origin parameters as inputs and sets the corresponding state variables.
 *
 *    ellipsoidSemiMajorAxis : Semi-major axis of ellipsoid, in meters   (input)
 *    ellipsoidFlattening    : Flattening of ellipsoid                   (input)
 *    originLongitude        : Longitude of the local origin, in radians (input)
 *    originLatitude         : Latitude of the local origin, in radians  (input)
 *    originHeight   : Ellipsoid height of the local origin, in meters   (input)
 *    orientation    : Orientation angle of the local cartesian coordinate system,
 *                               in radians (input)
 */

  double N0;
  double inv_f = 1 / ellipsoidFlattening;
  double val;

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
  if ((originLongitude < -PI) || (originLongitude > TWO_PI))
  { /* origin longitude out of range */
    throw CoordinateConversionException( ErrorMessages::originLongitude  );
  }
  if ((orientation < -PI) || (orientation > TWO_PI))
  { /* orientation angle out of range */
    throw CoordinateConversionException( ErrorMessages::orientation  );
  }

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening = ellipsoidFlattening;

  LocalCart_Origin_Lat = originLatitude;
  if (originLongitude > PI)
    originLongitude -= TWO_PI;
  LocalCart_Origin_Long = originLongitude;
  LocalCart_Origin_Height = originHeight;
  if (orientation > PI)
    orientation -= TWO_PI;
  LocalCart_Orientation = orientation;
  es2 = 2 * flattening - flattening * flattening;

  Sin_LocalCart_Origin_Lat = sin(LocalCart_Origin_Lat);
  Cos_LocalCart_Origin_Lat = cos(LocalCart_Origin_Lat);
  Sin_LocalCart_Origin_Lon = sin(LocalCart_Origin_Long);
  Cos_LocalCart_Origin_Lon = cos(LocalCart_Origin_Long);
  Sin_LocalCart_Orientation = sin(LocalCart_Orientation);
  Cos_LocalCart_Orientation = cos(LocalCart_Orientation);

  Sin_Lat_Sin_Orient = Sin_LocalCart_Origin_Lat * Sin_LocalCart_Orientation;
  Sin_Lat_Cos_Orient = Sin_LocalCart_Origin_Lat * Cos_LocalCart_Orientation;

  N0 = semiMajorAxis / sqrt(1 - es2 * Sin_LocalCart_Origin_Lat * Sin_LocalCart_Origin_Lat);
 
  val = (N0 + LocalCart_Origin_Height) * Cos_LocalCart_Origin_Lat;
  u0 = val * Cos_LocalCart_Origin_Lon;
  v0 = val * Sin_LocalCart_Origin_Lon;
  w0 = ((N0 * (1 - es2)) + LocalCart_Origin_Height) * Sin_LocalCart_Origin_Lat;

  geocentric = new Geocentric( semiMajorAxis, flattening );
}


LocalCartesian::LocalCartesian( const LocalCartesian &lc )
{
  geocentric = new Geocentric( *( lc.geocentric ) );
  semiMajorAxis = lc.semiMajorAxis;
  flattening = lc.flattening;
  es2 = lc.es2;     
  u0 = lc.u0; 
  v0 = lc.v0; 
  w0 = lc.w0; 
  LocalCart_Origin_Long = lc.LocalCart_Origin_Long; 
  LocalCart_Origin_Lat = lc.LocalCart_Origin_Lat; 
  LocalCart_Origin_Height = lc.LocalCart_Origin_Height; 
  LocalCart_Orientation = lc.LocalCart_Orientation; 
  Sin_LocalCart_Origin_Lat = lc.Sin_LocalCart_Origin_Lat; 
  Cos_LocalCart_Origin_Lat = lc.Cos_LocalCart_Origin_Lat; 
  Sin_LocalCart_Origin_Lon = lc.Sin_LocalCart_Origin_Lon; 
  Cos_LocalCart_Origin_Lon = lc.Cos_LocalCart_Origin_Lon; 
  Sin_LocalCart_Orientation = lc.Sin_LocalCart_Orientation; 
  Cos_LocalCart_Orientation = lc.Cos_LocalCart_Orientation; 
  Sin_Lat_Sin_Orient = lc.Sin_Lat_Sin_Orient; 
  Sin_Lat_Cos_Orient = lc.Sin_Lat_Cos_Orient; 
}


LocalCartesian::~LocalCartesian()
{
  delete geocentric;
  geocentric = 0;
}


LocalCartesian& LocalCartesian::operator=( const LocalCartesian &lc )
{
  if( this != &lc )
  {
    geocentric->operator=( *lc.geocentric );
    semiMajorAxis = lc.semiMajorAxis;
    flattening = lc.flattening;
    es2 = lc.es2;     
    u0 = lc.u0; 
    v0 = lc.v0; 
    w0 = lc.w0; 
    LocalCart_Origin_Long = lc.LocalCart_Origin_Long; 
    LocalCart_Origin_Lat = lc.LocalCart_Origin_Lat; 
    LocalCart_Origin_Height = lc.LocalCart_Origin_Height; 
    LocalCart_Orientation = lc.LocalCart_Orientation; 
    Sin_LocalCart_Origin_Lat = lc.Sin_LocalCart_Origin_Lat; 
    Cos_LocalCart_Origin_Lat = lc.Cos_LocalCart_Origin_Lat; 
    Sin_LocalCart_Origin_Lon = lc.Sin_LocalCart_Origin_Lon; 
    Cos_LocalCart_Origin_Lon = lc.Cos_LocalCart_Origin_Lon; 
    Sin_LocalCart_Orientation = lc.Sin_LocalCart_Orientation; 
    Cos_LocalCart_Orientation = lc.Cos_LocalCart_Orientation; 
    Sin_Lat_Sin_Orient = lc.Sin_Lat_Sin_Orient; 
    Sin_Lat_Cos_Orient = lc.Sin_Lat_Cos_Orient; 
  }

  return *this;
}


LocalCartesianParameters* LocalCartesian::getParameters() const
{
/*
 * The function getParameters returns the ellipsoid parameters
 * and local origin parameters.
 *
 *    ellipsoidSemiMajorAxis    : Semi-major axis of ellipsoid, in meters           (output)
 *    ellipsoidFlattening       : Flattening of ellipsoid					                  (output)
 *    originLongitude           : Longitude of the local origin, in radians         (output)
 *    originLatitude            : Latitude of the local origin, in radians          (output)
 *    originHeight              : Ellipsoid height of the local origin, in meters   (output)
 *    orientation               : Orientation angle of the local cartesian coordinate system,
 *                                in radians                                        (output)
 */

  return new LocalCartesianParameters( CoordinateType::localCartesian, LocalCart_Origin_Long, LocalCart_Origin_Lat, LocalCart_Origin_Height, LocalCart_Orientation );
}


MSP::CCS::CartesianCoordinates* LocalCartesian::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic coordinates
 * (latitude, longitude, and height) to local cartesian coordinates (X, Y, Z),
 * according to the current ellipsoid and local origin parameters.
 *
 *    longitude : Geodetic longitude, in radians                       (input)
 *    latitude  : Geodetic latitude, in radians                        (input)
 *    Height    : Geodetic height, in meters                           (input)
 *    X         : Calculated local cartesian X coordinate, in meters   (output)
 *    Y         : Calculated local cartesian Y coordinate, in meters   (output)
 *    Z         : Calculated local cartesian Z coordinate, in meters   (output)
 *
 */

  double longitude = geodeticCoordinates->longitude();
  double latitude = geodeticCoordinates->latitude();
  double height = geodeticCoordinates->height();

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  { /* geodetic latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  { /* geodetic longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  CartesianCoordinates* geocentricCoordinates = 0;
  CartesianCoordinates* cartesianCoordinates = 0;
  try
  {
     geocentricCoordinates = geocentric->convertFromGeodetic(
        geodeticCoordinates );
     cartesianCoordinates = convertFromGeocentric( geocentricCoordinates );
     delete geocentricCoordinates;
  }
  catch ( CoordinateConversionException e )
  {
     delete geocentricCoordinates;
     delete cartesianCoordinates;
     throw e;
  }

  return cartesianCoordinates;
}


MSP::CCS::GeodeticCoordinates* LocalCartesian::convertToGeodetic(
   MSP::CCS::CartesianCoordinates* cartesianCoordinates )
{
/*
 * The function convertToGeodetic converts local cartesian
 * coordinates (X, Y, Z) to geodetic coordinates (latitude, longitude, 
 * and height), according to the current ellipsoid and local origin parameters.
 *
 *    X         : Local cartesian X coordinate, in meters    (input)
 *    Y         : Local cartesian Y coordinate, in meters    (input)
 *    Z         : Local cartesian Z coordinate, in meters    (input)
 *    longitude : Calculated longitude value, in radians     (output)
 *    latitude  : Calculated latitude value, in radians      (output)
 *    Height    : Calculated height value, in meters         (output)
 */
  CartesianCoordinates* geocentricCoordinates = 0;
  GeodeticCoordinates*  geodeticCoordinates   = 0;
  try {
     geocentricCoordinates = convertToGeocentric( cartesianCoordinates );
     geodeticCoordinates   = geocentric->convertToGeodetic(
        geocentricCoordinates );

     double longitude = geodeticCoordinates->longitude();

     if (longitude > PI)
        geodeticCoordinates->setLongitude( longitude -= TWO_PI );

     longitude = geodeticCoordinates->longitude();

     if (longitude < -PI)
        geodeticCoordinates->setLongitude( longitude += TWO_PI );

     delete geocentricCoordinates;
  }
  catch ( CoordinateConversionException e )
  {
     delete geocentricCoordinates;
     delete geodeticCoordinates;
     throw e;
  }

  return geodeticCoordinates;
}


MSP::CCS::CartesianCoordinates* LocalCartesian::convertFromGeocentric(
   const MSP::CCS::CartesianCoordinates* cartesianCoordinates )
{
/*
 * The function convertFromGeocentric converts geocentric
 * coordinates according to the current ellipsoid and local origin parameters.
 *
 *    U         : Geocentric latitude, in meters                       (input)
 *    V         : Geocentric longitude, in meters                      (input)
 *    W         : Geocentric height, in meters                         (input)
 *    X         : Calculated local cartesian X coordinate, in meters   (output)
 *    Y         : Calculated local cartesian Y coordinate, in meters   (output)
 *    Z         : Calculated local cartesian Z coordinate, in meters   (output)
 *
 */

  double X, Y, Z;
  double u_MINUS_u0, v_MINUS_v0, w_MINUS_w0;

  double U = cartesianCoordinates->x();
  double V = cartesianCoordinates->y();
  double W = cartesianCoordinates->z();

  u_MINUS_u0 = U - u0;
  v_MINUS_v0 = V - v0;
  w_MINUS_w0 = W - w0;

  if (LocalCart_Orientation == 0.0)
  {
    double cos_lon_u_MINUS_u0 = Cos_LocalCart_Origin_Lon * u_MINUS_u0;
    double sin_lon_v_MINUS_v0 = Sin_LocalCart_Origin_Lon * v_MINUS_v0;

    X = -Sin_LocalCart_Origin_Lon * u_MINUS_u0 + Cos_LocalCart_Origin_Lon * v_MINUS_v0;
    Y = -Sin_LocalCart_Origin_Lat * cos_lon_u_MINUS_u0 + -Sin_LocalCart_Origin_Lat * sin_lon_v_MINUS_v0 + Cos_LocalCart_Origin_Lat * w_MINUS_w0;
    Z = Cos_LocalCart_Origin_Lat * cos_lon_u_MINUS_u0 + Cos_LocalCart_Origin_Lat * sin_lon_v_MINUS_v0 + Sin_LocalCart_Origin_Lat * w_MINUS_w0;
  }
  else
  {
    double cos_lat_w_MINUS_w0 = Cos_LocalCart_Origin_Lat * w_MINUS_w0;

    X = (-Cos_LocalCart_Orientation * Sin_LocalCart_Origin_Lon + Sin_Lat_Sin_Orient * Cos_LocalCart_Origin_Lon) * u_MINUS_u0 +
         (Cos_LocalCart_Orientation * Cos_LocalCart_Origin_Lon + Sin_Lat_Sin_Orient * Sin_LocalCart_Origin_Lon) * v_MINUS_v0 +
         (-Sin_LocalCart_Orientation * cos_lat_w_MINUS_w0);

    Y = (-Sin_LocalCart_Orientation * Sin_LocalCart_Origin_Lon - Sin_Lat_Cos_Orient * Cos_LocalCart_Origin_Lon) * u_MINUS_u0 +
         (Sin_LocalCart_Orientation * Cos_LocalCart_Origin_Lon - Sin_Lat_Cos_Orient * Sin_LocalCart_Origin_Lon) * v_MINUS_v0 +
         (Cos_LocalCart_Orientation * cos_lat_w_MINUS_w0);

    Z = (Cos_LocalCart_Origin_Lat * Cos_LocalCart_Origin_Lon) * u_MINUS_u0 +
         (Cos_LocalCart_Origin_Lat * Sin_LocalCart_Origin_Lon) * v_MINUS_v0 +
         Sin_LocalCart_Origin_Lat * w_MINUS_w0;
  }

  return new CartesianCoordinates( CoordinateType::localCartesian, X, Y, Z );
}


MSP::CCS::CartesianCoordinates* LocalCartesian::convertToGeocentric( const MSP::CCS::CartesianCoordinates* cartesianCoordinates )
{
/*
 * The function Convert_Local_Cartesian_To_Geocentric converts local cartesian
 * coordinates (x, y, z) to geocentric coordinates (X, Y, Z) according to the 
 * current ellipsoid and local origin parameters.
 *
 *    X         : Local cartesian X coordinate, in meters    (input)
 *    Y         : Local cartesian Y coordinate, in meters    (input)
 *    Z         : Local cartesian Z coordinate, in meters    (input)
 *    U         : Calculated U value, in meters              (output)
 *    V         : Calculated v value, in meters              (output)
 *    W         : Calculated w value, in meters              (output)
 */

  double U, V, W;

  double X = cartesianCoordinates->x();
  double Y = cartesianCoordinates->y(); 
  double Z = cartesianCoordinates->z();

  if (LocalCart_Orientation == 0.0)
  {
    double sin_lat_y = Sin_LocalCart_Origin_Lat * Y;
    double cos_lat_z = Cos_LocalCart_Origin_Lat * Z;

    U = -Sin_LocalCart_Origin_Lon * X - sin_lat_y * Cos_LocalCart_Origin_Lon + cos_lat_z * Cos_LocalCart_Origin_Lon + u0;
    V = Cos_LocalCart_Origin_Lon * X -  sin_lat_y * Sin_LocalCart_Origin_Lon + cos_lat_z * Sin_LocalCart_Origin_Lon + v0;
    W = Cos_LocalCart_Origin_Lat * Y + Sin_LocalCart_Origin_Lat * Z + w0;
  }
  else
  {
    double rotated_x, rotated_y;
    double rotated_y_sin_lat, z_cos_lat;

    rotated_x = Cos_LocalCart_Orientation * X + Sin_LocalCart_Orientation * Y;
    rotated_y = -Sin_LocalCart_Orientation * X + Cos_LocalCart_Orientation * Y;

    rotated_y_sin_lat = rotated_y * Sin_LocalCart_Origin_Lat;
    z_cos_lat = Z * Cos_LocalCart_Origin_Lat;

    U = -Sin_LocalCart_Origin_Lon * rotated_x - Cos_LocalCart_Origin_Lon * rotated_y_sin_lat + Cos_LocalCart_Origin_Lon * z_cos_lat + u0;
    V = Cos_LocalCart_Origin_Lon * rotated_x -  Sin_LocalCart_Origin_Lon * rotated_y_sin_lat + Sin_LocalCart_Origin_Lon * z_cos_lat + v0;
    W = Cos_LocalCart_Origin_Lat * rotated_y + Sin_LocalCart_Origin_Lat * Z + w0;
  }

  return new CartesianCoordinates( CoordinateType::geocentric, U, V, W );
}

// CLASSIFICATION: UNCLASSIFIED
