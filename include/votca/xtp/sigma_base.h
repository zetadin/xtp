/*
 *            Copyright 2009-2018 The VOTCA Development Team
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

#ifndef _VOTCA_XTP_SIGMA_BASE_H
#define _VOTCA_XTP_SIGMA_BASE_H
#include <votca/ctp/logger.h>
#include <votca/xtp/eigen.h>

namespace votca {
namespace xtp {

class TCMatrix_gwbse;
class RPA;

class Sigma_base {
 public:
  Sigma_base(TCMatrix_gwbse& Mmn, const RPA& rpa) : _Mmn(Mmn), _rpa(rpa){};

  virtual ~Sigma_base(){};

  struct options {
    int homo;
    int qpmin;
    int qpmax;
    int rpamin;
  };

  void configure(options opt) {
    _opt = opt;
    _qptotal = opt.qpmax - opt.qpmin + 1;
  }

  // Calculates Full exchange matrix
  Eigen::MatrixXd CalcExchange() const;

  // Sets up the screening parametrisation
  virtual void PrepareScreening() = 0;
  // Calculates Sigma_c diag elements
  virtual Eigen::VectorXd CalcCorrelationDiag(
      const Eigen::VectorXd& frequencies) const = 0;
  // Calculates Sigma_c offdiag elements
  virtual Eigen::MatrixXd CalcCorrelationOffDiag(
      const Eigen::VectorXd& frequencies) const = 0;

 protected:
  options _opt;
  TCMatrix_gwbse& _Mmn;
  const RPA& _rpa;

  int _qptotal;
};
}  // namespace xtp
}  // namespace votca

#endif /* _VOTCA_XTP_SIGMA_BASE_H */
