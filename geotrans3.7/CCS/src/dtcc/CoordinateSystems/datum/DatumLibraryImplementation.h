// CLASSIFICATION: UNCLASSIFIED

#ifndef DatumLibraryImplementation_H
#define DatumLibraryImplementation_H

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
 *    Date         Description
 *    ----         -----------
 *    03/30/97     Original Code
 *    05/28/99     Added user-definable datums (for JMTK)
 *                 Added datum domain of validity checking (for JMTK)
 *                 Added datum shift accuracy calculation (for JMTK)
 *    06/27/06     Moved data files to data directory
 *    03-14-07     Original C++ Code
 *    05/26/10     S. Gillis, BAEts26674, Added Validate Datum to the API
 *                 in MSP Geotrans 3.0
 *    08/13/12     S. Gillis, MSP_00029654, Added lat/lon to define7ParamDatum
 */


#include <vector>
#include "DatumType.h"
#include "Precision.h"
#include "DtccApi.h"


namespace MSP
{
  class CCSThreadMutex;
  namespace CCS
  {
    class Accuracy;
    class Datum;
    class EllipsoidLibraryImplementation;
    class CartesianCoordinates;
    class GeodeticCoordinates;


    class MSP_DTCC_API DatumLibraryImplementation
    {
    friend class DatumLibraryImplementationCleaner;

    public:

      /* The function getInstance returns an instance of the DatumLibraryImplementation
       */

      static DatumLibraryImplementation* getInstance();


      /*
       * The function removeInstance removes this DatumLibraryImplementation instance from the
       * total number of instances. 
       */

      static void removeInstance();


	    ~DatumLibraryImplementation( void );


      /*
       * The function define3ParamDatum creates a new local (3-parameter) datum with the
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

      void define3ParamDatum( const char *code, const char *name, const char *ellipsoidCode, 
                              double deltaX, double deltaY, double deltaZ,
                              double sigmaX, double sigmaY, double sigmaZ, 
                              double westLongitude, double eastLongitude, double southLatitude, double northLatitude );


      /*
       * The function define7ParamDatum creates a new local (7-parameter) datum with the
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

      void define7ParamDatum( const char *code, const char *name, const char *ellipsoidCode, 
                              double deltaX, double deltaY, double deltaZ,
                              double rotationX, double rotationY, double rotationZ, 
                              double scale, double westLongitude, 
                              double eastLongitude, double southLatitude, 
                              double northLatitude);

      /*
       * The function removeDatum deletes a local (3-parameter) datum with the
       * specified code.  If the datum table has not been initialized or a new
       * version of the 3-param.dat file cannot be created, an exception is thrown.
       * Note that the indexes of all datums
       * in the datum table may be changed by this function.
       *
       *   code           : 5-letter datum code.                      (input)
       *
       */

      void removeDatum( const char* code );


      /*
       *  The function datumCount returns the number of Datums in the table
       *  if the table was initialized without error.
       *
       *  count        : number of datums in the datum table     (output)
       */

      void datumCount( long *count );


      /*
       *  The function datumIndex returns the index of the datum with the
       *  specified code.
       *
       *  code    : The datum code being searched for.                    (input)
       *  index   : The index of the datum in the table with the          (output)
       *              specified code.
       */

      void datumIndex( const char *code, long *index );


      /*
       *  The function datumCode returns the 5-letter code of the datum
       *  referenced by index.
       *
       *  index   : The index of a given datum in the datum table.        (input)
       *  code    : The datum Code of the datum referenced by Index.      (output)
       */

      void datumCode( const long index, char *code );


      /*
       *  The function datumName returns the name of the datum referenced by
       *  index.
       *
       *  index   : The index of a given datum in the datum table.        (input)
       *  name    : The datum Name of the datum referenced by Index.      (output)
       */

      void datumName( const long index, char *name );


