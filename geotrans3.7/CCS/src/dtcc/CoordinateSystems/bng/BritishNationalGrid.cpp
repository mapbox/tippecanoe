// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: BRITISH NATIONAL GRID
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates 
 *    (latitude and longitude) and British National Grid coordinates.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       BNG_NO_ERROR               : No errors occurred in function
 *       BNG_LAT_ERROR              : Latitude outside of valid range
 *                                      (49.5 to 61.5 degrees)
 *       BNG_LON_ERROR              : Longitude outside of valid range
 *                                      (-10.0 to 3.5 degrees)
 *       BNG_EASTING_ERROR          : Easting outside of valid range
 *                                      (depending on ellipsoid and
 *                                       projection parameters)
 *       BNG_NORTHING_ERROR         : Northing outside of valid range
 *                                      (depending on ellipsoid and
 *                                       projection parameters)
 *       BNG_STRING_ERROR           : A BNG string error: string too long,
 *                                      too short, or badly formed
 *       BNG_INVALID_AREA_ERROR     : Coordinate is outside of valid area
 *       BNG_ELLIPSOID_ERROR        : Invalid ellipsoid - must be Airy
 *
 * REUSE NOTES
 *
 *    BRITISH NATIONAL GRID is intended for reuse by any application that 
 *    performs a British National Grid projection or its inverse.
 *    
 * REFERENCES
 *
 *    Further information on BRITISH NATIONAL GRID can be found in the 
 *    Reuse Manual.
 *
 *    BRITISH NATIONAL GRID originated from :  
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
 *    BRITISH NATIONAL GRID has no restrictions.
 *
 * ENVIRONMENT
 *
 *    BRITISH NATIONAL GRID was tested and certified in the following 
 *    environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date             Description
 *    ----             -----------
 *    09-06-00         Original Code
 *    03-02-07         Original C++ Code
 *    3/23/2011 NGL    BAEts28583 Updated for memory leaks in convertToGeodetic 
 *                     and convertFromGeodetic. 
 * 
 *    1/19/2016        A. Layne MSP_DR30125 Updated to pass ellipsoid code to 
 *					   TransverseMercator 
 *
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "TransverseMercator.h"
#include "BritishNationalGrid.h"
#include "BNGCoordinates.h"
#include "EllipsoidParameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    ctype.h     - Standard C character handling library
 *    math.h      - Standard C math library
 *    stdio.h     - Standard C input/output library
 *    string.h    - Standard C string handling library
 *    TransverseMercator.h  - Is used to convert transverse mercator coordinates
 *    BritishNationalGrid.h       - Is for prototype error checking
 *    BNGCoordinates.h   - defines bng coordinates
 *    MapProjectionCoordinates.h   - defines map projection coordinates
 *    GeodeticCoordinates.h   - defines geodetic coordinates
 *    CoordinateConversionException.h - Exception handler
 *    ErrorMessages.h  - Contains exception messages
 */


using namespace MSP::CCS;


/***************************************************************************/
/*                               DEFINES                                   */

const double PI = 3.14159265358979323e0;              /* PI     */
const double PI_OVER_2 = (PI / 2.0e0);                /* PI over 2 */
const double TWO_PI = (2.0e0 * PI);                   /* 2 * PI */
const double MAX_LAT = (61.5 * PI / 180.0);           /* 61.5 degrees */
const double MIN_LAT = (49.5 * PI / 180.0);           /* 49.5 degrees */
const double MAX_LON = (3.5 * PI / 180.0);            /* 3.5 degrees */
const double MIN_LON = (-10.0 * PI / 180.0);          /* -10 degrees */
const char* BNG500GRID = "STNOHJ";                    /* 500,000 unit square identifications */
const char* BNG100GRID = "VWXYZQRSTULMNOPFGHJKABCDE"; /* 100,000 unit square identifications */

/* BNG projection Parameters */
const double BNG_Origin_Lat  = (49.0 * PI / 180.0); // Latitude of origin, in radians
const double BNG_Origin_Long = (-2.0 * PI / 180.0); // Longitude of origin, in radians
const double BNG_False_Northing = -100000.0;      // False northing, in meters
const double BNG_False_Easting  = 400000.0;       // False easting, in meters
const double BNG_Scale_Factor   = .9996012717;    // Scale factor

