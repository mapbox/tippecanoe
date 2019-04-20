// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: Datum Library
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
 *    11/20/08          Original Code
 *    05/26/10          S. Gillis, BAEts26674, Added Validate Datum API to the
 *                      in MSP Geotrans 3.0
 *    06/04/10          S. Gillis, BAEts26676, Fixed the error always returned 
 *                      when calling CCS API getDatumParamters
 *    08/13/12          S. Gillis, MSP_00029654, Added lat/lon to define7ParamDatum
 */

/***************************************************************************/
/*
 *                               INCLUDES
 */

#include "DatumLibrary.h"
#include "DatumLibraryImplementation.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    DatumLibrary.h    - accesses datum information
 *    DatumLibraryImplementation.h    - for prototype error ehecking and error codes
 */


using namespace MSP::CCS;


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

DatumLibrary::DatumLibrary( DatumLibraryImplementation* __datumLibraryImplementation )
{
/*
 * The constructor creates an empty list to store the datum information
 *  contained in two external files, 3_param.dat and 7_param.dat.  
 */

  _datumLibraryImplementation = __datumLibraryImplementation;
}


DatumLibrary::DatumLibrary( const DatumLibrary &dl )
{
  _datumLibraryImplementation = dl._datumLibraryImplementation;
}


DatumLibrary::~DatumLibrary()
{
  _datumLibraryImplementation = 0;
}


DatumLibrary& DatumLibrary::operator=( const DatumLibrary &dl )
{
  if ( &dl == this )
	  return *this;

  _datumLibraryImplementation = dl._datumLibraryImplementation;

  return *this;
}


void DatumLibrary::defineDatum( const int datumType, const char *datumCode, const char *datumName, const char *ellipsoidCode,
                                double deltaX, double deltaY, double deltaZ,
                                double sigmaX, double sigmaY,  double sigmaZ,
                                double westLongitude, double eastLongitude, double southLatitude, double northLatitude,
                                double rotationX, double rotationY,  double rotationZ, double scaleFactor)
{
/* The function defineDatum creates a new local (3 or 7-parameter) datum with the
 * specified code, name, shift values, and standard error values or rotation and scale factor values.  
 * If the datum table has not been initialized, the specified code is already in use,
 * or a new version of the 3-param.dat or 7-param.dat file cannot be created, an 
 * exception is thrown.  Note that the indexes
 * of all datums in the datum table may be changed by this function.
 *
 *   datumType     : Specifies 3 parameter or 7 parameter datum    (input)
 *   datumCode     : 5-letter new datum code.                      (input)
 *   datumName     : Name of the new datum                         (input)
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
 *   rotationX     : X rotation to WGS84 in arc seconds            (input)
 *   rotationY     : Y rotation to WGS84 in arc seconds            (input)
 *   rotationZ     : Z rotation to WGS84 in arc seconds            (input)
 *   scalefactor   : Scale factor                                  (input)
 *
 */

  if( datumType == DatumType::threeParamDatum )
  {
    _datumLibraryImplementation->define3ParamDatum( datumCode, datumName, ellipsoidCode,
                               deltaX, deltaY, deltaZ, sigmaX, sigmaY, sigmaZ,
                               westLongitude, eastLongitude, southLatitude, northLatitude );
  }
  else if( datumType == DatumType::sevenParamDatum )
  {
    _datumLibraryImplementation->define7ParamDatum( datumCode, datumName, ellipsoidCode,
                               deltaX, deltaY, deltaZ, rotationX, rotationY, rotationZ,
                               scaleFactor,westLongitude, eastLongitude, 
                               southLatitude, northLatitude );
  }
}


void DatumLibrary::removeDatum( const char* code )
{ 
/*
 * The function removeDatum deletes a local (3-parameter) datum with the
 * specified code.  If the datum table has not been initialized or a new
 * version of the 3-param.dat file cannot be created, an error code is returned,
 * otherwise DATUM_NO_ERROR is returned.  Note that the indexes of all datums
 * in the datum table may be changed by this function.
 *
 *   code           : 5-letter datum code.                      (input)
 *
 */

  _datumLibraryImplementation->removeDatum( code );
} 


void DatumLibrary::getDatumCount( long *count )
{ 
/*
 *  The function datumCount returns the number of Datums in the table
 *  if the table was initialized without error.
 *
 *  count        : number of datums in the datum table     (output)
 */

  _datumLibraryImplementation->datumCount( count );
} 


void DatumLibrary::getDatumIndex( const char *code, long *index )
{ 
/*
 *  The function datumIndex returns the index of the datum with the
 *  specified code.
 *
 *  code    : The datum code being searched for.                    (input)
 *  index   : The index of the datum in the table with the          (output)
 *              specified code.
 */

  _datumLibraryImplementation->datumIndex( code, index );
} 


