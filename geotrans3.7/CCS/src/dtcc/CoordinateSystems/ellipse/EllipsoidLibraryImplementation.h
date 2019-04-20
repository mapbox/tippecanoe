// CLASSIFICATION: UNCLASSIFIED

#ifndef EllipsoidLibraryImplementation_H
#define EllipsoidLibraryImplementation_H

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


#include <vector>


namespace MSP
{
  class CCSThreadMutex;
  namespace CCS
  {
    class Ellipsoid;
    class DatumLibraryImplementation;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class EllipsoidLibraryImplementation
    {
    friend class EllipsoidLibraryImplementationCleaner;

    public:

      /* 
       * The function getInstance returns an instance of the EllipsoidLibraryImplementation
       */

      static EllipsoidLibraryImplementation* getInstance();


      /*
       * The function removeInstance removes this EllipsoidLibraryImplementation instance from the
       * total number of instances. 
       */

      static void removeInstance();


 	    ~EllipsoidLibraryImplementation( void );


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

      void defineEllipsoid( const char* code, const char* name, double semiMajorAxis, double flattening );


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

      void removeEllipsoid( const char* Code );


      /*
       * The function ellipsoidCount returns the number of ellipsoids in the
       * ellipsoid table.  If the ellipsoid table has not been initialized,
       * an exception is thrown.
       *
       *   count    : The number of ellipsoids in the ellipsoid table. (output)
       *
       */

      void ellipsoidCount ( long *count );


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

      void ellipsoidIndex( const char* code, long* index );


      /*
       *  The Function ellipsoidCode returns the 2-letter code for the
       *  ellipsoid in the ellipsoid table with the specified index.  If index is
       *  invalid, an exception is thrown.
       *
       *    index    : Index of a given ellipsoid in the ellipsoid table (input)
       *    code     : 2-letter ellipsoid code.                          (output)
       *
       */

      void ellipsoidCode( const long index, char *code );


      /*
       *  The Function ellipsoidName returns the name of the ellipsoid in
       *  the ellipsoid table with the specified index.  If index is invalid,
       *  an exception is thrown.
       *
       *    index   : Index of a given ellipsoid.in the ellipsoid table with the
       *                 specified index                             (input)
       *    name    : Name of the ellipsoid referencd by index       (output)
       *
       */

      void ellipsoidName( const long index, char *name );


      /*
       *  The function ellipsoidParameters returns the semi-major axis and flattening
       *  for the ellipsoid with the specified index.  If index is invalid,
       *  an exception is thrown.
       *
       *    index    : Index of a given ellipsoid in the ellipsoid table (input)
       *    a        : Semi-major axis, in meters, of ellipsoid          (output)
       *    f        : Flattening of ellipsoid.                          (output)
       *
       */

      void ellipsoidParameters( const long index, double *a, double *f );


      /*
       *  The function ellipsoidEccentricity2 returns the square of the
       *  eccentricity for the ellipsoid with the specified index.  If index is
       *  invalid, an exception is thrown.
       *
       *    index                : Index of a given ellipsoid in the ellipsoid table (input)
       *    eccentricitySquared  : Square of eccentricity of ellipsoid               (output)
       *
       */

      void ellipsoidEccentricity2( const long index, double *eccentricitySquared );


      /*
       *  The function ellipsoidUserDefined returns 1 if the ellipsoid is user
       *  defined.  Otherwise, 0 is returned.  If index is invalid an
       *  exception is thrown.
       *
       *    index    : Index of a given ellipsoid in the ellipsoid table (input)
       *    result   : Indicates whether specified ellipsoid is user defined (1)
       *               or not (0)                                        (output)
       *
       */

      void ellipsoidUserDefined( const long index, long *result );


      /*
       *  The function setDatumLibraryImplementation sets the datum library information
       *  which is needed to ensure a user defined ellipsoid is not in use before being deleted.
       *
       *   __datumLibraryImplementation  : Datum library implementation      (input)
       *
       */

      void setDatumLibraryImplementation( DatumLibraryImplementation* __datumLibraryImplementation );


    protected:

      /*
       * The constructor creates an empty list to store the ellipsoid data from ellips.dat,
       * which is used to build the ellipsoid table.  
       */

	    EllipsoidLibraryImplementation();


      EllipsoidLibraryImplementation( const EllipsoidLibraryImplementation &e );


      EllipsoidLibraryImplementation& operator=( const EllipsoidLibraryImplementation &e );



   private:

      static CCSThreadMutex mutex;
      static EllipsoidLibraryImplementation* instance;
      static int instanceCount;

      std::vector<Ellipsoid*> ellipsoidList;


      DatumLibraryImplementation* _datumLibraryImplementation;

      /*
       * The function loadEllipsoids reads ellipsoid data from ellips.dat
       * and builds the ellipsoid table from it.  If an error occurs, 
       * an exception is thrown.
       */

      void loadEllipsoids();

      
      /*
       * Delete the singleton.
       */

      static void deleteInstance();
    };
  }
}
	
#endif 

// CLASSIFICATION: UNCLASSIFIED
