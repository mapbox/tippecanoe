// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: Datum Library Implementation
 *
 * ABSTRACT
 *
 *    This component provides datum shifts for a large collection of local
 *    datums, WGS72, and WGS84.  A particular datum can be accessed by using its
 *    standard 5-letter code to find its index in the datum table.  The index
 *    can then be used to retrieve the name, type, ellipsoid code, and datum
 *    shift parameters, and to perform shifts to or from that datum.
 *
 *    By sequentially retrieving all of the datum codes and/or names, a menu
 *    of the available datums can be constructed.  The index values resulting
 *    from selections from this menu can then be used to access the parameters
 *    of the selected datum, or to perform datum shifts involving that datum.
 *
 *    This component supports both 3-parameter local datums, for which only X,
 *    Y, and Z translations relative to WGS 84 have been defined, and
 *    7-parameter local datums, for which X, Y, and Z rotations, and a scale
 *    factor, are also defined.  It also includes entries for WGS 84 (with an
 *    index of 0), and WGS 72 (with an index of 1), but no shift parameter
 *    values are defined for these.
 *
 *    This component provides datum shift functions for both geocentric and
 *    geodetic coordinates.  WGS84 is used as an intermediate state when
 *    shifting from one local datum to another.  When geodetic coordinates are
 *    given Molodensky's method is used, except near the poles where the 3-step
 *    step method is used instead.  Specific algorithms are used for shifting
 *    between WGS72 and WGS84.
 *
 *    This component depends on two data files, named 3_param.dat and
 *    7_param.dat, which contain the datum parameter values.  Copies of these
 *    files must be located in the directory specified by the value of the
 *    environment variable "MSPCCS_DATA", if defined, or else in the current
 *    directory whenever a program containing this component is executed.
 *
 *    Additional datums can be added to these files, either manually or using
 *    the Create_Datum function.  However, if a large number of datums are
 *    added, the datum table array sizes in this component will have to be
 *    increased.
 *
 *    This component depends on two other components: the Ellipsoid component
 *    for access to ellipsoid parameters; and the Geocentric component for
 *    conversions between geodetic and geocentric coordinates.
 *
 * ERROR HANDLING
 *
 *    This component checks for input file errors and input parameter errors.
 *    If an invalid value is found, the error code is combined with the current
 *    error code using the bitwise or.  This combining allows multiple error
 *    codes to be returned. The possible error codes are:
 *
 *  DATUM_NO_ERROR                  : No errors occurred in function
 *  DATUM_NOT_INITIALIZED_ERROR     : Datum module has not been initialized
 *  DATUM_7PARAM_FILE_OPEN_ERROR    : 7 parameter file opening error
 *  DATUM_7PARAM_FILE_PARSING_ERROR : 7 parameter file structure error
 *  DATUM_3PARAM_FILE_OPEN_ERROR    : 3 parameter file opening error
 *  DATUM_3PARAM_FILE_PARSING_ERROR : 3 parameter file structure error
 *  DATUM_INVALID_INDEX_ERROR       : Index out of valid range (less than one
 *                                      or more than Number_of_Datums)
 *  DATUM_INVALID_SRC_INDEX_ERROR   : Source datum index invalid
 *  DATUM_INVALID_DEST_INDEX_ERROR  : Destination datum index invalid
 *  DATUM_INVALID_CODE_ERROR        : Datum code not found in table
 *  DATUM_LAT_ERROR                 : Latitude out of valid range (-90 to 90)
 *  DATUM_LON_ERROR                 : Longitude out of valid range (-180 to
 *                                    360)
 *  DATUM_SIGMA_ERROR               : Standard error values must be positive
 *                                    (or -1 if unknown)
 *  DATUM_DOMAIN_ERROR              : Domain of validity not well defined
 *  DATUM_ELLIPSE_ERROR             : Error in ellipsoid module
 *  DATUM_NOT_USERDEF_ERROR         : Datum code is not user defined - cannot
 *                                    be deleted
 *
 *
 * REUSE NOTES
 *
 *    Datum is intended for reuse by any application that needs access to
 *    datum shift parameters relative to WGS 84.
 *
 *
 * REFERENCES
 *
 *    Further information on Datum can be found in the Reuse Manual.
 *
 *    Datum originated from :  U.S. Army Topographic Engineering Center (USATEC)
 *                             Geospatial Information Division (GID)
 *                             7701 Telegraph Road
 *                             Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    Datum has no restrictions.
 *
 * ENVIRONMENT
 *
 *    Datum was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC 2.8.1
 *    2. MS Windows 95 with MS Visual C++ 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    03/30/97          Original Code
 *    05/28/99          Added user-definable datums (for JMTK)
 *                      Added datum domain of validity checking (for JMTK)
 *                      Added datum shift accuracy calculation (for JMTK)
 *    06/27/06          Moved data files to data directory
 *    03-14-07          Original C++ Code
 *    03-21-08          Added check for west, east longitude values > 180
 *    06-11-10          S. Gillis, BAEts26724, Fixed memory error problem
 *                      when MSPCCS_DATA is not set
 *    07-07-10          K.Lam, BAEts27269, Replace C functions in threads.h
 *                      with C++ methods in classes CCSThreadMutex
 *    05/17/11          T. Thompson, BAEts27393, let user know when problem
 *                      is due to undefined MSPCCS_DATA
 *    07/13/12          K.Lam, BAEts29544, fixed problem with create datum
 *    07/17/12          S.Gillis,MSP_00029561,Fixed problem with deleting datum
 *    08/13/12          S. Gillis, MSP_00029654, Added lat/lon to define7ParamDatum
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "DatumLibraryImplementation.h"
#include "EllipsoidLibraryImplementation.h"
#include "EllipsoidParameters.h"
#include "SevenParameterDatum.h"
#include "ThreeParameterDatum.h"
#include "Geocentric.h"
#include "Datum.h"
#include "CartesianCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"
#include "Accuracy.h"
#include "CCSThreadMutex.h"
#include "CCSThreadLock.h"

/*
 *    math.h     - standard C mathematics library
 *    EllipsoidLibrary.h  - used to get ellipsoid parameters
 *    SevenParameterDatum.h  - creates a 7 parameter datum
 *    ThreeParameterDatum.h  - creates a 3 parameter datum
 *    Geocentric.h  - converts between geodetic and geocentric coordinates
 *    Datum.h       - used to store individual datum information
 *    DatumLibraryImplementation.h - for error ehecking and error codes
 *    CCSThreadMutex.h  - used for thread safety
 *    CCSThreadLock.h  - used for thread safety
 *    CartesianCoordinates.h   - defines cartesian coordinates
 *    GeodeticCoordinates.h   - defines geodetic coordinates
 *    CoordinateConversionException.h - Exception handler
 *    ErrorMessages.h  - Contains exception messages
 */


using namespace MSP::CCS;
using MSP::CCSThreadMutex;
using MSP::CCSThreadLock;

/***************************************************************************/
/*
 *                               DEFINES
 */

const double SECONDS_PER_RADIAN = 206264.8062471;   /* Seconds in a radian */
const double PI = 3.14159265358979323e0;
const double PI_OVER_2 = (PI / 2.0);
const double PI_OVER_180 = (PI / 180.0);
const double _180_OVER_PI = (180.0 / PI);
const double TWO_PI = (2.0 * PI);
const double MIN_LAT = (-PI/2.0);
const double MAX_LAT = (+PI/2.0);
const double MIN_LON = -PI;
const double MAX_LON = (2.0 * PI);
const int DATUM_CODE_LENGTH = 7;
const int DATUM_NAME_LENGTH = 33;
const int ELLIPSOID_CODE_LENGTH = 3;
const int MAX_WGS = 2;
const double MOLODENSKY_MAX = (89.75 * PI_OVER_180); /* Polar limit */
const int FILENAME_LENGTH = 128;
const char *WGS84_Datum_Code = "WGE";
const char *WGS72_Datum_Code = "WGC";


/************************************************************************/
/*                              LOCAL FUNCTIONS     
 *
 */

GeodeticCoordinates* molodenskyShift(
   const double a,
   const double da,
   const double f,
   const double df,
   const double dx,
   const double dy,
   const double dz,
   const double sourceLongitude,
   const double sourceLatitude,
   const double sourceHeight )
{ 
/*
 *  The function molodenskyShift shifts geodetic coordinates
 *  using the Molodensky method.
 *
 *    a               : Semi-major axis of source ellipsoid in meters  (input)
 *    da              : Destination a minus source a                   (input)
 *    f               : Flattening of source ellipsoid                 (input)
 *    df              : Destination f minus source f                   (input)
 *    dx              : X coordinate shift in meters                   (input)
 *    dy              : Y coordinate shift in meters                   (input)
 *    dz              : Z coordinate shift in meters                   (input)
 *    sourceLongitude : Longitude in radians                           (input)
 *    sourceLatitude  : Latitude in radians.                           (input)
 *    sourceHeight    : Height in meters.                              (input)
 *    targetLongitude : Calculated longitude in radians.               (output)
 *    targetLatitude  : Calculated latitude in radians.                (output)
 *    targetHeight    : Calculated height in meters.                   (output)
 */

  double tLon_in;   /* temp longitude                                     */
  double e2;        /* Intermediate calculations for dp, dl               */
  double ep2;       /* Intermediate calculations for dp, dl               */
  double sin_Lat;   /* sin(Latitude_1)                                    */
  double sin2_Lat;  /* (sin(Latitude_1))^2                                */
  double sin_Lon;   /* sin(Longitude_1)                                   */
  double cos_Lat;   /* cos(Latitude_1)                                    */
  double cos_Lon;   /* cos(Longitude_1)                                   */
  double w2;        /* Intermediate calculations for dp, dl               */
  double w;         /* Intermediate calculations for dp, dl               */
  double w3;        /* Intermediate calculations for dp, dl               */
  double m;         /* Intermediate calculations for dp, dl               */
  double n;         /* Intermediate calculations for dp, dl               */
  double dp;        /* Delta phi                                          */
  double dp1;       /* Delta phi calculations                             */
  double dp2;       /* Delta phi calculations                             */
  double dp3;       /* Delta phi calculations                             */
  double dl;        /* Delta lambda                                       */
  double dh;        /* Delta height                                       */
  double dh1;       /* Delta height calculations                          */
  double dh2;       /* Delta height calculations                          */

  if (sourceLongitude > PI)
    tLon_in = sourceLongitude - TWO_PI;
  else
    tLon_in = sourceLongitude;

  e2  = 2 * f - f * f;
  ep2 = e2 / (1 - e2);
  sin_Lat = sin(sourceLatitude);
  cos_Lat = cos(sourceLatitude);
  sin_Lon = sin(tLon_in);
  cos_Lon = cos(tLon_in);
  sin2_Lat = sin_Lat * sin_Lat;
  w2  = 1.0 - e2 * sin2_Lat;
  w   = sqrt(w2);
  w3  = w * w2;
  m   = (a * (1.0 - e2)) / w3;
  n   = a / w;
  dp1 = cos_Lat * dz - sin_Lat * cos_Lon * dx - sin_Lat * sin_Lon * dy;
  dp2 = ((e2 * sin_Lat * cos_Lat) / w) * da;
  dp3 = sin_Lat * cos_Lat * (2.0 * n + ep2 * m * sin2_Lat) * (1.0 - f) * df;
  dp  = (dp1 + dp2 + dp3) / (m + sourceHeight);
  dl  = (-sin_Lon * dx + cos_Lon * dy) / ((n + sourceHeight) * cos_Lat);
  dh1 = (cos_Lat * cos_Lon * dx) + (cos_Lat * sin_Lon * dy) + (sin_Lat * dz);
  dh2 = -(w * da) + ((a * (1 - f)) / w) * sin2_Lat * df;
  dh  = dh1 + dh2;

  double targetLatitude = sourceLatitude + dp;
  double targetLongitude = sourceLongitude + dl;
  double targetHeight = sourceHeight + dh;
  
  if (targetLongitude > TWO_PI)
    targetLongitude -= TWO_PI;
  if (targetLongitude < (- PI))
    targetLongitude += TWO_PI;

  return new GeodeticCoordinates(
     CoordinateType::geodetic, targetLongitude, targetLatitude, targetHeight );
} 


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

/* This class is a safeguard to make sure the singleton gets deleted
 * when the application exits
 */