/* Maximum variance for easting and northing values for Airy. */
const double BNG_Max_Easting  = 759961.0;
const double BNG_Max_Northing = 1257875.0;
const double BNG_Min_Easting  = -133134.0;
const double BNG_Min_Northing = -14829.0;

static const char* Airy = "AA";

/************************************************************************/
//                           LOCAL FUNCTIONS

void findIndex( char letter, const char* letterArray, long *index )
{   
/*
 * The function findIndex searches for a given letter in an array. 
 * It returns the index of the letter in the array if the letter is found.
 * If the letter is not found, an error code is returned, otherwise
 * BNG_NO_ERROR is returned. 
 *
 *    letter              : Letter being searched for         
 *    letter_Array        : Array being searched         
 *    index               : Index of letter in array        
 */
  long i = 0;
  long not_Found = 1;
  long length = strlen(letterArray);

  while ((i < length) && (not_Found))
  {
    if (letter == letterArray[i])
    {
      *index = i;
      not_Found = 0;
    }
    i++;
  }
  if (not_Found)
    throw CoordinateConversionException( ErrorMessages::bngString );
} 


long roundBNG( double value )
{ 
/* Round value to nearest integer, using standard engineering rule */
  double ivalue;
  long ival;
  double fraction = modf (value, &ivalue);
  ival = (long)(ivalue);
  if ((fraction > 0.5) || ((fraction == 0.5) && (ival%2 == 1)))
    ival++;

  return (ival);
} 


void makeBNGString( char ltrnum[4], long easting, long northing, char* BNGString, long precision )
/* Construct a BNG string from its component parts */
{ 

  double divisor;
  long east;
  long north;
  long i;
  long j;
  double unitInterval;

  i = 0;
  for (j = 0; j < 3; j++)
    BNGString[i++] = ltrnum[j];
  divisor = pow (10.0, (5.0 - precision));
  unitInterval = pow (10.0, (double)precision);

  east = roundBNG (easting/divisor);
  if (east == unitInterval)
    east -= 1;
  if ((precision == 0) && (east == 1))
    east = 0;
  i += sprintf (BNGString + i, "%*.*ld", precision, precision, east);

  north = roundBNG (northing/divisor);
  if (north == unitInterval)
    north -= 1;
  if ((precision == 0) && (north == 1))
    north = 0;
  i += sprintf (BNGString + i, "%*.*ld",  precision, precision, north);
} 


bool checkOutOfArea( char BNG500, char BNG100 )
{ 
/*
 * The function checkOutOfArea checks if the 500,000 and 
 * 100,000 unit square identifications are within the valid area. If
 * they are not, an error code is returned, otherwise BNG_NO_ERROR 
 * is returned.
 *
 *    BNG500        : 500,000 square unit identification         
 *    BNG100        : 100,000 square unit identification         
 */

  bool outOfArea = false;

  switch (BNG500)
  {
    case 'S':
      switch (BNG100)
      {
        case 'A':
        case 'F':
        case 'L':
          outOfArea = true;
          break;
        default:
          break;
      }
      break;
    case 'N':
      switch (BNG100)
      {
        case 'V':
          outOfArea = true;
          break;
        default:
          break;
      }
      break;

    case 'H':
      if (BNG100 < 'L')
        outOfArea = true;
      break;
    case 'T':
      switch (BNG100)
      {
        case 'D':
        case 'E':
        case 'J':
        case 'K':
        case 'O':
        case 'P':
        case 'T':
        case 'U':
        case 'X':
        case 'Y':
        case 'Z':
          outOfArea = true;
          break;
        default:
          break;
      }
      break;
    case 'O':
      switch (BNG100)
      {
        case 'C':
        case 'D':
        case 'E':
        case 'J':
        case 'K':
        case 'O':
        case 'P':
        case 'T':
        case 'U':
        case 'Y':
        case 'Z':
          outOfArea = true;
          break;
        default:
          break;
      }
      break;
    case 'J':
      switch (BNG100)
      {
        case 'L':
        case 'M':
        case 'Q':
        case 'R':
        case 'V':
        case 'W':
       ///   Error_Code = BNG_NO_ERROR;
          break;
        default:
          outOfArea = true;
          break;
      }
      break;
    default:
      outOfArea = true;
      break;
  }

  return outOfArea;
} 


