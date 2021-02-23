// CLASSIFICATION: UNCLASSIFIED
#ifndef CoordinateConversionService_H
#define CoordinateConversionService_H

/***************************************************************************/
/* RSC IDENTIFIER: Coordinate Conversion Service
 *
 * ABSTRACT
 *
 *    This component is the coordinate conversion service for the MSPCCS
 *    application.  It provides an external input interface that supports the
 *    MSPCCS GUI (Java) and the MSPCCS file processing
 *    component.
 *
 *    This component depends on the DT&CC modules:  DATUM, ELLIPSOID,
 *    GEOCENTRIC, GEOREF, MERCATOR, TRANSVERSE MERCATOR, UTM, MGRS, USNG, POLAR
 *    STEREOGRAPHIC, UPS, LAMBERT_1, LAMBERT_2, ALBERS, AZIMUTHAL EQUIDISTANT,
 *    BONNE, BRITISH NATIONAL GRID,  CASSINI, CYLINDRICAL EQUAL AREA,ECKERT4,
 *    ECKERT6, EQUIDISTANT CYLINDRICAL, GARS, GNOMONIC, LOCAL CARTESIAN,
 *    MILLER, MOLLWEIDE, NEYS, NEW ZEALAND MAP GRID, OBLIQUE MERCATOR,
 *    ORTHOGRAPHIC, POLYCONIC, SINUSOIDAL, STEREOGRAPHIC,
 *    TRANSVERSE CYLINDRICAL EQUAL AREA, VAN DER GRINTEN, AND WEB MERCATOR.
 *
 *
 * ERROR HANDLING
 *
 *    This component checks for error codes returned by the DT&CC modules.
 *    If an error code is returned, it is combined with the current
 *    error code using the bitwise or.  This combining allows multiple error
 *    codes to be returned. The possible error codes are listed below.
 *
 *
 * REUSE NOTES
 *
 *    Coordinate Conversion Service is intended for reuse by other applications
 *    that require coordinate conversions between multiple coordinate systems
 *    and/or datum transformations between multiple datums.
 *
 *
 * REFERENCES
 *
 *    Further information on Coordinate Conversion Service can be found in the
 *    Coordinate Conversion Service Reuse Manual.
 *
 *    Coordinate Conversion Service originated from :
 *            U.S. Army Topographic Engineering Center
 *            Digital Concepts & Analysis Center
 *            7701 Telegraph Road
 *            Alexandria, VA  22310-3864
 *
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    Coordinate Conversion Service has no restrictions.
 *
 * MODIFICATIONS
 *
 *   Date        Description
 *   ----        -----------
 *   04-22-97    Original Code
 *   09-30-99    Added support for 15 new projections
 *   05-30-00    Added support for 2 new projections
 *   06-30-00    Added support for 1 new projection
 *   09-30-00    Added support for 4 new projections
 *   03-24-05    Added support for Lambert Conformal Conic (1 Standard Parallel)
 *   08-17-05    Changed Lambert_Conformal_Conic to Lambert_Conformal_Conic_2
 *   06-06-06    Added support for USNG
 *   07-17-06    Added support for GARS
 *   03-17-07    Original C++ Code
 *   07-20-10    NGL BAEts27152 Updated getServiceVersion to return an int
 */


#include <vector>
#include "CoordinateType.h"
#include "Precision.h"
#include "SourceOrTarget.h"
#include "CoordinateTuple.h"


#ifdef WIN32
#ifdef MSP_CCS_EXPORTS
#define MSP_CCS __declspec(dllexport)
#elif defined (MSP_CCS_IMPORTS)
#define MSP_CCS __declspec(dllimport)
#else
#define MSP_CCS
#endif
#endif