namespace MSP
{
  namespace CCS
  {
     class DatumLibraryImplementationCleaner
     {
        public:

           ~DatumLibraryImplementationCleaner()
           {
              CCSThreadLock lock(&DatumLibraryImplementation::mutex);
              DatumLibraryImplementation::deleteInstance();
           }
           
     } datumLibraryImplementationCleanerInstance;
  }
}

// Make this class a singleton, so the data files are only initialized once
CCSThreadMutex DatumLibraryImplementation::mutex;
DatumLibraryImplementation* DatumLibraryImplementation::instance = 0;
int DatumLibraryImplementation::instanceCount = 0;


DatumLibraryImplementation* DatumLibraryImplementation::getInstance()
{
  CCSThreadLock lock(&mutex);
  if( instance == 0 )
    instance = new DatumLibraryImplementation;

  instanceCount++;

  return instance;
}


void DatumLibraryImplementation::removeInstance()
{
/*
 * The function removeInstance removes this DatumLibraryImplementation
 * instance from the total number of instances. 
 */
  CCSThreadLock lock(&mutex);
  if( --instanceCount < 1 )
  {
    deleteInstance();
  }
}


void DatumLibraryImplementation::deleteInstance()
{
/*
 * Delete the singleton.
 */

  if( instance != 0 )
  {
    delete instance;
    instance = 0;
  }
}


DatumLibraryImplementation::DatumLibraryImplementation():
  _ellipsoidLibraryImplementation( 0 ),
  datum3ParamCount( 0 ),
  datum7ParamCount( 0 )
{
   loadDatums();
}


DatumLibraryImplementation::DatumLibraryImplementation(
   const DatumLibraryImplementation &dl )
{
  int size = dl.datumList.size();
  for( int i = 0; i < size; i++ )
  {
    switch( dl.datumList[i]->datumType() )
    {
       case DatumType::threeParamDatum:
          datumList.push_back( new ThreeParameterDatum(
             *( ( ThreeParameterDatum* )( dl.datumList[i] ) ) ) );
          break;
       case DatumType::sevenParamDatum:
          datumList.push_back( new SevenParameterDatum(
             *( ( SevenParameterDatum* )( dl.datumList[i] ) ) ) );
          break;
       case DatumType::wgs84Datum:
       case DatumType::wgs72Datum:
          datumList.push_back( new Datum( *( dl.datumList[i] ) ) );
          break;
    }
  }

  _ellipsoidLibraryImplementation = dl._ellipsoidLibraryImplementation;
  datum3ParamCount = dl.datum3ParamCount;
  datum7ParamCount = dl.datum7ParamCount;
}


DatumLibraryImplementation::~DatumLibraryImplementation()
{
  std::vector<Datum*>::iterator iter = datumList.begin();
  while( iter != datumList.end() )
	{
		delete( *iter );
    iter++;
	}
  datumList.clear();

  _ellipsoidLibraryImplementation = 0;
}


DatumLibraryImplementation& DatumLibraryImplementation::operator=(
   const DatumLibraryImplementation &dl )
{
  if ( &dl == this )
     return *this;

  int size = dl.datumList.size();
  for( int i = 0; i < size; i++ )
  {
     switch( dl.datumList[i]->datumType() )
     {
        case DatumType::threeParamDatum:
           datumList.push_back( new ThreeParameterDatum(
              *( ( ThreeParameterDatum* )( dl.datumList[i] ) ) ) );
           break;
        case DatumType::sevenParamDatum:
           datumList.push_back( new SevenParameterDatum(
              *( ( SevenParameterDatum* )( dl.datumList[i] ) ) ) );
           break;
        case DatumType::wgs84Datum:
        case DatumType::wgs72Datum:
           datumList.push_back( new Datum( *( dl.datumList[i] ) ) );
           break;
     }
  }

  _ellipsoidLibraryImplementation = dl._ellipsoidLibraryImplementation;
  datum3ParamCount = dl.datum3ParamCount;
  datum7ParamCount = dl.datum7ParamCount;

  return *this;
}


void DatumLibraryImplementation::define3ParamDatum(
   const char *code,
   const char *name,
   const char *ellipsoidCode, 
   double deltaX,
   double deltaY,
   double deltaZ,
   double sigmaX,
   double sigmaY,
   double sigmaZ, 
   double westLongitude,
   double eastLongitude,
   double southLatitude,
   double northLatitude )
{ 
/*
 * The function define3ParamDatum creates a new local 3-parameter datum with the
 * specified code, name, and axes.  If the datum table has not been initialized,
 * the specified code is already in use, or a new version of the 3-param.dat
 * file cannot be created, an exception is thrown.
 * Note that the indexes of all datums in the datum table may be
 * changed by this function.
 *
 *   code          : 5-letter new datum code.                      (input)
 *   name          : Name of the new datum                         (input)
 *   ellipsoidCode : 2-letter code for the associated ellipsoid    (input)
 *   deltaX        : X translation to WGS84 in meters              (input)
 *   deltaY        : Y translation to WGS84 in meters              (input)
 *   deltaZ        : Z translation to WGS84 in meters              (input)
 *   sigmaX        : Standard error in X in meters                 (input)
 *   sigmaY        : Standard error in Y in meters                 (input)
 *   sigmaZ        : Standard error in Z in meters                 (input)
 *   westLongitude : Western edge of validity rectangle in radians (input)
 *   eastLongitude : Eastern edge of validity rectangle in radians (input)
 *   southLatitude : Southern edge of validity rectangle in radians(input)
 *   northLatitude : Northern edge of validity rectangle in radians(input)
 */

  char  datum_Code[DATUM_CODE_LENGTH];
  long  index = 0;
  long  ellipsoid_index = 0;
  long  code_length = 0;
  char *PathName = NULL;
  FILE *fp_3param = NULL;

  if (!(((sigmaX > 0.0) || (sigmaX == -1.0)) &&
        ((sigmaY > 0.0) || (sigmaY == -1.0)) &&
        ((sigmaZ > 0.0) || (sigmaZ == -1.0))))
    throw CoordinateConversionException( ErrorMessages::datumSigma );

  if ((southLatitude < MIN_LAT) || (southLatitude > MAX_LAT))
    throw CoordinateConversionException( ErrorMessages::latitude );
  if ((westLongitude < MIN_LON) || (westLongitude > MAX_LON))
    throw CoordinateConversionException( ErrorMessages::longitude );
  if ((northLatitude < MIN_LAT) || (northLatitude > MAX_LAT))
    throw CoordinateConversionException( ErrorMessages::latitude );
  if ((eastLongitude < MIN_LON) || (eastLongitude > MAX_LON))
    throw CoordinateConversionException( ErrorMessages::longitude );
  if (southLatitude >= northLatitude)
    throw CoordinateConversionException( ErrorMessages::datumDomain );
  if((westLongitude >= eastLongitude) &&
     (westLongitude >= 0 && westLongitude < 180) &&
     (eastLongitude >= 0 && eastLongitude < 180))
    throw CoordinateConversionException( ErrorMessages::datumDomain );

  // assume the datum code is new
  bool isNewDatumCode = true;
  try
  {
    // check if datum code exists
    datumIndex( code, &index );
    // get here if datum code is found in current datum table
    isNewDatumCode = false;
  }
  catch(CoordinateConversionException e)
  {
     // the datum code is new, keep going
  }

  // the datum code exists in current datum table, throw an error
  if ( !isNewDatumCode )
    throw CoordinateConversionException( ErrorMessages::invalidDatumCode );

  code_length = strlen( code );

  if( code_length > ( DATUM_CODE_LENGTH-1 ) )
    throw CoordinateConversionException( ErrorMessages::invalidDatumCode );

  if( _ellipsoidLibraryImplementation )
  {
    _ellipsoidLibraryImplementation->ellipsoidIndex(
       ellipsoidCode, &ellipsoid_index );
  }
  else
    throw CoordinateConversionException( ErrorMessages::ellipse );


  strcpy( datum_Code, code );
  /* Convert code to upper case */
  for( long i = 0; i < code_length; i++ )
    datum_Code[i] = ( char )toupper( datum_Code[i] );

  int numDatums = datumList.size();
  datumList.push_back( new ThreeParameterDatum(
     numDatums, ( char* )datum_Code, ( char* )ellipsoidCode,
     ( char* )name, DatumType::threeParamDatum, deltaX, deltaY, deltaZ,
     westLongitude, eastLongitude, southLatitude, northLatitude, sigmaX,
     sigmaY, sigmaZ, true ) );
  datum3ParamCount++;

  write3ParamFile();
} 


void DatumLibraryImplementation::define7ParamDatum(
   const char *code,
   const char *name,
   const char *ellipsoidCode, 
   double deltaX,
   double deltaY,
   double deltaZ,
   double rotationX,
   double rotationY,
   double rotationZ, 
   double scale, 
   double westLongitude,                            
   double eastLongitude, 
   double southLatitude, 
   double northLatitude )
{ 
/*
 * The function define7ParamDatum creates a new local 7-parameter datum with the
 * specified code, name, and axes.  If the datum table has not been initialized,
 * the specified code is already in use, or a new version of the 7-param.dat
 * file cannot be created, an exception is thrown.
 * Note that the indexes of all datums in the datum table may be
 * changed by this function.
 *
 *   code          : 5-letter new datum code.                      (input)
 *   name          : Name of the new datum                         (input)
 *   ellipsoidCode : 2-letter code for the associated ellipsoid    (input)
 *   deltaX        : X translation to WGS84 in meters              (input)
 *   deltaY        : Y translation to WGS84 in meters              (input)
 *   deltaZ        : Z translation to WGS84 in meters              (input)
 *   rotationX     : X rotation to WGS84 in arc seconds            (input)
 *   rotationY     : Y rotation to WGS84 in arc seconds            (input)
 *   rotationZ     : Z rotation to WGS84 in arc seconds            (input)
 *   scale         : Scale factor                                  (input)
 *   westLongitude : Western edge of validity rectangle in radians (input)
 *   eastLongitude : Eastern edge of validity rectangle in radians (input)
 *   southLatitude : Southern edge of validity rectangle in radians(input)
 *   northLatitude : Northern edge of validity rectangle in radians(input)
 */

  char datum_Code[DATUM_CODE_LENGTH];
  long index = 0;
  long ellipsoid_index = 0;
  long code_length = 0;
  char *PathName = NULL;
  FILE *fp_7param = NULL;

  if ((rotationX < -60.0) || (rotationX > 60.0))
    throw CoordinateConversionException( ErrorMessages::datumRotation );
  if ((rotationY < -60.0) || (rotationY > 60.0))
    throw CoordinateConversionException( ErrorMessages::datumRotation );
  if ((rotationZ < -60.0) || (rotationZ > 60.0))
    throw CoordinateConversionException( ErrorMessages::datumRotation );

  if ((scale < -0.001) || (scale > 0.001))
    throw CoordinateConversionException( ErrorMessages::scaleFactor );

  // assume the datum code is new
  bool isNewDatumCode = true;
  try
  {
    // check if datum code exists
    datumIndex( code, &index );
    // get here if datum code is found in current datum table
    isNewDatumCode = false;
  }
  catch(CoordinateConversionException e)
  {
     // the datum code is new, keep going
  }

  // the datum code exists in current datum table, throw an error
  if ( !isNewDatumCode )
    throw CoordinateConversionException( ErrorMessages::invalidDatumCode );

  code_length = strlen( code );
  if( code_length > ( DATUM_CODE_LENGTH-1 ) )
    throw CoordinateConversionException( ErrorMessages::invalidDatumCode );

  if( _ellipsoidLibraryImplementation )
  {
    _ellipsoidLibraryImplementation->ellipsoidIndex(
       ellipsoidCode, &ellipsoid_index );
  }
  else
    throw CoordinateConversionException( ErrorMessages::ellipse );

  long i;
  strcpy( datum_Code, code );
  /* Convert code to upper case */
  for( i = 0; i < code_length; i++ )
    datum_Code[i] = ( char )toupper( datum_Code[i] );

  datumList.insert( datumList.begin() + MAX_WGS + datum7ParamCount,
     new SevenParameterDatum( datum7ParamCount, ( char* )datum_Code,
        ( char* )ellipsoidCode, ( char* )name, DatumType::sevenParamDatum,
        deltaX, deltaY, deltaZ, 
        westLongitude, eastLongitude, southLatitude, northLatitude,
        rotationX / SECONDS_PER_RADIAN,
        rotationY / SECONDS_PER_RADIAN, rotationZ / SECONDS_PER_RADIAN,
        scale, true ) );
  datum7ParamCount++;

  write7ParamFile();
} 


