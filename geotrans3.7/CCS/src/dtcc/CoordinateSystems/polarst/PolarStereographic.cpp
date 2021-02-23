// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: POLAR STEREOGRAPHIC 
 *
 *
 * ABSTRACT
 *
 *    This component provides conversions between geodetic (latitude and
 *    longitude) coordinates and Polar Stereographic (easting and northing) 
 *    coordinates.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid 
 *    value is found the error code is combined with the current error code 
 *    using the bitwise or.  This combining allows multiple error codes to 
 *    be returned. The possible error codes are:
 *
 *          POLAR_NO_ERROR          : No errors occurred in function
 *          POLAR_LAT_ERROR         : Latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *          POLAR_LON_ERROR         : Longitude outside of valid range
 *                                     (-180 to 360 degrees) 
 *          POLAR_ORIGIN_LAT_ERROR  : Latitude of true scale outside of valid
 *                                     range (-90 to 90 degrees)
 *          POLAR_ORIGIN_LON_ERROR  : Longitude down from pole outside of valid
 *                                     range (-180 to 360 degrees)
 *          POLAR_EASTING_ERROR     : Easting outside of valid range,
 *                                     depending on ellipsoid and
 *                                     projection parameters
 *          POLAR_NORTHING_ERROR    : Northing outside of valid range,
 *                                     depending on ellipsoid and
 *                                    projection parameters
 *          POLAR_RADIUS_ERROR      : Coordinates too far from pole,
 *                                     depending on ellipsoid and
 *                                     projection parameters
 *          POLAR_A_ERROR          : Semi-major axis less than or equal to zero
 *          POLAR_INV_F_ERROR      : Inverse flattening outside of valid range
 *                                     (250 to 350)
 *
 *
 * REUSE NOTES
 *
 *    POLAR STEREOGRAPHIC is intended for reuse by any application that  
 *    performs a Polar Stereographic projection.
 *
 *
 * REFERENCES
 *
 *    Further information on POLAR STEREOGRAPHIC can be found in the
 *    Reuse Manual.
 *
 *
 *    POLAR STEREOGRAPHIC originated from :
 *                                U.S. Army Topographic Engineering Center
 *                                Geospatial Information Division
 *                                7701 Telegraph Road
 *                                Alexandria, VA  22310-3864
 *
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 *
 * RESTRICTIONS
 *
 *    POLAR STEREOGRAPHIC has no restrictions.
 *
 *
 * ENVIRONMENT
 *
 *    POLAR STEREOGRAPHIC was tested and certified in the following
 *    environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Window 95 with MS Visual C++, version 6
 *
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    2-27-07          Original Code
 *
 *
 */


/************************************************************************/
/*
 *                               INCLUDES
 */

#include <stdio.h>
#include <math.h>
#include "PolarStereographic.h"
#include "PolarStereographicStandardParallelParameters.h"
#include "PolarStereographicScaleFactorParameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h     - Standard C math library
 *    PolarStereographic.h  - Is for prototype error checking
 *    MapProjectionCoordinates.h   - defines map projection coordinates
 *    GeodeticCoordinates.h   - defines geodetic coordinates
 *    CoordinateConversionException.h - Exception handler
 *    ErrorMessages.h  - Contains exception messages
 */


using namespace MSP::CCS;


/************************************************************************/
/*                               DEFINES
 *
 */

const double PI = 3.14159265358979323e0;       /* PI     */
const double PI_OVER_2 = (PI / 2.0);           
const double PI_OVER_4 = (PI / 4.0);           
const double TWO_PI = (2.0 * PI);

#define MIN_SCALE_FACTOR  0.1
#define MAX_SCALE_FACTOR  3.0


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

