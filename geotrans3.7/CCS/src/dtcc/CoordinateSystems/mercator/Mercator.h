// CLASSIFICATION: UNCLASSIFIED

#ifndef Mercator_H
#define Mercator_H

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


#include "CoordinateSystem.h"
#include "CoordinateType.h"


namespace MSP
{
  namespace CCS
  {
    class MercatorStandardParallelParameters;
    class MercatorScaleFactorParameters;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class Mercator : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * Mercator projection parameters as inputs, and sets the corresponding state 
       * variables.  It calculates and returns the scale factor. If any errors occur, 
       * an exception is thrown with a description of the error.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
       *    ellipsoidFlattening     : Flattening of ellipsoid						        (input)
       *    centralMeridian         : Longitude in radians at the center of     (input)
       *                              the projection
       *    standardParallel        : Latitude in radians at which the          (input)
       *                              point scale factor is 1.0
       *    falseEasting            : A coordinate value in meters assigned to the
       *                              central meridian of the projection.       (input)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                              origin latitude of the projection         (input)
       *    scaleFactor             : Multiplier which reduces distances in the 
       *                              projection to the actual distance on the
       *                              ellipsoid                                 (output)
       */

	    Mercator( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double standardParallel, double falseEasting, double falseNorthing, double* scaleFactor );


      /*
       * The constructor receives the ellipsoid parameters and
       * Mercator projection parameters as inputs, and sets the corresponding state 
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

      Mercator( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double falseEasting, double falseNorthing, double scaleFactor );


      Mercator( const Mercator &m );


	    ~Mercator( void );


      Mercator& operator=( const Mercator &m );


      /*
       * The function getStandardParallelParameters returns the current ellipsoid
       * parameters and Mercator (Standard Parallel) projection parameters.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
       *    ellipsoidFlattening     : Flattening of ellipsoid						        (output)
       *    centralMeridian         : Longitude in radians at the center of     (output)
       *                              the projection
       *    standardParallel        : Latitude in radians at which the          (output)
       *                              point scale factor is 1.0
       *    falseEasting            : A coordinate value in meters assigned to the
       *                              central meridian of the projection.       (output)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                              origin latitude of the projection         (output)
       */

      MercatorStandardParallelParameters* getStandardParallelParameters() const;


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

      MercatorScaleFactorParameters* getScaleFactorParameters() const;


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

      MSP::CCS::MapProjectionCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates );


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

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates );

    private:
    
      CoordinateType::Enum coordinateType;

      /* Ellipsoid Parameters, default to WGS 84 */
      double Merc_e;               /* Eccentricity of ellipsoid    */
      double Merc_es;              /* Eccentricity squared         */

      /* Mercator projection Parameters */
      double Merc_Standard_Parallel;      /* Latitude of true scale in radians     */
      double Merc_Cent_Mer;               /* Central meridian in radians    */
      double Merc_False_Northing;         /* False northing in meters          */
      double Merc_False_Easting;          /* False easting in meters           */
      double Merc_Scale_Factor;           /* Scale factor                      */

      /* Isometric to geodetic latitude parameters, default to WGS 84 */
      double Merc_ab;
      double Merc_bb;
      double Merc_cb;
      double Merc_db;

      /* Maximum variance for easting and northing values for WGS 84.*/

      double Merc_Delta_Easting;
      double Merc_Delta_Northing;

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
