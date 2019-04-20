// CLASSIFICATION: UNCLASSIFIED

#ifndef Orthographic_H
#define Orthographic_H

/***************************************************************************/
/* RSC IDENTIFIER: ORTHOGRAPHIC
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Orthographic projection 
 *    coordinates (easting and northing in meters).  The Orthographic
 *    projection employs a spherical Earth model.  The Spherical Radius
 *    used is the the radius of the sphere having the same area as the
 *    ellipsoid.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *          ORTH_NO_ERROR           : No errors occurred in function
 *          ORTH_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          ORTH_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          ORTH_EASTING_ERROR      : Easting outside of valid range
 *                                      (False_Easting +/- ~6,500,000 m,
 *                                       depending on ellipsoid parameters)
 *          ORTH_NORTHING_ERROR     : Northing outside of valid range
 *                                      (False_Northing +/- ~6,500,000 m,
 *                                       depending on ellipsoid parameters)
 *          ORTH_RADIUS_ERROR       : Coordinates too far from pole,
 *                                      depending on ellipsoid and
 *                                      projection parameters
 *          ORTH_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          ORTH_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          ORTH_A_ERROR            : Semi-major axis less than or equal to zero
 *          ORTH_INV_F_ERROR        : Inverse flattening outside of valid range
 *								  	                  (250 to 350)
 *
 * REUSE NOTES
 *
 *    ORTHOGRAPHIC is intended for reuse by any application that performs a
 *    Orthographic projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on ORTHOGRAPHIC can be found in the Reuse Manual.
 *
 *    ORTHOGRAPHIC originated from :  U.S. Army Topographic Engineering Center
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
 *    ORTHOGRAPHIC has no restrictions.
 *
 * ENVIRONMENT
 *
 *    ORTHOGRAPHIC was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    06-15-99          Original Code
 *    03-05-07          Original C++ Code
 *
 */


#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class MapProjection4Parameters;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class Orthographic : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * projection parameters as inputs, and sets the corresponding state
       * variables.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
       *    ellipsoidFlattening     : Flattening of ellipsoid						        (input)
       *    centralMeridian         : Longitude in radians at the center of     (input)
       *                              the projection
       *    originLatitude          : Latitude in radians at which the          (input)
       *                              point scale factor is 1.0
       *    falseEasting            : A coordinate value in meters assigned to the
       *                              central meridian of the projection.       (input)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                              origin latitude of the projection         (input)                          
       */

	    Orthographic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing );


      Orthographic( const Orthographic &o );


	    ~Orthographic( void );


      Orthographic& operator=( const Orthographic &o );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters and Orthographic projection parameters.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
       *    ellipsoidFlattening     : Flattening of ellipsoid						        (output)
       *    centralMeridian         : Longitude in radians at the center of     (output)
       *                              the projection
       *    originLatitude          : Latitude in radians at which the          (output)
       *                              point scale factor is 1.0
       *    falseEasting            : A coordinate value in meters assigned to the
       *                              central meridian of the projection.       (output)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                              origin latitude of the projection         (output) 
       */

      MapProjection4Parameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts geodetic (latitude and
       * longitude) coordinates to Orthographic projection (easting and northing)
       * coordinates, according to the current ellipsoid and Orthographic projection
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
       * The function convertToGeodetic converts Orthographic projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid and Orthographic projection
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
    
      /* Ellipsoid Parameters, default to WGS 84 */
      double es2;             /* Eccentricity (0.08181919084262188000) squared         */
      double es4;             /* es2 * es2 */
      double es6;             /* es4 * es2 */
      double Ra;              /* Spherical Radius */

      /* Orthographic projection Parameters */
      double Orth_Origin_Lat;                   /* Latitude of origin in radians     */
      double Orth_Origin_Long;                  /* Longitude of origin in radians    */
      double Orth_False_Easting;
      double Orth_False_Northing;
      double Sin_Orth_Origin_Lat;               /* sin(Orth_Origin_Lat) */
      double Cos_Orth_Origin_Lat;               /* cos(Orth_Origin_Lat) */

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