void DatumLibraryImplementation::removeDatum( const char* code )
{ 
/*
 * The function removeDatum deletes a local (3-parameter) datum with the
 * specified code.  If the datum table has not been initialized or a new
 * version of the 3-param.dat file cannot be created, an exception is thrown,
 * Note that the indexes of all datums
 * in the datum table may be changed by this function.
 *
 *   code           : 5-letter datum code.                      (input)
 *
 */

  char *PathName = NULL;
  FILE *fp_3param = NULL;
  FILE *fp_7param = NULL;
  long index = 0;
  bool delete_3param_datum = true;

  datumIndex( code, &index );

  if( datumList[index]->datumType() == DatumType::threeParamDatum )
  {
    if( !( ( ThreeParameterDatum* )datumList[index] )->userDefined() )
      throw CoordinateConversionException( ErrorMessages::notUserDefined );
  }
  else if( datumList[index]->datumType() == DatumType::sevenParamDatum )
  {
    delete_3param_datum = false;
    if( !( ( SevenParameterDatum* )datumList[index] )->userDefined() )
      throw CoordinateConversionException( ErrorMessages::notUserDefined );
  }
  else
    throw CoordinateConversionException( ErrorMessages::notUserDefined );

  datumList.erase( datumList.begin() + index ); 

  if( !delete_3param_datum )
  {
    datum7ParamCount--;

    write7ParamFile();
  }
  else if( delete_3param_datum )
  {
    datum3ParamCount--;

    write3ParamFile();
  }
} 


void DatumLibraryImplementation::datumCount( long *count )
{ 
/*
 *  The function datumCount returns the number of Datums in the table
 *  if the table was initialized without error.
 *
 *  count        : number of datums in the datum table     (output)
 */

  *count = datumList.size();
} 


void DatumLibraryImplementation::datumIndex( const char *code, long *index )
{ 
/*
 *  The function datumIndex returns the index of the datum with the
 *  specified code.
 *
 *  code    : The datum code being searched for.                    (input)
 *  index   : The index of the datum in the table with the          (output)
 *              specified code.
 */

  char temp_code[DATUM_CODE_LENGTH];
  long length;
  long pos = 0;
  long i = 0;

  *index = 0;

  if( !code )
    throw CoordinateConversionException( ErrorMessages::invalidDatumCode );

  length = strlen( code );
  if ( length > ( DATUM_CODE_LENGTH-1 ) )
    throw CoordinateConversionException( ErrorMessages::invalidDatumCode );
  else
  {
    strcpy( temp_code, code );

    /* Convert to upper case */
    for( i=0; i < length; i++ )
      temp_code[i] = ( char )toupper( temp_code[i] );

    /* Strip blank spaces */
    while( pos < length )
    {
      if( isspace( temp_code[pos] ) )
      {
        for( i=pos; i <= length; i++ )
          temp_code[i] = temp_code[i+1];
        length -= 1;
      }
      else
        pos += 1;
    }

    int numDatums = datumList.size();
    /* Search for code */
    i = 0;
    while( i < numDatums && strcmp( temp_code, datumList[i]->code() ) )
    {
      i++;
    }
    if( i == numDatums || strcmp( temp_code, datumList[i]->code() ) )
      throw CoordinateConversionException( ErrorMessages::invalidDatumCode );
    else
      *index = i;
  }
} 


void DatumLibraryImplementation::datumCode( const long index, char *code )
{ 
/*
 *  The function datumCode returns the 5-letter code of the datum
 *  referenced by index.
 *
 *  index   : The index of a given datum in the datum table.        (input)
 *  code    : The datum Code of the datum referenced by Index.      (output)
 */

  if( index < 0 || index >= datumList.size() )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  else
    strcpy( code, datumList[index]->code() );
} 


void DatumLibraryImplementation::datumName( const long index, char *name )
{
/*
 *  The function datumName returns the name of the datum referenced by
 *  index.
 *
 *  index   : The index of a given datum in the datum table.        (input)
 *  name    : The datum Name of the datum referenced by Index.      (output)
 */

  if( index < 0 || index >= datumList.size() )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  else
    strcpy( name, datumList[index]->name() );
} 


void DatumLibraryImplementation::datumEllipsoidCode(
   const long index, char *code )
{ 
/*
 *  The function datumEllipsoidCode returns the 2-letter ellipsoid code
 *  for the ellipsoid associated with the datum referenced by index.
 *
 *  index   : The index of a given datum in the datum table.          (input)
 *  code    : The ellipsoid code for the ellipsoid associated with    (output)
 *               the datum referenced by index.
 */

  if( index < 0 || index >= datumList.size() )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  else
    strcpy( code, datumList[index]->ellipsoidCode() );
} 


void DatumLibraryImplementation::datumStandardErrors(
   const long index,
   double *sigmaX,
   double *sigmaY,
   double *sigmaZ )
{ 
/*
 *   The function datumStandardErrors returns the standard errors in X,Y, & Z
 *   for the datum referenced by index.
 *
 *    index      : The index of a given datum in the datum table   (input)
 *    sigma_X    : Standard error in X in meters                   (output)
 *    sigma_Y    : Standard error in Y in meters                   (output)
 *    sigma_Z    : Standard error in Z in meters                   (output)
 */

  if( index < 0 || index >= datumList.size() )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  else
  {
    Datum* datum = datumList[index];

    if( datum->datumType() == DatumType::threeParamDatum )
    {
      ThreeParameterDatum* threeParameterDatum = ( ThreeParameterDatum* )datum;
      *sigmaX = threeParameterDatum->sigmaX();
      *sigmaY = threeParameterDatum->sigmaY();
      *sigmaZ = threeParameterDatum->sigmaZ();
    }
    else
      throw CoordinateConversionException( ErrorMessages::invalidIndex );
  }
} 


void DatumLibraryImplementation::datumSevenParameters(
   const long index,
   double *rotationX,
   double *rotationY,
   double *rotationZ,
   double *scaleFactor )
{ 
/*
 *   The function datumSevenParameters returns parameter values, 
 *   used only by a seven parameter datum,
 *   for the datum referenced by index.
 *
 *    index       : The index of a given datum in the datum table. (input)
 *    rotationX   : X rotation in radians                          (output)
 *    rotationY   : Y rotation in radians                          (output)
 *    rotationZ   : Z rotation in radians                          (output)
 *    scaleFactor : Scale factor                                   (output)
 */

  if( index < 0 || index >= datumList.size() )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  else
  {
    Datum* datum = datumList[index];

    if( datum->datumType() == DatumType::sevenParamDatum )
    {
      SevenParameterDatum* sevenParameterDatum = ( SevenParameterDatum* )datum;

      *rotationX = sevenParameterDatum->rotationX();
      *rotationY = sevenParameterDatum->rotationY();
      *rotationZ = sevenParameterDatum->rotationZ();
      *scaleFactor = sevenParameterDatum->scaleFactor();
    }
    else
      throw CoordinateConversionException( ErrorMessages::invalidIndex );
  }
} 


void DatumLibraryImplementation::datumTranslationValues(
   const long index,
   double *deltaX,
   double *deltaY,
   double *deltaZ  )
{ 
/*
 *   The function datumTranslationValues returns the translation values
 *   for the datum referenced by index.
 *
 *    index       : The index of a given datum in the datum table. (input)
 *    deltaX      : X translation in meters                        (output)
 *    deltaY      : Y translation in meters                        (output)
 *    deltaZ      : Z translation in meters                        (output)
 */

  if( index < 0 || index >= datumList.size() )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  else
  {
    Datum* datum = datumList[index];

    *deltaX = datum->deltaX();
    *deltaY = datum->deltaY();
    *deltaZ = datum->deltaZ();
  }
} 


Accuracy* DatumLibraryImplementation::datumShiftError(
   const long      sourceIndex,
   const long      targetIndex, 
   double          longitude,
   double          latitude,
   Accuracy*       sourceAccuracy,
   Precision::Enum precision )
{
  double sinlat = sin( latitude );
  double coslat = cos( latitude );
  double sinlon = sin( longitude );
  double coslon = cos( longitude );
  double sigma_delta_lat;
  double sigma_delta_lon;
  double sigma_delta_height;
  double sx, sy, sz;
  double ce90_in = -1.0;
  double le90_in = -1.0;
  double se90_in = -1.0;
  double ce90_out = -1.0;
  double le90_out = -1.0;
  double se90_out = -1.0;
  double circularError90 = sourceAccuracy->circularError90();
  double linearError90 = sourceAccuracy->linearError90();
  double sphericalError90 = sourceAccuracy->sphericalError90();

  int numDatums = datumList.size();

  if( ( sourceIndex < 0 ) || ( sourceIndex >= numDatums ) )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  if( ( targetIndex < 0 ) || ( targetIndex >= numDatums ) )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  if( ( latitude < ( -90 * PI_OVER_180 ) ) ||
     ( latitude > ( 90 * PI_OVER_180 ) ) )
    throw CoordinateConversionException( ErrorMessages::latitude );
  if( ( longitude < ( -PI ) ) || ( longitude > TWO_PI ) )
    throw CoordinateConversionException( ErrorMessages::longitude );

  if( sourceIndex == targetIndex )
  { /* Just copy */
    circularError90 = circularError90;
    linearError90 = linearError90;
    sphericalError90 = sphericalError90;
  }
  else
  {
    Datum* sourceDatum = datumList[sourceIndex];
    Datum* targetDatum = datumList[targetIndex];

    /* calculate input datum errors */
    switch( sourceDatum->datumType() )
    {
    case DatumType::wgs84Datum:
    case DatumType::wgs72Datum:
    case DatumType::sevenParamDatum:
      {
        ce90_in = 0.0;
        le90_in = 0.0;
        se90_in = 0.0;
        break;
      }
    case DatumType::threeParamDatum:
      {
        ThreeParameterDatum* sourceThreeParameterDatum = 
           ( ThreeParameterDatum* )sourceDatum;

        if( ( sourceThreeParameterDatum->sigmaX() < 0 )
            || ( sourceThreeParameterDatum->sigmaY() < 0 )
            || ( sourceThreeParameterDatum->sigmaZ() < 0 ) )
        {
          ce90_in = -1.0;
          le90_in = -1.0;
          se90_in = -1.0;
        }
        else
        {
          sx = sourceThreeParameterDatum->sigmaX() * sinlat * coslon;
          sy = sourceThreeParameterDatum->sigmaY() * sinlat * sinlon;
          sz = sourceThreeParameterDatum->sigmaZ() * coslat;
          sigma_delta_lat = sqrt( ( sx * sx ) + ( sy * sy ) + ( sz * sz ) );

          sx = sourceThreeParameterDatum->sigmaX() * sinlon;
          sy = sourceThreeParameterDatum->sigmaY() * coslon;
          sigma_delta_lon = sqrt( ( sx * sx ) + ( sy * sy ) );

          sx = sourceThreeParameterDatum->sigmaX() * coslat * coslon;
          sy = sourceThreeParameterDatum->sigmaY() * coslat * sinlon;
          sz = sourceThreeParameterDatum->sigmaZ() * sinlat;
          sigma_delta_height = sqrt( ( sx * sx ) + ( sy * sy ) + ( sz * sz ) );

          ce90_in = 2.146 * ( sigma_delta_lat + sigma_delta_lon ) / 2.0;
          le90_in = 1.6449 * sigma_delta_height;
          se90_in = 2.5003 * 
             ( sourceThreeParameterDatum->sigmaX() +
                sourceThreeParameterDatum->sigmaY() +
                sourceThreeParameterDatum->sigmaZ() ) / 3.0;
        }
        break;
      }
    } 

    /* calculate output datum errors */
    switch( targetDatum->datumType() )
    {
    case DatumType::wgs84Datum:
    case DatumType::wgs72Datum:
    case DatumType::sevenParamDatum:
      {
        ce90_out = 0.0;
        le90_out = 0.0;
        se90_out = 0.0;
        break;
      }
    case DatumType::threeParamDatum:
      {
        ThreeParameterDatum* targetThreeParameterDatum =
           ( ThreeParameterDatum* )targetDatum;
        if ((targetThreeParameterDatum->sigmaX() < 0)
            ||(targetThreeParameterDatum->sigmaY() < 0)
            ||(targetThreeParameterDatum->sigmaZ() < 0))
        {
          ce90_out = -1.0;
          le90_out = -1.0;
          se90_out = -1.0;
        }
        else
        {
          sx = targetThreeParameterDatum->sigmaX() * sinlat * coslon;
          sy = targetThreeParameterDatum->sigmaY() * sinlat * sinlon;
          sz = targetThreeParameterDatum->sigmaZ() * coslat;
          sigma_delta_lat = sqrt((sx * sx) + (sy * sy) + (sz * sz));

          sx = targetThreeParameterDatum->sigmaX() * sinlon;
          sy = targetThreeParameterDatum->sigmaY() * coslon;
          sigma_delta_lon = sqrt( ( sx * sx ) + ( sy * sy ) );

          sx = targetThreeParameterDatum->sigmaX() * coslat * coslon;
          sy = targetThreeParameterDatum->sigmaY() * coslat * sinlon;
          sz = targetThreeParameterDatum->sigmaZ() * sinlat;
          sigma_delta_height = sqrt( ( sx * sx ) + ( sy * sy ) + ( sz * sz ) );

          ce90_out = 2.146 * ( sigma_delta_lat + sigma_delta_lon ) / 2.0;
          le90_out = 1.6449 * sigma_delta_height;
          se90_out = 2.5003 *
             ( targetThreeParameterDatum->sigmaX() +
                targetThreeParameterDatum->sigmaY() +
                targetThreeParameterDatum->sigmaZ() ) / 3.0;
        }
        break;
      }
    } 

    /* combine errors */
    if( ( circularError90 < 0.0 ) || ( ce90_in < 0.0 ) || ( ce90_out < 0.0 ) )
    {
      circularError90 = -1.0;
      linearError90 = -1.0;
      sphericalError90 = -1.0;
    }
    else
    {
      circularError90 = sqrt( ( circularError90 * circularError90 ) +
         ( ce90_in * ce90_in ) + ( ce90_out * ce90_out ) );
      if( circularError90 < 1.0 )
      {
        circularError90 = 1.0;
      }

      if( ( linearError90 < 0.0 ) || ( le90_in < 0.0 ) || ( le90_out < 0.0 ) )
      {
        linearError90 = -1.0;
        sphericalError90 = -1.0;
      }
      else
      {
        linearError90 = sqrt( ( linearError90 * linearError90 ) +
           ( le90_in * le90_in ) + ( le90_out * le90_out ) );
        if( linearError90 < 1.0 )
        {
          linearError90 = 1.0;
        }

        if( ( sphericalError90 < 0.0 )
           || ( se90_in < 0.0 )
           || ( se90_out < 0.0 ) )
          sphericalError90 = -1.0;
        else
        {
          sphericalError90 = sqrt( 
             ( sphericalError90 * sphericalError90 ) + 
             ( se90_in * se90_in ) + ( se90_out * se90_out ) );
          if( sphericalError90 < 1.0 )
          {
            sphericalError90 = 1.0;
          }
        }
      }
    }
  }

  // Correct for limited precision of input/output coordinate.

  // sigma for uniform distribution due to rounding.
  double sigma = Precision::toMeters( precision ) / sqrt( 12.0 );

  // For linear error
  if( linearError90 > 0.0 )
  {
     double lePrec = 1.6449 * sigma;
     linearError90 = sqrt(
        linearError90 * linearError90 + lePrec * lePrec);
  }

  // For circular error
  if( circularError90 > 0.0 )
  {
     double cePrec = 2.146 * sigma;
     circularError90 = sqrt(
        circularError90 * circularError90 + cePrec * cePrec);
  }

  // For spherical error
  if( sphericalError90 > 0.0 )
  {
     // Assume sigma in all directions
     double sePrec = 2.5003 * sigma;
     sphericalError90 = sqrt( 
        sphericalError90 * sphericalError90 + sePrec * sePrec );
  }

  return new Accuracy( circularError90, linearError90, sphericalError90 );
}


