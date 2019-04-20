// CLASSIFICATION: UNCLASSIFIED

#ifndef CoordinateTuple_H
#define CoordinateTuple_H

#include "CoordinateType.h"
#include "Precision.h"
#include "DtccApi.h"


namespace MSP
{
   namespace CCS
   {
      class MSP_DTCC_API CoordinateTuple
      {
      public:

         CoordinateTuple();
         CoordinateTuple( MSP::CCS::CoordinateType::Enum __coordinateType );
         CoordinateTuple(
            MSP::CCS::CoordinateType::Enum __coordinateType,
            const char*                    __warningMessage );
         CoordinateTuple( const CoordinateTuple& ct );

         virtual ~CoordinateTuple();

         CoordinateTuple& operator=( const CoordinateTuple &ct );

         void set(
            MSP::CCS::CoordinateType::Enum __coordinateType,
            const char*                    __warningMessage,
            const char*                    __errorMessage);

         void setCoordinateType(
            MSP::CCS::CoordinateType::Enum __coordinateType );
         CoordinateType::Enum coordinateType() const;

         void setErrorMessage( const char* __errorMessage );
         const char* errorMessage() const;

         void setWarningMessage( const char* __warningMessage );
         const char* warningMessage() const;

         virtual Precision::Enum precision() const
         {
            return Precision::tenThousandthOfSecond;
         }

      protected:

         CoordinateType::Enum _coordinateType;
         char                 _errorMessage[500];
         char                 _warningMessage[500];
      };
   }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
