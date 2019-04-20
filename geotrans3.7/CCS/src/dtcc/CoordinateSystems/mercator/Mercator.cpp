// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: MERCATOR
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Mercator projection coordinates
 *    (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          MERC_NO_ERROR                  : No errors occurred in function
 *          MERC_LAT_ERROR                 : Latitude outside of valid range
 *                                           (-89.5 to 89.5 degrees)
 *          MERC_LON_ERROR                 : Longitude outside of valid range
 *                                           (-180 to 360 degrees)
 *          MERC_EASTING_ERROR             : Easting outside of valid range
 *                                           (False_Easting +/- ~20,500,000 m,
 *                                           depending on ellipsoid parameters
 *                                           and Origin_Latitude)
 *          MERC_NORTHING_ERROR            : Northing outside of valid range
 *                                           (False_Northing +/- ~23,500,000 m,
 *                                           depending on ellipsoid parameters
 *                                           and Origin_Latitude)
 *          MERC_LAT_OF_TRUE_SCALE_ERROR   : Latitude of true scale outside of valid range
 *                                           (-89.5 to 89.5 degrees)
 *          MERC_CENT_MER_ERROR            : Central meridian outside of valid range
 *                                           (-180 to 360 degrees)
 *          MERC_A_ERROR                   : Semi-major axis less than or equal to zero
 *          MERC_INV_F_ERROR               : Inverse flattening outside of valid range
 *									                         (250 to 350)
 *
 * REUSE NOTES
 *
 *    MERCATOR is intended for reuse by any application that performs a 
 *    Mercator projection or its inverse.
 *    
 * REFERENCES
 *
 *    Further information on MERCATOR can be found in the Reuse Manual.
 *
 *    MERCATOR originated from :  U.S. Army Topographic Engineering Center
 *                                Geospatial Information Division
 *                                7701 Telegraph Road
 *                                Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    MERCATOR has no restrictions.
 *
 * ENVIRONMENT
 *
 *    MERCATOR was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    10-02-97          Original Code
 *    03-06-07          Original C++ Code
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <stdio.h>
#include <math.h>
#include "Mercator.h"
#include "MercatorStandardParallelParameters.h"
#include "MercatorScaleFactorParameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    math.h     - Standard C math library
 *    Mercator.h - Is for prototype error checking
 *    MapProjectionCoordinates.h   - defines map projection coordinates
 *    GeodeticCoordinates.h   - defines geodetic coordinates
 *    CoordinateConversionException.h - Exception handler
 *    ErrorMessages.h  - Contains exception messages
 */


using namespace MSP::CCS;


/***************************************************************************/
/*                               DEFINES 
 *
 */

const double PI = 3.14159265358979323e0;         /* PI   */
const double PI_OVER_2 = ( PI / 2.0e0);  
const double TWO_PI = (2.0 * PI);                 
const double MAX_LAT = ( (PI * 89.5) / 180.0 );  /* 89.5 degrees in radians */
const double MIN_SCALE_FACTOR = 0.3;
const double MAX_SCALE_FACTOR = 3.0;


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

Mercator::Mercator( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double standardParallel, double falseEasting, double falseNorthing, double* scaleFactor ) :
  CoordinateSystem(),
  coordinateType( CoordinateType::mercatorStandardParallel ),
  Merc_e( 0.08181919084262188000 ),
  Merc_es( 0.0066943799901413800 ),
  Merc_Cent_Mer( 0.0 ),
  Merc_Standard_Parallel( 0.0 ),
  Merc_False_Easting( 0.0 ),
  Merc_False_Northing( 0.0 ),
  Merc_Scale_Factor( 1.0 ),
  Merc_ab( 0.00335655146887969400 ),
  Merc_bb( 0.00000657187271079536 ),
  Merc_cb( 0.00000001764564338702 ),
  Merc_db( 0.00000000005328478445 ),
  Merc_Delta_Easting( 20237883.0 ),
  Merc_Delta_Northing( 23421740.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * Mercator (Standard Parallel) projection parameters as inputs, and sets the corresponding state 
 * variables.  It calculates and returns the scale factor.  If any errors occur, 
 * an exception is thrown with a description of the error.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters (input)
 *    ellipsoidFlattening     : Flattening of ellipsoid		 	(input)
 *    centralMeridian         : Longitude in radians at the center of   (input)
 *                              the projection
 *    standardParallel        : Latitude in radians at which the        (input)
 *                              point scale factor is 1.0
 *    falseEasting            : A coordinate value in meters assigned to the
 *                              central meridian of the projection.     (input)
 *    falseNorthing           : A coordinate value in meters assigned to the
 *                              origin latitude of the projection       (input)
 *    scaleFactor             : Multiplier which reduces distances in the 
 *                              projection to the actual distance on the
 *                              ellipsoid                               (output)
 */

  double es2;   /* Eccentricity squared of ellipsoid to the second power    */
  double es3;   /* Eccentricity squared of ellipsoid to the third power     */
  double es4;   /* Eccentricity squared of ellipsoid to the fourth power    */
  double sin_olat; /* sin(Origin_Latitude), temp variable */
  double inv_f = 1 / ellipsoidFlattening;

  if (ellipsoidSemiMajorAxis <= 0.0)
  { /* Semi-major axis must be greater than zero */
    throw CoordinateConversionException( ErrorMessages::semiMajorAxis  );
  }
  if ((inv_f < 250) || (inv_f > 350))
  { /* Inverse flattening must be between 250 and 350 */
    throw CoordinateConversionException( ErrorMessages::ellipsoidFlattening  );
  }
  if ((standardParallel < -MAX_LAT) || (standardParallel > MAX_LAT))
  { /* latitude of true scale out of range */
    throw CoordinateConversionException( ErrorMessages::originLatitude  );
  }
  if ((centralMeridian < -PI) || (centralMeridian > TWO_PI))
  { /* central meridian out of range */
    throw CoordinateConversionException( ErrorMessages::centralMeridian  );
  }

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening    = ellipsoidFlattening;

  Merc_Standard_Parallel = standardParallel;
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Merc_Cent_Mer       = centralMeridian;
  Merc_False_Northing = falseNorthing;
  Merc_False_Easting  = falseEasting;

  Merc_es = 2 * flattening - flattening * flattening;
  Merc_e = sqrt(Merc_es);
  sin_olat = sin(Merc_Standard_Parallel);
  Merc_Scale_Factor = cos(Merc_Standard_Parallel) / sqrt(1.e0 - Merc_es * sin_olat * sin_olat );
//  Merc_Scale_Factor = 1.0 / ( sqrt(1.e0 - Merc_es * sin_olat * sin_olat) 
//                              / cos(Merc_Standard_Parallel) );
  es2 = Merc_es * Merc_es;
  es3 = es2 * Merc_es;
  es4 = es3 * Merc_es;
  Merc_ab = Merc_es / 2.e0 + 5.e0 * es2 / 24.e0 + es3 / 12.e0
            + 13.e0 * es4 / 360.e0;
  Merc_bb = 7.e0 * es2 / 48.e0 + 29.e0 * es3 / 240.e0 
            + 811.e0 * es4 / 11520.e0;
  Merc_cb = 7.e0 * es3 / 120.e0 + 81.e0 * es4 / 1120.e0;
  Merc_db = 4279.e0 * es4 / 161280.e0;
  *scaleFactor = Merc_Scale_Factor;

  /* Calculate the width of the bounding box */
  /* Note: The width of the bounding box needs to be relative */
  /* to a false origin of 0,0, so subtract the false easting */
  /* and false northing values from the delta easting and */
  /* delta northing values */
  GeodeticCoordinates gcTemp( CoordinateType::geodetic, ( Merc_Cent_Mer + PI ), MAX_LAT );
  MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcTemp );      
  Merc_Delta_Easting = tempCoordinates->easting();
  Merc_Delta_Northing = tempCoordinates->northing();
  delete tempCoordinates;

  if(Merc_False_Easting)
    Merc_Delta_Easting -= Merc_False_Easting;
  if (Merc_Delta_Easting < 0)
    Merc_Delta_Easting = -Merc_Delta_Easting;
  Merc_Delta_Easting *= 1.01;

  if(Merc_False_Northing)
    Merc_Delta_Northing -= Merc_False_Northing;
  if (Merc_Delta_Northing < 0)
    Merc_Delta_Northing = -Merc_Delta_Northing;
  Merc_Delta_Northing *= 1.01;
}


