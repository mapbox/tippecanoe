// CLASSIFICATION: UNCLASSIFIED

/****************************************************************************/
/* RSC IDENTIFIER:  Ellipsoid Library Implementation
 *
 * ABSTRACT
 *
 *    The purpose of ELLIPSOID is to provide access to ellipsoid parameters
 *    for a collection of common ellipsoids.  A particular ellipsoid can be
 *    accessed by using its standard 2-letter code to find its index in the
 *    ellipsoid table.  The index can then be used to retrieve the ellipsoid
 *    name and parameters.
 *
 *    By sequentially retrieving all of the ellipsoid codes and/or names, a
 *    menu of the available ellipsoids can be constructed.  The index values
 *    resulting from selections from this menu can then be used to access the
 *    parameters of the selected ellipsoid.
 *
 *    This component depends on a data file named "ellips.dat", which contains
 *    the ellipsoid parameter values.  A copy of this file must be located in
 *    the directory specified by the environment variable "MSPCCS_DATA", if
 *    defined, or else in the current directory, whenever a program containing
 *    this component is executed.
 *
 *    Additional ellipsoids can be added to this file, either manually or using
 *    the Create_Ellipsoid function.  However, if a large number of ellipsoids
 *    are added, the ellipsoid table array size in this component will have to
 *    be increased.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *  ELLIPSE_NO_ERROR             : No errors occured in function
 *  ELLIPSE_FILE_OPEN_ERROR      : Ellipsoid file opening error
 *  ELLIPSE_INITIALIZE_ERROR     : Ellipsoid table can not initialize
 *  ELLIPSE_NOT_INITIALIZED_ERROR: Ellipsoid table not initialized properly
 *  ELLIPSE_INVALID_INDEX_ERROR  : Index is an invalid value
 *  ELLIPSE_INVALID_CODE_ERROR   : Code was not found in table
 *  ELLIPSE_A_ERROR              : Semi-major axis less than or equal to zero
 *  ELLIPSE_INV_F_ERROR          : Inverse flattening outside of valid range
 *                                  (250 to 350)
 *  ELLIPSE_NOT_USERDEF_ERROR    : Ellipsoid is not user defined - cannot be
 *                                  deleted
 *
 * REUSE NOTES
 *
 *    Ellipsoid is intended for reuse by any application that requires Earth
 *    approximating ellipsoids.
 *
 * REFERENCES
 *
 *    Further information on Ellipsoid can be found in the Reuse Manual.
 *
 *    Ellipsoid originated from :  U.S. Army Topographic Engineering Center (USATEC)
 *                                 Geospatial Information Division (GID)
 *                                 7701 Telegraph Road
 *                                 Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    Ellipsoid has no restrictions.
 *
 * ENVIRONMENT
 *
 *    Ellipsoid was tested and certified in the following environments
 *
 *    1. Solaris 2.5
 *    2. Windows 95
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    11-19-95          Original Code
 *    17-Jan-97         Moved local constants out of public interface
 *                      Improved efficiency in algorithms (GEOTRANS)
 *    24-May-99         Added user-defined ellipsoids (GEOTRANS for JMTK)
 *    06-27-06          Moved data file to data directory
 *    03-09-07          Original C++ Code
 *    06-11-10          S. Gillis, BAEts26724, Fixed memory error problem
 *                      when MSPCCS_DATA is not set
 *    07-07-10          K.Lam, BAEts27269, Replace C functions in threads.h
 *                      with C++ methods in classes CCSThreadMutex
 *    05-16-11          T. Thompson, BAEts27393, Inform user when MSPCCS_DATA
 *                      is not defined.
 *    07/17/12          S.Gillis,MSP_00029561,Fixed problem with creating and 
 *                      deleting ellipsoid
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "EllipsoidLibraryImplementation.h"
#include "Ellipsoid.h"
#include "DatumLibraryImplementation.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"
#include "CCSThreadMutex.h"
#include "CCSThreadLock.h"

#ifdef NDK_BUILD
#include <string>
#include <iostream>
#include <sstream>
#include <android/log.h>

using namespace std;

#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "GtApp", __VA_ARGS__))
#endif


/*
 *    ctype.h    - standard C character handling library
 *    stdio.h    - standard C input/output library
 *    stdlib.h   - standard C general utilities library
 *    string.h   - standard C string handling library
 *    DatumLibraryImplementation.h    - used to determine
 *          if user defined ellipsoid is in use by a user defined datum
 *    EllipsoidLibraryImplementation.h  - prototype error checking and error codes
 *    Ellipsoid.h  - used to store individual ellipsoid information
 *    threads.h  - used for thread safety
 *    CoordinateConversionException.h - Exception handler
 *    ErrorMessages.h  - Contains exception messages
 */


