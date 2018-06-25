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

#ifndef _VOTCA_XTP_LINALG_H
#define _VOTCA_XTP_LINALG_H
#include <votca/xtp/eigen.h>

//this file is temporary till conversion to eigen in all of votca is realized

namespace votca {
namespace xtp {

    bool linalg_eigenvalues(Eigen::MatrixXd&A, Eigen::VectorXd &E, Eigen::MatrixXd&V , int nmax );
     
    bool linalg_eigenvalues(Eigen::MatrixXf&A, Eigen::VectorXf &E, Eigen::MatrixXf&V , int nmax );
     
    
}
}

#endif /* _VOTCA_XTP_LINALG_H */