void DatumLibraryImplementation::datumUserDefined(
   const long index, long *result )
{
/*
 *  The function datumUserDefined checks whether or not the specified datum is
 *  user defined. It returns 1 if the datum is user defined, and returns
 *  0 otherwise. If index is valid DATUM_NO_ERROR is returned, otherwise
 *  DATUM_INVALID_INDEX_ERROR is returned.
 *
 *    index    : Index of a given datum in the datum table (input)
 *    result   : Indicates whether specified datum is user defined (1)
 *               or not (0)                                (output)
 */

  *result = false;

  if( index < 0 || index >= datumList.size() )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  else
  {
    Datum* datum = datumList[index];

    if( datum->datumType() == DatumType::threeParamDatum )
    {
      ThreeParameterDatum* threeParameterDatum = ( ThreeParameterDatum* )datum;

      if( threeParameterDatum->userDefined() )
        *result = true;
    }
    else if( datum->datumType() == DatumType::sevenParamDatum )
    {
      SevenParameterDatum* sevenParamDatum = ( SevenParameterDatum* )datum;

      if( sevenParamDatum->userDefined() )
        *result = true;
    }
    else
      throw CoordinateConversionException( ErrorMessages::invalidIndex );
  }
} 


bool DatumLibraryImplementation::datumUsesEllipsoid( const char *ellipsoidCode )
{ 
/*
 *  The function datumUsesEllipsoid returns 1 if the ellipsoid is in use by a
 *  user defined datum.  Otherwise, 0 is returned.
 *
 *  ellipsoidCode    : The ellipsoid code being searched for.    (input)
 */

  char temp_code[DATUM_CODE_LENGTH];
  long length;
  long pos = 0;
  long i = 0;
  bool ellipsoid_in_use = false;

  length = strlen( ellipsoidCode );
  if( length <= ( ELLIPSOID_CODE_LENGTH-1 ) )
  {
    strcpy( temp_code, ellipsoidCode );

    /* Convert to upper case */
    for( i=0;i<length;i++ )
      temp_code[i] = ( char )toupper( temp_code[i] );

    /* Strip blank spaces */
    while( pos < length )
    {
      if( isspace( temp_code[pos] ) )
      {
        for( i=pos; i<=length; i++ )
          temp_code[i] = temp_code[i+1];
        length -= 1;
      }
      else
        pos += 1;
    }

    /* Search for code */
    i = 0;
    int numDatums = datumList.size();
    while( ( i < numDatums ) && ( !ellipsoid_in_use ) )
    {
      if( strcmp( temp_code, datumList[i]->ellipsoidCode() ) == 0 )
        ellipsoid_in_use = true;
      i++;
    }
  }

  return ellipsoid_in_use;
} 


void DatumLibraryImplementation::datumValidRectangle(
   const long index,
   double *westLongitude,
   double *eastLongitude,
   double *southLatitude,
   double *northLatitude )
{ 
/*
 *   The function datumValidRectangle returns the edges of the validity
 *   rectangle for the datum referenced by index.
 *
 *   index          : The index of a given datum in the datum table   (input)
 *   westLongitude : Western edge of validity rectangle in radians   (output)
 *   eastLongitude : Eastern edge of validity rectangle in radians   (output)
 *   southLatitude : Southern edge of validity rectangle in radians  (output)
 *   northLatitude : Northern edge of validity rectangle in radians  (output)
 *
 */

  if( index < 0 && index >= datumList.size() )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  else
  {
    *westLongitude = datumList[index]->westLongitude();
    *eastLongitude = datumList[index]->eastLongitude();
    *southLatitude = datumList[index]->southLatitude();
    *northLatitude = datumList[index]->northLatitude();
  }
} 


CartesianCoordinates* DatumLibraryImplementation::geocentricDatumShift(
   const long   sourceIndex,
   const double sourceX,
   const double sourceY,
   const double sourceZ,
   const long   targetIndex )
{ 
/*
 *  The function geocentricDatumShift shifts a geocentric coordinate (X, Y, Z in meters) relative
 *  to the source datum to geocentric coordinate (X, Y, Z in meters) relative
 *  to the destination datum.
 *
 *  sourceIndex : Index of source datum                      (input)
 *  sourceX     : X coordinate relative to source datum      (input)
 *  sourceY     : Y coordinate relative to source datum      (input)
 *  sourceZ     : Z coordinate relative to source datum      (input)
 *  targetIndex : Index of destination datum                 (input)
 *  targetX     : X coordinate relative to destination datum (output)
 *  targetY     : Y coordinate relative to destination datum (output)
 *  targetZ     : Z coordinate relative to destination datum (output)
 *
 */

  int numDatums = datumList.size();

  if( ( sourceIndex < 0 ) || ( sourceIndex >= numDatums ) )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  if( ( targetIndex < 0 ) || ( targetIndex >= numDatums ) )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );

  if( sourceIndex == targetIndex )
  {
    return new CartesianCoordinates(
       CoordinateType::geocentric, sourceX, sourceY, sourceZ );
  }
  else
  {
    CartesianCoordinates* wgs84CartesianCoordinates = geocentricShiftToWGS84(
       sourceIndex, sourceX, sourceY, sourceZ );

    CartesianCoordinates* targetCartesianCoordinates = geocentricShiftFromWGS84(
       wgs84CartesianCoordinates->x(), wgs84CartesianCoordinates->y(),
       wgs84CartesianCoordinates->z(), targetIndex );

    delete wgs84CartesianCoordinates;

    return targetCartesianCoordinates;
  }
} 


CartesianCoordinates* DatumLibraryImplementation::geocentricShiftFromWGS84(
   const double WGS84X,
   const double WGS84Y,
   const double WGS84Z,
   const long targetIndex )
{ 
/*
 *  The function geocentricShiftFromWGS84 shifts a geocentric coordinate (X, Y, Z in meters) relative
 *  to WGS84 to a geocentric coordinate (X, Y, Z in meters) relative to the
 *  local datum referenced by index.
 *
 *  WGS84X        : X coordinate relative to WGS84                      (input)
 *  WGS84Y        : Y coordinate relative to WGS84                      (input)
 *  WGS84Z        : Z coordinate relative to WGS84                      (input)
 *  targetIndex   : Index of destination datum                          (input)
 *  targetX       : X coordinate relative to the destination datum      (output)
 *  targetY       : Y coordinate relative to the destination datum      (output)
 *  targetZ       : Z coordinate relative to the destination datum      (output)
 */

  int numDatums = datumList.size();

  if( ( targetIndex < 0 ) || ( targetIndex >= numDatums ) )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );

  Datum* localDatum = datumList[targetIndex];
  switch( localDatum->datumType() )
  {
    case DatumType::wgs72Datum:
    {
      CartesianCoordinates* wgs72CartesianCoordinates =
         geocentricShiftWGS84ToWGS72( WGS84X, WGS84Y, WGS84Z );

      return wgs72CartesianCoordinates;
    }
    case DatumType::wgs84Datum:
    {
      return new CartesianCoordinates(
         CoordinateType::geocentric, WGS84X, WGS84Y, WGS84Z );
    }
    case DatumType::sevenParamDatum:
    {
      SevenParameterDatum* sevenParameterDatum =
         ( SevenParameterDatum* )localDatum;

      double targetX = WGS84X - sevenParameterDatum->deltaX() - sevenParameterDatum->rotationZ() * WGS84Y
                 + sevenParameterDatum->rotationY() * WGS84Z - sevenParameterDatum->scaleFactor() * WGS84X;

      double targetY = WGS84Y - sevenParameterDatum->deltaY() + sevenParameterDatum->rotationZ() * WGS84X
                 - sevenParameterDatum->rotationX() * WGS84Z - sevenParameterDatum->scaleFactor() * WGS84Y;

      double targetZ = WGS84Z - sevenParameterDatum->deltaZ() - sevenParameterDatum->rotationY() * WGS84X
                 + sevenParameterDatum->rotationX() * WGS84Y - sevenParameterDatum->scaleFactor() * WGS84Z;

      return new CartesianCoordinates( CoordinateType::geocentric, targetX, targetY, targetZ );
    }
    case DatumType::threeParamDatum:
    {
      ThreeParameterDatum* threeParameterDatum = ( ThreeParameterDatum* )localDatum;

      double targetX = WGS84X - threeParameterDatum->deltaX();
      double targetY = WGS84Y - threeParameterDatum->deltaY();
      double targetZ = WGS84Z - threeParameterDatum->deltaZ();

      return new CartesianCoordinates( CoordinateType::geocentric, targetX, targetY, targetZ );
    }
    default:
      throw CoordinateConversionException( ErrorMessages::datumType );
  } 
} 


