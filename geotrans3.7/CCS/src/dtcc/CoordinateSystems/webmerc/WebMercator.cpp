// CLASSIFICATION: UNCLASSIFIED
/***************************************************************************/
/* RSC IDENTIFIER: Web Mercator
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates 
 *    (latitude and longitude) and Web Mercator coordinates
 *    (easting and northing).
 *
 * REFERENCES
 *
 *    Further information on Web Mercator can be found in the NGA document 
 *    "Implementation Practice Web Mercator Map Projection", 2014-02-18.
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    06-14-14          Original Code
 *
 */

#include <string.h>
#include <math.h>
#include "WebMercator.h"
#include "EllipsoidParameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    string.h    - Standard C string handling library
 *    math.h      - Standard C math library
 *    NZMG.h      - Is for prototype error checking
 *    MapProjectionCoordinates.h   - defines map projection coordinates
 *    GeodeticCoordinates.h   - defines geodetic coordinates
 *    CoordinateConversionException.h - Exception handler
 *    ErrorMessages.h  - Contains exception messages
 */


using namespace MSP::CCS;


//                               DEFINES
const double PI = 3.14159265358979323e0;


WebMercator::WebMercator( char* ellipsoidCode ) :
  CoordinateSystem( 6378137.0, 0.0 )
{
   /*
    * The constructor receives the ellipsoid code which must be "WE"
    *
    *   ellipsoidCode : 2-letter code for ellipsoid       (input)
    */

  if (strcmp(ellipsoidCode, "WE") != 0)
  { /* Ellipsoid must be WGS84 */
     throw CoordinateConversionException( ErrorMessages::webmEllipsoid );
  }
}


EllipsoidParameters* WebMercator::getParameters() const
{
/*                         
 * The function getParameters returns the current ellipsoid code.
 *
 *   ellipsoidCode : 2-letter code for ellipsoid          (output)
 */

  return new EllipsoidParameters(
     semiMajorAxis, flattening, "WE" ); // Always WGS84 radius
}

MSP::CCS::MapProjectionCoordinates* WebMercator::convertFromGeodetic(
   MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
   double longitude = geodeticCoordinates->longitude();
   double latitude = geodeticCoordinates->latitude();

   double easting  = longitude * semiMajorAxis;
   double northing = semiMajorAxis * log( tan( PI/4.0 + latitude / 2.0 ) );

   // Always throw an error because NGA does not want to allow
   // conversions to Web Mecator
   throw CoordinateConversionException( ErrorMessages::webmConversionTo  );

  return new MapProjectionCoordinates(
     CoordinateType::webMercator, easting, northing );
}

MSP::CCS::GeodeticCoordinates* WebMercator::convertToGeodetic(
   MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  double longitude = easting / semiMajorAxis;
  double latitude  = 2.0 * atan( exp( northing / semiMajorAxis ) ) - PI/2.0;

  return new GeodeticCoordinates(
     CoordinateType::geodetic, longitude, latitude );
}

// CLASSIFICATION: UNCLASSIFIED