PolarStereographic::PolarStereographic(
   double ellipsoidSemiMajorAxis,
   double ellipsoidFlattening,
   double centralMeridian,
   double standardParallel,
   double falseEasting,
   double falseNorthing ) :
  CoordinateSystem(),
  coordinateType( CoordinateType::polarStereographicStandardParallel ),
  es( 0.08181919084262188000 ),
  es_OVER_2( .040909595421311 ),
  Southern_Hemisphere( 0 ),
  Polar_tc( 1.0 ),
  Polar_k90( 1.0033565552493 ),
  Polar_a_mc( 6378137.0 ),                 
  two_Polar_a( 12756274.0 ),
  Polar_Central_Meridian( 0.0 ),
  Polar_Standard_Parallel( ((PI * 90) / 180) ),
  Polar_False_Easting( 0.0 ),
  Polar_False_Northing( 0.0 ),
  Polar_Scale_Factor( 1.0 ),
  Polar_Delta_Easting( 12713601.0 ),
  Polar_Delta_Northing( 12713601.0 )
{
/*  
 * The constructor receives the ellipsoid
 * parameters and Polar Stereograpic (Standard Parallel) projection parameters as inputs, and
 * sets the corresponding state variables.  If any errors occur, an 
 * exception is thrown with a description of the error.
 *
 *  ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters (input)
 *  ellipsoidFlattening     : Flattening of ellipsoid                 (input)
 *  centralMeridian         : Longitude down from pole, in radians    (input)
 *  standardParallel        : Latitude of true scale, in radians      (input)
 *  falseEasting       : Easting (X) at center of projection, in meters  (input)
 *  falseNorthing      : Northing (Y) at center of projection, in meters (input)
 */

  double es2;
  double slat, sinolat, cosolat;
  double essin;
  double one_PLUS_es, one_MINUS_es;
  double one_PLUS_es_sinolat, one_MINUS_es_sinolat;
  double pow_es;
  double inv_f = 1 / ellipsoidFlattening;

  if (ellipsoidSemiMajorAxis <= 0.0)
  { /* Semi-major axis must be greater than zero */
    throw CoordinateConversionException( ErrorMessages::semiMajorAxis  );
  }
  if ((inv_f < 250) || (inv_f > 350))
  { /* Inverse flattening must be between 250 and 350 */
    throw CoordinateConversionException( ErrorMessages::ellipsoidFlattening  );
  }
  if ((standardParallel < -PI_OVER_2) || (standardParallel > PI_OVER_2))
  { /* Origin Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::originLatitude  );
  }
  if ((centralMeridian < -PI) || (centralMeridian > TWO_PI))
  { /* Origin Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::centralMeridian  );
  }

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening = ellipsoidFlattening;

  two_Polar_a = 2.0 * semiMajorAxis;

  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  if (standardParallel < 0)
  {
    Southern_Hemisphere = 1;
    Polar_Standard_Parallel = -standardParallel;
    Polar_Central_Meridian = -centralMeridian;
  }
  else
  {
    Southern_Hemisphere = 0;
    Polar_Standard_Parallel = standardParallel;
    Polar_Central_Meridian = centralMeridian;
  }
  Polar_False_Easting = falseEasting;
  Polar_False_Northing = falseNorthing;

  es2 = 2 * flattening - flattening * flattening;
  es = sqrt(es2);
  es_OVER_2 = es / 2.0;

  if (fabs(fabs(Polar_Standard_Parallel) - PI_OVER_2) > 1.0e-10)
  {
    sinolat = sin(Polar_Standard_Parallel);
    essin = es * sinolat;
    pow_es = polarPow(essin);
    cosolat = cos(Polar_Standard_Parallel);
    double mc = cosolat / sqrt(1.0 - essin * essin);
    Polar_a_mc = semiMajorAxis * mc;
    Polar_tc = tan(PI_OVER_4 - Polar_Standard_Parallel / 2.0) / pow_es;
  }

  one_PLUS_es = 1.0 + es;
	one_MINUS_es = 1.0 - es;
	Polar_k90 = sqrt(pow(one_PLUS_es, one_PLUS_es) * pow(one_MINUS_es, one_MINUS_es));

  slat = sin(fabs(standardParallel));
  one_PLUS_es_sinolat = 1.0 + es * slat;
  one_MINUS_es_sinolat = 1.0 - es * slat;
  Polar_Scale_Factor = ((1 + slat) / 2) * (Polar_k90 / sqrt(pow(one_PLUS_es_sinolat, one_PLUS_es) * pow(one_MINUS_es_sinolat, one_MINUS_es)));

  /* Calculate Radius */
  GeodeticCoordinates tempGeodeticCoordinates = GeodeticCoordinates( CoordinateType::geodetic, centralMeridian, 0 );
  //MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( (GeodeticCoordinates&) GeodeticCoordinates( CoordinateType::geodetic, centralMeridian, 0 ) );
  MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &tempGeodeticCoordinates );
  Polar_Delta_Northing = tempCoordinates->northing();
  delete tempCoordinates;

  if(Polar_False_Northing)
    Polar_Delta_Northing -= Polar_False_Northing;
  if (Polar_Delta_Northing < 0)
    Polar_Delta_Northing = -Polar_Delta_Northing;
  Polar_Delta_Northing *= 1.01;

  Polar_Delta_Easting = Polar_Delta_Northing;
}


