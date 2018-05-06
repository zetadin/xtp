/* 
 *            Copyright 2009-2017 The VOTCA Development Team
 *                       (http://www.votca.org)
 *
 *      Licensed under the Apache License, Version 2.0 (the "License")
 *
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *              http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef __XTP_EIGEN__H
#define	__XTP_EIGEN__H

#include <votca/xtp/votca_config.h>

#if (GWBSE_DOUBLE)
#define real_gwbse double
#else
#define real_gwbse float
#endif


#if defined(MKL)
#include <mkl.h>
  #define EIGEN_USE_MKL_ALL 
#endif

//! Macro to detect strictly gcc.
//! \details __GNUC__ and __GNUG__ were intended to indicate the GNU compilers.
//! However, they're also defined by Clang/LLVM and Intel compilers to indicate
//! compatibility. This macro can be used to detect strictly gcc and not clang
//! or icc.
#if (defined(__GNUC__) || defined(__GNUG__)) && !(defined(__clang__) || defined(__INTEL_COMPILER))
  #define STRICT_GNUC
#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)
#endif



#if(defined STRICT_GNUC) && GCC_VERSION>70000
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-in-bool-context"
#endif
#include <Eigen/Dense>
#if(defined STRICT_GNUC) && GCC_VERSION>70000
#pragma GCC diagnostic pop
#endif

namespace votca {
    namespace xtp {
        #if defined(MKL)
        const bool XTP_USE_MKL=true;
#else
        const bool XTP_USE_MKL=false;
#endif
       
        
        

 typedef Eigen::Matrix<real_gwbse, Eigen::Dynamic, Eigen::Dynamic> MatrixXfd;
 typedef Eigen::Matrix<real_gwbse, Eigen::Dynamic, 1> VectorXfd;


    }}





#endif	/*XTP_EIGEN__H */