CartesianCoordinates* DatumLibraryImplementation::geocentricShiftToWGS84(
   const long sourceIndex,
   const double sourceX,
   const double sourceY,
   const double sourceZ )
{ 
/*
 *  The function geocentricShiftToWGS84 shifts a geocentric coordinate (X, Y, Z in meters) relative
 *  to the datum referenced by index to a geocentric coordinate (X, Y, Z in
 *  meters) relative to WGS84.
 *
 *  sourceIndex : Index of source datum                         (input)
 *  sourceX     : X coordinate relative to the source datum     (input)
 *  sourceY     : Y coordinate relative to the source datum     (input)
 *  sourceZ     : Z coordinate relative to the source datum     (input)
 *  WGS84X      : X coordinate relative to WGS84                (output)
 *  WGS84Y      : Y coordinate relative to WGS84                (output)
 *  WGS84Z      : Z coordinate relative to WGS84                (output)
 */

  int numDatums = datumList.size();

  if( ( sourceIndex < 0 ) || (sourceIndex > numDatums ) )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );

  Datum* localDatum = datumList[sourceIndex];
  switch( localDatum->datumType() )
  {
    case DatumType::wgs72Datum:
    {
      CartesianCoordinates* wgs84CartesianCoordinates = 
         geocentricShiftWGS72ToWGS84( sourceX, sourceY, sourceZ );

      return wgs84CartesianCoordinates;
    }
    case DatumType::wgs84Datum:
    {
      return new CartesianCoordinates( CoordinateType::geocentric, sourceX, sourceY, sourceZ );
    }
    case DatumType::sevenParamDatum:
    {
      SevenParameterDatum* sevenParameterDatum = ( SevenParameterDatum* )localDatum;

      double wgs84X = sourceX + sevenParameterDatum->deltaX() + sevenParameterDatum->rotationZ() * sourceY
                 - sevenParameterDatum->rotationY() * sourceZ + sevenParameterDatum->scaleFactor() * sourceX;

      double wgs84Y = sourceY + sevenParameterDatum->deltaY() - sevenParameterDatum->rotationZ() * sourceX
                 + sevenParameterDatum->rotationX() * sourceZ + sevenParameterDatum->scaleFactor() * sourceY;

      double wgs84Z = sourceZ + sevenParameterDatum->deltaZ() + sevenParameterDatum->rotationY() * sourceX
                 - sevenParameterDatum->rotationX() * sourceY + sevenParameterDatum->scaleFactor() * sourceZ;

      return new CartesianCoordinates( CoordinateType::geocentric, wgs84X, wgs84Y, wgs84Z );
    }
    case DatumType::threeParamDatum:
    {
      ThreeParameterDatum* threeParameterDatum = ( ThreeParameterDatum* )localDatum;

      double wgs84X = sourceX + threeParameterDatum->deltaX();
      double wgs84Y = sourceY + threeParameterDatum->deltaY();
      double wgs84Z = sourceZ + threeParameterDatum->deltaZ();

      return new CartesianCoordinates( CoordinateType::geocentric, wgs84X, wgs84Y, wgs84Z );
    }
    default:
      throw CoordinateConversionException( ErrorMessages::datumType );
  } 
} 


GeodeticCoordinates* DatumLibraryImplementation::geodeticDatumShift(
   const long sourceIndex,
   const GeodeticCoordinates* sourceCoordinates,
   const long targetIndex )
{ 
/*
 *  The function geodeticDatumShift shifts geodetic coordinates (latitude, longitude in radians
 *  and height in meters) relative to the source datum to geodetic coordinates
 *  (latitude, longitude in radians and height in meters) relative to the
 *  destination datum.
 *
 *  sourceIndex     : Index of source datum                            (input)
 *  sourceLongitude : Longitude in radians relative to source datum    (input)
 *  sourceLatitude  : Latitude in radians relative to source datum     (input)
 *  sourceHeight    : Height in meters relative to source datum        (input)
 *  targetIndex     : Index of destination datum                       (input)
 *  targetLongitude : Longitude in radians relative to destination datum(output)
 *  targetLatitude  : Latitude in radians relative to destination datum (output)
 *  targetHeight    : Height in meters relative to destination datum    (output)
 */

  long E_Index;
  double a;
  double f;

  double sourceLongitude = sourceCoordinates->longitude();
  double sourceLatitude = sourceCoordinates->latitude();
  double sourceHeight = sourceCoordinates->height();

  int numDatums = datumList.size();

  if( sourceIndex < 0 || sourceIndex >= numDatums )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  if( targetIndex < 0 || targetIndex >= numDatums )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );

  if(( sourceLatitude < ( -90 * PI_OVER_180 ) ) ||
     ( sourceLatitude > (  90 * PI_OVER_180 ) ) )
    throw CoordinateConversionException( ErrorMessages::latitude );
  if( ( sourceLongitude < ( -PI )) || ( sourceLongitude > TWO_PI ) )
    throw CoordinateConversionException( ErrorMessages::longitude );

  Datum* sourceDatum = datumList[sourceIndex];
  Datum* targetDatum = datumList[targetIndex];

  if ( sourceIndex == targetIndex )
  { /* Just copy */
    return new GeodeticCoordinates(
       CoordinateType::geodetic, sourceLatitude, sourceLongitude, sourceHeight);
  }
  else
  {
    if( _ellipsoidLibraryImplementation )
    {
      if( sourceDatum->datumType() == DatumType::sevenParamDatum )
      {
        _ellipsoidLibraryImplementation->ellipsoidIndex(
           sourceDatum->ellipsoidCode(), &E_Index );
        _ellipsoidLibraryImplementation->ellipsoidParameters( E_Index, &a, &f );

        Geocentric geocentricFromGeodetic( a, f );

        CartesianCoordinates* sourceCartesianCoordinates = 
           geocentricFromGeodetic.convertFromGeodetic( sourceCoordinates );

        if( targetDatum->datumType() == DatumType::sevenParamDatum )
        { /* Use 3-step method for both stages */
           CartesianCoordinates* targetCartesianCoordinates = geocentricDatumShift(
              sourceIndex, 
              sourceCartesianCoordinates->x(),
              sourceCartesianCoordinates->y(),
              sourceCartesianCoordinates->z(), targetIndex );

            _ellipsoidLibraryImplementation->ellipsoidIndex(
               targetDatum->ellipsoidCode(), &E_Index );
            _ellipsoidLibraryImplementation->ellipsoidParameters( E_Index, &a, &f );

            Geocentric geocentricToGeodetic( a, f );
            GeodeticCoordinates* targetGeodeticCoordinates =
               geocentricToGeodetic.convertToGeodetic( targetCartesianCoordinates );

            delete targetCartesianCoordinates;
            delete sourceCartesianCoordinates;

            return targetGeodeticCoordinates;
        }
        else
        { /* Use 3-step method for 1st stage, Molodensky if possible for 2nd stage */
           CartesianCoordinates* wgs84CartesianCoordinates = geocentricShiftToWGS84(
              sourceIndex, sourceCartesianCoordinates->x(),
              sourceCartesianCoordinates->y(), sourceCartesianCoordinates->z() );

           long wgs84EllipsoidIndex;
           _ellipsoidLibraryImplementation->ellipsoidIndex( "WE", &wgs84EllipsoidIndex );
           _ellipsoidLibraryImplementation->ellipsoidParameters(
              wgs84EllipsoidIndex, &a, &f );

           Geocentric geocentricToGeodetic( a, f );
           GeodeticCoordinates* wgs84GeodeticCoordinates =
              geocentricToGeodetic.convertToGeodetic( wgs84CartesianCoordinates );

           GeodeticCoordinates* targetGeodeticCoordinates =
              geodeticShiftFromWGS84( wgs84GeodeticCoordinates, targetIndex );

           delete wgs84GeodeticCoordinates;
           delete wgs84CartesianCoordinates;

           return targetGeodeticCoordinates;
        }
      }
      else if( targetDatum->datumType() == DatumType::sevenParamDatum )
      { /* Use Molodensky if possible for 1st stage, 3-step method for 2nd stage */
         GeodeticCoordinates* wgs84GeodeticCoordinates = geodeticShiftToWGS84(
            sourceIndex, sourceCoordinates );

        long wgs84EllipsoidIndex;
        _ellipsoidLibraryImplementation->ellipsoidIndex( "WE", &wgs84EllipsoidIndex );
        _ellipsoidLibraryImplementation->ellipsoidParameters(
           wgs84EllipsoidIndex, &a, &f );

        Geocentric geocentricFromGeodetic( a, f );
        CartesianCoordinates* wgs84CartesianCoordinates = 
           geocentricFromGeodetic.convertFromGeodetic( wgs84GeodeticCoordinates );

        CartesianCoordinates* targetCartesianCoordinates =
           geocentricShiftFromWGS84(
              wgs84CartesianCoordinates->x(),
              wgs84CartesianCoordinates->y(),
              wgs84CartesianCoordinates->z(), targetIndex );

        _ellipsoidLibraryImplementation->ellipsoidIndex(
           targetDatum->ellipsoidCode(), &E_Index );
        _ellipsoidLibraryImplementation->ellipsoidParameters( E_Index, &a, &f );

        Geocentric geocentricToGeodetic( a, f );
        GeodeticCoordinates* targetGeodeticCoordinates =
           geocentricToGeodetic.convertToGeodetic( targetCartesianCoordinates );

        delete targetCartesianCoordinates;
        delete wgs84CartesianCoordinates;
        delete wgs84GeodeticCoordinates;

        return targetGeodeticCoordinates;
      }
      else
      { /* Use Molodensky if possible for both stages */
        GeodeticCoordinates* wgs84GeodeticCoordinates = geodeticShiftToWGS84(
           sourceIndex, sourceCoordinates );

        GeodeticCoordinates* targetGeodeticCoordinates = geodeticShiftFromWGS84(
           wgs84GeodeticCoordinates, targetIndex );

        delete wgs84GeodeticCoordinates;

        return targetGeodeticCoordinates;
      }
    }
    else
      throw CoordinateConversionException( ErrorMessages::ellipse );
  }
} 


