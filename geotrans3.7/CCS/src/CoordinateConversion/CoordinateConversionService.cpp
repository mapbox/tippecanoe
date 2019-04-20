// CLASSIFICATION: UNCLASSIFIED
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
 *    Date      Description
 *    ----      -----------
 *    04-22-97  Original Code
 *    09-30-99  Added support for 15 new projections
 *    05-30-00  Added support for 2 new projections
 *    06-30-00  Added support for 1 new projection
 *    09-30-00  Added support for 4 new projections
 *    03-24-05  Added support for Lambert Conformal Conic (1 Standard Parallel)
 *    08-17-05  Changed Lambert_Conformal_Conic to Lambert_Conformal_Conic_2
 *    06-06-06  Added support for USNG
 *    07-17-06  Added support for GARS
 *    03-17-07  Original C++ Code
 *    05-12-10  S. Gillis, BAEts26542, MSP TS MSL-HAE conversion 
 *              should use CCS 
 *    06-11-10  S. Gillis, BAEts26724, Fixed memory error problem
 *              when MSPCCS_DATA is not set
 *    07-07-10  K.Lam, BAEts27269, Replace C functions in threads.h
 *              with C++ methods in classes CCSThreadMutex
 *    7/20/10   NGL BAEts27152 Updated getServiceVersion to return an int
 *              return 310 for MSP Geotrans 3.1
 *    01/24/11  S. Gillis, BAEts26267, Add support for EGM2008 geoid heights
 *    02/02/11  K.Lam, BAE27554, Fix memory leaks caused by mgrs function calls
 *              convertToUTM, convertToUPS, convertFromUTM, convertFromUPS
 *    02/14/11  S. Gillis, BAE26267, Add support for EGM2008 geoid heights
 *    3/23/11   N. Lundgren BAE28583 Updated for memory leaks in convert method
 *    05/31/11  K. Lam BAE28657  Update service version for MSP 1.1.5
 *    11/18/11  K. Lam MSP_29475  Update service version for MSP 1.2
 *    06/16/14  Krinsky Add Web Mercator
 *    01/16/16  A. Layne MSP_DR30125 added pass of ellipsoid code into transverseMercator
 *              and UTM. 
 */

#include <stdio.h>
#include "CoordinateConversionService.h"
#include "CoordinateSystemParameters.h"
#include "CoordinateTuple.h"
#include "CoordinateType.h"
#include "EllipsoidLibrary.h"
#include "EllipsoidLibraryImplementation.h"
#include "DatumLibrary.h"
#include "DatumLibraryImplementation.h"
#include "GeoidLibrary.h"
#include "Accuracy.h"
#include "EquidistantCylindricalParameters.h"
#include "GeodeticParameters.h"
#include "LocalCartesianParameters.h"
#include "MercatorStandardParallelParameters.h"
#include "MercatorScaleFactorParameters.h"
#include "MapProjection3Parameters.h"
#include "MapProjection4Parameters.h"
#include "MapProjection5Parameters.h"
#include "MapProjection6Parameters.h"
#include "NeysParameters.h"
#include "ObliqueMercatorParameters.h"
#include "PolarStereographicStandardParallelParameters.h"
#include "PolarStereographicScaleFactorParameters.h"
#include "UTMParameters.h"
#include "BNGCoordinates.h"
#include "CartesianCoordinates.h"
#include "GARSCoordinates.h"
#include "GeodeticCoordinates.h"
#include "GEOREFCoordinates.h"
#include "MapProjectionCoordinates.h"
#include "MGRSorUSNGCoordinates.h"
#include "UPSCoordinates.h"
#include "UTMCoordinates.h"
#include "AlbersEqualAreaConic.h"
#include "AzimuthalEquidistant.h"
#include "Bonne.h"
#include "BritishNationalGrid.h"
#include "Cassini.h"
#include "CylindricalEqualArea.h"
#include "Eckert4.h"
#include "Eckert6.h"
#include "EquidistantCylindrical.h"
#include "GARS.h"
#include "Geocentric.h"
#include "GEOREF.h"
#include "Gnomonic.h"
#include "LambertConformalConic.h"
#include "LocalCartesian.h"
#include "Mercator.h"
#include "MGRS.h"
#include "MillerCylindrical.h"
#include "Mollweide.h"
#include "Neys.h"
#include "NZMG.h"
#include "ObliqueMercator.h"
#include "Orthographic.h"
#include "PolarStereographic.h"
#include "Polyconic.h"
#include "Sinusoidal.h"
#include "Stereographic.h"
#include "TransverseCylindricalEqualArea.h"
#include "TransverseMercator.h"
#include "UPS.h"
#include "USNG.h"
#include "UTM.h"
#include "VanDerGrinten.h"
#include "WebMercator.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"
#include "WarningMessages.h"
#include "CCSThreadMutex.h"
#include "CCSThreadLock.h"

using namespace MSP::CCS;
using MSP::CCSThreadMutex;
using MSP::CCSThreadLock;

//                DEFINES
const double PI = 3.14159265358979323e0;
CCSThreadMutex CoordinateConversionService::mutex;



CoordinateConversionService::CCSData::CCSData() :
  refCount( 1 )
{
  ellipsoidLibraryImplementation = EllipsoidLibraryImplementation::getInstance();
  ellipsoidLibrary    = new EllipsoidLibrary( ellipsoidLibraryImplementation ); 
  datumLibraryImplementation = DatumLibraryImplementation::getInstance();
  datumLibrary               = new DatumLibrary( datumLibraryImplementation ); 
  geoidLibrary               = GeoidLibrary::getInstance();
}


CoordinateConversionService::CCSData::~CCSData()
{
  delete ellipsoidLibrary;
  ellipsoidLibrary = 0;

  EllipsoidLibraryImplementation::removeInstance();
  ellipsoidLibraryImplementation = 0;

  delete datumLibrary;
  datumLibrary = 0;

  DatumLibraryImplementation::removeInstance();
  datumLibraryImplementation = 0;

  GeoidLibrary::removeInstance();
  geoidLibrary = 0;

  refCount = 0;
}


/************************************************************************/
/*                              FUNCTIONS
 *
 */

CoordinateConversionService::CoordinateConversionService(
   const char*                           sourceDatumCode,
   MSP::CCS::CoordinateSystemParameters* sourceParameters,
   const char*                           targetDatumCode,
   MSP::CCS::CoordinateSystemParameters* targetParameters ) :
   WGS84_datum_index( 0 )
{
  //Instantiate the variables here so exceptions can be caught
  try
  {
    ellipsoidLibraryImplementation = EllipsoidLibraryImplementation::getInstance();
    datumLibraryImplementation     = DatumLibraryImplementation::getInstance();
    geoidLibrary = GeoidLibrary::getInstance();
    ccsData = new CCSData();
  }
  catch(CoordinateConversionException e)
  {
    //Manage the memory since there could be an instance
    EllipsoidLibraryImplementation::removeInstance();
    ellipsoidLibraryImplementation = 0;

    DatumLibraryImplementation::removeInstance();
    datumLibraryImplementation = 0;

    GeoidLibrary::removeInstance();
    geoidLibrary = 0;

    throw e;
  }

  ellipsoidLibraryImplementation = ccsData->ellipsoidLibraryImplementation;
  datumLibraryImplementation     = ccsData->datumLibraryImplementation;
  geoidLibrary                   = ccsData->geoidLibrary;

  initCoordinateSystemState( SourceOrTarget::source );
  initCoordinateSystemState( SourceOrTarget::target );

  /* Initialize Coordinate System Table */
  strcpy(Coordinate_System_Table[0].Name, "Albers Equal Area Conic");
  strcpy(Coordinate_System_Table[0].Code, "AC");
  Coordinate_System_Table[0].coordinateSystem =
     CoordinateType::albersEqualAreaConic;

  strcpy(Coordinate_System_Table[1].Name, "Azimuthal Equidistant (S)");
  strcpy(Coordinate_System_Table[1].Code, "AL");
  Coordinate_System_Table[1].coordinateSystem =
     CoordinateType::azimuthalEquidistant;

  strcpy(Coordinate_System_Table[2].Name, "Bonne");
  strcpy(Coordinate_System_Table[2].Code, "BF");
  Coordinate_System_Table[2].coordinateSystem = CoordinateType::bonne;

  strcpy(Coordinate_System_Table[3].Name, "British National Grid (BNG)");
  strcpy(Coordinate_System_Table[3].Code, "BN");
  Coordinate_System_Table[3].coordinateSystem =
     CoordinateType::britishNationalGrid;

  strcpy(Coordinate_System_Table[4].Name, "Cassini");
  strcpy(Coordinate_System_Table[4].Code, "CS");
  Coordinate_System_Table[4].coordinateSystem =
     CoordinateType::cassini;

  strcpy(Coordinate_System_Table[5].Name, "Cylindrical Equal Area");
  strcpy(Coordinate_System_Table[5].Code, "LI");
  Coordinate_System_Table[5].coordinateSystem =
     CoordinateType::cylindricalEqualArea;

  strcpy(Coordinate_System_Table[6].Name, "Eckert IV (S)");
  strcpy(Coordinate_System_Table[6].Code, "EF");
  Coordinate_System_Table[6].coordinateSystem =
     CoordinateType::eckert4;

  strcpy(Coordinate_System_Table[7].Name, "Eckert VI (S)");
  strcpy(Coordinate_System_Table[7].Code, "ED");
  Coordinate_System_Table[7].coordinateSystem =
     CoordinateType::eckert6;

  strcpy(Coordinate_System_Table[8].Name, "Equidistant Cylindrical (S)");
  strcpy(Coordinate_System_Table[8].Code, "CP");
  Coordinate_System_Table[8].coordinateSystem =
     CoordinateType::equidistantCylindrical;

  strcpy(Coordinate_System_Table[9].Name, "Geocentric");
  strcpy(Coordinate_System_Table[9].Code, "GC");
  Coordinate_System_Table[9].coordinateSystem =
     CoordinateType::geocentric;

  strcpy(Coordinate_System_Table[10].Name, "Geodetic");
  strcpy(Coordinate_System_Table[10].Code, "GD");
  Coordinate_System_Table[10].coordinateSystem = CoordinateType::geodetic;

  strcpy(Coordinate_System_Table[11].Name, "GEOREF");
  strcpy(Coordinate_System_Table[11].Code, "GE");
  Coordinate_System_Table[11].coordinateSystem = CoordinateType::georef;

  strcpy(Coordinate_System_Table[12].Name,
     "Global Area Reference System (GARS)");
  strcpy(Coordinate_System_Table[12].Code, "GA");
  Coordinate_System_Table[12].coordinateSystem =
     CoordinateType::globalAreaReferenceSystem;

  strcpy(Coordinate_System_Table[13].Name, "Gnomonic (S)");
  strcpy(Coordinate_System_Table[13].Code, "GN");
  Coordinate_System_Table[13].coordinateSystem = CoordinateType::gnomonic;

  strcpy(Coordinate_System_Table[14].Name,
     "Lambert Conformal Conic (1 Standard Parallel)");
  strcpy(Coordinate_System_Table[14].Code, "L1");
  Coordinate_System_Table[14].coordinateSystem =
     CoordinateType::lambertConformalConic1Parallel;

  strcpy(Coordinate_System_Table[15].Name,
     "Lambert Conformal Conic (2 Standard Parallel)");
  strcpy(Coordinate_System_Table[15].Code, "L2");
  Coordinate_System_Table[15].coordinateSystem =
     CoordinateType::lambertConformalConic2Parallels;

  strcpy(Coordinate_System_Table[16].Name, "Local Cartesian");
  strcpy(Coordinate_System_Table[16].Code, "LC");
  Coordinate_System_Table[16].coordinateSystem = CoordinateType::localCartesian;

  strcpy(Coordinate_System_Table[17].Name, "Mercator (Standard Parallel)");
  strcpy(Coordinate_System_Table[17].Code, "MC");
  Coordinate_System_Table[17].coordinateSystem = 
     CoordinateType::mercatorStandardParallel;

  strcpy(Coordinate_System_Table[18].Name, "Mercator (Scale Factor)");
  strcpy(Coordinate_System_Table[18].Code, "MF");
  Coordinate_System_Table[18].coordinateSystem =
     CoordinateType::mercatorScaleFactor;

  strcpy(Coordinate_System_Table[19].Name, "Military Grid Reference System (MGRS)");
  strcpy(Coordinate_System_Table[19].Code, "MG");
  Coordinate_System_Table[19].coordinateSystem =
     CoordinateType::militaryGridReferenceSystem;

  strcpy(Coordinate_System_Table[20].Name, "Miller Cylindrical (S)");
  strcpy(Coordinate_System_Table[20].Code, "MH");
  Coordinate_System_Table[20].coordinateSystem =
     CoordinateType::millerCylindrical;

  strcpy(Coordinate_System_Table[21].Name, "Mollweide (S)");
  strcpy(Coordinate_System_Table[21].Code, "MP");
  Coordinate_System_Table[21].coordinateSystem = CoordinateType::mollweide;

  strcpy(Coordinate_System_Table[22].Name, "New Zealand Map Grid (NZMG)");
  strcpy(Coordinate_System_Table[22].Code, "NT");
  Coordinate_System_Table[22].coordinateSystem =
     CoordinateType::newZealandMapGrid;

  strcpy(Coordinate_System_Table[23].Name,
     "Ney's (Modified Lambert Conformal Conic)");
  strcpy(Coordinate_System_Table[23].Code, "NY");
  Coordinate_System_Table[23].coordinateSystem = CoordinateType::neys;

  strcpy(Coordinate_System_Table[24].Name, "Oblique Mercator");
  strcpy(Coordinate_System_Table[24].Code, "OC");
  Coordinate_System_Table[24].coordinateSystem =
     CoordinateType::obliqueMercator;

  strcpy(Coordinate_System_Table[25].Name, "Orthographic (S)");
  strcpy(Coordinate_System_Table[25].Code, "OD");
  Coordinate_System_Table[25].coordinateSystem = CoordinateType::orthographic;

  strcpy(Coordinate_System_Table[26].Name,
     "Polar Stereographic (Standard Parallel)");
  strcpy(Coordinate_System_Table[26].Code, "PG");
  Coordinate_System_Table[26].coordinateSystem =
     CoordinateType::polarStereographicStandardParallel;

  strcpy(Coordinate_System_Table[27].Name, "Polar Stereographic (Scale Factor)");
  strcpy(Coordinate_System_Table[27].Code, "PF");
  Coordinate_System_Table[27].coordinateSystem =
     CoordinateType::polarStereographicScaleFactor;

  strcpy(Coordinate_System_Table[28].Name, "Polyconic");
  strcpy(Coordinate_System_Table[28].Code, "PH");
  Coordinate_System_Table[28].coordinateSystem = CoordinateType::polyconic;

  strcpy(Coordinate_System_Table[29].Name, "Sinusoidal");
  strcpy(Coordinate_System_Table[29].Code, "SA");
  Coordinate_System_Table[20].coordinateSystem = CoordinateType::sinusoidal;

  strcpy(Coordinate_System_Table[30].Name, "Stereographic (S)");
  strcpy(Coordinate_System_Table[30].Code, "SD");
  Coordinate_System_Table[30].coordinateSystem = CoordinateType::stereographic;

  strcpy(Coordinate_System_Table[31].Name, "Transverse Cylindrical Equal Area");
  strcpy(Coordinate_System_Table[31].Code, "TX");
  Coordinate_System_Table[31].coordinateSystem =
     CoordinateType::transverseCylindricalEqualArea;

  strcpy(Coordinate_System_Table[32].Name, "Transverse Mercator");
  strcpy(Coordinate_System_Table[32].Code, "TC");
  Coordinate_System_Table[32].coordinateSystem =
     CoordinateType::transverseMercator;

  strcpy(Coordinate_System_Table[33].Name,
     "Universal Polar Stereographic (UPS)");
  strcpy(Coordinate_System_Table[33].Code, "UP");
  Coordinate_System_Table[33].coordinateSystem =
     CoordinateType::universalPolarStereographic;

  strcpy(Coordinate_System_Table[34].Name,
     "Universal Transverse Mercator (UTM)");
  strcpy(Coordinate_System_Table[34].Code, "UT");
  Coordinate_System_Table[34].coordinateSystem =
     CoordinateType::universalTransverseMercator;

  strcpy(Coordinate_System_Table[35].Name,
     "United States National Grid (USNG)");
  strcpy(Coordinate_System_Table[35].Code, "US");
  Coordinate_System_Table[35].coordinateSystem =
     CoordinateType::usNationalGrid;

  strcpy(Coordinate_System_Table[36].Name, "Van der Grinten");
  strcpy(Coordinate_System_Table[36].Code, "VA");
  Coordinate_System_Table[36].coordinateSystem = CoordinateType::vanDerGrinten;

  strcpy(Coordinate_System_Table[37].Name, "Web Mercator");
  strcpy(Coordinate_System_Table[37].Code, "WM");
  Coordinate_System_Table[37].coordinateSystem = CoordinateType::webMercator;

  setDataLibraries();

  setDatum(SourceOrTarget::source, sourceDatumCode);
  try
  {
    setCoordinateSystem(SourceOrTarget::source, sourceParameters);
  }
  catch( CoordinateConversionException e )
  {
    throw CoordinateConversionException(
       "Input ",
       Coordinate_System_Table[sourceParameters->coordinateType()].Name,
       ": \n", e.getMessage() );
  }
  
  setDatum(SourceOrTarget::target, targetDatumCode);

  try
  {
    setCoordinateSystem(SourceOrTarget::target, targetParameters);
  }
  catch( CoordinateConversionException e )
  {
    throw CoordinateConversionException(
       "Output ",
       Coordinate_System_Table[targetParameters->coordinateType()].Name,
       ": \n", e.getMessage() );
  }

  datumLibraryImplementation->datumIndex( "WGE", &WGS84_datum_index );
}


