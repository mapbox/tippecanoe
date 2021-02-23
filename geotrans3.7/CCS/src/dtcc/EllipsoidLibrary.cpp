// CLASSIFICATION: UNCLASSIFIED

/****************************************************************************/
/* RSC IDENTIFIER:  Ellipsoid Library
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
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include "EllipsoidLibrary.h"
#include "EllipsoidLibraryImplementation.h"

/*
 *    ctype.h    - standard C character handling library
 *    stdio.h    - standard C input/output library
 *    stdlib.h   - standard C general utilities library
 *    string.h   - standard C string handling library
 *    DatumLibrary.h    - used to determine if user defined ellipsoid
 *                  is in use by a user defined datum
 *    EllipsoidLibrary.h  - prototype error checking and error codes
 *    Ellipsoid.h  - used to store individual ellipsoid information
 *    threads.h  - used for thread safety
 *    CoordinateConversionException.h - Exception handler
 *    ErrorMessages.h  - Contains exception messages
 */


using namespace MSP::CCS;


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

EllipsoidLibrary::EllipsoidLibrary( EllipsoidLibraryImplementation* __ellipsoidLibraryImplementation )
{
/*
 * The constructor creates an empty list to store the ellipsoid data from ellips.dat
 * which is used to build the ellipsoid table.  
 */

  _ellipsoidLibraryImplementation = __ellipsoidLibraryImplementation;
}


EllipsoidLibrary::EllipsoidLibrary( const EllipsoidLibrary &el )
{
  _ellipsoidLibraryImplementation = el._ellipsoidLibraryImplementation;
}


EllipsoidLibrary::~EllipsoidLibrary()
{
  _ellipsoidLibraryImplementation = 0;
}


EllipsoidLibrary& EllipsoidLibrary::operator=( const EllipsoidLibrary &el )
{
  if ( &el == this )
	  return *this;

  _ellipsoidLibraryImplementation = el._ellipsoidLibraryImplementation;

  return *this;
}


void EllipsoidLibrary::defineEllipsoid( const char* code, const char* name, double semiMajorAxis, double flattening )
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

  _ellipsoidLibraryImplementation->defineEllipsoid( code, name, semiMajorAxis, flattening );
} 


void EllipsoidLibrary::removeEllipsoid( const char* code )
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

  _ellipsoidLibraryImplementation->removeEllipsoid( code );
}


void EllipsoidLibrary::getEllipsoidCount( long *count )
{ 
/*
 * The function getEllipsoidCount returns the number of ellipsoids in the
 * ellipsoid table.  
 *
 *   count    : The number of ellipsoids in the ellipsoid table. (output)
 *
 */

  _ellipsoidLibraryImplementation->ellipsoidCount( count );
} 


void EllipsoidLibrary::getEllipsoidIndex( const char *code, long* index )
{ 
/*
 *  The function getEllipsoidIndex returns the index of the ellipsoid in
 *  the ellipsoid table with the specified code.  If ellipsoid code is not found,
 *  an exception is thrown.
 *
 *    code     : 2-letter ellipsoid code.                      (input)
 *    index    : Index of the ellipsoid in the ellipsoid table with the
 *                  specified code                             (output)
 *
 */

  _ellipsoidLibraryImplementation->ellipsoidIndex( code, index );
} 


void EllipsoidLibrary::getEllipsoidInfo( const long index, char *code, char *name  )
{ 
/*
 *  The Function getEllipsoidInfo returns the 2-letter code and name of the
 *  ellipsoid in the ellipsoid table with the specified index.  If index is
 *  invalid, an exception is thrown.
 *
 *    index    : Index of a given ellipsoid in the ellipsoid table (input)
 *    code     : 2-letter ellipsoid code.                          (output)
 *    name    : Name of the ellipsoid referencd by index           (output)
 *
 */

  _ellipsoidLibraryImplementation->ellipsoidCode( index, code  );
  _ellipsoidLibraryImplementation->ellipsoidName( index, name  );
} 


void EllipsoidLibrary::getEllipsoidParameters( const long index, double *a, double *f )
{ 
/*
 *  The function getEllipsoidParameters returns the semi-major axis and flattening
 *  for the ellipsoid with the specified index.  If index is invalid,
 *  an exception is thrown.
 *
 *    index    : Index of a given ellipsoid in the ellipsoid table (input)
 *    a        : Semi-major axis, in meters, of ellipsoid          (output)
 *    f        : Flattening of ellipsoid.                          (output)
 *
 */

  _ellipsoidLibraryImplementation->ellipsoidParameters( index, a, f  );
} 

// CLASSIFICATION: UNCLASSIFIED