void breakBNGString( char* BNGString, char letters[3], double* easting, double* northing, long* precision )
{ 
/* Break down a BNG string into its component parts */

  long i = 0;
  long j;
  long num_digits = 0;
  long num_letters;
  long length = strlen(BNGString);

  while (BNGString[i] == ' ')
    i++;  /* skip any leading blanks */
  j = i;
  while (isalpha(BNGString[i]))
    i++;
  num_letters = i - j;
  if (num_letters == 2)
  {
    /* get letters */
    letters[0] = (char)toupper(BNGString[j]);
    letters[1] = (char)toupper(BNGString[j+1]);
    letters[2] = 0;
  }
  else
    throw CoordinateConversionException( ErrorMessages::bngString );

  checkOutOfArea(letters[0], letters[1]);
  while (BNGString[i] == ' ')
    i++;
  j = i;
  if (BNGString[length-1] == ' ')
    length --;
  while (i < length)
  {
    if (isdigit(BNGString[i]))
      i++;
    else
      throw CoordinateConversionException( ErrorMessages::bngString );
  }

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
    n = num_digits / 2;
    *precision = n;
    if (n > 0)
    {
      strncpy (east_string, BNGString+j, n);
      east_string[n] = 0;
      sscanf (east_string, "%ld", &east);
      strncpy (north_string, BNGString+j+n, n);
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
    throw CoordinateConversionException( ErrorMessages::bngString );
} 


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

BritishNationalGrid::BritishNationalGrid( char *ellipsoidCode ) :
  CoordinateSystem( 6377563.396, 1 / 299.324964600 ),
  transverseMercator( 0 ),
  BNG_Easting( 0.0 ),
  BNG_Northing( 0.0 )
{
/*
 * The constructor receives the ellipsoid code and sets
 * the corresponding state variables. If any errors occur, an exception is thrown 
 * with a description of the error.
 *
 *   ellipsoidCode : 2-letter code for ellipsoid           (input)
 */

  strcpy( BNG_Letters, "SV" );
  strcpy( BNG_Ellipsoid_Code, "AA");

  if ( strcmp( ellipsoidCode, Airy ) != 0 )
  { /* Ellipsoid must be Airy */
    throw CoordinateConversionException( ErrorMessages::bngEllipsoid );
  }
    
  strcpy( BNG_Ellipsoid_Code, ellipsoidCode );
  transverseMercator = new TransverseMercator(
     semiMajorAxis, flattening, BNG_Origin_Long, BNG_Origin_Lat,
     BNG_False_Easting, BNG_False_Northing, BNG_Scale_Factor, BNG_Ellipsoid_Code );
}


BritishNationalGrid::BritishNationalGrid( const BritishNationalGrid &bng )
{
  transverseMercator = new TransverseMercator( *( bng.transverseMercator ) );
  semiMajorAxis = bng.semiMajorAxis;
  flattening = bng.flattening;
  BNG_Easting = bng.BNG_Easting;     
  BNG_Northing = bng.BNG_Northing; 
}


BritishNationalGrid::~BritishNationalGrid()
{
  delete transverseMercator;
  transverseMercator = 0;
}


BritishNationalGrid& BritishNationalGrid::operator=( const BritishNationalGrid &bng )
{
  if( this != &bng )
  {
    transverseMercator->operator=( *bng.transverseMercator );
    semiMajorAxis = bng.semiMajorAxis;
    flattening = bng.flattening;
    BNG_Easting = bng.BNG_Easting;     
    BNG_Northing = bng.BNG_Northing; 
  }

  return *this;
}


EllipsoidParameters* BritishNationalGrid::getParameters() const
{
/*                         
 * The function getParameters returns the current ellipsoid
 * code.
 *
 *   ellipsoidCode : 2-letter code for ellipsoid          (output)
 */

  return new EllipsoidParameters( semiMajorAxis, flattening, (char*)BNG_Ellipsoid_Code );
}


MSP::CCS::BNGCoordinates* BritishNationalGrid::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates, long precision )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to a BNG coordinate string, according to the 
 * current ellipsoid parameters.  If any errors occur, an exception is thrown 
 * with a description of the error.
 * 
 *    longitude  : Longitude, in radians                   (input)
 *    latitude   : Latitude, in radians                    (input)
 *    precision  : Precision level of BNG string           (input)
 *    BNGString  : British National Grid coordinate string (output)
 *  
 */

  double TMEasting, TMNorthing;

  double longitude = geodeticCoordinates->longitude();
  double latitude  = geodeticCoordinates->latitude();

  if ((latitude < MIN_LAT) || (latitude > MAX_LAT))
  {  /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < MIN_LON) || (longitude > MAX_LON))
  {  /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  MapProjectionCoordinates* transverseMercatorCoordinates =
     transverseMercator->convertFromGeodetic( geodeticCoordinates );

  TMEasting  = transverseMercatorCoordinates->easting();
  TMNorthing = transverseMercatorCoordinates->northing();

  if ((TMEasting < 0.0) && (TMEasting > -2.0))
    transverseMercatorCoordinates->setEasting( 0.0 );
  if ((TMNorthing < 0.0) && (TMNorthing > -2.0))
    transverseMercatorCoordinates->setNorthing( 0.0 );

  TMEasting  = transverseMercatorCoordinates->easting();
  TMNorthing = transverseMercatorCoordinates->northing();

  if ((TMEasting < BNG_Min_Easting) || (TMEasting > BNG_Max_Easting))
  {
    delete transverseMercatorCoordinates;
    throw CoordinateConversionException( ErrorMessages::invalidArea );
  }
  if ((TMNorthing < BNG_Min_Northing) || (TMNorthing > BNG_Max_Northing))
  {
    delete transverseMercatorCoordinates;
    throw CoordinateConversionException( ErrorMessages::invalidArea );
  }

  BNGCoordinates* bngCoordinates = 0;
  try {
	bngCoordinates = convertFromTransverseMercator( transverseMercatorCoordinates, precision );
	delete transverseMercatorCoordinates;
  }
  catch ( CoordinateConversionException e ){
    delete transverseMercatorCoordinates;
    throw e;
  }

  return bngCoordinates;
}


