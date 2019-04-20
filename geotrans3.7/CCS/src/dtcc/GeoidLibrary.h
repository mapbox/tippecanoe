// CLASSIFICATION: UNCLASSIFIED

#ifndef GeoidLibrary_H
#define GeoidLibrary_H

/***************************************************************************/
/* RSC IDENTIFIER:  Geoid Library
 *
 * ABSTRACT
 *
 *    The purpose of GEOID is to support conversions between WGS84 ellipsoid
 *    heights and WGS84 geoid heights.
 *
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found, the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *  GEOID_NO_ERROR               : No errors occurred in function
 *  GEOID_FILE_OPEN_ERROR        : Geoid file opening error
 *  GEOID_INITIALIZE_ERROR       : Geoid separation database can not initialize
 *  GEOID_LAT_ERROR              : Latitude out of valid range
 *                                 (-90 to 90 degrees)
 *  GEOID_LON_ERROR              : Longitude out of valid range
 *                                 (-180 to 360 degrees)
 *
 * REUSE NOTES
 *
 *    Geoid is intended for reuse by any application that requires conversion
 *    between WGS84 ellipsoid heights and WGS84 geoid heights.
 *
 * REFERENCES
 *
 *    Further information on Geoid can be found in the Reuse Manual.
 *
 *    Geoid originated from :  U.S. Army Topographic Engineering Center
 *                             Geospatial Information Division
 *                             7701 Telegraph Road
 *                             Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    Geoid has no restrictions.
 *
 * ENVIRONMENT
 *
 *    Geoid was tested and certified in the following environments
 *
 *    1. Solaris 2.5 with GCC 2.8.1
 *    2. MS Windows XP with MS Visual C++ 6.0
 *
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    24-May-99         Original Code
 *    09-Jan-06         Added new geoid height interpolation methods
 *    03-14-07          Original C++ Code
 *    05-12-10          S. Gillis, BAEts26542, MSL-HAE for 30 minute grid added
 *    07-21-10          Read in full file at once instead of one post at a time 
 *    12-17-10          RD Craig added pointer to EGM2008 interpolator (BAEts26267).
 *
 */

#include "egm2008_geoid_grid.h"

#include "DtccApi.h"

namespace MSP
{
   class CCSThreadMutex;
   namespace CCS
   {
       /**
        *    The purpose of GEOID is to support conversions between WGS84 ellipsoid
        *    heights and WGS84 geoid heights.
        *
        *
        * ERROR HANDLING
        *
        *    This component checks parameters for valid values.  If an invalid value
        *    is found, the error code is combined with the current error code using
        *    the bitwise or.  This combining allows multiple error codes to be
        *    returned. The possible error codes are:
        *
        *  GEOID_NO_ERROR               : No errors occurred in function\n
        *  GEOID_FILE_OPEN_ERROR        : Geoid file opening error\n
        *  GEOID_INITIALIZE_ERROR       : Geoid separation database can not initialize\n
        *  GEOID_LAT_ERROR              : Latitude out of valid range
        *                                 (-90 to 90 degrees)\n
        *  GEOID_LON_ERROR              : Longitude out of valid range\n
        *                                 (-180 to 360 degrees)\n
        *
        * REUSE NOTES
        *
        *    Geoid is intended for reuse by any application that requires conversion
        *    between WGS84 ellipsoid heights and WGS84 geoid heights.
        *
        * REFERENCES
        *
        *    Further information on Geoid can be found in the Reuse Manual.
        *
        *    Geoid originated from :  U.S. Army Topographic Engineering Center
        *                             Geospatial Information Division
        *                             7701 Telegraph Road
        *                             Alexandria, VA  22310-3864
        *
        * LICENSES
        *
        *    None apply to this component.
        *
        * RESTRICTIONS
        *
        *    Geoid has no restrictions.
        *
        * ENVIRONMENT
        *
        *    Geoid was tested and certified in the following environments
        *
        *    1. Solaris 2.5 with GCC 2.8.1
        *    2. MS Windows XP with MS Visual C++ 6.0
        *
        */
      class MSP_DTCC_API GeoidLibrary
      {
            friend class GeoidLibraryCleaner;
            
