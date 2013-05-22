/*
 *            Copyright 2009-2012 The VOTCA Development Team
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

#ifndef __QMTOOLFACTORY__H
#define	__QMTOOLFACTORY__H

#include <votca/tools/objectfactory.h>
#include "qmtool.h"

namespace votca { namespace ctp {

using namespace std;

class QMToolFactory : public ObjectFactory<std::string, QMTool>
{
    
private:
    QMToolFactory() {}
    
public:
    
    static void RegisterAll(void);

    friend QMToolFactory &QMTools();
};

inline QMToolFactory &QMTools()
{
    static QMToolFactory _instance;
    return _instance;
}

}}

#endif	/* __QMTOOLFACTORY__H */
