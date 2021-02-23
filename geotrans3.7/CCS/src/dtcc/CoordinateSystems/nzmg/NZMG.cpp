// CLASSIFICATION: UNCLASSIFIED

/***************************************************************************/
/* RSC IDENTIFIER: NEW ZEALAND MAP GRID
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates 
 *    (latitude and longitude) and New Zealand Map Grid coordinates
 *    (easting and northing).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       NZMG_NO_ERROR              : No errors occurred in function
 *       NZMG_LAT_ERROR             : Latitude outside of valid range
 *                                      (-33.5 to -48.5 degrees)
 *       NZMG_LON_ERROR             : Longitude outside of valid range
 *                                      (165.5 to 180.0 degrees)
 *       NZMG_EASTING_ERROR         : Easting outside of valid range
 *                                      (depending on ellipsoid and
 *                                       projection parameters)
 *       NZMG_NORTHING_ERROR        : Northing outside of valid range
 *                                      (depending on ellipsoid and
 *                                       projection parameters)
 *       NZMG_ELLIPSOID_ERROR       : Invalid ellipsoid - must be International
 *
 * REUSE NOTES
 *
 *    NEW ZEALAND MAP GRID is intended for reuse by any application that 
 *    performs a New Zealand Map Grid projection or its inverse.
 *    
 * REFERENCES
 *
 *    Further information on NEW ZEALAND MAP GRID can be found in the 
 *    Reuse Manual.
 *
 *    NEW ZEALAND MAP GRID originated from :  
 *                      U.S. Army Topographic Engineering Center
 *                      Geospatial Information Division
 *                      7701 Telegraph Road
 *                      Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    NEW ZEALAND MAP GRID has no restrictions.
 *
 * ENVIRONMENT
 *
 *    NEW ZEALAND MAP GRID was tested and certified in the following 
 *    environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    09-14-00          Original Code
 *    03-2-07           Original C++ Code
 *
 *
 */


/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <string.h>
#include <math.h>
#include "NZMG.h"
#include "EllipsoidParameters.h"
#include "MapProjectionCoordinates.h"
#include "GeodeticCoordinates.h"
#include "CoordinateConversionException.h"
#include "ErrorMessages.h"

/*
 *    string.h    - Standard C string handling library
 *    math.h      - Standard C math library
 *    NZMG.h      - Is for prototype error checking
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

const double PI = 3.14159265358979323e0;        /* PI     */
const double PI_OVER_2 = (PI / 2.0);                 
const double TWO_PI = (2.0 * PI);                 
const double MAX_LAT = (-33.5 * PI / 180.0);    /* -33.5 degrees */
const double MIN_LAT = (-48.5 * PI / 180.0);    /* -48.5 degrees */
const double MAX_LON = (180.0 * PI / 180.0);    /* 180 degrees */
const double MIN_LON = (165.5 * PI / 180.0);    /* 165.5 degrees */

const char* International = "IN";

/* NZMG projection Parameters */
const double NZMG_Origin_Lat = (-41.0);               /* Latitude of origin, in radians */
const double NZMG_Origin_Long = (173.0 * PI / 180.0); /* Longitude of origin, in radians */
const double NZMG_False_Northing = 6023150.0;         /* False northing, in meters */
const double NZMG_False_Easting = 2510000.0;          /* False easting, in meters */

/* Maximum variance for easting and northing values for International. */
const double NZMG_Max_Easting = 3170000.0;
const double NZMG_Max_Northing = 6900000.0;
const double NZMG_Min_Easting = 1810000.0;
const double NZMG_Min_Northing = 5160000.0;

struct Complex
{
  double real;
  double imag;
}; 

double A[] = { 0.6399175073, -0.1358797613, 0.063294409,
              -0.02526853, 0.0117879, -0.0055161,
               0.0026906, -0.001333, 0.00067, -0.00034 };

Complex B[] = { { 0.7557853228, 0.0 },
                { 0.249204646, 0.003371507 },
                { -0.001541739, 0.041058560 },
                { -0.10162907, 0.01727609 },
                { -0.26623489, -0.36249218 },
                { -0.6870983, -1.1651967 } };

Complex C[] = { { 1.3231270439, 0.0 },
                { -0.577245789, -0.007809598 },
                { 0.508307513, -0.112208952 },
                { -0.15094762, 0.18200602 },
                { 1.01418179, 1.64497696 },
                { 1.9660549, 2.5127645 } };

