// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: UTM
 *
 * ABSTRACT
 *
 *    This component provides conversions between geodetic coordinates
 *    (latitude and longitudes) and Universal Transverse Mercator (UTM)
 *    projection (zone, hemisphere, easting, and northing) coordinates.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          UTM_NO_ERROR           : No errors occurred in function
 *          UTM_LAT_ERROR          : Latitude outside of valid range
 *                                    (-80.5 to 84.5 degrees)
 *          UTM_LON_ERROR          : Longitude outside of valid range
 *                                    (-180 to 360 degrees)
 *          UTM_EASTING_ERROR      : Easting outside of valid range
 *                                    (100,000 to 900,000 meters)
 *          UTM_NORTHING_ERROR     : Northing outside of valid range
 *                                    (0 to 10,000,000 meters)
 *          UTM_ZONE_ERROR         : Zone outside of valid range (1 to 60)
 *          UTM_HEMISPHERE_ERROR   : Invalid hemisphere ('N' or 'S')
 *          UTM_ZONE_OVERRIDE_ERROR: Zone outside of valid range
 *                                    (1 to 60) and within 1 of 'natural' zone
 *          UTM_A_ERROR            : Semi-major axis less than or equal to zero
 *          UTM_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                    (250 to 350)
 *
 * REUSE NOTES
 *
 *    UTM is intended for reuse by any application that performs a Universal
 *    Transverse Mercator (UTM) projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on UTM can be found in the Reuse Manual.
 *
 *    UTM originated from :  U.S. Army Topographic Engineering Center
 *                           Geospatial Information Division
 *                           7701 Telegraph Road
 *                           Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    UTM has no restrictions.
 *
 * ENVIRONMENT
 *
 *    UTM was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. MSDOS with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    2-27-07           Original C++ Code
 *    7/18/2010   NGL   BAEts27181 Add logic for special zones over southern
 *                      Norway and Svalbard. This was removed with BAEts24468 
 *                      and we are being asked to put it back in.
 *    5-09-11     KNL   BAEts28908, add default constructor.
 *	  1-16-16	  AL    MSP_DR30125 added ellipsoid code into constructor 
 *						and passes ellipsoid code to TransMercator
 *	  1-21-16	  KC	BAE_MSP00030211, removed the shift from longitude.
 *						Shift is applied when determining the zone. 
 *
 */


/***************************************************************************/
/*
 *                              INCLUDES
 */

#include <math.h>
#include "UTM.h"
#include "TransverseMercator.h"
#include "UTMParameters.h"
#include "MapProjectionCoordinates.h"
#include "UTMCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    UTM.h          - Defines the function prototypes for the utm module.
 *    TransverseMercator.h - Is used to convert transverse mercator coordinates
 *    MapProjectionCoordinates.h  - defines map projection coordinates
 *    UTMCoordinates.h   - defines UTM coordinates
 *    GeodeticCoordinates.h   - defines geodetic coordinates
 *    CoordinateConversionException.h - Exception handler
 *    ErrorMessages.h  - Contains exception messages
 */

using namespace MSP::CCS;

/***************************************************************************/
/*
 *                              DEFINES
 */

#define PI           3.14159265358979323e0
#define PI_OVER_180  (3.14159265358979323e0 / 180.0)
#define MIN_LAT      ((-80.5 * PI) / 180.0) /* -80.5 degrees in radians */
#define MAX_LAT      (( 84.5 * PI) / 180.0) /*  84.5 degrees in radians */
#define MIN_EASTING   100000.0
#define MAX_EASTING   900000.0
#define MIN_NORTHING  0.0
#define MAX_NORTHING  10000000.0

#define EPSILON       1.75e-7   /* approx 1.0e-5 deg (~1 meter) in radians */

/************************************************************************/
/*                              FUNCTIONS
 *
 */

