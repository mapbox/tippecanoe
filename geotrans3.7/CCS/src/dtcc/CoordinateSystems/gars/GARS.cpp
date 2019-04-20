// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: GARS
 *
 * ABSTRACT
 *
 *    This component provides conversions from Geodetic coordinates (latitude
 *    and longitude in radians) to a GARS coordinate string.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          GARS_NO_ERROR          : No errors occurred in function
 *          GARS_LAT_ERROR         : Latitude outside of valid range 
 *                                      (-90 to 90 degrees)
 *          GARS_LON_ERROR         : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          GARS_STR_ERROR         : A GARS string error: string too long,
 *                                      string too short, invalid numbers/letters
 *          GARS_STR_LAT_ERROR     : The latitude part of the GARS string
 *                                     (fourth and fifth  characters) is invalid.
 *          GARS_STR_LON_ERROR     : The longitude part of the GARS string
 *                                     (first three characters) is invalid.
 *          GARS_STR_15_MIN_ERROR  : The 15 minute part of the GARS
 *                                      string is less than 1 or greater than 4.
 *          GARS_STR_5_MIN_ERROR   : The 5 minute part of the GARS
 *                                      string is less than 1 or greater than 9.
 *          GARS_PRECISION_ERROR   : The precision must be between 0 and 5 
 *                                      inclusive.
 *
 *
 * REUSE NOTES
 *
 *    GARS is intended for reuse by any application that performs a 
 *    conversion between Geodetic and GARS coordinates.
 *    
 * REFERENCES
 *
 *    Further information on GARS can be found in the Reuse Manual.
 *
 *    GARS originated from : 
 *                              
 *                   http://earth-info.nga.mil/GandG/coordsys/grids/gars.html           
 *                              
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    GARS has no restrictions.
 *
 * ENVIRONMENT
 *
 *    GARS was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC version 2.8.1
 *    2. Windows XP with MS Visual C++ version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    07-10-06          Original Code
 *    03-02-07          Original C++ Code
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "GARS.h"
#include "GARSCoordinates.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *  ctype.h    - Standard C character handling library
 *  stdio.h    - Standard C input/output library
 *  stdlib.h   - Standard C general utility library
 *  string.h   - Standard C string handling library
 *  GARS.h     - for prototype error checking
 *  GARSCoordinates.h   - defines gars coordinates
 *  MapProjectionCoordinates.h   - defines map projection coordinates
 *  GeodeticCoordinates.h   - defines geodetic coordinates
 *  CoordinateConversionException.h - Exception handler
 *  ErrorMessages.h  - Contains exception messages
 */


using namespace MSP::CCS;


/***************************************************************************/
/*                               DEFINES 
 *
 */

const double MIN_LATITUDE = -90.0;    /* Minimum latitude                      */
const double MAX_LATITUDE = 90.0;     /* Maximum latitude                      */
const double MIN_LONGITUDE = -180.0;  /* Minimum longitude                     */
const double MAX_LONGITUDE = 360.0;   /* Maximum longitude                     */
const double MIN_PER_DEG = 60;        /* Number of minutes per degree          */
const int GARS_MINIMUM = 5;           /* Minimum number of chars for GARS      */
const int GARS_MAXIMUM = 7;           /* Maximum number of chars for GARS      */
const int GARS_LETTERS = 4;           /* Number of letters in GARS string      */
const int MAX_PRECISION = 5;          /* Maximum precision of minutes part     */
const int LETTER_A_OFFSET = 65;       /* Letter A offset in character set      */
const double PI = 3.14159265358979323e0;     /* PI                             */
const double PI_OVER_180 = PI / 180.0;
const double RADIAN_TO_DEGREE = 180.0e0 / PI;