GeodeticCoordinates* DatumLibraryImplementation::geodeticShiftFromWGS84(
   const GeodeticCoordinates* sourceCoordinates, const long targetIndex )
{ 
/*
 *  The function geodeticShiftFromWGS84 shifts geodetic coordinates relative to WGS84
 *  to geodetic coordinates relative to a given local datum.
 *
 *    WGS84Longitude  : Longitude in radians relative to WGS84           (input)
 *    WGS84Latitude   : Latitude in radians relative to WGS84            (input)
 *    WGS84Height     : Height in meters  relative to WGS84              (input)
 *    targetIndex     : Index of destination datum                       (input)
 *    targetLongitude : Longitude (rad) relative to destination datum   (output)
 *    targetLatitude  : Latitude (rad) relative to destination datum    (output)
 *    targetHeight    : Height in meters relative to destination datum  (output)
 *
 */

  double WGS84_a;   /* Semi-major axis of WGS84 ellipsoid in meters */
  double WGS84_f;   /* Flattening of WGS84 ellisoid                 */
  double a;         /* Semi-major axis of ellipsoid in meters       */
  double da;        /* Difference in semi-major axes                */
  double f;         /* Flattening of ellipsoid                      */
  double df;        /* Difference in flattening                     */
  double dx;
  double dy;
  double dz;
  long E_Index;

  double WGS84Longitude = sourceCoordinates->longitude();
  double WGS84Latitude = sourceCoordinates->latitude();
  double WGS84Height = sourceCoordinates->height();

  if( ( targetIndex < 0) || (targetIndex >= datumList.size() ) )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  if(( WGS84Latitude < ( -90 * PI_OVER_180 ) ) ||
     ( WGS84Latitude > (  90 * PI_OVER_180 ) ) )
    throw CoordinateConversionException( ErrorMessages::latitude );
  if( ( WGS84Longitude < ( -PI ) ) || ( WGS84Longitude > TWO_PI ) )
    throw CoordinateConversionException( ErrorMessages::longitude );

  Datum* localDatum = datumList[targetIndex];
  switch( localDatum->datumType() )
  {
    case DatumType::wgs72Datum:
    {
      GeodeticCoordinates* targetGeodeticCoordinates = geodeticShiftWGS84ToWGS72( WGS84Longitude, WGS84Latitude, WGS84Height );
      return targetGeodeticCoordinates;
    }
    case DatumType::wgs84Datum:
    {
      return new GeodeticCoordinates( CoordinateType::geodetic, WGS84Longitude, WGS84Latitude, WGS84Height );
    }
    case DatumType::sevenParamDatum:
    case DatumType::threeParamDatum:
    {
      if( _ellipsoidLibraryImplementation )
      {
        _ellipsoidLibraryImplementation->ellipsoidIndex( localDatum->ellipsoidCode(), &E_Index );
       _ellipsoidLibraryImplementation->ellipsoidParameters( E_Index, &a, &f );

        long wgs84EllipsoidIndex;
        _ellipsoidLibraryImplementation->ellipsoidIndex( "WE", &wgs84EllipsoidIndex );
        _ellipsoidLibraryImplementation->ellipsoidParameters( wgs84EllipsoidIndex, &WGS84_a, &WGS84_f );

        if( ( localDatum->datumType() == DatumType::sevenParamDatum ) ||
            ( WGS84Latitude < ( -MOLODENSKY_MAX ) ) ||
            ( WGS84Latitude > MOLODENSKY_MAX ) )
        { /* Use 3-step method */
          Geocentric geocentricFromGeodetic( WGS84_a, WGS84_f );
          CartesianCoordinates* wgs84CartesianCoordinates = geocentricFromGeodetic.convertFromGeodetic( sourceCoordinates );

          CartesianCoordinates* localCartesianCoordinates = geocentricShiftFromWGS84( wgs84CartesianCoordinates->x(), wgs84CartesianCoordinates->y(), 
                wgs84CartesianCoordinates->z(), targetIndex );

          Geocentric geocentricToGeodetic( a, f );
          GeodeticCoordinates* targetGeodeticCoordinates = geocentricToGeodetic.convertToGeodetic( localCartesianCoordinates );

          delete localCartesianCoordinates;
          delete wgs84CartesianCoordinates;

          return targetGeodeticCoordinates;
        }
        else
        { /* Use Molodensky's method */
          da = a - WGS84_a;
          df = f - WGS84_f;
          dx = -( localDatum->deltaX() );
          dy = -( localDatum->deltaY() );
          dz = -( localDatum->deltaZ() );

          GeodeticCoordinates* targetGeodeticCoordinates = molodenskyShift( WGS84_a, da, WGS84_f, df, dx, dy, dz,
                           WGS84Longitude, WGS84Latitude, WGS84Height );

          return targetGeodeticCoordinates;
        }
      }
    }
    default:
      throw CoordinateConversionException( ErrorMessages::datumType );
  } 
} 


GeodeticCoordinates* DatumLibraryImplementation::geodeticShiftToWGS84( const long sourceIndex,  const GeodeticCoordinates* sourceCoordinates )
{ 
/*
 *  The function geodeticShiftToWGS84 shifts geodetic coordinates relative to a given source datum
 *  to geodetic coordinates relative to WGS84.
 *
 *    sourceIndex     : Index of source datum                         (input)
 *    sourceLongitude : Longitude in radians relative to source datum (input)
 *    sourceLatitude  : Latitude in radians relative to source datum  (input)
 *    sourceHeight    : Height in meters relative to source datum     (input)
 *    WGS84Longitude  : Longitude in radians relative to WGS84        (output)
 *    WGS84Latitude   : Latitude in radians relative to WGS84         (output)
 *    WGS84Height     : Height in meters relative to WGS84            (output)
 *
 */

  double WGS84_a;   /* Semi-major axis of WGS84 ellipsoid in meters */
  double WGS84_f;   /* Flattening of WGS84 ellisoid                 */
  double a;         /* Semi-major axis of ellipsoid in meters       */
  double da;        /* Difference in semi-major axes                */
  double f;         /* Flattening of ellipsoid                      */
  double df;        /* Difference in flattening                     */
  double dx;
  double dy;
  double dz;
  long E_Index;

  double sourceLongitude = sourceCoordinates->longitude();
  double sourceLatitude = sourceCoordinates->latitude(); 
  double sourceHeight = sourceCoordinates->height();

  if( ( sourceIndex < 0 ) || ( sourceIndex >= datumList.size() ) )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  if(( sourceLatitude < ( -90 * PI_OVER_180 ) ) ||
     ( sourceLatitude > (  90 * PI_OVER_180 ) ) )
    throw CoordinateConversionException( ErrorMessages::latitude );
  if( ( sourceLongitude < ( -PI ) ) || ( sourceLongitude > TWO_PI ) )
    throw CoordinateConversionException( ErrorMessages::longitude );

  Datum* localDatum = datumList[sourceIndex];
  switch( localDatum->datumType() )
  {
    case DatumType::wgs72Datum:
    { /* Special case for WGS72 */
      return geodeticShiftWGS72ToWGS84( sourceLongitude, sourceLatitude, sourceHeight );
    }
    case DatumType::wgs84Datum:
    {        /* Just  copy */
      return new GeodeticCoordinates(CoordinateType::geodetic, sourceLongitude, sourceLatitude, sourceHeight);
    }
    case DatumType::sevenParamDatum:
    case DatumType::threeParamDatum:
    {
      if( _ellipsoidLibraryImplementation )
      {
        _ellipsoidLibraryImplementation->ellipsoidIndex( localDatum->ellipsoidCode(), &E_Index );
        _ellipsoidLibraryImplementation->ellipsoidParameters( E_Index, &a, &f );

        if( ( localDatum->datumType() == DatumType::sevenParamDatum ) ||
           ( sourceLatitude < (-MOLODENSKY_MAX ) ) ||
           ( sourceLatitude > MOLODENSKY_MAX ) )
        { /* Use 3-step method */
            Geocentric geocentricFromGeodetic( a, f );
            CartesianCoordinates* localCartesianCoordinates =
               geocentricFromGeodetic.convertFromGeodetic( sourceCoordinates );

            CartesianCoordinates* wgs84CartesianCoordinates = geocentricShiftToWGS84( sourceIndex, localCartesianCoordinates->x(), localCartesianCoordinates->y(), localCartesianCoordinates->z() );

            long wgs84EllipsoidIndex;
            _ellipsoidLibraryImplementation->ellipsoidIndex( "WE", &wgs84EllipsoidIndex );
            _ellipsoidLibraryImplementation->ellipsoidParameters( wgs84EllipsoidIndex, &WGS84_a, &WGS84_f );

            Geocentric geocentricToGeodetic( WGS84_a, WGS84_f );
            GeodeticCoordinates* wgs84GeodeticCoordinates = geocentricToGeodetic.convertToGeodetic( wgs84CartesianCoordinates );

            delete wgs84CartesianCoordinates;
            delete localCartesianCoordinates;

            return wgs84GeodeticCoordinates;
          }
          else
          { /* Use Molodensky's method */
            long wgs84EllipsoidIndex;
            _ellipsoidLibraryImplementation->ellipsoidIndex( "WE", &wgs84EllipsoidIndex );
            _ellipsoidLibraryImplementation->ellipsoidParameters( wgs84EllipsoidIndex, &WGS84_a, &WGS84_f );

            da = WGS84_a - a;
            df = WGS84_f - f;
            dx = localDatum->deltaX();
            dy = localDatum->deltaY();
            dz = localDatum->deltaZ();

            GeodeticCoordinates* wgs84GeodeticCoordinates = molodenskyShift( a, da, f, df, dx, dy, dz, sourceLongitude, sourceLatitude, sourceHeight );

            return wgs84GeodeticCoordinates;
          }
        }
    }
    default:
      throw CoordinateConversionException( ErrorMessages::datumType );
  } 
} 


void DatumLibraryImplementation::retrieveDatumType(
   const long index,
   DatumType::Enum *datumType )
{ 
/*
 *  The function retrieveDatumType returns the type of the datum referenced by
 *  index.
 *
 *  index     : The index of a given datum in the datum table.   (input)
 *  datumType : The type of datum referenced by index.           (output)
 *
 */

  if( index < 0 || index >= datumList.size() )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  else
    *datumType = datumList[index]->datumType();
} 


void DatumLibraryImplementation::validDatum(
   const long index,
   double longitude,
   double latitude,
   long  *result )
{ 
/*
 *  The function validDatum checks whether or not the specified location is within the
 *  validity rectangle for the specified datum.  It returns zero if the specified
 *  location is NOT within the validity rectangle, and returns 1 otherwise.
 *
 *   index     : The index of a given datum in the datum table      (input)
 *   latitude  : Latitude of the location to be checked in radians  (input)
 *   longitude : Longitude of the location to be checked in radians (input)
 *   result    : Indicates whether location is inside (1) or outside (0)
 *               of the validity rectangle of the specified datum   (output)
 */
  *result = 0;

  if( ( index < 0 ) || ( index >= datumList.size() ) )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  if( ( latitude < MIN_LAT ) || ( latitude > MAX_LAT ) )
    throw CoordinateConversionException( ErrorMessages::latitude );
  if( ( longitude < MIN_LON ) || ( longitude > MAX_LON ) )
    throw CoordinateConversionException( ErrorMessages::longitude );
  
  Datum* datum = datumList[index];

  double west_longitude = datum->westLongitude();
  double east_longitude = datum->eastLongitude();

  /* The west and east longitudes may be in the range 0 to 360
     or -180 to 180, longitude is always in the -180 to 180 range
     Figure out which range west and east longitudes are in.
     If west and east are in the -180 to 180 range and west > east, put them in the 0 to 360 range and adjust longitude if
     necessary.     
     If west and east are in the 0 to 360 range and west > east, put them in the -180 to 180 range. If west < east, adjust 
     longitude to the 0 to 360 range
  */
  if( ( west_longitude < 0 ) || ( east_longitude < 0 ) )
  {
    if( west_longitude > east_longitude )
    {
      if( west_longitude < 0 )
        west_longitude += TWO_PI;
      if( east_longitude < 0 )
        east_longitude += TWO_PI;
      if( longitude < 0 )
        longitude += TWO_PI;
    }
  }
  else if( ( west_longitude > PI ) || ( east_longitude > PI ) )
  {
    if( west_longitude > east_longitude )
    {
      if( west_longitude > PI )
        west_longitude -= TWO_PI;
      if( east_longitude > PI )
        east_longitude -= TWO_PI;
    }
    else 
    {
      if( longitude < 0 )
        longitude += TWO_PI;
    }
  }

  if( ( datum->southLatitude() <= latitude ) &&
      ( latitude <= datum->northLatitude() ) &&
      ( west_longitude <= longitude ) &&
      ( longitude <= east_longitude ) )
  {
    *result = 1;
  }
  else
  {
    *result = 0;
  }
} 


void DatumLibraryImplementation::setEllipsoidLibraryImplementation(
   EllipsoidLibraryImplementation* __ellipsoidLibraryImplementation )
{
/*
 *  The function setEllipsoidLibraryImplementation sets the ellipsoid library information
 *  which is needed to create datums and calculate datum shifts.
 *
 *   __ellipsoidLibraryImplementation  : Ellipsoid library implementation(input)
 *
 */

  _ellipsoidLibraryImplementation = __ellipsoidLibraryImplementation;
}


/************************************************************************/
/*                              PRIVATE FUNCTIONS     
 *
 */

