// CLASSIFICATION: UNCLASSIFIED

/********************************************************************/
/* RSC IDENTIFIER: UPS
 *
 *
 * ABSTRACT
 *
 *    This component provides conversions between geodetic (latitude
 *    and longitude) coordinates and Universal Polar Stereographic (UPS)
 *    projection (hemisphere, easting, and northing) coordinates.
 *
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an 
 *    invalid value is found the error code is combined with the 
 *    current error code using the bitwise or.  This combining allows  
 *    multiple error codes to be returned. The possible error codes 
 *    are:
 *
 *         UPS_NO_ERROR           : No errors occurred in function
 *         UPS_LAT_ERROR          : Latitude outside of valid range
 *                                   (North Pole: 83.5 to 90,
 *                                    South Pole: -79.5 to -90)
 *         UPS_LON_ERROR          : Longitude outside of valid range
 *                                   (-180 to 360 degrees)
 *         UPS_HEMISPHERE_ERROR   : Invalid hemisphere ('N' or 'S')
 *         UPS_EASTING_ERROR      : Easting outside of valid range,
 *                                   (0 to 4,000,000m)
 *         UPS_NORTHING_ERROR     : Northing outside of valid range,
 *                                   (0 to 4,000,000m)
 *         UPS_A_ERROR            : Semi-major axis less than or equal to zero
 *         UPS_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                   (250 to 350)
 *
 *
 * REUSE NOTES
 *
 *    UPS is intended for reuse by any application that performs a Universal
 *    Polar Stereographic (UPS) projection.
 *
 *
 * REFERENCES
 *
 *    Further information on UPS can be found in the Reuse Manual.
 *
 *    UPS originated from :  U.S. Army Topographic Engineering Center
 *                           Geospatial Information Division
 *                           7701 Telegraph Road
 *                           Alexandria, VA  22310-3864
 *
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 *
 * RESTRICTIONS
 *
 *    UPS has no restrictions.
 *
 *
 * ENVIRONMENT
 *
 *    UPS was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC version 2.8.1
 *    2. Windows 95 with MS Visual C++ version 6
 *
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    2-27-07          Original Code
 *
 *
 */


/************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "UPS.h"
#include "PolarStereographic.h"
#include "PolarStereographicScaleFactorParameters.h"
#include "MapProjectionCoordinates.h"
#include "UPSCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    UPS.h      - Defines the function prototypes for the ups module.
 *    PolarStereographic.h    - Is used to convert polar stereographic coordinates
 *    MapProjectionCoordinates.h   - defines map projection coordinates
 *    UPSCoordinates.h   - defines UPS coordinates
 *    GeodeticCoordinates.h   - defines geodetic coordinates
 *    CoordinateConversionException.h - Exception handler
 *    ErrorMessages.h  - Contains exception messages
 */


using namespace MSP::CCS;


/***************************************************************************/
/*
 *                              DEFINES
 */

#define EPSILON 1.75e-7   /* approx 1.0e-5 degrees (~1 meter) in radians */

const double PI = 3.14159265358979323e0;     /* PI     */
const double PI_OVER = (PI/2.0e0);           /* PI over 2 */
const double MAX_LAT = 90.0 * (PI / 180.0);    /* 90 degrees in radians */
const double MAX_ORIGIN_LAT = 81.114528 * (PI / 180.0);
const double MIN_NORTH_LAT = 83.5 * (PI / 180.0);
const double MAX_SOUTH_LAT = -79.5 * (PI / 180.0);
const double MIN_EAST_NORTH = 0.0;
const double MAX_EAST_NORTH = 4000000.0;

const double UPS_False_Easting = 2000000;
const double UPS_False_Northing = 2000000;
const double UPS_Origin_Longitude = 0.0;


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

