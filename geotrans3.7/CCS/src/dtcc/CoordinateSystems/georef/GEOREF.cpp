// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: GEOREF
 *
 * ABSTRACT
 *
 *    This component provides conversions from Geodetic coordinates (latitude
 *    and longitude in radians) to a GEOREF coordinate string.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          GEOREF_NO_ERROR          : No errors occurred in function
 *          GEOREF_LAT_ERROR         : Latitude outside of valid range 
 *                                      (-90 to 90 degrees)
 *          GEOREF_LON_ERROR         : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          GEOREF_STR_ERROR         : A GEOREF string error: string too long,
 *                                      string too short, or string length
 *                                      not even.
 *          GEOREF_STR_LAT_ERROR     : The latitude part of the GEOREF string
 *                                     (second or fourth character) is invalid.
 *          GEOREF_STR_LON_ERROR     : The longitude part of the GEOREF string
 *                                     (first or third character) is invalid.
 *          GEOREF_STR_LAT_MIN_ERROR : The latitude minute part of the GEOREF
 *                                      string is greater than 60.
 *          GEOREF_STR_LON_MIN_ERROR : The longitude minute part of the GEOREF
 *                                      string is greater than 60.
 *          GEOREF_PRECISION_ERROR   : The precision must be between 0 and 5 
 *                                      inclusive.
 *
 *
 * REUSE NOTES
 *
 *    GEOREF is intended for reuse by any application that performs a 
 *    conversion between Geodetic and GEOREF coordinates.
 *    
 * REFERENCES
 *
 *    Further information on GEOREF can be found in the Reuse Manual.
 *
 *    GEOREF originated from :  U.S. Army Topographic Engineering Center
 *                              Geospatial Information Division
 *                              7701 Telegraph Road
 *                              Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    GEOREF has no restrictions.
 *
 * ENVIRONMENT
 *
 *    GEOREF was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC version 2.8.1
 *    2. Windows 95 with MS Visual C++ version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    02-20-97          Original Code
 *    03-02-07          Original C++ Code
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "GEOREF.h"
#include "GEOREFCoordinates.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *  ctype.h    - Standard C character handling library
 *  math.h     - Standard C math library
 *  stdio.h    - Standard C input/output library
 *  stdlib.h   - Standard C general utility library
 *  string.h   - Standard C string handling library
 *  GEOREF.h   - for prototype error checking
 *  GEOREFCoordinates.h   - defines GEOREF coordinates
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

const int TRUE = 1;
const int FALSE = 0;
const double LATITUDE_LOW = -90.0;           /* Minimum latitude                      */
const double LATITUDE_HIGH = 90.0;           /* Maximum latitude                      */
const double LONGITUDE_LOW = -180.0;         /* Minimum longitude                     */
const double LONGITUDE_HIGH = 360.0;         /* Maximum longitude                     */
const double MIN_PER_DEG = 60.0;             /* Number of minutes per degree          */
const int GEOREF_MINIMUM = 4;                /* Minimum number of chars for GEOREF    */
const int GEOREF_MAXIMUM = 14;               /* Maximum number of chars for GEOREF    */
const int GEOREF_LETTERS = 4;                /* Number of letters in GEOREF string    */
const int MAX_PRECISION = 5;                 /* Maximum precision of minutes part     */
const int LETTER_I = 8;                      /* Index for letter I                    */
const int LETTER_M = 12;                     /* Index for letter M                    */
const int LETTER_O = 14;                     /* Index for letter O                    */
const int LETTER_Q = 16;                     /* Index for letter Q                    */
const int LETTER_Z = 25;                     /* Index for letter Z                    */
const int LETTER_A_OFFSET = 65;              /* Letter A offset in character set      */
const int ZERO_OFFSET = 48;                  /* Number zero offset in character set   */
const double PI = 3.14159265358979323e0;     /* PI                                    */
const double DEGREE_TO_RADIAN = (PI / 180.0);
const double RADIAN_TO_DEGREE = (180.0 / PI);
const double QUAD = 15.0;                    /* Degrees per grid square               */
const double ROUND_ERROR = 0.0000005;        /* Rounding factor                       */