PolarStereographic::PolarStereographic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double scaleFactor, char hemisphere, double falseEasting, double falseNorthing ) :
  CoordinateSystem(),
  coordinateType( CoordinateType::polarStereographicScaleFactor ),
  es( 0.08181919084262188000 ),
  es_OVER_2( .040909595421311 ),
  Southern_Hemisphere( 0 ),
  Polar_tc( 1.0 ),
  Polar_k90( 1.0033565552493 ),
  Polar_a_mc( 6378137.0 ),                 
  two_Polar_a( 12756274.0 ),
  Polar_Central_Meridian( 0.0 ),
  Polar_Standard_Parallel( ((PI * 90) / 180) ),
  Polar_False_Easting( 0.0 ),
  Polar_False_Northing( 0.0 ),
  Polar_Scale_Factor( 1.0 ),
  Polar_Delta_Easting( 12713601.0 ),
  Polar_Delta_Northing( 12713601.0 )
{
/*  
 * The constructor receives the ellipsoid
 * parameters and Polar Stereograpic (Scale Factor) projection parameters
 * as inputs, and sets the corresponding state variables.  If any errors occur,
 * an exception is thrown with a description of the error.
 *
 *  ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters  (input)
 *  ellipsoidFlattening     : Flattening of ellipsoid                  (input)
 *  centralMeridian         : Longitude down from pole, in radians     (input)
 *  scaleFactor             : Scale Factor                             (input)
 *  falseEasting     : Easting (X) at center of projection, in meters  (input)
 *  falseNorthing    : Northing (Y) at center of projection, in meters (input)
 */

  double es2;
  double sinolat, cosolat;
  double essin;
  double pow_es;
  double mc;                    
  double one_PLUS_es, one_MINUS_es;
  double one_PLUS_es_sk, one_MINUS_es_sk;
  double sk, sk_PLUS_1;
  double tolerance = 1.0e-15;
  int count = 30;
  double inv_f = 1 / ellipsoidFlattening;

  if (ellipsoidSemiMajorAxis <= 0.0)
  { /* Semi-major axis must be greater than zero */
    throw CoordinateConversionException( ErrorMessages::semiMajorAxis  );
  }
  if ((inv_f < 250) || (inv_f > 350))
  { /* Inverse flattening must be between 250 and 350 */
    throw CoordinateConversionException( ErrorMessages::ellipsoidFlattening  );
  }
  if ((scaleFactor < MIN_SCALE_FACTOR) || (scaleFactor > MAX_SCALE_FACTOR))
  {
    throw CoordinateConversionException( ErrorMessages::scaleFactor  );
  }
  if ((centralMeridian < -PI) || (centralMeridian > TWO_PI))
  { /* Origin Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::centralMeridian  );
  }
  if ((hemisphere != 'N') && (hemisphere != 'S'))
    throw CoordinateConversionException( ErrorMessages::hemisphere  );

  semiMajorAxis        = ellipsoidSemiMajorAxis;
  flattening           = ellipsoidFlattening;
  Polar_Scale_Factor   = scaleFactor;
  Polar_False_Easting  = falseEasting;
  Polar_False_Northing = falseNorthing;

  two_Polar_a = 2.0 * semiMajorAxis;
  es2         = 2 * flattening - flattening * flattening;
  es          = sqrt(es2);
  es_OVER_2   = es / 2.0;

  one_PLUS_es = 1.0 + es;
  one_MINUS_es = 1.0 - es;
  Polar_k90 = sqrt(pow(one_PLUS_es, one_PLUS_es) * pow(one_MINUS_es, one_MINUS_es));

  sk = 0;
  sk_PLUS_1 = -1 + 2 * Polar_Scale_Factor;
  while (fabs(sk_PLUS_1 - sk) > tolerance && count)
  {
    sk = sk_PLUS_1;
    one_PLUS_es_sk = 1.0 + es * sk;
    one_MINUS_es_sk = 1.0 - es * sk;
    sk_PLUS_1 = ((2 * Polar_Scale_Factor * sqrt(pow(one_PLUS_es_sk, one_PLUS_es) * pow(one_MINUS_es_sk, one_MINUS_es))) / Polar_k90) - 1;
    count --;
  }

  if(!count)
    throw CoordinateConversionException( ErrorMessages::originLatitude );

  double standardParallel = 0.0;
  if(sk_PLUS_1 >= -1.0 && sk_PLUS_1 <= 1.0)
    standardParallel = asin(sk_PLUS_1);
  else
    throw CoordinateConversionException( ErrorMessages::originLatitude );

  if (hemisphere == 'S')
    standardParallel *= -1.0;

  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  if (standardParallel < 0)
  {
    Southern_Hemisphere = 1;
    Polar_Standard_Parallel = -standardParallel;
    Polar_Central_Meridian = -centralMeridian;
  }
  else
  {
    Southern_Hemisphere = 0;
    Polar_Standard_Parallel = standardParallel;
    Polar_Central_Meridian = centralMeridian;
  }

  sinolat = sin(Polar_Standard_Parallel);

  if (fabs(fabs(Polar_Standard_Parallel) - PI_OVER_2) > 1.0e-10)
  {
    essin = es * sinolat;
    pow_es = polarPow(essin);
    cosolat = cos(Polar_Standard_Parallel);
    mc = cosolat / sqrt(1.0 - essin * essin);
    Polar_a_mc = semiMajorAxis * mc;
    Polar_tc = tan(PI_OVER_4 - Polar_Standard_Parallel / 2.0) / pow_es;
  }

  /* Calculate Radius */
  GeodeticCoordinates tempGeodeticCoordinates = GeodeticCoordinates( CoordinateType::geodetic, centralMeridian, 0 ) ;
  MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &tempGeodeticCoordinates );
  Polar_Delta_Northing = tempCoordinates->northing();
  delete tempCoordinates;

  if(Polar_False_Northing)
    Polar_Delta_Northing -= Polar_False_Northing;
  if (Polar_Delta_Northing < 0)
    Polar_Delta_Northing = -Polar_Delta_Northing;
  Polar_Delta_Northing *= 1.01;

  Polar_Delta_Easting = Polar_Delta_Northing;
}