using namespace MSP::CCS;
using MSP::CCSThreadMutex;
using MSP::CCSThreadLock;


/***************************************************************************/
/*                               DEFINES 
 *
 */

const int ELLIPSOID_CODE_LENGTH = 3;  /* Length of ellipsoid code (including null) */
const int ELLIPSOID_NAME_LENGTH = 30; /* Max length of ellipsoid name */
const int ELLIPSOID_BUF = 90;
const int FILENAME_LENGTH = 128;
const char *WGS84_Ellipsoid_Code = "WE";
const char *WGS72_Ellipsoid_Code = "WD";


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
class EllipsoidLibraryImplementationCleaner
{
  public:

  ~EllipsoidLibraryImplementationCleaner()
  {
    CCSThreadLock lock(&EllipsoidLibraryImplementation::mutex);
    EllipsoidLibraryImplementation::deleteInstance();
  }

} ellipsoidLibraryImplementationCleanerInstance;
  }
}

// Make this class a singleton, so the data file is only initialized once
CCSThreadMutex EllipsoidLibraryImplementation::mutex;
EllipsoidLibraryImplementation* EllipsoidLibraryImplementation::instance = 0;
int EllipsoidLibraryImplementation::instanceCount = 0;


EllipsoidLibraryImplementation* EllipsoidLibraryImplementation::getInstance()
{
  CCSThreadLock lock(&mutex);
  if( instance == 0 )
    instance = new EllipsoidLibraryImplementation;

  instanceCount++;

  return instance;
}


void EllipsoidLibraryImplementation::removeInstance()
{
/*
 * The function removeInstance removes this EllipsoidLibraryImplementation instance from the
 * total number of instances. 
 */
  CCSThreadLock lock(&mutex);
  if( --instanceCount < 1 )
  {
    deleteInstance();
  }
}


void EllipsoidLibraryImplementation::deleteInstance()
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


EllipsoidLibraryImplementation::EllipsoidLibraryImplementation():
  _datumLibraryImplementation( 0 )
{
   /*
    * The constructor loads ellipsoids from data file. 
    */
   loadEllipsoids();
}


EllipsoidLibraryImplementation::EllipsoidLibraryImplementation( const EllipsoidLibraryImplementation &el )
{
  int size = el.ellipsoidList.size();
  for( int i = 0; i < size; i++ )
    ellipsoidList.push_back( new Ellipsoid( *( el.ellipsoidList[i] ) ) );

  _datumLibraryImplementation = el._datumLibraryImplementation;
}


EllipsoidLibraryImplementation::~EllipsoidLibraryImplementation()
{
  std::vector<Ellipsoid*>::iterator iter = ellipsoidList.begin();
  while( iter != ellipsoidList.end() )
	{
		delete( *iter );
    iter++;
	}
  ellipsoidList.clear();

  _datumLibraryImplementation = 0;
}


EllipsoidLibraryImplementation& EllipsoidLibraryImplementation::operator=( const EllipsoidLibraryImplementation &el )
{
  if ( &el == this )
	  return *this;

  int size = el.ellipsoidList.size();
  for( int i = 0; i < size; i++ )
    ellipsoidList[i] = new Ellipsoid( *( el.ellipsoidList[i] ) );

  _datumLibraryImplementation = el._datumLibraryImplementation;

  return *this;
}


