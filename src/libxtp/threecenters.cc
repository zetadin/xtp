/* 
 *            Copyright 2009-2016 The VOTCA Development Team
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

// Overload of uBLAS prod function with MKL/GSL implementations


#include <votca/xtp/threecenters.h>


using namespace votca::tools;

namespace votca {
    namespace xtp {
        namespace ub = boost::numeric::ublas;

 
        
        /*
         * Calculate 3-center overlap integrals 
         *    S_{abc} = int{ phi_a^DFT phi_b^GW phi_c^DFT d3r }
         * for a given set of a b c as in 
         *    Obara, Saika, J. Chem. Phys. 84, 3963 (1986)
         * section II.B for cartesian Gaussians, then transforming
         * to spherical (angular momentum) Gaussians ("complete" shells
         * from S to Lmax, and finally cutting out those angular momentum 
         * components actually present in shell-shell-shell combination.
         * Currently supported for 
         *      S,P,D   functions in DFT basis and 
         *      S,P,D,F functions in GW  basis
         * 
         */
        bool TCrawMatrix::FillThreeCenterOLBlock(ub::matrix<double>& _subvector, AOShell* _shell_gw, AOShell* _shell_alpha, AOShell* _shell_gamma) {
	  //bool TCrawMatrix::FillThreeCenterOLBlock(ub::matrix<float>& _subvector, AOShell* _shell_gw, AOShell* _shell_alpha, AOShell* _shell_gamma) {
            const double pi = boost::math::constants::pi<double>();
              // get shell positions
            const vec& _pos_gw = _shell_gw->getPos();
            const vec& _pos_alpha = _shell_alpha->getPos();
            const vec& _pos_gamma = _shell_gamma->getPos();

            // shell info, only lmax tells how far to go
            int _lmax_gw = _shell_gw->getLmax();
            int _lmax_alpha = _shell_alpha->getLmax();
            int _lmax_gamma = _shell_gamma->getLmax();

            // set size of internal block for recursion
            int _ngw = this->getBlockSize(_lmax_gw);
            int _nalpha = this->getBlockSize(_lmax_alpha);
            int _ngamma = this->getBlockSize(_lmax_gamma);
            // definition of cutoff for contribution
            
            const double gwaccuracy = 1.e-9; // should become an OPTION
            
            bool _does_contribute = false;
            
             typedef std::vector< AOGaussianPrimitive* >::iterator GaussianIterator;
                // iterate over Gaussians in this _shell_row
            for ( GaussianIterator italpha = _shell_alpha->firstGaussian(); italpha != _shell_alpha->lastGaussian(); ++italpha){
            // iterate over Gaussians in this _shell_col
                const double _decay_alpha = (*italpha)->decay;
            
                for ( GaussianIterator itgamma = _shell_gamma->firstGaussian(); itgamma != _shell_gamma->lastGaussian(); ++itgamma){
                    const double& _decay_gamma = (*itgamma)->decay;
                    // check third threshold
                    vec _diff = _pos_alpha - _pos_gamma;
                    double test = _decay_alpha * _decay_gamma * _diff*_diff;
                    
                    for ( GaussianIterator itgw = _shell_gw->firstGaussian(); itgw != _shell_gw->lastGaussian(); ++itgw){
            // get decay constants (this all is still valid only for uncontracted functions)
                        const double _decay_gw = (*itgw)->decay;
            
 
            double threshold = -(_decay_alpha + _decay_gamma + _decay_gw) * log(gwaccuracy);
            // check third threshold
            if (test > threshold) { continue; }
            // check first threshold
            _diff = _pos_alpha - _pos_gw;
            
            test += _decay_alpha * _decay_gw * _diff*_diff;
            if (test > threshold) { continue; }

            // check second threshold
            _diff = _pos_gamma - _pos_gw;
            test += _decay_gamma * _decay_gw * _diff*_diff;
            if (test > threshold) { continue; }

            
            
            

            // if all threshold test are passed, start evaluating

            // some helpers
            double fak = 0.5 / (_decay_alpha + _decay_gw + _decay_gamma);
            double fak2 = 2.0 * fak;
            
            //double fak4=  4.0 * fak;
            
            double expo = _decay_alpha * _decay_gamma * (_pos_alpha - _pos_gamma)*(_pos_alpha - _pos_gamma)
                    + _decay_gamma * _decay_gw * (_pos_gamma - _pos_gw) * (_pos_gamma - _pos_gw)
                    + _decay_alpha * _decay_gw * (_pos_alpha - _pos_gw) * (_pos_alpha - _pos_gw);

            
            double prefak = pow(8.0 * _decay_alpha * _decay_gamma * _decay_gw / pi, 0.75) * pow(fak2, 1.5);

            double value = prefak * exp(-fak2 * expo);
            double fak3 = 3.0 * fak;
            // check if it contributes
            if (value < gwaccuracy) { continue; }

            //double _dist1=(_pos_alpha - _pos_gamma)*(_pos_alpha - _pos_gamma);
            //double _dist2=(_pos_gamma - _pos_gw) * (_pos_gamma - _pos_gw);
            //double _dist3=(_pos_alpha - _pos_gw) * (_pos_alpha - _pos_gw);
            
                  
            vec gvv = fak2 * (_decay_alpha * _pos_alpha + _decay_gw * _pos_gw + _decay_gamma * _pos_gamma);
            vec gma = gvv - _pos_alpha;
            vec gmb = gvv - _pos_gamma;
            vec gmc = gvv - _pos_gw; 
            
            double gma0 = gma.getX();
            double gmb0 = gmb.getX();
            double gmc0 = gmc.getX();

            double gma1 = gma.getY();
            double gmb1 = gmb.getY();
            double gmc1 = gmc.getY();

            double gma2 = gma.getZ();
            double gmb2 = gmb.getZ();
            double gmc2 = gmc.getZ();

            
            // get s-s-s element
            

            _does_contribute = true;
            // if it does, go on and create multiarray
            typedef boost::multi_array<double, 3> ma_type;
            typedef boost::multi_array_types::extent_range range;
            typedef ma_type::index index;
            ma_type::extent_gen extents;
            ma_type S;
            S.resize(extents[ range(1, _nalpha + 1) ][ range(1, _ngw + 1) ][ range(1, _ngamma + 1)]);

            //cout << S.shape()[0]<< " : "<< S.shape()[1]<< " : "<< S.shape()[2]<<endl;
            
            // now fill s-s-s element
            S[1][1][1] = value;

            // s-p-s elements
            if (_lmax_gw >= 1 ) {
                S[1][2][1] = gmc0 * S[1][1][1];
                S[1][3][1] = gmc1 * S[1][1][1];
                S[1][4][1] = gmc2 * S[1][1][1];
            }

            // s-d-s elements
            if (_lmax_gw >= 2 ) {
                S[1][8][1] = gmc0 * S[1][2][1] + fak * S[1][1][1];
                S[1][5][1] = gmc1 * S[1][2][1];
                S[1][6][1] = gmc2 * S[1][2][1];
                S[1][9][1] = gmc1 * S[1][3][1] + fak * S[1][1][1];
                S[1][7][1] = gmc2 * S[1][3][1];
                S[1][10][1] = gmc2 * S[1][4][1] + fak * S[1][1][1];
            }

            // p-s-s elements
            if (_lmax_alpha >= 1) {
                S[2][1][1] = gma0 * S[1][1][1];
                S[3][1][1] = gma1 * S[1][1][1];
                S[4][1][1] = gma2 * S[1][1][1];
            }

            // p-p-s elements
            if (_lmax_alpha >= 1 && _lmax_gw >= 1) {
                S[2][2][1] = gma0 * S[1][2][1] + fak * S[1][1][1];
                S[3][2][1] = gma1 * S[1][2][1];
                S[4][2][1] = gma2 * S[1][2][1];
                S[2][3][1] = gma0 * S[1][3][1];
                S[3][3][1] = gma1 * S[1][3][1] + fak * S[1][1][1];
                S[4][3][1] = gma2 * S[1][3][1];
                S[2][4][1] = gma0 * S[1][4][1];
                S[3][4][1] = gma1 * S[1][4][1];
                S[4][4][1] = gma2 * S[1][4][1] + fak * S[1][1][1];
            }

            // p-d-s elements
            if (_lmax_alpha >= 1 && _lmax_gw >= 2) {
                S[2][5][1] = gma0 * S[1][5][1] + fak * S[1][3][1];
                S[3][5][1] = gma1 * S[1][5][1] + fak * S[1][2][1];
                S[4][5][1] = gma2 * S[1][5][1];
                S[2][6][1] = gma0 * S[1][6][1] + fak * S[1][4][1];
                S[3][6][1] = gma1 * S[1][6][1];
                S[4][6][1] = gma2 * S[1][6][1] + fak * S[1][2][1];
                S[2][7][1] = gma0 * S[1][7][1];
                S[3][7][1] = gma1 * S[1][7][1] + fak * S[1][4][1];
                S[4][7][1] = gma2 * S[1][7][1] + fak * S[1][3][1];
                S[2][8][1] = gma0 * S[1][8][1] + fak2 * S[1][2][1];
                S[3][8][1] = gma1 * S[1][8][1];
                S[4][8][1] = gma2 * S[1][8][1];
                S[2][9][1] = gma0 * S[1][9][1];
                S[3][9][1] = gma1 * S[1][9][1] + fak2 * S[1][3][1];
                S[4][9][1] = gma2 * S[1][9][1];
                S[2][10][1] = gma0 * S[1][10][1];
                S[3][10][1] = gma1 * S[1][10][1];
                S[4][10][1] = gma2 * S[1][10][1] + fak2 * S[1][4][1];
            }

            // s-s-p elements
            if ( _lmax_gamma >= 1) {
                S[1][1][2] = gmb0 * S[1][1][1];
                S[1][1][3] = gmb1 * S[1][1][1];
                S[1][1][4] = gmb2 * S[1][1][1];
            }

            // s-p-p elements
            if (_lmax_gw >= 1 && _lmax_gamma >= 1) {
                S[1][2][2] = gmc0 * S[1][1][2] + fak * S[1][1][1];
                S[1][3][2] = gmc1 * S[1][1][2];
                S[1][4][2] = gmc2 * S[1][1][2];
                S[1][2][3] = gmc0 * S[1][1][3];
                S[1][3][3] = gmc1 * S[1][1][3] + fak * S[1][1][1];
                S[1][4][3] = gmc2 * S[1][1][3];
                S[1][2][4] = gmc0 * S[1][1][4];
                S[1][3][4] = gmc1 * S[1][1][4];
                S[1][4][4] = gmc2 * S[1][1][4] + fak * S[1][1][1];
            }

            // s-d-p elements
            if (_lmax_gw >= 2 && _lmax_gamma >= 1) {
                S[1][8][2] = gmc0 * S[1][2][2] + fak * (S[1][1][2] + S[1][2][1]);
                S[1][5][2] = gmc1 * S[1][2][2];
                S[1][6][2] = gmc2 * S[1][2][2];
                S[1][8][3] = gmc0 * S[1][2][3] + fak * S[1][1][3];
                S[1][5][3] = gmc1 * S[1][2][3] + fak * S[1][2][1];
                S[1][6][3] = gmc2 * S[1][2][3];
                S[1][8][4] = gmc0 * S[1][2][4] + fak * S[1][1][4];
                S[1][5][4] = gmc1 * S[1][2][4];
                S[1][6][4] = gmc2 * S[1][2][4] + fak * S[1][2][1];
                S[1][9][2] = gmc1 * S[1][3][2] + fak * S[1][1][2];
                S[1][7][2] = gmc2 * S[1][3][2];
                S[1][9][3] = gmc1 * S[1][3][3] + fak * (S[1][1][3] + S[1][3][1]);
                S[1][7][3] = gmc2 * S[1][3][3];
                S[1][9][4] = gmc1 * S[1][3][4] + fak * S[1][1][4];
                S[1][7][4] = gmc2 * S[1][3][4] + fak * S[1][3][1];
                S[1][10][2] = gmc2 * S[1][4][2] + fak * S[1][1][2];
                S[1][10][3] = gmc2 * S[1][4][3] + fak * S[1][1][3];
                S[1][10][4] = gmc2 * S[1][4][4] + fak * (S[1][1][4] + S[1][4][1]);
            }

            // p-s-p elements
            if (_lmax_alpha >= 1 && _lmax_gamma >= 1) {
                S[2][1][2] = gma0 * S[1][1][2] + fak * S[1][1][1];
                S[3][1][2] = gma1 * S[1][1][2];
                S[4][1][2] = gma2 * S[1][1][2];
                S[2][1][3] = gma0 * S[1][1][3];
                S[3][1][3] = gma1 * S[1][1][3] + fak * S[1][1][1];
                S[4][1][3] = gma2 * S[1][1][3];
                S[2][1][4] = gma0 * S[1][1][4];
                S[3][1][4] = gma1 * S[1][1][4];
                S[4][1][4] = gma2 * S[1][1][4] + fak * S[1][1][1];
            }

            // p-p-p elements
            if (_lmax_alpha >= 1 && _lmax_gw >= 1 && _lmax_gamma >= 1) {
                S[2][2][2] = gma0 * S[1][2][2] + fak * (S[1][1][2] + S[1][2][1]);
                S[3][2][2] = gma1 * S[1][2][2];
                S[4][2][2] = gma2 * S[1][2][2];
                S[2][2][3] = gma0 * S[1][2][3] + fak * S[1][1][3];
                S[3][2][3] = gma1 * S[1][2][3] + fak * S[1][2][1];
                S[4][2][3] = gma2 * S[1][2][3];
                S[2][2][4] = gma0 * S[1][2][4] + fak * S[1][1][4];
                S[3][2][4] = gma1 * S[1][2][4];
                S[4][2][4] = gma2 * S[1][2][4] + fak * S[1][2][1];
                S[2][3][2] = gma0 * S[1][3][2] + fak * S[1][3][1];
                S[3][3][2] = gma1 * S[1][3][2] + fak * S[1][1][2];
                S[4][3][2] = gma2 * S[1][3][2];
                S[2][3][3] = gma0 * S[1][3][3];
                S[3][3][3] = gma1 * S[1][3][3] + fak * (S[1][1][3] + S[1][3][1]);
                S[4][3][3] = gma2 * S[1][3][3];
                S[2][3][4] = gma0 * S[1][3][4];
                S[3][3][4] = gma1 * S[1][3][4] + fak * S[1][1][4];
                S[4][3][4] = gma2 * S[1][3][4] + fak * S[1][3][1];
                S[2][4][2] = gma0 * S[1][4][2] + fak * S[1][4][1];
                S[3][4][2] = gma1 * S[1][4][2];
                S[4][4][2] = gma2 * S[1][4][2] + fak * S[1][1][2];
                S[2][4][3] = gma0 * S[1][4][3];
                S[3][4][3] = gma1 * S[1][4][3] + fak * S[1][4][1];
                S[4][4][3] = gma2 * S[1][4][3] + fak * S[1][1][3];
                S[2][4][4] = gma0 * S[1][4][4];
                S[3][4][4] = gma1 * S[1][4][4];
                S[4][4][4] = gma2 * S[1][4][4] + fak * (S[1][1][4] + S[1][4][1]);
            }

            // p-d-p elements
            if (_lmax_alpha >= 1 && _lmax_gw >= 2 && _lmax_gamma >= 1) {
                S[2][5][2] = gma0 * S[1][5][2] + fak * (S[1][3][2] + S[1][5][1]);
                S[3][5][2] = gma1 * S[1][5][2] + fak * S[1][2][2];
                S[4][5][2] = gma2 * S[1][5][2];
                S[2][5][3] = gma0 * S[1][5][3] + fak * S[1][3][3];
                S[3][5][3] = gma1 * S[1][5][3] + fak * (S[1][2][3] + S[1][5][1]);
                S[4][5][3] = gma2 * S[1][5][3];
                S[2][5][4] = gma0 * S[1][5][4] + fak * S[1][3][4];
                S[3][5][4] = gma1 * S[1][5][4] + fak * S[1][2][4];
                S[4][5][4] = gma2 * S[1][5][4] + fak * S[1][5][1];
                S[2][6][2] = gma0 * S[1][6][2] + fak * (S[1][4][2] + S[1][6][1]);
                S[3][6][2] = gma1 * S[1][6][2];
                S[4][6][2] = gma2 * S[1][6][2] + fak * S[1][2][2];
                S[2][6][3] = gma0 * S[1][6][3] + fak * S[1][4][3];
                S[3][6][3] = gma1 * S[1][6][3] + fak * S[1][6][1];
                S[4][6][3] = gma2 * S[1][6][3] + fak * S[1][2][3];
                S[2][6][4] = gma0 * S[1][6][4] + fak * S[1][4][4];
                S[3][6][4] = gma1 * S[1][6][4];
                S[4][6][4] = gma2 * S[1][6][4] + fak * (S[1][2][4] + S[1][6][1]);
                S[2][7][2] = gma0 * S[1][7][2] + fak * S[1][7][1];
                S[3][7][2] = gma1 * S[1][7][2] + fak * S[1][4][2];
                S[4][7][2] = gma2 * S[1][7][2] + fak * S[1][3][2];
                S[2][7][3] = gma0 * S[1][7][3];
                S[3][7][3] = gma1 * S[1][7][3] + fak * (S[1][4][3] + S[1][7][1]);
                S[4][7][3] = gma2 * S[1][7][3] + fak * S[1][3][3];
                S[2][7][4] = gma0 * S[1][7][4];
                S[3][7][4] = gma1 * S[1][7][4] + fak * S[1][4][4];
                S[4][7][4] = gma2 * S[1][7][4] + fak * (S[1][3][4] + S[1][7][1]);
                S[2][8][2] = gma0 * S[1][8][2] + fak * (S[1][2][2] + S[1][2][2] + S[1][8][1]);
                S[3][8][2] = gma1 * S[1][8][2];
                S[4][8][2] = gma2 * S[1][8][2];
                S[2][8][3] = gma0 * S[1][8][3] + fak2 * S[1][2][3];
                S[3][8][3] = gma1 * S[1][8][3] + fak * S[1][8][1];
                S[4][8][3] = gma2 * S[1][8][3];
                S[2][8][4] = gma0 * S[1][8][4] + fak * (2 * S[1][2][4]);
                S[3][8][4] = gma1 * S[1][8][4];
                S[4][8][4] = gma2 * S[1][8][4] + fak * S[1][8][1];
                S[2][9][2] = gma0 * S[1][9][2] + fak * S[1][9][1];
                S[3][9][2] = gma1 * S[1][9][2] + fak2 * S[1][3][2];
                S[4][9][2] = gma2 * S[1][9][2];
                S[2][9][3] = gma0 * S[1][9][3];
                S[3][9][3] = gma1 * S[1][9][3] + fak * (S[1][3][3] + S[1][3][3] + S[1][9][1]);
                S[4][9][3] = gma2 * S[1][9][3];
                S[2][9][4] = gma0 * S[1][9][4];
                S[3][9][4] = gma1 * S[1][9][4] + fak2 * S[1][3][4];
                S[4][9][4] = gma2 * S[1][9][4] + fak * S[1][9][1];
                S[2][10][2] = gma0 * S[1][10][2] + fak * S[1][10][1];
                S[3][10][2] = gma1 * S[1][10][2];
                S[4][10][2] = gma2 * S[1][10][2] + fak2 * S[1][4][2];
                S[2][10][3] = gma0 * S[1][10][3];
                S[3][10][3] = gma1 * S[1][10][3] + fak * S[1][10][1];
                S[4][10][3] = gma2 * S[1][10][3] + fak2 * S[1][4][3];
                S[2][10][4] = gma0 * S[1][10][4];
                S[3][10][4] = gma1 * S[1][10][4];
                S[4][10][4] = gma2 * S[1][10][4] + fak * (S[1][4][4] + S[1][4][4] + S[1][10][1]);
            }

            // s-s-d elements
            if (_lmax_gamma >= 2) {
                S[1][1][8] = gmb0 * S[1][1][2] + fak * S[1][1][1];
                S[1][1][5] = gmb1 * S[1][1][2];
                S[1][1][6] = gmb2 * S[1][1][2];
                S[1][1][9] = gmb1 * S[1][1][3] + fak * S[1][1][1];
                S[1][1][7] = gmb2 * S[1][1][3];
                S[1][1][10] = gmb2 * S[1][1][4] + fak * S[1][1][1];
            }

            // s-p-d elements
            if (_lmax_gw >= 1 && _lmax_gamma >= 2) {
                S[1][2][5] = gmc0 * S[1][1][5] + fak * S[1][1][3];
                S[1][3][5] = gmc1 * S[1][1][5] + fak * S[1][1][2];
                S[1][4][5] = gmc2 * S[1][1][5];
                S[1][2][6] = gmc0 * S[1][1][6] + fak * S[1][1][4];
                S[1][3][6] = gmc1 * S[1][1][6];
                S[1][4][6] = gmc2 * S[1][1][6] + fak * S[1][1][2];
                S[1][2][7] = gmc0 * S[1][1][7];
                S[1][3][7] = gmc1 * S[1][1][7] + fak * S[1][1][4];
                S[1][4][7] = gmc2 * S[1][1][7] + fak * S[1][1][3];
                S[1][2][8] = gmc0 * S[1][1][8] + fak2 * S[1][1][2];
                S[1][3][8] = gmc1 * S[1][1][8];
                S[1][4][8] = gmc2 * S[1][1][8];
                S[1][2][9] = gmc0 * S[1][1][9];
                S[1][3][9] = gmc1 * S[1][1][9] + fak2 * S[1][1][3];
                S[1][4][9] = gmc2 * S[1][1][9];
                S[1][2][10] = gmc0 * S[1][1][10];
                S[1][3][10] = gmc1 * S[1][1][10];
                S[1][4][10] = gmc2 * S[1][1][10] + fak2 * S[1][1][4];
            }

            // s-d-d elements
            if (_lmax_gw >= 2 && _lmax_gamma >= 2) {
                S[1][8][5] = gmc0 * S[1][2][5] + fak * (S[1][1][5] + S[1][2][3]);
                S[1][5][5] = gmc1 * S[1][2][5] + fak * S[1][2][2];
                S[1][6][5] = gmc2 * S[1][2][5];
                S[1][8][6] = gmc0 * S[1][2][6] + fak * (S[1][1][6] + S[1][2][4]);
                S[1][5][6] = gmc1 * S[1][2][6];
                S[1][6][6] = gmc2 * S[1][2][6] + fak * S[1][2][2];
                S[1][8][7] = gmc0 * S[1][2][7] + fak * S[1][1][7];
                S[1][5][7] = gmc1 * S[1][2][7] + fak * S[1][2][4];
                S[1][6][7] = gmc2 * S[1][2][7] + fak * S[1][2][3];
                S[1][8][8] = gmc0 * S[1][2][8] + fak * (S[1][1][8] + S[1][2][2] + S[1][2][2]);
                S[1][5][8] = gmc1 * S[1][2][8];
                S[1][6][8] = gmc2 * S[1][2][8];
                S[1][8][9] = gmc0 * S[1][2][9] + fak * S[1][1][9];
                S[1][5][9] = gmc1 * S[1][2][9] + fak2 * S[1][2][3];
                S[1][6][9] = gmc2 * S[1][2][9];
                S[1][8][10] = gmc0 * S[1][2][10] + fak * S[1][1][10];
                S[1][5][10] = gmc1 * S[1][2][10];
                S[1][6][10] = gmc2 * S[1][2][10] + fak2 * S[1][2][4];
                S[1][9][5] = gmc1 * S[1][3][5] + fak * (S[1][1][5] + S[1][3][2]);
                S[1][7][5] = gmc2 * S[1][3][5];
                S[1][9][6] = gmc1 * S[1][3][6] + fak * S[1][1][6];
                S[1][7][6] = gmc2 * S[1][3][6] + fak * S[1][3][2];
                S[1][9][7] = gmc1 * S[1][3][7] + fak * (S[1][1][7] + S[1][3][4]);
                S[1][7][7] = gmc2 * S[1][3][7] + fak * S[1][3][3];
                S[1][9][8] = gmc1 * S[1][3][8] + fak * S[1][1][8];
                S[1][7][8] = gmc2 * S[1][3][8];
                S[1][9][9] = gmc1 * S[1][3][9] + fak * (S[1][1][9] + S[1][3][3] + S[1][3][3]);
                S[1][7][9] = gmc2 * S[1][3][9];
                S[1][9][10] = gmc1 * S[1][3][10] + fak * S[1][1][10];
                S[1][7][10] = gmc2 * S[1][3][10] + fak2 * S[1][3][4];
                S[1][10][5] = gmc2 * S[1][4][5] + fak * S[1][1][5];
                S[1][10][6] = gmc2 * S[1][4][6] + fak * (S[1][1][6] + S[1][4][2]);
                S[1][10][7] = gmc2 * S[1][4][7] + fak * (S[1][1][7] + S[1][4][3]);
                S[1][10][8] = gmc2 * S[1][4][8] + fak * S[1][1][8];
                S[1][10][9] = gmc2 * S[1][4][9] + fak * S[1][1][9];
                S[1][10][10] = gmc2 * S[1][4][10] + fak * (S[1][1][10] + S[1][4][4] + S[1][4][4]);
            }

            // d-s-s elements
            if (_lmax_alpha >= 2) {
                S[8][1][1] = gma0 * S[2][1][1] + fak * S[1][1][1];
                S[5][1][1] = gma1 * S[2][1][1];
                S[6][1][1] = gma2 * S[2][1][1];
                S[9][1][1] = gma1 * S[3][1][1] + fak * S[1][1][1];
                S[7][1][1] = gma2 * S[3][1][1];
                S[10][1][1] = gma2 * S[4][1][1] + fak * S[1][1][1];
            }

            // d-p-s elements
            if (_lmax_alpha >= 2 && _lmax_gw >= 1) {
                S[8][2][1] = gma0 * S[2][2][1] + fak * (S[1][2][1] + S[2][1][1]);
                S[5][2][1] = gma1 * S[2][2][1];
                S[6][2][1] = gma2 * S[2][2][1];
                S[8][3][1] = gma0 * S[2][3][1] + fak * S[1][3][1];
                S[5][3][1] = gma1 * S[2][3][1] + fak * S[2][1][1];
                S[6][3][1] = gma2 * S[2][3][1];
                S[8][4][1] = gma0 * S[2][4][1] + fak * S[1][4][1];
                S[5][4][1] = gma1 * S[2][4][1];
                S[6][4][1] = gma2 * S[2][4][1] + fak * S[2][1][1];
                S[9][2][1] = gma1 * S[3][2][1] + fak * S[1][2][1];
                S[7][2][1] = gma2 * S[3][2][1];
                S[9][3][1] = gma1 * S[3][3][1] + fak * (S[1][3][1] + S[3][1][1]);
                S[7][3][1] = gma2 * S[3][3][1];
                S[9][4][1] = gma1 * S[3][4][1] + fak * S[1][4][1];
                S[7][4][1] = gma2 * S[3][4][1] + fak * S[3][1][1];
                S[10][2][1] = gma2 * S[4][2][1] + fak * S[1][2][1];
                S[10][3][1] = gma2 * S[4][3][1] + fak * S[1][3][1];
                S[10][4][1] = gma2 * S[4][4][1] + fak * (S[1][4][1] + S[4][1][1]);
            }

            // d-d-s elements
            if (_lmax_alpha >= 2 && _lmax_gw >= 2) {
                S[8][5][1] = gma0 * S[2][5][1] + fak * (S[1][5][1] + S[2][3][1]);
                S[5][5][1] = gma1 * S[2][5][1] + fak * S[2][2][1];
                S[6][5][1] = gma2 * S[2][5][1];
                S[8][6][1] = gma0 * S[2][6][1] + fak * (S[1][6][1] + S[2][4][1]);
                S[5][6][1] = gma1 * S[2][6][1];
                S[6][6][1] = gma2 * S[2][6][1] + fak * S[2][2][1];
                S[8][7][1] = gma0 * S[2][7][1] + fak * S[1][7][1];
                S[5][7][1] = gma1 * S[2][7][1] + fak * S[2][4][1];
                S[6][7][1] = gma2 * S[2][7][1] + fak * S[2][3][1];
                S[8][8][1] = gma0 * S[2][8][1] + fak * (S[1][8][1] + S[2][2][1] + S[2][2][1]);
                S[5][8][1] = gma1 * S[2][8][1];
                S[6][8][1] = gma2 * S[2][8][1];
                S[8][9][1] = gma0 * S[2][9][1] + fak * S[1][9][1];
                S[5][9][1] = gma1 * S[2][9][1] + fak2 * S[2][3][1];
                S[6][9][1] = gma2 * S[2][9][1];
                S[8][10][1] = gma0 * S[2][10][1] + fak * S[1][10][1];
                S[5][10][1] = gma1 * S[2][10][1];
                S[6][10][1] = gma2 * S[2][10][1] + fak2 * S[2][4][1];
                S[9][5][1] = gma1 * S[3][5][1] + fak * (S[1][5][1] + S[3][2][1]);
                S[7][5][1] = gma2 * S[3][5][1];
                S[9][6][1] = gma1 * S[3][6][1] + fak * S[1][6][1];
                S[7][6][1] = gma2 * S[3][6][1] + fak * S[3][2][1];
                S[9][7][1] = gma1 * S[3][7][1] + fak * (S[1][7][1] + S[3][4][1]);
                S[7][7][1] = gma2 * S[3][7][1] + fak * S[3][3][1];
                S[9][8][1] = gma1 * S[3][8][1] + fak * S[1][8][1];
                S[7][8][1] = gma2 * S[3][8][1];
                S[9][9][1] = gma1 * S[3][9][1] + fak * (S[1][9][1] + S[3][3][1] + S[3][3][1]);
                S[7][9][1] = gma2 * S[3][9][1];
                S[9][10][1] = gma1 * S[3][10][1] + fak * S[1][10][1];
                S[7][10][1] = gma2 * S[3][10][1] + fak2 * S[3][4][1];
                S[10][5][1] = gma2 * S[4][5][1] + fak * S[1][5][1];
                S[10][6][1] = gma2 * S[4][6][1] + fak * (S[1][6][1] + S[4][2][1]);
                S[10][7][1] = gma2 * S[4][7][1] + fak * (S[1][7][1] + S[4][3][1]);
                S[10][8][1] = gma2 * S[4][8][1] + fak * S[1][8][1];
                S[10][9][1] = gma2 * S[4][9][1] + fak * S[1][9][1];
                S[10][10][1] = gma2 * S[4][10][1] + fak * (S[1][10][1] + S[4][4][1] + S[4][4][1]);
            }

            // p-s-d elements
            if (_lmax_alpha >= 1  && _lmax_gamma >= 2) {
                S[2][1][5] = gma0 * S[1][1][5] + fak * S[1][1][3];
                S[3][1][5] = gma1 * S[1][1][5] + fak * S[1][1][2];
                S[4][1][5] = gma2 * S[1][1][5];
                S[2][1][6] = gma0 * S[1][1][6] + fak * S[1][1][4];
                S[3][1][6] = gma1 * S[1][1][6];
                S[4][1][6] = gma2 * S[1][1][6] + fak * S[1][1][2];
                S[2][1][7] = gma0 * S[1][1][7];
                S[3][1][7] = gma1 * S[1][1][7] + fak * S[1][1][4];
                S[4][1][7] = gma2 * S[1][1][7] + fak * S[1][1][3];
                S[2][1][8] = gma0 * S[1][1][8] + fak2 * S[1][1][2];
                S[3][1][8] = gma1 * S[1][1][8];
                S[4][1][8] = gma2 * S[1][1][8];
                S[2][1][9] = gma0 * S[1][1][9];
                S[3][1][9] = gma1 * S[1][1][9] + fak2 * S[1][1][3];
                S[4][1][9] = gma2 * S[1][1][9];
                S[2][1][10] = gma0 * S[1][1][10];
                S[3][1][10] = gma1 * S[1][1][10];
                S[4][1][10] = gma2 * S[1][1][10] + fak2 * S[1][1][4];
            }

            // p-p-d elements
            if (_lmax_alpha >= 1 && _lmax_gw >= 1 && _lmax_gamma >= 2) {
                S[2][2][5] = gma0 * S[1][2][5] + fak * (S[1][1][5] + S[1][2][3]);
                S[3][2][5] = gma1 * S[1][2][5] + fak * S[1][2][2];
                S[4][2][5] = gma2 * S[1][2][5];
                S[2][2][6] = gma0 * S[1][2][6] + fak * (S[1][1][6] + S[1][2][4]);
                S[3][2][6] = gma1 * S[1][2][6];
                S[4][2][6] = gma2 * S[1][2][6] + fak * S[1][2][2];
                S[2][2][7] = gma0 * S[1][2][7] + fak * S[1][1][7];
                S[3][2][7] = gma1 * S[1][2][7] + fak * S[1][2][4];
                S[4][2][7] = gma2 * S[1][2][7] + fak * S[1][2][3];
                S[2][2][8] = gma0 * S[1][2][8] + fak * (S[1][1][8] + S[1][2][2] + S[1][2][2]);
                S[3][2][8] = gma1 * S[1][2][8];
                S[4][2][8] = gma2 * S[1][2][8];
                S[2][2][9] = gma0 * S[1][2][9] + fak * S[1][1][9];
                S[3][2][9] = gma1 * S[1][2][9] + fak2 * S[1][2][3];
                S[4][2][9] = gma2 * S[1][2][9];
                S[2][2][10] = gma0 * S[1][2][10] + fak * S[1][1][10];
                S[3][2][10] = gma1 * S[1][2][10];
                S[4][2][10] = gma2 * S[1][2][10] + fak2 * S[1][2][4];
                S[2][3][5] = gma0 * S[1][3][5] + fak * S[1][3][3];
                S[3][3][5] = gma1 * S[1][3][5] + fak * (S[1][1][5] + S[1][3][2]);
                S[4][3][5] = gma2 * S[1][3][5];
                S[2][3][6] = gma0 * S[1][3][6] + fak * S[1][3][4];
                S[3][3][6] = gma1 * S[1][3][6] + fak * S[1][1][6];
                S[4][3][6] = gma2 * S[1][3][6] + fak * S[1][3][2];
                S[2][3][7] = gma0 * S[1][3][7];
                S[3][3][7] = gma1 * S[1][3][7] + fak * (S[1][1][7] + S[1][3][4]);
                S[4][3][7] = gma2 * S[1][3][7] + fak * S[1][3][3];
                S[2][3][8] = gma0 * S[1][3][8] + fak2 * S[1][3][2];
                S[3][3][8] = gma1 * S[1][3][8] + fak * S[1][1][8];
                S[4][3][8] = gma2 * S[1][3][8];
                S[2][3][9] = gma0 * S[1][3][9];
                S[3][3][9] = gma1 * S[1][3][9] + fak * (S[1][1][9] + S[1][3][3] + S[1][3][3]);
                S[4][3][9] = gma2 * S[1][3][9];
                S[2][3][10] = gma0 * S[1][3][10];
                S[3][3][10] = gma1 * S[1][3][10] + fak * S[1][1][10];
                S[4][3][10] = gma2 * S[1][3][10] + fak2 * S[1][3][4];
                S[2][4][5] = gma0 * S[1][4][5] + fak * S[1][4][3];
                S[3][4][5] = gma1 * S[1][4][5] + fak * S[1][4][2];
                S[4][4][5] = gma2 * S[1][4][5] + fak * S[1][1][5];
                S[2][4][6] = gma0 * S[1][4][6] + fak * S[1][4][4];
                S[3][4][6] = gma1 * S[1][4][6];
                S[4][4][6] = gma2 * S[1][4][6] + fak * (S[1][1][6] + S[1][4][2]);
                S[2][4][7] = gma0 * S[1][4][7];
                S[3][4][7] = gma1 * S[1][4][7] + fak * S[1][4][4];
                S[4][4][7] = gma2 * S[1][4][7] + fak * (S[1][1][7] + S[1][4][3]);
                S[2][4][8] = gma0 * S[1][4][8] + fak2 * S[1][4][2];
                S[3][4][8] = gma1 * S[1][4][8];
                S[4][4][8] = gma2 * S[1][4][8] + fak * S[1][1][8];
                S[2][4][9] = gma0 * S[1][4][9];
                S[3][4][9] = gma1 * S[1][4][9] + fak2 * S[1][4][3];
                S[4][4][9] = gma2 * S[1][4][9] + fak * S[1][1][9];
                S[2][4][10] = gma0 * S[1][4][10];
                S[3][4][10] = gma1 * S[1][4][10];
                S[4][4][10] = gma2 * S[1][4][10] + fak * (S[1][1][10] + S[1][4][4] + S[1][4][4]);
            }

            // p-d-d elements
            if (_lmax_alpha >= 1 && _lmax_gw >= 2 && _lmax_gamma >= 2) {
                S[2][5][5] = gma0 * S[1][5][5] + fak * (S[1][3][5] + S[1][5][3]);
                S[3][5][5] = gma1 * S[1][5][5] + fak * (S[1][2][5] + S[1][5][2]);
                S[4][5][5] = gma2 * S[1][5][5];
                S[2][5][6] = gma0 * S[1][5][6] + fak * (S[1][3][6] + S[1][5][4]);
                S[3][5][6] = gma1 * S[1][5][6] + fak * S[1][2][6];
                S[4][5][6] = gma2 * S[1][5][6] + fak * S[1][5][2];
                S[2][5][7] = gma0 * S[1][5][7] + fak * S[1][3][7];
                S[3][5][7] = gma1 * S[1][5][7] + fak * (S[1][2][7] + S[1][5][4]);
                S[4][5][7] = gma2 * S[1][5][7] + fak * S[1][5][3];
                S[2][5][8] = gma0 * S[1][5][8] + fak * (S[1][3][8] + S[1][5][2] + S[1][5][2]);
                S[3][5][8] = gma1 * S[1][5][8] + fak * S[1][2][8];
                S[4][5][8] = gma2 * S[1][5][8];
                S[2][5][9] = gma0 * S[1][5][9] + fak * S[1][3][9];
                S[3][5][9] = gma1 * S[1][5][9] + fak * (S[1][2][9] + S[1][5][3] + S[1][5][3]);
                S[4][5][9] = gma2 * S[1][5][9];
                S[2][5][10] = gma0 * S[1][5][10] + fak * S[1][3][10];
                S[3][5][10] = gma1 * S[1][5][10] + fak * S[1][2][10];
                S[4][5][10] = gma2 * S[1][5][10] + fak2 * S[1][5][4];
                S[2][6][5] = gma0 * S[1][6][5] + fak * (S[1][4][5] + S[1][6][3]);
                S[3][6][5] = gma1 * S[1][6][5] + fak * S[1][6][2];
                S[4][6][5] = gma2 * S[1][6][5] + fak * S[1][2][5];
                S[2][6][6] = gma0 * S[1][6][6] + fak * (S[1][4][6] + S[1][6][4]);
                S[3][6][6] = gma1 * S[1][6][6];
                S[4][6][6] = gma2 * S[1][6][6] + fak * (S[1][2][6] + S[1][6][2]);
                S[2][6][7] = gma0 * S[1][6][7] + fak * S[1][4][7];
                S[3][6][7] = gma1 * S[1][6][7] + fak * S[1][6][4];
                S[4][6][7] = gma2 * S[1][6][7] + fak * (S[1][2][7] + S[1][6][3]);
                S[2][6][8] = gma0 * S[1][6][8] + fak * (S[1][4][8] + S[1][6][2] + S[1][6][2]);
                S[3][6][8] = gma1 * S[1][6][8];
                S[4][6][8] = gma2 * S[1][6][8] + fak * S[1][2][8];
                S[2][6][9] = gma0 * S[1][6][9] + fak * S[1][4][9];
                S[3][6][9] = gma1 * S[1][6][9] + fak2 * S[1][6][3];
                S[4][6][9] = gma2 * S[1][6][9] + fak * S[1][2][9];
                S[2][6][10] = gma0 * S[1][6][10] + fak * S[1][4][10];
                S[3][6][10] = gma1 * S[1][6][10];
                S[4][6][10] = gma2 * S[1][6][10] + fak * (S[1][2][10] + S[1][6][4] + S[1][6][4]);
                S[2][7][5] = gma0 * S[1][7][5] + fak * S[1][7][3];
                S[3][7][5] = gma1 * S[1][7][5] + fak * (S[1][4][5] + S[1][7][2]);
                S[4][7][5] = gma2 * S[1][7][5] + fak * S[1][3][5];
                S[2][7][6] = gma0 * S[1][7][6] + fak * S[1][7][4];
                S[3][7][6] = gma1 * S[1][7][6] + fak * S[1][4][6];
                S[4][7][6] = gma2 * S[1][7][6] + fak * (S[1][3][6] + S[1][7][2]);
                S[2][7][7] = gma0 * S[1][7][7];
                S[3][7][7] = gma1 * S[1][7][7] + fak * (S[1][4][7] + S[1][7][4]);
                S[4][7][7] = gma2 * S[1][7][7] + fak * (S[1][3][7] + S[1][7][3]);
                S[2][7][8] = gma0 * S[1][7][8] + fak * (2 * S[1][7][2]);
                S[3][7][8] = gma1 * S[1][7][8] + fak * S[1][4][8];
                S[4][7][8] = gma2 * S[1][7][8] + fak * S[1][3][8];
                S[2][7][9] = gma0 * S[1][7][9];
                S[3][7][9] = gma1 * S[1][7][9] + fak * (S[1][4][9] + S[1][7][3] + S[1][7][3]);
                S[4][7][9] = gma2 * S[1][7][9] + fak * S[1][3][9];
                S[2][7][10] = gma0 * S[1][7][10];
                S[3][7][10] = gma1 * S[1][7][10] + fak * S[1][4][10];
                S[4][7][10] = gma2 * S[1][7][10] + fak * (S[1][3][10] + S[1][7][4] + S[1][7][4]);
                S[2][8][5] = gma0 * S[1][8][5] + fak * (S[1][2][5] + S[1][2][5] + S[1][8][3]);
                S[3][8][5] = gma1 * S[1][8][5] + fak * S[1][8][2];
                S[4][8][5] = gma2 * S[1][8][5];
                S[2][8][6] = gma0 * S[1][8][6] + fak * (S[1][2][6] + S[1][2][6] + S[1][8][4]);
                S[3][8][6] = gma1 * S[1][8][6];
                S[4][8][6] = gma2 * S[1][8][6] + fak * S[1][8][2];
                S[2][8][7] = gma0 * S[1][8][7] + fak2 * S[1][2][7];
                S[3][8][7] = gma1 * S[1][8][7] + fak * S[1][8][4];
                S[4][8][7] = gma2 * S[1][8][7] + fak * S[1][8][3];
                S[2][8][8] = gma0 * S[1][8][8] + fak2 * (S[1][2][8] + S[1][8][2]);
                S[3][8][8] = gma1 * S[1][8][8];
                S[4][8][8] = gma2 * S[1][8][8];
                S[2][8][9] = gma0 * S[1][8][9] + fak2 * S[1][2][9];
                S[3][8][9] = gma1 * S[1][8][9] + fak2 * S[1][8][3];
                S[4][8][9] = gma2 * S[1][8][9];
                S[2][8][10] = gma0 * S[1][8][10] + fak2 * S[1][2][10];
                S[3][8][10] = gma1 * S[1][8][10];
                S[4][8][10] = gma2 * S[1][8][10] + fak2 * S[1][8][4];
                S[2][9][5] = gma0 * S[1][9][5] + fak * S[1][9][3];
                S[3][9][5] = gma1 * S[1][9][5] + fak * (S[1][3][5] + S[1][3][5] + S[1][9][2]);
                S[4][9][5] = gma2 * S[1][9][5];
                S[2][9][6] = gma0 * S[1][9][6] + fak * S[1][9][4];
                S[3][9][6] = gma1 * S[1][9][6] + fak2 * S[1][3][6];
                S[4][9][6] = gma2 * S[1][9][6] + fak * S[1][9][2];
                S[2][9][7] = gma0 * S[1][9][7];
                S[3][9][7] = gma1 * S[1][9][7] + fak * (S[1][3][7] + S[1][3][7] + S[1][9][4]);
                S[4][9][7] = gma2 * S[1][9][7] + fak * S[1][9][3];
                S[2][9][8] = gma0 * S[1][9][8] + fak2 * S[1][9][2];
                S[3][9][8] = gma1 * S[1][9][8] + fak2 * S[1][3][8];
                S[4][9][8] = gma2 * S[1][9][8];
                S[2][9][9] = gma0 * S[1][9][9];
                S[3][9][9] = gma1 * S[1][9][9] + fak2 * (S[1][3][9] + S[1][9][3]);
                S[4][9][9] = gma2 * S[1][9][9];
                S[2][9][10] = gma0 * S[1][9][10];
                S[3][9][10] = gma1 * S[1][9][10] + fak2 * S[1][3][10];
                S[4][9][10] = gma2 * S[1][9][10] + fak2 * S[1][9][4];
                S[2][10][5] = gma0 * S[1][10][5] + fak * S[1][10][3];
                S[3][10][5] = gma1 * S[1][10][5] + fak * S[1][10][2];
                S[4][10][5] = gma2 * S[1][10][5] + fak2 * S[1][4][5];
                S[2][10][6] = gma0 * S[1][10][6] + fak * S[1][10][4];
                S[3][10][6] = gma1 * S[1][10][6];
                S[4][10][6] = gma2 * S[1][10][6] + fak * (S[1][4][6] + S[1][4][6] + S[1][10][2]);
                S[2][10][7] = gma0 * S[1][10][7];
                S[3][10][7] = gma1 * S[1][10][7] + fak * S[1][10][4];
                S[4][10][7] = gma2 * S[1][10][7] + fak * (S[1][4][7] + S[1][4][7] + S[1][10][3]);
                S[2][10][8] = gma0 * S[1][10][8] + fak2 * S[1][10][2];
                S[3][10][8] = gma1 * S[1][10][8];
                S[4][10][8] = gma2 * S[1][10][8] + fak2 * S[1][4][8];
                S[2][10][9] = gma0 * S[1][10][9];
                S[3][10][9] = gma1 * S[1][10][9] + fak2 * S[1][10][3];
                S[4][10][9] = gma2 * S[1][10][9] + fak2 * S[1][4][9];
                S[2][10][10] = gma0 * S[1][10][10];
                S[3][10][10] = gma1 * S[1][10][10];
                S[4][10][10] = gma2 * S[1][10][10] + fak2 * (S[1][4][10] + S[1][10][4]);
            }

            // d-s-p elements
            if (_lmax_alpha >= 2  && _lmax_gamma >= 1) {
                S[8][1][2] = gma0 * S[2][1][2] + fak * (S[1][1][2] + S[2][1][1]);
                S[5][1][2] = gma1 * S[2][1][2];
                S[6][1][2] = gma2 * S[2][1][2];
                S[8][1][3] = gma0 * S[2][1][3] + fak * S[1][1][3];
                S[5][1][3] = gma1 * S[2][1][3] + fak * S[2][1][1];
                S[6][1][3] = gma2 * S[2][1][3];
                S[8][1][4] = gma0 * S[2][1][4] + fak * S[1][1][4];
                S[5][1][4] = gma1 * S[2][1][4];
                S[6][1][4] = gma2 * S[2][1][4] + fak * S[2][1][1];
                S[9][1][2] = gma1 * S[3][1][2] + fak * S[1][1][2];
                S[7][1][2] = gma2 * S[3][1][2];
                S[9][1][3] = gma1 * S[3][1][3] + fak * (S[1][1][3] + S[3][1][1]);
                S[7][1][3] = gma2 * S[3][1][3];
                S[9][1][4] = gma1 * S[3][1][4] + fak * S[1][1][4];
                S[7][1][4] = gma2 * S[3][1][4] + fak * S[3][1][1];
                S[10][1][2] = gma2 * S[4][1][2] + fak * S[1][1][2];
                S[10][1][3] = gma2 * S[4][1][3] + fak * S[1][1][3];
                S[10][1][4] = gma2 * S[4][1][4] + fak * (S[1][1][4] + S[4][1][1]);
            }

            // d-p-d elements
            if (_lmax_alpha >= 2 && _lmax_gw >= 1 && _lmax_gamma >= 1) {
                S[8][2][2] = gma0 * S[2][2][2] + fak * (S[1][2][2] + S[2][1][2] + S[2][2][1]);
                S[5][2][2] = gma1 * S[2][2][2];
                S[6][2][2] = gma2 * S[2][2][2];
                S[8][2][3] = gma0 * S[2][2][3] + fak * (S[1][2][3] + S[2][1][3]);
                S[5][2][3] = gma1 * S[2][2][3] + fak * S[2][2][1];
                S[6][2][3] = gma2 * S[2][2][3];
                S[8][2][4] = gma0 * S[2][2][4] + fak * (S[1][2][4] + S[2][1][4]);
                S[5][2][4] = gma1 * S[2][2][4];
                S[6][2][4] = gma2 * S[2][2][4] + fak * S[2][2][1];
                S[8][3][2] = gma0 * S[2][3][2] + fak * (S[1][3][2] + S[2][3][1]);
                S[5][3][2] = gma1 * S[2][3][2] + fak * S[2][1][2];
                S[6][3][2] = gma2 * S[2][3][2];
                S[8][3][3] = gma0 * S[2][3][3] + fak * S[1][3][3];
                S[5][3][3] = gma1 * S[2][3][3] + fak * (S[2][1][3] + S[2][3][1]);
                S[6][3][3] = gma2 * S[2][3][3];
                S[8][3][4] = gma0 * S[2][3][4] + fak * S[1][3][4];
                S[5][3][4] = gma1 * S[2][3][4] + fak * S[2][1][4];
                S[6][3][4] = gma2 * S[2][3][4] + fak * S[2][3][1];
                S[8][4][2] = gma0 * S[2][4][2] + fak * (S[1][4][2] + S[2][4][1]);
                S[5][4][2] = gma1 * S[2][4][2];
                S[6][4][2] = gma2 * S[2][4][2] + fak * S[2][1][2];
                S[8][4][3] = gma0 * S[2][4][3] + fak * S[1][4][3];
                S[5][4][3] = gma1 * S[2][4][3] + fak * S[2][4][1];
                S[6][4][3] = gma2 * S[2][4][3] + fak * S[2][1][3];
                S[8][4][4] = gma0 * S[2][4][4] + fak * S[1][4][4];
                S[5][4][4] = gma1 * S[2][4][4];
                S[6][4][4] = gma2 * S[2][4][4] + fak * (S[2][1][4] + S[2][4][1]);
                S[9][2][2] = gma1 * S[3][2][2] + fak * S[1][2][2];
                S[7][2][2] = gma2 * S[3][2][2];
                S[9][2][3] = gma1 * S[3][2][3] + fak * (S[1][2][3] + S[3][2][1]);
                S[7][2][3] = gma2 * S[3][2][3];
                S[9][2][4] = gma1 * S[3][2][4] + fak * S[1][2][4];
                S[7][2][4] = gma2 * S[3][2][4] + fak * S[3][2][1];
                S[9][3][2] = gma1 * S[3][3][2] + fak * (S[1][3][2] + S[3][1][2]);
                S[7][3][2] = gma2 * S[3][3][2];
                S[9][3][3] = gma1 * S[3][3][3] + fak * (S[1][3][3] + S[3][1][3] + S[3][3][1]);
                S[7][3][3] = gma2 * S[3][3][3];
                S[9][3][4] = gma1 * S[3][3][4] + fak * (S[1][3][4] + S[3][1][4]);
                S[7][3][4] = gma2 * S[3][3][4] + fak * S[3][3][1];
                S[9][4][2] = gma1 * S[3][4][2] + fak * S[1][4][2];
                S[7][4][2] = gma2 * S[3][4][2] + fak * S[3][1][2];
                S[9][4][3] = gma1 * S[3][4][3] + fak * (S[1][4][3] + S[3][4][1]);
                S[7][4][3] = gma2 * S[3][4][3] + fak * S[3][1][3];
                S[9][4][4] = gma1 * S[3][4][4] + fak * S[1][4][4];
                S[7][4][4] = gma2 * S[3][4][4] + fak * (S[3][1][4] + S[3][4][1]);
                S[10][2][2] = gma2 * S[4][2][2] + fak * S[1][2][2];
                S[10][2][3] = gma2 * S[4][2][3] + fak * S[1][2][3];
                S[10][2][4] = gma2 * S[4][2][4] + fak * (S[1][2][4] + S[4][2][1]);
                S[10][3][2] = gma2 * S[4][3][2] + fak * S[1][3][2];
                S[10][3][3] = gma2 * S[4][3][3] + fak * S[1][3][3];
                S[10][3][4] = gma2 * S[4][3][4] + fak * (S[1][3][4] + S[4][3][1]);
                S[10][4][2] = gma2 * S[4][4][2] + fak * (S[1][4][2] + S[4][1][2]);
                S[10][4][3] = gma2 * S[4][4][3] + fak * (S[1][4][3] + S[4][1][3]);
                S[10][4][4] = gma2 * S[4][4][4] + fak * (S[1][4][4] + S[4][1][4] + S[4][4][1]);
            }

            // d-d-p elements
            if (_lmax_alpha >= 2 && _lmax_gw >= 2 && _lmax_gamma >= 1) {
                S[8][5][2] = gma0 * S[2][5][2] + fak * (S[1][5][2] + S[2][3][2] + S[2][5][1]);
                S[5][5][2] = gma1 * S[2][5][2] + fak * S[2][2][2];
                S[6][5][2] = gma2 * S[2][5][2];
                S[8][5][3] = gma0 * S[2][5][3] + fak * (S[1][5][3] + S[2][3][3]);
                S[5][5][3] = gma1 * S[2][5][3] + fak * (S[2][2][3] + S[2][5][1]);
                S[6][5][3] = gma2 * S[2][5][3];
                S[8][5][4] = gma0 * S[2][5][4] + fak * (S[1][5][4] + S[2][3][4]);
                S[5][5][4] = gma1 * S[2][5][4] + fak * S[2][2][4];
                S[6][5][4] = gma2 * S[2][5][4] + fak * S[2][5][1];
                S[8][6][2] = gma0 * S[2][6][2] + fak * (S[1][6][2] + S[2][4][2] + S[2][6][1]);
                S[5][6][2] = gma1 * S[2][6][2];
                S[6][6][2] = gma2 * S[2][6][2] + fak * S[2][2][2];
                S[8][6][3] = gma0 * S[2][6][3] + fak * (S[1][6][3] + S[2][4][3]);
                S[5][6][3] = gma1 * S[2][6][3] + fak * S[2][6][1];
                S[6][6][3] = gma2 * S[2][6][3] + fak * S[2][2][3];
                S[8][6][4] = gma0 * S[2][6][4] + fak * (S[1][6][4] + S[2][4][4]);
                S[5][6][4] = gma1 * S[2][6][4];
                S[6][6][4] = gma2 * S[2][6][4] + fak * (S[2][2][4] + S[2][6][1]);
                S[8][7][2] = gma0 * S[2][7][2] + fak * (S[1][7][2] + S[2][7][1]);
                S[5][7][2] = gma1 * S[2][7][2] + fak * S[2][4][2];
                S[6][7][2] = gma2 * S[2][7][2] + fak * S[2][3][2];
                S[8][7][3] = gma0 * S[2][7][3] + fak * S[1][7][3];
                S[5][7][3] = gma1 * S[2][7][3] + fak * (S[2][4][3] + S[2][7][1]);
                S[6][7][3] = gma2 * S[2][7][3] + fak * S[2][3][3];
                S[8][7][4] = gma0 * S[2][7][4] + fak * S[1][7][4];
                S[5][7][4] = gma1 * S[2][7][4] + fak * S[2][4][4];
                S[6][7][4] = gma2 * S[2][7][4] + fak * (S[2][3][4] + S[2][7][1]);
                S[8][8][2] = gma0 * S[2][8][2] + fak * (S[1][8][2] + S[2][2][2] + S[2][2][2] + S[2][8][1]);
                S[5][8][2] = gma1 * S[2][8][2];
                S[6][8][2] = gma2 * S[2][8][2];
                S[8][8][3] = gma0 * S[2][8][3] + fak * (S[1][8][3] + S[2][2][3] + S[2][2][3]);
                S[5][8][3] = gma1 * S[2][8][3] + fak * S[2][8][1];
                S[6][8][3] = gma2 * S[2][8][3];
                S[8][8][4] = gma0 * S[2][8][4] + fak * (S[1][8][4] + S[2][2][4] + S[2][2][4]);
                S[5][8][4] = gma1 * S[2][8][4];
                S[6][8][4] = gma2 * S[2][8][4] + fak * S[2][8][1];
                S[8][9][2] = gma0 * S[2][9][2] + fak * (S[1][9][2] + S[2][9][1]);
                S[5][9][2] = gma1 * S[2][9][2] + fak2 * S[2][3][2];
                S[6][9][2] = gma2 * S[2][9][2];
                S[8][9][3] = gma0 * S[2][9][3] + fak * S[1][9][3];
                S[5][9][3] = gma1 * S[2][9][3] + fak * (S[2][3][3] + S[2][3][3] + S[2][9][1]);
                S[6][9][3] = gma2 * S[2][9][3];
                S[8][9][4] = gma0 * S[2][9][4] + fak * S[1][9][4];
                S[5][9][4] = gma1 * S[2][9][4] + fak * (2 * S[2][3][4]);
                S[6][9][4] = gma2 * S[2][9][4] + fak * S[2][9][1];
                S[8][10][2] = gma0 * S[2][10][2] + fak * (S[1][10][2] + S[2][10][1]);
                S[5][10][2] = gma1 * S[2][10][2];
                S[6][10][2] = gma2 * S[2][10][2] + fak * (2 * S[2][4][2]);
                S[8][10][3] = gma0 * S[2][10][3] + fak * S[1][10][3];
                S[5][10][3] = gma1 * S[2][10][3] + fak * S[2][10][1];
                S[6][10][3] = gma2 * S[2][10][3] + fak * (2 * S[2][4][3]);
                S[8][10][4] = gma0 * S[2][10][4] + fak * S[1][10][4];
                S[5][10][4] = gma1 * S[2][10][4];
                S[6][10][4] = gma2 * S[2][10][4] + fak * (S[2][4][4] + S[2][4][4] + S[2][10][1]);
                S[9][5][2] = gma1 * S[3][5][2] + fak * (S[1][5][2] + S[3][2][2]);
                S[7][5][2] = gma2 * S[3][5][2];
                S[9][5][3] = gma1 * S[3][5][3] + fak * (S[1][5][3] + S[3][2][3] + S[3][5][1]);
                S[7][5][3] = gma2 * S[3][5][3];
                S[9][5][4] = gma1 * S[3][5][4] + fak * (S[1][5][4] + S[3][2][4]);
                S[7][5][4] = gma2 * S[3][5][4] + fak * S[3][5][1];
                S[9][6][2] = gma1 * S[3][6][2] + fak * S[1][6][2];
                S[7][6][2] = gma2 * S[3][6][2] + fak * S[3][2][2];
                S[9][6][3] = gma1 * S[3][6][3] + fak * (S[1][6][3] + S[3][6][1]);
                S[7][6][3] = gma2 * S[3][6][3] + fak * S[3][2][3];
                S[9][6][4] = gma1 * S[3][6][4] + fak * S[1][6][4];
                S[7][6][4] = gma2 * S[3][6][4] + fak * (S[3][2][4] + S[3][6][1]);
                S[9][7][2] = gma1 * S[3][7][2] + fak * (S[1][7][2] + S[3][4][2]);
                S[7][7][2] = gma2 * S[3][7][2] + fak * S[3][3][2];
                S[9][7][3] = gma1 * S[3][7][3] + fak * (S[1][7][3] + S[3][4][3] + S[3][7][1]);
                S[7][7][3] = gma2 * S[3][7][3] + fak * S[3][3][3];
                S[9][7][4] = gma1 * S[3][7][4] + fak * (S[1][7][4] + S[3][4][4]);
                S[7][7][4] = gma2 * S[3][7][4] + fak * (S[3][3][4] + S[3][7][1]);
                S[9][8][2] = gma1 * S[3][8][2] + fak * S[1][8][2];
                S[7][8][2] = gma2 * S[3][8][2];
                S[9][8][3] = gma1 * S[3][8][3] + fak * (S[1][8][3] + S[3][8][1]);
                S[7][8][3] = gma2 * S[3][8][3];
                S[9][8][4] = gma1 * S[3][8][4] + fak * S[1][8][4];
                S[7][8][4] = gma2 * S[3][8][4] + fak * S[3][8][1];
                S[9][9][2] = gma1 * S[3][9][2] + fak * (S[1][9][2] + S[3][3][2] + S[3][3][2]);
                S[7][9][2] = gma2 * S[3][9][2];
                S[9][9][3] = gma1 * S[3][9][3] + fak * (S[1][9][3] + S[3][3][3] + S[3][3][3] + S[3][9][1]);
                S[7][9][3] = gma2 * S[3][9][3];
                S[9][9][4] = gma1 * S[3][9][4] + fak * (S[1][9][4] + S[3][3][4] + S[3][3][4]);
                S[7][9][4] = gma2 * S[3][9][4] + fak * S[3][9][1];
                S[9][10][2] = gma1 * S[3][10][2] + fak * S[1][10][2];
                S[7][10][2] = gma2 * S[3][10][2] + fak2 * S[3][4][2];
                S[9][10][3] = gma1 * S[3][10][3] + fak * (S[1][10][3] + S[3][10][1]);
                S[7][10][3] = gma2 * S[3][10][3] + fak2 * S[3][4][3];
                S[9][10][4] = gma1 * S[3][10][4] + fak * S[1][10][4];
                S[7][10][4] = gma2 * S[3][10][4] + fak * (S[3][4][4] + S[3][4][4] + S[3][10][1]);
                S[10][5][2] = gma2 * S[4][5][2] + fak * S[1][5][2];
                S[10][5][3] = gma2 * S[4][5][3] + fak * S[1][5][3];
                S[10][5][4] = gma2 * S[4][5][4] + fak * (S[1][5][4] + S[4][5][1]);
                S[10][6][2] = gma2 * S[4][6][2] + fak * (S[1][6][2] + S[4][2][2]);
                S[10][6][3] = gma2 * S[4][6][3] + fak * (S[1][6][3] + S[4][2][3]);
                S[10][6][4] = gma2 * S[4][6][4] + fak * (S[1][6][4] + S[4][2][4] + S[4][6][1]);
                S[10][7][2] = gma2 * S[4][7][2] + fak * (S[1][7][2] + S[4][3][2]);
                S[10][7][3] = gma2 * S[4][7][3] + fak * (S[1][7][3] + S[4][3][3]);
                S[10][7][4] = gma2 * S[4][7][4] + fak * (S[1][7][4] + S[4][3][4] + S[4][7][1]);
                S[10][8][2] = gma2 * S[4][8][2] + fak * S[1][8][2];
                S[10][8][3] = gma2 * S[4][8][3] + fak * S[1][8][3];
                S[10][8][4] = gma2 * S[4][8][4] + fak * (S[1][8][4] + S[4][8][1]);
                S[10][9][2] = gma2 * S[4][9][2] + fak * S[1][9][2];
                S[10][9][3] = gma2 * S[4][9][3] + fak * S[1][9][3];
                S[10][9][4] = gma2 * S[4][9][4] + fak * (S[1][9][4] + S[4][9][1]);
                S[10][10][2] = gma2 * S[4][10][2] + fak * (S[1][10][2] + S[4][4][2] + S[4][4][2]);
                S[10][10][3] = gma2 * S[4][10][3] + fak * (S[1][10][3] + S[4][4][3] + S[4][4][3]);
                S[10][10][4] = gma2 * S[4][10][4] + fak * (S[1][10][4] + S[4][4][4] + S[4][4][4] + S[4][10][1]);
            }

            // d-s-d elements
            if (_lmax_alpha >= 2  && _lmax_gamma >= 2) {
                S[8][1][5] = gma0 * S[2][1][5] + fak * (S[1][1][5] + S[2][1][3]);
                S[5][1][5] = gma1 * S[2][1][5] + fak * S[2][1][2];
                S[6][1][5] = gma2 * S[2][1][5];
                S[8][1][6] = gma0 * S[2][1][6] + fak * (S[1][1][6] + S[2][1][4]);
                S[5][1][6] = gma1 * S[2][1][6];
                S[6][1][6] = gma2 * S[2][1][6] + fak * S[2][1][2];
                S[8][1][7] = gma0 * S[2][1][7] + fak * S[1][1][7];
                S[5][1][7] = gma1 * S[2][1][7] + fak * S[2][1][4];
                S[6][1][7] = gma2 * S[2][1][7] + fak * S[2][1][3];
                S[8][1][8] = gma0 * S[2][1][8] + fak * (S[1][1][8] + S[2][1][2] + S[2][1][2]);
                S[5][1][8] = gma1 * S[2][1][8];
                S[6][1][8] = gma2 * S[2][1][8];
                S[8][1][9] = gma0 * S[2][1][9] + fak * S[1][1][9];
                S[5][1][9] = gma1 * S[2][1][9] + fak2 * S[2][1][3];
                S[6][1][9] = gma2 * S[2][1][9];
                S[8][1][10] = gma0 * S[2][1][10] + fak * S[1][1][10];
                S[5][1][10] = gma1 * S[2][1][10];
                S[6][1][10] = gma2 * S[2][1][10] + fak2 * S[2][1][4];
                S[9][1][5] = gma1 * S[3][1][5] + fak * (S[1][1][5] + S[3][1][2]);
                S[7][1][5] = gma2 * S[3][1][5];
                S[9][1][6] = gma1 * S[3][1][6] + fak * S[1][1][6];
                S[7][1][6] = gma2 * S[3][1][6] + fak * S[3][1][2];
                S[9][1][7] = gma1 * S[3][1][7] + fak * (S[1][1][7] + S[3][1][4]);
                S[7][1][7] = gma2 * S[3][1][7] + fak * S[3][1][3];
                S[9][1][8] = gma1 * S[3][1][8] + fak * S[1][1][8];
                S[7][1][8] = gma2 * S[3][1][8];
                S[9][1][9] = gma1 * S[3][1][9] + fak * (S[1][1][9] + S[3][1][3] + S[3][1][3]);
                S[7][1][9] = gma2 * S[3][1][9];
                S[9][1][10] = gma1 * S[3][1][10] + fak * S[1][1][10];
                S[7][1][10] = gma2 * S[3][1][10] + fak2 * S[3][1][4];
                S[10][1][5] = gma2 * S[4][1][5] + fak * S[1][1][5];
                S[10][1][6] = gma2 * S[4][1][6] + fak * (S[1][1][6] + S[4][1][2]);
                S[10][1][7] = gma2 * S[4][1][7] + fak * (S[1][1][7] + S[4][1][3]);
                S[10][1][8] = gma2 * S[4][1][8] + fak * S[1][1][8];
                S[10][1][9] = gma2 * S[4][1][9] + fak * S[1][1][9];
                S[10][1][10] = gma2 * S[4][1][10] + fak * (S[1][1][10] + S[4][1][4] + S[4][1][4]);
            }

            // d-p-d elements
            if (_lmax_alpha >= 2 && _lmax_gw >= 1 && _lmax_gamma >= 2) {
                S[8][2][5] = gma0 * S[2][2][5] + fak * (S[1][2][5] + S[2][1][5] + S[2][2][3]);
                S[5][2][5] = gma1 * S[2][2][5] + fak * S[2][2][2];
                S[6][2][5] = gma2 * S[2][2][5];
                S[8][2][6] = gma0 * S[2][2][6] + fak * (S[1][2][6] + S[2][1][6] + S[2][2][4]);
                S[5][2][6] = gma1 * S[2][2][6];
                S[6][2][6] = gma2 * S[2][2][6] + fak * S[2][2][2];
                S[8][2][7] = gma0 * S[2][2][7] + fak * (S[1][2][7] + S[2][1][7]);
                S[5][2][7] = gma1 * S[2][2][7] + fak * S[2][2][4];
                S[6][2][7] = gma2 * S[2][2][7] + fak * S[2][2][3];
                S[8][2][8] = gma0 * S[2][2][8] + fak * (S[1][2][8] + S[2][1][8] + S[2][2][2] + S[2][2][2]);
                S[5][2][8] = gma1 * S[2][2][8];
                S[6][2][8] = gma2 * S[2][2][8];
                S[8][2][9] = gma0 * S[2][2][9] + fak * (S[1][2][9] + S[2][1][9]);
                S[5][2][9] = gma1 * S[2][2][9] + fak2 * S[2][2][3];
                S[6][2][9] = gma2 * S[2][2][9];
                S[8][2][10] = gma0 * S[2][2][10] + fak * (S[1][2][10] + S[2][1][10]);
                S[5][2][10] = gma1 * S[2][2][10];
                S[6][2][10] = gma2 * S[2][2][10] + fak2 * S[2][2][4];
                S[8][3][5] = gma0 * S[2][3][5] + fak * (S[1][3][5] + S[2][3][3]);
                S[5][3][5] = gma1 * S[2][3][5] + fak * (S[2][1][5] + S[2][3][2]);
                S[6][3][5] = gma2 * S[2][3][5];
                S[8][3][6] = gma0 * S[2][3][6] + fak * (S[1][3][6] + S[2][3][4]);
                S[5][3][6] = gma1 * S[2][3][6] + fak * S[2][1][6];
                S[6][3][6] = gma2 * S[2][3][6] + fak * S[2][3][2];
                S[8][3][7] = gma0 * S[2][3][7] + fak * S[1][3][7];
                S[5][3][7] = gma1 * S[2][3][7] + fak * (S[2][1][7] + S[2][3][4]);
                S[6][3][7] = gma2 * S[2][3][7] + fak * S[2][3][3];
                S[8][3][8] = gma0 * S[2][3][8] + fak * (S[1][3][8] + S[2][3][2] + S[2][3][2]);
                S[5][3][8] = gma1 * S[2][3][8] + fak * S[2][1][8];
                S[6][3][8] = gma2 * S[2][3][8];
                S[8][3][9] = gma0 * S[2][3][9] + fak * S[1][3][9];
                S[5][3][9] = gma1 * S[2][3][9] + fak * (S[2][1][9] + S[2][3][3] + S[2][3][3]);
                S[6][3][9] = gma2 * S[2][3][9];
                S[8][3][10] = gma0 * S[2][3][10] + fak * S[1][3][10];
                S[5][3][10] = gma1 * S[2][3][10] + fak * S[2][1][10];
                S[6][3][10] = gma2 * S[2][3][10] + fak2 * S[2][3][4];
                S[8][4][5] = gma0 * S[2][4][5] + fak * (S[1][4][5] + S[2][4][3]);
                S[5][4][5] = gma1 * S[2][4][5] + fak * S[2][4][2];
                S[6][4][5] = gma2 * S[2][4][5] + fak * S[2][1][5];
                S[8][4][6] = gma0 * S[2][4][6] + fak * (S[1][4][6] + S[2][4][4]);
                S[5][4][6] = gma1 * S[2][4][6];
                S[6][4][6] = gma2 * S[2][4][6] + fak * (S[2][1][6] + S[2][4][2]);
                S[8][4][7] = gma0 * S[2][4][7] + fak * S[1][4][7];
                S[5][4][7] = gma1 * S[2][4][7] + fak * S[2][4][4];
                S[6][4][7] = gma2 * S[2][4][7] + fak * (S[2][1][7] + S[2][4][3]);
                S[8][4][8] = gma0 * S[2][4][8] + fak * (S[1][4][8] + S[2][4][2] + S[2][4][2]);
                S[5][4][8] = gma1 * S[2][4][8];
                S[6][4][8] = gma2 * S[2][4][8] + fak * S[2][1][8];
                S[8][4][9] = gma0 * S[2][4][9] + fak * S[1][4][9];
                S[5][4][9] = gma1 * S[2][4][9] + fak2 * S[2][4][3];
                S[6][4][9] = gma2 * S[2][4][9] + fak * S[2][1][9];
                S[8][4][10] = gma0 * S[2][4][10] + fak * S[1][4][10];
                S[5][4][10] = gma1 * S[2][4][10];
                S[6][4][10] = gma2 * S[2][4][10] + fak * (S[2][1][10] + S[2][4][4] + S[2][4][4]);
                S[9][2][5] = gma1 * S[3][2][5] + fak * (S[1][2][5] + S[3][2][2]);
                S[7][2][5] = gma2 * S[3][2][5];
                S[9][2][6] = gma1 * S[3][2][6] + fak * S[1][2][6];
                S[7][2][6] = gma2 * S[3][2][6] + fak * S[3][2][2];
                S[9][2][7] = gma1 * S[3][2][7] + fak * (S[1][2][7] + S[3][2][4]);
                S[7][2][7] = gma2 * S[3][2][7] + fak * S[3][2][3];
                S[9][2][8] = gma1 * S[3][2][8] + fak * S[1][2][8];
                S[7][2][8] = gma2 * S[3][2][8];
                S[9][2][9] = gma1 * S[3][2][9] + fak * (S[1][2][9] + S[3][2][3] + S[3][2][3]);
                S[7][2][9] = gma2 * S[3][2][9];
                S[9][2][10] = gma1 * S[3][2][10] + fak * S[1][2][10];
                S[7][2][10] = gma2 * S[3][2][10] + fak2 * S[3][2][4];
                S[9][3][5] = gma1 * S[3][3][5] + fak * (S[1][3][5] + S[3][1][5] + S[3][3][2]);
                S[7][3][5] = gma2 * S[3][3][5];
                S[9][3][6] = gma1 * S[3][3][6] + fak * (S[1][3][6] + S[3][1][6]);
                S[7][3][6] = gma2 * S[3][3][6] + fak * S[3][3][2];
                S[9][3][7] = gma1 * S[3][3][7] + fak * (S[1][3][7] + S[3][1][7] + S[3][3][4]);
                S[7][3][7] = gma2 * S[3][3][7] + fak * S[3][3][3];
                S[9][3][8] = gma1 * S[3][3][8] + fak * (S[1][3][8] + S[3][1][8]);
                S[7][3][8] = gma2 * S[3][3][8];
                S[9][3][9] = gma1 * S[3][3][9] + fak * (S[1][3][9] + S[3][1][9] + S[3][3][3] + S[3][3][3]);
                S[7][3][9] = gma2 * S[3][3][9];
                S[9][3][10] = gma1 * S[3][3][10] + fak * (S[1][3][10] + S[3][1][10]);
                S[7][3][10] = gma2 * S[3][3][10] + fak2 * S[3][3][4];
                S[9][4][5] = gma1 * S[3][4][5] + fak * (S[1][4][5] + S[3][4][2]);
                S[7][4][5] = gma2 * S[3][4][5] + fak * S[3][1][5];
                S[9][4][6] = gma1 * S[3][4][6] + fak * S[1][4][6];
                S[7][4][6] = gma2 * S[3][4][6] + fak * (S[3][1][6] + S[3][4][2]);
                S[9][4][7] = gma1 * S[3][4][7] + fak * (S[1][4][7] + S[3][4][4]);
                S[7][4][7] = gma2 * S[3][4][7] + fak * (S[3][1][7] + S[3][4][3]);
                S[9][4][8] = gma1 * S[3][4][8] + fak * S[1][4][8];
                S[7][4][8] = gma2 * S[3][4][8] + fak * S[3][1][8];
                S[9][4][9] = gma1 * S[3][4][9] + fak * (S[1][4][9] + S[3][4][3] + S[3][4][3]);
                S[7][4][9] = gma2 * S[3][4][9] + fak * S[3][1][9];
                S[9][4][10] = gma1 * S[3][4][10] + fak * S[1][4][10];
                S[7][4][10] = gma2 * S[3][4][10] + fak * (S[3][1][10] + S[3][4][4] + S[3][4][4]);
                S[10][2][5] = gma2 * S[4][2][5] + fak * S[1][2][5];
                S[10][2][6] = gma2 * S[4][2][6] + fak * (S[1][2][6] + S[4][2][2]);
                S[10][2][7] = gma2 * S[4][2][7] + fak * (S[1][2][7] + S[4][2][3]);
                S[10][2][8] = gma2 * S[4][2][8] + fak * S[1][2][8];
                S[10][2][9] = gma2 * S[4][2][9] + fak * S[1][2][9];
                S[10][2][10] = gma2 * S[4][2][10] + fak * (S[1][2][10] + S[4][2][4] + S[4][2][4]);
                S[10][3][5] = gma2 * S[4][3][5] + fak * S[1][3][5];
                S[10][3][6] = gma2 * S[4][3][6] + fak * (S[1][3][6] + S[4][3][2]);
                S[10][3][7] = gma2 * S[4][3][7] + fak * (S[1][3][7] + S[4][3][3]);
                S[10][3][8] = gma2 * S[4][3][8] + fak * S[1][3][8];
                S[10][3][9] = gma2 * S[4][3][9] + fak * S[1][3][9];
                S[10][3][10] = gma2 * S[4][3][10] + fak * (S[1][3][10] + S[4][3][4] + S[4][3][4]);
                S[10][4][5] = gma2 * S[4][4][5] + fak * (S[1][4][5] + S[4][1][5]);
                S[10][4][6] = gma2 * S[4][4][6] + fak * (S[1][4][6] + S[4][1][6] + S[4][4][2]);
                S[10][4][7] = gma2 * S[4][4][7] + fak * (S[1][4][7] + S[4][1][7] + S[4][4][3]);
                S[10][4][8] = gma2 * S[4][4][8] + fak * (S[1][4][8] + S[4][1][8]);
                S[10][4][9] = gma2 * S[4][4][9] + fak * (S[1][4][9] + S[4][1][9]);
                S[10][4][10] = gma2 * S[4][4][10] + fak * (S[1][4][10] + S[4][1][10] + S[4][4][4] + S[4][4][4]);
            }

            // d-d-d elements
            if (_lmax_alpha >= 2 && _lmax_gw >= 2 && _lmax_gamma >= 2) {
                S[8][5][5] = gma0 * S[2][5][5] + fak * (S[1][5][5] + S[2][3][5] + S[2][5][3]);
                S[5][5][5] = gma1 * S[2][5][5] + fak * (S[2][2][5] + S[2][5][2]);
                S[6][5][5] = gma2 * S[2][5][5];
                S[8][5][6] = gma0 * S[2][5][6] + fak * (S[1][5][6] + S[2][3][6] + S[2][5][4]);
                S[5][5][6] = gma1 * S[2][5][6] + fak * S[2][2][6];
                S[6][5][6] = gma2 * S[2][5][6] + fak * S[2][5][2];
                S[8][5][7] = gma0 * S[2][5][7] + fak * (S[1][5][7] + S[2][3][7]);
                S[5][5][7] = gma1 * S[2][5][7] + fak * (S[2][2][7] + S[2][5][4]);
                S[6][5][7] = gma2 * S[2][5][7] + fak * S[2][5][3];
                S[8][5][8] = gma0 * S[2][5][8] + fak * (S[1][5][8] + S[2][3][8] + S[2][5][2] + S[2][5][2]);
                S[5][5][8] = gma1 * S[2][5][8] + fak * S[2][2][8];
                S[6][5][8] = gma2 * S[2][5][8];
                S[8][5][9] = gma0 * S[2][5][9] + fak * (S[1][5][9] + S[2][3][9]);
                S[5][5][9] = gma1 * S[2][5][9] + fak * (S[2][2][9] + S[2][5][3] + S[2][5][3]);
                S[6][5][9] = gma2 * S[2][5][9];
                S[8][5][10] = gma0 * S[2][5][10] + fak * (S[1][5][10] + S[2][3][10]);
                S[5][5][10] = gma1 * S[2][5][10] + fak * S[2][2][10];
                S[6][5][10] = gma2 * S[2][5][10] + fak2 * S[2][5][4];
                S[8][6][5] = gma0 * S[2][6][5] + fak * (S[1][6][5] + S[2][4][5] + S[2][6][3]);
                S[5][6][5] = gma1 * S[2][6][5] + fak * S[2][6][2];
                S[6][6][5] = gma2 * S[2][6][5] + fak * S[2][2][5];
                S[8][6][6] = gma0 * S[2][6][6] + fak * (S[1][6][6] + S[2][4][6] + S[2][6][4]);
                S[5][6][6] = gma1 * S[2][6][6];
                S[6][6][6] = gma2 * S[2][6][6] + fak * (S[2][2][6] + S[2][6][2]);
                S[8][6][7] = gma0 * S[2][6][7] + fak * (S[1][6][7] + S[2][4][7]);
                S[5][6][7] = gma1 * S[2][6][7] + fak * S[2][6][4];
                S[6][6][7] = gma2 * S[2][6][7] + fak * (S[2][2][7] + S[2][6][3]);
                S[8][6][8] = gma0 * S[2][6][8] + fak * (S[1][6][8] + S[2][4][8] + S[2][6][2] + S[2][6][2]);
                S[5][6][8] = gma1 * S[2][6][8];
                S[6][6][8] = gma2 * S[2][6][8] + fak * S[2][2][8];
                S[8][6][9] = gma0 * S[2][6][9] + fak * (S[1][6][9] + S[2][4][9]);
                S[5][6][9] = gma1 * S[2][6][9] + fak2 * S[2][6][3];
                S[6][6][9] = gma2 * S[2][6][9] + fak * S[2][2][9];
                S[8][6][10] = gma0 * S[2][6][10] + fak * (S[1][6][10] + S[2][4][10]);
                S[5][6][10] = gma1 * S[2][6][10];
                S[6][6][10] = gma2 * S[2][6][10] + fak * (S[2][2][10] + S[2][6][4] + S[2][6][4]);
                S[8][7][5] = gma0 * S[2][7][5] + fak * (S[1][7][5] + S[2][7][3]);
                S[5][7][5] = gma1 * S[2][7][5] + fak * (S[2][4][5] + S[2][7][2]);
                S[6][7][5] = gma2 * S[2][7][5] + fak * S[2][3][5];
                S[8][7][6] = gma0 * S[2][7][6] + fak * (S[1][7][6] + S[2][7][4]);
                S[5][7][6] = gma1 * S[2][7][6] + fak * S[2][4][6];
                S[6][7][6] = gma2 * S[2][7][6] + fak * (S[2][3][6] + S[2][7][2]);
                S[8][7][7] = gma0 * S[2][7][7] + fak * S[1][7][7];
                S[5][7][7] = gma1 * S[2][7][7] + fak * (S[2][4][7] + S[2][7][4]);
                S[6][7][7] = gma2 * S[2][7][7] + fak * (S[2][3][7] + S[2][7][3]);
                S[8][7][8] = gma0 * S[2][7][8] + fak * (S[1][7][8] + S[2][7][2] + S[2][7][2]);
                S[5][7][8] = gma1 * S[2][7][8] + fak * S[2][4][8];
                S[6][7][8] = gma2 * S[2][7][8] + fak * S[2][3][8];
                S[8][7][9] = gma0 * S[2][7][9] + fak * S[1][7][9];
                S[5][7][9] = gma1 * S[2][7][9] + fak * (S[2][4][9] + S[2][7][3] + S[2][7][3]);
                S[6][7][9] = gma2 * S[2][7][9] + fak * S[2][3][9];
                S[8][7][10] = gma0 * S[2][7][10] + fak * S[1][7][10];
                S[5][7][10] = gma1 * S[2][7][10] + fak * S[2][4][10];
                S[6][7][10] = gma2 * S[2][7][10] + fak * (S[2][3][10] + S[2][7][4] + S[2][7][4]);
                S[8][8][5] = gma0 * S[2][8][5] + fak * (S[1][8][5] + S[2][2][5] + S[2][2][5] + S[2][8][3]);
                S[5][8][5] = gma1 * S[2][8][5] + fak * S[2][8][2];
                S[6][8][5] = gma2 * S[2][8][5];
                S[8][8][6] = gma0 * S[2][8][6] + fak * (S[1][8][6] + S[2][2][6] + S[2][2][6] + S[2][8][4]);
                S[5][8][6] = gma1 * S[2][8][6];
                S[6][8][6] = gma2 * S[2][8][6] + fak * S[2][8][2];
                S[8][8][7] = gma0 * S[2][8][7] + fak * (S[1][8][7] + S[2][2][7] + S[2][2][7]);
                S[5][8][7] = gma1 * S[2][8][7] + fak * S[2][8][4];
                S[6][8][7] = gma2 * S[2][8][7] + fak * S[2][8][3];
                S[8][8][8] = gma0 * S[2][8][8] + fak * (S[1][8][8] + S[2][2][8] + S[2][2][8] + S[2][8][2] + S[2][8][2]);
                S[5][8][8] = gma1 * S[2][8][8];
                S[6][8][8] = gma2 * S[2][8][8];
                S[8][8][9] = gma0 * S[2][8][9] + fak * (S[1][8][9] + S[2][2][9] + S[2][2][9]);
                S[5][8][9] = gma1 * S[2][8][9] + fak2 * S[2][8][3];
                S[6][8][9] = gma2 * S[2][8][9];
                S[8][8][10] = gma0 * S[2][8][10] + fak * (S[1][8][10] + S[2][2][10] + S[2][2][10]);
                S[5][8][10] = gma1 * S[2][8][10];
                S[6][8][10] = gma2 * S[2][8][10] + fak2 * S[2][8][4];
                S[8][9][5] = gma0 * S[2][9][5] + fak * (S[1][9][5] + S[2][9][3]);
                S[5][9][5] = gma1 * S[2][9][5] + fak * (S[2][3][5] + S[2][3][5] + S[2][9][2]);
                S[6][9][5] = gma2 * S[2][9][5];
                S[8][9][6] = gma0 * S[2][9][6] + fak * (S[1][9][6] + S[2][9][4]);
                S[5][9][6] = gma1 * S[2][9][6] + fak2 * S[2][3][6];
                S[6][9][6] = gma2 * S[2][9][6] + fak * S[2][9][2];
                S[8][9][7] = gma0 * S[2][9][7] + fak * S[1][9][7];
                S[5][9][7] = gma1 * S[2][9][7] + fak * (S[2][3][7] + S[2][3][7] + S[2][9][4]);
                S[6][9][7] = gma2 * S[2][9][7] + fak * S[2][9][3];
                S[8][9][8] = gma0 * S[2][9][8] + fak * (S[1][9][8] + S[2][9][2] + S[2][9][2]);
                S[5][9][8] = gma1 * S[2][9][8] + fak2 * S[2][3][8];
                S[6][9][8] = gma2 * S[2][9][8];
                S[8][9][9] = gma0 * S[2][9][9] + fak * S[1][9][9];
                S[5][9][9] = gma1 * S[2][9][9] + fak2 * (S[2][3][9] + S[2][9][3]);
                S[6][9][9] = gma2 * S[2][9][9];
                S[8][9][10] = gma0 * S[2][9][10] + fak * S[1][9][10];
                S[5][9][10] = gma1 * S[2][9][10] + fak2 * S[2][3][10];
                S[6][9][10] = gma2 * S[2][9][10] + fak2 * S[2][9][4];
                S[8][10][5] = gma0 * S[2][10][5] + fak * (S[1][10][5] + S[2][10][3]);
                S[5][10][5] = gma1 * S[2][10][5] + fak * S[2][10][2];
                S[6][10][5] = gma2 * S[2][10][5] + fak2 * S[2][4][5];
                S[8][10][6] = gma0 * S[2][10][6] + fak * (S[1][10][6] + S[2][10][4]);
                S[5][10][6] = gma1 * S[2][10][6];
                S[6][10][6] = gma2 * S[2][10][6] + fak * (S[2][4][6] + S[2][4][6] + S[2][10][2]);
                S[8][10][7] = gma0 * S[2][10][7] + fak * S[1][10][7];
                S[5][10][7] = gma1 * S[2][10][7] + fak * S[2][10][4];
                S[6][10][7] = gma2 * S[2][10][7] + fak * (S[2][4][7] + S[2][4][7] + S[2][10][3]);
                S[8][10][8] = gma0 * S[2][10][8] + fak * (S[1][10][8] + S[2][10][2] + S[2][10][2]);
                S[5][10][8] = gma1 * S[2][10][8];
                S[6][10][8] = gma2 * S[2][10][8] + fak2 * S[2][4][8];
                S[8][10][9] = gma0 * S[2][10][9] + fak * S[1][10][9];
                S[5][10][9] = gma1 * S[2][10][9] + fak2 * S[2][10][3];
                S[6][10][9] = gma2 * S[2][10][9] + fak2 * S[2][4][9];
                S[8][10][10] = gma0 * S[2][10][10] + fak * S[1][10][10];
                S[5][10][10] = gma1 * S[2][10][10];
                S[6][10][10] = gma2 * S[2][10][10] + fak2 * (S[2][4][10] + S[2][10][4]);
                S[9][5][5] = gma1 * S[3][5][5] + fak * (S[1][5][5] + S[3][2][5] + S[3][5][2]);
                S[7][5][5] = gma2 * S[3][5][5];
                S[9][5][6] = gma1 * S[3][5][6] + fak * (S[1][5][6] + S[3][2][6]);
                S[7][5][6] = gma2 * S[3][5][6] + fak * S[3][5][2];
                S[9][5][7] = gma1 * S[3][5][7] + fak * (S[1][5][7] + S[3][2][7] + S[3][5][4]);
                S[7][5][7] = gma2 * S[3][5][7] + fak * S[3][5][3];
                S[9][5][8] = gma1 * S[3][5][8] + fak * (S[1][5][8] + S[3][2][8]);
                S[7][5][8] = gma2 * S[3][5][8];
                S[9][5][9] = gma1 * S[3][5][9] + fak * (S[1][5][9] + S[3][2][9] + S[3][5][3] + S[3][5][3]);
                S[7][5][9] = gma2 * S[3][5][9];
                S[9][5][10] = gma1 * S[3][5][10] + fak * (S[1][5][10] + S[3][2][10]);
                S[7][5][10] = gma2 * S[3][5][10] + fak2 * S[3][5][4];
                S[9][6][5] = gma1 * S[3][6][5] + fak * (S[1][6][5] + S[3][6][2]);
                S[7][6][5] = gma2 * S[3][6][5] + fak * S[3][2][5];
                S[9][6][6] = gma1 * S[3][6][6] + fak * S[1][6][6];
                S[7][6][6] = gma2 * S[3][6][6] + fak * (S[3][2][6] + S[3][6][2]);
                S[9][6][7] = gma1 * S[3][6][7] + fak * (S[1][6][7] + S[3][6][4]);
                S[7][6][7] = gma2 * S[3][6][7] + fak * (S[3][2][7] + S[3][6][3]);
                S[9][6][8] = gma1 * S[3][6][8] + fak * S[1][6][8];
                S[7][6][8] = gma2 * S[3][6][8] + fak * S[3][2][8];
                S[9][6][9] = gma1 * S[3][6][9] + fak * (S[1][6][9] + S[3][6][3] + S[3][6][3]);
                S[7][6][9] = gma2 * S[3][6][9] + fak * S[3][2][9];
                S[9][6][10] = gma1 * S[3][6][10] + fak * S[1][6][10];
                S[7][6][10] = gma2 * S[3][6][10] + fak * (S[3][2][10] + S[3][6][4] + S[3][6][4]);
                S[9][7][5] = gma1 * S[3][7][5] + fak * (S[1][7][5] + S[3][4][5] + S[3][7][2]);
                S[7][7][5] = gma2 * S[3][7][5] + fak * S[3][3][5];
                S[9][7][6] = gma1 * S[3][7][6] + fak * (S[1][7][6] + S[3][4][6]);
                S[7][7][6] = gma2 * S[3][7][6] + fak * (S[3][3][6] + S[3][7][2]);
                S[9][7][7] = gma1 * S[3][7][7] + fak * (S[1][7][7] + S[3][4][7] + S[3][7][4]);
                S[7][7][7] = gma2 * S[3][7][7] + fak * (S[3][3][7] + S[3][7][3]);
                S[9][7][8] = gma1 * S[3][7][8] + fak * (S[1][7][8] + S[3][4][8]);
                S[7][7][8] = gma2 * S[3][7][8] + fak * S[3][3][8];
                S[9][7][9] = gma1 * S[3][7][9] + fak * (S[1][7][9] + S[3][4][9] + S[3][7][3] + S[3][7][3]);
                S[7][7][9] = gma2 * S[3][7][9] + fak * S[3][3][9];
                S[9][7][10] = gma1 * S[3][7][10] + fak * (S[1][7][10] + S[3][4][10]);
                S[7][7][10] = gma2 * S[3][7][10] + fak * (S[3][3][10] + S[3][7][4] + S[3][7][4]);
                S[9][8][5] = gma1 * S[3][8][5] + fak * (S[1][8][5] + S[3][8][2]);
                S[7][8][5] = gma2 * S[3][8][5];
                S[9][8][6] = gma1 * S[3][8][6] + fak * S[1][8][6];
                S[7][8][6] = gma2 * S[3][8][6] + fak * S[3][8][2];
                S[9][8][7] = gma1 * S[3][8][7] + fak * (S[1][8][7] + S[3][8][4]);
                S[7][8][7] = gma2 * S[3][8][7] + fak * S[3][8][3];
                S[9][8][8] = gma1 * S[3][8][8] + fak * S[1][8][8];
                S[7][8][8] = gma2 * S[3][8][8];
                S[9][8][9] = gma1 * S[3][8][9] + fak * (S[1][8][9] + S[3][8][3] + S[3][8][3]);
                S[7][8][9] = gma2 * S[3][8][9];
                S[9][8][10] = gma1 * S[3][8][10] + fak * S[1][8][10];
                S[7][8][10] = gma2 * S[3][8][10] + fak2 * S[3][8][4];
                S[9][9][5] = gma1 * S[3][9][5] + fak * (S[1][9][5] + S[3][3][5] + S[3][3][5] + S[3][9][2]);
                S[7][9][5] = gma2 * S[3][9][5];
                S[9][9][6] = gma1 * S[3][9][6] + fak * (S[1][9][6] + S[3][3][6] + S[3][3][6]);
                S[7][9][6] = gma2 * S[3][9][6] + fak * S[3][9][2];
                S[9][9][7] = gma1 * S[3][9][7] + fak * (S[1][9][7] + S[3][3][7] + S[3][3][7] + S[3][9][4]);
                S[7][9][7] = gma2 * S[3][9][7] + fak * S[3][9][3];
                S[9][9][8] = gma1 * S[3][9][8] + fak * (S[1][9][8] + S[3][3][8] + S[3][3][8]);
                S[7][9][8] = gma2 * S[3][9][8];
                S[9][9][9] = gma1 * S[3][9][9] + fak * (S[1][9][9] + S[3][3][9] + S[3][3][9] + S[3][9][3] + S[3][9][3]);
                S[7][9][9] = gma2 * S[3][9][9];
                S[9][9][10] = gma1 * S[3][9][10] + fak * (S[1][9][10] + S[3][3][10] + S[3][3][10]);
                S[7][9][10] = gma2 * S[3][9][10] + fak2 * S[3][9][4];
                S[9][10][5] = gma1 * S[3][10][5] + fak * (S[1][10][5] + S[3][10][2]);
                S[7][10][5] = gma2 * S[3][10][5] + fak2 * S[3][4][5];
                S[9][10][6] = gma1 * S[3][10][6] + fak * S[1][10][6];
                S[7][10][6] = gma2 * S[3][10][6] + fak * (S[3][4][6] + S[3][4][6] + S[3][10][2]);
                S[9][10][7] = gma1 * S[3][10][7] + fak * (S[1][10][7] + S[3][10][4]);
                S[7][10][7] = gma2 * S[3][10][7] + fak * (S[3][4][7] + S[3][4][7] + S[3][10][3]);
                S[9][10][8] = gma1 * S[3][10][8] + fak * S[1][10][8];
                S[7][10][8] = gma2 * S[3][10][8] + fak2 * S[3][4][8];
                S[9][10][9] = gma1 * S[3][10][9] + fak * (S[1][10][9] + S[3][10][3] + S[3][10][3]);
                S[7][10][9] = gma2 * S[3][10][9] + fak2 * S[3][4][9];
                S[9][10][10] = gma1 * S[3][10][10] + fak * S[1][10][10];
                S[7][10][10] = gma2 * S[3][10][10] + fak2 * (S[3][4][10] + S[3][10][4]);
                S[10][5][5] = gma2 * S[4][5][5] + fak * S[1][5][5];
                S[10][5][6] = gma2 * S[4][5][6] + fak * (S[1][5][6] + S[4][5][2]);
                S[10][5][7] = gma2 * S[4][5][7] + fak * (S[1][5][7] + S[4][5][3]);
                S[10][5][8] = gma2 * S[4][5][8] + fak * S[1][5][8];
                S[10][5][9] = gma2 * S[4][5][9] + fak * S[1][5][9];
                S[10][5][10] = gma2 * S[4][5][10] + fak * (S[1][5][10] + S[4][5][4] + S[4][5][4]);
                S[10][6][5] = gma2 * S[4][6][5] + fak * (S[1][6][5] + S[4][2][5]);
                S[10][6][6] = gma2 * S[4][6][6] + fak * (S[1][6][6] + S[4][2][6] + S[4][6][2]);
                S[10][6][7] = gma2 * S[4][6][7] + fak * (S[1][6][7] + S[4][2][7] + S[4][6][3]);
                S[10][6][8] = gma2 * S[4][6][8] + fak * (S[1][6][8] + S[4][2][8]);
                S[10][6][9] = gma2 * S[4][6][9] + fak * (S[1][6][9] + S[4][2][9]);
                S[10][6][10] = gma2 * S[4][6][10] + fak * (S[1][6][10] + S[4][2][10] + S[4][6][4] + S[4][6][4]);
                S[10][7][5] = gma2 * S[4][7][5] + fak * (S[1][7][5] + S[4][3][5]);
                S[10][7][6] = gma2 * S[4][7][6] + fak * (S[1][7][6] + S[4][3][6] + S[4][7][2]);
                S[10][7][7] = gma2 * S[4][7][7] + fak * (S[1][7][7] + S[4][3][7] + S[4][7][3]);
                S[10][7][8] = gma2 * S[4][7][8] + fak * (S[1][7][8] + S[4][3][8]);
                S[10][7][9] = gma2 * S[4][7][9] + fak * (S[1][7][9] + S[4][3][9]);
                S[10][7][10] = gma2 * S[4][7][10] + fak * (S[1][7][10] + S[4][3][10] + S[4][7][4] + S[4][7][4]);
                S[10][8][5] = gma2 * S[4][8][5] + fak * (1 * S[1][8][5]);
                S[10][8][6] = gma2 * S[4][8][6] + fak * (S[1][8][6] + S[4][8][2]);
                S[10][8][7] = gma2 * S[4][8][7] + fak * (S[1][8][7] + S[4][8][3]);
                S[10][8][8] = gma2 * S[4][8][8] + fak * S[1][8][8];
                S[10][8][9] = gma2 * S[4][8][9] + fak * S[1][8][9];
                S[10][8][10] = gma2 * S[4][8][10] + fak * (S[1][8][10] + S[4][8][4] + S[4][8][4]);
                S[10][9][5] = gma2 * S[4][9][5] + fak * S[1][9][5];
                S[10][9][6] = gma2 * S[4][9][6] + fak * (S[1][9][6] + S[4][9][2]);
                S[10][9][7] = gma2 * S[4][9][7] + fak * (S[1][9][7] + S[4][9][3]);
                S[10][9][8] = gma2 * S[4][9][8] + fak * S[1][9][8];
                S[10][9][9] = gma2 * S[4][9][9] + fak * S[1][9][9];
                S[10][9][10] = gma2 * S[4][9][10] + fak * (S[1][9][10] + S[4][9][4] + S[4][9][4]);
                S[10][10][5] = gma2 * S[4][10][5] + fak * (S[1][10][5] + S[4][4][5] + S[4][4][5]);
                S[10][10][6] = gma2 * S[4][10][6] + fak * (S[1][10][6] + S[4][4][6] + S[4][4][6] + S[4][10][2]);
                S[10][10][7] = gma2 * S[4][10][7] + fak * (S[1][10][7] + S[4][4][7] + S[4][4][7] + S[4][10][3]);
                S[10][10][8] = gma2 * S[4][10][8] + fak * (S[1][10][8] + S[4][4][8] + S[4][4][8]);
                S[10][10][9] = gma2 * S[4][10][9] + fak * (S[1][10][9] + S[4][4][9] + S[4][4][9]);
                S[10][10][10] = gma2 * S[4][10][10] + fak * (S[1][10][10] + S[4][4][10] + S[4][4][10] + S[4][10][4] + S[4][10][4]);
            }
            
            // s-f-s elements
             if (  _lmax_gw >= 3) {
                S[1][14][1] = gmc0*S[1][5][1] + fak * S[1][3][1];
                S[1][15][1] = gmc1*S[1][5][1] + fak * S[1][2][1];
                S[1][20][1] = gmc2*S[1][5][1];
                S[1][16][1] = gmc0*S[1][6][1] + fak * S[1][4][1];
                S[1][17][1] = gmc2*S[1][6][1] + fak * S[1][2][1];
                S[1][18][1] = gmc1*S[1][7][1] + fak * S[1][4][1];
                S[1][19][1] = gmc2*S[1][7][1] + fak * S[1][3][1];
                S[1][11][1] = gmc0*S[1][8][1] + fak2* S[1][2][1];
                S[1][12][1] = gmc1*S[1][9][1] + fak2* S[1][3][1];
                S[1][13][1] = gmc2*S[1][10][1] + fak2* S[1][4][1];
             }

            // s-f-p elements 
             if (_lmax_gw >= 3 && _lmax_gamma >= 1) {
                S[1][14][2] = gmc0*S[1][5][2] + fak * (S[1][3][2] +S[1][5][1] );
                S[1][15][2] = gmc1*S[1][5][2] + fak * S[1][2][2];
                S[1][20][2] = gmc2*S[1][5][2];
                S[1][14][3] = gmc0*S[1][5][3] + fak * S[1][3][3];
                S[1][15][3] = gmc1*S[1][5][3] + fak * (S[1][2][3] +S[1][5][1] );
                S[1][20][3] = gmc2*S[1][5][3];
                S[1][14][4] = gmc0*S[1][5][4] + fak * S[1][3][4];
                S[1][15][4] = gmc1*S[1][5][4] + fak * S[1][2][4];
                S[1][20][4] = gmc2*S[1][5][4] + fak * S[1][5][1];
                S[1][16][2] = gmc0*S[1][6][2] + fak * (S[1][4][2] +S[1][6][1] );
                S[1][17][2] = gmc2*S[1][6][2] + fak * S[1][2][2];
                S[1][16][3] = gmc0*S[1][6][3] + fak * S[1][4][3];
                S[1][17][3] = gmc2*S[1][6][3] + fak * S[1][2][3];
                S[1][16][4] = gmc0*S[1][6][4] + fak * S[1][4][4];
                S[1][17][4] = gmc2*S[1][6][4] + fak * (S[1][2][4] +S[1][6][1] );
                S[1][18][2] = gmc1*S[1][7][2] + fak * S[1][4][2] ;
                S[1][19][2] = gmc2*S[1][7][2] + fak * S[1][3][2];
                S[1][18][3] = gmc1*S[1][7][3] + fak * (S[1][4][3] +S[1][7][1] );
                S[1][19][3] = gmc2*S[1][7][3] + fak * S[1][3][3];
                S[1][18][4] = gmc1*S[1][7][4] + fak * S[1][4][4];
                S[1][19][4] = gmc2*S[1][7][4] + fak * (S[1][3][4] +S[1][7][1] );
                S[1][11][2] = gmc0*S[1][8][2] + fak * (2.0*S[1][2][2] +S[1][8][1] );
                S[1][11][3] = gmc0*S[1][8][3] + fak2* S[1][2][3];
                S[1][11][4] = gmc0*S[1][8][4] + fak2* S[1][2][4];
                S[1][12][2] = gmc1*S[1][9][2] + fak2* S[1][3][2];
                S[1][12][3] = gmc1*S[1][9][3] + fak * (2.0*S[1][3][3] +S[1][9][1] );
                S[1][12][4] = gmc1*S[1][9][4] + fak2* S[1][3][4];
                S[1][13][2] = gmc2*S[1][10][2] + fak2* S[1][4][2];
                S[1][13][3] = gmc2*S[1][10][3] + fak2* S[1][4][3];
                S[1][13][4] = gmc2*S[1][10][4] + fak * (2.0*S[1][4][4] +S[1][10][1] );
             }

            // p-f-s elements
             if ( _lmax_alpha >= 1 && _lmax_gw >= 3) {
                S[2][11][1] = gma0*S[1][11][1] + fak3* S[1][8][1];
                S[3][11][1] = gma1*S[1][11][1];
                S[4][11][1] = gma2*S[1][11][1];
                S[2][12][1] = gma0*S[1][12][1];
                S[3][12][1] = gma1*S[1][12][1] + fak3* S[1][9][1];
                S[4][12][1] = gma2*S[1][12][1];
                S[2][13][1] = gma0*S[1][13][1];
                S[3][13][1] = gma1*S[1][13][1];
                S[4][13][1] = gma2*S[1][13][1] + fak3* S[1][10][1];
                S[2][14][1] = gma0*S[1][14][1] + fak2* S[1][5][1];
                S[3][14][1] = gma1*S[1][14][1] + fak * S[1][8][1];
                S[4][14][1] = gma2*S[1][14][1];
                S[2][15][1] = gma0*S[1][15][1] + fak * S[1][9][1];
                S[3][15][1] = gma1*S[1][15][1] + fak2* S[1][5][1];
                S[4][15][1] = gma2*S[1][15][1];
                S[2][16][1] = gma0*S[1][16][1] + fak2* S[1][6][1];
                S[3][16][1] = gma1*S[1][16][1];
                S[4][16][1] = gma2*S[1][16][1] + fak * S[1][8][1];
                S[2][17][1] = gma0*S[1][17][1] + fak * S[1][10][1];
                S[3][17][1] = gma1*S[1][17][1];
                S[4][17][1] = gma2*S[1][17][1] + fak2* S[1][6][1];
                S[2][18][1] = gma0*S[1][18][1];
                S[3][18][1] = gma1*S[1][18][1] + fak2* S[1][7][1];
                S[4][18][1] = gma2*S[1][18][1] + fak * S[1][9][1];
                S[2][19][1] = gma0*S[1][19][1];
                S[3][19][1] = gma1*S[1][19][1] + fak * S[1][10][1];
                S[4][19][1] = gma2*S[1][19][1] + fak2* S[1][7][1];
                S[2][20][1] = gma0*S[1][20][1] + fak * S[1][7][1];
                S[3][20][1] = gma1*S[1][20][1] + fak * S[1][6][1];
                S[4][20][1] = gma2*S[1][20][1] + fak * S[1][5][1];
             }

            // s-f-d elements
             if ( _lmax_gw >= 3 && _lmax_gamma >= 2) {
                S[1][14][5] = gmc0*S[1][5][5] + fak * (S[1][3][5] +S[1][5][3] );
                S[1][15][5] = gmc1*S[1][5][5] + fak * (S[1][2][5] +S[1][5][2] );
                S[1][20][5] = gmc2*S[1][5][5];
                S[1][14][6] = gmc0*S[1][5][6] + fak * (S[1][3][6] +S[1][5][4] );
                S[1][15][6] = gmc1*S[1][5][6] + fak * S[1][2][6];
                S[1][20][6] = gmc2*S[1][5][6] + fak * S[1][5][2];
                S[1][14][7] = gmc0*S[1][5][7] + fak * S[1][3][7];
                S[1][15][7] = gmc1*S[1][5][7] + fak * (S[1][2][7] +S[1][5][4] );
                S[1][20][7] = gmc2*S[1][5][7] + fak * S[1][5][3];
                S[1][14][8] = gmc0*S[1][5][8] + fak * (S[1][3][8] +2.0*S[1][5][2] );
                S[1][15][8] = gmc1*S[1][5][8] + fak * S[1][2][8];
                S[1][20][8] = gmc2*S[1][5][8];
                S[1][14][9] = gmc0*S[1][5][9] + fak * S[1][3][9];
                S[1][15][9] = gmc1*S[1][5][9] + fak * (S[1][2][9] +2.0*S[1][5][3] );
                S[1][20][9] = gmc2*S[1][5][9];
                S[1][14][10] = gmc0*S[1][5][10] + fak * S[1][3][10];
                S[1][15][10] = gmc1*S[1][5][10] + fak * S[1][2][10];
                S[1][20][10] = gmc2*S[1][5][10] + fak2* S[1][5][4];
                S[1][16][5] = gmc0*S[1][6][5] + fak * (S[1][4][5] +S[1][6][3] );
                S[1][17][5] = gmc2*S[1][6][5] + fak * S[1][2][5];
                S[1][16][6] = gmc0*S[1][6][6] + fak * (S[1][4][6] +S[1][6][4] );
                S[1][17][6] = gmc2*S[1][6][6] + fak * (S[1][2][6] +S[1][6][2] );
                S[1][16][7] = gmc0*S[1][6][7] + fak * S[1][4][7];
                S[1][17][7] = gmc2*S[1][6][7] + fak * (S[1][2][7] +S[1][6][3] );
                S[1][16][8] = gmc0*S[1][6][8] + fak * (S[1][4][8] +2.0*S[1][6][2] );
                S[1][17][8] = gmc2*S[1][6][8] + fak * S[1][2][8];
                S[1][16][9] = gmc0*S[1][6][9] + fak * S[1][4][9];
                S[1][17][9] = gmc2*S[1][6][9] + fak * S[1][2][9];
                S[1][16][10] = gmc0*S[1][6][10] + fak * S[1][4][10];
                S[1][17][10] = gmc2*S[1][6][10] + fak * (S[1][2][10] +2.0*S[1][6][4] );
                S[1][18][5] = gmc1*S[1][7][5] + fak * (S[1][4][5] +S[1][7][2] );
                S[1][19][5] = gmc2*S[1][7][5] + fak * S[1][3][5];
                S[1][18][6] = gmc1*S[1][7][6] + fak * S[1][4][6];
                S[1][19][6] = gmc2*S[1][7][6] + fak * (S[1][3][6] +S[1][7][2] );
                S[1][18][7] = gmc1*S[1][7][7] + fak * (S[1][4][7] +S[1][7][4] );
                S[1][19][7] = gmc2*S[1][7][7] + fak * (S[1][3][7] +S[1][7][3] );
                S[1][18][8] = gmc1*S[1][7][8] + fak * S[1][4][8];
                S[1][19][8] = gmc2*S[1][7][8] + fak * S[1][3][8];
                S[1][18][9] = gmc1*S[1][7][9] + fak * (S[1][4][9] +2.0*S[1][7][3] );
                S[1][19][9] = gmc2*S[1][7][9] + fak * S[1][3][9];
                S[1][18][10] = gmc1*S[1][7][10] + fak * S[1][4][10];
                S[1][19][10] = gmc2*S[1][7][10] + fak * (S[1][3][10] +2.0*S[1][7][4] );
                S[1][11][5] = gmc0*S[1][8][5] + fak * (2.0*S[1][2][5] +S[1][8][3] );
                S[1][11][6] = gmc0*S[1][8][6] + fak * (2.0*S[1][2][6] +S[1][8][4] );
                S[1][11][7] = gmc0*S[1][8][7] + fak2* S[1][2][7];
                S[1][11][8] = gmc0*S[1][8][8] + fak2* (S[1][2][8] +S[1][8][2] );
                S[1][11][9] = gmc0*S[1][8][9] + fak2* S[1][2][9];
                S[1][11][10] = gmc0*S[1][8][10] + fak2* S[1][2][10];
                S[1][12][5] = gmc1*S[1][9][5] + fak * (2.0*S[1][3][5] +S[1][9][2] );
                S[1][12][6] = gmc1*S[1][9][6] + fak2* S[1][3][6];
                S[1][12][7] = gmc1*S[1][9][7] + fak * (2.0*S[1][3][7] +S[1][9][4] );
                S[1][12][8] = gmc1*S[1][9][8] + fak2* S[1][3][8];
                S[1][12][9] = gmc1*S[1][9][9] + fak2* (S[1][3][9] +S[1][9][3] );
                S[1][12][10] = gmc1*S[1][9][10] + fak2* S[1][3][10];
                S[1][13][5] = gmc2*S[1][10][5] + fak2* S[1][4][5];
                S[1][13][6] = gmc2*S[1][10][6] + fak * (2.0*S[1][4][6] +S[1][10][2] );
                S[1][13][7] = gmc2*S[1][10][7] + fak * (2.0*S[1][4][7] +S[1][10][3] );
                S[1][13][8] = gmc2*S[1][10][8] + fak2* S[1][4][8];
                S[1][13][9] = gmc2*S[1][10][9] + fak2* S[1][4][9];
                S[1][13][10] = gmc2*S[1][10][10] + fak2* (S[1][4][10] +S[1][10][4] );
             }

            // p-f-p elements
             if ( _lmax_alpha >= 1 && _lmax_gw >= 3 && _lmax_gamma >= 1) {
                S[2][11][2] = gma0*S[1][11][2] + fak * (3.0*S[1][8][2] +S[1][11][1] );
                S[3][11][2] = gma1*S[1][11][2];
                S[4][11][2] = gma2*S[1][11][2];
                S[2][11][3] = gma0*S[1][11][3] + fak3* S[1][8][3];
                S[3][11][3] = gma1*S[1][11][3] + fak * S[1][11][1];
                S[4][11][3] = gma2*S[1][11][3];
                S[2][11][4] = gma0*S[1][11][4] + fak3* S[1][8][4];
                S[3][11][4] = gma1*S[1][11][4];
                S[4][11][4] = gma2*S[1][11][4] + fak * S[1][11][1];
                S[2][12][2] = gma0*S[1][12][2] + fak * S[1][12][1];
                S[3][12][2] = gma1*S[1][12][2] + fak3* S[1][9][2];
                S[4][12][2] = gma2*S[1][12][2];
                S[2][12][3] = gma0*S[1][12][3];
                S[3][12][3] = gma1*S[1][12][3] + fak * (3.0*S[1][9][3] +S[1][12][1] );
                S[4][12][3] = gma2*S[1][12][3];
                S[2][12][4] = gma0*S[1][12][4];
                S[3][12][4] = gma1*S[1][12][4] + fak3* S[1][9][4];
                S[4][12][4] = gma2*S[1][12][4] + fak * S[1][12][1];
                S[2][13][2] = gma0*S[1][13][2] + fak * S[1][13][1];
                S[3][13][2] = gma1*S[1][13][2];
                S[4][13][2] = gma2*S[1][13][2] + fak3* S[1][10][2];
                S[2][13][3] = gma0*S[1][13][3];
                S[3][13][3] = gma1*S[1][13][3] + fak * S[1][13][1];
                S[4][13][3] = gma2*S[1][13][3] + fak3* S[1][10][3];
                S[2][13][4] = gma0*S[1][13][4];
                S[3][13][4] = gma1*S[1][13][4];
                S[4][13][4] = gma2*S[1][13][4] + fak * (3.0*S[1][10][4] +S[1][13][1] );
                S[2][14][2] = gma0*S[1][14][2] + fak * (2.0*S[1][5][2] +S[1][14][1] );
                S[3][14][2] = gma1*S[1][14][2] + fak * S[1][8][2];
                S[4][14][2] = gma2*S[1][14][2];
                S[2][14][3] = gma0*S[1][14][3] + fak2* S[1][5][3];
                S[3][14][3] = gma1*S[1][14][3] + fak * (S[1][8][3] +S[1][14][1] );
                S[4][14][3] = gma2*S[1][14][3];
                S[2][14][4] = gma0*S[1][14][4] + fak2* S[1][5][4];
                S[3][14][4] = gma1*S[1][14][4] + fak * S[1][8][4];
                S[4][14][4] = gma2*S[1][14][4] + fak * S[1][14][1];
                S[2][15][2] = gma0*S[1][15][2] + fak * (S[1][9][2] +S[1][15][1] );
                S[3][15][2] = gma1*S[1][15][2] + fak2* S[1][5][2];
                S[4][15][2] = gma2*S[1][15][2];
                S[2][15][3] = gma0*S[1][15][3] + fak * S[1][9][3];
                S[3][15][3] = gma1*S[1][15][3] + fak * (2.0*S[1][5][3] +S[1][15][1] );
                S[4][15][3] = gma2*S[1][15][3];
                S[2][15][4] = gma0*S[1][15][4] + fak * S[1][9][4];
                S[3][15][4] = gma1*S[1][15][4] + fak2* S[1][5][4];
                S[4][15][4] = gma2*S[1][15][4] + fak * S[1][15][1];
                S[2][16][2] = gma0*S[1][16][2] + fak * (2.0*S[1][6][2] +S[1][16][1] );
                S[3][16][2] = gma1*S[1][16][2];
                S[4][16][2] = gma2*S[1][16][2] + fak * S[1][8][2];
                S[2][16][3] = gma0*S[1][16][3] + fak2* S[1][6][3];
                S[3][16][3] = gma1*S[1][16][3] + fak * S[1][16][1];
                S[4][16][3] = gma2*S[1][16][3] + fak * S[1][8][3];
                S[2][16][4] = gma0*S[1][16][4] + fak2* S[1][6][4];
                S[3][16][4] = gma1*S[1][16][4];
                S[4][16][4] = gma2*S[1][16][4] + fak * (S[1][8][4] +S[1][16][1] );
                S[2][17][2] = gma0*S[1][17][2] + fak * (S[1][10][2] +S[1][17][1] );
                S[3][17][2] = gma1*S[1][17][2];
                S[4][17][2] = gma2*S[1][17][2] + fak2* S[1][6][2];
                S[2][17][3] = gma0*S[1][17][3] + fak * S[1][10][3];
                S[3][17][3] = gma1*S[1][17][3] + fak * S[1][17][1];
                S[4][17][3] = gma2*S[1][17][3] + fak2* S[1][6][3];
                S[2][17][4] = gma0*S[1][17][4] + fak * S[1][10][4];
                S[3][17][4] = gma1*S[1][17][4];
                S[4][17][4] = gma2*S[1][17][4] + fak * (2.0*S[1][6][4] +S[1][17][1] );
                S[2][18][2] = gma0*S[1][18][2] + fak * S[1][18][1];
                S[3][18][2] = gma1*S[1][18][2] + fak2* S[1][7][2];
                S[4][18][2] = gma2*S[1][18][2] + fak * S[1][9][2];
                S[2][18][3] = gma0*S[1][18][3];
                S[3][18][3] = gma1*S[1][18][3] + fak * (2.0*S[1][7][3] +S[1][18][1] );
                S[4][18][3] = gma2*S[1][18][3] + fak * S[1][9][3];
                S[2][18][4] = gma0*S[1][18][4];
                S[3][18][4] = gma1*S[1][18][4] + fak2* S[1][7][4];
                S[4][18][4] = gma2*S[1][18][4] + fak * (S[1][9][4] +S[1][18][1] );
                S[2][19][2] = gma0*S[1][19][2] + fak * S[1][19][1];
                S[3][19][2] = gma1*S[1][19][2] + fak * S[1][10][2];
                S[4][19][2] = gma2*S[1][19][2] + fak2* S[1][7][2];
                S[2][19][3] = gma0*S[1][19][3];
                S[3][19][3] = gma1*S[1][19][3] + fak * (S[1][10][3] +S[1][19][1] );
                S[4][19][3] = gma2*S[1][19][3] + fak2* S[1][7][3];
                S[2][19][4] = gma0*S[1][19][4];
                S[3][19][4] = gma1*S[1][19][4] + fak * S[1][10][4];
                S[4][19][4] = gma2*S[1][19][4] + fak * (2.0*S[1][7][4] +S[1][19][1] );
                S[2][20][2] = gma0*S[1][20][2] + fak * (S[1][7][2] +S[1][20][1] );
                S[3][20][2] = gma1*S[1][20][2] + fak * S[1][6][2];
                S[4][20][2] = gma2*S[1][20][2] + fak * S[1][5][2];
                S[2][20][3] = gma0*S[1][20][3] + fak * S[1][7][3];
                S[3][20][3] = gma1*S[1][20][3] + fak * (S[1][6][3] +S[1][20][1] );
                S[4][20][3] = gma2*S[1][20][3] + fak * S[1][5][3];
                S[2][20][4] = gma0*S[1][20][4] + fak * S[1][7][4];
                S[3][20][4] = gma1*S[1][20][4] + fak * S[1][6][4];
                S[4][20][4] = gma2*S[1][20][4] + fak * (S[1][5][4] +S[1][20][1] );
             }

            // d-f-s
             if ( _lmax_alpha >= 2 && _lmax_gw >= 3) {
                S[8][11][1] = gma0*S[2][11][1] + fak * (S[1][11][1] +3.0*S[2][8][1] );
                S[5][11][1] = gma1*S[2][11][1];
                S[6][11][1] = gma2*S[2][11][1];
                S[8][12][1] = gma0*S[2][12][1] + fak * S[1][12][1];
                S[5][12][1] = gma1*S[2][12][1] + fak3* S[2][9][1];
                S[6][12][1] = gma2*S[2][12][1];
                S[8][13][1] = gma0*S[2][13][1] + fak * S[1][13][1];
                S[5][13][1] = gma1*S[2][13][1];
                S[6][13][1] = gma2*S[2][13][1] + fak3* S[2][10][1];
                S[8][14][1] = gma0*S[2][14][1] + fak * (S[1][14][1] +2.0*S[2][5][1] );
                S[5][14][1] = gma1*S[2][14][1] + fak * S[2][8][1];
                S[6][14][1] = gma2*S[2][14][1];
                S[8][15][1] = gma0*S[2][15][1] + fak * (S[1][15][1] +S[2][9][1] );
                S[5][15][1] = gma1*S[2][15][1] + fak2* S[2][5][1];
                S[6][15][1] = gma2*S[2][15][1];
                S[8][16][1] = gma0*S[2][16][1] + fak * (S[1][16][1] +2.0*S[2][6][1] );
                S[5][16][1] = gma1*S[2][16][1];
                S[6][16][1] = gma2*S[2][16][1] + fak * S[2][8][1];
                S[8][17][1] = gma0*S[2][17][1] + fak * (S[1][17][1] +S[2][10][1] );
                S[5][17][1] = gma1*S[2][17][1];
                S[6][17][1] = gma2*S[2][17][1] + fak2* S[2][6][1];
                S[8][18][1] = gma0*S[2][18][1] + fak * S[1][18][1];
                S[5][18][1] = gma1*S[2][18][1] + fak2* S[2][7][1];
                S[6][18][1] = gma2*S[2][18][1] + fak * S[2][9][1];
                S[8][19][1] = gma0*S[2][19][1] + fak * S[1][19][1];
                S[5][19][1] = gma1*S[2][19][1] + fak * S[2][10][1];
                S[6][19][1] = gma2*S[2][19][1] + fak2* S[2][7][1];
                S[8][20][1] = gma0*S[2][20][1] + fak * (S[1][20][1] +S[2][7][1] );
                S[5][20][1] = gma1*S[2][20][1] + fak * S[2][6][1];
                S[6][20][1] = gma2*S[2][20][1] + fak * S[2][5][1];
                S[9][11][1] = gma1*S[3][11][1] + fak * S[1][11][1];
                S[7][11][1] = gma2*S[3][11][1];
                S[9][12][1] = gma1*S[3][12][1] + fak * (S[1][12][1] +3.0*S[3][9][1] );
                S[7][12][1] = gma2*S[3][12][1];
                S[9][13][1] = gma1*S[3][13][1] + fak * S[1][13][1];
                S[7][13][1] = gma2*S[3][13][1] + fak3* S[3][10][1];
                S[9][14][1] = gma1*S[3][14][1] + fak * (S[1][14][1] +S[3][8][1] );
                S[7][14][1] = gma2*S[3][14][1];
                S[9][15][1] = gma1*S[3][15][1] + fak * (S[1][15][1] +2.0*S[3][5][1] );
                S[7][15][1] = gma2*S[3][15][1];
                S[9][16][1] = gma1*S[3][16][1] + fak * S[1][16][1];
                S[7][16][1] = gma2*S[3][16][1] + fak * S[3][8][1];
                S[9][17][1] = gma1*S[3][17][1] + fak * S[1][17][1];
                S[7][17][1] = gma2*S[3][17][1] + fak2* S[3][6][1];
                S[9][18][1] = gma1*S[3][18][1] + fak * (S[1][18][1] +2.0*S[3][7][1] );
                S[7][18][1] = gma2*S[3][18][1] + fak * S[3][9][1];
                S[9][19][1] = gma1*S[3][19][1] + fak * (S[1][19][1] +S[3][10][1] );
                S[7][19][1] = gma2*S[3][19][1] + fak2* S[3][7][1];
                S[9][20][1] = gma1*S[3][20][1] + fak * (S[1][20][1] +S[3][6][1] );
                S[7][20][1] = gma2*S[3][20][1] + fak * S[3][5][1];
                S[10][11][1] = gma2*S[4][11][1] + fak * S[1][11][1];
                S[10][12][1] = gma2*S[4][12][1] + fak * S[1][12][1];
                S[10][13][1] = gma2*S[4][13][1] + fak * (S[1][13][1] +3.0*S[4][10][1] );
                S[10][14][1] = gma2*S[4][14][1] + fak * S[1][14][1];
                S[10][15][1] = gma2*S[4][15][1] + fak * S[1][15][1];
                S[10][16][1] = gma2*S[4][16][1] + fak * (S[1][16][1] +S[4][8][1] );
                S[10][17][1] = gma2*S[4][17][1] + fak * (S[1][17][1] +2.0*S[4][6][1] );
                S[10][18][1] = gma2*S[4][18][1] + fak * (S[1][18][1] +S[4][9][1] );
                S[10][19][1] = gma2*S[4][19][1] + fak * (S[1][19][1] +2.0*S[4][7][1] );
                S[10][20][1] = gma2*S[4][20][1] + fak * (S[1][20][1] +S[4][5][1] );
             }

            // p-f-d elements
             if ( _lmax_alpha >= 1 && _lmax_gw >= 3 && _lmax_gamma >= 2) {
                S[2][11][5] = gma0*S[1][11][5] + fak * (3.0*S[1][8][5] +S[1][11][3] );
                S[3][11][5] = gma1*S[1][11][5] + fak * S[1][11][2];
                S[4][11][5] = gma2*S[1][11][5];
                S[2][11][6] = gma0*S[1][11][6] + fak * (3.0*S[1][8][6] +S[1][11][4] );
                S[3][11][6] = gma1*S[1][11][6];
                S[4][11][6] = gma2*S[1][11][6] + fak * S[1][11][2];
                S[2][11][7] = gma0*S[1][11][7] + fak3* S[1][8][7];
                S[3][11][7] = gma1*S[1][11][7] + fak * S[1][11][4];
                S[4][11][7] = gma2*S[1][11][7] + fak * S[1][11][3];
                S[2][11][8] = gma0*S[1][11][8] + fak *(3.0*S[1][8][8]+2.0*S[1][11][2]);
                S[3][11][8] = gma1*S[1][11][8];
                S[4][11][8] = gma2*S[1][11][8];
                S[2][11][9] = gma0*S[1][11][9] + fak3* S[1][8][9];
                S[3][11][9] = gma1*S[1][11][9] + fak2* S[1][11][3];
                S[4][11][9] = gma2*S[1][11][9];
                S[2][11][10] = gma0*S[1][11][10] + fak3* S[1][8][10];
                S[3][11][10] = gma1*S[1][11][10];
                S[4][11][10] = gma2*S[1][11][10] + fak2* S[1][11][4];
                S[2][12][5] = gma0*S[1][12][5] + fak * S[1][12][3];
                S[3][12][5] = gma1*S[1][12][5] + fak * (3.0*S[1][9][5] +S[1][12][2] );
                S[4][12][5] = gma2*S[1][12][5];
                S[2][12][6] = gma0*S[1][12][6] + fak * S[1][12][4];
                S[3][12][6] = gma1*S[1][12][6] + fak3* S[1][9][6];
                S[4][12][6] = gma2*S[1][12][6] + fak * S[1][12][2];
                S[2][12][7] = gma0*S[1][12][7];
                S[3][12][7] = gma1*S[1][12][7] + fak * (3.0*S[1][9][7] +S[1][12][4] );
                S[4][12][7] = gma2*S[1][12][7] + fak * S[1][12][3];
                S[2][12][8] = gma0*S[1][12][8] + fak2* S[1][12][2];
                S[3][12][8] = gma1*S[1][12][8] + fak3* S[1][9][8];
                S[4][12][8] = gma2*S[1][12][8];
                S[2][12][9] = gma0*S[1][12][9];
                S[3][12][9] = gma1*S[1][12][9] + fak *(3.0*S[1][9][9]+2.0*S[1][12][3]);
                S[4][12][9] = gma2*S[1][12][9];
                S[2][12][10] = gma0*S[1][12][10];
                S[3][12][10] = gma1*S[1][12][10] + fak3* S[1][9][10];
                S[4][12][10] = gma2*S[1][12][10] + fak2* S[1][12][4];
                S[2][13][5] = gma0*S[1][13][5] + fak * S[1][13][3];
                S[3][13][5] = gma1*S[1][13][5] + fak * S[1][13][2];
                S[4][13][5] = gma2*S[1][13][5] + fak3* S[1][10][5];
                S[2][13][6] = gma0*S[1][13][6] + fak * S[1][13][4];
                S[3][13][6] = gma1*S[1][13][6];
                S[4][13][6] = gma2*S[1][13][6] + fak * (3.0*S[1][10][6] +S[1][13][2] );
                S[2][13][7] = gma0*S[1][13][7];
                S[3][13][7] = gma1*S[1][13][7] + fak * S[1][13][4];
                S[4][13][7] = gma2*S[1][13][7] + fak * (3.0*S[1][10][7] +S[1][13][3] );
                S[2][13][8] = gma0*S[1][13][8] + fak2* S[1][13][2];
                S[3][13][8] = gma1*S[1][13][8];
                S[4][13][8] = gma2*S[1][13][8] + fak3* S[1][10][8];
                S[2][13][9] = gma0*S[1][13][9];
                S[3][13][9] = gma1*S[1][13][9] + fak2* S[1][13][3];
                S[4][13][9] = gma2*S[1][13][9] + fak3* S[1][10][9];
                S[2][13][10] = gma0*S[1][13][10];
                S[3][13][10] = gma1*S[1][13][10];
                S[4][13][10] = gma2*S[1][13][10] +    fak * (3.0*S[1][10][10] +2.0*S[1][13][4] );
                S[2][14][5] = gma0*S[1][14][5] + fak * (2.0*S[1][5][5] +S[1][14][3] );
                S[3][14][5] = gma1*S[1][14][5] + fak * (S[1][8][5] +S[1][14][2] );
                S[4][14][5] = gma2*S[1][14][5];
                S[2][14][6] = gma0*S[1][14][6] + fak * (2.0*S[1][5][6] +S[1][14][4] );
                S[3][14][6] = gma1*S[1][14][6] + fak * S[1][8][6];
                S[4][14][6] = gma2*S[1][14][6] + fak * S[1][14][2];
                S[2][14][7] = gma0*S[1][14][7] + fak2* S[1][5][7];
                S[3][14][7] = gma1*S[1][14][7] + fak * (S[1][8][7] +S[1][14][4] );
                S[4][14][7] = gma2*S[1][14][7] + fak * S[1][14][3];
                S[2][14][8] = gma0*S[1][14][8] + fak2* (S[1][5][8] +S[1][14][2] );
                S[3][14][8] = gma1*S[1][14][8] + fak * S[1][8][8];
                S[4][14][8] = gma2*S[1][14][8];
                S[2][14][9] = gma0*S[1][14][9] + fak2* S[1][5][9];
                S[3][14][9] = gma1*S[1][14][9] + fak * (S[1][8][9] +2.0*S[1][14][3] );
                S[4][14][9] = gma2*S[1][14][9];
                S[2][14][10] = gma0*S[1][14][10] + fak2* S[1][5][10];
                S[3][14][10] = gma1*S[1][14][10] + fak * S[1][8][10];
                S[4][14][10] = gma2*S[1][14][10] + fak2* S[1][14][4];
                S[2][15][5] = gma0*S[1][15][5] + fak * (S[1][9][5] +S[1][15][3] );
                S[3][15][5] = gma1*S[1][15][5] + fak * (2.0*S[1][5][5] +S[1][15][2] );
                S[4][15][5] = gma2*S[1][15][5];
                S[2][15][6] = gma0*S[1][15][6] + fak * (S[1][9][6] +S[1][15][4] );
                S[3][15][6] = gma1*S[1][15][6] + fak2* S[1][5][6];
                S[4][15][6] = gma2*S[1][15][6] + fak * S[1][15][2];
                S[2][15][7] = gma0*S[1][15][7] + fak * S[1][9][7];
                S[3][15][7] = gma1*S[1][15][7] + fak * (2.0*S[1][5][7] +S[1][15][4] );
                S[4][15][7] = gma2*S[1][15][7] + fak * S[1][15][3];
                S[2][15][8] = gma0*S[1][15][8] + fak * (S[1][9][8] +2.0*S[1][15][2] );
                S[3][15][8] = gma1*S[1][15][8] + fak2* S[1][5][8];
                S[4][15][8] = gma2*S[1][15][8];
                S[2][15][9] = gma0*S[1][15][9] + fak * S[1][9][9];
                S[3][15][9] = gma1*S[1][15][9] + fak2* (S[1][5][9] +S[1][15][3] );
                S[4][15][9] = gma2*S[1][15][9];
                S[2][15][10] = gma0*S[1][15][10] + fak * S[1][9][10];
                S[3][15][10] = gma1*S[1][15][10] + fak2* S[1][5][10];
                S[4][15][10] = gma2*S[1][15][10] + fak2* S[1][15][4];
                S[2][16][5] = gma0*S[1][16][5] + fak * (2.0*S[1][6][5] +S[1][16][3] );
                S[3][16][5] = gma1*S[1][16][5] + fak * S[1][16][2];
                S[4][16][5] = gma2*S[1][16][5] + fak * S[1][8][5];
                S[2][16][6] = gma0*S[1][16][6] + fak * (2.0*S[1][6][6] +S[1][16][4] );
                S[3][16][6] = gma1*S[1][16][6];
                S[4][16][6] = gma2*S[1][16][6] + fak * (S[1][8][6] +S[1][16][2] );
                S[2][16][7] = gma0*S[1][16][7] + fak2* S[1][6][7];
                S[3][16][7] = gma1*S[1][16][7] + fak * S[1][16][4];
                S[4][16][7] = gma2*S[1][16][7] + fak * (S[1][8][7] +S[1][16][3] );
                S[2][16][8] = gma0*S[1][16][8] + fak2* (S[1][6][8] +S[1][16][2] );
                S[3][16][8] = gma1*S[1][16][8];
                S[4][16][8] = gma2*S[1][16][8] + fak * S[1][8][8];
                S[2][16][9] = gma0*S[1][16][9] + fak2* S[1][6][9];
                S[3][16][9] = gma1*S[1][16][9] + fak2* S[1][16][3];
                S[4][16][9] = gma2*S[1][16][9] + fak * S[1][8][9];
                S[2][16][10] = gma0*S[1][16][10] + fak2* S[1][6][10];
                S[3][16][10] = gma1*S[1][16][10];
                S[4][16][10] = gma2*S[1][16][10] + fak * (S[1][8][10] +2.0*S[1][16][4]);
                S[2][17][5] = gma0*S[1][17][5] + fak * (S[1][10][5] +S[1][17][3] );
                S[3][17][5] = gma1*S[1][17][5] + fak * S[1][17][2];
                S[4][17][5] = gma2*S[1][17][5] + fak2* S[1][6][5];
                S[2][17][6] = gma0*S[1][17][6] + fak * (S[1][10][6] +S[1][17][4] );
                S[3][17][6] = gma1*S[1][17][6];
                S[4][17][6] = gma2*S[1][17][6] + fak * (2.0*S[1][6][6] +S[1][17][2] );
                S[2][17][7] = gma0*S[1][17][7] + fak * S[1][10][7];
                S[3][17][7] = gma1*S[1][17][7] + fak * S[1][17][4];
                S[4][17][7] = gma2*S[1][17][7] + fak * (2.0*S[1][6][7] +S[1][17][3] );
                S[2][17][8] = gma0*S[1][17][8] + fak * (S[1][10][8] +2.0*S[1][17][2] );
                S[3][17][8] = gma1*S[1][17][8];
                S[4][17][8] = gma2*S[1][17][8] + fak2* S[1][6][8];
                S[2][17][9] = gma0*S[1][17][9] + fak * S[1][10][9];
                S[3][17][9] = gma1*S[1][17][9] + fak2* S[1][17][3];
                S[4][17][9] = gma2*S[1][17][9] + fak2* S[1][6][9];
                S[2][17][10] = gma0*S[1][17][10] + fak * S[1][10][10];
                S[3][17][10] = gma1*S[1][17][10];
                S[4][17][10] = gma2*S[1][17][10] + fak2* (S[1][6][10] +S[1][17][4] );
                S[2][18][5] = gma0*S[1][18][5] + fak * S[1][18][3];
                S[3][18][5] = gma1*S[1][18][5] + fak * (2.0*S[1][7][5] +S[1][18][2] );
                S[4][18][5] = gma2*S[1][18][5] + fak * S[1][9][5];
                S[2][18][6] = gma0*S[1][18][6] + fak * S[1][18][4];
                S[3][18][6] = gma1*S[1][18][6] + fak2* S[1][7][6];
                S[4][18][6] = gma2*S[1][18][6] + fak * (S[1][9][6] +S[1][18][2] );
                S[2][18][7] = gma0*S[1][18][7];
                S[3][18][7] = gma1*S[1][18][7] + fak * (2.0*S[1][7][7] +S[1][18][4] );
                S[4][18][7] = gma2*S[1][18][7] + fak * (S[1][9][7] +S[1][18][3] );
                S[2][18][8] = gma0*S[1][18][8] + fak2* S[1][18][2];
                S[3][18][8] = gma1*S[1][18][8] + fak2* S[1][7][8];
                S[4][18][8] = gma2*S[1][18][8] + fak * S[1][9][8];
                S[2][18][9] = gma0*S[1][18][9];
                S[3][18][9] = gma1*S[1][18][9] + fak2* (S[1][7][9] +S[1][18][3] );
                S[4][18][9] = gma2*S[1][18][9] + fak * S[1][9][9];
                S[2][18][10] = gma0*S[1][18][10];
                S[3][18][10] = gma1*S[1][18][10] + fak2* S[1][7][10];
                S[4][18][10] = gma2*S[1][18][10] + fak * (S[1][9][10] +2.0*S[1][18][4]);
                S[2][19][5] = gma0*S[1][19][5] + fak * S[1][19][3];
                S[3][19][5] = gma1*S[1][19][5] + fak * (S[1][10][5] +S[1][19][2] );
                S[4][19][5] = gma2*S[1][19][5] + fak2* S[1][7][5];
                S[2][19][6] = gma0*S[1][19][6] + fak * S[1][19][4];
                S[3][19][6] = gma1*S[1][19][6] + fak * S[1][10][6];
                S[4][19][6] = gma2*S[1][19][6] + fak * (2.0*S[1][7][6] +S[1][19][2] );
                S[2][19][7] = gma0*S[1][19][7];
                S[3][19][7] = gma1*S[1][19][7] + fak * (S[1][10][7] +S[1][19][4] );
                S[4][19][7] = gma2*S[1][19][7] + fak * (2.0*S[1][7][7] +S[1][19][3] );
                S[2][19][8] = gma0*S[1][19][8] + fak2* S[1][19][2];
                S[3][19][8] = gma1*S[1][19][8] + fak * S[1][10][8];
                S[4][19][8] = gma2*S[1][19][8] + fak2* S[1][7][8];
                S[2][19][9] = gma0*S[1][19][9];
                S[3][19][9] = gma1*S[1][19][9] + fak * (S[1][10][9] +2.0*S[1][19][3] );
                S[4][19][9] = gma2*S[1][19][9] + fak2* S[1][7][9];
                S[2][19][10] = gma0*S[1][19][10];
                S[3][19][10] = gma1*S[1][19][10] + fak * S[1][10][10];
                S[4][19][10] = gma2*S[1][19][10] + fak2* (S[1][7][10] +S[1][19][4] );
                S[2][20][5] = gma0*S[1][20][5] + fak * (S[1][7][5] +S[1][20][3] );
                S[3][20][5] = gma1*S[1][20][5] + fak * (S[1][6][5] +S[1][20][2] );
                S[4][20][5] = gma2*S[1][20][5] + fak * S[1][5][5];
                S[2][20][6] = gma0*S[1][20][6] + fak * (S[1][7][6] +S[1][20][4] );
                S[3][20][6] = gma1*S[1][20][6] + fak * S[1][6][6];
                S[4][20][6] = gma2*S[1][20][6] + fak * (S[1][5][6] +S[1][20][2] );
                S[2][20][7] = gma0*S[1][20][7] + fak * S[1][7][7];
                S[3][20][7] = gma1*S[1][20][7] + fak * (S[1][6][7] +S[1][20][4] );
                S[4][20][7] = gma2*S[1][20][7] + fak * (S[1][5][7] +S[1][20][3] );
                S[2][20][8] = gma0*S[1][20][8] + fak * (S[1][7][8] +2.0*S[1][20][2] );
                S[3][20][8] = gma1*S[1][20][8] + fak * S[1][6][8];
                S[4][20][8] = gma2*S[1][20][8] + fak * S[1][5][8];
                S[2][20][9] = gma0*S[1][20][9] + fak * S[1][7][9];
                S[3][20][9] = gma1*S[1][20][9] + fak * (S[1][6][9] +2.0*S[1][20][3] );
                S[4][20][9] = gma2*S[1][20][9] + fak * S[1][5][9];
                S[2][20][10] = gma0*S[1][20][10] + fak * S[1][7][10];
                S[3][20][10] = gma1*S[1][20][10] + fak * S[1][6][10];
                S[4][20][10] = gma2*S[1][20][10] + fak * (S[1][5][10] +2.0*S[1][20][4]);
             }

            // d-f-p elements
             if ( _lmax_alpha >= 2 && _lmax_gw >= 3 && _lmax_gamma >= 1) {
                S[8][11][2] = gma0*S[2][11][2] + fak * (S[1][11][2] +3.0*S[2][8][2] +S[2][11][1] );
                S[5][11][2] = gma1*S[2][11][2];
                S[6][11][2] = gma2*S[2][11][2];
                S[8][11][3] = gma0*S[2][11][3] + fak * (S[1][11][3] +3.0*S[2][8][3] );
                S[5][11][3] = gma1*S[2][11][3] + fak * S[2][11][1];
                S[6][11][3] = gma2*S[2][11][3];
                S[8][11][4] = gma0*S[2][11][4] + fak * (S[1][11][4] +3.0*S[2][8][4] );
                S[5][11][4] = gma1*S[2][11][4];
                S[6][11][4] = gma2*S[2][11][4] + fak * S[2][11][1];
                S[8][12][2] = gma0*S[2][12][2] + fak * (S[1][12][2] +S[2][12][1] );
                S[5][12][2] = gma1*S[2][12][2] + fak3* S[2][9][2];
                S[6][12][2] = gma2*S[2][12][2];
                S[8][12][3] = gma0*S[2][12][3] + fak * S[1][12][3];
                S[5][12][3] = gma1*S[2][12][3] + fak * (3.0*S[2][9][3] +S[2][12][1] );
                S[6][12][3] = gma2*S[2][12][3];
                S[8][12][4] = gma0*S[2][12][4] + fak * S[1][12][4];
                S[5][12][4] = gma1*S[2][12][4] + fak3* S[2][9][4];
                S[6][12][4] = gma2*S[2][12][4] + fak * S[2][12][1];
                S[8][13][2] = gma0*S[2][13][2] + fak * (S[1][13][2] +S[2][13][1] );
                S[5][13][2] = gma1*S[2][13][2];
                S[6][13][2] = gma2*S[2][13][2] + fak3* S[2][10][2];
                S[8][13][3] = gma0*S[2][13][3] + fak * S[1][13][3];
                S[5][13][3] = gma1*S[2][13][3] + fak * S[2][13][1];
                S[6][13][3] = gma2*S[2][13][3] + fak3* S[2][10][3];
                S[8][13][4] = gma0*S[2][13][4] + fak * S[1][13][4];
                S[5][13][4] = gma1*S[2][13][4];
                S[6][13][4] = gma2*S[2][13][4] + fak * (3.0*S[2][10][4] +S[2][13][1] );
                S[8][14][2] = gma0*S[2][14][2] + fak * (S[1][14][2] +2.0*S[2][5][2] +S[2][14][1] );
                S[5][14][2] = gma1*S[2][14][2] + fak * S[2][8][2];
                S[6][14][2] = gma2*S[2][14][2];
                S[8][14][3] = gma0*S[2][14][3] + fak * (S[1][14][3] +2.0*S[2][5][3] );
                S[5][14][3] = gma1*S[2][14][3] + fak * (S[2][8][3] +S[2][14][1] );
                S[6][14][3] = gma2*S[2][14][3];
                S[8][14][4] = gma0*S[2][14][4] + fak * (S[1][14][4] +2.0*S[2][5][4] );
                S[5][14][4] = gma1*S[2][14][4] + fak * S[2][8][4];
                S[6][14][4] = gma2*S[2][14][4] + fak * S[2][14][1];
                S[8][15][2] = gma0*S[2][15][2] + fak *(S[1][15][2]+S[2][9][2]+S[2][15][1]);
                S[5][15][2] = gma1*S[2][15][2] + fak2* S[2][5][2];
                S[6][15][2] = gma2*S[2][15][2];
                S[8][15][3] = gma0*S[2][15][3] + fak * (S[1][15][3] +S[2][9][3] );
                S[5][15][3] = gma1*S[2][15][3] + fak * (2.0*S[2][5][3] +S[2][15][1] );
                S[6][15][3] = gma2*S[2][15][3];
                S[8][15][4] = gma0*S[2][15][4] + fak * (S[1][15][4] +S[2][9][4] );
                S[5][15][4] = gma1*S[2][15][4] + fak2* S[2][5][4];
                S[6][15][4] = gma2*S[2][15][4] + fak * S[2][15][1];
                S[8][16][2] = gma0*S[2][16][2] + fak * (S[1][16][2] +2.0*S[2][6][2] +S[2][16][1] );
                S[5][16][2] = gma1*S[2][16][2];
                S[6][16][2] = gma2*S[2][16][2] + fak * S[2][8][2];
                S[8][16][3] = gma0*S[2][16][3] + fak * (S[1][16][3] +2.0*S[2][6][3] );
                S[5][16][3] = gma1*S[2][16][3] + fak * S[2][16][1];
                S[6][16][3] = gma2*S[2][16][3] + fak * S[2][8][3];
                S[8][16][4] = gma0*S[2][16][4] + fak * (S[1][16][4] +2.0*S[2][6][4] );
                S[5][16][4] = gma1*S[2][16][4];
                S[6][16][4] = gma2*S[2][16][4] + fak * (S[2][8][4] +S[2][16][1] );
                S[8][17][2] = gma0*S[2][17][2] + fak*(S[1][17][2]+S[2][10][2]+S[2][17][1]);
                S[5][17][2] = gma1*S[2][17][2];
                S[6][17][2] = gma2*S[2][17][2] + fak2* S[2][6][2];
                S[8][17][3] = gma0*S[2][17][3] + fak * (S[1][17][3] +S[2][10][3] );
                S[5][17][3] = gma1*S[2][17][3] + fak * S[2][17][1];
                S[6][17][3] = gma2*S[2][17][3] + fak2* S[2][6][3];
                S[8][17][4] = gma0*S[2][17][4] + fak * (S[1][17][4] +S[2][10][4] );
                S[5][17][4] = gma1*S[2][17][4];
                S[6][17][4] = gma2*S[2][17][4] + fak * (2.0*S[2][6][4] +S[2][17][1] );
                S[8][18][2] = gma0*S[2][18][2] + fak * (S[1][18][2] +S[2][18][1] );
                S[5][18][2] = gma1*S[2][18][2] + fak2* S[2][7][2];
                S[6][18][2] = gma2*S[2][18][2] + fak * S[2][9][2];
                S[8][18][3] = gma0*S[2][18][3] + fak * S[1][18][3];
                S[5][18][3] = gma1*S[2][18][3] + fak * (2.0*S[2][7][3] +S[2][18][1] );
                S[6][18][3] = gma2*S[2][18][3] + fak * S[2][9][3];
                S[8][18][4] = gma0*S[2][18][4] + fak * S[1][18][4];
                S[5][18][4] = gma1*S[2][18][4] + fak2* S[2][7][4];
                S[6][18][4] = gma2*S[2][18][4] + fak * (S[2][9][4] +S[2][18][1] );
                S[8][19][2] = gma0*S[2][19][2] + fak * (S[1][19][2] +S[2][19][1] );
                S[5][19][2] = gma1*S[2][19][2] + fak * S[2][10][2];
                S[6][19][2] = gma2*S[2][19][2] + fak2* S[2][7][2];
                S[8][19][3] = gma0*S[2][19][3] + fak * S[1][19][3];
                S[5][19][3] = gma1*S[2][19][3] + fak * (S[2][10][3] +S[2][19][1] );
                S[6][19][3] = gma2*S[2][19][3] + fak2* S[2][7][3];
                S[8][19][4] = gma0*S[2][19][4] + fak * S[1][19][4];
                S[5][19][4] = gma1*S[2][19][4] + fak * S[2][10][4];
                S[6][19][4] = gma2*S[2][19][4] + fak * (2.0*S[2][7][4] +S[2][19][1] );
                S[8][20][2] = gma0*S[2][20][2] + fak *(S[1][20][2]+S[2][7][2]+S[2][20][1]);
                S[5][20][2] = gma1*S[2][20][2] + fak * S[2][6][2];
                S[6][20][2] = gma2*S[2][20][2] + fak * S[2][5][2];
                S[8][20][3] = gma0*S[2][20][3] + fak * (S[1][20][3] +S[2][7][3] );
                S[5][20][3] = gma1*S[2][20][3] + fak * (S[2][6][3] +S[2][20][1] );
                S[6][20][3] = gma2*S[2][20][3] + fak * S[2][5][3];
                S[8][20][4] = gma0*S[2][20][4] + fak * (S[1][20][4] +S[2][7][4] );
                S[5][20][4] = gma1*S[2][20][4] + fak * S[2][6][4];
                S[6][20][4] = gma2*S[2][20][4] + fak * (S[2][5][4] +S[2][20][1] );
                S[9][11][2] = gma1*S[3][11][2] + fak * S[1][11][2];
                S[7][11][2] = gma2*S[3][11][2];
                S[9][11][3] = gma1*S[3][11][3] + fak * (S[1][11][3] +S[3][11][1] );
                S[7][11][3] = gma2*S[3][11][3];
                S[9][11][4] = gma1*S[3][11][4] + fak * S[1][11][4];
                S[7][11][4] = gma2*S[3][11][4] + fak * S[3][11][1];
                S[9][12][2] = gma1*S[3][12][2] + fak * (S[1][12][2] +3.0*S[3][9][2] );
                S[7][12][2] = gma2*S[3][12][2];
                S[9][12][3] = gma1*S[3][12][3] + fak * (S[1][12][3] +3.0*S[3][9][3] +S[3][12][1] );
                S[7][12][3] = gma2*S[3][12][3];
                S[9][12][4] = gma1*S[3][12][4] + fak * (S[1][12][4] +3.0*S[3][9][4] );
                S[7][12][4] = gma2*S[3][12][4] + fak * S[3][12][1];
                S[9][13][2] = gma1*S[3][13][2] + fak * S[1][13][2];
                S[7][13][2] = gma2*S[3][13][2] + fak3* S[3][10][2];
                S[9][13][3] = gma1*S[3][13][3] + fak * (S[1][13][3] +S[3][13][1] );
                S[7][13][3] = gma2*S[3][13][3] + fak3* S[3][10][3];
                S[9][13][4] = gma1*S[3][13][4] + fak * S[1][13][4];
                S[7][13][4] = gma2*S[3][13][4] + fak * (3.0*S[3][10][4] +S[3][13][1] );
                S[9][14][2] = gma1*S[3][14][2] + fak * (S[1][14][2] +S[3][8][2] );
                S[7][14][2] = gma2*S[3][14][2];
                S[9][14][3] = gma1*S[3][14][3] + fak *(S[1][14][3]+S[3][8][3]+S[3][14][1]);
                S[7][14][3] = gma2*S[3][14][3];
                S[9][14][4] = gma1*S[3][14][4] + fak * (S[1][14][4] +S[3][8][4] );
                S[7][14][4] = gma2*S[3][14][4] + fak * S[3][14][1];
                S[9][15][2] = gma1*S[3][15][2] + fak * (S[1][15][2] +2.0*S[3][5][2] );
                S[7][15][2] = gma2*S[3][15][2];
                S[9][15][3] = gma1*S[3][15][3] + fak * (S[1][15][3] +2.0*S[3][5][3] +S[3][15][1] );
                S[7][15][3] = gma2*S[3][15][3];
                S[9][15][4] = gma1*S[3][15][4] + fak * (S[1][15][4] +2.0*S[3][5][4] );
                S[7][15][4] = gma2*S[3][15][4] + fak * S[3][15][1];
                S[9][16][2] = gma1*S[3][16][2] + fak * S[1][16][2];
                S[7][16][2] = gma2*S[3][16][2] + fak * S[3][8][2];
                S[9][16][3] = gma1*S[3][16][3] + fak * (S[1][16][3] +S[3][16][1] );
                S[7][16][3] = gma2*S[3][16][3] + fak * S[3][8][3];
                S[9][16][4] = gma1*S[3][16][4] + fak * S[1][16][4];
                S[7][16][4] = gma2*S[3][16][4] + fak * (S[3][8][4] +S[3][16][1] );
                S[9][17][2] = gma1*S[3][17][2] + fak * S[1][17][2];
                S[7][17][2] = gma2*S[3][17][2] + fak2* S[3][6][2];
                S[9][17][3] = gma1*S[3][17][3] + fak * (S[1][17][3] +S[3][17][1] );
                S[7][17][3] = gma2*S[3][17][3] + fak2* S[3][6][3];
                S[9][17][4] = gma1*S[3][17][4] + fak * S[1][17][4];
                S[7][17][4] = gma2*S[3][17][4] + fak * (2.0*S[3][6][4] +S[3][17][1] );
                S[9][18][2] = gma1*S[3][18][2] + fak * (S[1][18][2] +2.0*S[3][7][2] );
                S[7][18][2] = gma2*S[3][18][2] + fak * S[3][9][2];
                S[9][18][3] = gma1*S[3][18][3] + fak * (S[1][18][3] +2.0*S[3][7][3] +S[3][18][1] );
                S[7][18][3] = gma2*S[3][18][3] + fak * S[3][9][3];
                S[9][18][4] = gma1*S[3][18][4] + fak * (S[1][18][4] +2.0*S[3][7][4] );
                S[7][18][4] = gma2*S[3][18][4] + fak * (S[3][9][4] +S[3][18][1] );
                S[9][19][2] = gma1*S[3][19][2] + fak * (S[1][19][2] +S[3][10][2] );
                S[7][19][2] = gma2*S[3][19][2] + fak2* S[3][7][2];
                S[9][19][3] = gma1*S[3][19][3] + fak*(S[1][19][3]+S[3][10][3]+S[3][19][1]);
                S[7][19][3] = gma2*S[3][19][3] + fak2* S[3][7][3];
                S[9][19][4] = gma1*S[3][19][4] + fak * (S[1][19][4] +S[3][10][4] );
                S[7][19][4] = gma2*S[3][19][4] + fak * (2.0*S[3][7][4] +S[3][19][1] );
                S[9][20][2] = gma1*S[3][20][2] + fak * (S[1][20][2] +S[3][6][2] );
                S[7][20][2] = gma2*S[3][20][2] + fak * S[3][5][2];
                S[9][20][3] = gma1*S[3][20][3] + fak *(S[1][20][3]+S[3][6][3]+S[3][20][1]);
                S[7][20][3] = gma2*S[3][20][3] + fak * S[3][5][3];
                S[9][20][4] = gma1*S[3][20][4] + fak * (S[1][20][4] +S[3][6][4] );
                S[7][20][4] = gma2*S[3][20][4] + fak * (S[3][5][4] +S[3][20][1] );
                S[10][11][2] = gma2*S[4][11][2] + fak * S[1][11][2];
                S[10][11][3] = gma2*S[4][11][3] + fak * S[1][11][3];
                S[10][11][4] = gma2*S[4][11][4] + fak * (S[1][11][4] +S[4][11][1] );
                S[10][12][2] = gma2*S[4][12][2] + fak * S[1][12][2];
                S[10][12][3] = gma2*S[4][12][3] + fak * S[1][12][3];
                S[10][12][4] = gma2*S[4][12][4] + fak * (S[1][12][4] +S[4][12][1] );
                S[10][13][2] = gma2*S[4][13][2] + fak * (S[1][13][2] +3.0*S[4][10][2] );
                S[10][13][3] = gma2*S[4][13][3] + fak * (S[1][13][3] +3.0*S[4][10][3] );
                S[10][13][4] = gma2*S[4][13][4] + fak * (S[1][13][4] +3.0*S[4][10][4] +S[4][13][1] );
                S[10][14][2] = gma2*S[4][14][2] + fak * S[1][14][2];
                S[10][14][3] = gma2*S[4][14][3] + fak * S[1][14][3];
                S[10][14][4] = gma2*S[4][14][4] + fak * (S[1][14][4] +S[4][14][1] );
                S[10][15][2] = gma2*S[4][15][2] + fak * S[1][15][2];
                S[10][15][3] = gma2*S[4][15][3] + fak * S[1][15][3];
                S[10][15][4] = gma2*S[4][15][4] + fak * (S[1][15][4] +S[4][15][1] );
                S[10][16][2] = gma2*S[4][16][2] + fak * (S[1][16][2] +S[4][8][2] );
                S[10][16][3] = gma2*S[4][16][3] + fak * (S[1][16][3] +S[4][8][3] );
                S[10][16][4] = gma2*S[4][16][4] + fak*(S[1][16][4]+S[4][8][4]+S[4][16][1]);
                S[10][17][2] = gma2*S[4][17][2] + fak * (S[1][17][2] +2.0*S[4][6][2] );
                S[10][17][3] = gma2*S[4][17][3] + fak * (S[1][17][3] +2.0*S[4][6][3] );
                S[10][17][4] = gma2*S[4][17][4] + fak * (S[1][17][4] +2.0*S[4][6][4] +S[4][17][1] );
                S[10][18][2] = gma2*S[4][18][2] + fak * (S[1][18][2] +S[4][9][2] );
                S[10][18][3] = gma2*S[4][18][3] + fak * (S[1][18][3] +S[4][9][3] );
                S[10][18][4] = gma2*S[4][18][4] + fak*(S[1][18][4]+S[4][9][4]+S[4][18][1]);
                S[10][19][2] = gma2*S[4][19][2] + fak * (S[1][19][2] +2.0*S[4][7][2] );
                S[10][19][3] = gma2*S[4][19][3] + fak * (S[1][19][3] +2.0*S[4][7][3] );
                S[10][19][4] = gma2*S[4][19][4] + fak * (S[1][19][4] +2.0*S[4][7][4] +S[4][19][1] );
                S[10][20][2] = gma2*S[4][20][2] + fak * (S[1][20][2] +S[4][5][2] );
                S[10][20][3] = gma2*S[4][20][3] + fak * (S[1][20][3] +S[4][5][3] );
                S[10][20][4] = gma2*S[4][20][4] + fak*(S[1][20][4]+S[4][5][4]+S[4][20][1]);
             }

            // d-f-d elements
             if ( _lmax_alpha >= 2 && _lmax_gw >= 3 && _lmax_gamma >= 2) {
                S[8][11][5] = gma0*S[2][11][5] + fak * (S[1][11][5] +3.0*S[2][8][5] +S[2][11][3] );
                S[5][11][5] = gma1*S[2][11][5] + fak * S[2][11][2];
                S[6][11][5] = gma2*S[2][11][5];
                S[8][11][6] = gma0*S[2][11][6] + fak * (S[1][11][6] +3.0*S[2][8][6] +S[2][11][4] );
                S[5][11][6] = gma1*S[2][11][6];
                S[6][11][6] = gma2*S[2][11][6] + fak * S[2][11][2];
                S[8][11][7] = gma0*S[2][11][7] + fak * (S[1][11][7] +3.0*S[2][8][7] );
                S[5][11][7] = gma1*S[2][11][7] + fak * S[2][11][4];
                S[6][11][7] = gma2*S[2][11][7] + fak * S[2][11][3];
                S[8][11][8] = gma0*S[2][11][8] + fak * (S[1][11][8] +3.0*S[2][8][8] +2.0*S[2][11][2] );
                S[5][11][8] = gma1*S[2][11][8];
                S[6][11][8] = gma2*S[2][11][8];
                S[8][11][9] = gma0*S[2][11][9] + fak * (S[1][11][9] +3.0*S[2][8][9] );
                S[5][11][9] = gma1*S[2][11][9] + fak2* S[2][11][3];
                S[6][11][9] = gma2*S[2][11][9];
                S[8][11][10] = gma0*S[2][11][10] + fak * (S[1][11][10]+3.0*S[2][8][10]);
                S[5][11][10] = gma1*S[2][11][10];
                S[6][11][10] = gma2*S[2][11][10] + fak2* S[2][11][4];
                S[8][12][5] = gma0*S[2][12][5] + fak * (S[1][12][5] +S[2][12][3] );
                S[5][12][5] = gma1*S[2][12][5] + fak * (3.0*S[2][9][5] +S[2][12][2] );
                S[6][12][5] = gma2*S[2][12][5];
                S[8][12][6] = gma0*S[2][12][6] + fak * (S[1][12][6] +S[2][12][4] );
                S[5][12][6] = gma1*S[2][12][6] + fak3* S[2][9][6];
                S[6][12][6] = gma2*S[2][12][6] + fak * S[2][12][2];
                S[8][12][7] = gma0*S[2][12][7] + fak * S[1][12][7];
                S[5][12][7] = gma1*S[2][12][7] + fak * (3.0*S[2][9][7] +S[2][12][4] );
                S[6][12][7] = gma2*S[2][12][7] + fak * S[2][12][3];
                S[8][12][8] = gma0*S[2][12][8] + fak * (S[1][12][8] +2.0*S[2][12][2] );
                S[5][12][8] = gma1*S[2][12][8] + fak3* S[2][9][8];
                S[6][12][8] = gma2*S[2][12][8];
                S[8][12][9] = gma0*S[2][12][9] + fak * S[1][12][9];
                S[5][12][9] = gma1*S[2][12][9] + fak *(3.0*S[2][9][9]+2.0*S[2][12][3]);
                S[6][12][9] = gma2*S[2][12][9];
                S[8][12][10] = gma0*S[2][12][10] + fak * S[1][12][10];
                S[5][12][10] = gma1*S[2][12][10] + fak3* S[2][9][10];
                S[6][12][10] = gma2*S[2][12][10] + fak2* S[2][12][4];
                S[8][13][5] = gma0*S[2][13][5] + fak * (S[1][13][5] +S[2][13][3] );
                S[5][13][5] = gma1*S[2][13][5] + fak * S[2][13][2];
                S[6][13][5] = gma2*S[2][13][5] + fak3* S[2][10][5];
                S[8][13][6] = gma0*S[2][13][6] + fak * (S[1][13][6] +S[2][13][4] );
                S[5][13][6] = gma1*S[2][13][6];
                S[6][13][6] = gma2*S[2][13][6] + fak * (3.0*S[2][10][6] +S[2][13][2] );
                S[8][13][7] = gma0*S[2][13][7] + fak * S[1][13][7];
                S[5][13][7] = gma1*S[2][13][7] + fak * S[2][13][4];
                S[6][13][7] = gma2*S[2][13][7] + fak * (3.0*S[2][10][7] +S[2][13][3] );
                S[8][13][8] = gma0*S[2][13][8] + fak * (S[1][13][8] +2.0*S[2][13][2] );
                S[5][13][8] = gma1*S[2][13][8];
                S[6][13][8] = gma2*S[2][13][8] + fak3* S[2][10][8];
                S[8][13][9] = gma0*S[2][13][9] + fak * S[1][13][9];
                S[5][13][9] = gma1*S[2][13][9] + fak2* S[2][13][3];
                S[6][13][9] = gma2*S[2][13][9] + fak3* S[2][10][9];
                S[8][13][10] = gma0*S[2][13][10] + fak * S[1][13][10];
                S[5][13][10] = gma1*S[2][13][10];
                S[6][13][10] = gma2*S[2][13][10] + fak * (3.0*S[2][10][10] +2.0*S[2][13][4] );
                S[8][14][5] = gma0*S[2][14][5] +  fak * (S[1][14][5] +2.0*S[2][5][5] +S[2][14][3] );
                S[5][14][5] = gma1*S[2][14][5] + fak * (S[2][8][5] +S[2][14][2] );
                S[6][14][5] = gma2*S[2][14][5];
                S[8][14][6] = gma0*S[2][14][6] + fak * (S[1][14][6] +2.0*S[2][5][6] +S[2][14][4] );
                S[5][14][6] = gma1*S[2][14][6] + fak * S[2][8][6];
                S[6][14][6] = gma2*S[2][14][6] + fak * S[2][14][2];
                S[8][14][7] = gma0*S[2][14][7] + fak * (S[1][14][7] +2.0*S[2][5][7] );
                S[5][14][7] = gma1*S[2][14][7] + fak * (S[2][8][7] +S[2][14][4] );
                S[6][14][7] = gma2*S[2][14][7] + fak * S[2][14][3];
                S[8][14][8] = gma0*S[2][14][8] + fak * (S[1][14][8] +2.0*S[2][5][8] +2.0*S[2][14][2] );
                S[5][14][8] = gma1*S[2][14][8] + fak * S[2][8][8];
                S[6][14][8] = gma2*S[2][14][8];
                S[8][14][9] = gma0*S[2][14][9] + fak * (S[1][14][9] +2.0*S[2][5][9] );
                S[5][14][9] = gma1*S[2][14][9] + fak * (S[2][8][9] +2.0*S[2][14][3] );
                S[6][14][9] = gma2*S[2][14][9];
                S[8][14][10] = gma0*S[2][14][10] + fak * (S[1][14][10]+2.0*S[2][5][10]);
                S[5][14][10] = gma1*S[2][14][10] + fak * S[2][8][10];
                S[6][14][10] = gma2*S[2][14][10] + fak2* S[2][14][4];
                S[8][15][5] = gma0*S[2][15][5] + fak *(S[1][15][5]+S[2][9][5]+S[2][15][3]);
                S[5][15][5] = gma1*S[2][15][5] + fak * (2.0*S[2][5][5] +S[2][15][2] );
                S[6][15][5] = gma2*S[2][15][5];
                S[8][15][6] = gma0*S[2][15][6] + fak *(S[1][15][6]+S[2][9][6]+S[2][15][4]);
                S[5][15][6] = gma1*S[2][15][6] + fak2* S[2][5][6];
                S[6][15][6] = gma2*S[2][15][6] + fak * S[2][15][2];
                S[8][15][7] = gma0*S[2][15][7] + fak * (S[1][15][7] +S[2][9][7] );
                S[5][15][7] = gma1*S[2][15][7] + fak * (2.0*S[2][5][7] +S[2][15][4] );
                S[6][15][7] = gma2*S[2][15][7] + fak * S[2][15][3];
                S[8][15][8] = gma0*S[2][15][8] + fak * (S[1][15][8] +S[2][9][8] +2.0*S[2][15][2] );
                S[5][15][8] = gma1*S[2][15][8] + fak2* S[2][5][8];
                S[6][15][8] = gma2*S[2][15][8];
                S[8][15][9] = gma0*S[2][15][9] + fak * (S[1][15][9] +S[2][9][9] );
                S[5][15][9] = gma1*S[2][15][9] + fak2* (S[2][5][9] +S[2][15][3] );
                S[6][15][9] = gma2*S[2][15][9];
                S[8][15][10] = gma0*S[2][15][10] + fak * (S[1][15][10] +S[2][9][10] );
                S[5][15][10] = gma1*S[2][15][10] + fak2* S[2][5][10];
                S[6][15][10] = gma2*S[2][15][10] + fak2* S[2][15][4];
                S[8][16][5] = gma0*S[2][16][5] + fak * (S[1][16][5] +2.0*S[2][6][5] +S[2][16][3] );
                S[5][16][5] = gma1*S[2][16][5] + fak * S[2][16][2];
                S[6][16][5] = gma2*S[2][16][5] + fak * S[2][8][5];
                S[8][16][6] = gma0*S[2][16][6] + fak * (S[1][16][6] +2.0*S[2][6][6] +S[2][16][4] );
                S[5][16][6] = gma1*S[2][16][6];
                S[6][16][6] = gma2*S[2][16][6] + fak * (S[2][8][6] +S[2][16][2] );
                S[8][16][7] = gma0*S[2][16][7] + fak * (S[1][16][7] +2.0*S[2][6][7] );
                S[5][16][7] = gma1*S[2][16][7] + fak * S[2][16][4];
                S[6][16][7] = gma2*S[2][16][7] + fak * (S[2][8][7] +S[2][16][3] );
                S[8][16][8] = gma0*S[2][16][8] + fak * (S[1][16][8] +2.0*S[2][6][8] +2.0*S[2][16][2] );
                S[5][16][8] = gma1*S[2][16][8];
                S[6][16][8] = gma2*S[2][16][8] + fak * S[2][8][8];
                S[8][16][9] = gma0*S[2][16][9] + fak * (S[1][16][9] +2.0*S[2][6][9] );
                S[5][16][9] = gma1*S[2][16][9] + fak2* S[2][16][3];
                S[6][16][9] = gma2*S[2][16][9] + fak * S[2][8][9];
                S[8][16][10] = gma0*S[2][16][10] + fak * (S[1][16][10]+2.0*S[2][6][10]);
                S[5][16][10] = gma1*S[2][16][10];
                S[6][16][10] = gma2*S[2][16][10] + fak * (S[2][8][10] +2.0*S[2][16][4]);
                S[8][17][5] = gma0*S[2][17][5] + fak*(S[1][17][5]+S[2][10][5]+S[2][17][3]);
                S[5][17][5] = gma1*S[2][17][5] + fak * S[2][17][2];
                S[6][17][5] = gma2*S[2][17][5] + fak2* S[2][6][5];
                S[8][17][6] = gma0*S[2][17][6] + fak*(S[1][17][6]+S[2][10][6]+S[2][17][4]);
                S[5][17][6] = gma1*S[2][17][6];
                S[6][17][6] = gma2*S[2][17][6] + fak * (2.0*S[2][6][6] +S[2][17][2] );
                S[8][17][7] = gma0*S[2][17][7] + fak * (S[1][17][7] +S[2][10][7] );
                S[5][17][7] = gma1*S[2][17][7] + fak * S[2][17][4];
                S[6][17][7] = gma2*S[2][17][7] + fak * (2.0*S[2][6][7] +S[2][17][3] );
                S[8][17][8] = gma0*S[2][17][8] + fak * (S[1][17][8] +S[2][10][8] +2.0*S[2][17][2] );
                S[5][17][8] = gma1*S[2][17][8];
                S[6][17][8] = gma2*S[2][17][8] + fak2* S[2][6][8];
                S[8][17][9] = gma0*S[2][17][9] + fak * (S[1][17][9] +S[2][10][9] );
                S[5][17][9] = gma1*S[2][17][9] + fak2* S[2][17][3];
                S[6][17][9] = gma2*S[2][17][9] + fak2* S[2][6][9];
                S[8][17][10] = gma0*S[2][17][10] + fak * (S[1][17][10] +S[2][10][10] );
                S[5][17][10] = gma1*S[2][17][10];
                S[6][17][10] = gma2*S[2][17][10] + fak2* (S[2][6][10] +S[2][17][4] );
                S[8][18][5] = gma0*S[2][18][5] + fak * (S[1][18][5] +S[2][18][3] );
                S[5][18][5] = gma1*S[2][18][5] + fak * (2.0*S[2][7][5] +S[2][18][2] );
                S[6][18][5] = gma2*S[2][18][5] + fak * S[2][9][5];
                S[8][18][6] = gma0*S[2][18][6] + fak * (S[1][18][6] +S[2][18][4] );
                S[5][18][6] = gma1*S[2][18][6] + fak2* S[2][7][6];
                S[6][18][6] = gma2*S[2][18][6] + fak * (S[2][9][6] +S[2][18][2] );
                S[8][18][7] = gma0*S[2][18][7] + fak * S[1][18][7];
                S[5][18][7] = gma1*S[2][18][7] + fak * (2.0*S[2][7][7] +S[2][18][4] );
                S[6][18][7] = gma2*S[2][18][7] + fak * (S[2][9][7] +S[2][18][3] );
                S[8][18][8] = gma0*S[2][18][8] + fak * (S[1][18][8] +2.0*S[2][18][2] );
                S[5][18][8] = gma1*S[2][18][8] + fak2* S[2][7][8];
                S[6][18][8] = gma2*S[2][18][8] + fak * S[2][9][8];
                S[8][18][9] = gma0*S[2][18][9] + fak * S[1][18][9];
                S[5][18][9] = gma1*S[2][18][9] + fak2* (S[2][7][9] +S[2][18][3] );
                S[6][18][9] = gma2*S[2][18][9] + fak * S[2][9][9];
                S[8][18][10] = gma0*S[2][18][10] + fak * S[1][18][10];
                S[5][18][10] = gma1*S[2][18][10] + fak2* S[2][7][10];
                S[6][18][10] = gma2*S[2][18][10] + fak * (S[2][9][10] +2.0*S[2][18][4]);
                S[8][19][5] = gma0*S[2][19][5] + fak * (S[1][19][5] +S[2][19][3] );
                S[5][19][5] = gma1*S[2][19][5] + fak * (S[2][10][5] +S[2][19][2] );
                S[6][19][5] = gma2*S[2][19][5] + fak2* S[2][7][5];
                S[8][19][6] = gma0*S[2][19][6] + fak * (S[1][19][6] +S[2][19][4] );
                S[5][19][6] = gma1*S[2][19][6] + fak * S[2][10][6];
                S[6][19][6] = gma2*S[2][19][6] + fak * (2.0*S[2][7][6] +S[2][19][2] );
                S[8][19][7] = gma0*S[2][19][7] + fak * S[1][19][7];
                S[5][19][7] = gma1*S[2][19][7] + fak * (S[2][10][7] +S[2][19][4] );
                S[6][19][7] = gma2*S[2][19][7] + fak * (2.0*S[2][7][7] +S[2][19][3] );
                S[8][19][8] = gma0*S[2][19][8] + fak * (S[1][19][8] +2.0*S[2][19][2] );
                S[5][19][8] = gma1*S[2][19][8] + fak * S[2][10][8];
                S[6][19][8] = gma2*S[2][19][8] + fak2* S[2][7][8];
                S[8][19][9] = gma0*S[2][19][9] + fak * S[1][19][9];
                S[5][19][9] = gma1*S[2][19][9] + fak * (S[2][10][9] +2.0*S[2][19][3] );
                S[6][19][9] = gma2*S[2][19][9] + fak2* S[2][7][9];
                S[8][19][10] = gma0*S[2][19][10] + fak * S[1][19][10];
                S[5][19][10] = gma1*S[2][19][10] + fak * S[2][10][10];
                S[6][19][10] = gma2*S[2][19][10] + fak2* (S[2][7][10] +S[2][19][4] );
                S[8][20][5] = gma0*S[2][20][5] + fak *(S[1][20][5]+S[2][7][5]+S[2][20][3]);
                S[5][20][5] = gma1*S[2][20][5] + fak * (S[2][6][5] +S[2][20][2] );
                S[6][20][5] = gma2*S[2][20][5] + fak * S[2][5][5];
                S[8][20][6] = gma0*S[2][20][6] + fak *(S[1][20][6]+S[2][7][6]+S[2][20][4]);
                S[5][20][6] = gma1*S[2][20][6] + fak * S[2][6][6];
                S[6][20][6] = gma2*S[2][20][6] + fak * (S[2][5][6] +S[2][20][2] );
                S[8][20][7] = gma0*S[2][20][7] + fak * (S[1][20][7] +S[2][7][7] );
                S[5][20][7] = gma1*S[2][20][7] + fak * (S[2][6][7] +S[2][20][4] );
                S[6][20][7] = gma2*S[2][20][7] + fak * (S[2][5][7] +S[2][20][3] );
                S[8][20][8] = gma0*S[2][20][8] + fak * (S[1][20][8] +S[2][7][8] +2.0*S[2][20][2] );
                S[5][20][8] = gma1*S[2][20][8] + fak * S[2][6][8];
                S[6][20][8] = gma2*S[2][20][8] + fak * S[2][5][8];
                S[8][20][9] = gma0*S[2][20][9] + fak * (S[1][20][9] +S[2][7][9] );
                S[5][20][9] = gma1*S[2][20][9] + fak * (S[2][6][9] +2.0*S[2][20][3] );
                S[6][20][9] = gma2*S[2][20][9] + fak * S[2][5][9];
                S[8][20][10] = gma0*S[2][20][10] + fak * (S[1][20][10] +S[2][7][10] );
                S[5][20][10] = gma1*S[2][20][10] + fak * S[2][6][10];
                S[6][20][10] = gma2*S[2][20][10] + fak * (S[2][5][10] +2.0*S[2][20][4]);
                S[9][11][5] = gma1*S[3][11][5] + fak * (S[1][11][5] +S[3][11][2] );
                S[7][11][5] = gma2*S[3][11][5];
                S[9][11][6] = gma1*S[3][11][6] + fak * S[1][11][6];
                S[7][11][6] = gma2*S[3][11][6] + fak * S[3][11][2];
                S[9][11][7] = gma1*S[3][11][7] + fak * (S[1][11][7] +S[3][11][4] );
                S[7][11][7] = gma2*S[3][11][7] + fak * S[3][11][3];
                S[9][11][8] = gma1*S[3][11][8] + fak * S[1][11][8];
                S[7][11][8] = gma2*S[3][11][8];
                S[9][11][9] = gma1*S[3][11][9] + fak * (S[1][11][9] +2.0*S[3][11][3] );
                S[7][11][9] = gma2*S[3][11][9];
                S[9][11][10] = gma1*S[3][11][10] + fak * S[1][11][10];
                S[7][11][10] = gma2*S[3][11][10] + fak2* S[3][11][4];
                S[9][12][5] = gma1*S[3][12][5]   + fak * (S[1][12][5] +3.0*S[3][9][5] +S[3][12][2] );
                S[7][12][5] = gma2*S[3][12][5];
                S[9][12][6] = gma1*S[3][12][6] + fak * (S[1][12][6] +3.0*S[3][9][6] );
                S[7][12][6] = gma2*S[3][12][6] + fak * S[3][12][2];
                S[9][12][7] = gma1*S[3][12][7] + fak * (S[1][12][7] +3.0*S[3][9][7] +S[3][12][4] );
                S[7][12][7] = gma2*S[3][12][7] + fak * S[3][12][3];
                S[9][12][8] = gma1*S[3][12][8] + fak * (S[1][12][8] +3.0*S[3][9][8] );
                S[7][12][8] = gma2*S[3][12][8];
                S[9][12][9] = gma1*S[3][12][9] + fak * (S[1][12][9] +3.0*S[3][9][9] +2.0*S[3][12][3] );
                S[7][12][9] = gma2*S[3][12][9];
                S[9][12][10] = gma1*S[3][12][10] + fak * (S[1][12][10]+3.0*S[3][9][10]);
                S[7][12][10] = gma2*S[3][12][10] + fak2* S[3][12][4];
                S[9][13][5] = gma1*S[3][13][5] + fak * (S[1][13][5] +S[3][13][2] );
                S[7][13][5] = gma2*S[3][13][5] + fak3* S[3][10][5];
                S[9][13][6] = gma1*S[3][13][6] + fak * S[1][13][6];
                S[7][13][6] = gma2*S[3][13][6] + fak * (3.0*S[3][10][6] +S[3][13][2] );
                S[9][13][7] = gma1*S[3][13][7] + fak * (S[1][13][7] +S[3][13][4] );
                S[7][13][7] = gma2*S[3][13][7] + fak * (3.0*S[3][10][7] +S[3][13][3] );
                S[9][13][8] = gma1*S[3][13][8] + fak * S[1][13][8];
                S[7][13][8] = gma2*S[3][13][8] + fak3* S[3][10][8];
                S[9][13][9] = gma1*S[3][13][9] + fak * (S[1][13][9] +2.0*S[3][13][3] );
                S[7][13][9] = gma2*S[3][13][9] + fak3* S[3][10][9];
                S[9][13][10] = gma1*S[3][13][10] + fak * S[1][13][10];
                S[7][13][10] = gma2*S[3][13][10] + fak * (3.0*S[3][10][10] +2.0*S[3][13][4] );
                S[9][14][5] = gma1*S[3][14][5] + fak *(S[1][14][5]+S[3][8][5]+S[3][14][2]);
                S[7][14][5] = gma2*S[3][14][5];
                S[9][14][6] = gma1*S[3][14][6] + fak * (S[1][14][6] +S[3][8][6] );
                S[7][14][6] = gma2*S[3][14][6] + fak * S[3][14][2];
                S[9][14][7] = gma1*S[3][14][7] + fak *(S[1][14][7]+S[3][8][7]+S[3][14][4]);
                S[7][14][7] = gma2*S[3][14][7] + fak * S[3][14][3];
                S[9][14][8] = gma1*S[3][14][8] + fak * (S[1][14][8] +S[3][8][8] );
                S[7][14][8] = gma2*S[3][14][8];
                S[9][14][9] = gma1*S[3][14][9] + fak * (S[1][14][9] +S[3][8][9] +2.0*S[3][14][3] );
                S[7][14][9] = gma2*S[3][14][9];
                S[9][14][10] = gma1*S[3][14][10] + fak * (S[1][14][10] +S[3][8][10] );
                S[7][14][10] = gma2*S[3][14][10] + fak2* S[3][14][4];
                S[9][15][5] = gma1*S[3][15][5] + fak * (S[1][15][5] +2.0*S[3][5][5] +S[3][15][2] );
                S[7][15][5] = gma2*S[3][15][5];
                S[9][15][6] = gma1*S[3][15][6] + fak * (S[1][15][6] +2.0*S[3][5][6] );
                S[7][15][6] = gma2*S[3][15][6] + fak * S[3][15][2];
                S[9][15][7] = gma1*S[3][15][7] + fak * (S[1][15][7] +2.0*S[3][5][7] +S[3][15][4] );
                S[7][15][7] = gma2*S[3][15][7] + fak * S[3][15][3];
                S[9][15][8] = gma1*S[3][15][8] + fak * (S[1][15][8] +2.0*S[3][5][8] );
                S[7][15][8] = gma2*S[3][15][8];
                S[9][15][9] = gma1*S[3][15][9] + fak * (S[1][15][9] +2.0*S[3][5][9] +2.0*S[3][15][3] );
                S[7][15][9] = gma2*S[3][15][9];
                S[9][15][10] = gma1*S[3][15][10] + fak * (S[1][15][10]+2.0*S[3][5][10]);
                S[7][15][10] = gma2*S[3][15][10] + fak2* S[3][15][4];
                S[9][16][5] = gma1*S[3][16][5] + fak * (S[1][16][5] +S[3][16][2] );
                S[7][16][5] = gma2*S[3][16][5] + fak * S[3][8][5];
                S[9][16][6] = gma1*S[3][16][6] + fak * S[1][16][6];
                S[7][16][6] = gma2*S[3][16][6] + fak * (S[3][8][6] +S[3][16][2] );
                S[9][16][7] = gma1*S[3][16][7] + fak * (S[1][16][7] +S[3][16][4] );
                S[7][16][7] = gma2*S[3][16][7] + fak * (S[3][8][7] +S[3][16][3] );
                S[9][16][8] = gma1*S[3][16][8] + fak * S[1][16][8];
                S[7][16][8] = gma2*S[3][16][8] + fak * S[3][8][8];
                S[9][16][9] = gma1*S[3][16][9] + fak * (S[1][16][9] +2.0*S[3][16][3] );
                S[7][16][9] = gma2*S[3][16][9] + fak * S[3][8][9];
                S[9][16][10] = gma1*S[3][16][10] + fak * S[1][16][10];
                S[7][16][10] = gma2*S[3][16][10] + fak * (S[3][8][10] +2.0*S[3][16][4]);
                S[9][17][5] = gma1*S[3][17][5] + fak * (S[1][17][5] +S[3][17][2] );
                S[7][17][5] = gma2*S[3][17][5] + fak2* S[3][6][5];
                S[9][17][6] = gma1*S[3][17][6] + fak * S[1][17][6];
                S[7][17][6] = gma2*S[3][17][6] + fak * (2.0*S[3][6][6] +S[3][17][2] );
                S[9][17][7] = gma1*S[3][17][7] + fak * (S[1][17][7] +S[3][17][4] );
                S[7][17][7] = gma2*S[3][17][7] + fak * (2.0*S[3][6][7] +S[3][17][3] );
                S[9][17][8] = gma1*S[3][17][8] + fak * S[1][17][8];
                S[7][17][8] = gma2*S[3][17][8] + fak2* S[3][6][8];
                S[9][17][9] = gma1*S[3][17][9] + fak * (S[1][17][9] +2.0*S[3][17][3] );
                S[7][17][9] = gma2*S[3][17][9] + fak2* S[3][6][9];
                S[9][17][10] = gma1*S[3][17][10] + fak * S[1][17][10];
                S[7][17][10] = gma2*S[3][17][10] + fak2* (S[3][6][10] +S[3][17][4] );
                S[9][18][5] = gma1*S[3][18][5]  + fak * (S[1][18][5] +2.0*S[3][7][5] +S[3][18][2] );
                S[7][18][5] = gma2*S[3][18][5] + fak * S[3][9][5];
                S[9][18][6] = gma1*S[3][18][6] + fak * (S[1][18][6] +2.0*S[3][7][6] );
                S[7][18][6] = gma2*S[3][18][6] + fak * (S[3][9][6] +S[3][18][2] );
                S[9][18][7] = gma1*S[3][18][7] + fak * (S[1][18][7] +2.0*S[3][7][7] +S[3][18][4] );
                S[7][18][7] = gma2*S[3][18][7] + fak * (S[3][9][7] +S[3][18][3] );
                S[9][18][8] = gma1*S[3][18][8] + fak * (S[1][18][8] +2.0*S[3][7][8] );
                S[7][18][8] = gma2*S[3][18][8] + fak * S[3][9][8];
                S[9][18][9] = gma1*S[3][18][9] + fak * (S[1][18][9] +2.0*S[3][7][9] +2.0*S[3][18][3] );
                S[7][18][9] = gma2*S[3][18][9] + fak * S[3][9][9];
                S[9][18][10] = gma1*S[3][18][10] + fak * (S[1][18][10]+2.0*S[3][7][10]);
                S[7][18][10] = gma2*S[3][18][10] + fak * (S[3][9][10] +2.0*S[3][18][4]);
                S[9][19][5] = gma1*S[3][19][5] + fak*(S[1][19][5]+S[3][10][5]+S[3][19][2]);
                S[7][19][5] = gma2*S[3][19][5] + fak2* S[3][7][5];
                S[9][19][6] = gma1*S[3][19][6] + fak * (S[1][19][6] +S[3][10][6] );
                S[7][19][6] = gma2*S[3][19][6] + fak * (2.0*S[3][7][6] +S[3][19][2] );
                S[9][19][7] = gma1*S[3][19][7] + fak*(S[1][19][7]+S[3][10][7]+S[3][19][4]);
                S[7][19][7] = gma2*S[3][19][7] + fak * (2.0*S[3][7][7] +S[3][19][3] );
                S[9][19][8] = gma1*S[3][19][8] + fak * (S[1][19][8] +S[3][10][8] );
                S[7][19][8] = gma2*S[3][19][8] + fak2* S[3][7][8];
                S[9][19][9] = gma1*S[3][19][9] + fak * (S[1][19][9] +S[3][10][9] +2.0*S[3][19][3] );
                S[7][19][9] = gma2*S[3][19][9] + fak2* S[3][7][9];
                S[9][19][10] = gma1*S[3][19][10] + fak * (S[1][19][10] +S[3][10][10] );
                S[7][19][10] = gma2*S[3][19][10] + fak2* (S[3][7][10] +S[3][19][4] );
                S[9][20][5] = gma1*S[3][20][5] + fak * (S[1][20][5] +S[3][6][5] + S[3][20][2] );
                S[7][20][5] = gma2*S[3][20][5] + fak * S[3][5][5];
                S[9][20][6] = gma1*S[3][20][6] + fak * (S[1][20][6] +S[3][6][6] );
                S[7][20][6] = gma2*S[3][20][6] + fak * (S[3][5][6] +S[3][20][2] );
                S[9][20][7] = gma1*S[3][20][7] + fak *(S[1][20][7]+S[3][6][7]+S[3][20][4]);
                S[7][20][7] = gma2*S[3][20][7] + fak * (S[3][5][7] +S[3][20][3] );
                S[9][20][8] = gma1*S[3][20][8] + fak * (S[1][20][8] +S[3][6][8] );
                S[7][20][8] = gma2*S[3][20][8] + fak * S[3][5][8];
                S[9][20][9] = gma1*S[3][20][9] + fak * (S[1][20][9] +S[3][6][9] +2.0*S[3][20][3] );
                S[7][20][9] = gma2*S[3][20][9] + fak * S[3][5][9];
                S[9][20][10] = gma1*S[3][20][10] + fak * (S[1][20][10] +S[3][6][10] );
                S[7][20][10] = gma2*S[3][20][10] + fak * (S[3][5][10] +2.0*S[3][20][4]);
                S[10][11][5] = gma2*S[4][11][5] + fak * S[1][11][5];
                S[10][11][6] = gma2*S[4][11][6] + fak * (S[1][11][6] +S[4][11][2] );
                S[10][11][7] = gma2*S[4][11][7] + fak * (S[1][11][7] +S[4][11][3] );
                S[10][11][8] = gma2*S[4][11][8] + fak * S[1][11][8];
                S[10][11][9] = gma2*S[4][11][9] + fak * S[1][11][9];
                S[10][11][10] = gma2*S[4][11][10] + fak *(S[1][11][10]+2.0*S[4][11][4]);
                S[10][12][5] = gma2*S[4][12][5] + fak * S[1][12][5];
                S[10][12][6] = gma2*S[4][12][6] + fak * (S[1][12][6] +S[4][12][2] );
                S[10][12][7] = gma2*S[4][12][7] + fak * (S[1][12][7] +S[4][12][3] );
                S[10][12][8] = gma2*S[4][12][8] + fak * S[1][12][8];
                S[10][12][9] = gma2*S[4][12][9] + fak * S[1][12][9];
                S[10][12][10] = gma2*S[4][12][10] + fak *(S[1][12][10]+2.0*S[4][12][4]);
                S[10][13][5] = gma2*S[4][13][5] + fak * (S[1][13][5] +3.0*S[4][10][5] );
                S[10][13][6] = gma2*S[4][13][6] + fak * (S[1][13][6] +3.0*S[4][10][6] +S[4][13][2] );
                S[10][13][7] = gma2*S[4][13][7] + fak * (S[1][13][7] +3.0*S[4][10][7] +S[4][13][3] );
                S[10][13][8] = gma2*S[4][13][8] + fak * (S[1][13][8] +3.0*S[4][10][8] );
                S[10][13][9] = gma2*S[4][13][9] + fak * (S[1][13][9] +3.0*S[4][10][9] );
                S[10][13][10] = gma2*S[4][13][10] + fak * (S[1][13][10] +3.0*S[4][10][10] +2.0*S[4][13][4] );
                S[10][14][5] = gma2*S[4][14][5] + fak * S[1][14][5];
                S[10][14][6] = gma2*S[4][14][6] + fak * (S[1][14][6] +S[4][14][2] );
                S[10][14][7] = gma2*S[4][14][7] + fak * (S[1][14][7] +S[4][14][3] );
                S[10][14][8] = gma2*S[4][14][8] + fak * S[1][14][8];
                S[10][14][9] = gma2*S[4][14][9] + fak * S[1][14][9];
                S[10][14][10] = gma2*S[4][14][10] + fak *(S[1][14][10]+2.0*S[4][14][4]);
                S[10][15][5] = gma2*S[4][15][5] + fak * S[1][15][5];
                S[10][15][6] = gma2*S[4][15][6] + fak * (S[1][15][6] +S[4][15][2] );
                S[10][15][7] = gma2*S[4][15][7] + fak * (S[1][15][7] +S[4][15][3] );
                S[10][15][8] = gma2*S[4][15][8] + fak * S[1][15][8];
                S[10][15][9] = gma2*S[4][15][9] + fak * S[1][15][9];
                S[10][15][10] = gma2*S[4][15][10] + fak *(S[1][15][10]+2.0*S[4][15][4]);
                S[10][16][5] = gma2*S[4][16][5] + fak * (S[1][16][5] +S[4][8][5] );
                S[10][16][6] = gma2*S[4][16][6] + fak*(S[1][16][6]+S[4][8][6]+S[4][16][2]);
                S[10][16][7] = gma2*S[4][16][7] + fak*(S[1][16][7]+S[4][8][7]+S[4][16][3]);
                S[10][16][8] = gma2*S[4][16][8] + fak * (S[1][16][8] +S[4][8][8] );
                S[10][16][9] = gma2*S[4][16][9] + fak * (S[1][16][9] +S[4][8][9] );
                S[10][16][10] = gma2*S[4][16][10] + fak * (S[1][16][10] +S[4][8][10] +2.0*S[4][16][4] );
                S[10][17][5] = gma2*S[4][17][5] + fak * (S[1][17][5] +2.0*S[4][6][5] );
                S[10][17][6] = gma2*S[4][17][6] + fak * (S[1][17][6] +2.0*S[4][6][6] +S[4][17][2] );
                S[10][17][7] = gma2*S[4][17][7] + fak * (S[1][17][7] +2.0*S[4][6][7] +S[4][17][3] );
                S[10][17][8] = gma2*S[4][17][8] + fak * (S[1][17][8] +2.0*S[4][6][8] );
                S[10][17][9] = gma2*S[4][17][9] + fak * (S[1][17][9] +2.0*S[4][6][9] );
                S[10][17][10] = gma2*S[4][17][10] + fak * (S[1][17][10] +2.0*S[4][6][10] +2.0*S[4][17][4] );
                S[10][18][5] = gma2*S[4][18][5] + fak * (S[1][18][5] +S[4][9][5] );
                S[10][18][6] = gma2*S[4][18][6] + fak*(S[1][18][6]+S[4][9][6]+S[4][18][2]);
                S[10][18][7] = gma2*S[4][18][7] + fak*(S[1][18][7]+S[4][9][7]+S[4][18][3]);
                S[10][18][8] = gma2*S[4][18][8] + fak * (S[1][18][8] +S[4][9][8] );
                S[10][18][9] = gma2*S[4][18][9] + fak * (S[1][18][9] +S[4][9][9] );
                S[10][18][10] = gma2*S[4][18][10] + fak * (S[1][18][10] +S[4][9][10] +2.0*S[4][18][4] );
                S[10][19][5] = gma2*S[4][19][5] + fak * (S[1][19][5] +2.0*S[4][7][5] );
                S[10][19][6] = gma2*S[4][19][6] + fak * (S[1][19][6] +2.0*S[4][7][6] +S[4][19][2] );
                S[10][19][7] = gma2*S[4][19][7] + fak * (S[1][19][7] +2.0*S[4][7][7] +S[4][19][3] );
                S[10][19][8] = gma2*S[4][19][8] + fak * (S[1][19][8] +2.0*S[4][7][8] );
                S[10][19][9] = gma2*S[4][19][9] + fak * (S[1][19][9] +2.0*S[4][7][9] );
                S[10][19][10] = gma2*S[4][19][10] + fak * (S[1][19][10] +2.0*S[4][7][10] +2.0*S[4][19][4] );
                S[10][20][5] = gma2*S[4][20][5] + fak * (S[1][20][5] +S[4][5][5] );
                S[10][20][6] = gma2*S[4][20][6] + fak*(S[1][20][6]+S[4][5][6]+S[4][20][2]);
                S[10][20][7] = gma2*S[4][20][7] + fak*(S[1][20][7]+S[4][5][7]+S[4][20][3]);
                S[10][20][8] = gma2*S[4][20][8] + fak * (S[1][20][8] +S[4][5][8] );
                S[10][20][9] = gma2*S[4][20][9] + fak * (S[1][20][9] +S[4][5][9] );
                S[10][20][10] = gma2*S[4][20][10] + fak * (S[1][20][10] +S[4][5][10] +2.0*S[4][20][4] );
             } 
            
            if ( _lmax_gw > 3) {
 
            FillThreeCenterOLBlock_g(S,gma,gmc,_lmax_alpha,_lmax_gamma,fak);
            
            
            }
            
            

            // data is now stored in unnormalized cartesian Gaussians in the multiarray
            // Now, weird-looking construction since multiarray is not accessible for ub::prod
            //              s px py pz dxz dyz dxy d3z2-r2 dx2-y2  f1  f2  f3  f4  f5  f6  f7  g1  g2  g3  g4  g5  g6  g7  g8  g9 
            int istart[] = {0, 1, 2, 3, 5,  6,  4,   7,     7,    12,  10, 11, 11, 10, 19, 15,  5, 25, 27, 23, 20, 25, 27, 23, 20}; //extend for g
            int istop[] =  {0, 1, 2, 3, 5,  6,  4,   9,     8,    17,  16, 18, 13, 14, 19, 17, 31, 33, 32 ,34, 30, 33, 32, 24, 31}; // extend for g

            // ub::vector<ub::matrix<double> >& _subvector
            // which ones do we want to store
            int _offset_gw = _shell_gw->getOffset();
            int _offset_alpha = _shell_alpha->getOffset();
            int _offset_gamma = _shell_gamma->getOffset();

            // prepare transformation matrices
            int _ntrafo_gw = _shell_gw->getNumFunc() + _offset_gw;
            int _ntrafo_alpha = _shell_alpha->getNumFunc() + _offset_alpha;
            int _ntrafo_gamma = _shell_gamma->getNumFunc() + _offset_gamma;

            ub::matrix<double> _trafo_gw = ub::zero_matrix<double>(_ntrafo_gw, _ngw);
            ub::matrix<double> _trafo_alpha = ub::zero_matrix<double>(_ntrafo_alpha, _nalpha);
            ub::matrix<double> _trafo_gamma = ub::zero_matrix<double>(_ntrafo_gamma, _ngamma);

            
            //std::vector<double> _contractions_alpha = (*italpha)->contraction;
            //std::vector<double> _contractions_gamma = (*itgamma)->contraction;
            //std::vector<double> _contractions_gw    = (*itgw)->contraction;
            
            // get transformation matrices
            this->getTrafo(_trafo_gw, _lmax_gw, _decay_gw, (*itgw)->contraction);
            this->getTrafo(_trafo_alpha, _lmax_alpha, _decay_alpha, (*italpha)->contraction);
            this->getTrafo(_trafo_gamma, _lmax_gamma, _decay_gamma, (*itgamma)->contraction);

            // transform from unnormalized cartesians to normalized sphericals
            // container with indices starting at zero
            //ma_type S_sph;
            //S_sph.resize(extents[range(_offset_alpha, _ntrafo_alpha) ][range(_offset_gw, _ntrafo_gw) ][range(_offset_gamma,_ntrafo_gamma )]);
            double S_sph;
            
            for (int _i_alpha = _offset_alpha; _i_alpha < _ntrafo_alpha; _i_alpha++) {
                int alpha=_i_alpha-_offset_alpha;
                for (int _i_gw =  _offset_gw; _i_gw < _ntrafo_gw; _i_gw++) {
                    int g_w=_i_gw-_offset_gw;
                    for (int _i_gamma = _offset_gamma; _i_gamma < _ntrafo_gamma; _i_gamma++) {

                        //S_sph[ _i_alpha ][  _i_gw ][ _i_gamma ] = 0.0;
                        S_sph=0.0;
                        
                        for (int _i_alpha_t = istart[ _i_alpha ]; _i_alpha_t <= istop[ _i_alpha ]; _i_alpha_t++) {
                            for (int _i_gw_t = istart[ _i_gw ]; _i_gw_t <= istop[ _i_gw ]; _i_gw_t++) {
                                for (int _i_gamma_t = istart[ _i_gamma ]; _i_gamma_t <= istop[ _i_gamma ]; _i_gamma_t++) {

                                    //S_sph[_i_alpha ][ _i_gw ][  _i_gamma ] += S[ _i_alpha_t + 1 ][ _i_gw_t + 1 ][ _i_gamma_t + 1]
                                    //        * _trafo_alpha(_i_alpha, _i_alpha_t) * _trafo_gw(_i_gw, _i_gw_t) * _trafo_gamma(_i_gamma, _i_gamma_t);
                                    S_sph+= S[ _i_alpha_t + 1 ][ _i_gw_t + 1 ][ _i_gamma_t + 1]
                                            * _trafo_alpha(_i_alpha, _i_alpha_t) * _trafo_gw(_i_gw, _i_gw_t) * _trafo_gamma(_i_gamma, _i_gamma_t);



                                }
                            }
                        }
                        int _i_index = _shell_gamma->getNumFunc() * g_w + _i_gamma-_offset_gamma;

                        _subvector(alpha, _i_index) += S_sph;//[ _i_alpha ][ _i_gw ][ _i_gamma ];
                        
                    }
                }
            }

          

                    }
                }
            }

            return _does_contribute;


        }

     

        void TCrawMatrix::getTrafo(ub::matrix<double>& _trafo,const int _lmax, const double& _decay,const std::vector<double>& contractions) {
        // s-functions
        _trafo(0,0) = contractions[0]; // s
        ///         0    1  2  3    4  5  6  7  8  9   10  11  12  13  14  15  16  17  18  19       20    21    22    23    24    25    26    27    28    29    30    31    32    33    34 
        ///         s,   x, y, z,   xy xz yz xx yy zz, xxy xyy xyz xxz xzz yyz yzz xxx yyy zzz,    xxxy, xxxz, xxyy, xxyz, xxzz, xyyy, xyyz, xyzz, xzzz, yyyz, yyzz, yzzz, xxxx, yyyy, zzzz,
        // p-functions
        if ( _lmax > 0 ){
            //cout << _trafo_row.size1() << ":" << _trafo_row.size2() << endl;
            double factor = 2.*sqrt(_decay)*contractions[1];
            _trafo(1,3) = factor;  // Y 1,0
            _trafo(2,2) = factor;  // Y 1,-1
            _trafo(3,1) = factor;  // Y 1,1
        }

         // d-functions
        if ( _lmax > 1 ) { // order of functions changed
          double factor = 2.*_decay*contractions[2];
          double factor_1 =  factor/sqrt(3.);
          _trafo(4,Cart::xx) = -factor_1;    // d3z2-r2 (dxx)
          _trafo(4,Cart::yy) = -factor_1;    // d3z2-r2 (dyy)  Y 2,0
          _trafo(4,Cart::zz) = 2.*factor_1;  // d3z2-r2 (dzz)

          _trafo(5,Cart::yz) = 2.*factor;     // dyz           Y 2,-1

          _trafo(6,Cart::xz) = 2.*factor;     // dxz           Y 2,1

          _trafo(7,Cart::xy) = 2.*factor;     // dxy           Y 2,-2

          _trafo(8,Cart::xx) = factor;       // dx2-y2 (dxx)   Y 2,2
          _trafo(8,Cart::yy) = -factor;      // dx2-y2 (dzz)
        }
       
        // f-functions
        if ( _lmax > 2 ) { // order of functions changed
          double factor = 2.*pow(_decay,1.5)*contractions[3];
          double factor_1 = factor*2./sqrt(15.);
          double factor_2 = factor*sqrt(2.)/sqrt(5.);
          double factor_3 = factor*sqrt(2.)/sqrt(3.);

          _trafo(9,Cart::xxz) = -3.*factor_1;        // f1 (f??) xxz 13
          _trafo(9,Cart::yyz) = -3.*factor_1;        // f1 (f??) yyz 15        Y 3,0
          _trafo(9,Cart::zzz) = 2.*factor_1;         // f1 (f??) zzz 19

          _trafo(10,Cart::xxy) = -factor_2;          // f3 xxy 10
          _trafo(10,Cart::yyy) = -factor_2;          // f3 yyy 18   Y 3,-1
          _trafo(10,Cart::yzz) = 4.*factor_2;        // f3 yzz 16

          _trafo(11,Cart::xxx) = -factor_2;          // f2 xxx 17
          _trafo(11,Cart::xyy) = -factor_2;          // f2 xyy 11   Y 3,1
          _trafo(11,Cart::xzz) = 4.*factor_2;        // f2 xzz 14

          _trafo(12,Cart::xyz) = 4.*factor;          // f6 xyz 12     Y 3,-2

          _trafo(13,Cart::xxz) = 2.*factor;          // f7 (f??)   xxz   13
          _trafo(13,Cart::yyz) = -2.*factor;         // f7 (f??)   yyz   15   Y 3,2

          _trafo(14,Cart::xxy) = 3.*factor_3;        // f4 xxy 10
          _trafo(14,Cart::yyy) = -factor_3;          // f4 yyy 18   Y 3,-3

          _trafo(15,Cart::xxx) = factor_3;           // f5 (f??) xxx 17
          _trafo(15,Cart::xyy) = -3.*factor_3;       // f5 (f??) xyy 11     Y 3,3
        }

        // g-functions
        if ( _lmax > 3 ) {
          double factor = 2./sqrt(3.)*_decay*_decay*contractions[4];
          double factor_1 = factor/sqrt(35.);
          double factor_2 = factor*4./sqrt(14.);
          double factor_3 = factor*2./sqrt(7.);
          double factor_4 = factor*2.*sqrt(2.);

          _trafo(16,Cart::xxxx) = 3.*factor_1;   /// Y 4,0
          _trafo(16,Cart::xxyy) = 6.*factor_1;
          _trafo(16,Cart::xxzz) = -24.*factor_1;
          _trafo(16,Cart::yyyy) = 3.*factor_1;
          _trafo(16,Cart::yyzz) = -24.*factor_1;
          _trafo(16,Cart::zzzz) = 8.*factor_1;

          _trafo(17,Cart::xxyz) = -3.*factor_2;  /// Y 4,-1
          _trafo(17,Cart::yyyz) = -3.*factor_2;
          _trafo(17,Cart::yzzz) = 4.*factor_2;

          _trafo(18,Cart::xxxz) = -3.*factor_2;  /// Y 4,1
          _trafo(18,Cart::xyyz) = -3.*factor_2;
          _trafo(18,Cart::xzzz) = 4.*factor_2;

          _trafo(19,Cart::xxxy) = -2.*factor_3;  /// Y 4,-2
          _trafo(19,Cart::xyyy) = -2.*factor_3;
          _trafo(19,Cart::xyzz) = 12.*factor_3;

          _trafo(20,Cart::xxxx) = -factor_3;     /// Y 4,2
          _trafo(20,Cart::xxzz) = 6.*factor_3;
          _trafo(20,Cart::yyyy) = factor_3;
          _trafo(20,Cart::yyzz) = -6.*factor_3;

          _trafo(21,Cart::xxyz) = 3.*factor_4;   /// Y 4,-3
          _trafo(21,Cart::yyyz) = -factor_4;

          _trafo(22,Cart::xxxz) = factor_4;      /// Y 4,3
          _trafo(22,Cart::xyyz) = -3.*factor_4;

          _trafo(23,Cart::xxxy) = 4.*factor;     /// Y 4,-4
          _trafo(23,Cart::xyyy) = -4.*factor;

          _trafo(24,Cart::xxxx) = factor;        /// Y 4,4
          _trafo(24,Cart::xxyy) = -6.*factor;
          _trafo(24,Cart::yyyy) = factor;
        }

        return;
        }
        

        int TCrawMatrix::getBlockSize(const int _lmax) {
            int _block_size=-1;
            if (_lmax == 0) {
                _block_size = 1;
            } // s
            else if (_lmax == 1) {
                _block_size = 4;
            } // p
            else if (_lmax == 2) {
                _block_size = 10;
            } // d
            else if (_lmax == 3) {
                _block_size = 20;
            } // f
            else if (_lmax == 4) {
                _block_size = 35;
            } // g
            else if (_lmax == 5) { ////
                _block_size = 56; /////
            } // h
            else if (_lmax == 6) { /////
                _block_size = 84; /////
            } // i
            else if (_lmax == 7) { /////
                _block_size = 120; /////
            } // j
            else if (_lmax == 8) { /////
                _block_size = 165; /////
            } // k
            else{
                throw runtime_error("lmax for getBlocksize not known.");
            }

            return _block_size;
        }


    }
}