         public:

            /**
             * The function getInstance returns an instance of the GeoidLibrary
             * @returns instance of GeoidLibrary
             */

            static GeoidLibrary* getInstance();


            /**
             * The function removeInstance removes this GeoidLibrary 
             * instance from the total number of instances. 
             */

            static void removeInstance();


	    ~GeoidLibrary( void );


            /**
             * The function convertEllipsoidToEGM96FifteenMinBilinearGeoidHeight
             * converts the specified WGS84 ellipsoid height at the specified
             * geodetic coordinates to the equivalent
             * geoid height, using the EGM96 gravity model and 
             * the bilinear interpolation method.
             *
             * @param[in]   longitude          : Geodetic longitude in radians
             * @param[in]   latitude           : Geodetic latitude in radians
             * @param[in]   ellipsoidHeight    : Ellipsoid height, in meters
             * @param[out]  geoidHeight        : Geoid height, in meters.
             *
             */

            void convertEllipsoidToEGM96FifteenMinBilinearGeoidHeight(
               double longitude,
               double latitude,
               double ellipsoidHeight,
               double *geoidHeight );


            /**
             * The function convertEllipsoidToEGM96VariableNaturalSplineHeight
             * converts the specified WGS84 ellipsoid height at the specified
             * geodetic coordinates to the equivalent geoid height, 
             * using the EGM96 gravity model and 
             * the natural spline interpolation method.
             *
             * @param[in]   longitude          : Geodetic longitude in radians
             * @param[in]   latitude           : Geodetic latitude in radians
             * @param[in]   ellipsoidHeight    : Ellipsoid height, in meters
             * @param[out]  geoidHeight        : Geoid height, in meters.
             *
             */

            void convertEllipsoidToEGM96VariableNaturalSplineHeight(
               double  longitude,
               double  latitude,
               double  ellipsoidHeight,
               double *geoidHeight );

            /**
             * The function convertEllipsoidToEGM84TenDegBilinearHeight
             * converts the specified WGS84 ellipsoid height at the specified
             * geodetic coordinates to the equivalent geoid height, 
             * using the EGM84 gravity model and
             * the bilinear interpolation method.
             *
             * @param[in]   longitude          : Geodetic longitude in radians
             * @param[in]   latitude           : Geodetic latitude in radians
             * @param[in]   ellipsoidHeight    : Ellipsoid height, in meters
             * @param[out]  geoidHeight        : Geoid height, in meters.
             *
             */

            void convertEllipsoidToEGM84TenDegBilinearHeight(
               double longitude,
               double latitude,
               double ellipsoidHeight,
               double *geoidHeight );

            /**
             * The function convertEllipsoidToEGM84TenDegNaturalSplineHeight
             * converts the specified WGS84 ellipsoid height at the specified
             * geodetic coordinates to the equivalent geoid height, 
             * using the EGM84 gravity model and
             * the natural splineinterpolation method.
             *
             * @param[in]   longitude           : Geodetic longitude in radians
             * @param[in]   latitude            : Geodetic latitude in radians
             * @param[in]   ellipsoidHeight     : Ellipsoid height, in meters
             * @param[out]  geoidHeight         : Geoid height, in meters.
             *
             */

            void convertEllipsoidToEGM84TenDegNaturalSplineHeight(
               double longitude,
               double latitude, 
               double ellipsoidHeight,
               double *geoidHeight );

            /**
             * The function convertEllipsoidToEGM84ThirtyMinBiLinearHeight
             * converts the specified WGS84 ellipsoid height at the specified
             * geodetic coordinates to the equivalent geoid height,
             * using the EGM84 gravity model and
             * the bilinear interpolation method.
             *
             * @param[in]   longitude           : Geodetic longitude in radians
             * @param[in]   latitude            : Geodetic latitude in radians
             * @param[in]   ellipsoidHeight     : Ellipsoid height, in meters
             * @param[out]  geoidHeight         : Geoid height, in meters.
             */