MSP::CCS::GeodeticCoordinates* BritishNationalGrid::convertToGeodetic( MSP::CCS::BNGCoordinates* bngCoordinates )
{
/*
 * The function convertToGeodetic converts a BNG coordinate string 
 * to geodetic (latitude and longitude) coordinates, according to the current
 * ellipsoid parameters. If any errors occur, an exception is thrown 
 * with a description of the error.
 * 
 *    BNGString  : British National Grid coordinate string (input)
 *    longitude  : Longitude, in radians                   (output)
 *    latitude   : Latitude, in radians                    (output)
 *  
 */

  double TMEasting, TMNorthing;
  long in_Precision;

  char* BNGString = bngCoordinates->BNGString();
  
  breakBNGString( BNGString, BNG_Letters, &BNG_Easting, &BNG_Northing, &in_Precision );
  
  MapProjectionCoordinates* transverseMercatorCoordinates = convertToTransverseMercator( bngCoordinates );
  TMEasting = transverseMercatorCoordinates->easting();
  TMNorthing = transverseMercatorCoordinates->northing();

  if ((TMEasting < BNG_Min_Easting) || (TMEasting > BNG_Max_Easting))
  {
    delete transverseMercatorCoordinates;
    throw CoordinateConversionException( ErrorMessages::invalidArea );
  }
  if ((TMNorthing < BNG_Min_Northing) || (TMNorthing > BNG_Max_Northing))
  {
    delete transverseMercatorCoordinates;
    throw CoordinateConversionException( ErrorMessages::invalidArea );
  }

  GeodeticCoordinates* geodeticCoordinates = 0;
  double latitude;
  double longitude;
  try {
	geodeticCoordinates = transverseMercator->convertToGeodetic( transverseMercatorCoordinates );
    latitude = geodeticCoordinates->latitude();
    longitude = geodeticCoordinates->longitude();
    delete transverseMercatorCoordinates;
  }
  catch ( CoordinateConversionException e ){
    delete transverseMercatorCoordinates;
    throw e;
  }

  if ((latitude < MIN_LAT) || (latitude > MAX_LAT))
  {
    delete geodeticCoordinates;
    throw CoordinateConversionException( ErrorMessages::invalidArea );
  }
  if ((longitude < MIN_LON) || (longitude > MAX_LON))
  {
    delete geodeticCoordinates;
    throw CoordinateConversionException( ErrorMessages::invalidArea );
  }

  return geodeticCoordinates;
}