UTM::UTM()
{
/*
 * The default constructor
 */
  // DR30125    default WGS-84
  strcpy( ellipsCode, "WE" ); 
  double ellipsoidSemiMajorAxis = 6378137.0;
  double ellipsoidFlattening =  1 / 298.257223563;
  double inv_f = 1 / ellipsoidFlattening;

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening = ellipsoidFlattening;
  UTM_Override = 0;

  double centralMeridian;
  double originLatitude = 0;
  double falseEasting   = 500000;
  double falseNorthing  = 0;
  double scale          = 0.9996;
  
  for(int zone = 1; zone <= 60; zone++)
  {
     if (zone >= 31)
        centralMeridian = ((6 * zone - 183) * PI_OVER_180);
     else
        centralMeridian = ((6 * zone + 177) * PI_OVER_180);
     
     transverseMercatorMap[zone] = new TransverseMercator(
        semiMajorAxis, flattening, centralMeridian, originLatitude,
        falseEasting, falseNorthing, scale, ellipsCode);
  }
}

UTM::UTM(
   double ellipsoidSemiMajorAxis,
   double ellipsoidFlattening,
   char *ellipsoidCode,
   long   override
   ) :
   UTM_Override( 0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * UTM zone override parameter as inputs, and sets the corresponding state
 * variables.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 * ellipsoidSemiMajorAxis : Semi-major axis of ellipsoid, in meters (input)
 * ellipsoidFlattening    : Flattening of ellipsoid                 (input)
 * override               : UTM override zone, 0 indicates no override (input)
 */
  /* get ellipsoid Code */
  strcpy( ellipsCode, ellipsoidCode ); 
  double inv_f = 1 / ellipsoidFlattening;

  if (ellipsoidSemiMajorAxis <= 0.0)
  { /* Semi-major axis must be greater than zero */
    throw CoordinateConversionException( ErrorMessages::semiMajorAxis );
  }
  if ((inv_f < 250) || (inv_f > 350))
  { /* Inverse flattening must be between 250 and 350 */
    throw CoordinateConversionException( ErrorMessages::ellipsoidFlattening );
  }
  if ((override < 0) || (override > 60))
  {
    throw CoordinateConversionException( ErrorMessages::zoneOverride );
  }

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening = ellipsoidFlattening;

  UTM_Override = override;

  double centralMeridian;
  double originLatitude = 0;
  double falseEasting   = 500000;
  double falseNorthing  = 0;
  double scale          = 0.9996;
  
  for(int zone = 1; zone <= 60; zone++)
  {
     if (zone >= 31)
        centralMeridian = ((6 * zone - 183) * PI_OVER_180);
     else
        centralMeridian = ((6 * zone + 177) * PI_OVER_180);
     
     transverseMercatorMap[zone] = new TransverseMercator(
        semiMajorAxis, flattening, centralMeridian, originLatitude,
        falseEasting, falseNorthing, scale, ellipsCode);
  }
}


UTM::UTM( const UTM &u )
{
  int zone = 1;
  std::map< int, TransverseMercator* > tempTransverseMercatorMap =
     u.transverseMercatorMap;
  std::map< int, TransverseMercator* >::iterator _iterator =
     tempTransverseMercatorMap.begin();
  while( _iterator != tempTransverseMercatorMap.end() && zone <= 60 )
  {
    transverseMercatorMap[zone] = new TransverseMercator( *_iterator->second );
    zone++;
  }

  semiMajorAxis = u.semiMajorAxis;
  flattening    = u.flattening;
  UTM_Override  = u.UTM_Override;
}


UTM::~UTM()
{
   while( transverseMercatorMap.begin() != transverseMercatorMap.end() )
   {
      delete ( ( *transverseMercatorMap.begin() ).second );
      transverseMercatorMap.erase( transverseMercatorMap.begin() );
   }
}


UTM& UTM::operator=( const UTM &u )
{
  if( this != &u )
  {
     int zone = 1;
     std::map< int, TransverseMercator* > tempTransverseMercatorMap =
        u.transverseMercatorMap;
     std::map< int, TransverseMercator* >::iterator _iterator =
        tempTransverseMercatorMap.begin();
     while( _iterator != tempTransverseMercatorMap.end() && zone <= 60 )
     {
        transverseMercatorMap[zone]->operator=( *_iterator->second );
        zone++;
     }

     ///   transverseMercator->operator=( *u.transverseMercator );
     semiMajorAxis = u.semiMajorAxis;
     flattening    = u.flattening;
     UTM_Override  = u.UTM_Override;
  }

  return *this;
}


