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
#include "tools/NeighborList.h"
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

//does not inherit from coordination base because nl is private
class CudaCoordination : public Colvar {
  bool pbc{true};
  bool serial{false};
  std::unique_ptr<NeighborList> nl;
  bool invalidateList{true};
  bool firsttime{true};
  SwitchingFunction switchingFunction;

public:
  explicit CudaCoordination(const ActionOptions&);
// active methods:
  static void registerKeywords( Keywords& keys );
  void prepare() override;
  void calculate() override;
};

PLUMED_REGISTER_ACTION(CudaCoordination,"CUDACOORDINATION")

void CudaCoordination::prepare() {
  if(nl->getStride()>0) {
    if(firsttime || (getStep()%nl->getStride()==0)) {
      requestAtoms(nl->getFullAtomList());
      invalidateList=true;
      firsttime=false;
    } else {
      requestAtoms(nl->getReducedAtomList());
      invalidateList=false;
      if(getExchangeStep()) error("Neighbor lists should be updated on exchange steps - choose a NL_STRIDE which divides the exchange stride!");
    }
    if(getExchangeStep()) firsttime=true;
  }
}
void CudaCoordination::registerKeywords( Keywords& keys ) {
  Colvar::registerKeywords(keys);
  keys.addFlag("SERIAL",false,"Perform the calculation in serial - for debug purpose");
  keys.addFlag("PAIR",false,"Pair only 1st element of the 1st group with 1st element in the second, etc");
  keys.addFlag("NLIST",false,"Use a neighbor list to speed up the calculation");
  keys.add("optional","NL_CUTOFF","The cutoff for the neighbor list");
  keys.add("optional","NL_STRIDE","The frequency with which we are updating the atoms in the neighbor list");
  keys.add("atoms","GROUPA","First list of atoms");
  keys.add("atoms","GROUPB","Second list of atoms (if empty, N*(N-1)/2 pairs in GROUPA are counted)");
  keys.add("compulsory","NN","6","The n parameter of the switching function ");
  keys.add("compulsory","MM","0","The m parameter of the switching function; 0 implies 2*NN");
  keys.add("compulsory","D_0","0.0","The d_0 parameter of the switching function");
  keys.add("compulsory","R_0","The r_0 parameter of the switching function");
}

//these constant will be used within the kernels
__constant__ double dmax_2;
__constant__ double invr0_2;
__constant__ double stretch;
__constant__ double shift;
__constant__ int nn;
__constant__ int mm;

__device__ double pcuda_fastpow(double base,int expo){
  if(expo<0) {
    expo=-expo;
    base=1.0/base;
  }
  double result = 1.0;
  while (expo) {
    if (expo & 1){
      result *= base;
    }
    expo >>= 1;
    base *= base;
  }
  return result;
}

__device__ double pcuda_Rational(double rdist,double&dfunc,int NN, int MM) {
  double result;
  if(2*NN==MM) {
// if 2*N==M, then (1.0-rdist^N)/(1.0-rdist^M) = 1.0/(1.0+rdist^N)
    double rNdist=pcuda_fastpow(rdist,NN-1);
    double iden=1.0/(1+rNdist*rdist);
    dfunc = -NN*rNdist*iden*iden;
    result = iden;
  } else {
    if(rdist>(1.-100.0*epsilon) && rdist<(1+100.0*epsilon)) {
      result=NN/MM;
      dfunc=0.5*NN*(NN-MM)/MM;
    } else {
      double rNdist=pcuda_fastpow(rdist,NN-1);
      double rMdist=pcuda_fastpow(rdist,MM-1);
      double num = 1.-rNdist*rdist;
      double iden = 1./(1.-rMdist*rdist);
      double func = num*iden;
      result = func;
      dfunc = ((-NN*rNdist*iden)+(func*(iden*MM)*rMdist));
    }
  }
  return result;
}

__global__ void getpcuda_Rational(double *rdists,double *dfunc,int NN, int MM,double*res) {
    const int i = threadIdx.x + blockIdx.x * blockDim.x;

    res[i]=pcuda_Rational(rdists[i],dfunc[i],NN,MM);
}

CudaCoordination::CudaCoordination(const ActionOptions&ao):
  PLUMED_COLVAR_INIT(ao)
  