void EllipsoidLibraryImplementation::defineEllipsoid( const char* code, const char* name, double semiMajorAxis, double flattening )
{ 
/*
 * The function defineEllipsoid creates a new ellipsoid with the specified
 * Code, name, and axes.  If the ellipsoid table has not been initialized,
 * the specified code is already in use, or a new version of the ellips.dat
 * file cannot be created, an exception is thrown.
 * Note that the indexes of all ellipsoids in the ellipsoid
 * table may be changed by this function.
 *
 *   code           : 2-letter ellipsoid code.                      (input)
 *   name           : Name of the new ellipsoid                     (input)
 *   semiMajorAxis  : Semi-major axis, in meters, of new ellipsoid  (input)
 *   flattening     : Flattening of new ellipsoid.                  (input)
 *
 */

  long code_length = 0;
  char *PathName = NULL;
  char FileName[FILENAME_LENGTH];
  char ellipsoid_code[ELLIPSOID_CODE_LENGTH];
  FILE *fp = NULL;                    /* File pointer to file ellips.dat     */
  long index = 0;
  long numEllipsoids = ellipsoidList.size();
  double inv_f = 1 / flattening;

#ifdef NDK_BUILD
  __android_log_print(ANDROID_LOG_VERBOSE, "GtApp", "numEllipsoid %d ", numEllipsoids );
#endif

  // assume the ellipsoid code is new
  bool isNewEllipsoidCode = true;
  try
  {
     // check if ellipsoid code exists
     ellipsoidIndex( code, &index );
     // get here if ellipsoid code is found in current ellipsoid table
     isNewEllipsoidCode = false;
  }
  catch(CoordinateConversionException e)
  {
     // the ellipsoid code is new, keep going
  }

  // the ellipsoid code exists in current ellipsoid table, throw an error
  if ( !isNewEllipsoidCode )
     throw CoordinateConversionException( ErrorMessages::invalidEllipsoidCode );

  code_length = strlen( code );

  if( ( code_length > ( ELLIPSOID_CODE_LENGTH - 1 ) ) )
     throw CoordinateConversionException( ErrorMessages::invalidEllipsoidCode );
  if( semiMajorAxis <= 0.0 )
     throw CoordinateConversionException( ErrorMessages::semiMajorAxis );
  if( (inv_f < 250 ) || ( inv_f > 350 ) )
  { /* Inverse flattening must be between 250 and 350 */
     throw CoordinateConversionException( ErrorMessages::ellipsoidFlattening );
  }

  strcpy( ellipsoid_code, code );
  /* Convert code to upper case */
  for( int i = 0; i < code_length; i++ )
     ellipsoid_code[i] = ( char )toupper( ellipsoid_code[i] );

  double semiMinorAxis = semiMajorAxis * ( 1 - flattening );
  double eccentricitySquared = 2.0 * flattening - flattening * flattening;
  ellipsoidList.push_back( new Ellipsoid( index, ellipsoid_code, ( char* )name, 
        semiMajorAxis, semiMinorAxis, flattening, eccentricitySquared, true ) );

  numEllipsoids++;

  CCSThreadLock lock(&mutex);

  /*output updated ellipsoid table*/
  PathName = getenv( "MSPCCS_DATA" );
  if( PathName != NULL )
  {
     strcpy( FileName, PathName );
     strcat( FileName, "/" );
  }
  else
  {
     strcpy( FileName, "../../data/" );
  }
  strcat( FileName, "ellips.dat" );

  if( ( fp = fopen( FileName, "w" ) ) == NULL )
  { /* fatal error */
     throw CoordinateConversionException( ErrorMessages::ellipsoidFileOpenError );
  }

  /* write file */
  index = 0;
  while( index < numEllipsoids )
  {
     if( ellipsoidList[index]->userDefined() )
     {
        fprintf( fp, "*%-28s  %-2s %11.9f %12.9f %13.13f \n",
           ellipsoidList[index]->name(),
           ellipsoidList[index]->code(),
           ellipsoidList[index]->semiMajorAxis(),
           ellipsoidList[index]->semiMinorAxis(),
           1 / ellipsoidList[index]->flattening() );
     }
     else
     {
        fprintf( fp, "%-29s  %-2s %11.9f %12.9f %13.13f \n",
           ellipsoidList[index]->name(),
           ellipsoidList[index]->code(),
           ellipsoidList[index]->semiMajorAxis(),
           ellipsoidList[index]->semiMinorAxis(),
           1 / ellipsoidList[index]->flattening() );
     }
     index++;
  }

  fclose( fp );
} 