PolarStereographic::PolarStereographic( const PolarStereographic &ps )
{
  coordinateType = ps.coordinateType;
  semiMajorAxis = ps.semiMajorAxis;
  flattening = ps.flattening;
  es = ps.es;     
  es_OVER_2 = ps.es_OVER_2; 
  Southern_Hemisphere = ps.Southern_Hemisphere; 
  Polar_tc = ps.Polar_tc; 
  Polar_k90 = ps.Polar_k90; 
  Polar_a_mc = ps.Polar_a_mc; 
  two_Polar_a = ps.two_Polar_a; 
  Polar_Central_Meridian = ps.Polar_Central_Meridian; 
  Polar_Standard_Parallel = ps.Polar_Standard_Parallel; 
  Polar_False_Easting = ps.Polar_False_Easting; 
  Polar_False_Northing = ps.Polar_False_Northing; 
  Polar_Scale_Factor = ps.Polar_Scale_Factor; 
  Polar_Delta_Easting = ps.Polar_Delta_Easting; 
  Polar_Delta_Northing = ps.Polar_Delta_Northing; 
}


PolarStereographic::~PolarStereographic()
{
}


PolarStereographic& PolarStereographic::operator=( const PolarStereographic &ps )
{
  if( this != &ps )
  {
    coordinateType = ps.coordinateType;
    semiMajorAxis = ps.semiMajorAxis;
    flattening = ps.flattening;
    es = ps.es;     
    es_OVER_2 = ps.es_OVER_2; 
    Southern_Hemisphere = ps.Southern_Hemisphere; 
    Polar_tc = ps.Polar_tc; 
    Polar_k90 = ps.Polar_k90; 
    Polar_a_mc = ps.Polar_a_mc; 
    two_Polar_a = ps.two_Polar_a; 
    Polar_Central_Meridian = ps.Polar_Central_Meridian; 
    Polar_Standard_Parallel = ps.Polar_Standard_Parallel; 
    Polar_False_Easting = ps.Polar_False_Easting; 
    Polar_False_Northing = ps.Polar_False_Northing; 
    Polar_Scale_Factor = ps.Polar_Scale_Factor; 
    Polar_Delta_Easting = ps.Polar_Delta_Easting; 
    Polar_Delta_Northing = ps.Polar_Delta_Northing; 
  }

  return *this;
}