Mercator::Mercator( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double falseEasting, double falseNorthing, double scaleFactor ) :
  CoordinateSystem(),
  coordinateType( CoordinateType::mercatorScaleFactor ),
  Merc_e( 0.08181919084262188000 ),
  Merc_es( 0.0066943799901413800 ),
  Merc_Cent_Mer( 0.0 ),
  Merc_Standard_Parallel( 0.0 ),
  Merc_False_Easting( 0.0 ),
  Merc_False_Northing( 0.0 ),
  Merc_Scale_Factor( 1.0 ),
  Merc_ab( 0.00335655146887969400 ),
  Merc_bb( 0.00000657187271079536 ),
  Merc_cb( 0.00000001764564338702 ),
  Merc_db( 0.00000000005328478445 ),
  Merc_Delta_Easting( 20237883.0 ),
  Merc_Delta_Northing( 23421740.0 )
{
/*
 * The constructor receives the ellipsoid parameters and
 * Mercator (Scale Factor) projection parameters as inputs, and sets the corresponding state 
 * variables.  It receives the scale factor as input.  If any errors occur, 
 * an exception is thrown with a description of the error.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
 *    ellipsoidFlattening     : Flattening of ellipsoid						        (input)
 *    centralMeridian         : Longitude in radians at the center of     (input)
 *                              the projection
 *    falseEasting            : A coordinate value in meters assigned to the
 *                              central meridian of the projection.       (input)
 *    falseNorthing           : A coordinate value in meters assigned to the
 *                              origin latitude of the projection         (input)
 *    scaleFactor             : Multiplier which reduces distances in the 
 *                              projection to the actual distance on the
 *                              ellipsoid                                 (input)
 */

  double es2;   /* Eccentricity squared of ellipsoid to the second power    */
  double es3;   /* Eccentricity squared of ellipsoid to the third power     */
  double es4;   /* Eccentricity squared of ellipsoid to the fourth power    */
  double Merc_Scale_Factor2;
  double inv_f = 1 / ellipsoidFlattening;

  if (ellipsoidSemiMajorAxis <= 0.0)
  { /* Semi-major axis must be greater than zero */
    throw CoordinateConversionException( ErrorMessages::semiMajorAxis  );
  }
  if ((inv_f < 250) || (inv_f > 350))
  { /* Inverse flattening must be between 250 and 350 */
    throw CoordinateConversionException( ErrorMessages::ellipsoidFlattening  );
  }
  if ((centralMeridian < -PI) || (centralMeridian > TWO_PI))
  { /* central meridian out of range */
    throw CoordinateConversionException( ErrorMessages::centralMeridian  );
  }
  if ((scaleFactor < MIN_SCALE_FACTOR) || (scaleFactor > MAX_SCALE_FACTOR))
  {
    throw CoordinateConversionException( ErrorMessages::scaleFactor  );
  }

  semiMajorAxis = ellipsoidSemiMajorAxis;
  flattening    = ellipsoidFlattening;

///  Merc_Standard_Parallel = standardParallel;
  if (centralMeridian > PI)
    centralMeridian -= TWO_PI;
  Merc_Cent_Mer = centralMeridian;
  Merc_False_Northing = falseNorthing;
  Merc_False_Easting = falseEasting;
  Merc_Scale_Factor = scaleFactor;///cos(Merc_Standard_Parallel) / sqrt(1.e0 - Merc_es * sin_olat * sin_olat );

  Merc_es = 2 * flattening - flattening * flattening;
  Merc_e = sqrt(Merc_es);
//  sin_olat = sin(Merc_Standard_Parallel);
//  Merc_Scale_Factor = cos(Merc_Standard_Parallel) / sqrt(1.e0 - Merc_es * sin_olat * sin_olat );
//  Merc_Scale_Factor = 1.0 / ( sqrt(1.e0 - Merc_es * sin_olat * sin_olat) 
//                              / cos(Merc_Standard_Parallel) );
  es2 = Merc_es * Merc_es;
  es3 = es2 * Merc_es;
  es4 = es3 * Merc_es;
  Merc_ab = Merc_es / 2.e0 + 5.e0 * es2 / 24.e0 + es3 / 12.e0
            + 13.e0 * es4 / 360.e0;
  Merc_bb = 7.e0 * es2 / 48.e0 + 29.e0 * es3 / 240.e0 
            + 811.e0 * es4 / 11520.e0;
  Merc_cb = 7.e0 * es3 / 120.e0 + 81.e0 * es4 / 1120.e0;
  Merc_db = 4279.e0 * es4 / 161280.e0;
//  *scaleFactor = Merc_Scale_Factor;

  Merc_Scale_Factor2 = Merc_Scale_Factor * Merc_Scale_Factor;
  Merc_Standard_Parallel = asin(sqrt((1 - Merc_Scale_Factor2)/(1 - Merc_Scale_Factor2 * Merc_es)));

  /* Calculate the width of the bounding box */
  /* Note: The width of the bounding box needs to be relative */
  /* to a false origin of 0,0, so subtract the false easting */
  /* and false northing values from the delta easting and */
  /* delta northing values */
  GeodeticCoordinates gcTemp( CoordinateType::geodetic, ( Merc_Cent_Mer + PI ), MAX_LAT );
  MapProjectionCoordinates* tempCoordinates = convertFromGeodetic( &gcTemp );      
  Merc_Delta_Easting = tempCoordinates->easting();
  Merc_Delta_Northing = tempCoordinates->northing();
  delete tempCoordinates;

  if(Merc_False_Easting)
    Merc_Delta_Easting -= Merc_False_Easting;
  if (Merc_Delta_Easting < 0)
    Merc_Delta_Easting = -Merc_Delta_Easting;
  Merc_Delta_Easting *= 1.01;

  if(Merc_False_Northing)
    Merc_Delta_Northing -= Merc_False_Northing;
  if (Merc_Delta_Northing < 0)
    Merc_Delta_Northing = -Merc_Delta_Northing;
  Merc_Delta_Northing *= 1.01;
}