void EllipsoidLibraryImplementation::removeEllipsoid( const char* code )
{
/*
 * The function removeEllipsoid deletes a user defined ellipsoid with
 * the specified Code.  If the ellipsoid table has not been created,
 * the specified code is in use by a user defined datum, or a new version
 * of the ellips.dat file cannot be created, an exception is thrown.
 * Note that the indexes of all
 * ellipsoids in the ellipsoid table may be changed by this function.
 *
 *   code     : 2-letter ellipsoid code.                      (input)
 *
 */

  long index = 0;
  char *PathName = NULL;
  char FileName[FILENAME_LENGTH];
  FILE *fp = NULL;                    /* File pointer to file ellips.dat     */

  ellipsoidIndex( code, &index );
  if( ellipsoidList[index]->userDefined() )
  {
    if( _datumLibraryImplementation )
    {
      if( _datumLibraryImplementation->datumUsesEllipsoid( code ) )
        throw CoordinateConversionException( ErrorMessages::ellipseInUse );
    }
  }
  else
    throw CoordinateConversionException( ErrorMessages::notUserDefined );

   ellipsoidList.erase( ellipsoidList.begin() + index ); 

   int numEllipsoids = ellipsoidList.size();

   CCSThreadLock lock(&mutex);

   /*output updated ellipsoid table*/
   PathName = getenv( "MSPCCS_DATA" );
   if( PathName != NULL )
   {
      strcpy( FileName, PathName );
      strcat( FileName, "/" );
   }
   else
   {
      strcpy( FileName, "../../data/" );
   }
   strcat( FileName, "ellips.dat" );
   if( ( fp = fopen( FileName, "w" ) ) == NULL )
   { /* fatal error */
      throw CoordinateConversionException( ErrorMessages::ellipsoidFileOpenError );
   }
   /* write file */
   index = 0;
   while( index < numEllipsoids )
   {
      if( ellipsoidList[index]->userDefined() )
      {
         fprintf(fp, "*%-28s  %-2s %11.3f %12.4f %13.9f \n",
            ellipsoidList[index]->name(),
            ellipsoidList[index]->code(),
            ellipsoidList[index]->semiMajorAxis(),
            ellipsoidList[index]->semiMinorAxis(),
            1 / ellipsoidList[index]->flattening() );
      }
      else
      {
         fprintf(fp, "*%-29s  %-2s %11.3f %12.4f %13.9f \n",
            ellipsoidList[index]->name(),
            ellipsoidList[index]->code(),
            ellipsoidList[index]->semiMajorAxis(),
            ellipsoidList[index]->semiMinorAxis(),
            1 / ellipsoidList[index]->flattening() );
      }
      index++;
   }

   fclose( fp );
}


void EllipsoidLibraryImplementation::ellipsoidCount( long *count )
{ 
/*
 * The function ellipsoidCount returns the number of ellipsoids in the
 * ellipsoid table.  If the ellipsoid table has not been initialized,
 * an exception is thrown.
 *
 *   count    : The number of ellipsoids in the ellipsoid table. (output)
 *
 */

  *count = ellipsoidList.size();
} 


void EllipsoidLibraryImplementation::ellipsoidIndex( const char *code, long* index )
{ 
/*
 *  The function ellipsoidIndex returns the index of the ellipsoid in
 *  the ellipsoid table with the specified code.  If ellipsoid code is not found,
 *  an exception is thrown.
 *
 *    code     : 2-letter ellipsoid code.                      (input)
 *    index    : Index of the ellipsoid in the ellipsoid table with the
 *                  specified code                             (output)
 *
 */

  char temp_code[3];
  long i = 0;                   /* index for ellipsoid table */
  long j = 0;

  while( j < ELLIPSOID_CODE_LENGTH )
  {
    temp_code[j] = ( char )toupper(code[j]);
    j++;
  }
  temp_code[ELLIPSOID_CODE_LENGTH - 1] = 0;

  int numEllipsoids = ellipsoidList.size();

#ifdef NDK_BUILD
  __android_log_print(ANDROID_LOG_VERBOSE, "GtApp", "ellipsoid code %s %d ", code, numEllipsoids );
#endif

  while( ( i < numEllipsoids )
         && strcmp( temp_code, ellipsoidList[i]->code() ) )
  {
    i++;
  }

  if( i == numEllipsoids )
    throw CoordinateConversionException( ErrorMessages::invalidEllipsoidCode );
  else
  {
    if ( strcmp( temp_code, ellipsoidList[i]->code() ) )
      throw CoordinateConversionException( ErrorMessages::invalidEllipsoidCode );
    else
      *index = i;
  }
} 