void DatumLibrary::getDatumInfo( const long index, char *code, char *name, char *ellipsoidCode )
{ 
/*
 *  The function getDatumInfo returns the 5-letter code, name and
 *  2-letter ellipsoid code of the datum referenced by index.
 *
 *  index            : The index of a given datum in the datum table.        (input)
 *  code             : The datum Code of the datum referenced by Index.      (output)
 *  name             : The datum Name of the datum referenced by Index.      (output)
 *  ellipsoidCode    : The ellipsoid code for the ellipsoid associated with  (output)
 *                     the datum referenced by index.
 */

  _datumLibraryImplementation->datumCode( index, code );
  _datumLibraryImplementation->datumName( index, name );
  _datumLibraryImplementation->datumEllipsoidCode( index, ellipsoidCode );
} 


void DatumLibrary::getDatumParameters( const long index, DatumType::Enum *datumType, double *deltaX, double *deltaY, double *deltaZ,
                                       double *sigmaX, double *sigmaY, double *sigmaZ,
                                       double *westLongitude, double *eastLongitude, double *southLatitude, double *northLatitude,
                                       double *rotationX, double *rotationY, double *rotationZ, double *scaleFactor )
{ 
/*
 *   The function getDatumParameters returns the following datum parameters 
 *   (specified as output parameters below): datumType, deltaX, deltaY, deltaZ,
 *   sigmaX, sigmaY, sigmaZ, westLongitude, eastLongitude, southLatitude, 
 *   northLatitude, rotationX, rotationY, rotationZ, and scaleFactor. 
 * 
 *   sigmaX, sigmaY, and sigmaZ only apply to 3 parameter datum and will be
 *   set to 0 if the datum type is a 7 parameter datum. 
 * 
 *   rotationX, rotationY, rotationZ, and scaleFactor only apply to 7
 *   parameter datum and will be set to 0 if the datum type is a 3 parameter
 *   datum. 
 *
 *   If the datum type is neither a 3 parameter datum nor a 7 parameter datum,
 *   a CoordinateConversionException will be thrown. 
 *
 *   index         : The index of a given datum in the datum table   (input)
 *   datumType     : Specifies datum type                            (output)
 *   deltaX        : X translation to WGS84 in meters                (output)
 *   deltaY        : Y translation to WGS84 in meters                (output)
 *   deltaZ        : Z translation to WGS84 in meters                (output)
 *   sigmaX        : Standard error in X in meters                   (output)
 *   sigmaY        : Standard error in Y in meters                   (output)
 *   sigmaZ        : Standard error in Z in meters                   (output)
 *   westLongitude : Western edge of validity rectangle in radians   (output)
 *   eastLongitude : Eastern edge of validity rectangle in radians   (output)
 *   southLatitude : Southern edge of validity rectangle in radians  (output)
 *   northLatitude : Northern edge of validity rectangle in radians  (output)
 *   rotationX     : X rotation to WGS84 in arc seconds              (output)
 *   rotationY     : Y rotation to WGS84 in arc seconds              (output)
 *   rotationZ     : Z rotation to WGS84 in arc seconds              (output)
 *   scaleFactor   : Scale factor                                    (output)
 */

  _datumLibraryImplementation->retrieveDatumType( index, datumType );
  _datumLibraryImplementation->datumTranslationValues( 
      index, deltaX, deltaY, deltaZ );
  _datumLibraryImplementation->datumValidRectangle( 
      index, westLongitude, eastLongitude, southLatitude, northLatitude );

  //If DatumType is DatumType::threeParamDatum
  if(*datumType == DatumType::threeParamDatum)
  {
    _datumLibraryImplementation->datumStandardErrors( 
        index, sigmaX, sigmaY, sigmaZ );

    //Initialize since they could not be set in datumSevenParameters()
    *rotationX = 0;
    *rotationY = 0;
    *rotationZ = 0;  
    *scaleFactor = 0;
  }
  //If DatumType is DatumType::sevenParamDatum
  else if(*datumType == DatumType::sevenParamDatum)
  {
    _datumLibraryImplementation->datumSevenParameters( 
        index, rotationX, rotationY, rotationZ, scaleFactor );

    //Initialize since they could not be set in datumStandardErrors()
    *sigmaX = 0;
    *sigmaY = 0; 
    *sigmaZ = 0;
  }
  else
  {
    throw CoordinateConversionException( ErrorMessages::datumType );
  }
} 


void DatumLibrary::getDatumValidRectangle( const long index, double *westLongitude, double *eastLongitude, double *southLatitude, double *northLatitude )
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

  _datumLibraryImplementation->datumValidRectangle( index, westLongitude, eastLongitude, southLatitude, northLatitude );
}

void DatumLibrary::validDatum( const long index, double longitude, 
                               double latitude, long *result )
{
/*
 *  The function validDatum checks whether or not the specified location 
 *  is within the validity rectangle for the specified datum.  It returns 
 *  zero if the specified location is NOT within the validity rectangle, 
 *  and returns 1 otherwise.
 *
 *   index     : The index of a given datum in the datum table      (input)
 *   latitude  : Latitude of the location to be checked in radians  (input)
 *   longitude : Longitude of the location to be checked in radians (input)
 *   result    : Indicates whether location is inside (1) or outside (0)
 *               of the validity rectangle of the specified datum   (output)
 */

  _datumLibraryImplementation->validDatum( index, longitude, latitude, result );
}


// CLASSIFICATION: UNCLASSIFIED
