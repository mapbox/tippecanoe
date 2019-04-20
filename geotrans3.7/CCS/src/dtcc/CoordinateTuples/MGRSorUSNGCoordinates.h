// CLASSIFICATION: UNCLASSIFIED

#ifndef MGRSorUSNGCoordinates_H
#define MGRSorUSNGCoordinates_H

#include "CoordinateTuple.h"
#include "Precision.h"
#include "DtccApi.h"



namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API MGRSorUSNGCoordinates : public CoordinateTuple
    {
    public:

      MGRSorUSNGCoordinates();
      MGRSorUSNGCoordinates( CoordinateType::Enum _coordinateType );
      MGRSorUSNGCoordinates( CoordinateType::Enum _coordinateType, const char* __MGRSString );
      MGRSorUSNGCoordinates( CoordinateType::Enum _coordinateType, const char* __MGRSString, Precision::Enum __precision );
      MGRSorUSNGCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, const char* __MGRSString, Precision::Enum __precision );
      MGRSorUSNGCoordinates( const MGRSorUSNGCoordinates& c );

      ~MGRSorUSNGCoordinates();

      MGRSorUSNGCoordinates& operator=( const MGRSorUSNGCoordinates &c );

      void set( char __MGRSString[21] );

      char* MGRSString();

      Precision::Enum precision() const;

    private:

      char _MGRSString[21];
      Precision::Enum _precision;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