/************************************************************************/
/*                              LOCAL FUNCTIONS     
 *
 */

void extractDegrees( char *GEOREFString, double *longitude, double *latitude )
{ 
/*    
 *  This function extracts the latitude and longitude degree parts of the 
 *  GEOREF string.  The latitude and longitude degree parts are the first four
 *  characters.
 *
 *    GEOREFString  : GEOREF string             (input)
 *    longitude     : Longitude in degrees      (output)
 *    latitude      : Latitude in degrees       (output)
 */
  long i;                             /* counter in for loops            */
  long temp_char;                     /* temporary character             */
  long letter_number[GEOREF_LETTERS]; /* number corresponding to letter  */

  for (i=0;i<GEOREF_LETTERS;i++)
  {
    temp_char = toupper(GEOREFString[i]);
    temp_char = temp_char - LETTER_A_OFFSET;
    if ((!isalpha(GEOREFString[i]))
        || (temp_char == LETTER_I)
        || (temp_char == LETTER_O))
    {
      if ((i == 0) || (i == 2))
        throw CoordinateConversionException( ErrorMessages::longitude );
      else
        throw CoordinateConversionException( ErrorMessages::latitude );
    }
    letter_number[i] = temp_char;
  }
  for (i=0;i<4;i++)
  {
    if (letter_number[i] > LETTER_O)
      letter_number[i] -= 2;
    else if (letter_number[i] > LETTER_I)
      letter_number[i] -= 1;
  }
  if ((letter_number[0] > 23) || (letter_number[2] > 14))
    throw CoordinateConversionException( ErrorMessages::georefString );
  if ((letter_number[1] > 11) || (letter_number[3] > 14))
    throw CoordinateConversionException( ErrorMessages::georefString );

  *latitude = (double)(letter_number[1]) * QUAD + (double)(letter_number[3]);
  *longitude = (double)(letter_number[0]) * QUAD + (double)(letter_number[2]);
} 


void extractMinutes( char *GEOREFString, long start, long length, long errorType, double *minutes )
{ 
/*    
 *  This function extracts the minutes from the GEOREF string.  The minutes
 *  part begins at position start and has length length.  The ERROR_TYPE is
 *  to allow this function to work with both latitude and longitude minutes.
 *
 *    GEOREFString : GEOREF string                                        (input)
 *    start        : Start position in the GEOREF string                  (input)
 *    length       : length of minutes in the GEOREF string               (input)
 *    errorType    : has a value of either GEOREF_STR_LAT_MIN_ERROR  (input)
 *                    or GEOREF_STR_LON_MIN_ERROR
 *    minutes      : minute part                                          (output)
 */
  long i;                    /* counter in for loop  */
  char temp_str[(GEOREF_MAXIMUM-GEOREF_LETTERS)/2 + 1];

  for (i=0;i<length;i++)
  {
    if (isdigit(GEOREFString[start+i]))
      temp_str[i] = GEOREFString[start+i];
    else
    {
      if( errorType == GEOREF_STR_LAT_MIN_ERROR )
        throw CoordinateConversionException( ErrorMessages::latitude_min );
      else ///if( errorType == GEOREF_STR_LON_MIN_ERROR )
        throw CoordinateConversionException( ErrorMessages::longitude_min );
    }
  }
  temp_str[length] = 0;
  *minutes = (double)atof(temp_str);  /* need atof, atoi can't handle 59999 */
  while (length > 2)
  {
    *minutes = *minutes / 10;
    length = length - 1;
  }
  if (*minutes > (double)MIN_PER_DEG)
    throw CoordinateConversionException( ErrorMessages::georefString );
} 