void EllipsoidLibraryImplementation::ellipsoidCode( const long index, char *code )
{ 
/*
 *  The Function ellipsoidCode returns the 2-letter code for the
 *  ellipsoid in the ellipsoid table with the specified index.  If index is
 *  invalid, an exception is thrown.
 *
 *    index    : Index of a given ellipsoid in the ellipsoid table (input)
 *    code     : 2-letter ellipsoid code.                          (output)
 *
 */

  strcpy( code, "" );

  if ( ( index < 0 ) || ( index >= ellipsoidList.size() ) )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  else
    strcpy( code, ellipsoidList[index]->code() );
} 


void EllipsoidLibraryImplementation::ellipsoidName( const long index, char *name )
{ 
/*
 *  The function ellipsoidName returns the name of the ellipsoid in
 *  the ellipsoid table with the specified index.  If index is invalid,
 *  an exception is thrown.
 *
 *    index   : Index of a given ellipsoid.in the ellipsoid table with the
 *                 specified index                             (input)
 *    name    : Name of the ellipsoid referencd by index       (output)
 *
 */

  strcpy( name,"" );

  if( ( index < 0 ) || ( index >= ellipsoidList.size() ) )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  else
    strcpy( name, ellipsoidList[index]->name() );
} 


void EllipsoidLibraryImplementation::ellipsoidParameters( const long index, double *a, double *f )
{ 
/*
 *  The function ellipsoidParameters returns the semi-major axis and flattening
 *  for the ellipsoid with the specified index.  If index is invalid,
 *  exception is thrown.
 *
 *    index    : Index of a given ellipsoid in the ellipsoid table (input)
 *    a        : Semi-major axis, in meters, of ellipsoid          (output)
 *    f        : Flattening of ellipsoid.                          (output)
 *
 */

  *a = 0;
  *f = 0;

  if( ( index < 0 ) || ( index >= ellipsoidList.size() ) )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  else
  {
    Ellipsoid* ellipsoid = ellipsoidList[index];
    *a = ellipsoid->semiMajorAxis();
    *f = ellipsoid->flattening();
  }
} 


void EllipsoidLibraryImplementation::ellipsoidEccentricity2( const long index, double *eccentricitySquared )
{ 
/*
 *  The function ellipsoidEccentricity2 returns the square of the
 *  eccentricity for the ellipsoid with the specified index.  If index is
 *  invalid, an exception is thrown.
 *
 *    index                : Index of a given ellipsoid in the ellipsoid table (input)
 *    eccentricitySquared  : Square of eccentricity of ellipsoid               (output)
 *
 */

  *eccentricitySquared = 0;

  if( ( index < 0 ) || ( index >= ellipsoidList.size() ) )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  else
    *eccentricitySquared = ellipsoidList[index]->eccentricitySquared();
} 


void EllipsoidLibraryImplementation::ellipsoidUserDefined( const long index, long *result )
{ 
/*
 *  The function ellipsoidUserDefined returns 1 if the ellipsoid is user
 *  defined.  Otherwise, 0 is returned.  If index is invalid,
 *  an exception is thrown.
 *
 *    index    : Index of a given ellipsoid in the ellipsoid table (input)
 *    result   : Indicates whether specified ellipsoid is user defined (1)
 *               or not (0)                                        (output)
 *
 */

  *result = false;

  if( ( index < 0 ) || ( index >= ellipsoidList.size() ) )
    throw CoordinateConversionException( ErrorMessages::invalidIndex );
  else
    *result = ellipsoidList[index]->userDefined();
} 


