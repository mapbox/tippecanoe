// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: NEYS
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Ney's (Modified Lambert
 *    Conformal Conic) projection coordinates (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       NEYS_NO_ERROR           : No errors occurred in function
 *       NEYS_LAT_ERROR          : Latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *       NEYS_LON_ERROR          : Longitude outside of valid range
 *                                     (-180 to 360 degrees)
 *       NEYS_EASTING_ERROR      : Easting outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       NEYS_NORTHING_ERROR     : Northing outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       NEYS_FIRST_STDP_ERROR   : First standard parallel outside of valid
 *                                     range (71 or 74 degrees)
 *       NEYS_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                     (-89 59 59.0 to 89 59 59.0 degrees)
 *       NEYS_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                     (-180 to 360 degrees)
 *       NEYS_A_ERROR            : Semi-major axis less than or equal to zero
 *       NEYS_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                     (250 to 350)
 *
 *
 * REUSE NOTES
 *
 *    NEYS is intended for reuse by any application that performs a Ney's (Modified
 *    Lambert Conformal Conic) projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on NEYS can be found in the Reuse Manual.
 *
 *    NEYS originated from:
 *                      U.S. Army Topographic Engineering Center
 *                      Geospatial Information Division
 *                      7701 Telegraph Road
 *                      Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    NEYS has no restrictions.
 *
 * ENVIRONMENT
 *
 *    NEYS was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    8-4-00            Original Code
 *    3-2-07            Original C++ Code
 *
 *
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "LambertConformalConic.h"
#include "Neys.h"
#include "NeysParameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h     - Standard C math library
 *    LambertConformalConic2.h  - Is used to convert lambert conformal conic coordinates
 *    Neys.h     - Is for prototype error checking
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

const double PI = 3.14159265358979323e0;                 /* PI     */
const double PI_OVER_2 = (PI / 2.0);
const double PI_OVER_180 = (PI / 180.0);
const double TWO_PI = (2.0 * PI);
const double SEVENTY_ONE = (71.0 * PI_OVER_180);          /* 71 degrees     */
const double SEVENTY_FOUR = (74.0 * PI_OVER_180);         /* 74 degrees     */
////const double MAX_LAT = (89.99972222222222 * PI / 180.0); /* 89 59 59.0 degrees     */
const double MAX_LAT = (89.999444444444444 * PI_OVER_180); /* 89 59 58.0 degrees     */


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

