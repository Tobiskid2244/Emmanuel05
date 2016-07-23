/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2016 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed.org for more information.

   This file is part of plumed, version 2.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#include "AlignedMatrixBase.h"
#include "core/ActionRegister.h"
#include "tools/KernelFunctions.h"
#include "tools/Torsion.h"
#include "tools/Matrix.h"

//+PLUMEDOC MATRIX SMAC_MATRIX 
/*
Adjacency matrix in which two molecules are adjacent if they are within a certain cutoff and if the angle between them is within certain ranges.

\par Examples

*/
//+ENDPLUMEDOC

namespace PLMD {
namespace adjmat {

class SMACMatrix : public AlignedMatrixBase {
private:
   Matrix<std::vector<KernelFunctions> > kernels;
public:
   /// 
   static void registerKeywords( Keywords& keys );
   ///
   explicit SMACMatrix(const ActionOptions&);
   void readOrientationConnector( const unsigned& i, const unsigned& j, const std::vector<std::string>& desc );
   double computeVectorFunction( const unsigned& iv, const unsigned& jv, 
                                 const Vector& conn, const std::vector<double>& vec1, const std::vector<double>& vec2,
                                 Vector& dconn, std::vector<double>& dvec1, std::vector<double>& dvec2 ) const ;
};

PLUMED_REGISTER_ACTION(SMACMatrix,"SMAC_MATRIX")

void SMACMatrix::registerKeywords( Keywords& keys ){
   AlignedMatrixBase::registerKeywords( keys );
   keys.add("numbered","KERNEL","The various kernels that are used to determine whether or not the molecules are aligned");
}

SMACMatrix::SMACMatrix( const ActionOptions& ao ):
Action(ao),
AlignedMatrixBase(ao)
{
   unsigned nrows, ncols, ig; retrieveTypeDimensions( nrows, ncols, ig );
   kernels.resize( nrows, ncols ); parseConnectionDescriptions("KERNEL",true,0);
}

void SMACMatrix::readOrientationConnector( const unsigned& iv, const unsigned& jv, const std::vector<std::string>& desc ){
   for(int i=0;i<desc.size();i++){
      KernelFunctions mykernel( desc[i] );
      kernels(iv,jv).push_back( mykernel );
      if( jv!=iv ) kernels(jv,iv).push_back( mykernel );
   }
   if( kernels(iv,jv).size()==0 ) error("no kernels defined");
}

double SMACMatrix::computeVectorFunction( const unsigned& iv, const unsigned& jv,
                                          const Vector& conn, const std::vector<double>& vec1, const std::vector<double>& vec2,
                                          Vector& dconn, std::vector<double>& dvec1, std::vector<double>& dvec2 ) const {

  unsigned nvectors = ( vec1.size() - 2 ) / 3; plumed_assert( (vec1.size()-2)%3==0 );
  std::vector<Vector> dv1(nvectors), dv2(nvectors), tdconn(nvectors); Torsion t; std::vector<Vector> v1(nvectors), v2(nvectors);
  std::vector<Value*> pos; for(unsigned i=0;i<nvectors;++i){ pos.push_back( new Value() ); pos[i]->setDomain( "-pi", "pi" ); }
   
  for(unsigned j=0;j<nvectors;++j){
      for(unsigned k=0;k<3;++k){
          v1[j][k]=vec1[2+3*j+k]; v2[j][k]=vec2[2+3*j+k];
      }
      double angle = t.compute( v1[j], conn, v2[j], dv1[j], tdconn[j], dv2[j] );
      pos[j]->set( angle );
  }
                                 
  double ans=0; std::vector<double> deriv( nvectors ), df( nvectors, 0 ); 
  for(unsigned i=0;i<kernels(iv,jv).size();++i){
      ans += kernels(iv,jv)[i].evaluate( pos, deriv );
      for(unsigned j=0;j<nvectors;++j) df[j] += deriv[j];
  }
  dconn.zero(); for(unsigned j=0;j<nvectors;++j) dconn += df[j]*tdconn[j];
  for(unsigned j=0;j<nvectors;++j){
      for(unsigned k=0;k<3;++k){ dvec1[2+3*j+k]=df[j]*dv1[j][k]; dvec2[2+3*j+k]=df[j]*dv2[j][k]; }
      delete pos[j];
  }
  return ans;
}

}
}



