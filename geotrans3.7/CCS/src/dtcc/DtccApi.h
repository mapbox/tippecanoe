// CLASSIFICATION: UNCLASSIFIED

/******************************************************************************
 * Filename: DtccApi.h
 *
 * Copyright BAE Systems Inc. 2012  ALL RIGHTS RESERVED
 *
 * Description: Windows DLL Import/Export symbol definitions.
 *
 ******************************************************************************/

#ifndef _MSP_DTCC_API_H
#define _MSP_DTCC_API_H

#if defined(WIN32)
#   if defined(_USRDLL)
#      if defined(MSP_CCS_EXPORTS)
#         define MSP_DTCC_API __declspec(dllexport)
#         define MSP_DTCC_TEMPLATE_EXPORT 
#      elif defined(MSP_CCS_IMPORTS)
#         define MSP_DTCC_API __declspec(dllimport)
#         define MSP_DTCC_TEMPLATE_EXPORT extern
#      else
#         define MSP_DTCC_API
#         define MSP_DTCC_TEMPLATE_EXPORT
#      endif
#   else
#      define MSP_DTCC_API
#      define MSP_DTCC_TEMPLATE_EXPORT 
#   endif
#else
#   define MSP_DTCC_API
#endif

#endif // _MSP_DTCC_API_H

// CLASSIFICATION: UNCLASSIFIED