double D[] = { 1.5627014243, 0.5185406398, -0.03333098,
              -0.1052906, -0.0368594, 0.007317,
               0.01220, 0.00394, -0.0013 };   

/************************************************************************/
/*                              FUNCTIONS     
 *
 */

/* Add two complex numbers */
Complex add(Complex z1, Complex z2)
{
  Complex z;

  z.real = z1.real + z2.real;
  z.imag = z1.imag + z2.imag;

  return z;
}


/* Multiply two complex numbers */
Complex multiply(Complex z1, Complex z2)
{
  Complex z;

  z.real = z1.real * z2.real - z1.imag * z2.imag;
  z.imag = z1.imag * z2.real + z1.real * z2.imag;

  return z;
}


/* Divide two complex numbers */
Complex divide(Complex z1, Complex z2)
{
  Complex z;
  double denom;

  denom = z2.real * z2.real + z2.imag * z2.imag;
  z.real = (z1.real * z2.real + z1.imag * z2.imag) / denom;
  z.imag = (z1.imag * z2.real - z1.real * z2.imag) / denom;

  return z;
}


NZMG::NZMG( char* ellipsoidCode ) :
  CoordinateSystem( 6378388.0, 1 / 297.0 )
{
/*
 * The constructor receives the ellipsoid code and sets
 * the corresponding state variables. If any errors occur, 
 * an exception is thrown with a description of the error.
 *
 *   ellipsoidCode : 2-letter code for ellipsoid           (input)
 */

  strcpy( NZMGEllipsoidCode, "IN" );

  if (strcmp(ellipsoidCode, International) != 0)
  { /* Ellipsoid must be International */
    throw CoordinateConversionException( ErrorMessages::nzmgEllipsoid  );
  }

  strcpy( NZMGEllipsoidCode, ellipsoidCode );
}


NZMG::NZMG( const NZMG &n )
{
  semiMajorAxis = n.semiMajorAxis;
  flattening = n.flattening;
}


NZMG::~NZMG()
{
}


NZMG& NZMG::operator=( const NZMG &n )
{
  if( this != &n )
  {
    semiMajorAxis = n.semiMajorAxis;
    flattening = n.flattening;
  }

  return *this;
}


EllipsoidParameters* NZMG::getParameters() const
{
/*                         
 * The function getParameters returns the current ellipsoid
 * code.
 *
 *   ellipsoidCode : 2-letter code for ellipsoid          (output)
 */

  return new EllipsoidParameters( semiMajorAxis, flattening, (char*)NZMGEllipsoidCode );
}


MSP::CCS::MapProjectionCoordinates* NZMG::convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates )
{
/*
 * The function convertFromGeodetic converts geodetic (latitude and
 * longitude) coordinates to New Zealand Map Grid projection (easting and northing)
 * coordinates, according to the current ellipsoid and New Zealand Map Grid 
 * projection parameters.  If any errors occur, an exception is thrown 
 * with a description of the error.
 *
 *    longitude         : Longitude (lambda), in radians       (input)
 *    latitude          : Latitude (phi), in radians           (input)
 *    easting           : Easting (X), in meters               (output)
 *    northing          : Northing (Y), in meters              (output)
 */

  Complex Zeta, z;
  int n;
  double dphi;
  double du, dlam;

  double longitude = geodeticCoordinates->longitude();
  double latitude = geodeticCoordinates->latitude();

  if ((latitude < MIN_LAT) || (latitude > MAX_LAT))
  {  /* Latitude out of range */
    throw CoordinateConversionException( ErrorMessages::latitude  );
  }
  if ((longitude < MIN_LON) || (longitude > MAX_LON))
  {  /* Longitude out of range */
    throw CoordinateConversionException( ErrorMessages::longitude  );
  }

  dphi = (latitude * (180.0 / PI) - NZMG_Origin_Lat) * 3600.0 * 1.0e-5;
  du = A[9];
  for (n = 8; n >= 0; n--)
    du = du * dphi + A[n];
  du *= dphi;

  dlam = longitude - NZMG_Origin_Long;

  Zeta.real = du;
  Zeta.imag = dlam;

  z.real = B[5].real;
  z.imag = B[5].imag;
  for (n = 4; n >= 0; n--)
  {
    z = multiply(z, Zeta);
    z = add(B[n], z);
  }
  z = multiply(z, Zeta);

  double easting = (z.imag * semiMajorAxis) + NZMG_False_Easting;
  double northing = (z.real * semiMajorAxis) + NZMG_False_Northing;

  if ((easting < NZMG_Min_Easting) || (easting > NZMG_Max_Easting)) 
    throw CoordinateConversionException( ErrorMessages::easting );
  if ((northing < NZMG_Min_Northing) || (northing > NZMG_Max_Northing))
    throw CoordinateConversionException( ErrorMessages::northing );

  return new MapProjectionCoordinates( CoordinateType::newZealandMapGrid, easting, northing );
}