CoordinateConversionService::CoordinateConversionService(
   const CoordinateConversionService &ccs ) :
   ccsData( ccs.ccsData )
{
  CCSThreadLock lock(&mutex);

  ++ccsData->refCount;

  ellipsoidLibraryImplementation = ccsData->ellipsoidLibraryImplementation;
  datumLibraryImplementation     = ccsData->datumLibraryImplementation;
  geoidLibrary                   = ccsData->geoidLibrary;

  coordinateSystemState[SourceOrTarget::source].coordinateType =
     ccs.coordinateSystemState[SourceOrTarget::source].coordinateType;
  coordinateSystemState[SourceOrTarget::target].coordinateType =
     ccs.coordinateSystemState[SourceOrTarget::target].coordinateType;

  copyParameters( SourceOrTarget::source,
     ccs.coordinateSystemState[SourceOrTarget::source].coordinateType,
     ccs.coordinateSystemState[SourceOrTarget::source].parameters );
  copyParameters( SourceOrTarget::target,
     ccs.coordinateSystemState[SourceOrTarget::target].coordinateType,
     ccs.coordinateSystemState[SourceOrTarget::target].parameters );

  coordinateSystemState[SourceOrTarget::source].datumIndex =
     ccs.coordinateSystemState[SourceOrTarget::source].datumIndex;
  coordinateSystemState[SourceOrTarget::target].datumIndex =
     ccs.coordinateSystemState[SourceOrTarget::target].datumIndex;

  WGS84_datum_index = ccs.WGS84_datum_index;
}


CoordinateConversionService::~CoordinateConversionService()
{
  CCSThreadLock lock(&mutex);

  if( --ccsData->refCount == 0 )
  {
    delete ccsData;
    ccsData = 0;

    EllipsoidLibraryImplementation::removeInstance();
    ellipsoidLibraryImplementation = 0;
    
    DatumLibraryImplementation::removeInstance();
    datumLibraryImplementation = 0;
    
    GeoidLibrary::removeInstance();
    geoidLibrary = 0;
  }

  deleteCoordinateSystem( SourceOrTarget::source );
  deleteCoordinateSystem( SourceOrTarget::target );
}


CoordinateConversionService& CoordinateConversionService::operator=(
   const CoordinateConversionService &ccs )
{
  CCSThreadLock lock(&mutex);

  if( ccsData == ccs.ccsData )
	  return *this;

  if( --ccsData->refCount == 0 )
    delete ccsData;

  ccsData = ccs.ccsData;
  ++ccsData->refCount;

  ellipsoidLibraryImplementation = ccsData->ellipsoidLibraryImplementation;
  datumLibraryImplementation     = ccsData->datumLibraryImplementation;
  geoidLibrary                   = ccsData->geoidLibrary;

  coordinateSystemState[SourceOrTarget::source].coordinateType =
     ccs.coordinateSystemState[SourceOrTarget::source].coordinateType;
  coordinateSystemState[SourceOrTarget::target].coordinateType =
     ccs.coordinateSystemState[SourceOrTarget::target].coordinateType;

  copyParameters( SourceOrTarget::source,
     ccs.coordinateSystemState[SourceOrTarget::source].coordinateType,
     ccs.coordinateSystemState[SourceOrTarget::source].parameters );
  copyParameters( SourceOrTarget::target,
     ccs.coordinateSystemState[SourceOrTarget::target].coordinateType,
     ccs.coordinateSystemState[SourceOrTarget::target].parameters );

  coordinateSystemState[SourceOrTarget::source].datumIndex =
     ccs.coordinateSystemState[SourceOrTarget::source].datumIndex;
  coordinateSystemState[SourceOrTarget::target].datumIndex =
     ccs.coordinateSystemState[SourceOrTarget::target].datumIndex;

  WGS84_datum_index = ccs.WGS84_datum_index;

  return *this;
}


void CoordinateConversionService::convertSourceToTarget(
   CoordinateTuple* sourceCoordinates,
   Accuracy*        sourceAccuracy,
   CoordinateTuple& targetCoordinates,
   Accuracy&        targetAccuracy )
{
/*
 *  The function convertSourceToTarget converts the current source coordinates
 *  in the coordinate system defined by the current source coordinate system
 *  parameters and source datum, into target coordinates in the coordinate
 *  system defined by the target coordinate system parameters and target datum.
 */

  convert( SourceOrTarget::source, SourceOrTarget::target,
     sourceCoordinates, sourceAccuracy, targetCoordinates, targetAccuracy );
}


void CoordinateConversionService::convertTargetToSource(
   CoordinateTuple* targetCoordinates,
   Accuracy*        targetAccuracy,
   CoordinateTuple& sourceCoordinates,
   Accuracy&        sourceAccuracy )
{
/*
 *  The function convertTargetToSource converts the current target coordinates in the coordinate
 *  system defined by the current target coordinate system parameters and target datum,
 *  into source coordinates in the coordinate system defined by the source coordinate
 *  system parameters and source datum.
 */

  convert(
     SourceOrTarget::target, SourceOrTarget::source,
     targetCoordinates, targetAccuracy,
     sourceCoordinates, sourceAccuracy );
}


void CoordinateConversionService::convertSourceToTargetCollection(
   const std::vector<MSP::CCS::CoordinateTuple*>& sourceCoordinates,
   const std::vector<MSP::CCS::Accuracy*>&        sourceAccuracy,
   std::vector<MSP::CCS::CoordinateTuple*>&       targetCoordinates,
   std::vector<MSP::CCS::Accuracy*>&              targetAccuracy )
{
/*
 *  The function convertSourceToTargetCollection will convert a list of 
 *  source coordinates to a list of target coordinates in a single step.
 *
 *  sourceCoordinates  : Coordinates to be converted                   (input)
 *  sourceAccuracy     : Source circular, linear and spherical errors  (input)
 *  targetCoordinates  : Converted coordinates of the target CS        (output)
 *  targetAccuracy     : Target circular, linear and spherical errors  (output)
 */

  convertCollection(
     sourceCoordinates, sourceAccuracy, targetCoordinates, targetAccuracy );
}


void CoordinateConversionService::convertTargetToSourceCollection(
   const std::vector<MSP::CCS::CoordinateTuple*>& targetCoordinates,
   const std::vector<MSP::CCS::Accuracy*>&        targetAccuracy,
   std::vector<MSP::CCS::CoordinateTuple*>&       sourceCoordinates,
   std::vector<MSP::CCS::Accuracy*>&              sourceAccuracy )
{
/*
 *  The function convertTargetToSourceCollection will convert a list of target
 *  coordinates to a list of source coordinates in a single step.
 *
 *  targetCoordinates  : Converted coordinates of the target CS         (input)
 *  targetAccuracy     : Target circular, linear and spherical errors   (input)
 *  sourceCoordinates  : Coordinates of the source CS to be converted   (output)
 *  sourceAccuracy     : Source circular, linear and spherical errors   (output)
 */

  convertCollection(
     targetCoordinates, targetAccuracy, sourceCoordinates, sourceAccuracy );
}
    
  
EllipsoidLibrary* CoordinateConversionService::getEllipsoidLibrary()
{
   /*
    * The function getEllipsoidLibrary returns the ellipsoid library 
    * which provides access to ellipsoidparameter information.
    * 
    */

   return ccsData->ellipsoidLibrary;
}


DatumLibrary* CoordinateConversionService::getDatumLibrary()
{
   /*
    * The function getDatumLibrary returns the datum library 
    * which provides access to datum transformation and parameter information.
    * 
    */

   return ccsData->datumLibrary;
}
  
  
int CoordinateConversionService::getServiceVersion()
{
   /*
    * The function getServiceVersion returns current service version.
    */

  return 360; // update service version for msp 1.5
}


const char* CoordinateConversionService::getDatum(
   const SourceOrTarget::Enum direction ) const
{
   /*
    *  The function getDatum returns the index of the current datum
    *
    *  direction  : Indicates whether the datum is for source or target  (input)
    */

  return coordinateSystemState[direction].datumCode;
}


MSP::CCS::CoordinateSystemParameters* 
CoordinateConversionService::getCoordinateSystem(
   const SourceOrTarget::Enum direction ) const
{
   /*
    *  The function getCoordinateSystem returns the current coordinate system
    *  type.
    *
    *  direction  : Indicates whether the coordinate system is to be used for
    *               source or target   (input)
    */

   switch( coordinateSystemState[direction].coordinateType )
   {
   case CoordinateType::albersEqualAreaConic:
   case CoordinateType::lambertConformalConic2Parallels:
      return coordinateSystemState[direction].parameters.mapProjection6Parameters;
   case CoordinateType::azimuthalEquidistant:
   case CoordinateType::bonne:
   case CoordinateType::cassini:
   case CoordinateType::cylindricalEqualArea:
   case CoordinateType::gnomonic:
   case CoordinateType::orthographic:
   case CoordinateType::polyconic:
   case CoordinateType::stereographic:
      return coordinateSystemState[direction].parameters.mapProjection4Parameters;
   case CoordinateType::eckert4:
   case CoordinateType::eckert6:
   case CoordinateType::millerCylindrical:
   case CoordinateType::mollweide:
   case CoordinateType::sinusoidal:
   case CoordinateType::vanDerGrinten:
      return coordinateSystemState[direction].parameters.mapProjection3Parameters;
   case CoordinateType::equidistantCylindrical:
      return coordinateSystemState[direction].parameters.equidistantCylindricalParameters;
   case CoordinateType::geodetic:
       return coordinateSystemState[direction].parameters.geodeticParameters;
   case CoordinateType::lambertConformalConic1Parallel:
   case CoordinateType::transverseMercator:
   case CoordinateType::transverseCylindricalEqualArea:
      return coordinateSystemState[direction].parameters.mapProjection5Parameters;
   case CoordinateType::localCartesian:
      return coordinateSystemState[direction].parameters.localCartesianParameters;
   case CoordinateType::mercatorStandardParallel:
      return ((Mercator*)(coordinateSystemState[direction].coordinateSystem))->getStandardParallelParameters(); // gets the calculated scale factor
   case CoordinateType::mercatorScaleFactor:
      return coordinateSystemState[direction].parameters.mercatorScaleFactorParameters;
   case CoordinateType::neys:
      return coordinateSystemState[direction].parameters.neysParameters;
   case CoordinateType::obliqueMercator:
      return coordinateSystemState[direction].parameters.obliqueMercatorParameters;
   case CoordinateType::polarStereographicStandardParallel:
      return coordinateSystemState[direction].parameters.polarStereographicStandardParallelParameters;
   case CoordinateType::polarStereographicScaleFactor:
      return coordinateSystemState[direction].parameters.polarStereographicScaleFactorParameters;
   case CoordinateType::universalTransverseMercator:
      return coordinateSystemState[direction].parameters.utmParameters;
   case CoordinateType::britishNationalGrid:
   case CoordinateType::geocentric:
   case CoordinateType::georef:
   case CoordinateType::globalAreaReferenceSystem:
   case CoordinateType::militaryGridReferenceSystem:
   case CoordinateType::newZealandMapGrid:
   case CoordinateType::universalPolarStereographic:
   case CoordinateType::usNationalGrid:
   case CoordinateType::webMercator:
      return coordinateSystemState[direction].parameters.coordinateSystemParameters;
   default:
      throw CoordinateConversionException(ErrorMessages::invalidType);
   }
}


/************************************************************************/
/*                              PRIVATE FUNCTIONS     
 *
 */