/* A = 0, B = 1, C = 2, D = 3, E = 4, F = 5, G = 6, H = 7                 */
const int LETTER_I = 8;                /* ARRAY INDEX FOR LETTER I        */
/* J = 9, K = 10, L = 11, M = 12, N = 13                                  */
const int LETTER_O = 14;              /* ARRAY INDEX FOR LETTER O         */
/* P = 15, Q = 16, R = 17, S = 18, T = 19, U = 20, V = 21,                */
/* W = 22, X = 23, Y = 24, Z = 25                                         */

const char _1 = '1';
const char _2 = '2';
const char _3 = '3';
const char _4 = '4';
const char _5 = '5';
const char _6 = '6';
const char _7 = '7';
const char _8 = '8';
const char _9 = '9';


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

GARS::GARS() :
  CoordinateSystem( 0, 0 )
{
}


GARS::GARS( const GARS &g )
{
  semiMajorAxis = g.semiMajorAxis;
  flattening = g.flattening;
}


GARS::~GARS()
{
}


GARS& GARS::operator=( const GARS &g )
{
  if( this != &g )
  {
    semiMajorAxis = g.semiMajorAxis;
    flattening = g.flattening;
  }

  return *this;
}


MSP::CCS::GARSCoordinates* GARS::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates, long precision )
{
/*   
 *  The function convertFromGeodetic converts Geodetic (latitude and longitude in radians)
 *  coordinates to a GARS coordinate string.  Precision specifies the
 *  number of digits in the GARS string for latitude and longitude:
 *                                  0: 30 minutes (5 characters)
 *                                  1: 15 minutes (6 characters)
 *                                  2: 5 minutes (7 characters)
 *
 *    longitude   : Longitude in radians.                  (input)
 *    latitude    : Latitude in radians.                   (input)
 *    precision   : Precision specified by the user.       (input)
 *    GARSString  : GARS coordinate string.                (output)
 *
 */

  long ew_value;
  long letter_index[GARS_LETTERS + 1];    /* GARS letters                 */
  char _15_minute_value_str[2] = "";
  char _5_minute_value_str[2] = "";
  double round_error = 5.0e-11;
  char* _15_minute_array[2][2] = {{"3", "1"}, {"4", "2"}};
  char* _5_minute_array[3][3] = {{"7", "4", "1"}, {"8", "5", "2"}, {"9", "6", "3"}};
  double long_minutes, lat_minutes; 
  double long_remainder, lat_remainder; 
  long horiz_index_30, vert_index_30; 
  long horiz_index_15, vert_index_15; 
  long horiz_index_5, vert_index_5; 
  char GARSString[8];

  double latitude = geodeticCoordinates->latitude() * RADIAN_TO_DEGREE;
  double longitude = geodeticCoordinates->longitude() *  RADIAN_TO_DEGREE;

  if ( ( latitude < MIN_LATITUDE ) || ( latitude > MAX_LATITUDE ) )
    throw CoordinateConversionException( ErrorMessages::latitude );
  if ( ( longitude < MIN_LONGITUDE ) || ( longitude > MAX_LONGITUDE ) )
    throw CoordinateConversionException( ErrorMessages::longitude );
  if ( ( precision < 0 ) || ( precision > MAX_PRECISION ) )
    throw CoordinateConversionException( ErrorMessages::precision );

  /* North pole is an exception, read over and down */
  if( latitude == MAX_LATITUDE )
    latitude = 89.99999999999;

  if( longitude >= 180.0 )
    longitude -= 360.0;

  /* Convert longitude and latitude from degrees to minutes */ 
  /* longitude assumed in -180 <= long < +180 range */ 
  long_minutes = ( longitude - MIN_LONGITUDE ) * 60.0 + round_error; 
  lat_minutes = ( latitude - MIN_LATITUDE ) * 60.0 + round_error; 
  /* now we have a positive number of minutes */ 

  /* Find 30-min cell indices 0-719 and 0-359 */ 
  horiz_index_30 = ( long )( long_minutes / 30.0 ); 
  vert_index_30 = ( long )( lat_minutes / 30.0 );

  /* Compute remainders 0 <= x < 30.0 */ 
  long_remainder = long_minutes - ( horiz_index_30 ) * 30.0; 
  lat_remainder = lat_minutes - ( vert_index_30 ) * 30.0; 

  /* Find 15-min cell indices 0 or 1 */ 
  horiz_index_15 = ( long )( long_remainder / 15.0 ); 
  vert_index_15 = ( long )( lat_remainder / 15.0 ); 

  /* Compute remainders 0 <= x < 15.0 */ 
  long_remainder = long_remainder - ( horiz_index_15 ) * 15.0; 
  lat_remainder = lat_remainder - ( vert_index_15 ) * 15.0; 

  /* Find 5-min cell indices 0, 1, or 2 */ 
  horiz_index_5 = ( long )( long_remainder / 5.0 ); 
  vert_index_5 = ( long )( lat_remainder / 5.0 );

  /* Calculate 30 minute east/west value, 1-720 */
  ew_value = horiz_index_30 + 1;

  /* Calculate 30 minute north/south first letter, A-Q */
  letter_index[0] = ( long )( vert_index_30 / 24.0 );

  /* Calculate 30 minute north/south second letter, A-Z */
  letter_index[1] = ( long )( vert_index_30 - letter_index[0] * 24.0 );

  /* Letters I and O are invalid, so skip them */
  if( letter_index[0] >= LETTER_I )
    letter_index[0]++;
  if( letter_index[0] >= LETTER_O )
    letter_index[0] ++;

  if( letter_index[1] >= LETTER_I )
    letter_index[1]++;
  if( letter_index[1] >= LETTER_O )
    letter_index[1] ++;

  /* Get 15 minute quadrant value, 1-4 */
  strcpy( _15_minute_value_str, _15_minute_array[horiz_index_15][vert_index_15] );

  /* Get 5 minute keypad value, 1-9 */
  strcpy( _5_minute_value_str, _5_minute_array[horiz_index_5][vert_index_5] );

  /* Form the gars string */
  if( ew_value < 10 )
    sprintf ( GARSString, "00%d%c%c", ew_value, ( char )( letter_index[0] + LETTER_A_OFFSET ), ( char )( letter_index[1] + LETTER_A_OFFSET ) );
  else if( ew_value < 100 )
    sprintf ( GARSString, "0%d%c%c", ew_value, ( char )( letter_index[0] + LETTER_A_OFFSET ), ( char )( letter_index[1] + LETTER_A_OFFSET ) );
  else
    sprintf ( GARSString, "%d%c%c", ew_value, ( char )( letter_index[0] + LETTER_A_OFFSET ), ( char )( letter_index[1] + LETTER_A_OFFSET ) );

  if( precision > 0 )
  {
    strcat( GARSString, _15_minute_value_str);

    if( precision > 1 )
      strcat( GARSString, _5_minute_value_str);
  }

  GARSString[7] = '\0';

  return new GARSCoordinates( CoordinateType::globalAreaReferenceSystem, GARSString );
}


