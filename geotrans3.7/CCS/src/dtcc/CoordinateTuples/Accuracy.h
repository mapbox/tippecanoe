// CLASSIFICATION: UNCLASSIFIED

#ifndef Accuracy_H
#define Accuracy_H

#include "DtccApi.h"


namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API Accuracy
    {
    public:

      Accuracy();
      Accuracy( double __circularError90, double __linearError90, double __sphericalError90 );
      Accuracy( const Accuracy &a );

      ~Accuracy( void );

      Accuracy& operator=( const Accuracy &a );

      void set( double __circularError90, double __linearError90, double __sphericalError90 );

      void setCircularError90( double __circularError90 );
      void setLinearError90( double __linearError90 );
      void setSphericalError90( double __sphericalError90 );

      double circularError90();
      double linearError90();
      double sphericalError90();

    private:

      double _circularError90;
      double _linearError90;
      double _sphericalError90;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