MSP::CCS::BNGCoordinates* BritishNationalGrid::convertFromTransverseMercator( MapProjectionCoordinates* mapProjectionCoordinates, long precision )
{
/*
 * The function convertFromTransverseMercator converts Transverse Mercator
 * (easting and northing) coordinates to a BNG coordinate string, according
 * to the current ellipsoid parameters.  If any errors occur, an exception is thrown 
 * with a description of the error.
 *
 *    easting    : Easting (X), in meters                  (input)
 *    northing   : Northing (Y), in meters                 (input)
 *    precision  : Precision level of BNG string           (input)
 *    BNGString  : British National Grid coordinate string (output)
 */

  char letters[4];
  long x, y;
  long index;
  long temp_Easting, temp_Northing;
  char BNGString[21];

  double easting = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < BNG_Min_Easting) || (easting > BNG_Max_Easting))
  { /* Easting out of range  */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < BNG_Min_Northing) || (northing > BNG_Max_Northing))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  temp_Easting  = roundBNG(easting);
  temp_Northing = roundBNG(northing);

  temp_Easting  += 1000000; 
  temp_Northing += 500000;

  x = temp_Easting / 500000;
  y = temp_Northing / 500000;

  index = y * 5 + x;
  if ((index >= 0) && (index < 25))
  {
    letters[0] = BNG100GRID[index];
    temp_Easting %= 500000;
    temp_Northing %= 500000;
    x = temp_Easting / 100000;
    y = temp_Northing / 100000;
    index = y * 5 + x;
    if ((index >= 0) && (index < 25))
    {
      letters[1] = BNG100GRID[index];

      if( checkOutOfArea(letters[0], letters[1]) )
      {
        throw CoordinateConversionException( ErrorMessages::invalidArea );
      }

      letters[2] = ' ';
      letters[3] = 0;
      temp_Easting %= 100000;
      temp_Northing %= 100000;
      makeBNGString(letters, temp_Easting, temp_Northing, BNGString, precision);
    }
    else
    {
      long five_y = 5 * y;
      if ((x >= (25 - five_y)) || (x < -five_y))
        throw CoordinateConversionException( ErrorMessages::easting );
      if ((five_y >= (25 - x)) || (five_y < -x))
      {
        throw CoordinateConversionException( ErrorMessages::northing );
      }
    }
  }
  else
  {
    long five_y = 5 * y;
    if ((x >= (25 - five_y)) || (x < -five_y))
      throw CoordinateConversionException( ErrorMessages::easting );
    if ((five_y >= (25 - x)) || (five_y < -x))
    {
       throw CoordinateConversionException( ErrorMessages::northing );
    }
  }

  return new BNGCoordinates( CoordinateType::britishNationalGrid, BNGString );
} 


MSP::CCS::MapProjectionCoordinates* BritishNationalGrid::convertToTransverseMercator( MSP::CCS::BNGCoordinates* bngCoordinates )
{ 
/*
 * The function convertToTransverseMercator converts a BNG coordinate string
 * to Transverse Mercator projection (easting and northing) coordinates 
 * according to the current ellipsoid parameters.  If any errors occur, an exception is thrown 
 * with a description of the error.
 * is returned.
 *
 *    BNGString  : British National Grid coordinate string (input)
 *    easting    : Easting (X), in meters                  (output)
 *    northing   : Northing (Y), in meters                 (output)
 */

  long northing_Offset, easting_Offset;
  long i = 0;
  long j = 0;
  long in_Precision;

  char* BNGString = bngCoordinates->BNGString();

  breakBNGString( BNGString, BNG_Letters, &BNG_Easting, &BNG_Northing, &in_Precision );

  findIndex(BNG_Letters[0], BNG500GRID, &i);

  northing_Offset = 500000 * (i / 2);
  easting_Offset = 500000 * (i % 2);

  findIndex(BNG_Letters[1], BNG100GRID, &j);

  northing_Offset += 100000 * (j / 5);
  easting_Offset += 100000 * (j % 5);

  double easting = BNG_Easting + easting_Offset;
  double northing = BNG_Northing + northing_Offset;

  return new MapProjectionCoordinates( CoordinateType::transverseMercator, easting, northing );
} 




// CLASSIFICATION: UNCLASSIFIED
