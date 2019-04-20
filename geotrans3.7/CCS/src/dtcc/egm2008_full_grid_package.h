
// CLASSIFICATION: UNCLASSIFIED

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//   File name: egm2008_full_grid_package.h                                   //
//                                                                            //
//   Description of this module:                                              //
//      Utility software that interpolates EGM 2008                           //
//      geoid heights from one of NGA's geoid height grids.                   //
//                                                                            //
//      This interpolator loads the worldwide EGM 2008 grid upon              //
//      instantiation, and it interpolates from the worldwide grid.           //
//                                                                            //
//      This interpolator gives exactly the same results as                   //
//      the companion egm2008_aoi_grid_package's interpolator.                //
//      However, this interpolator is faster when                             //
//      users are requesting tens of thousands of geoid                       //
//      heights at widely dispersed horizontal locations.                     //
//                                                                            //
//   Revision History:                                                        //
//   Date         Name          Description                                   //
//   -----------  ------------  ----------------------------------------------//
//   19 Nov 2010  RD Craig      Release                                       //
//   11 Feg 2011  RD Craig      Upgrades following code review                //
//   30 May 2013  RD Craig      MSP 1.3: ER29758                              //
//                              Added second constructor to                   //
//                              permit multiple geoid-height grids            //
//                              when assessing relative interpolation errors. //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef EGM2008_FULL_GRID_PACKAGE_H
#define EGM2008_FULL_GRID_PACKAGE_H

// This file declares a C++ class
// that interpolates EGM 2008 geoid heights from a
// reformatted version of NGA's geoid-height grid.

// THIS DERIVED CLASS IMPLEMENTS COMPUTATIONAL
// DETAILS SPECIFIC TO THE EGM 2008 FULL-GRID ALGORITHM.

#include "DtccApi.h"
#include "egm2008_geoid_grid.h"

namespace MSP
{
   class MSP_DTCC_API Egm2008FullGrid : public Egm2008GeoidGrid {
      
      protected:

      // heightGrid:         A pointer to a
      //                     one-dimensional array containing
      //                     the reformatted geoid-height grid.

      float*                 _heightGrid;

      public:

      // Basic functions .....

      Egm2008FullGrid( void );

      Egm2008FullGrid( const std::string  &gridFname );  // new 5/30/2013
 
      Egm2008FullGrid( const Egm2008FullGrid& oldGrid );

      ~Egm2008FullGrid( void );

      Egm2008FullGrid&
      operator = ( const Egm2008FullGrid& oldGrid );

      // User functions .....
 
      // geoidHeight:        A function that interpolates
      //                     local geoid height (meters) from
      //                     a reformatted geoid height grid;
      //                     it uses bi-cubic spline interpolation.

      virtual int
      geoidHeight(
         int     wSize,                     // input
         double  latitude,                  // input
         double  longitude,                 // input
         double& gHeight );                 // output

      protected:
 
      // geoidHeight:        A function that interpolates
      //                     local geoid height (meters) from
      //                     a reformatted geoid height grid;
      //                     it uses bilinear interpolation.

      virtual int
      geoidHeight(
         double  latitude,                  // input
         double  longitude,                 // input
         double& gHeight );                 // output

      // loadGrid:           A function that 
      //                     retrieves a reformatted
      //                     EGM 2008 worldwide geoid height grid.

      int
      loadGrid( void );

   }; // End of Egm2008FullGrid class declaration

}  // End of namespace block

#endif

// CLASSIFICATION: UNCLASSIFIED