Mercator::Mercator( const Mercator &m )
{
  coordinateType = m.coordinateType;
  semiMajorAxis = m.semiMajorAxis;
  flattening = m.flattening;
  Merc_e = m.Merc_e;
  Merc_es = m.Merc_es;
  Merc_Cent_Mer = m.Merc_Cent_Mer;
  Merc_Standard_Parallel = m.Merc_Standard_Parallel;
  Merc_False_Easting = m.Merc_False_Easting;
  Merc_False_Northing = m.Merc_False_Northing;
  Merc_Scale_Factor = m.Merc_Scale_Factor;
  Merc_ab = m.Merc_ab;
  Merc_bb = m.Merc_bb;
  Merc_cb = m.Merc_cb;
  Merc_db = m.Merc_db;
  Merc_Delta_Easting = m.Merc_Delta_Easting;
  Merc_Delta_Northing = m.Merc_Delta_Northing;
}


Mercator::~Mercator()
{
}


Mercator& Mercator::operator=( const Mercator &m )
{
  if( this != &m )
  {
    coordinateType = m.coordinateType;
    semiMajorAxis = m.semiMajorAxis;
    flattening = m.flattening;
    Merc_e = m.Merc_e;
    Merc_es = m.Merc_es;
    Merc_Cent_Mer = m.Merc_Cent_Mer;
    Merc_Standard_Parallel = m.Merc_Standard_Parallel;
    Merc_False_Easting = m.Merc_False_Easting;
    Merc_False_Northing = m.Merc_False_Northing;
    Merc_Scale_Factor = m.Merc_Scale_Factor;
    Merc_ab = m.Merc_ab;
    Merc_bb = m.Merc_bb;
    Merc_cb = m.Merc_cb;
    Merc_db = m.Merc_db;
    Merc_Delta_Easting = m.Merc_Delta_Easting;
    Merc_Delta_Northing = m.Merc_Delta_Northing;
  }

  return *this;
}