MSP::CCS::GeodeticCoordinates* NZMG::convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates )
{
/*
 * The function convertToGeodetic converts New Zealand Map Grid projection
 * (easting and northing) coordinates to geodetic (latitude and longitude)
 * coordinates, according to the current ellipsoid and New Zealand Map Grid projection
 * coordinates.  If any errors occur, an exception is thrown with a description 
 * of the error.
 *
 *    easting           : Easting (X), in meters                  (input)
 *    northing          : Northing (Y), in meters                 (input)
 *    longitude         : Longitude (lambda), in radians          (output)
 *    latitude          : Latitude (phi), in radians              (output)
 */

  int i, n;
  Complex coeff;
  Complex z, Zeta, Zeta_Numer, Zeta_Denom, Zeta_sqr;
  double dphi;

  double easting  = mapProjectionCoordinates->easting();
  double northing = mapProjectionCoordinates->northing();

  if ((easting < NZMG_Min_Easting) || (easting > NZMG_Max_Easting)) 
  { /* Easting out of range  */
    throw CoordinateConversionException( ErrorMessages::easting  );
  }
  if ((northing < NZMG_Min_Northing) || (northing > NZMG_Max_Northing))
  { /* Northing out of range */
    throw CoordinateConversionException( ErrorMessages::northing  );
  }

  z.real = (northing - NZMG_False_Northing) / semiMajorAxis;
  z.imag = (easting - NZMG_False_Easting) / semiMajorAxis;

  Zeta.real = C[5].real;
  Zeta.imag = C[5].imag;
  for (n = 4; n >= 0; n--)
  {
    Zeta = multiply(Zeta, z);
    Zeta = add(C[n], Zeta);
  }
  Zeta = multiply(Zeta, z);

  for (i = 0; i < 2; i++)
  {
    Zeta_Numer.real = 5.0 * B[5].real;
    Zeta_Numer.imag = 5.0 * B[5].imag;
    Zeta_Denom.real = 6.0 * B[5].real;
    Zeta_Denom.imag = 6.0 * B[5].imag;
    for (n = 4; n >= 1; n--)
    {
      Zeta_Numer = multiply(Zeta_Numer, Zeta);
      coeff.real = n * B[n].real;
      coeff.imag = n * B[n].imag;
      Zeta_Numer = add(coeff, Zeta_Numer); 

      Zeta_Denom = multiply(Zeta_Denom, Zeta);
      coeff.real = (n+1) * B[n].real;
      coeff.imag = (n+1) * B[n].imag;
      Zeta_Denom = add(coeff, Zeta_Denom);
    }
    Zeta_sqr = multiply(Zeta, Zeta);

    Zeta_Numer = multiply(Zeta_Numer, Zeta_sqr);
    Zeta_Numer = add(z, Zeta_Numer);

    Zeta_Denom = multiply(Zeta_Denom, Zeta);
    Zeta_Denom = add(B[0], Zeta_Denom);

    Zeta = divide(Zeta_Numer, Zeta_Denom);  
  }
  dphi = D[8];
  for (n = 7; n >= 0; n--)
    dphi = dphi * Zeta.real + D[n];
  dphi *= Zeta.real;
    
  double latitude = NZMG_Origin_Lat + (dphi * 1.0e5 / 3600.0);
  latitude *= PI / 180.0;
  double longitude = NZMG_Origin_Long + Zeta.imag;

  if ((longitude > PI) && (longitude - PI < 1.0e-6))
    longitude = PI;

  if ((latitude < MIN_LAT) || (latitude > MAX_LAT))
    throw CoordinateConversionException( ErrorMessages::latitude );
  if ((longitude < MIN_LON) || (longitude > MAX_LON))
    throw CoordinateConversionException( ErrorMessages::longitude );

  return new GeodeticCoordinates( CoordinateType::geodetic, longitude, latitude );
}



// CLASSIFICATION: UNCLASSIFIED