            void convertEllipsoidToEGM84ThirtyMinBiLinearHeight(
               double longitude,
               double latitude,
               double ellipsoidHeight,
               double *geoidHeight );

            /**
             * The function convertEllipsoidHeightToEGM2008GeoidHeight
             * converts the specified WGS84 ellipsoid height at the specified
             * geodetic coordinates to the equivalent height above the geoid. This function
             * uses the EGM2008 gravity model, plus the bicubic spline interpolation method.
             *
             * @param[in]   longitude           : Geodetic longitude in radians
             * @param[in]   latitude            : Geodetic latitude in radians
             * @param[in]   ellipsoidHeight     : Ellipsoid height, in meters
             * @param[out]   geoidHeight         : Height above the geoid, meters
             */

            void
            convertEllipsoidHeightToEGM2008GeoidHeight(
               double longitude,
               double latitude,
               double ellipsoidHeight,
               double *geoidHeight );

            /**
             * The function convertEGM96FifteenMinBilinearGeoidToEllipsoidHeight
             * converts the specified WGS84 geoid height at the specified
             * geodetic coordinates to the equivalent ellipsoid height,
             * using the EGM96 gravity model and 
             * the bilinear interpolation method.
             *
             * @param[in]   longitude           : Geodetic longitude in radians
             * @param[in]   latitude            : Geodetic latitude in radians
             * @param[in]   geoidHeight         : Geoid height, in meters.
             * @param[out]  ellipsoidHeight     : Ellipsoid height, in meters
             *
             */
            
            void convertEGM96FifteenMinBilinearGeoidToEllipsoidHeight(
               double longitude,
               double latitude,
               double geoidHeight,
               double *ellipsoidHeight );

            /**
             * The function convertEGM96VariableNaturalSplineToEllipsoidHeight
             * converts the specified WGS84 geoid height at the specified
             * geodetic coordinates to the equivalent ellipsoid height,
             * using the EGM96 gravity model and
             * the natural spline interpolation method.
             *
             * @param[in]   longitude           : Geodetic longitude in radians
             * @param[in]   latitude            : Geodetic latitude in radians
             * @param[in]   geoidHeight         : Geoid height, in meters.
             * @param[out]  ellipsoidHeight     : Ellipsoid height, in meters
             *
             */

            void convertEGM96VariableNaturalSplineToEllipsoidHeight(
               double longitude,
               double latitude,
               double geoidHeight,
               double *ellipsoidHeight );


            /**
             * The function convertEGM84TenDegBilinearToEllipsoidHeight
             * converts the specified WGS84 geoid height at the specified
             * geodetic coordinates to the equivalent ellipsoid height, using
             * the EGM84 gravity model and
             * the bilinear interpolation method.
             *
             * @param[in]   longitude           : Geodetic longitude in radians
             * @param[in]   latitude            : Geodetic latitude in radians
             * @param[in]   geoidHeight         : Geoid height, in meters.
             * @param[out]  ellipsoidHeight     : Ellipsoid height, in meters
             *
             */

            void convertEGM84TenDegBilinearToEllipsoidHeight(
               double longitude,
               double latitude,
               double geoidHeight,
               double *ellipsoidHeight );

            /**
             * The function convertEGM84TenDegNaturalSplineToEllipsoidHeight
             * converts the specified WGS84 geoid height at the specified 
             * geodetic coordinates to the equivalent ellipsoid height,
             * using the EGM84 gravity model and
             * the natural spline interpolation method.
             *
             * @param[in]   longitude           : Geodetic longitude in radians
             * @param[in]   latitude            : Geodetic latitude in radians
             * @param[in]   geoidHeight         : Geoid height, in meters.
             * @param[out]  ellipsoidHeight     : Ellipsoid height, in meters
             *
             */

            void convertEGM84TenDegNaturalSplineToEllipsoidHeight(
               double longitude,
               double latitude,
               double geoidHeight,
               double *ellipsoidHeight );

