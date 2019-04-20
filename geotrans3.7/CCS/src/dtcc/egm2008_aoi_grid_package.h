
// CLASSIFICATION: UNCLASSIFIED

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//   File name: egm2008_aoi_grid_package.h                                    //
//                                                                            //
//   Description of this module:                                              //
//      Utility software that interpolates EGM 2008 geoid                     //
//      heights from one of NGA's reformatted geoid height grids.             //
//                                                                            //
//      This interpolator uses Area of Interest (AOI)                         //
//      geoid height grids, not the worldwide geoid height grid.              //
//                                                                            //
//      This interpolator does not load an Area of Interest (AOI)             //
//      geoid height grid until a user first requests a geoid height.         //
//      The interpolator then loads an AOI grid centered near the point of    // 
//      interest, and it interpolates local geoid height from the AOI grid.   //
//      This interpolator re-uses the AOI grid until a subsequent point of    //
//      interest lies outside the AOI.  The interpolator then loads a         //
//      new AOI grid centered at the new horizontal location of interest.     //
//                                                                            //
//      This interpolator gives exactly the same results as                   //
//      the companion egm2008_full_grid_package's interpolator.               //
//      However, the AOI interpolator requires far less computer memory.      //
//                                                                            //
//   Revision History:                                                        //
//   Date         Name          Description                                   //
//   -----------  ------------  ----------------------------------------------//
//   19 Nov 2010  RD Craig      Release                                       //
//   11 Feb 2011  RD Craig      Updates following code review                 //
//   30 May 2013  RD Craig      MSP 1.3: ER29758                              //
//                              Added second constructor to                   //
//                              permit multiple geoid-height grids            //
//                              when assessing relative interpolation errors. //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef EGM2008_AOI_GRID_PACKAGE_H
#define EGM2008_AOI_GRID_PACKAGE_H

// This file declares a C++ class
// that interpolates EGM 2008 geoid heights from a
// reformatted version of NGA's worldwide geoid height grids.

#include "DtccApi.h"
#include "egm2008_geoid_grid.h"

namespace MSP
{

   class MSP_DTCC_API Egm2008AoiGrid : public Egm2008GeoidGrid {
      
      protected:

      // _maxAoiColIndex:    The AOI grid's maximum column index;
      //                     this is referenced to the worldwide grid.

      int                    _maxAoiColIndex;

      // _minAoiColIndex:    The AOI grid's minimum column index;
      //                     this is referenced to the worldwide grid.

      int                    _minAoiColIndex;

      // _maxAoiRowIndex:    The AOI grid's maximum row index;
      //                     this is referenced to the worldwide grid.

      int                    _maxAoiRowIndex;

      // _minAoiRowIndex:    The AOI grid's minimum row index;
      //                     this is referenced to the worldwide grid.

      int                    _minAoiRowIndex;

      // nAoiCols:           The number of columns in the AOI grid.

      int                    _nAoiCols;

      // nAoiRows:           The number of rows in the AOI grid.

      int                    _nAoiRows;

      // nomAoiCols:         Nominal number of columns in the AOI 
      //                     grid; actual number is latitude dependent.

      int                    _nomAoiCols;

      // nomAoiRows:         Nominal number of rows in the AOI grid.

      int                    _nomAoiRows;

      // heightGrid:         A pointer to a
      //                     one-dimensional array containing a
      //                     part of the reformatted geoid-height grid.

      float*                 _heightGrid;

      public:

      // Basic functions .....

      Egm2008AoiGrid( void );

      Egm2008AoiGrid( const std::string  &gridFname );  // new 5/30/2013
 
      Egm2008AoiGrid( const Egm2008AoiGrid& oldGrid );

      ~Egm2008AoiGrid( void );

      Egm2008AoiGrid&
      operator = ( const Egm2008AoiGrid& oldGrid );

      // User functions .....
 
      // geoidHeight:        A function that interpolates
      //                     local geoid height (meters) from
      //                     a reformatted EGM 2008 geoid height grid;
      //                     this function uses bi-cubic spline interpolation.

      virtual int
      geoidHeight(
         int     wSize,                     // input
         double  latitude,                  // input
         double  longitude,                 // input
         double& gHeight );                 // output

      protected:
 
      // geoidHeight:        A function that interpolates
      //                     local geoid height (meters) from
      //                     a reformatted EGM 2008 geoid height grid;
      //                     this function uses bilinear interpolation.

      virtual int
      geoidHeight(
         double  latitude,                  // input
         double  longitude,                 // input
         double& gHeight );                 // output

      // loadAoiParms:       A function that loads an AOI grid's
      //                     parameters relative to an input worldwide grid.

      int
      loadAoiParms( 
         int i0, int j0 );

      // loadGrid:           A function that loads an AOI grid from
      //                     a reformatted EGM 2008 worldwide geoid height grid.

      int
      loadGrid( void );

      // loadGridMetadata:   A function that loads worldwide EGM 2008
      //                     grid metadata from a reformatted worldwide grid file. 

      int
      loadGridMetadata( void );

   }; // End of Egm2008AoiGrid class declaration

}  // End of namespace block

#endif

// CLASSIFICATION: UNCLASSIFIED