void CoordinateConversionService::initCoordinateSystemState(
   const SourceOrTarget::Enum direction )
{
/*
 *  The function initCoordinateSystemState initializes coordinateSystemState.
 *
 *  direction  : Indicates whether the coordinate system is to be used for
 *               source or target                                      (input)
 */

  CCSThreadLock lock(&mutex);

  coordinateSystemState[direction].datumIndex       = 0;
  coordinateSystemState[direction].coordinateType   = CoordinateType::geodetic;
  coordinateSystemState[direction].coordinateSystem = 0;

  coordinateSystemState[direction].parameters.coordinateSystemParameters = 0;
  coordinateSystemState[direction].parameters.mapProjection3Parameters   = 0;
  coordinateSystemState[direction].parameters.mapProjection4Parameters   = 0;
  coordinateSystemState[direction].parameters.mapProjection5Parameters   = 0;
  coordinateSystemState[direction].parameters.mapProjection6Parameters   = 0;
  coordinateSystemState[direction].parameters.equidistantCylindricalParameters = 0;
  coordinateSystemState[direction].parameters.geodeticParameters       = 0;
  coordinateSystemState[direction].parameters.localCartesianParameters = 0;
  coordinateSystemState[direction].parameters.mercatorStandardParallelParameters = 0;
  coordinateSystemState[direction].parameters.mercatorScaleFactorParameters = 0;
  coordinateSystemState[direction].parameters.neysParameters                = 0;
  coordinateSystemState[direction].parameters.obliqueMercatorParameters     = 0;
  coordinateSystemState[direction].parameters.polarStereographicStandardParallelParameters = 0;
  coordinateSystemState[direction].parameters.polarStereographicScaleFactorParameters = 0;
  coordinateSystemState[direction].parameters.utmParameters = 0;
}


void CoordinateConversionService::setDataLibraries()
{
   /*
    *  The function setDataLibraries sets the initial state of the engine in
    *  in preparation for coordinate conversion and/or datum transformation
    *  operations.
    */

   try 
   {
      datumLibraryImplementation->setEllipsoidLibraryImplementation(
         ellipsoidLibraryImplementation );
      ellipsoidLibraryImplementation->setDatumLibraryImplementation(
         datumLibraryImplementation );
   }
   catch(CoordinateConversionException e)
   {
      char message[256] = "Error initializing MSP CCS data: ";
      strcpy( message, e.getMessage() );
      throw CoordinateConversionException( message );
   }
}


void CoordinateConversionService::setDatum(
   const SourceOrTarget::Enum direction,
   const char*                datumCode )
{
   /*
    *  The function setDatum sets the datum to the
    *  datum corresponding to the specified index.
    *
    *  direction  : Indicates whether the datum is for source or  target (input)
    *  datumCode  : Identifies the code of the datum to be used          (input)
    */

   CCSThreadLock lock(&mutex);

   if( !datumCode )
      throw CoordinateConversionException( ErrorMessages::invalidDatumCode );

   strcpy( coordinateSystemState[direction].datumCode, datumCode );

   long datumIndex = 0;
   datumLibraryImplementation->datumIndex( datumCode, &datumIndex );
   coordinateSystemState[direction].datumIndex = datumIndex;
}


void CoordinateConversionService::setCoordinateSystem(
   const SourceOrTarget::Enum            direction,
   MSP::CCS::CoordinateSystemParameters* parameters )
{
   /*
    *  The function setCoordinateSystem sets the coordinate system.
    *
    *  direction  : Indicates whether the coordinate system is to be used for
    *               source or target                                  (input)
    *  parameters : Coordinate system parameters to be used           (input)
    */

   CCSThreadLock lock(&mutex);

   coordinateSystemState[direction].coordinateSystem = 0;

   switch( parameters->coordinateType() )
   {
   case CoordinateType::albersEqualAreaConic:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::albersEqualAreaConic;
      coordinateSystemState[direction].parameters.mapProjection6Parameters =
         new MapProjection6Parameters(
            *dynamic_cast< MapProjection6Parameters* >( parameters ) );
      break;
   case CoordinateType::azimuthalEquidistant:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::azimuthalEquidistant;
      coordinateSystemState[direction].parameters.mapProjection4Parameters =
         new MapProjection4Parameters(
            *dynamic_cast< MapProjection4Parameters* >( parameters ) );
      break;
    case CoordinateType::bonne:
       coordinateSystemState[direction].coordinateType = CoordinateType::bonne;
       coordinateSystemState[direction].parameters.mapProjection4Parameters =
          new MapProjection4Parameters(
             *dynamic_cast< MapProjection4Parameters* >( parameters ) );
       break;
   case CoordinateType::britishNationalGrid:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::britishNationalGrid;
      coordinateSystemState[direction].parameters.coordinateSystemParameters =
         new CoordinateSystemParameters(
            *dynamic_cast< CoordinateSystemParameters* >( parameters ) );
      break;
    case CoordinateType::cassini:
      coordinateSystemState[direction].coordinateType = CoordinateType::cassini;
      coordinateSystemState[direction].parameters.mapProjection4Parameters =
         new MapProjection4Parameters(
            *dynamic_cast< MapProjection4Parameters* >( parameters ) );
      break;
    case CoordinateType::cylindricalEqualArea:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::cylindricalEqualArea;
      coordinateSystemState[direction].parameters.mapProjection4Parameters =
         new MapProjection4Parameters(
            *dynamic_cast< MapProjection4Parameters* >( parameters ) );
      break;
    case CoordinateType::eckert4:
      coordinateSystemState[direction].coordinateType = CoordinateType::eckert4;
      coordinateSystemState[direction].parameters.mapProjection3Parameters =
         new MapProjection3Parameters(
            *dynamic_cast< MapProjection3Parameters* >( parameters ) );
      break;
    case CoordinateType::eckert6:
      coordinateSystemState[direction].coordinateType = CoordinateType::eckert6;
      coordinateSystemState[direction].parameters.mapProjection3Parameters =
         new MapProjection3Parameters(
            *dynamic_cast< MapProjection3Parameters* >( parameters ) );
      break;
    case CoordinateType::equidistantCylindrical:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::equidistantCylindrical;
      coordinateSystemState[direction].parameters.equidistantCylindricalParameters =
         new EquidistantCylindricalParameters(
            *dynamic_cast< EquidistantCylindricalParameters* >( parameters ) );
      break;
    case CoordinateType::geocentric:
       coordinateSystemState[direction].coordinateType =
          CoordinateType::geocentric;
      coordinateSystemState[direction].parameters.coordinateSystemParameters =
         new CoordinateSystemParameters(
            *dynamic_cast< CoordinateSystemParameters* >( parameters ) );
      break;
    case CoordinateType::geodetic:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::geodetic;
      coordinateSystemState[direction].parameters.geodeticParameters =
         new GeodeticParameters(
            *dynamic_cast< GeodeticParameters* >( parameters ) );
      break;
    case CoordinateType::georef:
      coordinateSystemState[direction].coordinateType = CoordinateType::georef;
      coordinateSystemState[direction].parameters.coordinateSystemParameters =
         new CoordinateSystemParameters(
            *dynamic_cast< CoordinateSystemParameters* >( parameters ) );
      break;
    case CoordinateType::globalAreaReferenceSystem:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::globalAreaReferenceSystem;
      coordinateSystemState[direction].parameters.coordinateSystemParameters =
         new CoordinateSystemParameters(
            *dynamic_cast< CoordinateSystemParameters* >( parameters ) );
      break;
    case CoordinateType::gnomonic:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::gnomonic;
      coordinateSystemState[direction].parameters.mapProjection4Parameters =
         new MapProjection4Parameters(
            *dynamic_cast< MapProjection4Parameters* >( parameters ) );
      break;
    case CoordinateType::lambertConformalConic1Parallel:
      coordinateSystemState[direction].coordinateType = CoordinateType::lambertConformalConic1Parallel;
      coordinateSystemState[direction].parameters.mapProjection5Parameters =
         new MapProjection5Parameters(
            *dynamic_cast< MapProjection5Parameters* >( parameters ) );
      break;
    case CoordinateType::lambertConformalConic2Parallels:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::lambertConformalConic2Parallels;
      coordinateSystemState[direction].parameters.mapProjection6Parameters =
         new MapProjection6Parameters(
            *dynamic_cast< MapProjection6Parameters* >( parameters ) );
      break;
    case CoordinateType::localCartesian:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::localCartesian;
      coordinateSystemState[direction].parameters.localCartesianParameters =
         new LocalCartesianParameters(
            *dynamic_cast< LocalCartesianParameters* >( parameters ) );
      break;
    case CoordinateType::mercatorStandardParallel:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::mercatorStandardParallel;
      coordinateSystemState[direction].parameters.mercatorStandardParallelParameters =
         new MercatorStandardParallelParameters(
            *dynamic_cast< MercatorStandardParallelParameters* >( parameters ));
      break;
    case CoordinateType::mercatorScaleFactor:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::mercatorScaleFactor;
      coordinateSystemState[direction].parameters.mercatorScaleFactorParameters =
         new MercatorScaleFactorParameters(
            *dynamic_cast< MercatorScaleFactorParameters* >( parameters ) );
      break;
    case CoordinateType::militaryGridReferenceSystem:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::militaryGridReferenceSystem;
      coordinateSystemState[direction].parameters.coordinateSystemParameters =
         new CoordinateSystemParameters(
            *dynamic_cast< CoordinateSystemParameters* >( parameters ) );
      break;
    case CoordinateType::millerCylindrical:
      coordinateSystemState[direction].coordinateType = CoordinateType::millerCylindrical;
      coordinateSystemState[direction].parameters.mapProjection3Parameters =
         new MapProjection3Parameters(
            *dynamic_cast< MapProjection3Parameters* >( parameters ) );
      break;
    case CoordinateType::mollweide:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::mollweide;
      coordinateSystemState[direction].parameters.mapProjection3Parameters =
         new MapProjection3Parameters(
            *dynamic_cast< MapProjection3Parameters* >( parameters ) );
      break;
    case CoordinateType::newZealandMapGrid:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::newZealandMapGrid;
      coordinateSystemState[direction].parameters.coordinateSystemParameters =
         new CoordinateSystemParameters(
            *dynamic_cast< CoordinateSystemParameters* >( parameters ) );
      break;
    case CoordinateType::neys:
      coordinateSystemState[direction].coordinateType = CoordinateType::neys;
      coordinateSystemState[direction].parameters.neysParameters =
         new NeysParameters( *dynamic_cast< NeysParameters* >( parameters ) );
      break;
    case CoordinateType::obliqueMercator:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::obliqueMercator;
      coordinateSystemState[direction].parameters.obliqueMercatorParameters =
         new ObliqueMercatorParameters(
            *dynamic_cast< ObliqueMercatorParameters* >( parameters ) );
      break;
    case CoordinateType::orthographic:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::orthographic;
      coordinateSystemState[direction].parameters.mapProjection4Parameters =
         new MapProjection4Parameters(
            *dynamic_cast< MapProjection4Parameters* >( parameters ) );
      break;
    case CoordinateType::polarStereographicStandardParallel:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::polarStereographicStandardParallel;
      coordinateSystemState[direction].parameters.polarStereographicStandardParallelParameters =
         new PolarStereographicStandardParallelParameters(
            *dynamic_cast< PolarStereographicStandardParallelParameters* >(
               parameters ) );
      break;
    case CoordinateType::polarStereographicScaleFactor:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::polarStereographicScaleFactor;
      coordinateSystemState[direction].parameters.polarStereographicScaleFactorParameters =
         new PolarStereographicScaleFactorParameters(
            *dynamic_cast< PolarStereographicScaleFactorParameters* >( parameters ) );
      break;
    case CoordinateType::polyconic:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::polyconic;
      coordinateSystemState[direction].parameters.mapProjection4Parameters =
         new MapProjection4Parameters(
            *dynamic_cast< MapProjection4Parameters* >( parameters ) );
      break;
    case CoordinateType::sinusoidal:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::sinusoidal;
      coordinateSystemState[direction].parameters.mapProjection3Parameters =
         new MapProjection3Parameters(
            *dynamic_cast< MapProjection3Parameters* >( parameters ) );
      break;
    case CoordinateType::stereographic:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::stereographic;
      coordinateSystemState[direction].parameters.mapProjection4Parameters =
         new MapProjection4Parameters(
            *dynamic_cast< MapProjection4Parameters* >( parameters ) );
      break;
    case CoordinateType::transverseCylindricalEqualArea:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::transverseCylindricalEqualArea;
      coordinateSystemState[direction].parameters.mapProjection5Parameters =
         new MapProjection5Parameters(
            *dynamic_cast< MapProjection5Parameters* >( parameters ) );
      break;
    case CoordinateType::transverseMercator:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::transverseMercator;
      coordinateSystemState[direction].parameters.mapProjection5Parameters =
         new MapProjection5Parameters(
            *dynamic_cast< MapProjection5Parameters* >( parameters ) );
      break;
    case CoordinateType::universalPolarStereographic:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::universalPolarStereographic;
      coordinateSystemState[direction].parameters.coordinateSystemParameters =
         new CoordinateSystemParameters(
            *dynamic_cast< CoordinateSystemParameters* >( parameters ) );
      break;
    case CoordinateType::universalTransverseMercator:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::universalTransverseMercator;
      coordinateSystemState[direction].parameters.utmParameters =
         new UTMParameters( *dynamic_cast< UTMParameters* >( parameters ) );
      break;
    case CoordinateType::usNationalGrid:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::usNationalGrid;
      coordinateSystemState[direction].parameters.coordinateSystemParameters =
         new CoordinateSystemParameters(
            *dynamic_cast< CoordinateSystemParameters* >( parameters ) );
      break;
    case CoordinateType::vanDerGrinten:
      coordinateSystemState[direction].coordinateType =
         CoordinateType::vanDerGrinten;
      coordinateSystemState[direction].parameters.mapProjection3Parameters =
         new MapProjection3Parameters(
            *dynamic_cast< MapProjection3Parameters* >( parameters ) );
      break;
    case CoordinateType::webMercator:
       coordinateSystemState[direction].coordinateType =
          CoordinateType::webMercator;
      coordinateSystemState[direction].parameters.coordinateSystemParameters =
         new CoordinateSystemParameters(
            *dynamic_cast< CoordinateSystemParameters* >( parameters ) );
      break;
    default:
      throw CoordinateConversionException(ErrorMessages::invalidType);
  }

  setParameters( direction );
}

