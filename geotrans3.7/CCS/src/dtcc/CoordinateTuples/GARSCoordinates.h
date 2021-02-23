// CLASSIFICATION: UNCLASSIFIED

#ifndef GARSCoordinates_H
#define GARSCoordinates_H

#include "CoordinateTuple.h"
#include "Precision.h"
#include "DtccApi.h"



namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API GARSCoordinates : public CoordinateTuple
    {
    public:

      GARSCoordinates();
      GARSCoordinates( CoordinateType::Enum _coordinateType );
      GARSCoordinates( CoordinateType::Enum _coordinateType, const char* __GARSString );
      GARSCoordinates( CoordinateType::Enum _coordinateType, const char* __GARSString, Precision::Enum __precision );
      GARSCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, const char* __GARSString, Precision::Enum __precision );
      GARSCoordinates( const GARSCoordinates& gc );

      ~GARSCoordinates();

      GARSCoordinates& operator=( const GARSCoordinates &gc );

      void set( char __GARSString[8] );

      char* GARSString();

      Precision::Enum precision() const;

    private:

      char _GARSString[8];
      Precision::Enum _precision;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