            /**
             * The function convertEGM84ThirtyMinBiLinearToEllipsoidHeight
             * converts the specified WGS84 geoid height at the specified
             * geodetic coordinates to the equivalent ellipsoid height, using
             * the EGM84 gravity model and the bilinear interpolation method.
             *
             * @param[in]   longitude           : Geodetic longitude in radians
             * @param[in]   latitude            : Geodetic latitude in radians
             * @param[in]   geoidHeight         : Geoid height, in meters.
             * @param[out]  ellipsoidHeight     : Ellipsoid height, in meters
             *
             */

            void convertEGM84ThirtyMinBiLinearToEllipsoidHeight(
               double longitude,
               double latitude,
               double geoidHeight,
               double *ellipsoidHeight );

            /**
             * The function convertEGM2008GeoidHeightToEllipsoidHeight(
             * converts the specified EGM2008 geoid height at the specified
             * geodetic coordinates to the equivalent ellipsoid height.  It uses
             * the EGM2008 gravity model, plus the bicubic spline interpolation method.
             *
             * @param[in]   longitude           : Geodetic longitude in radians
             * @param[in]   latitude            : Geodetic latitude in radians
             * @param[in]   geoidHeight         : Height above the geoid, meters
             * @param[out]  ellipsoidHeight     : Ellipsoid height, in meters
             */

            void convertEGM2008GeoidHeightToEllipsoidHeight(
               double longitude,
               double latitude,
               double geoidHeight,
               double *ellipsoidHeight ); 

         protected:

            /**
             * The constructor creates empty lists which are used 
             * to store the geoid separation data
             * contained in the data files egm84.grd and egm96.grd.
             *
             * The constructor now also instantiates the 
             * appropriate EGM2008 geoid separation interpolator.
             */
            
	    GeoidLibrary();


            GeoidLibrary( const GeoidLibrary &gl );


            GeoidLibrary& operator=( const GeoidLibrary &gl );


         private:

            static CCSThreadMutex mutex;
            static GeoidLibrary* instance;
            static int instanceCount;

            /* List of EGM96 elevations */
            float *egm96GeoidList;

            /* List of EGM84 elevations */
            float *egm84GeoidList;

            /* List of EGM84 thirty minute elevations */
            double *egm84ThirtyMinGeoidList;

            /* Pointer to EGM2008 interpolator object */

            Egm2008GeoidGrid*  egm2008Geoid;

            /**
             * The function loadGeoids reads geoid separation data from a file
             * in the current directory and builds the geoid separation table
             * from it.
             * If the separation file can not be found or accessed, an 
             * error code of GEOID_FILE_OPEN_ERROR is returned,
             * If the separation file is incomplete or improperly formatted,
             * an error code of GEOID_INITIALIZE_ERROR is returned,
             * otherwise GEOID_NO_ERROR is returned.
             * This function must be called before 
             * any of the other functions in this component.
             */
            
            void loadGeoids();

            /**
             * The function initializeEGM96Geoid reads geoid separation data
             * from the egm96.grd file in the current directory 
             * and builds a geoid separation table from it.
             * If the separation file can not be found or accessed,
             * an error code of GEOID_FILE_OPEN_ERROR is returned,
             * If the separation file is incomplete or improperly formatted,
             * an error code of GEOID_INITIALIZE_ERROR is returned,
             * otherwise GEOID_NO_ERROR is returned.
             */

            void initializeEGM96Geoid();

            /**
             * The function initializeEGM84Geoid reads geoid separation data
             * from a file in the current directory and builds
             * the geoid separation table from it.
             * If the separation file can not be found or accessed,
             * an error code of GEOID_FILE_OPEN_ERROR is returned,
             * If the separation file is incomplete or improperly formatted,
             * an error code of GEOID_INITIALIZE_ERROR is returned,
             * otherwise GEOID_NO_ERROR is returned.
             */

            void initializeEGM84Geoid();

            /**
             * The function initializeEGM84ThirtyMinGeoid reads geoid 
             * separation data from a file in the current directory and builds
             * the geoid separation table from it.
             * If the separation file can not be found or accessed,
             * an error code of GEOID_FILE_OPEN_ERROR is returned,
             * If the separation file is incomplete or improperly formatted,
             * an error code of GEOID_INITIALIZE_ERROR is returned,
             * otherwise GEOID_NO_ERROR is returned.
             */

