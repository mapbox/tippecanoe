// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER:  USNG
 *
 * ABSTRACT
 *
 *    This component converts between geodetic coordinates (latitude and
 *    longitude) and United States National Grid (USNG) coordinates.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          USNG_NO_ERROR          : No errors occurred in function
 *          USNG_LAT_ERROR         : Latitude outside of valid range
 *                                    (-90 to 90 degrees)
 *          USNG_LON_ERROR         : Longitude outside of valid range
 *                                    (-180 to 360 degrees)
 *          USNG_STR_ERROR         : An USNG string error: string too long,
 *                                    too short, or badly formed
 *          USNG_PRECISION_ERROR   : The precision must be between 0 and 5
 *                                    inclusive.
 *          USNG_A_ERROR           : Semi-major axis less than or equal to zero
 *          USNG_INV_F_ERROR       : Inverse flattening outside of valid range
 *                                    (250 to 350)
 *          USNG_EASTING_ERROR     : Easting outside of valid range
 *                                    (100,000 to 900,000 meters for UTM)
 *                                    (0 to 4,000,000 meters for UPS)
 *          USNG_NORTHING_ERROR    : Northing outside of valid range
 *                                    (0 to 10,000,000 meters for UTM)
 *                                    (0 to 4,000,000 meters for UPS)
 *          USNG_ZONE_ERROR        : Zone outside of valid range (1 to 60)
 *          USNG_HEMISPHERE_ERROR  : Invalid hemisphere ('N' or 'S')
 *
 * REUSE NOTES
 *
 *    USNG is intended for reuse by any application that does conversions
 *    between geodetic coordinates and USNG coordinates.
 *
 * REFERENCES
 *
 *    Further information on USNG can be found in the Reuse Manual.
 *
 *    USNG originated from : Federal Geographic Data Committee
 *                           590 National Center
 *                           12201 Sunrise Valley Drive
 *                           Reston, VA  22092
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *
 * ENVIRONMENT
 *
 *    USNG was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC version 2.8.1
 *    2. Windows XP with MS Visual C++ version 6
 *
 * MODIFICATIONS
 *
 * Date     Description
 * ----     -----------
 * 3-1-07   Original Code (cloned from MGRS)
 * 3/23/11  N. Lundgren BAEts28583 Updated for memory leaks in convert methods
 * 
 * 1/16/16  A. Layne MSP_DR30125 Updated to pass ellipsoid code in call to UTM.
 */

/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "UPS.h"
#include "UTM.h"
#include "USNG.h"
#include "EllipsoidParameters.h"
#include "MGRSorUSNGCoordinates.h"
#include "GeodeticCoordinates.h"
#include "UPSCoordinates.h"
#include "UTMCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"
#include "WarningMessages.h"

/*
 *      ctype.h     - Standard C character handling library
 *      math.h      - Standard C math library
 *      stdio.h     - Standard C input/output library
 *      string.h    - Standard C string handling library
 *      UPS.h       - Universal Polar Stereographic (UPS) projection
 *      UTM.h       - Universal Transverse Mercator (UTM) projection
 *      USNG.h      - function prototype error checking
 *      MGRSorUSNGCoordinates.h   - defines mgrs coordinates
 *      GeodeticCoordinates.h   - defines geodetic coordinates
 *      UPSCoordinates.h   - defines ups coordinates
 *      UTMCoordinates.h   - defines utm coordinates
 *      CoordinateConversionException.h - Exception handler
 *      ErrorMessages.h  - Contains exception messages
 *      WarningMessages.h  - Contains warning messages
 */

using namespace MSP::CCS;

/************************************************************************/
/*                               DEFINES
 *
 */

#define EPSILON 1.75e-7   /* approx 1.0e-5 degrees (~1 meter) in radians */

const int LETTER_A = 0;   /* ARRAY INDEX FOR LETTER A               */
const int LETTER_B = 1;   /* ARRAY INDEX FOR LETTER B               */
const int LETTER_C = 2;   /* ARRAY INDEX FOR LETTER C               */
const int LETTER_D = 3;   /* ARRAY INDEX FOR LETTER D               */
const int LETTER_E = 4;   /* ARRAY INDEX FOR LETTER E               */
const int LETTER_F = 5;   /* ARRAY INDEX FOR LETTER F               */
const int LETTER_G = 6;   /* ARRAY INDEX FOR LETTER G               */
const int LETTER_H = 7;   /* ARRAY INDEX FOR LETTER H               */
const int LETTER_I = 8;   /* ARRAY INDEX FOR LETTER I               */
const int LETTER_J = 9;   /* ARRAY INDEX FOR LETTER J               */
const int LETTER_K = 10;   /* ARRAY INDEX FOR LETTER K               */
const int LETTER_L = 11;   /* ARRAY INDEX FOR LETTER L               */
const int LETTER_M = 12;   /* ARRAY INDEX FOR LETTER M               */
const int LETTER_N = 13;   /* ARRAY INDEX FOR LETTER N               */
const int LETTER_O = 14;   /* ARRAY INDEX FOR LETTER O               */
const int LETTER_P = 15;   /* ARRAY INDEX FOR LETTER P               */
const int LETTER_Q = 16;   /* ARRAY INDEX FOR LETTER Q               */
const int LETTER_R = 17;   /* ARRAY INDEX FOR LETTER R               */
const int LETTER_S = 18;   /* ARRAY INDEX FOR LETTER S               */
const int LETTER_T = 19;   /* ARRAY INDEX FOR LETTER T               */
const int LETTER_U = 20;   /* ARRAY INDEX FOR LETTER U               */
const int LETTER_V = 21;   /* ARRAY INDEX FOR LETTER V               */
const int LETTER_W = 22;   /* ARRAY INDEX FOR LETTER W               */
const int LETTER_X = 23;   /* ARRAY INDEX FOR LETTER X               */
const int LETTER_Y = 24;   /* ARRAY INDEX FOR LETTER Y               */
const int LETTER_Z = 25;   /* ARRAY INDEX FOR LETTER Z               */
const double ONEHT = 100000.e0;    /* ONE HUNDRED THOUSAND           */
const double TWOMIL = 2000000.e0;  /* TWO MILLION                    */
const double PI = 3.14159265358979323e0;  /* PI                      */
const double PI_OVER_2 = (PI / 2.0e0);
const double PI_OVER_180 = (PI / 180.0e0);

