// CLASSIFICATION: UNCLASSIFIED

#ifndef AlbersEqualAreaConic_H
#define AlbersEqualAreaConic_H

/***************************************************************************/
/* RSC IDENTIFIER: ALBERS
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Albers Equal Area Conic
 *    projection coordinates (easting and northing in meters) defined
 *    by two standard parallels.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       ALBERS_NO_ERROR           : No errors occurred in function
 *       ALBERS_LAT_ERROR          : Latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *       ALBERS_LON_ERROR          : Longitude outside of valid range
 *                                     (-180 to 360 degrees)
 *       ALBERS_EASTING_ERROR      : Easting outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       ALBERS_NORTHING_ERROR     : Northing outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       ALBERS_FIRST_STDP_ERROR   : First standard parallel outside of valid
 *                                     range (-90 to 90 degrees)
 *       ALBERS_SECOND_STDP_ERROR  : Second standard parallel outside of valid
 *                                     range (-90 to 90 degrees)
 *       ALBERS_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *       ALBERS_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                     (-180 to 360 degrees)
 *       ALBERS_A_ERROR            : Semi-major axis less than or equal to zero
 *       ALBERS_INV_F_ERROR        : Inverse flattening outside of valid range
 *									                 (250 to 350)
 *       ALBERS_HEMISPHERE_ERROR   : Standard parallels cannot be opposite
 *                                     latitudes
 *       ALBERS_FIRST_SECOND_ERROR : The 1st & 2nd standard parallels cannot
 *                                   both be 0
 *
 *
 * REUSE NOTES
 *
 *    ALBERS is intended for reuse by any application that performs an Albers
 *    Equal Area Conic projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on ALBERS can be found in the Reuse Manual.
 *
 *    ALBERS originated from:     U.S. Army Topographic Engineering Center
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
 *    ALBERS has no restrictions.
 *
 * ENVIRONMENT
 *
 *    ALBERS was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. MSDOS with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    07-09-99          Original Code
 *    03-08-07          Original C++ Code
 *    
 *
 */


#include "CoordinateSystem.h"

#include "DtccApi.h"



namespace MSP
{
  namespace CCS
  {
    class MapProjection6Parameters;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

   class MSP_DTCC_API AlbersEqualAreaConic : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * projection parameters as inputs, and sets the corresponding state
       * variables.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
       *    ellipsoidFlattening     : Flattening of ellipsoid                   (input)
       *    centralMeridian         : Longitude in radians at the center of     (input)
       *                              the projection
       *    originLatitude          : Latitude in radians at which the          (input)
       *                              point scale factor is 1.0
       *    standardParallel1       : First standard parallel                   (input)
       *    standardParallel2       : Second standard parallel                  (input)
       *    falseEasting            : A coordinate value in meters assigned to the
       *                              central meridian of the projection.       (input)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                              origin latitude of the projection         (input)
       */

	    AlbersEqualAreaConic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double standardParallel1, double standardParallel2, double falseEasting, double falseNorthing );


      AlbersEqualAreaConic( const AlbersEqualAreaConic &aeac );


	    ~AlbersEqualAreaConic( void );


      AlbersEqualAreaConic& operator=( const AlbersEqualAreaConic &aeac );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters, and Albers projection parameters.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
       *    ellipsoidFlattening     : Flattening of ellipsoid										(output)
       *    centralMeridian         : Longitude in radians at the center of     (output)
       *                              the projection
       *    originLatitude          : Latitude in radians at which the          (output)
       *                              point scale factor is 1.0
       *    standardParallel1       : First standard parallel                   (output)
       *    standardParallel2       : Second standard parallel                  (output)
       *    falseEasting            : A coordinate value in meters assigned to the
       *                              central meridian of the projection.       (output)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                              origin latitude of the projection         (output)
       */

      MapProjection6Parameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts geodetic (latitude and
       * longitude) coordinates to Albers projection (easting and northing)
       * coordinates, according to the current ellipsoid and Albers projection
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
       * The function convertToGeodetic converts Albers projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid and Albers projection
       * coordinates.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    easting           : Easting (X) in meters                  (input)
       *    northing          : Northing (Y) in meters                 (input)
       *    latitude          : Latitude (phi) in radians              (output)
       *    longitude         : Longitude (lambda) in radians          (output)
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates );

    private:
    
      /* Ellipsoid Parameters, default to WGS 84 */
      double es;                   /* Eccentricity of ellipsoid */
      double es2;                  /* Eccentricity squared         */
      double C;                    /* constant c   */
      double rho0;                 /* height above ellipsoid		*/
      double n;                    /* ratio between meridians		*/
      double Albers_a_OVER_n;      /* Albers_a / n */
      double one_MINUS_es2;        /* 1 - es2 */
      double two_es;               /* 2 * es */

      /* Albers Projection Parameters */
      double Albers_Origin_Lat;         /* Latitude of origin in radians     */
      double Albers_Origin_Long;        /* Longitude of origin in radians    */
      double Albers_Std_Parallel_1;
      double Albers_Std_Parallel_2;
      double Albers_False_Easting;
      double Albers_False_Northing;

      double Albers_Delta_Northing;
      double Albers_Delta_Easting;

      double esSine( double sinlat );

      double albersQ( double slat, double oneminussqressin, double essin );

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