PolarStereographicStandardParallelParameters* PolarStereographic::getStandardParallelParameters() const
{
/*
 * The function getStandardParallelParameters returns the current
 * ellipsoid parameters and Polar (Standard Parallel) projection parameters.
 *
 *  ellipsoidSemiMajorAxis          : Semi-major axis of ellipsoid, in meters         (output)
 *  ellipsoidFlattening             : Flattening of ellipsoid					                (output)
 *  centralMeridian                 : Longitude down from pole, in radians            (output)
 *  standardParallel                : Latitude of true scale, in radians              (output)
 *  falseEasting                    : Easting (X) at center of projection, in meters  (output)
 *  falseNorthing                   : Northing (Y) at center of projection, in meters (output)
 */

  return new PolarStereographicStandardParallelParameters( CoordinateType::polarStereographicStandardParallel, Polar_Central_Meridian, Polar_Standard_Parallel, Polar_False_Easting, Polar_False_Northing );
}


PolarStereographicScaleFactorParameters* PolarStereographic::getScaleFactorParameters() const
{
/*
 * The function getScaleFactorParameters returns the current
 * ellipsoid parameters and Polar (Scale Factor) projection parameters.
 *
 *  ellipsoidSemiMajorAxis          : Semi-major axis of ellipsoid, in meters         (output)
 *  ellipsoidFlattening             : Flattening of ellipsoid					                (output)
 *  centralMeridian                 : Longitude down from pole, in radians            (output)
 *  scaleFactor                     : Scale factor                                    (output)
 *  falseEasting                    : Easting (X) at center of projection, in meters  (output)
 *  falseNorthing                   : Northing (Y) at center of projection, in meters (output)
 */

  if(Southern_Hemisphere == 0)
    return new PolarStereographicScaleFactorParameters( CoordinateType::polarStereographicScaleFactor, Polar_Central_Meridian, Polar_Scale_Factor, 'N', Polar_False_Easting, Polar_False_Northing );
  else
    return new PolarStereographicScaleFactorParameters( CoordinateType::polarStereographicScaleFactor, Polar_Central_Meridian, Polar_Scale_Factor, 'S', Polar_False_Easting, Polar_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* PolarStereographic::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic
 * coordinates (latitude and longitude) to Polar Stereographic coordinates
 * (easting and northing), according to the current ellipsoid
 * and Polar Stereographic projection parameters. If any errors occur, 
 * an exception is thrown with a description of the error.
 *
 *    longitude  :  Longitude, in radians                     (input)
 *    latitude   :  Latitude, in radians                      (input)
 *    easting    :  Easting (X), in meters                    (output)
 *    northing   :  Northing (Y), in meters                   (output)
 */

  double dlam;
  double slat;
  double essin;
  double t;
  double rho;
  double pow_es;
  double easting, northing;

  double longitude = geodeticCoordinates->longitude();
  double latitude = geodeticCoordinates->latitude();

  if ((latitude < -PI_OVER_2) || (latitude > PI_OVER_2))
  {   /* latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  else if ((latitude < 0) && (Southern_Hemisphere == 0))
  {   /* latitude and Origin Latitude in different hemispheres */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  else if ((latitude > 0) && (Southern_Hemisphere == 1))
  {   /* latitude and Origin Latitude in different hemispheres */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  {  /* longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  if (fabs(fabs(latitude) - PI_OVER_2) < 1.0e-10)
  {
    easting = Polar_False_Easting;
    northing = Polar_False_Northing;
  }
  else
  {
    if (Southern_Hemisphere != 0)
    {
      longitude *= -1.0;
      latitude *= -1.0;
    }
    dlam = longitude - Polar_Central_Meridian;
    if (dlam > PI)
    {
      dlam -= TWO_PI;
    }
    if (dlam < -PI)
    {
      dlam += TWO_PI;
    }
    slat = sin(latitude);
    essin = es * slat;
    pow_es = polarPow(essin);
    t = tan(PI_OVER_4 - latitude / 2.0) / pow_es;

    if (fabs(fabs(Polar_Standard_Parallel) - PI_OVER_2) > 1.0e-10)
      rho = Polar_a_mc * t / Polar_tc;
    else
      rho = two_Polar_a * t / Polar_k90;


    if (Southern_Hemisphere != 0)
    {
      easting = -(rho * sin(dlam) - Polar_False_Easting);
      northing = rho * cos(dlam) + Polar_False_Northing;
    }
    else
    {
      easting = rho * sin(dlam) + Polar_False_Easting;
      northing = -rho * cos(dlam) + Polar_False_Northing;
    }
  }

  return new MapProjectionCoordinates( coordinateType, easting, northing );
}


MSP::CCS::GeodeticCoordinates* PolarStereographic::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Polar
 * Stereographic coordinates (easting and northing) to geodetic
 * coordinates (latitude and longitude) according to the current ellipsoid
 * and Polar Stereographic projection Parameters. If any errors occur, 
 * an exception is thrown with a description of the error.
 *
 *  easting          : Easting (X), in meters                   (input)
 *  northing         : Northing (Y), in meters                  (input)
 *  longitude        : Longitude, in radians                    (output)
 *  latitude         : Latitude, in radians                     (output)
 *
 */

  double dy = 0, dx = 0;
  double rho = 0;
  double t;
  double PHI, sin_PHI;
  double tempPHI = 0.0;
  double essin;
  double pow_es;
  double delta_radius;
  double longitude, latitude;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  double min_easting  = Polar_False_Easting  - Polar_Delta_Easting;
  double max_easting  = Polar_False_Easting  + Polar_Delta_Easting;
  double min_northing = Polar_False_Northing - Polar_Delta_Northing;
  double max_northing = Polar_False_Northing + Polar_Delta_Northing;

  if (easting > max_easting || easting < min_easting)
  { /* easting out of range */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if (northing > max_northing || northing < min_northing)
  { /* northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy = northing - Polar_False_Northing;
  dx = easting - Polar_False_Easting;

  /* Radius of point with origin of false easting, false northing */
  rho = sqrt(dx * dx + dy * dy);   
    
  delta_radius = sqrt(
     Polar_Delta_Easting * Polar_Delta_Easting +
     Polar_Delta_Northing * Polar_Delta_Northing);

  if(rho > delta_radius)
  { /* Point is outside of projection area */
     throw CoordinateConversionException( ErrorMessages::radius );
  }

  if ((dy == 0.0) && (dx == 0.0))
  {
     latitude = PI_OVER_2;
     longitude = Polar_Central_Meridian;
  }
  else
  {
     if (Southern_Hemisphere != 0)
     {
        dy *= -1.0;
        dx *= -1.0;
     }

     if (fabs(fabs(Polar_Standard_Parallel) - PI_OVER_2) > 1.0e-10)
        t = rho * Polar_tc / (Polar_a_mc);
     else
        t = rho * Polar_k90 / (two_Polar_a);
     PHI = PI_OVER_2 - 2.0 * atan(t);
     while (fabs(PHI - tempPHI) > 1.0e-10)
     {
        tempPHI = PHI;
        sin_PHI = sin(PHI);
        essin =  es * sin_PHI;
        pow_es = polarPow(essin);
        PHI = PI_OVER_2 - 2.0 * atan(t * pow_es);
     }
     latitude = PHI;
     longitude = Polar_Central_Meridian + atan2(dx, -dy);

     if (longitude > PI)
        longitude -= TWO_PI;
     else if (longitude < -PI)
        longitude += TWO_PI;


     if (latitude > PI_OVER_2)  /* force distorted values to 90, -90 degrees */
        latitude = PI_OVER_2;
     else if (latitude < -PI_OVER_2)
        latitude = -PI_OVER_2;

     if (longitude > PI)  /* force distorted values to 180, -180 degrees */
        longitude = PI;
     else if (longitude < -PI)
        longitude = -PI;

  }
  if (Southern_Hemisphere != 0)
  {
     latitude *= -1.0;
     longitude *= -1.0;
  }

  return new GeodeticCoordinates(
     CoordinateType::geodetic, longitude, latitude );
}


double PolarStereographic::polarPow( double esSin )
{
  return pow((1.0 - esSin) / (1.0 + esSin), es_OVER_2);
}


// CLASSIFICATION: UNCLASSIFIED