const double MIN_EASTING = 100000.0;
const double MAX_EASTING = 900000.0;
const double MIN_NORTHING = 0.0;
const double MAX_NORTHING = 10000000.0;
const int MAX_PRECISION = 5;   /* Maximum precision of easting & northing */
const double MIN_USNG_NON_POLAR_LAT = -80.0 * ( PI / 180.0 ); /* -80 degrees in radians    */
const double MAX_USNG_NON_POLAR_LAT = 84.0 * ( PI / 180.0 );  /* 84 degrees in radians     */

const double MIN_EAST_NORTH = 0.0;
const double MAX_EAST_NORTH = 3999999.0;

const double _6 = (6.0 * (PI / 180.0));
const double _8 = (8.0 * (PI / 180.0));
const double _72 = (72.0 * (PI / 180.0));
const double _80 = (80.0 * (PI / 180.0));
const double _80_5 = (80.5 * (PI / 180.0));
const double _84_5 = (84.5 * (PI / 180.0));

#define _500000  500000.0

struct Latitude_Band
{
  long letter;            /* letter representing latitude band  */
  double min_northing;    /* minimum northing for latitude band */
  double north;           /* upper latitude for latitude band   */
  double south;           /* lower latitude for latitude band   */
  double northing_offset; /* latitude band northing offset      */
};

const Latitude_Band Latitude_Band_Table[20] =
  {{LETTER_C, 1100000.0, -72.0, -80.5, 0.0},
  {LETTER_D, 2000000.0, -64.0, -72.0, 2000000.0},
  {LETTER_E, 2800000.0, -56.0, -64.0, 2000000.0},
  {LETTER_F, 3700000.0, -48.0, -56.0, 2000000.0},
  {LETTER_G, 4600000.0, -40.0, -48.0, 4000000.0},
  {LETTER_H, 5500000.0, -32.0, -40.0, 4000000.0},
  {LETTER_J, 6400000.0, -24.0, -32.0, 6000000.0},
  {LETTER_K, 7300000.0, -16.0, -24.0, 6000000.0},
  {LETTER_L, 8200000.0, -8.0, -16.0, 8000000.0},
  {LETTER_M, 9100000.0, 0.0, -8.0, 8000000.0},
  {LETTER_N, 0.0, 8.0, 0.0, 0.0},
  {LETTER_P, 800000.0, 16.0, 8.0, 0.0},
  {LETTER_Q, 1700000.0, 24.0, 16.0, 0.0},
  {LETTER_R, 2600000.0, 32.0, 24.0, 2000000.0},
  {LETTER_S, 3500000.0, 40.0, 32.0, 2000000.0},
  {LETTER_T, 4400000.0, 48.0, 40.0, 4000000.0},
  {LETTER_U, 5300000.0, 56.0, 48.0, 4000000.0},
  {LETTER_V, 6200000.0, 64.0, 56.0, 6000000.0},
  {LETTER_W, 7000000.0, 72.0, 64.0, 6000000.0},
  {LETTER_X, 7900000.0, 84.5, 72.0, 6000000.0}};


struct UPS_Constant
{
  long letter;            /* letter representing latitude band      */
  long ltr2_low_value;    /* 2nd letter range - low number         */
  long ltr2_high_value;   /* 2nd letter range - high number          */
  long ltr3_high_value;   /* 3rd letter range - high number (UPS)   */
  double false_easting;   /* False easting based on 2nd letter      */
  double false_northing;  /* False northing based on 3rd letter     */
};

const UPS_Constant UPS_Constant_Table[4] =
  {{LETTER_A, LETTER_J, LETTER_Z, LETTER_Z, 800000.0, 800000.0},
  {LETTER_B, LETTER_A, LETTER_R, LETTER_Z, 2000000.0, 800000.0},
  {LETTER_Y, LETTER_J, LETTER_Z, LETTER_P, 800000.0, 1300000.0},
  {LETTER_Z, LETTER_A, LETTER_J, LETTER_P, 2000000.0, 1300000.0}};


/************************************************************************/
/*                              LOCAL FUNCTIONS
 *
 */

void makeUSNGString(
   char*  USNGString,
   long   zone,
   int    letters[USNG_LETTERS],
   double easting,
   double northing,
   long   precision )
{
/*
 * The function makeUSNGString constructs an USNG string
 * from its component parts.
 *
 *   USNGString     : USNG coordinate string          (output)
 *   zone           : UTM Zone                        (input)
 *   letters        : USNG coordinate string letters  (input)
 *   easting        : Easting value                   (input)
 *   northing       : Northing value                  (input)
 *   precision      : Precision level of USNG string  (input)
 */

  long i;
  long j;
  double divisor;
  long east;
  long north;
  char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  i = 0;
  if (zone)
    i = sprintf (USNGString+i,"%2.2ld",zone);
  else
    strncpy(USNGString, "  ", 2);  // 2 spaces

  for (j=0;j<3;j++)
    USNGString[i++] = alphabet[letters[j]];
  divisor = pow (10.0, (5.0 - precision));
  easting = fmod (easting, 100000.0);
  if (easting >= 99999.5)
    easting = 99999.0;
  east = (long)(easting/divisor);
  i += sprintf (USNGString+i, "%*.*ld", precision, precision, east);
  northing = fmod (northing, 100000.0);
  if (northing >= 99999.5)
    northing = 99999.0;
  north = (long)(northing/divisor);
  i += sprintf (USNGString+i, "%*.*ld", precision, precision, north);
}


