// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: LAMBERT
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Lambert Conformal Conic
 *    (1 or 2 Standard Parallel) projection coordinates (easting and northing in meters) defined
 *    by one standard parallel and specified scale true along that parallel, or two standard parallels.  
 *    When both standard parallel parameters
 *    are set to the same latitude value, the result is a Lambert 
 *    Conformal Conic projection with one standard parallel at the 
 *    specified latitude.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       LAMBERT_NO_ERROR           : No errors occurred in function
 *       LAMBERT_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *       LAMBERT_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *       LAMBERT_EASTING_ERROR      : Easting outside of valid range
 *                                      (depends on ellipsoid and projection
 *                                     parameters)
 *       LAMBERT_NORTHING_ERROR     : Northing outside of valid range
 *                                      (depends on ellipsoid and projection
 *                                     parameters)
 *       LAMBERT_ORIGIN_LAT_ERROR   : Origin latitude outside of valid
 *                                      range (-89 59 59.0 to 89 59 59.0 degrees)
 *                                      or within one second of 0 degrees
 *       LAMBERT_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *		   LAMBERT_SCALE_FACTOR_ERROR : Scale factor greater than minimum scale
 *                                      factor (1.0e-9)
 *       LAMBERT_A_ERROR            : Semi-major axis less than or equal to zero
 *       LAMBERT_INV_F_ERROR        : Inverse flattening outside of valid range
 *									                    (250 to 350)
 *
 *
 * REUSE NOTES
 *
 *    LAMBERT is intended for reuse by any application that performs a Lambert
 *    Conformal Conic (1 or 2 Standard Parallel) projection or its inverse.
 *    
 * REFERENCES
 *
 *    Further information on LAMBERT can be found in the Reuse Manual.
 *
 *    LAMBERT originated from:
 *                      Information Technologoy - Spatial Reference Model(SRM)
 *                      ISO/IEC FDIS 18026
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    LAMBERT has no restrictions.
 *
 * ENVIRONMENT
 *
 *    LAMBERT was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 98/2000/XP with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    03-05-2005        Original Code
 *    03-02-2007        Original C++ Code
 *    02-25-2009        Merged Lambert 1 and 2
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <math.h>
#include "LambertConformalConic.h"
#include "MapProjection5Parameters.h"
#include "MapProjection6Parameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h     - Standard C math library
 *    LambertConformalConic.h  - Is for prototype error checking
 *    CoordinateConversionException.h - Exception handler
 *    MapProjectionCoordinates.h   - defines map projection coordinates
 *    GeodeticCoordinates.h   - defines geodetic coordinates
 *    ErrorMessages.h  - Contains exception messages
 */


using namespace MSP::CCS;


/***************************************************************************/
/*                               DEFINES 
 *
 */

const double PI = 3.14159265358979323e0;   /* PI     */
const double PI_OVER_2 = (PI / 2.0);
const double PI_OVER_4 = (PI / 4.0);
const double PI_OVER_180 = (PI / 180.0);
const double MAX_LAT = (( PI *  89.99972222222222) / 180.0);  /* 89 59 59.0 degrees in radians */
const double TWO_PI = (2.0 * PI);
const double MIN_SCALE_FACTOR = 1.0e-9;
const double ONE_SECOND = 4.89e-6;


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