      /*
       *  The function datumEllipsoidCode returns the 2-letter ellipsoid code
       *  for the ellipsoid associated with the datum referenced by index.
       *
       *  index   : The index of a given datum in the datum table.          (input)
       *  code    : The ellipsoid code for the ellipsoid associated with    (output)
       *               the datum referenced by index.
       */

      void datumEllipsoidCode( const long index, char *code );


      /*
       *   The function datumStandardErrors returns the standard errors in X,Y, & Z
       *   for the datum referenced by index.
       *
       *    index      : The index of a given datum in the datum table   (input)
       *    sigma_X    : Standard error in X in meters                   (output)
       *    sigma_Y    : Standard error in Y in meters                   (output)
       *    sigma_Z    : Standard error in Z in meters                   (output)
       */

      void datumStandardErrors( const long index, double *sigmaX, double *sigmaY, double *sigmaZ );


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

      void datumSevenParameters( const long index, double *rotationX, double *rotationY, double *rotationZ, double *scaleFactor);


      /*
       *   The function datumTranslationValues returns the translation values
       *   for the datum referenced by index.
       *
       *    index       : The index of a given datum in the datum table. (input)
       *    deltaX      : X translation in meters                       (output)
       *    deltaY      : Y translation in meters                       (output)
       *    deltaZ      : Z translation in meters                       (output)
       */

      void datumTranslationValues(
         const long index,
         double *deltaX,
         double *deltaY,
         double *deltaZ );


      /*
       *  The function datumShiftError returns the 90% horizontal (circular),
       *  vertical (linear), and spherical errors for a shift from the
       *  specified source datum to the specified destination datum at
       *  the specified location.
       *
       *  sourceIndex      : Index of source datum                       (input)
       *  targetIndex      : Index of destination datum                  (input)
       *  longitude        : Longitude of point being converted (radians)(input)
       *  latitude         : Latitude of point being converted (radians) (input)
       *  sourceAccuracy   : Accuracy of the source coordinate           (input)
       *  precision        : Precision of the source coordinate          (input)
       */

      Accuracy* datumShiftError(
         const long      sourceIndex,
         const long      targetIndex, 
         double          longitude,
         double          latitude,
         Accuracy*       sourceAccuracy,
         Precision::Enum precision );

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

      void datumUserDefined( const long index, long *result );


      /*
       *  The function datumUsesEllipsoid returns 1 if the ellipsoid is in use by a
       *  user defined datum.  Otherwise, 0 is returned.
       *
       *  ellipsoidCode    : The ellipsoid code being searched for.    (input)
       */

      bool datumUsesEllipsoid( const char *ellipsoidCode );


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

      void datumValidRectangle( const long index, double *westLongitude, double *eastLongitude, double *southLatitude, double *northLatitude );


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

      CartesianCoordinates* geocentricDatumShift( const long sourceIndex, const double sourceX, const double sourceY, const double sourceZ,
                                 const long targetIndex );


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

      CartesianCoordinates* geocentricShiftFromWGS84( const double WGS84X, const double WGS84Y, const double WGS84Z, const long targetIndex );


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

      CartesianCoordinates* geocentricShiftToWGS84( const long sourceIndex, const double sourceX, const double sourceY, const double sourceZ );


      /*
       *  The function geodeticDatumShift shifts geodetic coordinates (latitude, longitude in radians
       *  and height in meters) relative to the source datum to geodetic coordinates
       *  (latitude, longitude in radians and height in meters) relative to the
       *  destination datum.
       *
       *  sourceIndex     : Index of source datum                               (input)
       *  sourceLongitude : Longitude in radians relative to source datum       (input)
       *  sourceLatitude  : Latitude in radians relative to source datum        (input)
       *  sourceHeight    : Height in meters relative to source datum           (input)
       *  targetIndex     : Index of destination datum                          (input)
       *  targetLongitude : Longitude in radians relative to destination datum  (output)
       *  targetLatitude  : Latitude in radians relative to destination datum   (output)
       *  targetHeight    : Height in meters relative to destination datum      (output)
       */