void breakUSNGString(
   char*   USNGString,
   long*   zone,
   long    letters[USNG_LETTERS],
   double* easting,
   double* northing,
   long*   precision )
{
/*
 * The function breakUSNGString breaks down an USNG
 * coordinate string into its component parts.
 *
 *   USNG           : USNG coordinate string          (input)
 *   zone           : UTM Zone                        (output)
 *   letters        : USNG coordinate string letters  (output)
 *   easting        : Easting value                   (output)
 *   northing       : Northing value                  (output)
 *   precision      : Precision level of USNG string  (output)
 */

  long num_digits;
  long num_letters;
  long i = 0;
  long j = 0;

  while (USNGString[i] == ' ')
    i++;  /* skip any leading blanks */
  j = i;
  while (isdigit(USNGString[i]))
    i++;
  num_digits = i - j;
  if (num_digits <= 2)
    if (num_digits > 0)
    {
      char zone_string[3];
      /* get zone */
      strncpy (zone_string, USNGString+j, 2);
      zone_string[2] = 0;
      sscanf (zone_string, "%ld", zone);
      if ((*zone < 1) || (*zone > 60))
        throw CoordinateConversionException( ErrorMessages::usngString );
    }
    else
      *zone = 0;
  else
    throw CoordinateConversionException( ErrorMessages::usngString );
  j = i;

  while (isalpha(USNGString[i]))
    i++;
  num_letters = i - j;
  if (num_letters == 3)
  {
    /* get letters */
    letters[0] = (toupper(USNGString[j]) - (long)'A');
    if ((letters[0] == LETTER_I) || (letters[0] == LETTER_O))
      throw CoordinateConversionException( ErrorMessages::usngString );
    letters[1] = (toupper(USNGString[j+1]) - (long)'A');
    if ((letters[1] == LETTER_I) || (letters[1] == LETTER_O))
      throw CoordinateConversionException( ErrorMessages::usngString );
    letters[2] = (toupper(USNGString[j+2]) - (long)'A');
    if ((letters[2] == LETTER_I) || (letters[2] == LETTER_O))
      throw CoordinateConversionException( ErrorMessages::usngString );
  }
  else
    throw CoordinateConversionException( ErrorMessages::usngString );
  j = i;
  while (isdigit(USNGString[i]))
    i++;
  num_digits = i - j;
  if ((num_digits <= 10) && (num_digits%2 == 0))
  {
    long n;
    char east_string[6];
    char north_string[6];
    long east;
    long north;
    double multiplier;
    /* get easting & northing */
    n = num_digits/2;
    *precision = n;
    if (n > 0)
    {
      strncpy (east_string, USNGString+j, n);
      east_string[n] = 0;
      sscanf (east_string, "%ld", &east);
      strncpy (north_string, USNGString+j+n, n);
      north_string[n] = 0;
      sscanf (north_string, "%ld", &north);
      multiplier = pow (10.0, 5.0 - n);
      *easting = east * multiplier;
      *northing = north * multiplier;
    }
    else
    {
      *easting = 0.0;
      *northing = 0.0;
    }
  }
  else
    throw CoordinateConversionException( ErrorMessages::usngString );
}


/************************************************************************/
/*                              FUNCTIONS
 *
 */

USNG::USNG( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, char* ellipsoidCode ) :
  CoordinateSystem(),
  ups( 0 ),
  utm( 0 )
{
/*
 * The constructor receives the ellipsoid parameters and sets
 * the corresponding state variables. 
 * If any errors occur, an exception is thrown with a description of the error.
 *
 *   ellipsoidSemiMajorAxis   : Semi-major axis of ellipsoid in meters  (input)
 *   ellipsoidFlattening      : Flattening of ellipsoid                 (input)
 *   ellipsoid_Code           : 2-letter code for ellipsoid             (input)
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

  strncpy (USNGEllipsoidCode, ellipsoidCode, 2);
   USNGEllipsoidCode[2] = '\0';

  ups = new UPS( semiMajorAxis, flattening );

  utm = new UTM( semiMajorAxis, flattening, USNGEllipsoidCode, 0 );
}


USNG::USNG( const USNG &u )
{
  ups = new UPS( *( u.ups ) );
  utm = new UTM( *( u.utm ) );

  semiMajorAxis = u.semiMajorAxis;
  flattening = u.flattening;
  strcpy( USNGEllipsoidCode, u.USNGEllipsoidCode );
}


USNG::~USNG()
{
  delete ups;
  ups = 0;

  delete utm;
  utm = 0;
}


USNG& USNG::operator=( const USNG &u )
{
  if( this != &u )
  {
    ups->operator=( *u.ups );
    utm->operator=( *u.utm );

    semiMajorAxis = u.semiMajorAxis;
    flattening = u.flattening;
    strcpy( USNGEllipsoidCode, u.USNGEllipsoidCode );
  }

  return *this;
}


EllipsoidParameters* USNG::getParameters() const
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters.
 *
 *  ellipsoidSemiMajorAxis   : Semi-major axis of ellipsoid, in meters (output)
 *  ellipsoidFlattening      : Flattening of ellipsoid                 (output)
 *  ellipsoidCode            : 2-letter code for ellipsoid             (output)
 */

  return new EllipsoidParameters(
     semiMajorAxis, flattening, (char*)USNGEllipsoidCode );
}