LambertConformalConic::LambertConformalConic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing, double scaleFactor ) :
  CoordinateSystem(),
  coordinateType( CoordinateType::lambertConformalConic1Parallel ),
  es( 0.08181919084262188000 ),
  es_OVER_2( .040909595421311 ),
  Lambert_1_n( 0.70710678118655 ),
  Lambert_1_rho0( 6388838.2901212 ),
  Lambert_1_rho_olat( 6388838.2901211 ),
  Lambert_1_t0( 0.41618115138974 ),
  Lambert_Origin_Long( 0.0 ),
  Lambert_Origin_Latitude( (45.0 * PI / 180.0) ),
  Lambert_False_Easting( 0.0 ),
  Lambert_False_Northing( 0.0 ),
  Lambert_Scale_Factor( 1.0 ),
  Lambert_2_Origin_Lat( (45 * PI / 180) ),
  Lambert_2_Std_Parallel_1( (40 * PI / 180) ),
  Lambert_2_Std_Parallel_2( (50 * PI / 180) ),
  Lambert_Delta_Easting( 400000000.0 ),
  Lambert_Delta_Northing( 400000000.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * Lambert Conformal Conic (1 Standard Parallel) projection parameters as
 * inputs, and sets the corresponding state variables. 
 * If any errors occur, an exception is thrown with a description of the error.
 *
 *   ellipsoidSemiMajorAxis : Semi-major axis of ellipsoid, in meters   (input)
 *   ellipsoidFlattening    : Flattening of ellipsoid                   (input)
 *   centralMeridian        : Longitude of origin, in radians           (input)
 *   originLatitude         : Latitude of origin, in radians            (input)
 *   falseEasting           : False easting, in meters                  (input)
 *   falseNorthing          : False northing, in meters                 (input)
 *   scaleFactor            : Projection scale factor                   (input)
 *
 */

  double es2;
  double inv_f = 1 / ellipsoidFlattening;

  if (ellipsoidSemiMajorAxis <= 0.0)
  { /* Semi-major axis must be greater than zero */
    throw CoordinateConversionException( ErrorMessages::semiMajorAxis );
  }
  if ((inv_f < 250) || (inv_f > 350))
  { /* Inverse flattening must be between 250 and 350 */
    throw CoordinateConversionException( ErrorMessages::ellipsoidFlattening );
  }
  if ((centralMeridian < -PI) || (centralMeridian > TWO_PI))
  { /* Origin Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::centralMeridian );
  }

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening = ellipsoidFlattening;

  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Lambert_Origin_Long = centralMeridian;
  Lambert_False_Easting = falseEasting;

  es2 = 2.0 * flattening - flattening * flattening;
  es = sqrt(es2);
  es_OVER_2 = es / 2.0;

  CommonParameters* parameters = setCommonLambert1StandardParallelParameters(
     originLatitude, falseNorthing, scaleFactor);    

  Lambert_1_n = parameters->_lambertN;                         
  Lambert_1_rho0 = parameters->_lambertRho0;                       
  Lambert_1_rho_olat = parameters->_lambertRhoOlat;
  Lambert_1_t0 = parameters->_lambertT0;
  Lambert_Origin_Latitude = parameters->_lambertOriginLatitude; 
  Lambert_False_Northing = parameters->_lambertFalseNorthing;             
  Lambert_Scale_Factor = parameters->_lambertScaleFactor;  
  
  Lambert_2_Origin_Lat = parameters->_lambertOriginLatitude; 

  double sinOlat = sin(Lambert_Origin_Latitude);
  double esSinOlat = es * sinOlat;//Math.sin(lambertOriginLat);
  double w0 = sqrt(1 - es2 * sinOlat * sinOlat);
  double f0 = cos(Lambert_Origin_Latitude) / (w0 * pow(Lambert_1_t0, Lambert_1_n));
  double c = Lambert_Scale_Factor * f0;

  double phi = 0.0;
  double tempPhi = 1.0;
  Lambert_2_Std_Parallel_1 = calculateLambert2StandardParallel(es2, phi, tempPhi, c);
  
  phi = 89.0 * PI_OVER_180;
  tempPhi = 0.0;
  Lambert_2_Std_Parallel_2 = calculateLambert2StandardParallel(es2, phi, tempPhi, c);

  delete parameters;
  parameters = 0;
}


LambertConformalConic::LambertConformalConic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double standardParallel1, double standardParallel2, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
  coordinateType( CoordinateType::lambertConformalConic2Parallels ),
  es( 0.081819190842621 ),
  es_OVER_2( 0.040909595421311 ),
  Lambert_1_n( 0.70710678118655 ),
  Lambert_1_rho0( 6388838.2901212 ),
  Lambert_1_rho_olat( 6388838.2901211 ),
  Lambert_1_t0( 0.41618115138974 ),
  Lambert_Origin_Long( 0.0 ),
  Lambert_Origin_Latitude( (45 * PI / 180) ),
  Lambert_False_Easting( 0.0 ),
  Lambert_False_Northing( 0.0 ),
  Lambert_Scale_Factor( 1.0 ),
  Lambert_2_Origin_Lat( (45 * PI / 180) ),
  Lambert_2_Std_Parallel_1( (40 * PI / 180) ),
  Lambert_2_Std_Parallel_2( (50 * PI / 180) ),
  Lambert_Delta_Easting( 400000000.0 ),
  Lambert_Delta_Northing( 400000000.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * Lambert Conformal Conic (2 Standard Parallel) projection parameters as inputs, and sets the
 * corresponding state variables.  If any errors occur, an exception is thrown 
 * with a description of the error.
 *
 *   ellipsoidSemiMajorAxis   : Semi-major axis of ellipsoid, in meters   (input)
 *   ellipsoidFlattening      : Flattening of ellipsoid				            (input)
 *   centralMeridian          : Longitude of origin, in radians           (input)
 *   originLatitude           : Latitude of origin, in radians            (input)
 *   standardParallel1        : First standard parallel, in radians       (input)
 *   standardParallel2        : Second standard parallel, in radians      (input)
 *   falseEasting             : False easting, in meters                  (input)
 *   falseNorthing            : False northing, in meters                 (input)
 *
 *   Note that when the two standard parallel parameters are both set to the 
 *   same latitude value, the result is a Lambert Conformal Conic projection 
 *   with one standard parallel at the specified latitude.
 */

  double es2;
  double es_sin;
  double t0;
  double t1;
  double t2;
  double t_olat;
  double m0;
  double m1;
  double m2;
  double m_olat;
  double n;                      /* Ratio of angle between meridians */
  double const_value;
  double Lambert_lat0;
  double Lambert_k0;
  double Lambert_false_northing;
  double inv_f = 1 / ellipsoidFlattening;

  if (ellipsoidSemiMajorAxis <= 0.0)
  { /* Semi-major axis must be greater than zero */
    throw CoordinateConversionException( ErrorMessages::semiMajorAxis );
  }
  if ((inv_f < 250) || (inv_f > 350))
  { /* Inverse flattening must be between 250 and 350 */
    throw CoordinateConversionException( ErrorMessages::ellipsoidFlattening );
  }
  if ((originLatitude < -MAX_LAT) || (originLatitude > MAX_LAT))
  { /* Origin Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::originLatitude );
  }
  if ((standardParallel1 < -MAX_LAT) || (standardParallel1 > MAX_LAT))
  { /* First Standard Parallel out of range */
    throw CoordinateConversionException( ErrorMessages::standardParallel1 );
  }
  if ((standardParallel2 < -MAX_LAT) || (standardParallel2 > MAX_LAT))
  { /* Second Standard Parallel out of range */
    throw CoordinateConversionException( ErrorMessages::standardParallel2 );
  }
  if ((standardParallel1 == 0) && (standardParallel2 == 0))
  { /* First & Second Standard Parallels are both 0 */
    throw CoordinateConversionException( ErrorMessages::standardParallel1_2 );
  }
  if (standardParallel1 == -standardParallel2)
  { /* Parallels are the negation of each other */
    throw CoordinateConversionException(
       ErrorMessages::standardParallelHemisphere );
  }
  if ((centralMeridian < -PI) || (centralMeridian > TWO_PI))
  { /* Central meridian out of range */
    throw CoordinateConversionException( ErrorMessages::centralMeridian );
  }

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening = ellipsoidFlattening;

  Lambert_2_Origin_Lat = originLatitude;
  Lambert_2_Std_Parallel_1 = standardParallel1;
  Lambert_2_Std_Parallel_2 = standardParallel2;

  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Lambert_Origin_Long = centralMeridian;
  Lambert_False_Easting = falseEasting;

    es2 = 2 * flattening - flattening * flattening;
    es = sqrt(es2);
    es_OVER_2 = es / 2.0;

  if (fabs(Lambert_2_Std_Parallel_1 - Lambert_2_Std_Parallel_2) > 1.0e-10)
  {
    es_sin = esSin(sin(originLatitude));
    m_olat = lambertM(cos(originLatitude), es_sin);
    t_olat = lambertT(originLatitude, es_sin);

    es_sin = esSin(sin(Lambert_2_Std_Parallel_1));
    m1 = lambertM(cos(Lambert_2_Std_Parallel_1), es_sin);
    t1 = lambertT(Lambert_2_Std_Parallel_1, es_sin);

    es_sin = esSin(sin(Lambert_2_Std_Parallel_2));
    m2 = lambertM(cos(Lambert_2_Std_Parallel_2), es_sin);
    t2 = lambertT(Lambert_2_Std_Parallel_2, es_sin);

    n = log(m1 / m2) / log(t1 / t2);

    Lambert_lat0 = asin(n);
  
    es_sin = esSin(sin(Lambert_lat0));
    m0 = lambertM(cos(Lambert_lat0), es_sin);
    t0 = lambertT(Lambert_lat0, es_sin);

    Lambert_k0 = (m1 / m0) * (pow(t0 / t1, n));

    const_value = ((semiMajorAxis * m2) / (n * pow(t2, n)));
    
    Lambert_false_northing =
       (const_value * pow(t_olat, n)) - (const_value * pow(t0, n))
       + falseNorthing;
  }
  else
  {
    Lambert_lat0 = Lambert_2_Std_Parallel_1;
    Lambert_k0 = 1.0;
    Lambert_false_northing = falseNorthing;
  }

  CommonParameters* parameters = setCommonLambert1StandardParallelParameters(
     Lambert_lat0, Lambert_false_northing, Lambert_k0);

  Lambert_1_n             = parameters->_lambertN;                         
  Lambert_1_rho0          = parameters->_lambertRho0;                       
  Lambert_1_rho_olat      = parameters->_lambertRhoOlat;
  Lambert_1_t0            = parameters->_lambertT0;
  Lambert_Origin_Latitude = parameters->_lambertOriginLatitude; 
  Lambert_False_Northing  = parameters->_lambertFalseNorthing;             
  Lambert_Scale_Factor    = parameters->_lambertScaleFactor;     
  
  delete parameters;
  parameters = 0;
}

LambertConformalConic::LambertConformalConic( const LambertConformalConic &lcc )
{
  coordinateType           = lcc.coordinateType;
  semiMajorAxis            = lcc.semiMajorAxis;
  flattening               = lcc.flattening;
  es                       = lcc.es;
  es_OVER_2                = lcc.es_OVER_2;
  Lambert_1_n              = lcc.Lambert_1_n;
  Lambert_1_rho0           = lcc.Lambert_1_rho0;
  Lambert_1_rho_olat       = lcc.Lambert_1_rho_olat;
  Lambert_1_t0             = lcc.Lambert_1_t0;
  Lambert_Origin_Long      = lcc.Lambert_Origin_Long;
  Lambert_Origin_Latitude  = lcc.Lambert_Origin_Latitude;
  Lambert_False_Easting    = lcc.Lambert_False_Easting;
  Lambert_False_Northing   = lcc.Lambert_False_Northing;
  Lambert_Scale_Factor     = lcc.Lambert_Scale_Factor;
  Lambert_2_Origin_Lat     = lcc.Lambert_2_Origin_Lat;
  Lambert_2_Std_Parallel_1 = lcc.Lambert_2_Std_Parallel_1;
  Lambert_2_Std_Parallel_2 = lcc.Lambert_2_Std_Parallel_2;
  Lambert_Delta_Easting    = lcc.Lambert_Delta_Easting;
  Lambert_Delta_Northing   = lcc.Lambert_Delta_Northing;
}


LambertConformalConic::~LambertConformalConic()
{
}


LambertConformalConic& LambertConformalConic::operator=(
   const LambertConformalConic &lcc )
{
  if( this != &lcc )
  {
    coordinateType           = lcc.coordinateType;
    semiMajorAxis            = lcc.semiMajorAxis;
    flattening               = lcc.flattening;
    es                       = lcc.es;
    es_OVER_2                = lcc.es_OVER_2;
    Lambert_1_n              = lcc.Lambert_1_n;
    Lambert_1_rho0           = lcc.Lambert_1_rho0;
    Lambert_1_rho_olat       = lcc.Lambert_1_rho_olat;
    Lambert_1_t0             = lcc.Lambert_1_t0;
    Lambert_Origin_Long      = lcc.Lambert_Origin_Long;
    Lambert_Origin_Latitude  = lcc.Lambert_Origin_Latitude;
    Lambert_False_Easting    = lcc.Lambert_False_Easting;
    Lambert_False_Northing   = lcc.Lambert_False_Northing;
    Lambert_Scale_Factor     = lcc.Lambert_Scale_Factor;
    Lambert_2_Origin_Lat     = lcc.Lambert_2_Origin_Lat;
    Lambert_2_Std_Parallel_1 = lcc.Lambert_2_Std_Parallel_1;
    Lambert_2_Std_Parallel_2 = lcc.Lambert_2_Std_Parallel_2;
    Lambert_Delta_Easting    = lcc.Lambert_Delta_Easting;
    Lambert_Delta_Northing   = lcc.Lambert_Delta_Northing;
  }

  return *this;
}


MapProjection5Parameters* LambertConformalConic::get1StandardParallelParameters() const
{
/*                         
 * The function get1StandardParallelParameters returns the current ellipsoid
 * parameters and Lambert Conformal Conic (1 Standard Parallel) projection parameters.
 *
 *   ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters (output)
 *   ellipsoidFlattening     : Flattening of ellipsoid		       (output)
 *   centralMeridian         : Longitude of origin, in radians         (output)
 *   originLatitude          : Latitude of origin, in radians          (output)
 *   falseEasting            : False easting, in meters                (output)
 *   falseNorthing           : False northing, in meters               (output)
 *   scaleFactor             : Projection scale factor                 (output) 
 */

  return new MapProjection5Parameters( CoordinateType::lambertConformalConic1Parallel, Lambert_Origin_Long, Lambert_Origin_Latitude, Lambert_Scale_Factor, Lambert_False_Easting, Lambert_False_Northing );
}


MapProjection6Parameters* LambertConformalConic::get2StandardParallelParameters() const
{
/*                         
 * The function get2StandardParallelParameters returns the current ellipsoid
 * parameters and Lambert Conformal Conic (2 Standard Parallel) projection parameters.
 *
 *   ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters  (output)
 *   ellipsoidFlattening     : Flattening of ellipsoid                  (output)
 *   centralMeridian         : Longitude of origin, in radians          (output)
 *   originLatitude          : Latitude of origin, in radians           (output)
 *   standardParallel1       : First standard parallel, in radians      (output)
 *   standardParallel2       : Second standard parallel, in radians     (output)
 *   falseEasting            : False easting, in meters                 (output)
 *   falseNorthing           : False northing, in meters                (output)
 */

  return new MapProjection6Parameters(
     CoordinateType::lambertConformalConic2Parallels,
     Lambert_Origin_Long, Lambert_2_Origin_Lat,
     Lambert_2_Std_Parallel_1, Lambert_2_Std_Parallel_2,
     Lambert_False_Easting, Lambert_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* LambertConformalConic::convertFromGeodetic(
   MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts Geodetic (latitude and
 * longitude) coordinates to Lambert Conformal Conic (1 or 2 Standard Parallel)
 * projection (easting and northing) coordinates, according to the current
 * ellipsoid and Lambert Conformal Conic (1 or 2 Standard Parallel) projection
 * parameters.  If any errors occur, an exception is thrown with a
 * description of the error.
 *
 *    longitude        : Longitude, in radians                        (input)
 *    latitude         : Latitude, in radians                         (input)
 *    easting          : Easting (X), in meters                       (output)
 *    northing         : Northing (Y), in meters                      (output)
 */

  double t;
  double rho;
  double dlam;
  double theta;

  double longitude = geodeticCoordinates->longitude();
  double latitude  = geodeticCoordinates->latitude();

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  {  /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  {  /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude );
  }

  if (fabs(fabs(latitude) - PI_OVER_2) > 1.0e-10)
  {
    t = lambertT(latitude, esSin(sin(latitude)));
    rho = Lambert_1_rho0 * pow(t / Lambert_1_t0, Lambert_1_n);
  }
  else
  {
    if ((latitude * Lambert_1_n) <= 0)
    { /* Point can not be projected */
      throw CoordinateConversionException( ErrorMessages::latitude );
    }
    rho = 0.0;
  }

  dlam = longitude - Lambert_Origin_Long;

  if (dlam > PI)
  {
    dlam -= TWO_PI;
  }
  if (dlam < -PI)
  {
    dlam += TWO_PI;
  }

  theta = Lambert_1_n * dlam;

  double easting = rho * sin(theta) + Lambert_False_Easting;
  double northing = Lambert_1_rho_olat - rho * cos(theta) + Lambert_False_Northing;

  return new MapProjectionCoordinates( coordinateType, easting, northing );
}


MSP::CCS::GeodeticCoordinates* LambertConformalConic::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Lambert Conformal
 * Conic (1 or 2 Standard Parallel) projection (easting and northing) coordinates to Geodetic
 * (latitude and longitude) coordinates, according to the current ellipsoid
 * and Lambert Conformal Conic (1 or 2 Standard Parallel) projection parameters.  If any 
 * errors occur, an exception is thrown with a description of the error.
 *
 *    easting          : Easting (X), in meters                       (input)
 *    northing         : Northing (Y), in meters                      (input)
 *    longitude        : Longitude, in radians                        (output)
 *    latitude         : Latitude, in radians                         (output)
 */

  double dx;
  double dy;
  double rho;
  double rho_olat_MINUS_dy;
  double t;
  double PHI;
  double es_sin;
  double tempPHI = 0.0;
  double theta = 0.0;
  double tolerance = 4.85e-10;
  int count = 30;
  double longitude, latitude;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Lambert_False_Easting - Lambert_Delta_Easting))
      ||(easting > (Lambert_False_Easting + Lambert_Delta_Easting)))
  { /* Easting out of range  */
    throw CoordinateConversionException( ErrorMessages::easting );
  }
  if ((northing < (Lambert_False_Northing - Lambert_Delta_Northing))
      || (northing > (Lambert_False_Northing + Lambert_Delta_Northing)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing );
  }

  dy                = northing - Lambert_False_Northing;
  dx                = easting  - Lambert_False_Easting;
  rho_olat_MINUS_dy = Lambert_1_rho_olat - dy;
  rho = sqrt(dx * dx + (rho_olat_MINUS_dy) * (rho_olat_MINUS_dy));

  if (Lambert_1_n < 0.0)
  {
    rho *= -1.0;
    dx *= -1.0;
    rho_olat_MINUS_dy *= -1.0;
  }

  if (rho != 0.0)
  {
    theta = atan2(dx, rho_olat_MINUS_dy) / Lambert_1_n;
    t = Lambert_1_t0 * pow(rho / Lambert_1_rho0, 1 / Lambert_1_n);
    PHI = PI_OVER_2 - 2.0 * atan(t);
    while (fabs(PHI - tempPHI) > tolerance && count)
    {
      tempPHI = PHI;
      es_sin = esSin(sin(PHI));
      PHI = PI_OVER_2 - 2.0 * atan(t * pow((1.0 - es_sin) / (1.0 + es_sin), es_OVER_2));
      count --;
    }

    if(!count)
      throw CoordinateConversionException( ErrorMessages::northing );

    latitude = PHI;
    longitude = theta + Lambert_Origin_Long;

    if (fabs(latitude) < 2.0e-7)  /* force lat to 0 to avoid -0 degrees */
      latitude = 0.0;
    if (latitude > PI_OVER_2)  /* force distorted values to 90, -90 degrees */
      latitude = PI_OVER_2;
    else if (latitude < -PI_OVER_2)
      latitude = -PI_OVER_2;

    if (longitude > PI)
    {
      if (longitude - PI < 3.5e-6) 
        longitude = PI;
      else
        longitude -= TWO_PI;
    }
    if (longitude < -PI)
    {
      if (fabs(longitude + PI) < 3.5e-6)
        longitude = -PI;
      else
        longitude += TWO_PI;
    }

    if (fabs(longitude) < 2.0e-7)  /* force lon to 0 to avoid -0 degrees */
      longitude = 0.0;
    if (longitude > PI)  /* force distorted values to 180, -180 degrees */
      longitude = PI;
    else if (longitude < -PI)
      longitude = -PI;
  }
  else
  {
    if (Lambert_1_n > 0.0)
      latitude = PI_OVER_2;
    else
      latitude = -PI_OVER_2;
    longitude = Lambert_Origin_Long;
  }

  return new GeodeticCoordinates( CoordinateType::geodetic, longitude, latitude );
}