void EllipsoidLibraryImplementation::setDatumLibraryImplementation( DatumLibraryImplementation* __datumLibraryImplementation )
{
/*
 *  The function setDatumLibraryImplementation sets the datum library information
 *  which is needed to ensure a user defined ellipsoid is not in use before being deleted.
 *
 *   __datumLibraryImplementation  : Datum library implementation      (input)
 *
 */

  _datumLibraryImplementation = __datumLibraryImplementation;
}


/************************************************************************/
/*                              PRIVATE FUNCTIONS     
 *
 */

void EllipsoidLibraryImplementation::loadEllipsoids()
{
/*
 * The function loadEllipsoids reads ellipsoid data from ellips.dat
 * and builds the ellipsoid table from it.  If an error occurs, an 
 * exception is thrown.
 */

  char* PathName = NULL;
  char* FileName = 0;
  FILE* fp = NULL;                    /* File pointer to file ellips.dat     */
  char buffer[ELLIPSOID_BUF];
  long index = 0;                     /* Array index                         */

  CCSThreadLock lock(&mutex);

  /*  Check the environment for a user provided path, else current directory;   */
  /*  Build a File Name, including specified or default path:                   */

#ifdef NDK_BUILD
  PathName = "/data/data/com.baesystems.msp.geotrans/lib/";
  FileName = new char[ 80 ];
  strcpy( FileName, PathName );
  strcat( FileName, "libellipsdat.so" );
#else
  PathName = getenv( "MSPCCS_DATA" );
  if (PathName != NULL)
  {
    FileName = new char[ strlen( PathName ) + 12 ];
    strcpy( FileName, PathName );
    strcat( FileName, "/" );
  }
  else
  {
    FileName = new char[ 22 ];
    strcpy( FileName, "../../data/" );
  }
  strcat( FileName, "ellips.dat" );
#endif

  /*  Open the File READONLY, or Return Error Condition:                        */

  if( ( fp = fopen( FileName, "r" ) ) == NULL )
  {
    delete [] FileName;
    FileName = 0;

    if (NULL == PathName)
    {
      throw CoordinateConversionException( "Environment variable undefined: MSPCCS_DATA." );
    }
    else
    {
      throw CoordinateConversionException( ErrorMessages::ellipsoidFileOpenError );
    }
  }

  /* read file */
  while( !feof( fp ) )
  {
     if( fgets( buffer, ELLIPSOID_BUF, fp ) )
     {
        char name[ELLIPSOID_NAME_LENGTH];
        char code[ELLIPSOID_CODE_LENGTH];
        double semiMajorAxis;
        double semiMinorAxis;
        double recpF;

        sscanf( buffer, "%30c %s %lf %lf %lf", name, code,  &semiMajorAxis, &semiMinorAxis, &recpF );

        bool userDefined = false;  /* Identifies a user defined ellipsoid */
        if( name[0] == '*' )
        {
          userDefined = true;
          for( int i = 0; i < ELLIPSOID_NAME_LENGTH; i++ )
            name[i] = name[i+1];
        }

        name[ELLIPSOID_NAME_LENGTH - 1] = '\0'; /* null terminate */

        double flattening = 1 / recpF;
        double eccentricitySquared = 2.0 * flattening - flattening * flattening;

#ifdef NDK_BUILD
        //      LOGW("GtApp: name=%s code=%s recpF=%f", name, code, recpF);
        __android_log_print(ANDROID_LOG_VERBOSE, "GtApp", "name %s", name);
        __android_log_print(ANDROID_LOG_VERBOSE, "GtApp", "code %s", code);
        __android_log_print(ANDROID_LOG_VERBOSE, "GtApp", "major %f", semiMajorAxis);
        __android_log_print(ANDROID_LOG_VERBOSE, "GtApp", "minor %f", semiMinorAxis);
        __android_log_print(ANDROID_LOG_VERBOSE, "GtApp", "recpF %f", recpF);
#endif

        ellipsoidList.push_back( new Ellipsoid( index, code, name, semiMajorAxis, semiMinorAxis, flattening, eccentricitySquared, userDefined ) );

        index++;
     }
  }
  
  fclose( fp );

  delete [] FileName;
  FileName = 0;
}

// CLASSIFICATION: UNCLASSIFIED