UTMParameters* UTM::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters and UTM zone override parameter.
 *
 *    ellipsoidSemiMajorAxis : Semi-major axis of ellipsoid, in meters (output)
 *    ellipsoidFlattening    : Flattening of ellipsoid                 (output)
 *    override         : UTM override zone, zero indicates no override (output)
 */

  return new UTMParameters(
     CoordinateType::universalTransverseMercator, UTM_Override );
}


MSP::CCS::UTMCoordinates* UTM::convertFromGeodetic(
   MSP::CCS::GeodeticCoordinates* geodeticCoordinates,
   int                            utmZoneOverride )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to UTM projection (zone, hemisphere, easting and
 * northing) coordinates according to the current ellipsoid and UTM zone
 * override parameters.  If any errors occur, an exception is thrown
 * with a description of the error.
 *
 *    longitude         : Longitude in radians                (input)
 *    latitude          : Latitude in radians                 (input)
 *    zone              : UTM zone                            (output)
 *    hemisphere        : North or South hemisphere           (output)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  long Lat_Degrees;
  long Long_Degrees;
  long temp_zone;
  char hemisphere;
  double False_Northing = 0;

  double longitude = geodeticCoordinates->longitude();
  double latitude  = geodeticCoordinates->latitude();

  if ((latitude < (MIN_LAT - EPSILON)) || (latitude >= (MAX_LAT + EPSILON)))
  { /* latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude );
  }
  if ((longitude < (-PI - EPSILON)) || (longitude > (2*PI + EPSILON)))
  { /* longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude );
  }

  if((latitude > -1.0e-9) && (latitude < 0))
    latitude = 0.0;

  if (longitude < 0)
    longitude += (2*PI);

  Lat_Degrees = (long)(latitude * 180.0 / PI);
  Long_Degrees = (long)(longitude * 180.0 / PI);

  if (longitude < PI)
    temp_zone = (long)(31 + (((longitude+1.0e-10) * 180.0 / PI) / 6.0));
  else
    temp_zone = (long)((((longitude+1.0e-10) * 180.0 / PI) / 6.0) - 29);

  if (temp_zone > 60)
    temp_zone = 1;

  /* allow UTM zone override up to +/- one zone of the calculated zone */  
  if( utmZoneOverride )
  {
    if ((temp_zone == 1) && (utmZoneOverride == 60))
      temp_zone = utmZoneOverride;
    else if ((temp_zone == 60) && (utmZoneOverride == 1))
      temp_zone = utmZoneOverride;
    else if (((temp_zone-1) <= utmZoneOverride) &&
              (utmZoneOverride <= (temp_zone+1)))
      temp_zone = utmZoneOverride;
    else
      throw CoordinateConversionException( ErrorMessages::zoneOverride );
  }
  else if( UTM_Override )
  {
    if ((temp_zone == 1) && (UTM_Override == 60))
      temp_zone = UTM_Override;
    else if ((temp_zone == 60) && (UTM_Override == 1))
      temp_zone = UTM_Override;
    else if (((temp_zone-1) <= UTM_Override) &&
              (UTM_Override <= (temp_zone+1)))
      temp_zone = UTM_Override;
    else
      throw CoordinateConversionException( ErrorMessages::zoneOverride );
  }
  else /* not UTM zone override */
  {
    /* check for special zone cases over southern Norway and Svalbard */
    if ((Lat_Degrees > 55) && (Lat_Degrees < 64) && (Long_Degrees > -1)
        && (Long_Degrees < 3))
      temp_zone = 31;
    if ((Lat_Degrees > 55) && (Lat_Degrees < 64) && (Long_Degrees > 2)
        && (Long_Degrees < 12))
      temp_zone = 32;
    if ((Lat_Degrees > 71) && (Long_Degrees > -1) && (Long_Degrees < 9))
      temp_zone = 31;
    if ((Lat_Degrees > 71) && (Long_Degrees > 8) && (Long_Degrees < 21))
      temp_zone = 33;
    if ((Lat_Degrees > 71) && (Long_Degrees > 20) && (Long_Degrees < 33))
      temp_zone = 35;
    if ((Lat_Degrees > 71) && (Long_Degrees > 32) && (Long_Degrees < 42))
      temp_zone = 37;
  }
 
  TransverseMercator *transverseMercator = transverseMercatorMap[temp_zone];

  if (latitude < 0)
  {
    False_Northing = 10000000;
    hemisphere = 'S';
  }
  else
    hemisphere = 'N';

  GeodeticCoordinates tempGeodeticCoordinates(
     CoordinateType::geodetic, longitude, latitude );
  MapProjectionCoordinates* transverseMercatorCoordinates =
     transverseMercator->convertFromGeodetic( &tempGeodeticCoordinates );
  double easting = transverseMercatorCoordinates->easting();
  double northing = transverseMercatorCoordinates->northing() + False_Northing;

  if ((easting < MIN_EASTING) || (easting > MAX_EASTING))
  {
    delete transverseMercatorCoordinates;
    throw CoordinateConversionException( ErrorMessages::easting );
  }

  if ((northing < MIN_NORTHING) || (northing > MAX_NORTHING))
  {
    delete transverseMercatorCoordinates;
    throw CoordinateConversionException( ErrorMessages::northing );
  }

  delete transverseMercatorCoordinates;

  return new UTMCoordinates(
     CoordinateType::universalTransverseMercator,
     temp_zone, hemisphere, easting, northing );
}