UPS::UPS( double ellipsoidSemiMajorAxis, double ellipsoidFlattening ) :
  CoordinateSystem(),
  UPS_Origin_Latitude( MAX_ORIGIN_LAT )
{
/*
 * The constructor receives the ellipsoid parameters and sets
 * the corresponding state variables. If any errors occur, an exception is 
 * thrown with a description of the error.
 *
 *   ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid in meters (input)
 *   ellipsoidFlattening     : Flattening of ellipsoid                (input)
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

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening    = ellipsoidFlattening;

  polarStereographicMap['N'] = new PolarStereographic(semiMajorAxis, flattening, UPS_Origin_Longitude, .994, 'N', UPS_False_Easting, UPS_False_Northing);
  polarStereographicMap['S'] = new PolarStereographic(semiMajorAxis, flattening, UPS_Origin_Longitude, .994, 'S', UPS_False_Easting, UPS_False_Northing);
 /// polarStereographicMap['N'] = new PolarStereographic(semiMajorAxis, flattening, UPS_Origin_Longitude, UPS_Origin_Latitude, UPS_False_Easting, UPS_False_Northing);
 /// polarStereographicMap['S'] = new PolarStereographic(semiMajorAxis, flattening, UPS_Origin_Longitude, -UPS_Origin_Latitude, UPS_False_Easting, UPS_False_Northing);
}


UPS::UPS( const UPS &u )
{
  std::map< char, PolarStereographic* > tempPolarStereographicMap = u.polarStereographicMap;
  polarStereographicMap['N'] = new PolarStereographic( *tempPolarStereographicMap['N'] );
  polarStereographicMap['S'] = new PolarStereographic( *tempPolarStereographicMap['S'] );
  semiMajorAxis = u.semiMajorAxis;
  flattening = u.flattening;
  UPS_Origin_Latitude = u.UPS_Origin_Latitude;     
}


UPS::~UPS()
{
	while( polarStereographicMap.begin() != polarStereographicMap.end() )
	{
		delete ( ( *polarStereographicMap.begin() ).second );
		polarStereographicMap.erase( polarStereographicMap.begin() );
	}
}


UPS& UPS::operator=( const UPS &u )
{
  if( this != &u )
  {
    std::map< char, PolarStereographic* > tempPolarStereographicMap = u.polarStereographicMap;
    polarStereographicMap['N']->operator=( *tempPolarStereographicMap['N'] );
    polarStereographicMap['S']->operator=( *tempPolarStereographicMap['S'] );
    semiMajorAxis = u.semiMajorAxis;
    flattening = u.flattening;
    UPS_Origin_Latitude = u.UPS_Origin_Latitude;     
  }

  return *this;
}


MSP::CCS::UPSCoordinates* UPS::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to UPS (hemisphere, easting, and northing)
 * coordinates, according to the current ellipsoid parameters. 
 * If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    latitude      : Latitude in radians                       (input)
 *    longitude     : Longitude in radians                      (input)
 *    hemisphere    : Hemisphere either 'N' or 'S'              (output)
 *    easting       : Easting/X in meters                       (output)
 *    northing      : Northing/Y in meters                      (output)
 */

  char hemisphere;

  double longitude = geodeticCoordinates->longitude();
  double latitude = geodeticCoordinates->latitude();

  if ((latitude < -MAX_LAT) || (latitude > MAX_LAT))
  {   /* latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  else if ((latitude < 0) && (latitude >= (MAX_SOUTH_LAT + EPSILON)))
    throw CoordinateConversionException( ErrorMessages::latitude  );
  else if ((latitude >= 0) && (latitude < (MIN_NORTH_LAT - EPSILON)))
    throw CoordinateConversionException( ErrorMessages::latitude  );
  if ((longitude < -PI) || (longitude > (2 * PI)))
  {  /* longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  if (latitude < 0)
  {
    UPS_Origin_Latitude = -MAX_ORIGIN_LAT; 
    hemisphere = 'S';
  }
  else
  {
    UPS_Origin_Latitude = MAX_ORIGIN_LAT; 
    hemisphere = 'N';
  }
  
  PolarStereographic polarStereographic = *polarStereographicMap[hemisphere];   
  MapProjectionCoordinates* polarStereographicCoordinates = polarStereographic.convertFromGeodetic( geodeticCoordinates );

  double easting = polarStereographicCoordinates->easting();
  double northing = polarStereographicCoordinates->northing();
  delete polarStereographicCoordinates;

  return new UPSCoordinates( CoordinateType::universalPolarStereographic, hemisphere, easting, northing );
}


MSP::CCS::GeodeticCoordinates* UPS::convertToGeodetic( MSP::CCS::UPSCoordinates* upsCoordinates )
{
/*
 * The function convertToGeodetic converts UPS (hemisphere, easting, 
 * and northing) coordinates to geodetic (latitude and longitude) coordinates
 * according to the current ellipsoid parameters.  If any errors occur, an 
 * exception is thrown with a description of the error.
 *
 *    hemisphere    : Hemisphere either 'N' or 'S'              (input)
 *    easting       : Easting/X in meters                       (input)
 *    northing      : Northing/Y in meters                      (input)
 *    latitude      : Latitude in radians                       (output)
 *    longitude     : Longitude in radians                      (output)
 */

  char hemisphere = upsCoordinates->hemisphere();
  double easting  = upsCoordinates->easting();
  double northing = upsCoordinates->northing();

  if ((hemisphere != 'N') && (hemisphere != 'S'))
    throw CoordinateConversionException( ErrorMessages::hemisphere  );
  if ((easting < MIN_EAST_NORTH) || (easting > MAX_EAST_NORTH))
    throw CoordinateConversionException( ErrorMessages::easting  );
  if ((northing < MIN_EAST_NORTH) || (northing > MAX_EAST_NORTH))
    throw CoordinateConversionException( ErrorMessages::northing  );

  if (hemisphere =='N')
  {
    UPS_Origin_Latitude = MAX_ORIGIN_LAT;
  }
  else if (hemisphere =='S')
  {
    UPS_Origin_Latitude = -MAX_ORIGIN_LAT;
  }

  MapProjectionCoordinates polarStereographicCoordinates(
     CoordinateType::polarStereographicStandardParallel, easting, northing );
  PolarStereographic polarStereographic    = *polarStereographicMap[hemisphere];
  GeodeticCoordinates* geodeticCoordinates = 
     polarStereographic.convertToGeodetic( &polarStereographicCoordinates ); 

  double latitude = geodeticCoordinates->latitude();

  if ((latitude < 0) && (latitude >= (MAX_SOUTH_LAT + EPSILON)))
  {
    delete geodeticCoordinates;
    throw CoordinateConversionException( ErrorMessages::latitude );
  }
  if ((latitude >= 0) && (latitude < (MIN_NORTH_LAT - EPSILON)))
  {
    delete geodeticCoordinates;
    throw CoordinateConversionException( ErrorMessages::latitude );
  }

  return geodeticCoordinates;
}

// CLASSIFICATION: UNCLASSIFIED