MercatorStandardParallelParameters* Mercator::getStandardParallelParameters() const
{
/*
 * The function getStandardParallelParameters returns the current ellipsoid
 * parameters and Mercator (Standard Parallel) projection parameters.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
 *    ellipsoidFlattening     : Flattening of ellipsoid						        (output)
 *    centralMeridian         : Longitude in radians at the center of     (output)
 *                              the projection
 *    standardParallel     : Latitude in radians at which the          (output)
 *                              point scale factor is 1.0
 *    falseEasting            : A coordinate value in meters assigned to the
 *                              central meridian of the projection.       (output)
 *    falseNorthing           : A coordinate value in meters assigned to the
 *                              origin latitude of the projection         (output)
 */

  return new MercatorStandardParallelParameters(CoordinateType::mercatorStandardParallel, Merc_Cent_Mer, Merc_Standard_Parallel, Merc_Scale_Factor, Merc_False_Easting, Merc_False_Northing);
}


MercatorScaleFactorParameters* Mercator::getScaleFactorParameters() const
{
/*
 * The function getScaleFactorParameters returns the current ellipsoid
 * parameters and Mercator (Scale Factor) projection parameters.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
 *    ellipsoidFlattening     : Flattening of ellipsoid						        (output)
 *    centralMeridian         : Longitude in radians at the center of     (output)
 *                              the projection
 *    falseEasting            : A coordinate value in meters assigned to the
 *                              central meridian of the projection.       (output)
 *    falseNorthing           : A coordinate value in meters assigned to the
 *                              origin latitude of the projection         (output)
 *    scaleFactor             : Multiplier which reduces distances in the 
 *                              projection to the actual distance on the
 *                              ellipsoid                                 (output)
 */

  return new MercatorScaleFactorParameters( CoordinateType::mercatorScaleFactor, Merc_Cent_Mer, Merc_Scale_Factor, Merc_False_Easting, Merc_False_Northing );
}