long roundGEOREF( double value )
{ 
/* Round value to nearest integer, using standard engineering rule */

  double ivalue;
  long ival;
  double fraction = modf (value, &ivalue);

  ival = (long)(ivalue);
  if ((fraction > 0.5) || ((fraction == 0.5) && (ival%2 == 1)))
    ival++;

  return ival;
} 


void convertMinutesToString( double minutes, long precision, char *str )
{ 
/*    
 *  This function converts minutes to a string of length precision.
 *
 *    minutes       : Minutes to be converted                  (input)
 *    precision     : Length of resulting string               (input)
 *    str           : String to hold converted minutes         (output)
 */

  double divisor;
  long min;
  divisor = pow (10.0, (5.0 - precision));
  if (minutes == 60.0)
    minutes = 59.999;
  minutes = minutes * 1000;
  min = roundGEOREF (minutes/divisor);
  sprintf (str, "%*.*ld", precision, precision, min);
  if (precision == 1)
    strcat (str, "0");
} 


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

GEOREF::GEOREF() :
  CoordinateSystem( 0, 0 )
{
}


GEOREF::GEOREF( const GEOREF &g )
{
  semiMajorAxis = g.semiMajorAxis;
  flattening = g.flattening;
}


GEOREF::~GEOREF()
{
}


GEOREF& GEOREF::operator=( const GEOREF &g )
{
  if( this != &g )
  {
    semiMajorAxis = g.semiMajorAxis;
    flattening = g.flattening;
  }

  return *this;
}


MSP::CCS::GEOREFCoordinates* GEOREF::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates, long precision )
{
/*   
 *  The function convertFromGeodetic converts Geodetic (latitude and longitude in radians)
 *  coordinates to a GEOREF coordinate string.  Precision specifies the
 *  number of digits in the GEOREF string for latitude and longitude:
 *                                  0 for nearest degree
 *                                  1 for nearest ten minutes
 *                                  2 for nearest minute
 *                                  3 for nearest tenth of a minute
 *                                  4 for nearest hundredth of a minute
 *                                  5 for nearest thousandth of a minute
 *
 *    longitude    : Longitude in radians.                  (input)
 *    latitude     : Latitude in radians.                   (input)
 *    precision    : Precision specified by the user.       (input)
 *    GEOREFString : GEOREF coordinate string.              (output)
 *
 */

  double long_min;                        /* GEOREF longitude minute part   */
  double lat_min;                         /* GEOREF latitude minute part    */
  double origin_long;                     /* Origin longitude (-180 degrees)*/
  double origin_lat;                      /* Origin latitude (-90 degrees)  */
  long letter_number[GEOREF_LETTERS + 1]; /* GEOREF letters                 */
  char long_min_str[MAX_PRECISION + 1];   /* Longitude minute string        */
  char lat_min_str[MAX_PRECISION + 1];    /* Latitude minute string         */
  long i;                                 /* counter in for loop            */
  char GEOREFString[21];

  double latitude = geodeticCoordinates->latitude() * RADIAN_TO_DEGREE;
  double longitude = geodeticCoordinates->longitude() * RADIAN_TO_DEGREE;

  if ((latitude < (double)LATITUDE_LOW) 
      || (latitude > (double)LATITUDE_HIGH))
    throw CoordinateConversionException( ErrorMessages::latitude );
  if ((longitude < (double)LONGITUDE_LOW) 
      || (longitude > (double)LONGITUDE_HIGH))
    throw CoordinateConversionException( ErrorMessages::longitude );
  if ((precision < 0) || (precision > MAX_PRECISION))
    throw CoordinateConversionException( ErrorMessages::precision );

  if (longitude > 180)
      longitude -= 360;

  origin_long = (double)LONGITUDE_LOW;
  origin_lat = (double)LATITUDE_LOW;
  letter_number[0] = (long)((longitude-origin_long) / QUAD + ROUND_ERROR);
  longitude = longitude - ((double)letter_number[0] * QUAD + origin_long);
  letter_number[2] = (long)(longitude + ROUND_ERROR);
  long_min = (longitude - (double)letter_number[2]) * (double)MIN_PER_DEG;
  letter_number[1] = (long)((latitude - origin_lat) / QUAD + ROUND_ERROR);
  latitude = latitude - ((double)letter_number[1] * QUAD + origin_lat);
  letter_number[3] = (long)(latitude + ROUND_ERROR);
  lat_min = (latitude - (double)letter_number[3]) * (double)MIN_PER_DEG;
  for (i = 0;i < 4; i++)
  {
    if (letter_number[i] >= LETTER_I)
      letter_number[i] += 1;
    if (letter_number[i] >= LETTER_O)
      letter_number[i] += 1;
  }

  if (letter_number[0] == 26)
  { /* longitude of 180 degrees */
    letter_number[0] = LETTER_Z;
    letter_number[2] = LETTER_Q;
    long_min = 59.999;
  }
  if (letter_number[1] == 13)
  { /* latitude of 90 degrees */
    letter_number[1] = LETTER_M;
    letter_number[3] = LETTER_Q;
    lat_min = 59.999;
  }

  for (i=0;i<4;i++)
    GEOREFString[i] = (char)(letter_number[i] + LETTER_A_OFFSET);
  GEOREFString[4] = 0;
  convertMinutesToString(long_min,precision,long_min_str);
  convertMinutesToString(lat_min,precision,lat_min_str);
  strcat(GEOREFString,long_min_str);
  strcat(GEOREFString,lat_min_str);

  return new GEOREFCoordinates( CoordinateType::georef, GEOREFString );
}