namespace MSP
{
  class CCSThreadMutex;
  namespace CCS
  {
    class EllipsoidLibrary;
    class EllipsoidLibraryImplementation;
    class DatumLibrary;
    class DatumLibraryImplementation;
    class GeoidLibrary;
    class Accuracy;
    class CoordinateSystemParameters;
    class CoordinateTuple;
    class MapProjection3Parameters;
    class MapProjection4Parameters;
    class MapProjection5Parameters;
    class MapProjection6Parameters;
    class EquidistantCylindricalParameters;
    class GeodeticParameters;
    class LocalCartesianParameters;
    class MercatorStandardParallelParameters;
    class MercatorScaleFactorParameters;
    class NeysParameters;
    class ObliqueMercatorParameters;
    class PolarStereographicStandardParallelParameters;
    class PolarStereographicScaleFactorParameters;
    class UTMParameters;
    class MapProjectionCoordinates;
    class BNGCoordinates;
    class CartesianCoordinates;
    class GeodeticCoordinates;
    class GEOREFCoordinates;
    class GARSCoordinates;
    class MGRSorUSNGCoordinates;
    class UPSCoordinates;
    class UTMCoordinates;
    class CoordinateSystem;


     /*
      *                  DEFINES
      */

     /* Symbolic constants */
     const int NUMBER_COORD_SYS = 38;      // Number of coordinate systems
     const int COORD_SYS_CODE_LENGTH = 3;  // Length of coordinate system codes (including null)
     const int COORD_SYS_NAME_LENGTH = 50; // Max length of coordinate system names (including null)
     const int DATUM_CODE_LENGTH = 7;      // Length of datum codes (including null)
     const int DATUM_NAME_LENGTH = 33;     // Max length of datum names (including null)
     const int ELLIPSOID_CODE_LENGTH = 3;  // Length of ellipsoid codes (including null)
     const int ELLIPSOID_NAME_LENGTH = 30; // Max length of ellipsoid names (including null)
     const int CONVERT_MSG_LENGTH = 2048;  // Max length of coordinate conversion status message
     const int RETURN_MSG_LENGTH = 256;    // Max length of return code status message