MSP::CCS::GeodeticCoordinates* GARS::convertToGeodetic( MSP::CCS::GARSCoordinates* garsCoordinates )
{
/*
 *  The function convertToGeodetic converts a GARS coordinate string to Geodetic (latitude
 *  and longitude in radians) coordinates.
 *
 *    GARSString  : GARS coordinate string.     (input)
 *    longitude   : Longitude in radians.       (output)
 *    latitude    : Latitude in radians.        (output)
 *
 */

  long gars_length;          /* length of GARS string                      */
  int index = 0;
  char ew_str[4];
  int ew_value = 0;
  char letter = ' ';
  int ns_str[3];
  char _15_minute_value = 0;
  char _5_minute_value = 0;
  double lat_minutes = 0;
  double lon_minutes = 0;
  double longitude, latitude;

  char* GARSString = garsCoordinates->GARSString();

  gars_length = strlen( GARSString );
  if ( ( gars_length < GARS_MINIMUM ) || ( gars_length > GARS_MAXIMUM ) )
    throw CoordinateConversionException( ErrorMessages::garsString );

  while( isdigit( GARSString[index] ) )
  {
  ew_str[index] = GARSString[index];
  index++;
  }

  if( index != 3 )
  throw CoordinateConversionException( ErrorMessages::garsString );

  /* Get 30 minute east/west value, 1-720 */
  ew_value = atoi( ew_str );

  letter = GARSString[index];
  if( !isalpha( letter ) )
  throw CoordinateConversionException( ErrorMessages::longitude );

  /* Get first 30 minute north/south letter, A-Q */
  ns_str[0] = toupper( letter ) - LETTER_A_OFFSET;
  letter = GARSString[++index];
  if( !isalpha( letter ) )
  throw CoordinateConversionException( ErrorMessages::latitude );

  /* Get second 30 minute north/south letter, A-Z */
  ns_str[1] = toupper( letter ) - LETTER_A_OFFSET;

  if( index + 1 < gars_length )
  {
    /* Get 15 minute quadrant value, 1-4 */
    _15_minute_value = GARSString[++index];
    if( !isdigit( _15_minute_value ) || _15_minute_value < _1 || _15_minute_value > _4 )
      throw CoordinateConversionException( ErrorMessages::longitude_min );
    else
    {
      if( index + 1 < gars_length )
      {
        /* Get 5 minute quadrant value, 1-9 */
        _5_minute_value = GARSString[++index];
        if( !isdigit( _5_minute_value ) || _5_minute_value < _1 || _5_minute_value > _9 )
          throw CoordinateConversionException( ErrorMessages::latitude_min );
      }
    }
  }

  longitude = ( ( ( ew_value - 1.0 ) / 2.0 ) - 180.0 );

  /* Letter I and O are invalid */
  if( ns_str[0] >= LETTER_O )
    ns_str[0] --;
  if( ns_str[0] >= LETTER_I ) 
    ns_str[0] --;

  if( ns_str[1] >= LETTER_O )
    ns_str[1] --;
  if( ns_str[1] >= LETTER_I )
    ns_str[1] --;

  latitude = ( ( -90.0 + ( ns_str[0] * 12.0 ) ) + ( ns_str[1] / 2.0 ) );

  switch( _15_minute_value )
  {
    case _1:
      lat_minutes = 15.0;
      break;
    case _4:
      lon_minutes = 15.0;
      break;
    case _2:
      lat_minutes = 15.0;
      lon_minutes = 15.0;
      break;
    case _3:
    default:
      break;
  }

  switch( _5_minute_value )
  {
    case _4:
      lat_minutes += 5.0;
      break;
    case _1:
      lat_minutes += 10.0;
      break;
    case _8:
      lon_minutes += 5.0;
      break;
    case _5:
      lon_minutes += 5.0;
      lat_minutes += 5.0;
      break;
    case _2:
      lon_minutes += 5.0;
      lat_minutes += 10.0;
      break;
    case _9:
      lon_minutes += 10.0;
      break;
    case _6:
      lon_minutes += 10.0;
      lat_minutes += 5.0;
      break;
    case _3:
      lon_minutes += 10.0;
      lat_minutes += 10.0;
      break;
    case _7:
    default:
      break;
  }

  /* Add these values to force the reference point to be the center of the box */
  if( _5_minute_value )
  {
    lat_minutes += 2.5;
    lon_minutes += 2.5;
  }
  else if( _15_minute_value )
  {
    lat_minutes += 7.5;
    lon_minutes += 7.5;
  }
  else
  {
    lat_minutes += 15.0;
    lon_minutes += 15.0;
  }

  latitude += lat_minutes / MIN_PER_DEG;
  longitude += lon_minutes / MIN_PER_DEG;
  longitude *= PI_OVER_180;
  latitude *= PI_OVER_180;

  return new GeodeticCoordinates( CoordinateType::geodetic, longitude, latitude );
}



// CLASSIFICATION: UNCLASSIFIED