MSP::CCS::GeodeticCoordinates* GEOREF::convertToGeodetic( MSP::CCS::GEOREFCoordinates* georefCoordinates )
{
/*
 *  The function convertToGeodetic converts a GEOREF coordinate string to Geodetic (latitude
 *  and longitude in radians) coordinates.
 *
 *    GEOREFString : GEOREF coordinate string.     (input)
 *    longitude    : Longitude in radians.         (output)
 *    latitude     : Latitude in radians.          (output)
 *
 */

  long start;                /* Position in the GEOREF string                */
  long minutes_length;       /* length of minutes in the GEOREF string       */
  long georef_length;        /* length of GEOREF string                      */
  double origin_long;        /* Origin longitude                             */
  double origin_lat;         /* Origin latitude                              */
  double long_minutes;       /* Longitude minute part of GEOREF              */
  double lat_minutes;        /* Latitude minute part of GEOREF               */
  double longitude, latitude;

  origin_long = (double)LONGITUDE_LOW;
  origin_lat = (double)LATITUDE_LOW;

  char* GEOREFString = georefCoordinates->GEOREFString();

  georef_length = strlen(GEOREFString);
  if ((georef_length < GEOREF_MINIMUM) || (georef_length > GEOREF_MAXIMUM) 
      || ((georef_length % 2) != 0))
    throw CoordinateConversionException( ErrorMessages::georefString );

  extractDegrees( GEOREFString, &longitude, &latitude );
  start = GEOREF_LETTERS;
  minutes_length = (georef_length - start) / 2;

  extractMinutes(GEOREFString, start, minutes_length, 
                                GEOREF_STR_LON_MIN_ERROR, &long_minutes);

  extractMinutes(GEOREFString, (start+minutes_length),
                                minutes_length, GEOREF_STR_LAT_MIN_ERROR, &lat_minutes);

  latitude = latitude + origin_lat + lat_minutes / (double)MIN_PER_DEG;
  longitude = longitude + origin_long + long_minutes / (double)MIN_PER_DEG;
  latitude = latitude * DEGREE_TO_RADIAN;
  longitude = longitude * DEGREE_TO_RADIAN;

  return new GeodeticCoordinates( CoordinateType::geodetic, longitude, latitude );
}



// CLASSIFICATION: UNCLASSIFIED