void DatumLibraryImplementation::loadDatums()
{ 
/*
 * The function loadDatums creates the datum table from two external
 * files.  If an error occurs, the initialization stops and an error code is
 * returned.  This function must be called before any of the other functions
 * in this component.
 */

  long index = 0, i = 0;
  char *PathName = NULL;
  char* FileName7 = 0;
  FILE *fp_7param = NULL;
  FILE *fp_3param = NULL;
  char* FileName3 = 0;
  const int file_name_length = 23;

  CCSThreadLock lock(&mutex);

  /*  Check the environment for a user provided path, else current directory; */
  /*  Build a File Name, including specified or default path:                */

#ifdef NDK_BUILD
  PathName = "/data/data/com.baesystems.msp.geotrans/lib/";
  FileName7 = new char[ 80 ];
  strcpy( FileName7, PathName );
  strcat( FileName7, "lib7paramdat.so" );
#else
  PathName = getenv( "MSPCCS_DATA" );
  if (PathName != NULL)
  {
    FileName7 = new char[ strlen( PathName ) + 13 ];
    strcpy( FileName7, PathName );
    strcat( FileName7, "/" );
  }
  else
  {
    FileName7 = new char[ file_name_length ];
    strcpy( FileName7, "../../data/" );
  }
  strcat( FileName7, "7_param.dat" );
#endif

  /*  Open the File READONLY, or Return Error Condition:                    */

  if (( fp_7param = fopen( FileName7, "r" ) ) == NULL)
  {
    delete [] FileName7;
    FileName7 = 0;

    char message[256] = "";
    if (NULL == PathName)
    {
      strcpy( message, "Environment variable undefined: MSPCCS_DATA." );
    }
    else
    {
      strcpy( message, ErrorMessages::datumFileOpenError );
      strcat( message, ": 7_param.dat\n" );
    }
    throw CoordinateConversionException( message );
  }

  /*  Open the File READONLY, or Return Error Condition:                    */

    /* WGS84 datum entry */
  datumList.push_back( new Datum(
     index, "WGE", "WE", "World Geodetic System 1984", DatumType::wgs84Datum,
     0.0, 0.0, 0.0, -PI, +PI, -PI / 2.0, +PI / 2.0, false ) );
  index ++;


    /* WGS72 datum entry */
  datumList.push_back( new Datum(
     index, "WGC", "WD", "World Geodetic System 1972", DatumType::wgs72Datum,
     0.0, 0.0, 0.0, -PI, +PI, -PI / 2.0, +PI / 2.0, false ) );

  index ++;

    datum7ParamCount = 0;
    /* build 7-parameter datum table entries */
    while ( !feof(fp_7param ) )
    {
        char code[DATUM_CODE_LENGTH];

        bool userDefined = false;

        if (fscanf(fp_7param, "%s ", code) <= 0)
        {
           fclose( fp_7param );
          throw CoordinateConversionException( ErrorMessages::datumFileParseError );
        }
        else
        {
          if( code[0] == '*' )
          {
            userDefined = true;
            for( int i = 0; i < DATUM_CODE_LENGTH; i++ )
              code[i] = code[i+1];
          }
        }

        char name[DATUM_NAME_LENGTH];
        if (fscanf(fp_7param, "\"%32[^\"]\"", name) <= 0)
          name[0] = '\0';

        char ellipsoidCode[ELLIPSOID_CODE_LENGTH];
        double deltaX;
        double deltaY;
        double deltaZ;
        double rotationX;
        double rotationY;
        double rotationZ;
        double scaleFactor;

        if( fscanf( fp_7param, " %s %lf %lf %lf %lf %lf %lf %lf ",
           ellipsoidCode,  &deltaX, &deltaY, &deltaZ,
           &rotationX, &rotationY, &rotationZ,  &scaleFactor ) <= 0 )
        {
           fclose( fp_7param );
           throw CoordinateConversionException( ErrorMessages::datumFileParseError );
        }
        else
        { /* convert from degrees to radians */

          rotationX /= SECONDS_PER_RADIAN;
          rotationY /= SECONDS_PER_RADIAN;
          rotationZ /= SECONDS_PER_RADIAN;
          
          datumList.push_back( new SevenParameterDatum(
             index, code, ellipsoidCode, name, DatumType::sevenParamDatum,
             deltaX, deltaY, deltaZ, -PI, +PI, -PI / 2.0, +PI / 2.0,
             rotationX, rotationY, rotationZ, scaleFactor, userDefined ) );
        }
        index++;
        datum7ParamCount++;
    }
    fclose( fp_7param );

#ifdef NDK_BUILD
  PathName = "/data/data/com.baesystems.msp.geotrans/lib/";
  FileName3 = new char[ 80 ];
  strcpy( FileName3, PathName );
  strcat( FileName3, "lib3paramdat.so" );
#else
    if (PathName != NULL)
    {
       FileName3 = new char[ strlen( PathName ) + 13 ];
       strcpy( FileName3, PathName );
       strcat( FileName3, "/" );
    }
    else
    {
       FileName3 = new char[ file_name_length ];
       strcpy( FileName3, "../../data/" );
    }
    strcat( FileName3, "3_param.dat" );
#endif

    if (( fp_3param = fopen( FileName3, "r" ) ) == NULL)
    {
       delete [] FileName7;
       FileName7 = 0;
       delete [] FileName3;
       FileName3 = 0;
       
       char message[256] = "";
       strcpy( message, ErrorMessages::datumFileOpenError );
       strcat( message, ": 3_param.dat\n" );
       throw CoordinateConversionException( message );
    }

    datum3ParamCount = 0;

    /* build 3-parameter datum table entries */
    while( !feof( fp_3param ) )
    { 
        char code[DATUM_CODE_LENGTH];

        bool userDefined = false;

        if( fscanf( fp_3param, "%s ", code ) <= 0 )
        {
           fclose( fp_3param );
           throw CoordinateConversionException( ErrorMessages::datumFileParseError );
        }
        else
        {
          if( code[0] == '*' )
          {
            userDefined = true;
            for( int i = 0; i < DATUM_CODE_LENGTH; i++ )
              code[i] = code[i+1];
          }
        }

        char name[DATUM_NAME_LENGTH];
        if( fscanf( fp_3param, "\"%32[^\"]\"", name) <= 0 )
          name[0] = '\0';

        char ellipsoidCode[ELLIPSOID_CODE_LENGTH];
        double deltaX;
        double deltaY;
        double deltaZ;
        double sigmaX;
        double sigmaY;
        double sigmaZ;
        double eastLongitude;
        double westLongitude;
        double northLatitude;
        double southLatitude;

        if( fscanf( fp_3param, " %s %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf ",
           ellipsoidCode, &deltaX, &sigmaX, &deltaY, &sigmaY, &deltaZ, &sigmaZ, 
           &southLatitude, &northLatitude, &westLongitude, &eastLongitude ) <= 0 )
        {
           fclose( fp_3param );
           throw CoordinateConversionException( ErrorMessages::datumFileParseError );
        }
        else
        {
          southLatitude *= PI_OVER_180;
          northLatitude *= PI_OVER_180;
          westLongitude *= PI_OVER_180;
          eastLongitude *= PI_OVER_180;

          datumList.push_back( new ThreeParameterDatum(
             index, code, ellipsoidCode, name, DatumType::threeParamDatum,
             deltaX, deltaY, deltaZ, westLongitude, eastLongitude,
             southLatitude, northLatitude, sigmaX, sigmaY, sigmaZ,
             userDefined ) );
        }

        index++;
        datum3ParamCount++;
    }
    fclose( fp_3param );

  delete [] FileName7;
  FileName7 = 0;
  delete [] FileName3;
  FileName3 = 0;
}


void DatumLibraryImplementation::write3ParamFile()
{
/*
 *  The function write3ParamFile writes the 3 parameter datums in the datum list
 *  to the 3_param.dat file.
 */

  char datum_name[DATUM_NAME_LENGTH+2];
  char *PathName = NULL;
  char FileName[FILENAME_LENGTH];
  FILE *fp_3param = NULL;

  CCSThreadLock lock(&mutex);

  /*output updated 3-parameter datum table*/
  PathName = getenv( "MSPCCS_DATA" );
  if (PathName != NULL)
  {
    strcpy( FileName, PathName );
    strcat( FileName, "/" );
  }
  else
  {
    strcpy( FileName, "../../data/" );
  }
  strcat( FileName, "3_param.dat" );

  if ((fp_3param = fopen(FileName, "w")) == NULL)
  { /* fatal error */
    char message[256] = "";
    if (NULL == PathName)
    {
      strcpy( message, "Environment variable undefined: MSPCCS_DATA." );
    }
    else
    {
      strcpy( message, ErrorMessages::datumFileOpenError );
      strcat( message, ": 3_param.dat\n" );
    }
    throw CoordinateConversionException( message );
  }

  /* write file */
  long index = MAX_WGS + datum7ParamCount;
  int size = datumList.size();
  while( index < size )
  {
    ThreeParameterDatum* _3parameterDatum = ( ThreeParameterDatum* )datumList[index];
    if( _3parameterDatum )
    {
      strcpy( datum_name, "\"" );
      strcat( datum_name, datumList[index]->name());
      strcat( datum_name, "\"" );
      if( _3parameterDatum->userDefined() )
        fprintf( fp_3param, "*");

      fprintf( fp_3param, "%-6s  %-33s%-2s %4.0f %4.0f %4.0f %4.0f %5.0f %4.0f %4.0f %4.0f %4.0f %4.0f \n",
               _3parameterDatum->code(),
               datum_name,
               _3parameterDatum->ellipsoidCode(),
               _3parameterDatum->deltaX(),
               _3parameterDatum->sigmaX(),
               _3parameterDatum->deltaY(),
               _3parameterDatum->sigmaY(),
               _3parameterDatum->deltaZ(),
               _3parameterDatum->sigmaZ(),
               ( _3parameterDatum->southLatitude() * _180_OVER_PI ),
               ( _3parameterDatum->northLatitude() * _180_OVER_PI ),
               ( _3parameterDatum->westLongitude() * _180_OVER_PI ),
               ( _3parameterDatum->eastLongitude() * _180_OVER_PI ) );
    }

    index++;
  }

  fclose( fp_3param );

}


void DatumLibraryImplementation::write7ParamFile()
{
/*
 *  The function write3ParamFile writes the 7 parameter datums in the datum list
 *  to the 7_param.dat file.
 */

  char datum_name[DATUM_NAME_LENGTH+2];
  char *PathName = NULL;
  char FileName[FILENAME_LENGTH];
  FILE *fp_7param = NULL;

  CCSThreadLock lock(&mutex);

  /*output updated 7-parameter datum table*/
  PathName = getenv( "MSPCCS_DATA" );
  if (PathName != NULL)
  {
    strcpy( FileName, PathName );
    strcat( FileName, "/" );
  }
  else
  {
    strcpy( FileName, "../../data/" );
  }
  strcat( FileName, "7_param.dat" );

  if ((fp_7param = fopen(FileName, "w")) == NULL)
  { /* fatal error */
    char message[256] = "";
    if (NULL == PathName)
    {
      strcpy( message, "Environment variable undefined: MSPCCS_DATA." );
    }
    else
    {
      strcpy( message, ErrorMessages::datumFileOpenError );
      strcat( message, ": 7_param.dat\n" );
    }
    throw CoordinateConversionException( message );
  }

  /* write file */
  long index = MAX_WGS;
  int endIndex = datum7ParamCount + MAX_WGS;
  while( index < endIndex )
  {
    SevenParameterDatum* _7parameterDatum = ( SevenParameterDatum* )datumList[index];
    if( _7parameterDatum )
    {
      strcpy( datum_name, "\"" );
      strcat( datum_name, datumList[index]->name());
      strcat( datum_name, "\"" );
      if( _7parameterDatum->userDefined() )
        fprintf( fp_7param, "*");

      fprintf( fp_7param, "%-6s  %-33s%-2s  %4.0f  %4.0f  %4.0f % 4.3f % 4.3f % 4.3f   % 4.10f \n",
               _7parameterDatum->code(),
               datum_name,
               _7parameterDatum->ellipsoidCode(),
               _7parameterDatum->deltaX(),
               _7parameterDatum->deltaY(),
               _7parameterDatum->deltaZ(),
               _7parameterDatum->rotationX() * SECONDS_PER_RADIAN,
               _7parameterDatum->rotationY() * SECONDS_PER_RADIAN,
               _7parameterDatum->rotationZ() * SECONDS_PER_RADIAN,
               _7parameterDatum->scaleFactor() );
    }

    index++;
  }

  fclose( fp_7param );
}