      GeodeticCoordinates* geodeticDatumShift( const long sourceIndex, const GeodeticCoordinates* sourceCoordinates,
                               const long targetIndex );


      /*
       *  The function geodeticShiftFromWGS84 shifts geodetic coordinates relative to WGS84
       *  to geodetic coordinates relative to a given local datum.
       *
       *    WGS84Longitude  : Longitude in radians relative to WGS84             (input)
       *    WGS84Latitude   : Latitude in radians relative to WGS84              (input)
       *    WGS84Height     : Height in meters  relative to WGS84                (input)
       *    targetIndex     : Index of destination datum                         (input)
       *    targetLongitude : Longitude in radians relative to destination datum (output)
       *    targetLatitude  : Latitude in radians relative to destination datum  (output)
       *    targetHeight    : Height in meters relative to destination datum     (output)
       *
       */

      GeodeticCoordinates* geodeticShiftFromWGS84( const GeodeticCoordinates* sourceCoordinates,
                                   const long targetIndex );


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

      GeodeticCoordinates* geodeticShiftToWGS84( const long sourceIndex,  const GeodeticCoordinates* sourceCoordinates );


      /*
       *  The function retrieveDatumType returns the type of the datum referenced by
       *  index.
       *
       *  index     : The index of a given datum in the datum table.   (input)
       *  datumType : The type of datum referenced by index.           (output)
       *
       */

      void retrieveDatumType( const long index, DatumType::Enum *datumType );


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

      void validDatum( const long index, double longitude, double latitude, long *result );


      /*
       *  The function setEllipsoidLibrary sets the ellipsoid library information
       *  which is needed to create datums and calculate datum shifts.
       *
       *   __ellipsoidLibrary  : Ellipsoid library      (input)
       *
       */

      void setEllipsoidLibraryImplementation( EllipsoidLibraryImplementation* __ellipsoidLibraryImplementation );


    protected:

      /*
       * The constructor creates an empty list to store the datum information
       *  contained in two external files, 3_param.dat and 7_param.dat.  
       */

	    DatumLibraryImplementation();


      DatumLibraryImplementation( const DatumLibraryImplementation &d );


      DatumLibraryImplementation& operator=( const DatumLibraryImplementation &d );


    private:

      static MSP::CCSThreadMutex mutex;
      static DatumLibraryImplementation* instance;
      static int instanceCount;

      std::vector<Datum*> datumList;

      EllipsoidLibraryImplementation* _ellipsoidLibraryImplementation;

      long datum3ParamCount;
      long datum7ParamCount;


      /*
       * The function loadDatums creates the datum table from two external
       * files.  If an error occurs, the initialization stops and an error code is
       * returned.  This function must be called before any of the other functions
       * in this component.
       */

      void loadDatums();

      
       /*
        *  The function write3ParamFile writes the 3 parameter datums in the datum list
        *  to the 3_param.dat file.
        */

      void write3ParamFile();


       /*
        *  The function write7ParamFile writes the 7 parameter datums in the datum list
        *  to the 7_param.dat file.
        */

      void write7ParamFile();

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

      GeodeticCoordinates* geodeticShiftWGS84ToWGS72( const double WGS84Longitude, const double WGS84Latitude, const double WGS84Height );


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

      GeodeticCoordinates* geodeticShiftWGS72ToWGS84( const double WGS72Longitude, const double WGS72Latitude, const double WGS72Height );


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

      CartesianCoordinates* geocentricShiftWGS84ToWGS72( const double X_WGS84, const double Y_WGS84, const double Z_WGS84 );


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

      CartesianCoordinates* geocentricShiftWGS72ToWGS84( const double X, const double Y, const double Z );

      
      /*
       * Delete the singleton.
       */

      static void deleteInstance();
    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