LambertConformalConic::CommonParameters*
LambertConformalConic::setCommonLambert1StandardParallelParameters(
   double originLatitude, double falseNorthing, double scaleFactor )
{
/**
 * Receives the
 * Lambert Conformal Conic (1 Standard Parallel) projection parameters as inputs, and sets the
 * corresponding state variables.  If any errors occur, an exception
 * is thrown with a description of the error.
 *
 *  @param    originLatitude          latitude of origin, in radians 
 *  @param    falseNorthing             false northing, in meters               
 *  @param    scaleFactor               projection scale factor                  
 *  @throws   CoordinateConversionException    invalid input parameters
 */

  double _esSin;
  double m0;

  if (((originLatitude < -MAX_LAT) || (originLatitude > MAX_LAT)) ||
       (originLatitude > -ONE_SECOND) && (originLatitude < ONE_SECOND))
  { /* Origin Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::originLatitude );
  }
  if (scaleFactor < MIN_SCALE_FACTOR)
  {
    throw CoordinateConversionException( ErrorMessages::scaleFactor );
  }

  CommonParameters* parameters = new CommonParameters();
  
  parameters->_lambertOriginLatitude = originLatitude;
  parameters->_lambertFalseNorthing  = falseNorthing;
  parameters->_lambertScaleFactor    = scaleFactor;

  parameters->_lambertN = sin(originLatitude);

  _esSin = esSin(sin(originLatitude));

  m0 = cos(originLatitude) / sqrt(1.0 - _esSin * _esSin);
  parameters->_lambertT0 = lambertT(originLatitude, _esSin);

  parameters->_lambertRho0 =
     semiMajorAxis * scaleFactor * m0 / parameters->_lambertN;

  parameters->_lambertRhoOlat = parameters->_lambertRho0;
  
  return parameters;
}


double LambertConformalConic::calculateLambert2StandardParallel(
   double es2, double phi, double tempPhi, double c)
{
/*
 * Calculates the
 * Lambert Conformal Conic (2 Standard Parallel) standard parallel value.
 *
 */
  double tolerance = 1.0e-11;
  int count = 30;
  while (fabs(phi - tempPhi) > tolerance && count > 0)
  {
    tempPhi = phi;
    double sinPhi = sin(phi);
    double esSinPhi = es * sinPhi;
    double tPhi = lambertT(phi, esSinPhi);   
    double wPhi = sqrt(1 - es2 * sinPhi * sinPhi);
    double fPhi = cos(phi) / (wPhi * pow(tPhi, Lambert_1_n));
    double fprPhi = ((Lambert_1_n - sinPhi) * (1 - es2)) / (pow(wPhi, 3) * pow(tPhi, Lambert_1_n));

    phi = phi + (c - fPhi) / fprPhi;
    
    count--;
  }   
  
  return phi;
}


double LambertConformalConic::lambertM( double clat, double essin )
{
  return clat / sqrt(1.0 - essin * essin);
}


double LambertConformalConic::lambertT( double lat, double essin )
{
  return tan(PI_OVER_4 - lat / 2) /	pow((1.0 - essin) / (1.0 + essin), es_OVER_2);
}


double LambertConformalConic::esSin( double sinlat )
{
  return es * sinlat;
}



// CLASSIFICATION: UNCLASSIFIED