Neys::Neys( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double standardParallel, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
  lambertConformalConic2( 0 ),
  Neys_Std_Parallel_1( SEVENTY_ONE ),
  Neys_Std_Parallel_2( MAX_LAT ),
  Neys_Origin_Long( 0.0 ),
  Neys_Origin_Lat( 80.0 * PI_OVER_180 ),
  Neys_False_Easting( 0.0 ),
  Neys_False_Northing( 0.0 ),
  Neys_Delta_Easting( 400000000.0 ),
  Neys_Delta_Northing( 400000000.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * Ney's (Modified Lambert Conformal Conic) projection parameters as inputs, and sets the
 * corresponding state variables.  If any errors occur, an exception is thrown 
 * with a description of the error.
 *
 *   ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
 *   ellipsoidFlattening     : Flattening of ellipsoid                   (input)
 *   centralMeridian         : Longitude of origin, in radians           (input)
 *   originLatitude          : Latitude of origin, in radians            (input)
 *   standardParallel        : First standard parallel, in radians       (input)
 *   falseEasting            : False easting, in meters                  (input)
 *   falseNorthing           : False northing, in meters                 (input)
 */

  double epsilon = 1.0e-10;
  double inv_f = 1 / ellipsoidFlattening;

  if (ellipsoidSemiMajorAxis <= 0.0)
  { /* Semi-major axis must be greater than zero */
    throw CoordinateConversionException( ErrorMessages::semiMajorAxis  );
  }
  if ((inv_f < 250) || (inv_f > 350))
  { /* Inverse flattening must be between 250 and 350 */
    throw CoordinateConversionException( ErrorMessages::ellipsoidFlattening  );

  }
  if ((originLatitude < -MAX_LAT) || (originLatitude > MAX_LAT))
  { /* Origin Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::originLatitude  );
  }
  if ((fabs(standardParallel - SEVENTY_ONE) > epsilon) && (fabs(standardParallel - SEVENTY_FOUR) > epsilon))
  { /* First Standard Parallel invalid */
    throw CoordinateConversionException( ErrorMessages::standardParallel1  );
  }
  if ((centralMeridian < -PI) || (centralMeridian > TWO_PI))
  { /* Origin Longitude out of range */
    throw CoordinateConversionException( ErrorMessages:: centralMeridian );
  }

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening = ellipsoidFlattening;

  Neys_Origin_Lat = originLatitude;
  if (Neys_Origin_Lat >= 0)
  {
    Neys_Std_Parallel_1 = standardParallel;
    Neys_Std_Parallel_2 = MAX_LAT;
  }
  else
  {
    Neys_Std_Parallel_1 = -standardParallel;
    Neys_Std_Parallel_2 = -MAX_LAT;
  }

  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Neys_Origin_Long = centralMeridian;
  Neys_False_Easting = falseEasting;
  Neys_False_Northing = falseNorthing;

  lambertConformalConic2 = new LambertConformalConic( semiMajorAxis, flattening, Neys_Origin_Long, Neys_Origin_Lat,
                                                      Neys_Std_Parallel_1, Neys_Std_Parallel_2,
                                                      Neys_False_Easting, Neys_False_Northing );
}


Neys::Neys( const Neys &n )
{
  lambertConformalConic2 = new LambertConformalConic( *( n.lambertConformalConic2 ) );
  semiMajorAxis = n.semiMajorAxis;
  flattening = n.flattening;
  Neys_Std_Parallel_1 = n.Neys_Std_Parallel_1;     
  Neys_Std_Parallel_2 = n.Neys_Std_Parallel_2; 
  Neys_Origin_Long = n.Neys_Origin_Long; 
  Neys_Origin_Lat = n.Neys_Origin_Lat; 
  Neys_False_Easting = n.Neys_False_Easting; 
  Neys_False_Northing = n.Neys_False_Northing; 
  Neys_Delta_Easting = n.Neys_Delta_Easting; 
  Neys_Delta_Northing = n.Neys_Delta_Northing; 
}


Neys::~Neys()
{
  delete lambertConformalConic2;
  lambertConformalConic2 = 0;
}


Neys& Neys::operator=( const Neys &n )
{
  if( this != &n )
  {
    lambertConformalConic2->operator=( *n.lambertConformalConic2 );
    semiMajorAxis = n.semiMajorAxis;
    flattening = n.flattening;
    Neys_Std_Parallel_1 = n.Neys_Std_Parallel_1;     
    Neys_Std_Parallel_2 = n.Neys_Std_Parallel_2; 
    Neys_Origin_Long = n.Neys_Origin_Long; 
    Neys_Origin_Lat = n.Neys_Origin_Lat; 
    Neys_False_Easting = n.Neys_False_Easting; 
    Neys_False_Northing = n.Neys_False_Northing; 
    Neys_Delta_Easting = n.Neys_Delta_Easting; 
    Neys_Delta_Northing = n.Neys_Delta_Northing; 
  }

  return *this;
}


NeysParameters* Neys::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters and Ney's (Modified Lambert Conformal Conic) projection parameters.
 *
 *   ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
 *   ellipsoidFlattening     : Flattening of ellipsoid                   (output)
 *   centralMeridian         : Longitude of origin, in radians           (output)
 *   originLatitude          : Latitude of origin, in radians            (output)
 *   standardParallel        : First standard parallel, in radians       (output)
 *   falseEasting            : False easting, in meters                  (output)
 *   falseNorthing           : False northing, in meters                 (output)
 */

  return new NeysParameters( CoordinateType::neys, Neys_Origin_Long, Neys_Origin_Lat, Neys_Std_Parallel_1, Neys_False_Easting, Neys_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* Neys::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts Geodetic (latitude and
 * longitude) coordinates to Ney's (Modified Lambert Conformal Conic) projection
 * (easting and northing) coordinates, according to the current ellipsoid and
 * Ney's (Modified Lambert Conformal Conic) projection parameters.  If any errors 
 * occur, an exception is thrown with a description of the error.
 *
 *    longitude        : Longitude, in radians                        (input)
 *    latitude         : Latitude, in radians                         (input)
 *    easting          : Easting (X), in meters                       (output)
 *    northing         : Northing (Y), in meters                      (output)
 */

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

  MapProjectionCoordinates* mapProjectionCoordinates =
     lambertConformalConic2->convertFromGeodetic( geodeticCoordinates );
  double easting = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();
  delete mapProjectionCoordinates;

  return new MapProjectionCoordinates( CoordinateType::neys, easting, northing );
}


MSP::CCS::GeodeticCoordinates* Neys::convertToGeodetic(
   MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Ney's (Modified Lambert Conformal
 * Conic) projection (easting and northing) coordinates to Geodetic (latitude)
 * and longitude) coordinates, according to the current ellipsoid and Ney's
 * (Modified Lambert Conformal Conic) projection parameters.  If any errors 
 * occur, an exception is thrown with a description of the error.
 *
 *    easting          : Easting (X), in meters                       (input)
 *    northing         : Northing (Y), in meters                      (input)
 *    longitude        : Longitude, in radians                        (output)
 *    latitude         : Latitude, in radians                         (output)
 */

  double easting = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Neys_False_Easting - Neys_Delta_Easting))
      ||(easting > (Neys_False_Easting + Neys_Delta_Easting)))
  { /* Easting out of range  */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < (Neys_False_Northing - Neys_Delta_Northing))
      || (northing > (Neys_False_Northing + Neys_Delta_Northing)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  GeodeticCoordinates* geodeticCoordinates = 
     lambertConformalConic2->convertToGeodetic( mapProjectionCoordinates );
  double longitude = geodeticCoordinates->longitude();
  double latitude = geodeticCoordinates->latitude();
  delete geodeticCoordinates;

  return new GeodeticCoordinates( CoordinateType::geodetic, longitude, latitude );
}



// CLASSIFICATION: UNCLASSIFIED
