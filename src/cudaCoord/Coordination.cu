/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011-2023 The plumed team
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
#include "colvar/CoordinationBase.h"
#include "tools/SwitchingFunction.h"
#include "core/ActionRegister.h"
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include <numeric>

namespace PLMD {
namespace colvar {

//+PLUMEDOC COLVAR CUDACOORDINATION
/*
Calculate coordination numbers. Like coordination, but on nvdia gpu and with no swithcing function

This keyword can be used to calculate the number of contacts between two groups of atoms
and is defined as
\f[
\sum_{i\in A} \sum_{i\in B} s_{ij}
\f]
where \f$s_{ij}\f$ is 1 if the contact between atoms \f$i\f$ and \f$j\f$ is formed,
zero otherwise.
In actuality, \f$s_{ij}\f$ is replaced with a switching function so as to ensure that the calculated CV has continuous derivatives.
The default switching function is:
\f[
s_{ij} = \frac{ 1 - \left(\frac{{\bf r}_{ij}-d_0}{r_0}\right)^n } { 1 - \left(\frac{{\bf r}_{ij}-d_0}{r_0}\right)^m }
\f]
but it can be changed using the optional SWITCH option.

To make your calculation faster you can use a neighbor list, which makes it that only a
relevant subset of the pairwise distance are calculated at every step.

If GROUPB is empty, it will sum the \f$\frac{N(N-1)}{2}\f$ pairs in GROUPA. This avoids computing
twice permuted indexes (e.g. pair (i,j) and (j,i)) thus running at twice the speed.

Notice that if there are common atoms between GROUPA and GROUPB the switching function should be
equal to one. These "self contacts" are discarded by plumed (since version 2.1),
so that they actually count as "zero".


\par Examples

The following example instructs plumed to calculate the total coordination number of the atoms in group 1-10 with the atoms in group 20-100.  For atoms 1-10 coordination numbers are calculated that count the number of atoms from the second group that are within 0.3 nm of the central atom.  A neighbor list is used to make this calculation faster, this neighbor list is updated every 100 steps.
\plumedfile
COORDINATION GROUPA=1-10 GROUPB=20-100 R_0=0.3 NLIST NL_CUTOFF=0.5 NL_STRIDE=100
\endplumedfile

The following is a dummy example which should compute the value 0 because the self interaction
of atom 1 is skipped. Notice that in plumed 2.0 "self interactions" were not skipped, and the
same calculation should return 1.
\plumedfile
c: COORDINATION GROUPA=1 GROUPB=1 R_0=0.3
PRINT ARG=c STRIDE=10
\endplumedfile

Here's an example that shows what happens when providing COORDINATION with
a single group:
\plumedfile
# define some huge group:
group: GROUP ATOMS=1-1000
# Here's coordination of a group against itself:
c1: COORDINATION GROUPA=group GROUPB=group R_0=0.3
# Here's coordination within a single group:
x: COORDINATION GROUPA=group R_0=0.3
# This is just multiplying times 2 the variable x:
c2: COMBINE ARG=x COEFFICIENTS=2 PERIODIC=NO

# the two variables c1 and c2 should be identical, but the calculation of c2 is twice faster
# since it runs on half of the pairs.
PRINT ARG=c1,c2 STRIDE=10
\endplumedfile



*/
//+ENDPLUMEDOC

class CudaCoordination : public CoordinationBase {
  SwitchingFunction switchingFunction;

public:
  explicit CudaCoordination(const ActionOptions&);
// active methods:
  static void registerKeywords( Keywords& keys );
  void calculate() override;
  double pairing(double distance,double&dfunc,unsigned i,unsigned j)const override;
};

PLUMED_REGISTER_ACTION(Coordination,"CUDACOORDINATION")

void CudaCoordination::registerKeywords( Keywords& keys ) {
  CoordinationBase::registerKeywords(keys);
  keys.add("compulsory","NN","6","The n parameter of the switching function ");
  keys.add("compulsory","MM","0","The m parameter of the switching function; 0 implies 2*NN");
  keys.add("compulsory","D_0","0.0","The d_0 parameter of the switching function");
  keys.add("compulsory","R_0","The r_0 parameter of the switching function");
  keys.add("optional","SWITCH","This keyword is used if you want to employ an alternative to the continuous switching function defined above. "
           "The following provides information on the \\ref switchingfunction that are available. "
           "When this keyword is present you no longer need the NN, MM, D_0 and R_0 keywords.");
}

CudaCoordination::CudaCoordination(const ActionOptions&ao):
  Action(ao),
  CoordinationBase(ao)
{

  std::string sw,errors;
  parse("SWITCH",sw);
  if(sw.length()>0) {
    switchingFunction.set(sw,errors);
    if( errors.length()!=0 ) error("problem reading SWITCH keyword : " + errors );
  } else {
    int nn=6;
    int mm=0;
    double d0=0.0;
    double r0=0.0;
    parse("R_0",r0);
    if(r0<=0.0) error("R_0 should be explicitly specified and positive");
    parse("D_0",d0);
    parse("NN",nn);
    parse("MM",mm);
    switchingFunction.set(nn,mm,r0,d0);
  }

  checkRead();

  log<<"  contacts are counted with cutoff "<<switchingFunction.description()<<"\n";
}

double CudaCoordination::pairing(double distance,double&/*dfunc*/,unsigned,unsigned )const {
  return distance;
}

__global__ void getCoord(double *ncoord,double *coordinates, unsigned *pairList
                         double Rsqr, unsigned nat) {
  //blockDIm are the number of threads in your block
  const int i = threadIdx.x + blockIdx.x * blockDim.x;
  unsigned i0= i*2;
  unsigned i1= i*2+1;
  ncoord[i] = 0.0;
  if (i0 == i1 || i >=nat) {
    return;
  }
  double dx = coordinates[3 * i0] - coordinates[3 * i1];
  double dy = coordinates[3 * i0 + 1] - coordinates[3 * i1 + 1];
  double dz = coordinates[3 * i0 + 2] - coordinates[3 * i1 + 2];
  if ((dx * dx + dy * dy + dz * dz) < Rsqr) {
    ncoord[i] = 1.0;
  }
}

void CudaCoordination::calculate(){
  Tensor virial;
  std::vector<Vector> deriv(getNumberOfAtoms());
  auto positions = getPositions();
  auto nat = positions.size();
  if(nl->getStride()>0 && invalidateList) {
    nl->update(getPositions());
  }
  auto pairList = nl->getClosePairs()
                  const unsigned nn=nl->size();
  //calculates the closest power of 2 (c++20 will have bit::bit_ceil(nn))
  size_t nextpw2 = pow(2, ceil(log2(nn)));
  pairList.resize(2*nextpw2);

  double *coords;
  double *ncoords;
  unsigned *cudaPairList;

  cudaMalloc(&coords, 3 * nat * sizeof(double));
  cudaMemcpy(coords, &positions[0][0], 3 *nat* sizeof(double),
             cudaMemcpyHostToDevice);

  cudaMalloc(&ncoords, nextpw2 * sizeof(double));

  cudaMalloc(&cudaPairList, 2*nextpw2 * sizeof(unsigned));
  cudaMemcpy(cudaPairList, &positions[0][0], 2*nextpw2* sizeof(unsigned),
             cudaMemcpyHostToDevice);
  //the occupancy MUST be set up correctly
  getCoord<<<max(1,nextpw2/256),256>>> getCoord(ncoords,coords, cudaPairList,R_0,nat);
  std::vector<double> coordsToSUM(nextpw2);
  cudaMemcpy(&coordsToSUM, ncoords, nextpw2*sizeof(double), cudaMemcpyDeviceToHost);
  double ncoord=std::accumulate(coordsToSUM.begin(),coordsToSUM.begin()+nat,0.0);
  //there are no pbcs
  /*
      std::vector<Vector> omp_deriv(getPositions().size());
      Tensor omp_virial;

      for(unsigned int i=rank; i<nn; i+=stride) {

        if(pbc) {
          distance=pbcDistance(getPosition(i0),getPosition(i1));
        } else {
          distance=delta(getPosition(i0),getPosition(i1));
        }

        double dfunc=0.;
        ncoord += pairing(distance.modulo2(), dfunc,i0,i1);

        Vector dd(dfunc*distance);
        Tensor vv(dd,distance);
        if(nt>1) {
          omp_deriv[i0]-=dd;
          omp_deriv[i1]+=dd;
          omp_virial-=vv;
        } else {
          deriv[i0]-=dd;
          deriv[i1]+=dd;
          virial-=vv;
        }

      }
      #pragma omp critical
      if(nt>1) {
        for(unsigned i=0; i<getPositions().size(); i++) deriv[i]+=omp_deriv[i];
        virial+=omp_virial;

    }

    if(!serial) {
      comm.Sum(ncoord);
      if(!deriv.empty()) comm.Sum(&deriv[0][0],3*deriv.size());
      comm.Sum(virial);
    }
  */
  for(unsigned i=0; i<deriv.size(); ++i) setAtomsDerivatives(i,deriv[i]);
  setValue           (ncoord);
  setBoxDerivatives  (virial);

}

} // namespace colvar
} // namespace PLMD