            void initializeEGM84ThirtyMinGeoid();

            /**
             * The function initializeEGM2008Geoid 
             * instantiates one of two geoid-separation objects.
             * The FULL_GRID interpolator reads the entire
             * geoid-separation file into memory upon instantiation,
             * while the AOI_GRID interpolator only reads the
             * grid file's header into memory upon instantiation.
             * 
             * The FULL_GRID interpolator builds its local interpolation
             * windows from data contained in the worldwide, memory-resident,
             * geoid separation grid.  The AOI_GRID interpolator reads intermediate 
             * Area of Interest grids into memory before populating local interpolation 
             * windows.  These AOI grids are much smaller than the EGM2008 worldwide grid,
             * so they load quickly, and they permit high-resolution geoid separations to be
             * computed even on small computer systems.  AOI grids do not need to be reloaded
             * into computer memory if all requested geoid separations are spatially localized.
             * AOI grid reloads are automatic, and they require no action by GeoidLibrary users.
             */

            void initializeEGM2008Geoid();

            /**
             * The private function bilinearInterpolateDoubleHeights returns the
             * height of the WGS84 geoid above or below the WGS84 ellipsoid,
             * at the specified geodetic coordinates, 
             * using a grid of height adjustments
             * and the bilinear interpolation method.
             *
             * @param[in]   longitude      : Geodetic longitude in radians
             * @param[in]   latitude       : Geodetic latitude in radians
             * @param[in]   scale_factor   : Grid scale factor
             * @param[in]   num_cols       : Number of columns in grid
             * @param[in]   num_rows       : Number of rows in grid
             * @param[in]   height_buffer  : Grid of height adjustments, doubles
             * @param[out]  delta_height   : Height Adjustment, in meters.
             *
             */

            void bilinearInterpolateDoubleHeights(
               double longitude,
               double latitude, 
               double scale_factor,
               int num_cols,
               int num_rows,
               double *height_buffer,
               double *delta_height );

            /**
             * The private function bilinearInterpolate returns the height of
             * the WGS84 geoid above or below the WGS84 ellipsoid, at the
             * specified geodetic coordinates, using a grid of height
             * adjustments and the bilinear interpolation method.
             *
             * @param[in]   longitude      : Geodetic longitude in radians
             * @param[in]   latitude       : Geodetic latitude in radians
             * @param[in]   scale_factor   : Grid scale factor
             * @param[in]   num_cols       : Number of columns in grid
             * @param[in]   num_rows       : Number of rows in grid
             * @param[in]   height_buffer  : Grid of height adjustments, floats
             * @param[out]  delta_height   : Height Adjustment, in meters.
             *
             */

            void bilinearInterpolate(
               double longitude,
               double latitude,
               double scale_factor,
               int num_cols,
               int num_rows,
               float  *height_buffer,
               double *delta_height );

            /**
             * The private function naturalSplineInterpolate returns the 
             * height of the WGS84 geoid above or below the WGS84 ellipsoid,
             * at the specified geodetic coordinates, 
             * using a grid of height adjustments
             * and the natural spline interpolation method.
             *
             * @param[in]   longitude     : Geodetic longitude in radians
             * @param[in]   latitude      : Geodetic latitude in radians
             * @param[in]   scale_factor  : Grid scale factor
             * @param[in]   num_cols      : Number of columns in grid
             * @param[in]   num_rows      : Number of rows in grid
             * @param[in]   height_buffer : Grid of height adjustments
             * @param[out]  DeltaHeight   : Height Adjustment, in meters.
             *
             */

            void naturalSplineInterpolate(
               double longitude,
               double latitude,
               double scale_factor,
               int num_cols,
               int num_rows,
               int max_index,
               float  *height_buffer,
               double *delta_height );
      
            /**
             * Delete the singleton.
             */

            static void deleteInstance();
      };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