void CoordinateConversionService::setParameters(
   const SourceOrTarget::Enum direction )
{
/*
 *  The function setParameters calls the setParameters function of the source
 *  or target coordinate system.
 *
 *  direction  : Indicates whether the coordinate system is to be used for
 *               source or target                                      (input)
 */


  Coordinate_State_Row* row = &coordinateSystemState[direction];

  char ellipsoidCode[3];
  long ellipsoidIndex;
  double semiMajorAxis;
  double flattening;

  datumLibraryImplementation->datumEllipsoidCode(
     row->datumIndex, ellipsoidCode );

  ellipsoidLibraryImplementation->ellipsoidIndex(
     ellipsoidCode, &ellipsoidIndex );

  ellipsoidLibraryImplementation->ellipsoidParameters(
     ellipsoidIndex, &semiMajorAxis, &flattening );

  switch( coordinateSystemState[direction].coordinateType )
  {
    case CoordinateType::albersEqualAreaConic:
    {
       MapProjection6Parameters* param =
          row->parameters.mapProjection6Parameters;

       coordinateSystemState[direction].coordinateSystem =
          new AlbersEqualAreaConic(
             semiMajorAxis, flattening,
             param->centralMeridian(),
             param->originLatitude(),
             param->standardParallel1(),
             param->standardParallel2(),
             param->falseEasting(),
             param->falseNorthing() );
       break;
    }
    case CoordinateType::azimuthalEquidistant:
    {
      MapProjection4Parameters* param =
         row->parameters.mapProjection4Parameters;

      coordinateSystemState[direction].coordinateSystem =
         new AzimuthalEquidistant(
            semiMajorAxis, flattening,
            param->centralMeridian(),
            param->originLatitude(),
            param->falseEasting(),
            param->falseNorthing() );
      break;
    }
    case CoordinateType::bonne:
    {
       MapProjection4Parameters* param = row->parameters.mapProjection4Parameters;

       coordinateSystemState[direction].coordinateSystem = new Bonne(
          semiMajorAxis, flattening,
          param->centralMeridian(),
          param->originLatitude(),
          param->falseEasting(),
          param->falseNorthing() );
       break;
    }
    case CoordinateType::britishNationalGrid:
    {
       coordinateSystemState[direction].coordinateSystem =
          new BritishNationalGrid( ellipsoidCode );

      break;
    }
    case CoordinateType::cassini:
    {
       MapProjection4Parameters* param =
          row->parameters.mapProjection4Parameters;

       coordinateSystemState[direction].coordinateSystem = new Cassini(
          semiMajorAxis, flattening,
          param->centralMeridian(),
          param->originLatitude(),
          param->falseEasting(),
          param->falseNorthing() );
       break;
    }
    case CoordinateType::cylindricalEqualArea:
    {
       MapProjection4Parameters* param =
          row->parameters.mapProjection4Parameters;

       coordinateSystemState[direction].coordinateSystem =
          new CylindricalEqualArea(
             semiMajorAxis, flattening,
             param->centralMeridian(),
             param->originLatitude(),
             param->falseEasting(),
             param->falseNorthing() );
       break;
    }
    case CoordinateType::eckert4:
    {
       MapProjection3Parameters* param =
          row->parameters.mapProjection3Parameters;

      coordinateSystemState[direction].coordinateSystem = new Eckert4(
         semiMajorAxis, flattening,
         param->centralMeridian(),
         param->falseEasting(),
         param->falseNorthing() );
      break;
    }
    case CoordinateType::eckert6:
    {
      MapProjection3Parameters* param =
         row->parameters.mapProjection3Parameters;

      coordinateSystemState[direction].coordinateSystem = new Eckert6(
         semiMajorAxis, flattening,
         param->centralMeridian(),
         param->falseEasting(),
         param->falseNorthing() );
      break;
    }
    case CoordinateType::equidistantCylindrical:
    {
      EquidistantCylindricalParameters* param =
         row->parameters.equidistantCylindricalParameters;

      coordinateSystemState[direction].coordinateSystem =
         new EquidistantCylindrical(
            semiMajorAxis,flattening,
            param->centralMeridian(),
            param->standardParallel(),
            param->falseEasting(),
            param->falseNorthing() );
      break;
    }
    case CoordinateType::geocentric:
    {
      coordinateSystemState[direction].coordinateSystem =
         new Geocentric( semiMajorAxis, flattening );
      break;
    }
    case CoordinateType::geodetic:
      coordinateSystemState[direction].coordinateSystem = 0;
      break;
    case CoordinateType::georef:
      coordinateSystemState[direction].coordinateSystem = new GEOREF();
      break;
    case CoordinateType::globalAreaReferenceSystem:
      coordinateSystemState[direction].coordinateSystem = new GARS();
      break;
    case CoordinateType::gnomonic:
    {
      MapProjection4Parameters* param =
         row->parameters.mapProjection4Parameters;

      coordinateSystemState[direction].coordinateSystem = new Gnomonic(
         semiMajorAxis, flattening,
         param->centralMeridian(),
         param->originLatitude(),
         param->falseEasting(),
         param->falseNorthing() );
      break;
    }
    case CoordinateType::lambertConformalConic1Parallel:
    {
      MapProjection5Parameters* param = row->parameters.mapProjection5Parameters;

      coordinateSystemState[direction].coordinateSystem =
         new LambertConformalConic(
            semiMajorAxis, flattening,
            param->centralMeridian(),
            param->originLatitude(),
            param->falseEasting(),
            param->falseNorthing(),
            param->scaleFactor() );
      break;
    }
    case CoordinateType::lambertConformalConic2Parallels:
    {
      MapProjection6Parameters* param =
         row->parameters.mapProjection6Parameters;

      coordinateSystemState[direction].coordinateSystem =
         new LambertConformalConic(
            semiMajorAxis, flattening,
            param->centralMeridian(),
            param->originLatitude(),
            param->standardParallel1(),
            param->standardParallel2(),
            param->falseEasting(),
            param->falseNorthing() );
      break;
    }
    case CoordinateType::localCartesian:
    {
      LocalCartesianParameters* param =
         row->parameters.localCartesianParameters;

      coordinateSystemState[direction].coordinateSystem = new LocalCartesian(
         semiMajorAxis, flattening,
         param->longitude(),
         param->latitude(),
         param->height(),
         param->orientation() );
      break;
    }
    case CoordinateType::mercatorStandardParallel:
    {
      MercatorStandardParallelParameters* param =
         row->parameters.mercatorStandardParallelParameters;
      double scaleFactor;
      coordinateSystemState[direction].coordinateSystem = new Mercator(
         semiMajorAxis, flattening,
         param->centralMeridian(),
         param->standardParallel(),
         param->falseEasting(),
         param->falseNorthing(),
         &scaleFactor );
      
      param->setScaleFactor( scaleFactor );

      break;
    }
    case CoordinateType::mercatorScaleFactor:
    {
       MercatorScaleFactorParameters* param =
          row->parameters.mercatorScaleFactorParameters;

       coordinateSystemState[direction].coordinateSystem = new Mercator(
          semiMajorAxis, flattening,
          param->centralMeridian(),
          param->falseEasting(),
          param->falseNorthing(),
          param->scaleFactor() );

      break;
    }
    case CoordinateType::militaryGridReferenceSystem:
    {
       coordinateSystemState[direction].coordinateSystem = new MGRS(
          semiMajorAxis, flattening, ellipsoidCode );

      break;
    }
    case CoordinateType::millerCylindrical:
    {
       MapProjection3Parameters* param =
          row->parameters.mapProjection3Parameters;

       coordinateSystemState[direction].coordinateSystem =
          new MillerCylindrical(
             semiMajorAxis, flattening,
             param->centralMeridian(),
             param->falseEasting(),
             param->falseNorthing() );
       break;
    }
    case CoordinateType::mollweide:
    {
       MapProjection3Parameters* param =
          row->parameters.mapProjection3Parameters;

       coordinateSystemState[direction].coordinateSystem = new Mollweide(
          semiMajorAxis,flattening,
          param->centralMeridian(),
          param->falseEasting(),
          param->falseNorthing() );
       break;
    }
    case CoordinateType::newZealandMapGrid:
    {
       coordinateSystemState[direction].coordinateSystem =
          new NZMG( ellipsoidCode );

      break;
    }
    case CoordinateType::neys:
    {
       NeysParameters* param = row->parameters.neysParameters;

       coordinateSystemState[direction].coordinateSystem = new Neys(
          semiMajorAxis, flattening,
          param->centralMeridian(),
          param->originLatitude(),
          param->standardParallel1(),
          param->falseEasting(),
          param->falseNorthing() );
       break;
    }
    case CoordinateType::obliqueMercator:
    {
       ObliqueMercatorParameters* param =
          row->parameters.obliqueMercatorParameters;

       coordinateSystemState[direction].coordinateSystem = new ObliqueMercator(
          semiMajorAxis, flattening,
          param->originLatitude(),
          param->longitude1(),
          param->latitude1(),
          param->longitude2(),
          param->latitude2(),
          param->falseEasting(),
          param->falseNorthing(),
          param->scaleFactor() );
       break;
    }
    case CoordinateType::orthographic:
    {
       MapProjection4Parameters* param =
          row->parameters.mapProjection4Parameters;

       coordinateSystemState[direction].coordinateSystem = new Orthographic(
          semiMajorAxis, flattening,
          param->centralMeridian(),
          param->originLatitude(),
          param->falseEasting(),
          param->falseNorthing() );
       break;
    }
    case CoordinateType::polarStereographicStandardParallel:
    {
       PolarStereographicStandardParallelParameters* param =
          row->parameters.polarStereographicStandardParallelParameters;

       coordinateSystemState[direction].coordinateSystem =
          new PolarStereographic(
             semiMajorAxis, flattening,
             param->centralMeridian(),
             param->standardParallel(),
             param->falseEasting(),
             param->falseNorthing() );

      break;
    }
    case CoordinateType::polarStereographicScaleFactor:
    {
      PolarStereographicScaleFactorParameters* param =
         row->parameters.polarStereographicScaleFactorParameters;

      coordinateSystemState[direction].coordinateSystem =
         new PolarStereographic(
            semiMajorAxis, flattening,
            param->centralMeridian(),
            param->scaleFactor(),
            param->hemisphere(),
            param->falseEasting(),
            param->falseNorthing() );
      break;
    }
    case CoordinateType::polyconic:
    {
       MapProjection4Parameters* param =
          row->parameters.mapProjection4Parameters;

       coordinateSystemState[direction].coordinateSystem = new Polyconic(
          semiMajorAxis, flattening,
          param->centralMeridian(),
          param->originLatitude(),
          param->falseEasting(),
          param->falseNorthing() );
       break;
    }
    case CoordinateType::sinusoidal:
    {
       MapProjection3Parameters* param =
          row->parameters.mapProjection3Parameters;

       coordinateSystemState[direction].coordinateSystem = new Sinusoidal(
          semiMajorAxis, flattening,
          param->centralMeridian(),
          param->falseEasting(),
          param->falseNorthing() );
       break;
    }
    case CoordinateType::stereographic:
    {
       MapProjection4Parameters* param =
          row->parameters.mapProjection4Parameters;

       coordinateSystemState[direction].coordinateSystem = new Stereographic(
          semiMajorAxis, flattening,
          param->centralMeridian(),
          param->originLatitude(),
          param->falseEasting(),
          param->falseNorthing() );
       break;
    }
    case CoordinateType::transverseCylindricalEqualArea:
    {
       MapProjection5Parameters* param =
          row->parameters.mapProjection5Parameters;

       coordinateSystemState[direction].coordinateSystem =
          new TransverseCylindricalEqualArea(
             semiMajorAxis, flattening,
             param->centralMeridian(),
             param->originLatitude(),
             param->falseEasting(),
             param->falseNorthing(),
             param->scaleFactor() );
       break;
    }
    case CoordinateType::transverseMercator:
    {
       MapProjection5Parameters* param =
          row->parameters.mapProjection5Parameters;
       
       coordinateSystemState[direction].coordinateSystem =
          new TransverseMercator(
             semiMajorAxis, flattening,
             param->centralMeridian(),
             param->originLatitude(),
             param->falseEasting(),
             param->falseNorthing(),
             param->scaleFactor(),
             ellipsoidCode);
       break;
    }
    case CoordinateType::universalPolarStereographic:
    {
       coordinateSystemState[direction].coordinateSystem = new UPS(
          semiMajorAxis, flattening );

       break;
    }
    case CoordinateType::universalTransverseMercator:
    {
       if( direction == SourceOrTarget::target )
       {
          UTMParameters* param = row->parameters.utmParameters;

          if((param->override() != 0) && (param->zone() == 0))
             throw CoordinateConversionException( ErrorMessages::zone );
          else
          {
             if(param->override() == 0)
                param->setZone( 0 );
          }

          coordinateSystemState[direction].coordinateSystem = new UTM(
             semiMajorAxis, flattening, ellipsoidCode, param->zone() );
       }
       else
          coordinateSystemState[direction].coordinateSystem = new UTM(
             semiMajorAxis, flattening, ellipsoidCode, 0 );

       break;
    }
    case CoordinateType::usNationalGrid:
    {
       coordinateSystemState[direction].coordinateSystem = new USNG(
          semiMajorAxis, flattening, ellipsoidCode );

      break;
    }
    case CoordinateType::vanDerGrinten:
    {
       MapProjection3Parameters* param =
          row->parameters.mapProjection3Parameters;

       coordinateSystemState[direction].coordinateSystem = new VanDerGrinten(
          semiMajorAxis, flattening,
          param->centralMeridian(),
          param->falseEasting(),
          param->falseNorthing() );
       break;
    }
    case CoordinateType::webMercator:
    {
       coordinateSystemState[direction].coordinateSystem =
          new WebMercator( ellipsoidCode );
       break;
    }
    default:
     break;
  }
}