GeodeticCoordinates* DatumLibraryImplementation::geodeticShiftWGS84ToWGS72( const double WGS84Longitude, const double WGS84Latitude, const double WGS84Height )
{ 
/*
 *  The function geodeticShiftWGS84ToWGS72 shifts a geodetic coordinate (latitude, longitude in radians
 *  and height in meters) relative to WGS84 to a geodetic coordinate
 *  (latitude, longitude in radians and height in meters) relative to WGS72.
 *
 *  WGS84Longitude : Longitude in radians relative to WGS84    (input)
 *  WGS84Latitude  : Latitude in radians relative to WGS84     (input)
 *  WGS84Height    : Height in meters  relative to WGS84       (input)
 *  WGS72Longitude : Longitude in radians relative to WGS72    (output)
 *  WGS72Latitude  : Latitude in radians relative to WGS72     (output)
 *  WGS72Height    : Height in meters relative to WGS72        (output)
 */

  double Delta_Lat;
  double Delta_Lon;
  double Delta_Hgt;
  double WGS84_a;       /* Semi-major axis of WGS84 ellipsoid               */
  double WGS84_f;       /* Flattening of WGS84 ellipsoid                    */
  double WGS72_a;       /* Semi-major axis of WGS72 ellipsoid               */
  double WGS72_f;       /* Flattening of WGS72 ellipsoid                    */
  double da;            /* WGS72_a - WGS84_a                                */
  double df;            /* WGS72_f - WGS84_f                                */
  double Q;
  double sin_Lat;
  double sin2_Lat;

  long wgs84EllipsoidIndex;
  _ellipsoidLibraryImplementation->ellipsoidIndex( "WE", &wgs84EllipsoidIndex );
  _ellipsoidLibraryImplementation->ellipsoidParameters( wgs84EllipsoidIndex, &WGS84_a, &WGS84_f );

  long wgs72EllipsoidIndex;
  _ellipsoidLibraryImplementation->ellipsoidIndex( "WD", &wgs72EllipsoidIndex );
  _ellipsoidLibraryImplementation->ellipsoidParameters( wgs72EllipsoidIndex, &WGS72_a, &WGS72_f );

  da = WGS72_a - WGS84_a;
  df = WGS72_f - WGS84_f;
  Q = PI / 648000;
  sin_Lat = sin(WGS84Latitude);
  sin2_Lat = sin_Lat * sin_Lat;

  Delta_Lat = (-4.5 * cos(WGS84Latitude)) / (WGS84_a*Q)
              + (df * sin(2*WGS84Latitude)) / Q;
  Delta_Lat /= SECONDS_PER_RADIAN;
  Delta_Lon = -0.554 / SECONDS_PER_RADIAN;
  Delta_Hgt = -4.5 * sin_Lat + WGS84_a * df * sin2_Lat - da - 1.4;

  double wgs72Latitude = WGS84Latitude + Delta_Lat;
  double wgs72Longitude = WGS84Longitude + Delta_Lon;
  double wgs72Height = WGS84Height + Delta_Hgt;

  if (wgs72Latitude > PI_OVER_2)
    wgs72Latitude = PI_OVER_2 - (wgs72Latitude - PI_OVER_2);
  else if (wgs72Latitude < -PI_OVER_2)
    wgs72Latitude = -PI_OVER_2  - (wgs72Latitude + PI_OVER_2);

  if (wgs72Longitude > PI)
    wgs72Longitude -= TWO_PI;
  if (wgs72Longitude < -PI)
    wgs72Longitude += TWO_PI;

  return new GeodeticCoordinates(CoordinateType::geodetic, wgs72Longitude, wgs72Latitude, wgs72Height);
} 


GeodeticCoordinates* DatumLibraryImplementation::geodeticShiftWGS72ToWGS84( const double WGS72Longitude, const double WGS72Latitude, const double WGS72Height  )
{ 
/*
 *  The function geodeticShiftWGS72ToWGS84 shifts a geodetic coordinate (latitude, longitude in radians
 *  and height in meters) relative to WGS72 to a geodetic coordinate
 *  (latitude, longitude in radians and height in meters) relative to WGS84.
 *
 *  WGS72Longitude : Longitude in radians relative to WGS72    (input)
 *  WGS72Latitude  : Latitude in radians relative to WGS72     (input)
 *  WGS72Height    : Height in meters relative to WGS72        (input)
 *  WGS84Longitude : Longitude in radians relative to WGS84    (output)
 *  WGS84Latitude  : Latitude in radians relative to WGS84     (output)
 *  WGS84Height    : Height in meters  relative to WGS84       (output)
 */

  double Delta_Lat;
  double Delta_Lon;
  double Delta_Hgt;
  double WGS84_a;       /* Semi-major axis of WGS84 ellipsoid               */
  double WGS84_f;       /* Flattening of WGS84 ellipsoid                    */
  double WGS72_a;       /* Semi-major axis of WGS72 ellipsoid               */
  double WGS72_f;       /* Flattening of WGS72 ellipsoid                    */
  double da;            /* WGS84_a - WGS72_a                                */
  double df;            /* WGS84_f - WGS72_f                                */
  double Q;
  double sin_Lat;
  double sin2_Lat;

  long wgs84EllipsoidIndex;
  _ellipsoidLibraryImplementation->ellipsoidIndex( "WE", &wgs84EllipsoidIndex );
  _ellipsoidLibraryImplementation->ellipsoidParameters( wgs84EllipsoidIndex, &WGS84_a, &WGS84_f );

  long wgs72EllipsoidIndex;
  _ellipsoidLibraryImplementation->ellipsoidIndex( "WD", &wgs72EllipsoidIndex );
  _ellipsoidLibraryImplementation->ellipsoidParameters( wgs72EllipsoidIndex, &WGS72_a, &WGS72_f );

  da = WGS84_a - WGS72_a;
  df = WGS84_f - WGS72_f;
  Q = PI /  648000;
  sin_Lat = sin(WGS72Latitude);
  sin2_Lat = sin_Lat * sin_Lat;

  Delta_Lat = (4.5 * cos(WGS72Latitude)) / (WGS72_a*Q) + (df * sin(2*WGS72Latitude)) / Q;
  Delta_Lat /= SECONDS_PER_RADIAN;
  Delta_Lon = 0.554 / SECONDS_PER_RADIAN;
  Delta_Hgt = 4.5 * sin_Lat + WGS72_a * df * sin2_Lat - da + 1.4;

  double wgs84Latitude = WGS72Latitude + Delta_Lat;
  double wgs84Longitude = WGS72Longitude + Delta_Lon;
  double wgs84Height = WGS72Height + Delta_Hgt;

  if (wgs84Latitude > PI_OVER_2)
    wgs84Latitude = PI_OVER_2 - (wgs84Latitude - PI_OVER_2);
  else if (wgs84Latitude < -PI_OVER_2)
    wgs84Latitude = -PI_OVER_2  - (wgs84Latitude + PI_OVER_2);

  if (wgs84Longitude > PI)
    wgs84Longitude -= TWO_PI;
  if (wgs84Longitude < -PI)
    wgs84Longitude += TWO_PI;

  return new GeodeticCoordinates(CoordinateType::geodetic, wgs84Longitude, wgs84Latitude, wgs84Height);
} 


CartesianCoordinates* DatumLibraryImplementation::geocentricShiftWGS84ToWGS72( const double X_WGS84, const double Y_WGS84, const double Z_WGS84 )
{ 
/*
 *  The function geocentricShiftWGS84ToWGS72 shifts a geocentric coordinate (X, Y, Z in meters) relative
 *  to WGS84 to a geocentric coordinate (X, Y, Z in meters) relative to WGS72.
 *
 *  X_WGS84 : X coordinate relative to WGS84            (input)
 *  Y_WGS84 : Y coordinate relative to WGS84            (input)
 *  Z_WGS84 : Z coordinate relative to WGS84            (input)
 *  X       : X coordinate relative to WGS72            (output)
 *  Y       : Y coordinate relative to WGS72            (output)
 *  Z       : Z coordinate relative to WGS72            (output)
 */

  double a_72;   /* Semi-major axis in meters of WGS72 ellipsoid */
  double f_72;   /* Flattening of WGS72 ellipsoid      */
  double a_84;   /* Semi-major axis in meters of WGS84 ellipsoid */
  double f_84;   /* Flattening of WGS84 ellipsoid      */

  /* Set WGS84 ellipsoid params */
  long wgs84EllipsoidIndex;
  _ellipsoidLibraryImplementation->ellipsoidIndex( "WE", &wgs84EllipsoidIndex );
  _ellipsoidLibraryImplementation->ellipsoidParameters( wgs84EllipsoidIndex, &a_84, &f_84 );

  Geocentric geocentric84( a_84, f_84 );

  GeodeticCoordinates* wgs84GeodeticCoordinates = geocentric84.convertToGeodetic( new CartesianCoordinates( CoordinateType::geocentric, X_WGS84, Y_WGS84, Z_WGS84 ) );

  GeodeticCoordinates* wgs72GeodeticCoordinates = geodeticShiftWGS84ToWGS72( wgs84GeodeticCoordinates->longitude(), wgs84GeodeticCoordinates->latitude(), wgs84GeodeticCoordinates->height() );

  /* Set WGS72 ellipsoid params */
  long wgs72EllipsoidIndex;
  _ellipsoidLibraryImplementation->ellipsoidIndex( "WD", &wgs72EllipsoidIndex );
  _ellipsoidLibraryImplementation->ellipsoidParameters( wgs72EllipsoidIndex, &a_72, &f_72 );

  Geocentric geocentric72( a_72, f_72 );

  CartesianCoordinates* wgs72GeocentricCoordinates = geocentric72.convertFromGeodetic( wgs72GeodeticCoordinates );

  delete wgs72GeodeticCoordinates;
  delete wgs84GeodeticCoordinates;

  return wgs72GeocentricCoordinates;
} 


CartesianCoordinates* DatumLibraryImplementation::geocentricShiftWGS72ToWGS84( const double X, const double Y, const double Z )
{ 
/*
 *  The function geocentricShiftWGS72ToWGS84 shifts a geocentric coordinate (X, Y, Z in meters) relative
 *  to WGS72 to a geocentric coordinate (X, Y, Z in meters) relative to WGS84.
 *
 *  X       : X coordinate relative to WGS72            (input)
 *  Y       : Y coordinate relative to WGS72            (input)
 *  Z       : Z coordinate relative to WGS72            (input)
 *  X_WGS84 : X coordinate relative to WGS84            (output)
 *  Y_WGS84 : Y coordinate relative to WGS84            (output)
 *  Z_WGS84 : Z coordinate relative to WGS84            (output)
 */

  double a_72;   /* Semi-major axis in meters of WGS72 ellipsoid */
  double f_72;   /* Flattening of WGS72 ellipsoid                */
  double a_84;   /* Semi-major axis in meters of WGS84 ellipsoid */
  double f_84;   /* Flattening of WGS84 ellipsoid                */

  /* Set WGS72 ellipsoid params */
  long wgs72EllipsoidIndex;
  _ellipsoidLibraryImplementation->ellipsoidIndex( "WD", &wgs72EllipsoidIndex );
  _ellipsoidLibraryImplementation->ellipsoidParameters( wgs72EllipsoidIndex, &a_72, &f_72 );

  Geocentric geocentric72( a_72, f_72 );
  GeodeticCoordinates* wgs72GeodeticCoordinates = geocentric72.convertToGeodetic( new CartesianCoordinates( CoordinateType::geocentric, X, Y, Z ) );

  GeodeticCoordinates* wgs84GeodeticCoordinates = geodeticShiftWGS72ToWGS84( wgs72GeodeticCoordinates->longitude(), wgs72GeodeticCoordinates->latitude(), wgs72GeodeticCoordinates->height() );

  /* Set WGS84 ellipsoid params */
  long wgs84EllipsoidIndex;
  _ellipsoidLibraryImplementation->ellipsoidIndex( "WE", &wgs84EllipsoidIndex );
  _ellipsoidLibraryImplementation->ellipsoidParameters( wgs84EllipsoidIndex, &a_84, &f_84 );

  Geocentric geocentric84( a_84, f_84 );
  CartesianCoordinates* wgs84GeocentricCoordinates = geocentric84.convertFromGeodetic( wgs84GeodeticCoordinates );

  delete wgs84GeodeticCoordinates;
  delete wgs72GeodeticCoordinates;

  return wgs84GeocentricCoordinates;
} 


// CLASSIFICATION: UNCLASSIFIED