     /**
      * Main Interface for Coordinate Conversion Service
      */

#ifdef WIN32
     class MSP_CCS CoordinateConversionService
#else
     class CoordinateConversionService
#endif
     {
     public:

        /**
         *  The constructor sets the initial state of the coordinate conversion
         * service in preparation for coordinate conversion and/or datum
         * transformation operations.
         *
         * @param[in] sourceDatumCode - standard 5-letter datum code
         * @param[in] sourceParameters - input coordinate system
         * @param[in] targetDatumCode - standard 5-letter datum code
         * @param[in] targetParameters - output coordinate system
         *
         */
        CoordinateConversionService(
           const char*                           sourceDatumCode,
           MSP::CCS::CoordinateSystemParameters* sourceParameters,
           const char*                           targetDatumCode,
           MSP::CCS::CoordinateSystemParameters* targetParameters );

        /**
         *  Copy constructor
         *
         *  @param[in] - Instance of Coordinate Conversion Service
         */
        CoordinateConversionService( const CoordinateConversionService &ccs );

        /**
         *  Destructor
         */
        ~CoordinateConversionService( void );

        /**
         *  Operator =
         *
         * @param[in] - Instance of Coordinate Conversion Service
         */
        CoordinateConversionService &operator=(
           const CoordinateConversionService &ccs );


      /**
      *  The function convertSourceToTarget converts the current source
      *  coordinates in the coordinate system defined by the current source
      *  coordinate system parameters and source datum, into target coordinates
      *  in the coordinate system defined by the target coordinate
      *  system parameters and target datum.
      *
      * @param[in] sourceCoordinates - Coordinates of the source coordinate system to be converted
      * @param[in] sourceAccuracy - Source circular, linear and spherical errors 
      * @param[out] targetCoordinates - Converted coordinates of the target coordinate system 
      * @param[out] targetAccuracy - Target circular, linear and spherical errors 
      * 
      */
      void convertSourceToTarget(
         CoordinateTuple* sourceCoordinates,
         Accuracy*        sourceAccuracy,
         CoordinateTuple& targetCoordinates,
         Accuracy&        targetAccuracy );


        /**
         *  The function convertTargetToSource converts the current target
         *  coordinates in the coordinate system defined by the current target
         *  coordinate system parameters and target datum,
         *  into source coordinates in the coordinate system defined by the 
         * source coordinate system parameters and source datum.
         *
         * @param[in] targetCoordinates - Converted coordinates of the target coordinate system 
         * @param[in] targetAccuracy - Target circular, linear and spherical errors 
         * @param[out] sourceCoordinates - Coordinates of the source coordinate system to be converted
         * @param[out] sourceAccuracy - Source circular, linear and spherical errors 
         * 
         */
        void convertTargetToSource(
           CoordinateTuple* targetCoordinates,
           Accuracy*        targetAccuracy,
           CoordinateTuple& sourceCoordinates,
           Accuracy&        sourceAccurac );
    
  
      /**
      *  The function convertSourceToTargetCollection will convert a list of
      *  source coordinates to a list of target coordinates in a single step.
      *
      * @param[in] sourceCoordinates - Coordinates of the source coordinate system to be converted
      * @param[in] sourceAccuracy - Source circular, linear and spherical errors 
      * @param[out] targetCoordinates - Converted coordinates of the target coordinate system 
      * @param[out] targetAccuracy - Target circular, linear and spherical errors 
      * 
      */
      void convertSourceToTargetCollection(
         const std::vector<MSP::CCS::CoordinateTuple*>& sourceCoordinates,
         const std::vector<MSP::CCS::Accuracy*>&        sourceAccuracy,
         std::vector<MSP::CCS::CoordinateTuple*>&       targetCoordinates,
         std::vector<MSP::CCS::Accuracy*>&              targetAccuracy );


        /**
         *  The function convertTargetToSourceCollection will convert a list
         *  of target coordinates to a list of source coordinates
         *  in a single step.
         *
         * @param[in] targetCoordinates - Converted coordinates of the target coordinate system 
         * @param[in] targetAccuracy - Target circular, linear and spherical errors 
         * @param[out] sourceCoordinates - Coordinates of the source coordinate system to be converted
         * @param[out] sourceAccuracy - Source circular, linear and spherical errors 
         * 
         */
        void convertTargetToSourceCollection(
           const std::vector<MSP::CCS::CoordinateTuple*>& targetCoordinates,
           const std::vector<MSP::CCS::Accuracy*>&        targetAccuracy,
           std::vector<MSP::CCS::CoordinateTuple*>&       sourceCoordinates,
           std::vector<MSP::CCS::Accuracy*>&              sourceAccuracy );


        /*
         * The function getEllipsoidLibrary returns the ellipsoid library 
         * which provides access to ellipsoidparameter information.
         * 
         */

        EllipsoidLibrary* getEllipsoidLibrary();


        /*
         * The function getDatumLibrary returns the datum library 
         * which provides access to datum transformation and parameter
         * information.
         * 
         * @return Datum library
         */
        DatumLibrary* getDatumLibrary();


        /*
         * The function getServiceVersion returns current service version.
         *
         * @return Service version
         */
        int getServiceVersion();


        /*
         *  The function getDatum returns the index of the current datum.
         *
         * @param[in] direction - indicates whether the datum is to be used for input or output
         * @return Index identifying the index of the current datum
         */

        const char* getDatum( const SourceOrTarget::Enum direction ) const;


        /*
         *  The function getCoordinateSystem returns the current
         *  coordinate system type.
         *
         *  @param[in] direction - Indicates whether the coordinate system is to
         *                         be used for input or output
         *  @return Parameters identifies current coordinate system type
         */

        MSP::CCS::CoordinateSystemParameters* getCoordinateSystem(
           const SourceOrTarget::Enum direction ) const;


     private:

        static CCSThreadMutex mutex;

        /* Object used to keep track of the number of ccs objects */
        struct CCSData
        {
           int refCount;
           
           EllipsoidLibrary*               ellipsoidLibrary;
           EllipsoidLibraryImplementation* ellipsoidLibraryImplementation;
           DatumLibrary*                   datumLibrary;
           DatumLibraryImplementation*     datumLibraryImplementation;
           GeoidLibrary*                   geoidLibrary;

           CCSData();
           ~CCSData();
        };

        CCSData* ccsData;

        EllipsoidLibraryImplementation* ellipsoidLibraryImplementation;
        DatumLibraryImplementation*     datumLibraryImplementation;
        GeoidLibrary*                   geoidLibrary;

//compiler bug on solaris 8. Parameters needs to be public
#ifdef SOLARIS
     public:
#endif

        /* Coordinate System Definition with Multiple Variants */
        union Parameters
        {
           CoordinateSystemParameters*        coordinateSystemParameters;
           MapProjection3Parameters*          mapProjection3Parameters;
           MapProjection4Parameters*          mapProjection4Parameters;
           MapProjection5Parameters*          mapProjection5Parameters;
           MapProjection6Parameters*          mapProjection6Parameters;
           EquidistantCylindricalParameters*  equidistantCylindricalParameters;
           GeodeticParameters*                geodeticParameters;
           LocalCartesianParameters*          localCartesianParameters;
           MercatorStandardParallelParameters* mercatorStandardParallelParameters;
           MercatorScaleFactorParameters*     mercatorScaleFactorParameters;
           NeysParameters*                    neysParameters;
           ObliqueMercatorParameters*         obliqueMercatorParameters;
           PolarStereographicStandardParallelParameters* polarStereographicStandardParallelParameters;
           PolarStereographicScaleFactorParameters* polarStereographicScaleFactorParameters;
           UTMParameters*                     utmParameters;
        };

        /* Coordinate Tuple Definition with Multiple Variants */
        union Coordinates
        {
           MapProjectionCoordinates*     mapProjectionCoordinates;
           BNGCoordinates*               bngCoordinates;
           CartesianCoordinates*         cartesianCoordinates;
           GeodeticCoordinates*          geodeticCoordinates;
           GEOREFCoordinates*            georefCoordinates;
           GARSCoordinates*              garsCoordinates;
           MGRSorUSNGCoordinates*        mgrsOrUSNGCoordinates;
           UPSCoordinates*               upsCoordinates;
           UTMCoordinates*               utmCoordinates;
        };

        struct Coordinate_System_Row
        {
           char Name[COORD_SYS_NAME_LENGTH];
           char Code[COORD_SYS_CODE_LENGTH];
           CoordinateType::Enum coordinateSystem;
        };


        /* Coordinate Conversion Service State Definition */
        struct Coordinate_State_Row
        {
           char datumCode[DATUM_CODE_LENGTH]; // currently specified datum code
           long datumIndex;                   // currently specified datum index
           CoordinateType::Enum coordinateType; // current CS type
           Parameters parameters;               // current CS parameters
           CoordinateSystem* coordinateSystem;  // current CS
      };

        /* coordinateSystemState[x] is set up as follows:
           c = Number of IO states (Source, Target, etc.) */
        Coordinate_State_Row coordinateSystemState[2];

        /* Local State Variables */
        Coordinate_System_Row Coordinate_System_Table[NUMBER_COORD_SYS];

        long WGS84_datum_index;

        /*
         *  The function setDataLibraries sets the initial state of the engine
         *  in preparation for coordinate conversion and/or datum transformation
         *  operations.
         */
        void setDataLibraries();


        /*
         *  The function initCoordinateSystemState initializes
         *  coordinateSystemState.
         *
         *  direction  : Indicates whether the coordinate system is to be used
         *               for  source or target               (input)
         */

        void initCoordinateSystemState( const SourceOrTarget::Enum direction );


        /*
         *  The function deleteCoordinateSystem frees memory of
         *  coordinateSystemState.
         *
         *  direction  : Indicates whether the coordinate system is to be used
         *               for  source or target                (input)
         */

        void deleteCoordinateSystem( const SourceOrTarget::Enum direction );


        /*
         *  The function copyParameters uses the input parameters to set the
         *  value of the current parameters.
         *
         *  direction       : Indicates whether the coordinate system is to
         *                    be used for source or target             (input)
         *  coordinateType  : Coordinate system type                   (input)
         *  parameters      : Coordinate system parameters to copy     (input)
         */

        void copyParameters(
           SourceOrTarget::Enum direction,
           CoordinateType::Enum coordinateType,
           Parameters           parameters );
        
        /*
         *  The function setDatum sets the datum to the
         *  datum corresponding to the specified index.
         *
         *  direction  : Indicates whether the datum is to be used for input or
         *               output                                         (input)
         *  index      : Identifies the index of the datum to be used   (input)
         */

        void setDatum(
           const SourceOrTarget::Enum direction,
           const char*                index );


        /*
         *  The function setCoordinateSystem sets the coordinate system
         *  to the specified coordinate system type.
         *
         *  direction  : Indicates whether the coordinate system is to be
         *               used for input or output                   (input)
         *  parameters : Coordinate system parameters to be used    (input)
         */

        void setCoordinateSystem(
           const SourceOrTarget::Enum            direction,
           MSP::CCS::CoordinateSystemParameters* parameters );

        /*
         *  The function setParameters calls the setParameters function
         *  of the source or target coordinate system.
         *
         *  direction  : Indicates whether the coordinate system is to be
         *               used for source or target                  (input)
         */

        void setParameters( const SourceOrTarget::Enum direction );


        /*
         *  The function convert converts the current input coordinates
         *  in the coordinate system defined by the current input coordinate
         *  system parameters and input datum, into output coordinates in
         *  the coordinate system defined by the output coordinate
         *  system parameters and output datum.
         *
         *  sourceDirection: Indicates which set of coordinates and parameters to use as the source (input)
         *  targetDirection: Indicates which set of coordinates and parameters to use as the target (input)
         */

        void convert(
           SourceOrTarget::Enum sourceDirection,
           SourceOrTarget::Enum targetDirection,
           CoordinateTuple*     sourceCoordinates,
           Accuracy*            sourceAccuracy,
           CoordinateTuple&     targetCoordinates,
           Accuracy&            targetAccuracy );


        GeodeticCoordinates* convertSourceToGeodetic(
           SourceOrTarget::Enum sourceDirection,
           CoordinateTuple*     sourceCoordinates,
           char*                sourceWarningMessage );


      void convertGeodeticToTarget(
         SourceOrTarget::Enum targetDirection,
         GeodeticCoordinates* _shiftedGeodetic,
         CoordinateTuple&     targetCoordinates,
         char*                targetWarningMessage );


        /*
         *  The function convertCollection will convert a list of source
         *  coordinates to a list of target coordinates in a single step.
         *
         *  sourceCoordinatesCollection  : Coordinates of the source coordinate system to be converted   (input)
         *  sourceAccuracyCollection     : Source circular, linear and spherical errors                  (input)
         *  targetCoordinatesCollection  : Converted coordinates of the target coordinate system         (output)
         *  targetAccuracyCollection     : Target circular, linear and spherical errors                  (output)
         */

        void convertCollection(
           const std::vector<MSP::CCS::CoordinateTuple*>& sourceCoordinatesCollection,
           const std::vector<MSP::CCS::Accuracy*>& sourceAccuracyCollection,
           std::vector<MSP::CCS::CoordinateTuple*>& targetCoordinatesCollection,
           std::vector<MSP::CCS::Accuracy*>& targetAccuracyCollection );
     };
  }
}

#endif


// CLASSIFICATION: UNCLASSIFIED
