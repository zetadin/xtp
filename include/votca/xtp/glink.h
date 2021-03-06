/*
 * Copyright 2009-2017 The VOTCA Development Team (http://www.votca.org)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _VOTCA_KMC_GLINK_H
#define _VOTCA_KMC_GLINK_H
#include <votca/tools/vec.h>

namespace votca {
namespace xtp {

struct GLink {
  int destination;
  double rate;
  votca::tools::vec dr;
  bool decayevent;
  // new stuff for Coulomb interaction
  double Jeff2;
  double reorg_out;
  double initialrate;
  double getValue() { return rate; }
};
}  // namespace xtp
}  // namespace votca

#endif  // _VOTCA_KMC_GLINK_H