{
parseFlag("SERIAL",serial);

  std::vector<AtomNumber> ga_lista,gb_lista;
  parseAtomList("GROUPA",ga_lista);
  parseAtomList("GROUPB",gb_lista);

  bool nopbc=!pbc;
  parseFlag("NOPBC",nopbc);
  pbc=!nopbc;

// pair stuff
  bool dopair=false;
  parseFlag("PAIR",dopair);

// neighbor list stuff
  bool doneigh=false;
  double nl_cut=0.0;
  int nl_st=0;
  parseFlag("NLIST",doneigh);
  if(doneigh) {
    parse("NL_CUTOFF",nl_cut);
    if(nl_cut<=0.0) error("NL_CUTOFF should be explicitly specified and positive");
    parse("NL_STRIDE",nl_st);
    if(nl_st<=0) error("NL_STRIDE should be explicitly specified and positive");
  }

  addValueWithDerivatives(); setNotPeriodic();
  if(gb_lista.size()>0) {
    if(doneigh)  
    {nl=Tools::make_unique<NeighborList>(ga_lista,gb_lista,serial,dopair,pbc,getPbc(),comm,nl_cut,nl_st);}
    else         
    {nl=Tools::make_unique<NeighborList>(ga_lista,gb_lista,serial,dopair,pbc,getPbc(),comm);}
  } else {
    if(doneigh)  
    {nl=Tools::make_unique<NeighborList>(ga_lista,serial,pbc,getPbc(),comm,nl_cut,nl_st);}
    else         
    {nl=Tools::make_unique<NeighborList>(ga_lista,serial,pbc,getPbc(),comm);}
  }

  requestAtoms(nl->getFullAtomList());

  log.printf("  between two groups of %u and %u atoms\n",static_cast<unsigned>(ga_lista.size()),static_cast<unsigned>(gb_lista.size()));
  log.printf("  first group:\n");
  for(unsigned int i=0; i<ga_lista.size(); ++i) {
    if ( (i+1) % 25 == 0 ) log.printf("  \n");
    log.printf("  %d", ga_lista[i].serial());
  }
  log.printf("  \n  second group:\n");
  for(unsigned int i=0; i<gb_lista.size(); ++i) {
    if ( (i+1) % 25 == 0 ) log.printf("  \n");
    log.printf("  %d", gb_lista[i].serial());
  }
  log.printf("  \n");
  if(pbc) log.printf("  using periodic boundary conditions\n");
  else    log.printf("  without periodic boundary conditions\n");
  if(dopair) log.printf("  with PAIR option\n");
  if(doneigh) {
    log.printf("  using neighbor lists with\n");
    log.printf("  update every %d steps and cutoff %f\n",nl_st,nl_cut);
  }
  std::string sw,errors;
  
  {
    int nn_=6;
    int mm_=0;
    double d0_=0.0;
    double r0_=0.0;
    parse("R_0",r0_);
    if(r0_<=0.0) {error("R_0 should be explicitly specified and positive");}
    parse("D_0",d0_);
    parse("NN",nn_);
    parse("MM",mm_);
    
    cudaMemcpyToSymbol(nn, &nn_, sizeof(int));
    cudaMemcpyToSymbol(mm, &mm_, sizeof(int));
    double dmax=d0_+r0_*std::pow(0.00001,1./(nn-mm));
    double inputs[2] = {0.0,dmax};
    double *inputsc,*dummy;
    double *sc;
    cudaMalloc(&inputsc, 2 *sizeof(double));
    cudaMalloc(&dummy, 2*sizeof(double));
    cudaMalloc(&sc, 2*sizeof(double));
    cudaMemcpy(inputsc, &inputs[0], 2* sizeof(double),
             cudaMemcpyHostToDevice);
    getpcuda_Rational<<<1,2>>>(inputsc,dummy,nn,mm,sc);
    double s[2] ;
    cudaMemcpy(s, &inputsc, 2* sizeof(double),
             cudaMemcpyDeviceToHost);
    cudaFree(inputsc);
    cudaFree(dummy);
    cudaFree(sc);
    

    double stretch_=1.0/(s[0]-s[1]);
    double shift_=-s[1]*stretch;
    cudaMemcpyToSymbol(stretch, &stretch_, sizeof(int));
    cudaMemcpyToSymbol(shift, &shift_, sizeof(int));
    dmax*=dmax;
    cudaMemcpyToSymbol(dmax_2, &dmax, sizeof(int));
  }

  checkRead();

  log<<"  contacts are counted with cutoff "<<switchingFunction.description()<<"\n";
}

__device__ double calculateSqr(double distancesq, double& dfunc){
    double result=0.0;
    dfunc=0.0;
    if(distancesq<dmax_2) {
      const double rdist_2 = distancesq*invr0_2;
      double result=pcuda_Rational(rdist_2,dfunc,nn/2,mm/2);
      // chain rule:
      dfunc*=2*invr0_2;
      // stretch:
      result=result*stretch+shift;
      dfunc*=stretch;
    }
    return result;
}

__global__ void getCoord(double *ncoord,double *coordinates, unsigned *pairList,
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

  double dsq=(dx * dx + dy * dy + dz * dz);
  double dfunc=0.;
  ncoord[i]= calculateSqr(dsq,dfunc);
  
}

void CudaCoordination::calculate(){
  Tensor virial;
  std::vector<Vector> deriv(getNumberOfAtoms());
  auto positions = getPositions();
  auto nat = positions.size();
  if(nl->getStride()>0 && invalidateList) {
    nl->update(getPositions());
  }
  auto pairList = nl->getClosePairs();
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
  auto r0 = switchingFunction.get_r0() ;
  auto ngroups=max(1ul,nextpw2/256);
  getCoord<<<ngroups,256>>> (ncoords,coords, cudaPairList,r0,nat);



 
  std::vector<double> coordsToSUM(nextpw2);
  cudaMemcpy(&coordsToSUM, ncoords, nextpw2*sizeof(double), cudaMemcpyDeviceToHost);
  double ncoord=std::accumulate(coordsToSUM.begin(),coordsToSUM.begin()+nat,0.0);
  
  for(unsigned i=0; i<deriv.size(); ++i) setAtomsDerivatives(i,deriv[i]);
  setValue           (ncoord);
  setBoxDerivatives  (virial);

}

} // namespace colvar
} // namespace PLMD