MSP::CCS::GeodeticCoordinates* UTM::convertToGeodetic(
   MSP::CCS::UTMCoordinates* utmCoordinates )
{
/*
 * The function convertToGeodetic converts UTM projection (zone,
 * hemisphere, easting and northing) coordinates to geodetic(latitude
 * and  longitude) coordinates, according to the current ellipsoid
 * parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    zone              : UTM zone                               (input)
 *    hemisphere        : North or South hemisphere              (input)
 *    easting           : Easting (X) in meters                  (input)
 *    northing          : Northing (Y) in meters                 (input)
 *    longitude         : Longitude in radians                   (output)
 *    latitude          : Latitude in radians                    (output)
 */

  double False_Northing = 0;

  long zone       = utmCoordinates->zone();
  char hemisphere = utmCoordinates->hemisphere();
  double easting  = utmCoordinates->easting();
  double northing = utmCoordinates->northing();

  if ((zone < 1) || (zone > 60))
    throw CoordinateConversionException( ErrorMessages::zone );
  if ((hemisphere != 'S') && (hemisphere != 'N'))
    throw CoordinateConversionException( ErrorMessages::hemisphere );
  if ((easting < MIN_EASTING) || (easting > MAX_EASTING))
    throw CoordinateConversionException( ErrorMessages::easting );
  if ((northing < MIN_NORTHING) || (northing > MAX_NORTHING))
    throw CoordinateConversionException( ErrorMessages::northing );

  TransverseMercator *transverseMercator = transverseMercatorMap[zone];

  if (hemisphere == 'S')
    False_Northing = 10000000;

  MapProjectionCoordinates transverseMercatorCoordinates(
     CoordinateType::transverseMercator, easting, northing - False_Northing );
  GeodeticCoordinates* geodeticCoordinates =
     transverseMercator->convertToGeodetic( &transverseMercatorCoordinates );
  geodeticCoordinates->setWarningMessage("");  

  double latitude = geodeticCoordinates->latitude();

  if ((latitude < (MIN_LAT - EPSILON)) || (latitude >= (MAX_LAT + EPSILON)))
  { /* latitude out of range */
    delete geodeticCoordinates;
    throw CoordinateConversionException( ErrorMessages::northing );
  }

  return geodeticCoordinates;
}

// CLASSIFICATION: UNCLASSIFIED