MSP::CCS::MGRSorUSNGCoordinates* USNG::convertFromGeodetic(
   MSP::CCS::GeodeticCoordinates* geodeticCoordinates,
   long precision )
{
/*
 * The function convertFromGeodetic converts Geodetic (latitude and
 * longitude) coordinates to an USNG coordinate string, according to the
 * current ellipsoid parameters.
 * If any errors occur, an exception is thrown with a description of the error.
 *
 *    latitude      : Latitude in radians              (input)
 *    longitude     : Longitude in radians             (input)
 *    precision     : Precision level of USNG string   (input)
 *    USNGString    : USNG coordinate string           (output)
 *
 */

  MGRSorUSNGCoordinates* mgrsorUSNGCoordinates = 0;
  UTMCoordinates* utmCoordinates = 0;
  UPSCoordinates* upsCoordinates = 0;

  double latitude  = geodeticCoordinates->latitude();
  double longitude = geodeticCoordinates->longitude();

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  { /* latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > (2*PI)))
  { /* longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }
  if ((precision < 0) || (precision > MAX_PRECISION))
    throw CoordinateConversionException( ErrorMessages::precision  );

  // If the latitude is within the valid usng non polar range [-80, 84),
  // convert to usng using the utm path,
  // otherwise convert to usng using the ups path
  try 
  {
     if((latitude >= MIN_USNG_NON_POLAR_LAT) &&
        (latitude < MAX_USNG_NON_POLAR_LAT))
     {
        utmCoordinates = utm->convertFromGeodetic( geodeticCoordinates );
        mgrsorUSNGCoordinates = fromUTM(
           utmCoordinates, longitude, latitude, precision );
        delete utmCoordinates;
     }
     else
     {
        upsCoordinates = ups->convertFromGeodetic( geodeticCoordinates );
        mgrsorUSNGCoordinates = fromUPS( upsCoordinates, precision );
        delete upsCoordinates;
     }
  }
  catch ( CoordinateConversionException e ) {
     delete utmCoordinates;
     delete upsCoordinates;
     throw e;
  }

  return mgrsorUSNGCoordinates;
}


MSP::CCS::GeodeticCoordinates* USNG::convertToGeodetic(
   MSP::CCS::MGRSorUSNGCoordinates* usngCoordinates )
{
/*
 * The function convertToGeodetic converts an USNG coordinate string
 * to Geodetic (latitude and longitude) coordinates
 * according to the current ellipsoid parameters.
 * If any errors occur, an exception is thrown with a description of the error.
 *
 *    USNG       : USNG coordinate string           (input)
 *    latitude   : Latitude in radians              (output)
 *    longitude  : Longitude in radians             (output)
 *
 */

  long zone;
  long letters[USNG_LETTERS];
  double usng_easting;
  double usng_northing;
  long precision;
  GeodeticCoordinates* geodeticCoordinates = 0;
  UTMCoordinates* utmCoordinates = 0;
  UPSCoordinates* upsCoordinates = 0;

  breakUSNGString(
     usngCoordinates->MGRSString(),
     &zone, letters, &usng_easting, &usng_northing, &precision );

  try 
  {
     if( zone )
     {
        utmCoordinates = toUTM(
           zone, letters, usng_easting, usng_northing, precision );
        geodeticCoordinates = utm->convertToGeodetic( utmCoordinates );
        delete utmCoordinates;
     }
     else
     {
        upsCoordinates = toUPS( letters, usng_easting, usng_northing );
        geodeticCoordinates = ups->convertToGeodetic( upsCoordinates );
        delete upsCoordinates;
     }
  }
  catch ( CoordinateConversionException e )
  {
     delete utmCoordinates;
     delete upsCoordinates;
     throw e;
  }

  return geodeticCoordinates;
}


MSP::CCS::MGRSorUSNGCoordinates* USNG::convertFromUTM (
   UTMCoordinates* utmCoordinates, long precision )
{
/*
 * The function convertFromUTM converts UTM (zone, easting, and
 * northing) coordinates to an USNG coordinate string, according to the
 * current ellipsoid parameters.
 * If any errors occur, an exception is thrown with a description of the error.
 *
 *    zone       : UTM zone                         (input)
 *    hemisphere : North or South hemisphere        (input)
 *    easting    : Easting (X) in meters            (input)
 *    northing   : Northing (Y) in meters           (input)
 *    precision  : Precision level of USNG string   (input)
 *    USNGString : USNG coordinate string           (output)
 */

  long zone       = utmCoordinates->zone();
  char hemisphere = utmCoordinates->hemisphere();
  double easting  = utmCoordinates->easting();
  double northing = utmCoordinates->northing();

  if ((zone < 1) || (zone > 60))
    throw CoordinateConversionException( ErrorMessages::zone  );
  if ((hemisphere != 'S') && (hemisphere != 'N'))
    throw CoordinateConversionException( ErrorMessages::hemisphere  );
  if ((easting < MIN_EASTING) || (easting > MAX_EASTING))
    throw CoordinateConversionException( ErrorMessages::easting  );
  if ((northing < MIN_NORTHING) || (northing > MAX_NORTHING))
    throw CoordinateConversionException( ErrorMessages::northing  );
  if ((precision < 0) || (precision > MAX_PRECISION))
    throw CoordinateConversionException( ErrorMessages::precision  );

  GeodeticCoordinates* geodeticCoordinates = utm->convertToGeodetic(
     utmCoordinates );

  // If the latitude is within the valid mgrs non polar range [-80, 84),
  // convert to mgrs using the utm path,
  // otherwise convert to mgrs using the ups path
  MGRSorUSNGCoordinates* mgrsorUSNGCoordinates = 0;
  UPSCoordinates*        upsCoordinates = 0;
  double latitude = geodeticCoordinates->latitude();

  try
  {
     if((latitude >= (MIN_USNG_NON_POLAR_LAT - EPSILON)) &&
        (latitude <  (MAX_USNG_NON_POLAR_LAT + EPSILON)))
        mgrsorUSNGCoordinates = fromUTM(
           utmCoordinates, geodeticCoordinates->longitude(),
           latitude, precision );
     else
     {
        upsCoordinates = ups->convertFromGeodetic( geodeticCoordinates );
        mgrsorUSNGCoordinates = fromUPS( upsCoordinates, precision );
     }
  }
  catch ( CoordinateConversionException e )
  {
     delete geodeticCoordinates;
     delete upsCoordinates;
     throw e;
  }

  delete geodeticCoordinates;
  delete upsCoordinates;

  return mgrsorUSNGCoordinates;
}


MSP::CCS::UTMCoordinates* USNG::convertToUTM(
   MSP::CCS::MGRSorUSNGCoordinates* mgrsorUSNGCoordinates )
{
/*
 * The function convertToUTM converts an USNG coordinate string
 * to UTM projection (zone, hemisphere, easting and northing) coordinates
 * according to the current ellipsoid parameters.  If any errors occur,
 * an exception is thrown with a description of the error.
 *
 *    USNGString : USNG coordinate string           (input)
 *    zone       : UTM zone                         (output)
 *    hemisphere : North or South hemisphere        (output)
 *    easting    : Easting (X) in meters            (output)
 *    northing   : Northing (Y) in meters           (output)
 */

  long zone;
  long letters[USNG_LETTERS];
  double usng_easting, usng_northing;
  long precision;
  UTMCoordinates* utmCoordinates = 0;
  GeodeticCoordinates* geodeticCoordinates = 0;
  UPSCoordinates* upsCoordinates = 0;

  breakUSNGString( mgrsorUSNGCoordinates->MGRSString(),
     &zone, letters, &usng_easting, &usng_northing, &precision );
  try
  {
     if (zone)
     {
        utmCoordinates = toUTM(
           zone, letters, usng_easting, usng_northing, precision );
        // Convert to geodetic to make sure the coordinates are in the valid utm range 
        geodeticCoordinates = utm->convertToGeodetic( utmCoordinates );
     }
     else
     {
        upsCoordinates = toUPS( letters, usng_easting, usng_northing );
        geodeticCoordinates = ups->convertToGeodetic( upsCoordinates );
        utmCoordinates = utm->convertFromGeodetic( geodeticCoordinates );
     }
  }
  catch ( CoordinateConversionException e )
  {
     delete utmCoordinates;
     delete geodeticCoordinates;
     delete upsCoordinates;
     throw e;
  }

  delete geodeticCoordinates;
  delete upsCoordinates;

  return utmCoordinates;
}


MSP::CCS::MGRSorUSNGCoordinates* USNG::convertFromUPS(
   MSP::CCS::UPSCoordinates* upsCoordinates,
   long precision )
{
/*
 * The function convertFromUPS converts UPS (hemisphere, easting,
 * and northing) coordinates to an USNG coordinate string according to
 * the current ellipsoid parameters.  If any errors occur, an
 * exception is thrown with a description of the error.
 *
 *    hemisphere    : Hemisphere either 'N' or 'S'     (input)
 *    easting       : Easting/X in meters              (input)
 *    northing      : Northing/Y in meters             (input)
 *    precision     : Precision level of USNG string   (input)
 *    USNGString    : USNG coordinate string           (output)
 */

  int index = 0;

  char hemisphere = upsCoordinates->hemisphere();
  double easting  = upsCoordinates->easting();
  double northing = upsCoordinates->northing();

  if ((hemisphere != 'N') && (hemisphere != 'S'))
    throw CoordinateConversionException( ErrorMessages::hemisphere  );
  if ((easting < MIN_EAST_NORTH) || (easting > MAX_EAST_NORTH))
    throw CoordinateConversionException( ErrorMessages::easting  );
  if ((northing < MIN_EAST_NORTH) || (northing > MAX_EAST_NORTH))
    throw CoordinateConversionException( ErrorMessages::northing  );
  if ((precision < 0) || (precision > MAX_PRECISION))
    throw CoordinateConversionException( ErrorMessages::precision  );

  GeodeticCoordinates* geodeticCoordinates = ups->convertToGeodetic(
     upsCoordinates );

  // If the latitude is within valid mgrs polar range [-90, -80) or [84, 90],
  // convert to mgrs using the ups path,
  // otherwise convert to mgrs using the utm path
  MGRSorUSNGCoordinates* mgrsorUSNGCoordinates = 0;
  UTMCoordinates* utmCoordinates = 0;
  double latitude = geodeticCoordinates->latitude();

  try
  {
     if((latitude < (MIN_USNG_NON_POLAR_LAT + EPSILON)) ||
        (latitude >= (MAX_USNG_NON_POLAR_LAT - EPSILON)))
        mgrsorUSNGCoordinates = fromUPS( upsCoordinates, precision );
     else
     {
        utmCoordinates = utm->convertFromGeodetic( geodeticCoordinates );
        double longitude = geodeticCoordinates->longitude();
        mgrsorUSNGCoordinates = fromUTM(
           utmCoordinates, longitude, latitude, precision );
     }
  }
  catch ( CoordinateConversionException e )
  {
     delete geodeticCoordinates;
     delete utmCoordinates;
     throw e;
  }

  delete geodeticCoordinates;
  delete utmCoordinates;

  return mgrsorUSNGCoordinates;
}


MSP::CCS::UPSCoordinates* USNG::convertToUPS(
   MSP::CCS::MGRSorUSNGCoordinates* mgrsorUSNGCoordinates )
{
/*
 * The function convertToUPS converts an USNG coordinate string
 * to UPS (hemisphere, easting, and northing) coordinates, according
 * to the current ellipsoid parameters. If any errors occur, an
 * exception is thrown with a description of the error.
 *
 *    USNGString    : USNG coordinate string           (input)
 *    hemisphere    : Hemisphere either 'N' or 'S'     (output)
 *    easting       : Easting/X in meters              (output)
 *    northing      : Northing/Y in meters             (output)
 */

  long zone;
  long letters[USNG_LETTERS];
  long precision;
  double usng_easting;
  double usng_northing;
  int index = 0;
  UPSCoordinates* upsCoordinates = 0;
  GeodeticCoordinates* geodeticCoordinates = 0;
  UTMCoordinates* utmCoordinates = 0;

  breakUSNGString( mgrsorUSNGCoordinates->MGRSString(),
     &zone, letters, &usng_easting, &usng_northing, &precision );
  try
  {
     if( !zone )
     {
        upsCoordinates = toUPS( letters, usng_easting, usng_northing );
        // Convert to geodetic to ensure coordinates are in the valid ups range 
        geodeticCoordinates = ups->convertToGeodetic( upsCoordinates );
     }
     else
     {
        utmCoordinates = toUTM(
           zone, letters, usng_easting, usng_northing, precision );
        geodeticCoordinates = utm->convertToGeodetic( utmCoordinates );
        upsCoordinates = ups->convertFromGeodetic( geodeticCoordinates );
     }
  }
  catch ( CoordinateConversionException e )
  {
     delete geodeticCoordinates;
     delete utmCoordinates;
     delete upsCoordinates;
     throw e;
  }

  delete geodeticCoordinates;
  delete utmCoordinates;

  return upsCoordinates;
}


MSP::CCS::MGRSorUSNGCoordinates* USNG::fromUTM(
   MSP::CCS::UTMCoordinates* utmCoordinates,
   double longitude,
   double latitude,
   long precision )
{
/*
 * The function fromUTM calculates an USNG coordinate string
 * based on the zone, latitude, easting and northing.
 *
 *    zone       : Zone number             (input)
 *    latitude   : Latitude in radians     (input)
 *    easting    : Easting                 (input)
 *    northing   : Northing                (input)
 *    precision  : Precision               (input)
 *    USNGString : USNG coordinate string  (output)
 */

  double pattern_offset;      /* Pattern offset for 3rd letter               */
  double grid_northing;       /* Northing used to derive 3rd letter of USNG  */
  long ltr2_low_value;        /* 2nd letter range - low number               */
  long ltr2_high_value;       /* 2nd letter range - high number              */
  int letters[USNG_LETTERS];  /* Number location of 3 letters in alphabet    */
  char USNGString[21];
  long override = 0;
  long natural_zone;     

  long zone = utmCoordinates->zone();
  char hemisphere = utmCoordinates->hemisphere();
  double easting = utmCoordinates->easting();
  double northing = utmCoordinates->northing();

  getLatitudeLetter( latitude, &letters[0] );

  // Check if the point is within it's natural zone
  // If it is not, put it there
  if (longitude < PI)
    natural_zone = (long)(31 + ((longitude) / _6));
  else
    natural_zone = (long)(((longitude) / _6) - 29);

  if (natural_zone > 60)
      natural_zone = 1;

  if (zone != natural_zone) 
  { // reconvert to override zone
    UTM utmOverride( semiMajorAxis, flattening, USNGEllipsoidCode, natural_zone );
    GeodeticCoordinates geodeticCoordinates(
       CoordinateType::geodetic, longitude, latitude );
    UTMCoordinates* utmCoordinatesOverride = utmOverride.convertFromGeodetic(
       &geodeticCoordinates );

    zone       = utmCoordinatesOverride->zone();
    hemisphere = utmCoordinatesOverride->hemisphere();
    easting    = utmCoordinatesOverride->easting();
    northing   = utmCoordinatesOverride->northing();

    delete utmCoordinatesOverride;
    utmCoordinatesOverride = 0;
  }

  /* UTM special cases */
  if (letters[0] == LETTER_V) // V latitude band
  {
    if ((zone == 31) && (easting >= _500000))
      override = 32;  // extension of zone 32V
  }
  else if (letters[0] == LETTER_X)
  {
     if ((zone == 32) && (easting < _500000)) // extension of zone 31X
        override = 31;  
     else if (((zone == 32) && (easting >= _500000)) || // western extension of zone 33X
        ((zone == 34) && (easting < _500000))) // eastern extension of zone 33X
        override = 33;  
     else if (((zone == 34) && (easting >= _500000)) || // western extension of zone 35X
        ((zone == 36) && (easting < _500000))) // eastern extension of zone 35X
        override = 35;  
     else if ((zone == 36) && (easting >= _500000)) // western extension of zone 37X
        override = 37;  
  }

  if (override) 
  { // reconvert to override zone
     UTM utmOverride( semiMajorAxis, flattening, USNGEllipsoidCode, override );
     GeodeticCoordinates geodeticCoordinates(
        CoordinateType::geodetic, longitude, latitude );
     UTMCoordinates* utmCoordinatesOverride = 
        utmOverride.convertFromGeodetic( &geodeticCoordinates );

     zone       = utmCoordinatesOverride->zone();
     hemisphere = utmCoordinatesOverride->hemisphere();
     easting    = utmCoordinatesOverride->easting();
     northing   = utmCoordinatesOverride->northing();

     delete utmCoordinatesOverride;
     utmCoordinatesOverride = 0;
  }

  /* Truncate easting and northing values */
  double divisor = pow (10.0, (5.0 - precision));
  easting        = (long)(easting/divisor) * divisor;
  northing       = (long)(northing/divisor) * divisor;

  if( latitude <= 0.0 && northing == 1.0e7)
  {
    latitude = 0.0;
    northing = 0.0;
  }

  getGridValues( zone, &ltr2_low_value, &ltr2_high_value, &pattern_offset );

  grid_northing = northing;

  while (grid_northing >= TWOMIL)
  {
    grid_northing = grid_northing - TWOMIL;
  }
  grid_northing = grid_northing + pattern_offset;
  if(grid_northing >= TWOMIL)
    grid_northing = grid_northing - TWOMIL;

  letters[2] = (long)(grid_northing / ONEHT);
  if (letters[2] > LETTER_H)
    letters[2] = letters[2] + 1;

  if (letters[2] > LETTER_N)
    letters[2] = letters[2] + 1;

  letters[1] = ltr2_low_value + ((long)(easting / ONEHT) -1);
  if ((ltr2_low_value == LETTER_J) && (letters[1] > LETTER_N))
    letters[1] = letters[1] + 1;

  makeUSNGString( USNGString, zone, letters, easting, northing, precision );

  return new MGRSorUSNGCoordinates( CoordinateType::usNationalGrid, USNGString);
}


MSP::CCS::UTMCoordinates* USNG::toUTM(
   long   zone,
   long   letters[USNG_LETTERS],
   double easting,
   double northing,
   long   precision )
{
/*
 * The function toUTM converts an USNG coordinate string
 * to UTM projection (zone, hemisphere, easting and northing) coordinates
 * according to the current ellipsoid parameters.  If any errors occur,
 * an exception is thrown with a description of the error.
 *
 *    USNGString : USNG coordinate string           (input)
 *    zone       : UTM zone                         (output)
 *    hemisphere : North or South hemisphere        (output)
 *    easting    : Easting (X) in meters            (output)
 *    northing   : Northing (Y) in meters           (output)
 */

  char hemisphere;
  double min_northing;
  double northing_offset;
  long ltr2_low_value;
  long ltr2_high_value;
  double pattern_offset;
  double upper_lat_limit;     /* North latitude limits based on 1st letter  */
  double lower_lat_limit;     /* South latitude limits based on 1st letter  */
  double grid_easting;        /* Easting for 100,000 meter grid square      */
  double grid_northing;       /* Northing for 100,000 meter grid square     */
  double temp_grid_northing = 0.0;
  double fabs_grid_northing = 0.0;
  double latitude  = 0.0;
  double longitude = 0.0;
  double divisor   = 1.0;
  UTMCoordinates* utmCoordinates = 0;

  if((letters[0] == LETTER_X) && ((zone == 32) || (zone == 34) || (zone == 36)))
     throw CoordinateConversionException( ErrorMessages::usngString );
  else if ((letters[0] == LETTER_V) && (zone == 31) && (letters[1] > LETTER_D))
     throw CoordinateConversionException( ErrorMessages::usngString );
  else
  {
     if (letters[0] < LETTER_N)
        hemisphere = 'S';
     else
        hemisphere = 'N';

     getGridValues(zone, &ltr2_low_value, &ltr2_high_value, &pattern_offset);

    /* Check that the second letter of the USNG string is within
     * the range of valid second letter values
     * Also check that the third letter is valid */
     if((letters[1] < ltr2_low_value) ||
        (letters[1] > ltr2_high_value) ||
        (letters[2] > LETTER_V))
        throw CoordinateConversionException( ErrorMessages::usngString );

     grid_easting = (double)((letters[1]) - ltr2_low_value + 1) * ONEHT;
     if ((ltr2_low_value == LETTER_J) && (letters[1] > LETTER_O))
        grid_easting = grid_easting - ONEHT;

     double row_letter_northing = (double)(letters[2]) * ONEHT;
     if (letters[2] > LETTER_O)
        row_letter_northing = row_letter_northing - ONEHT;

     if (letters[2] > LETTER_I)
        row_letter_northing = row_letter_northing - ONEHT;

     if (row_letter_northing >= TWOMIL)
        row_letter_northing = row_letter_northing - TWOMIL;

     getLatitudeBandMinNorthing(letters[0], &min_northing, &northing_offset);

     grid_northing = row_letter_northing - pattern_offset;
     if(grid_northing < 0)
        grid_northing += TWOMIL;

     grid_northing += northing_offset;

     if(grid_northing < min_northing)
        grid_northing += TWOMIL;

     easting = grid_easting + easting;
     northing = grid_northing + northing;

     utmCoordinates = new UTMCoordinates(
        CoordinateType::universalTransverseMercator,
        zone, hemisphere, easting, northing );

     /* check that point is within Zone Letter bounds */
     GeodeticCoordinates* geodeticCoordinates = 0;
     try
     {
        geodeticCoordinates = utm->convertToGeodetic( utmCoordinates );
     }
     catch ( CoordinateConversionException e )
     {
        delete utmCoordinates;
        throw e;
     }

     divisor = pow (10.0, (double)precision);
     getLatitudeRange(letters[0], &upper_lat_limit, &lower_lat_limit);

     double latitude = geodeticCoordinates->latitude();

     delete geodeticCoordinates;

     if(!(((lower_lat_limit - PI_OVER_180/divisor) <= latitude) &&
         (latitude <= (upper_lat_limit + PI_OVER_180/divisor))))
        utmCoordinates->setWarningMessage( MSP::CCS::WarningMessages::latitude);
  }

  return utmCoordinates;
}


MSP::CCS::MGRSorUSNGCoordinates* USNG::fromUPS(
   MSP::CCS::UPSCoordinates* upsCoordinates,
   long precision )
{
/*
 * The function fromUPS converts UPS (hemisphere, easting,
 * and northing) coordinates to an USNG coordinate string according to
 * the current ellipsoid parameters.
 *
 *    hemisphere    : Hemisphere either 'N' or 'S'     (input)
 *    easting       : Easting/X in meters              (input)
 *    northing      : Northing/Y in meters             (input)
 *    precision     : Precision level of USNG string   (input)
 *    USNGString    : USNG coordinate string           (output)
 */

  double false_easting;      /* False easting for 2nd letter                 */
  double false_northing;     /* False northing for 3rd letter                */
  double grid_easting;       /* Easting used to derive 2nd letter of USNG    */
  double grid_northing;      /* Northing used to derive 3rd letter of USNG   */
  long ltr2_low_value;       /* 2nd letter range - low number                */
  int letters[USNG_LETTERS]; /* Number location of 3 letters in alphabet     */
  double divisor;
  int index = 0;
  char USNGString[21];

  char hemisphere = upsCoordinates->hemisphere();
  double easting  = upsCoordinates->easting();
  double northing = upsCoordinates->northing();

  divisor  = pow (10.0, (5.0 - precision));
  easting  = (long)(easting/divisor) * divisor;
  northing = (long)(northing/divisor) * divisor;

  if (hemisphere == 'N')
  {
    if (easting >= TWOMIL)
      letters[0] = LETTER_Z;
    else
      letters[0] = LETTER_Y;

    index = letters[0] - 22;
    ltr2_low_value = UPS_Constant_Table[index].ltr2_low_value;
    false_easting = UPS_Constant_Table[index].false_easting;
    false_northing = UPS_Constant_Table[index].false_northing;
  }
  else
  {
    if (easting >= TWOMIL)
      letters[0] = LETTER_B;
    else
      letters[0] = LETTER_A;

    ltr2_low_value = UPS_Constant_Table[letters[0]].ltr2_low_value;
    false_easting = UPS_Constant_Table[letters[0]].false_easting;
    false_northing = UPS_Constant_Table[letters[0]].false_northing;
  }

  grid_northing = northing;
  grid_northing = grid_northing - false_northing;
  letters[2] = (long)(grid_northing / ONEHT);

  if (letters[2] > LETTER_H)
    letters[2] = letters[2] + 1;

  if (letters[2] > LETTER_N)
    letters[2] = letters[2] + 1;

  grid_easting = easting;
  grid_easting = grid_easting - false_easting;
  letters[1] = ltr2_low_value + ((long)(grid_easting / ONEHT));

  if (easting < TWOMIL)
  {
    if (letters[1] > LETTER_L)
      letters[1] = letters[1] + 3;

    if (letters[1] > LETTER_U)
      letters[1] = letters[1] + 2;
  }
  else
  {
    if (letters[1] > LETTER_C)
      letters[1] = letters[1] + 2;

    if (letters[1] > LETTER_H)
      letters[1] = letters[1] + 1;

    if (letters[1] > LETTER_L)
      letters[1] = letters[1] + 3;
  }

  makeUSNGString( USNGString, 0, letters, easting, northing, precision );

  return new MGRSorUSNGCoordinates( CoordinateType::usNationalGrid, USNGString);
}


MSP::CCS::UPSCoordinates* USNG::toUPS(
   long letters[USNG_LETTERS],
   double easting,
   double northing )
{
/*
 * The function toUPS converts an USNG coordinate string
 * to UPS (hemisphere, easting, and northing) coordinates, according
 * to the current ellipsoid parameters. If any errors occur, an
 * exception is thrown with a description of the error.
 *
 *    USNGString    : USNG coordinate string           (input)
 *    hemisphere    : Hemisphere either 'N' or 'S'     (output)
 *    easting       : Easting/X in meters              (output)
 *    northing      : Northing/Y in meters             (output)
 */

  long ltr2_high_value;       /* 2nd letter range - high number             */
  long ltr3_high_value;       /* 3rd letter range - high number (UPS)       */
  long ltr2_low_value;        /* 2nd letter range - low number              */
  double false_easting;       /* False easting for 2nd letter               */
  double false_northing;      /* False northing for 3rd letter              */
  double grid_easting;        /* easting for 100,000 meter grid square      */
  double grid_northing;       /* northing for 100,000 meter grid square     */
  char hemisphere;
  int index = 0;

  if ((letters[0] == LETTER_Y) || (letters[0] == LETTER_Z))
  {
    hemisphere = 'N';

    index = letters[0] - 22;
    ltr2_low_value = UPS_Constant_Table[index].ltr2_low_value;
    ltr2_high_value = UPS_Constant_Table[index].ltr2_high_value;
    ltr3_high_value = UPS_Constant_Table[index].ltr3_high_value;
    false_easting = UPS_Constant_Table[index].false_easting;
    false_northing = UPS_Constant_Table[index].false_northing;
  }
  else if ((letters[0] == LETTER_A) || (letters[0] == LETTER_B))
  {
    hemisphere = 'S';

    ltr2_low_value = UPS_Constant_Table[letters[0]].ltr2_low_value;
    ltr2_high_value = UPS_Constant_Table[letters[0]].ltr2_high_value;
    ltr3_high_value = UPS_Constant_Table[letters[0]].ltr3_high_value;
    false_easting = UPS_Constant_Table[letters[0]].false_easting;
    false_northing = UPS_Constant_Table[letters[0]].false_northing;
  }
  else
    throw CoordinateConversionException( ErrorMessages::usngString );

  /* Check that the second letter of the USNG string is within
   * the range of valid second letter values
   * Also check that the third letter is valid */
  if ((letters[1] < ltr2_low_value) || (letters[1] > ltr2_high_value) ||
      ((letters[1] == LETTER_D) || (letters[1] == LETTER_E) ||
      (letters[1] == LETTER_M) || (letters[1] == LETTER_N) ||
      (letters[1] == LETTER_V) || (letters[1] == LETTER_W)) ||
      (letters[2] > ltr3_high_value))
    throw CoordinateConversionException( ErrorMessages::usngString );

  grid_northing = (double)letters[2] * ONEHT + false_northing;
  if (letters[2] > LETTER_I)
    grid_northing = grid_northing - ONEHT;

  if (letters[2] > LETTER_O)
    grid_northing = grid_northing - ONEHT;

  grid_easting = (double)((letters[1]) - ltr2_low_value) * ONEHT +false_easting;
  if (ltr2_low_value != LETTER_A)
  {
    if (letters[1] > LETTER_L)
      grid_easting = grid_easting - 300000.0;

    if (letters[1] > LETTER_U)
      grid_easting = grid_easting - 200000.0;
  }
  else
  {
    if (letters[1] > LETTER_C)
      grid_easting = grid_easting - 200000.0;

    if (letters[1] > LETTER_I)
      grid_easting = grid_easting - ONEHT;

    if (letters[1] > LETTER_L)
      grid_easting = grid_easting - 300000.0;
  }

  easting = grid_easting + easting;
  northing = grid_northing + northing;

  return new UPSCoordinates(
     CoordinateType::universalPolarStereographic,
     hemisphere, easting, northing);
}


void USNG::getGridValues(
   long    zone,
   long*   ltr2_low_value,
   long*   ltr2_high_value,
   double *pattern_offset )
{
/*
 * The function getGridValues sets the letter range used for
 * the 2nd letter in the USNG coordinate string, based on the set
 * number of the utm zone. It also sets the pattern offset using a
 * value of A for the second letter of the grid square, based on
 * the grid pattern and set number of the utm zone.
 *
 *    zone            : Zone number             (input)
 *    ltr2_low_value  : 2nd letter low number   (output)
 *    ltr2_high_value : 2nd letter high number  (output)
 *    pattern_offset  : Pattern offset          (output)
 */

  long set_number;    /* Set number (1-6) based on UTM zone number */

  set_number = zone % 6;

  if (!set_number)
    set_number = 6;

  if ((set_number == 1) || (set_number == 4))
  {
    *ltr2_low_value = LETTER_A;
    *ltr2_high_value = LETTER_H;
  }
  else if ((set_number == 2) || (set_number == 5))
  {
    *ltr2_low_value = LETTER_J;
    *ltr2_high_value = LETTER_R;
  }
  else if ((set_number == 3) || (set_number == 6))
  {
    *ltr2_low_value = LETTER_S;
    *ltr2_high_value = LETTER_Z;
  }

  /* False northing at A for second letter of grid square */
  if ((set_number % 2) ==  0)
    *pattern_offset = 500000.0;
  else
    *pattern_offset = 0.0;
}


void USNG::getLatitudeBandMinNorthing(
   long    letter,
   double* min_northing,
   double* northing_offset )
{
/*
 * The function getLatitudeBandMinNorthing receives a latitude band letter
 * and uses the Latitude_Band_Table to determine the minimum northing
 * and northing offset for that latitude band letter.
 *
 *   letter          : Latitude band letter             (input)
 *   min_northing    : Minimum northing for that letter (output)
 *   northing_offset : Latitude band northing offset    (output)
 */

  if ((letter >= LETTER_C) && (letter <= LETTER_H))
  {
    *min_northing = Latitude_Band_Table[letter-2].min_northing;
    *northing_offset = Latitude_Band_Table[letter-2].northing_offset;
  }
  else if ((letter >= LETTER_J) && (letter <= LETTER_N))
  {
    *min_northing = Latitude_Band_Table[letter-3].min_northing;
    *northing_offset = Latitude_Band_Table[letter-3].northing_offset;
  }
  else if ((letter >= LETTER_P) && (letter <= LETTER_X))
  {
    *min_northing = Latitude_Band_Table[letter-4].min_northing;
    *northing_offset = Latitude_Band_Table[letter-4].northing_offset;
  }
  else
    throw CoordinateConversionException( ErrorMessages::usngString );
}


void USNG::getLatitudeRange( long letter, double* north, double* south )
{
/*
 * The function getLatitudeRange receives a latitude band letter
 * and uses the Latitude_Band_Table to determine the latitude band
 * boundaries for that latitude band letter.
 *
 *   letter   : Latitude band letter                        (input)
 *   north    : Northern latitude boundary for that letter  (output)
 *   north    : Southern latitude boundary for that letter  (output)
 */

  if ((letter >= LETTER_C) && (letter <= LETTER_H))
  {
    *north = Latitude_Band_Table[letter-2].north * PI_OVER_180;
    *south = Latitude_Band_Table[letter-2].south * PI_OVER_180;
  }
  else if ((letter >= LETTER_J) && (letter <= LETTER_N))
  {
    *north = Latitude_Band_Table[letter-3].north * PI_OVER_180;
    *south = Latitude_Band_Table[letter-3].south * PI_OVER_180;
  }
  else if ((letter >= LETTER_P) && (letter <= LETTER_X))
  {
    *north = Latitude_Band_Table[letter-4].north * PI_OVER_180;
    *south = Latitude_Band_Table[letter-4].south * PI_OVER_180;
  }
  else
    throw CoordinateConversionException( ErrorMessages::usngString );
}


void USNG::getLatitudeLetter( double latitude, int* letter )
{
/*
 * The function getLatitudeLetter receives a latitude value
 * and uses the Latitude_Band_Table to determine the latitude band
 * letter for that latitude.
 *
 *   latitude   : Latitude              (input)
 *   letter     : Latitude band letter  (output)
 */

  long band = 0;

  if (latitude >= _72 && latitude < _84_5)
    *letter = LETTER_X;
  else if (latitude > -_80_5 && latitude < _72)
  {
    band = (long)(((latitude + _80) / _8) + 1.0e-12);
    if(band < 0)
      band = 0;
    *letter = Latitude_Band_Table[band].letter;
  }
  else
    throw CoordinateConversionException( ErrorMessages::latitude );
}

// CLASSIFICATION: UNCLASSIFIED