void CoordinateConversionService::deleteCoordinateSystem(
   const SourceOrTarget::Enum direction )
{
/*
 *  The function deleteCoordinateSystem frees memory of coordinateSystemState.
 *
 *  direction  : Indicates whether the coordinate system is to be used for
 *               source or target                                      (input)
 */

  switch( coordinateSystemState[direction].coordinateType )
  {
    case CoordinateType::albersEqualAreaConic:
      if( coordinateSystemState[direction].parameters.mapProjection6Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection6Parameters;
        coordinateSystemState[direction].parameters.mapProjection6Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((AlbersEqualAreaConic*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::lambertConformalConic2Parallels:
      if( coordinateSystemState[direction].parameters.mapProjection6Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection6Parameters;
        coordinateSystemState[direction].parameters.mapProjection6Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((LambertConformalConic*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::azimuthalEquidistant:
      if( coordinateSystemState[direction].parameters.mapProjection4Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection4Parameters;
        coordinateSystemState[direction].parameters.mapProjection4Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((AzimuthalEquidistant*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::bonne:
      if( coordinateSystemState[direction].parameters.mapProjection4Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection4Parameters;
        coordinateSystemState[direction].parameters.mapProjection4Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((Bonne*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::cassini:
      if( coordinateSystemState[direction].parameters.mapProjection4Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection4Parameters;
        coordinateSystemState[direction].parameters.mapProjection4Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((Cassini*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::cylindricalEqualArea:
      if( coordinateSystemState[direction].parameters.mapProjection4Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection4Parameters;
        coordinateSystemState[direction].parameters.mapProjection4Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((CylindricalEqualArea*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::gnomonic:
      if( coordinateSystemState[direction].parameters.mapProjection4Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection4Parameters;
        coordinateSystemState[direction].parameters.mapProjection4Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((Gnomonic*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::orthographic:
      if( coordinateSystemState[direction].parameters.mapProjection4Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection4Parameters;
        coordinateSystemState[direction].parameters.mapProjection4Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((Orthographic*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::polyconic:
      if( coordinateSystemState[direction].parameters.mapProjection4Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection4Parameters;
        coordinateSystemState[direction].parameters.mapProjection4Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((Polyconic*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::stereographic:
      if( coordinateSystemState[direction].parameters.mapProjection4Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection4Parameters;
        coordinateSystemState[direction].parameters.mapProjection4Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((Stereographic*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::britishNationalGrid:
      if( coordinateSystemState[direction].parameters.coordinateSystemParameters )
      {
        delete coordinateSystemState[direction].parameters.coordinateSystemParameters;
        coordinateSystemState[direction].parameters.coordinateSystemParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((BritishNationalGrid*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::eckert4:
      if( coordinateSystemState[direction].parameters.mapProjection3Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection3Parameters;
        coordinateSystemState[direction].parameters.mapProjection3Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((Eckert4*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::eckert6:
      if( coordinateSystemState[direction].parameters.mapProjection3Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection3Parameters;
        coordinateSystemState[direction].parameters.mapProjection3Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((Eckert6*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::millerCylindrical:
      if( coordinateSystemState[direction].parameters.mapProjection3Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection3Parameters;
        coordinateSystemState[direction].parameters.mapProjection3Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((MillerCylindrical*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::mollweide:
      if( coordinateSystemState[direction].parameters.mapProjection3Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection3Parameters;
        coordinateSystemState[direction].parameters.mapProjection3Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((Mollweide*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::sinusoidal:
      if( coordinateSystemState[direction].parameters.mapProjection3Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection3Parameters;
        coordinateSystemState[direction].parameters.mapProjection3Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((Sinusoidal*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::vanDerGrinten:
      if( coordinateSystemState[direction].parameters.mapProjection3Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection3Parameters;
        coordinateSystemState[direction].parameters.mapProjection3Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((VanDerGrinten*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::equidistantCylindrical:
      if( coordinateSystemState[direction].parameters.equidistantCylindricalParameters )
      {
        delete coordinateSystemState[direction].parameters.equidistantCylindricalParameters;
        coordinateSystemState[direction].parameters.equidistantCylindricalParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((EquidistantCylindrical*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::geocentric:
      if( coordinateSystemState[direction].parameters.coordinateSystemParameters )
      {
        delete coordinateSystemState[direction].parameters.coordinateSystemParameters;
        coordinateSystemState[direction].parameters.coordinateSystemParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((Geocentric*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::geodetic:
      if( coordinateSystemState[direction].parameters.geodeticParameters )
      {
        delete coordinateSystemState[direction].parameters.geodeticParameters;
        coordinateSystemState[direction].parameters.geodeticParameters = 0;
      }
      break;
    case CoordinateType::georef:
      if( coordinateSystemState[direction].parameters.coordinateSystemParameters )
      {
        delete coordinateSystemState[direction].parameters.coordinateSystemParameters;
        coordinateSystemState[direction].parameters.coordinateSystemParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((GEOREF*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::globalAreaReferenceSystem:
      if( coordinateSystemState[direction].parameters.coordinateSystemParameters )
      {
        delete coordinateSystemState[direction].parameters.coordinateSystemParameters;
        coordinateSystemState[direction].parameters.coordinateSystemParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((GARS*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::lambertConformalConic1Parallel:
      if( coordinateSystemState[direction].parameters.mapProjection5Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection5Parameters;
        coordinateSystemState[direction].parameters.mapProjection5Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((LambertConformalConic*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::transverseCylindricalEqualArea:
      if( coordinateSystemState[direction].parameters.mapProjection5Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection5Parameters;
        coordinateSystemState[direction].parameters.mapProjection5Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((TransverseCylindricalEqualArea*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::transverseMercator:
      if( coordinateSystemState[direction].parameters.mapProjection5Parameters )
      {
        delete coordinateSystemState[direction].parameters.mapProjection5Parameters;
        coordinateSystemState[direction].parameters.mapProjection5Parameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((TransverseMercator*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::localCartesian:
      if( coordinateSystemState[direction].parameters.localCartesianParameters )
      {
        delete coordinateSystemState[direction].parameters.localCartesianParameters;
        coordinateSystemState[direction].parameters.localCartesianParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((LocalCartesian*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::mercatorStandardParallel:
      if( coordinateSystemState[direction].parameters.mercatorStandardParallelParameters )
      {
        delete coordinateSystemState[direction].parameters.mercatorStandardParallelParameters;
        coordinateSystemState[direction].parameters.mercatorStandardParallelParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((Mercator*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::mercatorScaleFactor:
      if( coordinateSystemState[direction].parameters.mercatorScaleFactorParameters )
      {
        delete coordinateSystemState[direction].parameters.mercatorScaleFactorParameters;
        coordinateSystemState[direction].parameters.mercatorScaleFactorParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((Mercator*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::militaryGridReferenceSystem:
      if( coordinateSystemState[direction].parameters.coordinateSystemParameters )
      {
        delete coordinateSystemState[direction].parameters.coordinateSystemParameters;
        coordinateSystemState[direction].parameters.coordinateSystemParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((MGRS*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::usNationalGrid:
      if( coordinateSystemState[direction].parameters.coordinateSystemParameters )
      {
        delete coordinateSystemState[direction].parameters.coordinateSystemParameters;
        coordinateSystemState[direction].parameters.coordinateSystemParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((USNG*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::newZealandMapGrid:
      if( coordinateSystemState[direction].parameters.coordinateSystemParameters )
      {
        delete coordinateSystemState[direction].parameters.coordinateSystemParameters;
        coordinateSystemState[direction].parameters.coordinateSystemParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((NZMG*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::neys:
      if( coordinateSystemState[direction].parameters.neysParameters )
      {
        delete coordinateSystemState[direction].parameters.neysParameters;
        coordinateSystemState[direction].parameters.neysParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((Neys*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::obliqueMercator:
      if( coordinateSystemState[direction].parameters.obliqueMercatorParameters )
      {
        delete coordinateSystemState[direction].parameters.obliqueMercatorParameters;
        coordinateSystemState[direction].parameters.obliqueMercatorParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((ObliqueMercator*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::polarStereographicStandardParallel:
      if( coordinateSystemState[direction].parameters.polarStereographicStandardParallelParameters )
      {
        delete coordinateSystemState[direction].parameters.polarStereographicStandardParallelParameters;
        coordinateSystemState[direction].parameters.polarStereographicStandardParallelParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((PolarStereographic*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::polarStereographicScaleFactor:
      if( coordinateSystemState[direction].parameters.polarStereographicScaleFactorParameters )
      {
        delete coordinateSystemState[direction].parameters.polarStereographicScaleFactorParameters;
        coordinateSystemState[direction].parameters.polarStereographicScaleFactorParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((PolarStereographic*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::universalPolarStereographic:
      if( coordinateSystemState[direction].parameters.coordinateSystemParameters )
      {
        delete coordinateSystemState[direction].parameters.coordinateSystemParameters;
        coordinateSystemState[direction].parameters.coordinateSystemParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((UPS*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    case CoordinateType::universalTransverseMercator:
      if( coordinateSystemState[direction].parameters.utmParameters )
      {
        delete coordinateSystemState[direction].parameters.utmParameters;
        coordinateSystemState[direction].parameters.utmParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((UTM*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
  case CoordinateType::webMercator:
      if( coordinateSystemState[direction].parameters.coordinateSystemParameters )
      {
        delete coordinateSystemState[direction].parameters.coordinateSystemParameters;
        coordinateSystemState[direction].parameters.coordinateSystemParameters = 0;
      }
      if( coordinateSystemState[direction].coordinateSystem )
      {
        delete ((WebMercator*)(coordinateSystemState[direction].coordinateSystem));
        coordinateSystemState[direction].coordinateSystem = 0;
      }
      break;
    default:
      break;
  }
}


void CoordinateConversionService::copyParameters(
   SourceOrTarget::Enum direction,
   CoordinateType::Enum coordinateType,
   Parameters           parameters )
{
/*
 *  The function copyParameters uses the input parameters to set the value of the 
 *  current parameters.
 *
 *  direction       : Indicates whether the coordinate system is to be used for
 *                  source or target                                      (input)
 *  coordinateType  : Coordinate system type                              (input)
 *  parameters      : Coordinate system parameters to copy                (input)
 */

  initCoordinateSystemState( direction );

  switch( coordinateType )
  {
    case CoordinateType::albersEqualAreaConic:
    case CoordinateType::lambertConformalConic2Parallels:
      setCoordinateSystem( direction, parameters.mapProjection6Parameters );
      break;
    case CoordinateType::azimuthalEquidistant:
    case CoordinateType::bonne:
    case CoordinateType::cassini:
    case CoordinateType::cylindricalEqualArea:
    case CoordinateType::gnomonic:
    case CoordinateType::orthographic:
    case CoordinateType::polyconic:
    case CoordinateType::stereographic:
      setCoordinateSystem( direction, parameters.mapProjection4Parameters );
      break;
    case CoordinateType::eckert4:
    case CoordinateType::eckert6:
    case CoordinateType::millerCylindrical:
    case CoordinateType::mollweide:
    case CoordinateType::sinusoidal:
    case CoordinateType::vanDerGrinten:
      setCoordinateSystem( direction, parameters.mapProjection3Parameters );
      break;
    case CoordinateType::equidistantCylindrical:
      setCoordinateSystem( direction, parameters.equidistantCylindricalParameters );
      break;
    case CoordinateType::geodetic:
      setCoordinateSystem( direction, parameters.geodeticParameters );
      break;
    case CoordinateType::lambertConformalConic1Parallel:
    case CoordinateType::transverseMercator:
    case CoordinateType::transverseCylindricalEqualArea:
      setCoordinateSystem( direction, parameters.mapProjection5Parameters );
      break;
    case CoordinateType::localCartesian:
      setCoordinateSystem( direction, parameters.localCartesianParameters );
      break;
    case CoordinateType::mercatorStandardParallel:
      setCoordinateSystem( direction, parameters.mercatorStandardParallelParameters );
      break;
    case CoordinateType::mercatorScaleFactor:
      setCoordinateSystem( direction, parameters.mercatorScaleFactorParameters );
      break;
    case CoordinateType::neys:
      setCoordinateSystem( direction, parameters.neysParameters );
      break;
    case CoordinateType::obliqueMercator:
      setCoordinateSystem( direction, parameters.obliqueMercatorParameters );
      break;
    case CoordinateType::polarStereographicStandardParallel:
      setCoordinateSystem( direction, parameters.polarStereographicStandardParallelParameters );
      break;
    case CoordinateType::polarStereographicScaleFactor:
      setCoordinateSystem( direction, parameters.polarStereographicScaleFactorParameters );
      break;
    case CoordinateType::universalTransverseMercator:
      setCoordinateSystem( direction, parameters.utmParameters );
      break;
    case CoordinateType::britishNationalGrid:
    case CoordinateType::geocentric:
    case CoordinateType::georef:
    case CoordinateType::globalAreaReferenceSystem:
    case CoordinateType::militaryGridReferenceSystem:
    case CoordinateType::newZealandMapGrid:
    case CoordinateType::universalPolarStereographic:
    case CoordinateType::usNationalGrid:
    case CoordinateType::webMercator:
      setCoordinateSystem( direction, parameters.coordinateSystemParameters );
      break;
    default:
      break;
  }
}


void CoordinateConversionService::convert(
   SourceOrTarget::Enum sourceDirection,
   SourceOrTarget::Enum targetDirection,
   CoordinateTuple*     sourceCoordinates,
   Accuracy*            sourceAccuracy,
   CoordinateTuple&     targetCoordinates,
   Accuracy&            targetAccuracy )
{
/*
 *  The function convert converts the current source coordinates in the coordinate
 *  system defined by the current source coordinate system parameters and source datum,
 *  into target coordinates in the coordinate system defined by the target coordinate
 *  system parameters and target datum.
 *
 *  sourceDirection: Indicates which set of coordinates and parameters to use as the source (input)
 *  targetDirection: Indicates which set of coordinates and parameters to use as the target (input)
 */

  CCSThreadLock lock(&mutex);

  GeodeticCoordinates* _convertedGeodetic = 0;
  GeodeticCoordinates* _wgs84Geodetic = 0;
  GeodeticCoordinates* _shiftedGeodetic = 0;

  bool special = false;

  Coordinate_State_Row* source = &coordinateSystemState[sourceDirection];
  Coordinate_State_Row* target = &coordinateSystemState[targetDirection];

  char sourceWarningMessage[256] = "";
  char targetWarningMessage[256] = "";

  if ( // NGA only allows Web Mercator conversions to geodetic
     (source->coordinateType == CoordinateType::webMercator) &&
     (target->coordinateType != CoordinateType::geodetic) )
  {
     throw CoordinateConversionException( ErrorMessages::webmInvalidTargetCS );
  }

  try
  {
     try
     {
        /********************************************************/
        /* Check for special cases when there is no datum shift */
        /********************************************************/
        if (source->datumIndex == target->datumIndex)
        {
           if((source->coordinateType == CoordinateType::geocentric) &&
              (target->coordinateType == CoordinateType::localCartesian))
           {
              special = true;

              CartesianCoordinates* coordinates =
                 dynamic_cast< CartesianCoordinates* >( sourceCoordinates );

              CartesianCoordinates* cartesianCoordinates =
                 ((LocalCartesian*)(
                     target->coordinateSystem))->convertFromGeocentric(
                        coordinates );
              (dynamic_cast< CartesianCoordinates& >( targetCoordinates ) ) =
                 *dynamic_cast< CartesianCoordinates* >( cartesianCoordinates );
              delete cartesianCoordinates;
           }
           else if((source->coordinateType == CoordinateType::localCartesian) &&
              (target->coordinateType == CoordinateType::geocentric))
           {
              special = true;

              CartesianCoordinates* coordinates =
                 dynamic_cast< CartesianCoordinates* >( sourceCoordinates );

          CartesianCoordinates* cartesianCoordinates =
             ((LocalCartesian*)(source->coordinateSystem))->convertToGeocentric(
                coordinates );
          (dynamic_cast< CartesianCoordinates& >( targetCoordinates ) ) =
             *dynamic_cast< CartesianCoordinates* >( cartesianCoordinates );
          delete cartesianCoordinates;
           }
           else if ((source->coordinateType == CoordinateType::militaryGridReferenceSystem) &&
              (target->coordinateType == CoordinateType::universalTransverseMercator) && 
              (target->parameters.utmParameters->override() == 0))
           {
              special = true;

              MGRSorUSNGCoordinates* coordinates =
                 dynamic_cast< MGRSorUSNGCoordinates* >( sourceCoordinates );

              UTMCoordinates* utmCoordinates =
                 ((MGRS*)(source->coordinateSystem))->convertToUTM( coordinates );
              ( dynamic_cast< UTMCoordinates& >( targetCoordinates ) ) =
                 *dynamic_cast< UTMCoordinates* >( utmCoordinates );
              delete utmCoordinates;
           }
        }
        else if ((source->coordinateType == CoordinateType::militaryGridReferenceSystem) &&
           (target->coordinateType == CoordinateType::universalPolarStereographic))
        {
          special = true;

          MGRSorUSNGCoordinates* coordinates = dynamic_cast< MGRSorUSNGCoordinates* >( sourceCoordinates );

          UPSCoordinates* upsCoordinates = ((MGRS*)(source->coordinateSystem))->convertToUPS( coordinates );
          ( dynamic_cast< UPSCoordinates& >( targetCoordinates ) ) = *dynamic_cast< UPSCoordinates* >( upsCoordinates );
          delete upsCoordinates;
      }
      else if ((source->coordinateType == CoordinateType::universalTransverseMercator) &&
               (target->coordinateType == CoordinateType::militaryGridReferenceSystem))
      {
          special = true;

          Precision::Enum temp_precision = ( dynamic_cast< MGRSorUSNGCoordinates& >( targetCoordinates ) ).precision();///ccsPrecision;
          if (temp_precision < 0)
            temp_precision = Precision::degree;
          if (temp_precision > 5)
            temp_precision = Precision::tenthOfSecond;

          UTMCoordinates* coordinates = dynamic_cast< UTMCoordinates* >( sourceCoordinates );

          MGRSorUSNGCoordinates* mgrsOrUSNGCoordinates = ((MGRS*)(target->coordinateSystem))->convertFromUTM( coordinates, temp_precision );
          ( dynamic_cast< MGRSorUSNGCoordinates& >( targetCoordinates ) ) = *dynamic_cast< MGRSorUSNGCoordinates* >( mgrsOrUSNGCoordinates );
          delete mgrsOrUSNGCoordinates;
      }
      else if ((source->coordinateType == CoordinateType::universalPolarStereographic) && (target->coordinateType == CoordinateType::militaryGridReferenceSystem))
      {
          special = true;

          Precision::Enum temp_precision = ( dynamic_cast< MGRSorUSNGCoordinates& >( targetCoordinates ) ).precision();///ccsPrecision;
          if (temp_precision < 0)
            temp_precision = Precision::degree;
          if (temp_precision > 5)
            temp_precision = Precision::tenthOfSecond;

          UPSCoordinates* coordinates = dynamic_cast< UPSCoordinates* >( sourceCoordinates );

          MGRSorUSNGCoordinates* mgrsOrUSNGCoordinates = ((MGRS*)(target->coordinateSystem))->convertFromUPS( coordinates, temp_precision );
          ( dynamic_cast< MGRSorUSNGCoordinates& >( targetCoordinates ) ) = *dynamic_cast< MGRSorUSNGCoordinates* >( mgrsOrUSNGCoordinates );
          delete mgrsOrUSNGCoordinates;
      }
      else if ((source->coordinateType == CoordinateType::usNationalGrid) &&
               (target->coordinateType == CoordinateType::universalTransverseMercator) && (target->parameters.utmParameters->override() == 0))
      {
          special = true;

          MGRSorUSNGCoordinates* coordinates = dynamic_cast< MGRSorUSNGCoordinates* >( sourceCoordinates );

          UTMCoordinates* utmCoordinates = ((USNG*)(source->coordinateSystem))->convertToUTM( coordinates );
          ( dynamic_cast< UTMCoordinates& >( targetCoordinates ) ) = *dynamic_cast< UTMCoordinates* >( utmCoordinates );
          delete utmCoordinates;
      }
      else if ((source->coordinateType == CoordinateType::usNationalGrid) && (target->coordinateType == CoordinateType::universalPolarStereographic))
      {
          special = true;

          MGRSorUSNGCoordinates* coordinates = dynamic_cast< MGRSorUSNGCoordinates* >( sourceCoordinates );

          UPSCoordinates* upsCoordinates = ((USNG*)(source->coordinateSystem))->convertToUPS( coordinates );
          ( dynamic_cast< UPSCoordinates& >( targetCoordinates ) ) = *dynamic_cast< UPSCoordinates* >( upsCoordinates );
          delete upsCoordinates;
      }
      else if ((source->coordinateType == CoordinateType::universalTransverseMercator) && (target->coordinateType == CoordinateType::usNationalGrid))
      {
          special = true;

          Precision::Enum temp_precision = ( dynamic_cast< MGRSorUSNGCoordinates& >( targetCoordinates ) ).precision();///ccsPrecision;
          if (temp_precision < 0)
            temp_precision = Precision::degree;
          if (temp_precision > 5)
            temp_precision = Precision::tenthOfSecond;

          UTMCoordinates* coordinates = dynamic_cast< UTMCoordinates* >( sourceCoordinates );

          MGRSorUSNGCoordinates* mgrsOrUSNGCoordinates = ((USNG*)(target->coordinateSystem))->convertFromUTM( coordinates, temp_precision );
          ( dynamic_cast< MGRSorUSNGCoordinates& >( targetCoordinates ) ) = *dynamic_cast< MGRSorUSNGCoordinates* >( mgrsOrUSNGCoordinates );
          delete mgrsOrUSNGCoordinates;
      }
      else if((source->coordinateType == CoordinateType::universalPolarStereographic) &&
         (target->coordinateType == CoordinateType::usNationalGrid))
      {
          special = true;

          Precision::Enum temp_precision =
             ( dynamic_cast< MGRSorUSNGCoordinates& >(
                  targetCoordinates ) ).precision();///ccsPrecision;
          if (temp_precision < 0)
            temp_precision = Precision::degree;
          if (temp_precision > 5)
            temp_precision = Precision::tenthOfSecond;

          UPSCoordinates* coordinates = dynamic_cast< UPSCoordinates* >(
             sourceCoordinates );

          MGRSorUSNGCoordinates* mgrsOrUSNGCoordinates =
             ((USNG*)(target->coordinateSystem))->convertFromUPS(
                coordinates, temp_precision );
          ( dynamic_cast< MGRSorUSNGCoordinates& >( targetCoordinates ) ) =
             *dynamic_cast< MGRSorUSNGCoordinates* >( mgrsOrUSNGCoordinates );
          delete mgrsOrUSNGCoordinates;
      }
      else if ((source->coordinateType == CoordinateType::transverseMercator) &&
               (target->coordinateType == CoordinateType::britishNationalGrid))
      {
        MapProjection5Parameters* param =
           source->parameters.mapProjection5Parameters;

        if ((param->centralMeridian() == -2.0 * PI / 180) &&
            (param->originLatitude()  == 49.0 * PI / 180) &&
            (param->scaleFactor()     == .9996012717) &&
            (param->falseEasting()    == 400000.0) &&
            (param->falseNorthing()   == -100000.0))
        {
            special = true;

            Precision::Enum temp_precision = ( dynamic_cast< BNGCoordinates& >( targetCoordinates ) ).precision();///ccsPrecision;
            if (temp_precision < 0)
               temp_precision = Precision::degree;
            if (temp_precision > 5)
               temp_precision = Precision::tenthOfSecond;

            MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

            BNGCoordinates* bngCoordinates =
               ((BritishNationalGrid*)(target->coordinateSystem))->
               convertFromTransverseMercator( coordinates, temp_precision );
            ( dynamic_cast< BNGCoordinates& >( targetCoordinates ) ) =
               *dynamic_cast< BNGCoordinates* >( bngCoordinates );
            delete bngCoordinates;
        }
        else
          special = false;
      }
      else if((source->coordinateType == CoordinateType::britishNationalGrid) &&
              (target->coordinateType == CoordinateType::transverseMercator))
      {
        MapProjection5Parameters* param =
           target->parameters.mapProjection5Parameters;

        if ((param->centralMeridian() == -2.0 * PI / 180) &&
            (param->originLatitude() == 49.0 * PI / 180) &&
            (param->scaleFactor() == .9996012717) &&
            (param->falseEasting() == 400000.0) &&
            (param->falseNorthing() == -100000.0))
        {
            special = true;

            BNGCoordinates* coordinates =
               dynamic_cast< BNGCoordinates* >( sourceCoordinates );

            MapProjectionCoordinates* mapProjectionCoordinates =
               ((BritishNationalGrid*)(
                   source->coordinateSystem))->convertToTransverseMercator(
                      coordinates );
            ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ) =
               *dynamic_cast< MapProjectionCoordinates* >(
                  mapProjectionCoordinates );
            delete mapProjectionCoordinates;
        }
        else
          special = false;
      }
     }
     catch( CoordinateConversionException e )
     {
       throw CoordinateConversionException(
          "Input ",
          Coordinate_System_Table[source->coordinateType].Name,
          ": \n", e.getMessage() );
    }

    if( !special )
    {
      /**********************************************************/
      /* First coordinate conversion stage, convert to Geodetic */
      /**********************************************************/

      _convertedGeodetic = convertSourceToGeodetic(
         sourceDirection, sourceCoordinates, sourceWarningMessage );

      /******************************/
      /* Datum Transformation Stage */
      /******************************/

        HeightType::Enum input_height_type;
        HeightType::Enum output_height_type;

        if (source->coordinateType == CoordinateType::geodetic)
          input_height_type = source->parameters.geodeticParameters->heightType();
        else if ((source->coordinateType == CoordinateType::geocentric) || (source->coordinateType == CoordinateType::localCartesian))
          input_height_type = HeightType::ellipsoidHeight;
        else
          input_height_type = HeightType::noHeight;

        if (target->coordinateType == CoordinateType::geodetic)
          output_height_type = target->parameters.geodeticParameters->heightType();
        else if ((target->coordinateType == CoordinateType::geocentric) || (target->coordinateType == CoordinateType::localCartesian))
          output_height_type = HeightType::ellipsoidHeight;
        else
          output_height_type = HeightType::noHeight;

        if ((source->datumIndex == target->datumIndex) &&
            ((input_height_type == output_height_type) ||
             (input_height_type == HeightType::noHeight) ||
             (output_height_type == HeightType::noHeight)))
        { /* Copy coordinate tuple */
          _wgs84Geodetic = new GeodeticCoordinates( *_convertedGeodetic );
          _shiftedGeodetic = new GeodeticCoordinates( *_convertedGeodetic );

          if ((input_height_type == HeightType::noHeight) ||
             (output_height_type == HeightType::noHeight))
          {
            _shiftedGeodetic->setHeight( 0 );
          }

          if(source->datumIndex != WGS84_datum_index &&
             target->datumIndex != WGS84_datum_index)
          {
            long valid = 0;

            /* check source datum validity */
            datumLibraryImplementation->validDatum(
               source->datumIndex, _wgs84Geodetic->longitude(),
               _wgs84Geodetic->latitude(), &valid );
            if( !valid )
            {
              strcat( sourceWarningMessage, MSP::CCS::WarningMessages::datum );
            }

            /* check target datum validity */
            datumLibraryImplementation->validDatum(
               target->datumIndex, _wgs84Geodetic->longitude(),
               _wgs84Geodetic->latitude(), &valid );
            if( !valid )
            {
              strcat( targetWarningMessage, MSP::CCS::WarningMessages::datum );
            }
          }
        }
        else
        { /* Shift to WGS84, apply geoid correction, shift to target datum */
          if (source->datumIndex != WGS84_datum_index)
          {
            _wgs84Geodetic = datumLibraryImplementation->geodeticShiftToWGS84(
               source->datumIndex, _convertedGeodetic );

            switch(input_height_type)
            {
              case HeightType::EGM2008TwoPtFiveMinBicubicSpline:
              case HeightType::EGM96FifteenMinBilinear:
              case HeightType::EGM96VariableNaturalSpline:
              case HeightType::EGM84TenDegBilinear:
              case HeightType::EGM84TenDegNaturalSpline:
              case HeightType::EGM84ThirtyMinBiLinear:
                _wgs84Geodetic->setHeight( _convertedGeodetic->height() );
                break;
              case HeightType::noHeight:
                _wgs84Geodetic->setHeight( 0.0 );
                break;
              case HeightType::ellipsoidHeight:
              default:
                break;
            }

            /* check source datum validity */
            long sourceValid = 0;

            datumLibraryImplementation->validDatum(
               source->datumIndex, _wgs84Geodetic->longitude(),
               _wgs84Geodetic->latitude(), &sourceValid );
            if( !sourceValid )
            {
              strcat( sourceWarningMessage, MSP::CCS::WarningMessages::datum );
            }
          }
          else
          { /* Copy coordinate tuple */
            _wgs84Geodetic = new GeodeticCoordinates( *_convertedGeodetic );
            if( input_height_type == HeightType::noHeight )
              _wgs84Geodetic->setHeight( 0.0 );
          }

          if(input_height_type != output_height_type)
          {
            double tempHeight;

            /* Convert the source height value to an ellipsoid height value */
            switch(input_height_type)
            {
              case HeightType::EGM2008TwoPtFiveMinBicubicSpline:
                geoidLibrary->convertEGM2008GeoidHeightToEllipsoidHeight(
                   _wgs84Geodetic->longitude(), _wgs84Geodetic->latitude(),
                   _wgs84Geodetic->height(), &tempHeight);
                break;
              case HeightType::EGM96FifteenMinBilinear:
                geoidLibrary->convertEGM96FifteenMinBilinearGeoidToEllipsoidHeight(
                   _wgs84Geodetic->longitude(), _wgs84Geodetic->latitude(),
                   _wgs84Geodetic->height(), &tempHeight);
                break;
              case HeightType::EGM96VariableNaturalSpline:
                geoidLibrary->convertEGM96VariableNaturalSplineToEllipsoidHeight(
                   _wgs84Geodetic->longitude(), _wgs84Geodetic->latitude(),
                   _wgs84Geodetic->height(), &tempHeight );
                break;
              case HeightType::EGM84TenDegBilinear:
                geoidLibrary->convertEGM84TenDegBilinearToEllipsoidHeight(
                   _wgs84Geodetic->longitude(), _wgs84Geodetic->latitude(),
                   _wgs84Geodetic->height(), &tempHeight );
                break;
              case HeightType::EGM84TenDegNaturalSpline:
                geoidLibrary->convertEGM84TenDegNaturalSplineToEllipsoidHeight(
                   _wgs84Geodetic->longitude(), _wgs84Geodetic->latitude(),
                   _wgs84Geodetic->height(), &tempHeight );
                break;
              case HeightType::EGM84ThirtyMinBiLinear:
                geoidLibrary->convertEGM84ThirtyMinBiLinearToEllipsoidHeight(
                   _wgs84Geodetic->longitude(), _wgs84Geodetic->latitude(),
                   _wgs84Geodetic->height(), &tempHeight );
                break;
              case HeightType::ellipsoidHeight:
              default:
                tempHeight = _wgs84Geodetic->height();
                break;
            }

            double correctedHeight;

            /* Convert the ellipsoid height value to the target height value */
            switch(output_height_type)
            {
              case HeightType::EGM2008TwoPtFiveMinBicubicSpline:
                geoidLibrary->convertEllipsoidHeightToEGM2008GeoidHeight(
                   _wgs84Geodetic->longitude(), _wgs84Geodetic->latitude(),
                   tempHeight, &correctedHeight );
                break;
              case HeightType::EGM96FifteenMinBilinear:
                geoidLibrary->convertEllipsoidToEGM96FifteenMinBilinearGeoidHeight(
                   _wgs84Geodetic->longitude(), _wgs84Geodetic->latitude(),
                   tempHeight, &correctedHeight );
                break;
              case HeightType::EGM96VariableNaturalSpline:
                geoidLibrary->convertEllipsoidToEGM96VariableNaturalSplineHeight(
                   _wgs84Geodetic->longitude(), _wgs84Geodetic->latitude(),
                   tempHeight, &correctedHeight );
                break;
              case HeightType::EGM84TenDegBilinear:
                geoidLibrary->convertEllipsoidToEGM84TenDegBilinearHeight(
                   _wgs84Geodetic->longitude(), _wgs84Geodetic->latitude(),
                   tempHeight, &correctedHeight );
                break;
              case HeightType::EGM84TenDegNaturalSpline:
                geoidLibrary->convertEllipsoidToEGM84TenDegNaturalSplineHeight(
                   _wgs84Geodetic->longitude(), _wgs84Geodetic->latitude(),
                   tempHeight, &correctedHeight );
                break;
              case HeightType::EGM84ThirtyMinBiLinear:
                geoidLibrary->convertEllipsoidToEGM84ThirtyMinBiLinearHeight(
                   _wgs84Geodetic->longitude(), _wgs84Geodetic->latitude(),
                   tempHeight, &correctedHeight );
                break;
              case HeightType::ellipsoidHeight:
              default:
                correctedHeight = tempHeight;
                break;
            }

            /* Set the target height */
            _wgs84Geodetic->setHeight( correctedHeight );
          }

          if (target->datumIndex != WGS84_datum_index)
          {
            _shiftedGeodetic = 
               datumLibraryImplementation->geodeticShiftFromWGS84(
                  _wgs84Geodetic, target->datumIndex );

            switch(output_height_type)
            {
              case HeightType::EGM2008TwoPtFiveMinBicubicSpline:
              case HeightType::EGM96FifteenMinBilinear:
              case HeightType::EGM96VariableNaturalSpline:
              case HeightType::EGM84TenDegBilinear:
              case HeightType::EGM84TenDegNaturalSpline:
              case HeightType::EGM84ThirtyMinBiLinear:
                _shiftedGeodetic->setHeight( _wgs84Geodetic->height() );
                break;
              case HeightType::noHeight:
                _shiftedGeodetic->setHeight( 0.0 );
                break;
              case HeightType::ellipsoidHeight:
              default:
                break;
            }

            /* check target datum validity */
            long targetValid = 0;

            datumLibraryImplementation->validDatum(
               target->datumIndex, _wgs84Geodetic->longitude(),
               _wgs84Geodetic->latitude(), &targetValid );
            if( !targetValid )
            {
               strcat( targetWarningMessage, MSP::CCS::WarningMessages::datum );
            }
          }
          else
          { /* Copy coordinate tuple */
            _shiftedGeodetic = new GeodeticCoordinates( *_wgs84Geodetic );
            if( output_height_type == HeightType::noHeight )
              _shiftedGeodetic->setHeight( 0.0 );
          }
        }

        /* calculate conversion errors */
        if( strlen( sourceWarningMessage ) > 0 ||
           strlen( targetWarningMessage ) > 0 )
           targetAccuracy.set(-1.0, -1.0, -1.0);
        else
        {
           Precision::Enum precS = sourceCoordinates->precision();
           Precision::Enum precT = targetCoordinates.precision();
           Precision::Enum prec = precS;
           if( precT < prec )
           {
              prec = precT;
           }

           Accuracy* _targetAccuracy =
              datumLibraryImplementation->datumShiftError(
           source->datumIndex, target->datumIndex,
           _wgs84Geodetic->longitude(), 
           _wgs84Geodetic->latitude(), sourceAccuracy, prec );

           targetAccuracy.set(
              _targetAccuracy->circularError90(),
              _targetAccuracy->linearError90(),
              _targetAccuracy->sphericalError90() );

           delete _targetAccuracy;
        }

      /*************************************************************/
      /* Second coordinate conversion stage, convert from Geodetic */
      /*************************************************************/

      convertGeodeticToTarget(
         targetDirection, _shiftedGeodetic,
         targetCoordinates, targetWarningMessage );

      // Format and set the warning message in the target coordinates
      char warningMessage[500] = "";
      bool warning = false;
      if( strlen( sourceWarningMessage ) > 0 )
      {
        strcpy( warningMessage, "Input " );
        strcat( warningMessage,
           Coordinate_System_Table[source->coordinateType].Name );
        strcat( warningMessage, ": \n" );
        strcat( warningMessage, sourceWarningMessage );
        warning = true;
      }
    
      if( strlen( targetWarningMessage ) > 0 )
      {
        if( strlen( warningMessage ) > 0 )
          strcat( warningMessage, "\nOutput " );
        else
          strcpy( warningMessage, "Output " );

        strcat( warningMessage,
           Coordinate_System_Table[target->coordinateType].Name );
        strcat( warningMessage, ": \n" );
        strcat( warningMessage, targetWarningMessage );
        warning = true;
      }
      
      if( warning )
      {
        warningMessage[strlen( warningMessage )] = '\0';
        targetCoordinates.setWarningMessage(warningMessage);
      }
    } /* if (!special) */
  }
  catch(CoordinateConversionException e)
  {
    targetAccuracy.set(-1.0, -1.0, -1.0);
	
	/* since initialized to 0 at the top is safe to 
	   cleanup memory before rethrowing the exception */
    delete _convertedGeodetic;
    delete _shiftedGeodetic;
    delete _wgs84Geodetic;

    throw CoordinateConversionException(e.getMessage());        
  }

  // cleanup memory before returning
  delete _convertedGeodetic;
  delete _shiftedGeodetic;
  delete _wgs84Geodetic;

}


GeodeticCoordinates* CoordinateConversionService::convertSourceToGeodetic(
   SourceOrTarget::Enum sourceDirection,
   CoordinateTuple*     sourceCoordinates,
   char*                sourceWarningMessage )
{
  Coordinate_State_Row* source = &coordinateSystemState[sourceDirection];

  /**********************************************************/
  /* First coordinate conversion stage, convert to Geodetic */
  /**********************************************************/
  try
  {
  switch (source->coordinateType)
  {
    case CoordinateType::geocentric:
    {
        CartesianCoordinates* coordinates =
           dynamic_cast< CartesianCoordinates* >( sourceCoordinates );

        return ((Geocentric*)(source->coordinateSystem))->convertToGeodetic(
           coordinates );
    }
    case CoordinateType::geodetic:
    {
        GeodeticCoordinates* _convertedGeodetic =
           new GeodeticCoordinates(
              *dynamic_cast< GeodeticCoordinates* >( sourceCoordinates ) );

        if( source->parameters.geodeticParameters->heightType() == HeightType::noHeight )
          _convertedGeodetic->setHeight( 0.0 );

        return _convertedGeodetic;
    }
     case CoordinateType::georef:
    {
        GEOREFCoordinates* coordinates = dynamic_cast< GEOREFCoordinates* >(
           sourceCoordinates );

         return ((GEOREF*)(source->coordinateSystem))->convertToGeodetic(
            coordinates );
    }
    case CoordinateType::albersEqualAreaConic:
    {
        MapProjectionCoordinates* coordinates =
           dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((AlbersEqualAreaConic*)(
                   source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::azimuthalEquidistant:
    {
         MapProjectionCoordinates* coordinates =
            dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((AzimuthalEquidistant*)(
                   source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::britishNationalGrid:
    {
        BNGCoordinates* coordinates = dynamic_cast< BNGCoordinates* >( sourceCoordinates );

        return ((BritishNationalGrid*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::bonne:
    {
        MapProjectionCoordinates* coordinates =
           dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((Bonne*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::cassini:
    {
        MapProjectionCoordinates* coordinates =
           dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        GeodeticCoordinates* _convertedGeodetic =
           ((Cassini*)(source->coordinateSystem))->convertToGeodetic(
              coordinates );

        if( strlen( _convertedGeodetic->warningMessage() ) > 0)
           strcat( sourceWarningMessage, _convertedGeodetic->warningMessage() );

        return _convertedGeodetic;
    }
  case CoordinateType::cylindricalEqualArea:
    {
         MapProjectionCoordinates* coordinates =
            dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

         return ((CylindricalEqualArea*)(
                    source->coordinateSystem))->convertToGeodetic( coordinates);
    }
    case CoordinateType::eckert4:
    {
        MapProjectionCoordinates* coordinates =
           dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((Eckert4*)(source->coordinateSystem))->convertToGeodetic(
           coordinates );
    }
    case CoordinateType::eckert6:
    {
        MapProjectionCoordinates* coordinates =
           dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((Eckert6*)(source->coordinateSystem))->convertToGeodetic(
           coordinates );
    }
    case CoordinateType::equidistantCylindrical:
    {
        MapProjectionCoordinates* coordinates =
           dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((EquidistantCylindrical*)(
                   source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::globalAreaReferenceSystem:
    {
        GARSCoordinates* coordinates = dynamic_cast< GARSCoordinates* >( sourceCoordinates );

        return ((GARS*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::gnomonic:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((Gnomonic*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::lambertConformalConic1Parallel:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((LambertConformalConic*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::lambertConformalConic2Parallels:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((LambertConformalConic*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::localCartesian:
    {
        CartesianCoordinates* coordinates = dynamic_cast< CartesianCoordinates* >( sourceCoordinates );

        return ((LocalCartesian*)(source->coordinateSystem))->convertToGeodetic( coordinates);
    }
    case CoordinateType::mercatorStandardParallel:
    case CoordinateType::mercatorScaleFactor:
    {
        MapProjectionCoordinates* coordinates =
           dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((Mercator*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::militaryGridReferenceSystem:
    {
        MGRSorUSNGCoordinates* coordinates = dynamic_cast< MGRSorUSNGCoordinates* >( sourceCoordinates );

        GeodeticCoordinates* _convertedGeodetic = ((MGRS*)(source->coordinateSystem))->convertToGeodetic( coordinates );

        if( strlen( _convertedGeodetic->warningMessage() ) > 0)
          strcat( sourceWarningMessage, _convertedGeodetic->warningMessage() );

        return _convertedGeodetic;
    }
    case CoordinateType::millerCylindrical:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((MillerCylindrical*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::mollweide:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((Mollweide*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::neys:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((Neys*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::newZealandMapGrid:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((NZMG*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::obliqueMercator:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        GeodeticCoordinates* _convertedGeodetic = ((ObliqueMercator*)(source->coordinateSystem))->convertToGeodetic( coordinates );

        if( strlen( _convertedGeodetic->warningMessage() ) > 0)
          strcat( sourceWarningMessage, _convertedGeodetic->warningMessage() );

        return _convertedGeodetic;
    }
    case CoordinateType::orthographic:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((Orthographic*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::polarStereographicStandardParallel:
    case CoordinateType::polarStereographicScaleFactor:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((PolarStereographic*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::polyconic:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((Polyconic*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::sinusoidal:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((Sinusoidal*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::stereographic:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((Stereographic*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::transverseCylindricalEqualArea:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((TransverseCylindricalEqualArea*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::transverseMercator:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

         GeodeticCoordinates* _convertedGeodetic = ((TransverseMercator*)(source->coordinateSystem))->convertToGeodetic( coordinates );

        if( strlen( _convertedGeodetic->warningMessage() ) > 0)
          strcat( sourceWarningMessage, _convertedGeodetic->warningMessage() );

        return _convertedGeodetic;
    }
    case CoordinateType::universalPolarStereographic:
    {
        UPSCoordinates* coordinates = dynamic_cast< UPSCoordinates* >( sourceCoordinates );

        return ((UPS*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::usNationalGrid:
    {
        MGRSorUSNGCoordinates* coordinates = dynamic_cast< MGRSorUSNGCoordinates* >( sourceCoordinates );

        GeodeticCoordinates* _convertedGeodetic = ((USNG*)(source->coordinateSystem))->convertToGeodetic( coordinates );

        if( strlen( _convertedGeodetic->warningMessage() ) > 0)
          strcat( sourceWarningMessage, _convertedGeodetic->warningMessage() );

        return _convertedGeodetic;
    }
    case CoordinateType::universalTransverseMercator:
    {
        UTMCoordinates* coordinates = dynamic_cast< UTMCoordinates* >( sourceCoordinates );

        return ((UTM*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::vanDerGrinten:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((VanDerGrinten*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
    case CoordinateType::webMercator:
    {
        MapProjectionCoordinates* coordinates = dynamic_cast< MapProjectionCoordinates* >( sourceCoordinates );

        return ((WebMercator*)(source->coordinateSystem))->convertToGeodetic( coordinates );
    }
  }
  }
  catch( CoordinateConversionException e )
  {
     throw CoordinateConversionException(
        "Input ", Coordinate_System_Table[source->coordinateType].Name,
        ": \n", e.getMessage() );
  }
}


void CoordinateConversionService::convertGeodeticToTarget(
   SourceOrTarget::Enum targetDirection,
   GeodeticCoordinates* _shiftedGeodetic,
   CoordinateTuple&     targetCoordinates,
   char*                targetWarningMessage )
{
  Coordinate_State_Row* target = &coordinateSystemState[targetDirection];

  /*************************************************************/
  /* Second coordinate conversion stage, convert from Geodetic */
  /*************************************************************/
  try
  {
  switch (target->coordinateType)
  {
    case CoordinateType::geocentric:
    {
        CartesianCoordinates* coordinates =
           ((Geocentric*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< CartesianCoordinates& >( targetCoordinates ) ).set(
           coordinates->x(), coordinates->y(), coordinates->z() );

        delete coordinates;

      break;
    }
    case CoordinateType::geodetic:
    {
      if (target->parameters.geodeticParameters->heightType() == HeightType::noHeight)
        ( dynamic_cast< GeodeticCoordinates& >( targetCoordinates ) ).set(
           _shiftedGeodetic->longitude(), _shiftedGeodetic->latitude(), 0.0 );
      else
        ( dynamic_cast< GeodeticCoordinates& >( targetCoordinates ) ).set(
           _shiftedGeodetic->longitude(), _shiftedGeodetic->latitude(),
           _shiftedGeodetic->height() );

      break;
    }
    case CoordinateType::georef:
    {
        Precision::Enum temp_precision =
           ( dynamic_cast< GEOREFCoordinates& >(
                targetCoordinates ) ).precision();
        if (temp_precision < 0)
          temp_precision = Precision::degree;
        if (temp_precision > 5)
          temp_precision = Precision::tenthOfSecond;

        GEOREFCoordinates* coordinates =
           ((GEOREF*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic, temp_precision );

        ( dynamic_cast< GEOREFCoordinates& >( targetCoordinates ) ).set(
           coordinates->GEOREFString() );

        delete coordinates;

      break;
    }
    case CoordinateType::albersEqualAreaConic:
    {
        MapProjectionCoordinates* coordinates =
           ((AlbersEqualAreaConic*)(
               target->coordinateSystem))->convertFromGeodetic(
                  _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
                coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::azimuthalEquidistant:
    {
        MapProjectionCoordinates* coordinates =
           ((AzimuthalEquidistant*)(
               target->coordinateSystem))->convertFromGeodetic(
                  _shiftedGeodetic );
        
        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::britishNationalGrid:
    {
        Precision::Enum temp_precision =
           ( dynamic_cast< BNGCoordinates& >(targetCoordinates ) ).precision();
        if (temp_precision < 0)
          temp_precision = Precision::degree;
        if (temp_precision > 5)
          temp_precision = Precision::tenthOfSecond;

        BNGCoordinates* coordinates = 
           ((BritishNationalGrid*)(
               target->coordinateSystem))->convertFromGeodetic(
                  _shiftedGeodetic, temp_precision );
        
        ( dynamic_cast< BNGCoordinates& >( targetCoordinates ) ).set(
           coordinates->BNGString() );
        
        delete coordinates;

      break;
    }
    case CoordinateType::bonne:
    {
        MapProjectionCoordinates* coordinates =
           ((Bonne*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );
        
        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );
       
        delete coordinates;

      break;
    }
    case CoordinateType::cassini:
    {
        MapProjectionCoordinates* coordinates =
           ((Cassini*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        if( strlen( coordinates->warningMessage() ) > 0)
        {
          targetCoordinates.setWarningMessage( coordinates->warningMessage() );
          strcat( targetWarningMessage, targetCoordinates.warningMessage() );
        }

        delete coordinates;

      break;
    }
    case CoordinateType::cylindricalEqualArea:
    {
        MapProjectionCoordinates* coordinates =
           ((CylindricalEqualArea*)(
               target->coordinateSystem))->convertFromGeodetic(
                  _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::eckert4:
    {
        MapProjectionCoordinates* coordinates =
           ((Eckert4*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::eckert6:
    {
        MapProjectionCoordinates* coordinates =
           ((Eckert6*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::equidistantCylindrical:
    {
        MapProjectionCoordinates* coordinates =
           ((EquidistantCylindrical*)(
               target->coordinateSystem))->convertFromGeodetic(
                  _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::globalAreaReferenceSystem:
    {
        Precision::Enum temp_precision =
           (dynamic_cast< GARSCoordinates& >( targetCoordinates )).precision();
        if (temp_precision < 0)
          temp_precision = Precision::degree;
        if (temp_precision > 5)
          temp_precision = Precision::tenthOfSecond;

        GARSCoordinates* coordinates =
           ((GARS*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic, temp_precision );

        ( dynamic_cast< GARSCoordinates& >( targetCoordinates ) ).set(
           coordinates->GARSString() );

        delete coordinates;

      break;
    }
    case CoordinateType::gnomonic:
    {
        MapProjectionCoordinates* coordinates =
           ((Gnomonic*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
   case CoordinateType::lambertConformalConic1Parallel:
    {
        MapProjectionCoordinates* coordinates =
           ((LambertConformalConic*)(
               target->coordinateSystem))->convertFromGeodetic(
                  _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
      }
     case CoordinateType::lambertConformalConic2Parallels:
    {
        MapProjectionCoordinates* coordinates =
           ((LambertConformalConic*)(
               target->coordinateSystem))->convertFromGeodetic(
                  _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::localCartesian:
    {
        CartesianCoordinates* coordinates =
           ((LocalCartesian*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< CartesianCoordinates& >( targetCoordinates ) ).set(
           coordinates->x(), coordinates->y(), coordinates->z() );

        delete coordinates;

      break;
    }
    case CoordinateType::mercatorStandardParallel:
    case CoordinateType::mercatorScaleFactor:
    {
        MapProjectionCoordinates* coordinates =
           ((Mercator*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::militaryGridReferenceSystem:
    {
        Precision::Enum temp_precision =
           ( dynamic_cast< MGRSorUSNGCoordinates& >(
                targetCoordinates ) ).precision();///ccsPrecision;

        if (temp_precision < 0)
          temp_precision = Precision::degree;
        if (temp_precision > 5)
          temp_precision = Precision::tenthOfSecond;
        MGRSorUSNGCoordinates* coordinates = 
           ((MGRS*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic, temp_precision );

        ( dynamic_cast< MGRSorUSNGCoordinates& >( targetCoordinates ) ).set(
           coordinates->MGRSString() );

        delete coordinates;

      break;
    }
    case CoordinateType::millerCylindrical:
    {
        MapProjectionCoordinates* coordinates =
           ((MillerCylindrical*)(
               target->coordinateSystem))->convertFromGeodetic(
                  _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::mollweide:
    {
        MapProjectionCoordinates* coordinates =
           ((Mollweide*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::neys:
    {
        MapProjectionCoordinates* coordinates =
           ((Neys*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::newZealandMapGrid:
    {
        MapProjectionCoordinates* coordinates =
           ((NZMG*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::obliqueMercator:
    {
        MapProjectionCoordinates* coordinates =
           ((ObliqueMercator*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        if( strlen( coordinates->warningMessage() ) > 0)
        {
          targetCoordinates.setWarningMessage( coordinates->warningMessage() );
          strcat( targetWarningMessage, targetCoordinates.warningMessage() );
        }

        delete coordinates;

      break;
    }
    case CoordinateType::orthographic:
    {
        MapProjectionCoordinates* coordinates =
           ((Orthographic*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::polarStereographicStandardParallel:
    case CoordinateType::polarStereographicScaleFactor:
    {
        MapProjectionCoordinates* coordinates =
           ((PolarStereographic*)(
               target->coordinateSystem))->convertFromGeodetic(
                  _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::polyconic:
    {
         MapProjectionCoordinates* coordinates =
            ((Polyconic*)(target->coordinateSystem))->convertFromGeodetic(
               _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        if( strlen( coordinates->warningMessage() ) > 0)
        {
          targetCoordinates.setWarningMessage( coordinates->warningMessage() );
          strcat( targetWarningMessage, targetCoordinates.warningMessage() );
        }

        delete coordinates;

      break;
    }
    case CoordinateType::sinusoidal:
    {
        MapProjectionCoordinates* coordinates =
           ((Sinusoidal*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::stereographic:
    {
        MapProjectionCoordinates* coordinates =
           ((Stereographic*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::transverseCylindricalEqualArea:
    {
        MapProjectionCoordinates* coordinates =
           ((TransverseCylindricalEqualArea*)(
               target->coordinateSystem))->convertFromGeodetic(
                  _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        if( strlen( coordinates->warningMessage() ) > 0)
        {
          targetCoordinates.setWarningMessage( coordinates->warningMessage() );
          strcat( targetWarningMessage, targetCoordinates.warningMessage() );
        }

        delete coordinates;

      break;
    }
    case CoordinateType::transverseMercator:
    {
        MapProjectionCoordinates* coordinates =
           ((TransverseMercator*)(
               target->coordinateSystem))->convertFromGeodetic(
                  _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        if( strlen( coordinates->warningMessage() ) > 0)
        {
          targetCoordinates.setWarningMessage( coordinates->warningMessage() );
          strcat( targetWarningMessage, targetCoordinates.warningMessage() );
        }

        delete coordinates;

      break;
    }
    case CoordinateType::universalPolarStereographic:
    {
        UPSCoordinates* coordinates =
           ((UPS*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< UPSCoordinates& >( targetCoordinates ) ).set(
           coordinates->hemisphere(),
           coordinates->easting(),
           coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::usNationalGrid:
    {
        Precision::Enum temp_precision =
           ( dynamic_cast< MGRSorUSNGCoordinates& >(
                targetCoordinates ) ).precision();///ccsPrecision;
        if (temp_precision < 0)
          temp_precision = Precision::degree;
        if (temp_precision > 5)
          temp_precision = Precision::tenthOfSecond;

        MGRSorUSNGCoordinates* coordinates =
           ((USNG*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic, temp_precision );

        ( dynamic_cast< MGRSorUSNGCoordinates& >( targetCoordinates ) ).set(
           coordinates->MGRSString() );

        delete coordinates;

      break;
    }
    case CoordinateType::universalTransverseMercator:
    {
        UTMCoordinates* coordinates =
           ((UTM*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< UTMCoordinates& >( targetCoordinates ) ).set(
           coordinates->zone(), coordinates->hemisphere(),
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::vanDerGrinten:
    {
        MapProjectionCoordinates* coordinates =
           ((VanDerGrinten*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
    case CoordinateType::webMercator:
    {
        MapProjectionCoordinates* coordinates =
           ((WebMercator*)(target->coordinateSystem))->convertFromGeodetic(
              _shiftedGeodetic );

        ( dynamic_cast< MapProjectionCoordinates& >( targetCoordinates ) ).set(
           coordinates->easting(), coordinates->northing() );

        delete coordinates;

      break;
    }
  } /* switch (target->coordinateType) */
  }
  catch( CoordinateConversionException e )
  {
     throw CoordinateConversionException(
        "Output ", Coordinate_System_Table[target->coordinateType].Name,
        ": \n", e.getMessage() );
  }
}


void CoordinateConversionService::convertCollection(
   const std::vector<MSP::CCS::CoordinateTuple*>& sourceCoordinatesCollection,
   const std::vector<MSP::CCS::Accuracy*>&        sourceAccuracyCollection,
   std::vector<MSP::CCS::CoordinateTuple*>&       targetCoordinatesCollection,
   std::vector<MSP::CCS::Accuracy*>&              targetAccuracyCollection )
{
/*
 *  The function convertCollection will convert a list of source coordinates
 *  to a list of target coordinates in a single step.
 *
 *  sourceCoordinatesCollection : Coordinates to be converted           (input)
 *  sourceAccuracyCollection  : Source circular/linear/spherical errors (input)
 *  targetCoordinatesCollection : Converted coordinates                 (output)
 *  targetAccuracyCollection  : Target circular/linear/spherical errors (output)
 */

  int num = sourceCoordinatesCollection.size();
  int numTargetCoordinates = targetCoordinatesCollection.size();
  int numTargetAccuracies = targetAccuracyCollection.size();
  CoordinateType::Enum targetCoordinateType = 
     coordinateSystemState[SourceOrTarget::target].coordinateType;

  CoordinateTuple* _targetCoordinates = 0;
  Accuracy* _targetAccuracy;

  for( int i = 0; i < num; i++ )
  {
    CoordinateTuple* _sourceCoordinates = sourceCoordinatesCollection[i];
    Accuracy* _sourceAccuracy = sourceAccuracyCollection[i];

    bool targetCoordinateExists = true;
    bool targetAccuracyExists = true;
      
    if(i < numTargetAccuracies)
      _targetAccuracy = targetAccuracyCollection[i];
    else
    {
      _targetAccuracy = new Accuracy();
      targetAccuracyExists = false;
    }

    if( _sourceCoordinates && _sourceAccuracy )
    {
      switch(targetCoordinateType)
      {
        case CoordinateType::albersEqualAreaConic:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::albersEqualAreaConic);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::azimuthalEquidistant:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::azimuthalEquidistant);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::bonne:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::bonne);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::britishNationalGrid:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new BNGCoordinates(
               CoordinateType::britishNationalGrid);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::cassini:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::cassini);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::cylindricalEqualArea:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::cylindricalEqualArea);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::eckert4:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::eckert4);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::eckert6:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::eckert6);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::equidistantCylindrical:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::equidistantCylindrical);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::geocentric:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new CartesianCoordinates(
               CoordinateType::geocentric);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source, SourceOrTarget::target,
               sourceCoordinatesCollection[i], sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::geodetic:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new GeodeticCoordinates(
               CoordinateType::geodetic);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source, SourceOrTarget::target,
               sourceCoordinatesCollection[i], sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
         break;
        }
        case CoordinateType::georef:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new GEOREFCoordinates(CoordinateType::georef);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::globalAreaReferenceSystem:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new GARSCoordinates(
               CoordinateType::globalAreaReferenceSystem);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::gnomonic:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::gnomonic);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::lambertConformalConic1Parallel:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::lambertConformalConic1Parallel);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates,
               *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::lambertConformalConic2Parallels:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::lambertConformalConic2Parallels);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::localCartesian:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new CartesianCoordinates(
               CoordinateType::localCartesian);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::mercatorStandardParallel:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::mercatorStandardParallel);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::mercatorScaleFactor:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::mercatorScaleFactor);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::militaryGridReferenceSystem:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MGRSorUSNGCoordinates(
               CoordinateType::militaryGridReferenceSystem);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::millerCylindrical:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::millerCylindrical);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::mollweide:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::mollweide);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::newZealandMapGrid:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::newZealandMapGrid);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::neys:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::neys);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::obliqueMercator:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::obliqueMercator);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::orthographic:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::orthographic);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::polarStereographicStandardParallel:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::polarStereographicStandardParallel);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::polarStereographicScaleFactor:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::polarStereographicScaleFactor);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::polyconic:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::polyconic);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::sinusoidal:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::sinusoidal);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::stereographic:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::stereographic);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::transverseCylindricalEqualArea:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::transverseCylindricalEqualArea);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::transverseMercator:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MapProjectionCoordinates(
               CoordinateType::transverseMercator);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::universalPolarStereographic:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new UPSCoordinates(
               CoordinateType::universalPolarStereographic);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               _sourceCoordinates,
               _sourceAccuracy,
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::universalTransverseMercator:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new UTMCoordinates(
               CoordinateType::universalTransverseMercator);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::usNationalGrid:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates = new MGRSorUSNGCoordinates(
               CoordinateType::usNationalGrid);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::vanDerGrinten:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates     = new MapProjectionCoordinates(
               CoordinateType::vanDerGrinten);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        case CoordinateType::webMercator:
        {
          if(i < numTargetCoordinates)
            _targetCoordinates = targetCoordinatesCollection[i];
          else
          {
            _targetCoordinates     = 
               new MapProjectionCoordinates(CoordinateType::webMercator);
            targetCoordinateExists = false;
          }
          try
          {
            convert(
               SourceOrTarget::source,
               SourceOrTarget::target,
               sourceCoordinatesCollection[i],
               sourceAccuracyCollection[i],
               *_targetCoordinates, *_targetAccuracy);
          }
          catch(CoordinateConversionException e)
          {
            _targetCoordinates->setErrorMessage(e.getMessage());
          }

          if(!targetCoordinateExists)
            targetCoordinatesCollection.push_back(_targetCoordinates);
          break;
        }
        default:   
          throw CoordinateConversionException(ErrorMessages::invalidType);
      }
      
      if(!targetAccuracyExists)
        targetAccuracyCollection.push_back( _targetAccuracy  );
    }
    else
    {
      if(i >= numTargetCoordinates)
        targetCoordinateExists = false;

      if( _sourceCoordinates )
      {
        if(!targetCoordinateExists)
          targetCoordinatesCollection.push_back(
             new CoordinateTuple( *_sourceCoordinates ) );
        else
        {
          _targetCoordinates = targetCoordinatesCollection[i];
          _targetCoordinates->set(
             _sourceCoordinates->coordinateType(),
             _sourceCoordinates->warningMessage(),
             _sourceCoordinates->errorMessage());
        }
      }
      else
      {
        if(!targetCoordinateExists)
          targetCoordinatesCollection.push_back( new CoordinateTuple() );
      }

      if(!targetAccuracyExists)
      {
        if( _sourceAccuracy )
          targetAccuracyCollection.push_back( _sourceAccuracy );
        else
        {
          Accuracy* __sourceAccuracy = new Accuracy();
          targetAccuracyCollection.push_back( __sourceAccuracy );
        }
      }
    }
  }

  if(numTargetCoordinates > num)
  {
    for(int i = num; i < numTargetCoordinates; i++)
    {
      delete targetCoordinatesCollection[i];
      targetCoordinatesCollection.pop_back();
    }
  }
  if(numTargetAccuracies > num)
  {
    for(int i = num; i < numTargetAccuracies; i++)
    {
      targetAccuracyCollection.pop_back();
    }
  }
}

// CLASSIFICATION: UNCLASSIFIED
