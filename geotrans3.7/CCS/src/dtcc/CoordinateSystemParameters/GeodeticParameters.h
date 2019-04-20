// CLASSIFICATION: UNCLASSIFIED

#ifndef GeodeticParameters_H
#define GeodeticParameters_H

#include "CoordinateSystemParameters.h"
#include "HeightType.h"
#include "DtccApi.h"



namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API GeodeticParameters : public CoordinateSystemParameters
    {
    public:

      GeodeticParameters();
      GeodeticParameters( CoordinateType::Enum _coordinateType );
      GeodeticParameters( CoordinateType::Enum _coordinateType, HeightType::Enum __heightType );
      GeodeticParameters( const GeodeticParameters& gp );

      ~GeodeticParameters();

      GeodeticParameters& operator=( const GeodeticParameters &gp );

      void setHeightType( HeightType::Enum __heightType );

      HeightType::Enum heightType() const;

    private:

      HeightType::Enum _heightType;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