MSP::CCS::MapProjectionCoordinates* Mercator::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to Mercator projection (easting and northing)
 * coordinates, according to the current ellipsoid and Mercator projection
 * parameters.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    longitude         : Longitude (lambda) in radians       (input)
 *    latitude          : Latitude (phi) in radians           (input)
 *    easting           : Easting (X) in meters               (output)
 *    northing          : Northing (Y) in meters              (output)
 */

  double ctanz2;        /* Cotangent of z/2 - z - Isometric colatitude     */
  double e_x_sinlat;    /* e * sin(latitude)                               */
  double Delta_Long;    /* Difference in origin longitude and longitude    */
  double tan_temp;
  double pow_temp;

  double longitude = geodeticCoordinates->longitude();
  double latitude  = geodeticCoordinates->latitude();

  if ((latitude < -MAX_LAT) || (latitude > MAX_LAT))
  { /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude );
  }
  if ((longitude < -PI) || (longitude > TWO_PI))
  { /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude );
  }

  if (longitude > PI)
    longitude -= TWO_PI;
  e_x_sinlat = Merc_e * sin(latitude);
  tan_temp = tan(PI / 4.e0 + latitude / 2.e0);
  pow_temp = pow( ((1.e0 - e_x_sinlat) / (1.e0 + e_x_sinlat)),
                  (Merc_e / 2.e0) );
  ctanz2 = tan_temp * pow_temp;
  double northing = Merc_Scale_Factor * semiMajorAxis * log(ctanz2) + Merc_False_Northing;
  Delta_Long = longitude - Merc_Cent_Mer;
  if (Delta_Long > PI)
    Delta_Long -= TWO_PI;
  if (Delta_Long < -PI)
    Delta_Long += TWO_PI;
  double easting = Merc_Scale_Factor * semiMajorAxis * Delta_Long
             + Merc_False_Easting;

  return new MapProjectionCoordinates( coordinateType, easting, northing );
}


MSP::CCS::GeodeticCoordinates* Mercator::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts Mercator projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid and Mercator projection
 * coordinates.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    easting           : Easting (X) in meters                  (input)
 *    northing          : Northing (Y) in meters                 (input)
 *    longitude         : Longitude (lambda) in radians          (output)
 *    latitude          : Latitude (phi) in radians              (output)
 */

  double dx;     /* Delta easting - Difference in easting (easting-FE)      */
  double dy;     /* Delta northing - Difference in northing (northing-FN)   */
  double xphi;   /* Isometric latitude                                      */

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < (Merc_False_Easting - Merc_Delta_Easting))
      || (easting > (Merc_False_Easting + Merc_Delta_Easting)))
  { /* Easting out of range */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < (Merc_False_Northing - Merc_Delta_Northing))
      || (northing > (Merc_False_Northing + Merc_Delta_Northing)))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  dy = northing - Merc_False_Northing;
  dx = easting - Merc_False_Easting;
  double longitude = Merc_Cent_Mer + dx / (Merc_Scale_Factor * semiMajorAxis);
  xphi = PI_OVER_2 
         - 2.e0 * atan(1.e0 / exp(dy / (Merc_Scale_Factor * semiMajorAxis)));
  double latitude = xphi + Merc_ab * sin(2.e0 * xphi) + Merc_bb * sin(4.e0 * xphi)
              + Merc_cb * sin(6.e0 * xphi) + Merc_db * sin(8.e0 * xphi);

  if (longitude > PI)
    longitude -= TWO_PI;
  if (longitude < -PI)
    longitude += TWO_PI;

  return new GeodeticCoordinates( CoordinateType::geodetic, longitude, latitude );
}



// CLASSIFICATION: UNCLASSIFIED
