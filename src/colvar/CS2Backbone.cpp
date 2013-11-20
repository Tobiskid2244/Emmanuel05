/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2013 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed-code.org for more information.

   This file is part of plumed, version 2.0.

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
#ifdef __PLUMED_HAS_ALMOST

#include "Colvar.h"
#include "ActionRegister.h"
#include "core/PlumedMain.h"
#include "core/Atoms.h"
#include "tools/Communicator.h"

#include <string>
#include <cmath>
#include <cassert>

#include <almost/mdb.h>
#include <almost/pdb.h>
#include <almost/forcefield/const/camshift2.h>
#include <almost/io/formostream.h>

using namespace std;
using namespace Almost;

namespace PLMD{

//+PLUMEDOC COLVAR CS2BACKBONE 
/*
This collective variable calculates a scoring function based on the comparison of backcalculated and
experimental backbone chemical shifts for a protein (CA, CB, C', H, HA, N).

CamShift \cite Kohlhoff:2009us is employed to back calculate the chemical shifts that are then compared
with a set of experimental values to generate a score \cite Robustelli:2010dn \cite Granata:2013dk.

It is also possible to backcalculate the chemical shifts from multiple replicas and then average them
to perform Replica-Averaged Restrained MD simulations \cite Camilloni:2012je \cite Camilloni:2013hs.

HOW TO COMPILE IT

In general the system for which chemical shifts are to be calculated must be completly included in
ATOMS. It should also be made WHOLE before the the backcalculation. CamShift is included in the
free package ALMOST v.2.1 that can be dowload via SVN (svn checkout svn://svn.code.sf.net/p/almost/code/ almost-code).
ALMOST 2.1 can be found in branches/almost-2.1/ and it can be compiled:

\verbatim
./configure 
make
\endverbatim

Once the code is compiled you should see the ALMOST library libAlm.a in src/lib/

PLUMED 2 must then be compiled by linking ALMOST: 

\verbatim
in DYNAMIC_LIBS the following paths should be added: 

-L/ALMOST_BASE_PATH/branches/almost-2.1/src/lib -lAlm \
-L/ALMOST_BASE_PATH/branches/almost-2.1/lib/sqlite-3.6.23.1 -lsqlite3 -lz -lbz2 \
-L/ALMOST_BASE_PATH/branches/almost-2.1/src/forcefield -lnbimpl \
-L/ALMOST_BASE_PATH/branches/almost-2.1/src/lib/modules -lshx

and in CPPFLAGS

-I/ALMOST_BASE_PATH/almost/branches/almost-2.1/include \
-I/ALMOST_BASE_PATH/almost/branches/almost-2.1/include/almost \
-I/ALMOST_BASE_PATH/almost/branches/almost-2.1/lib/sqlite-3.6.23.1 -D__PLUMED_HAS_ALMOST

with ALMOST_BASE_PATH the full path to the ALMOST folder
\endverbatim

HOW TO USE IT

To use CamShift as a restraint or as collective variable or as a replica-averaged restraint 
first of all the experimental data are needed. CamShift uses backbone and Cb chemical shifts 
that must be provided as text files:

\verbatim
CAshifts.dat:
CBshifts.dat:
Cshifts.dat:
Hshifts.dat:
HAshifts.dat:
Nshifts.dat:
#1 0.0
2 55.5
3 58.4
.
.
#last 0.0
#last+1 (first) of second chain
.
#last of second chain
\endverbatim

All of them must always be there. If a chemical shift for an atom of a residue is not available 0.0 must be used. 
So if for example all the Cb are not available all the chemical shifts for all the residues should be set as 0.0.

A template.pdb file is needed to the generate a topology of the protein within ALMOST. For histidines in protonation 
states different from D the HIE/HIP name should be used in the template.pdb. GLH and ASH can be used for the alternative 
protonation of GLU and ASP. Non-standard amino acids and other molecules are not yet supported! If multiple chains are 
present the chain identifier must be in the standard PDB format, together with the TER keyword at the end of each chain.
Residues numbering should always go from 1 to N in both the chemical shifts files as well as in the template.pdb file.
Two more keywords can be used to setup the topology: CYS-DISU to tell ALMOST to look for disulphide bridges and TERMINI
to define the protonation state of the the chain's termini (currently only DEFAULT (NH3+, CO2-) and NONE (NH, CO)).

Two more standard files are also needed: an ALMOST force-field file, corresponding to the force-field family used in
the MD code, (a03_cs2_gromacs.mdb for the amber family or all22_gromacs.mdb for the charmm family) and camshift.db, 
both these files can be copied from almost/branches/almost-2.1/toppar.

All the above files must be in a single folder that must be specified with the keyword DATA. 

\par Examples

case 1:

\verbatim
WHOLEMOLECULES ENTITY0=1-174
cs: CS2BACKBONE ATOMS=1-174 DATA=data/ FF=a03_gromacs.mdb FLAT=0.0 NRES=13 ENSEMBLE
cse: RESTRAINT ARG=cs SLOPE=24 KAPPA=0 AT=0.
PRINT ARG=cs,cse.bias
\endverbatim

case 2:

\verbatim
WHOLEMOLECULES ENTITY0=1-174
cs: CS2BACKBONE ATOMS=1-174 DATA=data/ FF=a03_gromacs.mdb FLAT=1.0 NRES=13 TERMINI=DEFAULT,NONE CYS-DISU WRITE_CS=1000
PRINT ARG=cs
\endverbatim

(See also \ref WHOLEMOLECULES, \ref RESTRAINT and \ref PRINT)

*/
//+ENDPLUMEDOC

class CS2Backbone : public Colvar {
  vector<CamShift2> cam_list;
  Molecules molecules;
  int  numResidues;
  int  pperiod;
  int  ens_dim;
  bool ensemble;
  bool serial;
  double **sh;
  double ene_pl2alm;
  double len_pl2alm;
  double for_pl2alm;
public:
  CS2Backbone(const ActionOptions&);
  ~CS2Backbone();
  static void registerKeywords( Keywords& keys );
  virtual void calculate();
};

PLUMED_REGISTER_ACTION(CS2Backbone,"CS2BACKBONE")

void CS2Backbone::registerKeywords( Keywords& keys ){
  Colvar::registerKeywords( keys );
  keys.addFlag("SERIAL",false,"Perform the calculation in serial - for debug purpose.");
  keys.add("atoms","ATOMS","The atoms to be included in the calculatios, e.g. the whole protein.");
  keys.add("compulsory","DATA","data/","The folder with the experimental chemical shifts.");
  keys.add("compulsory","FF","a03_gromacs.mdb","The ALMOST force-field to map the atoms' names.");
  keys.add("compulsory","FLAT","1.0","Flat region in the scoring function.");
  keys.add("compulsory","NEIGH_FREQ","10","Period in step for neighbour list update.");
  keys.add("compulsory","WRITE_CS","0","Write the back-calculated chemical shifts every # steps.");
  keys.add("compulsory","NRES","Number of residues, corresponding to the number of chemical shifts.");
  keys.add("optional","TERMINI","Defines the protonation states of the chain-termini.");
  keys.addFlag("CYS-DISU",false,"Set to TRUE if your system has disulphide bridges.");  
  keys.addFlag("ENSEMBLE",false,"Set to TRUE if you want to average over multiple replicas.");  
  keys.remove("NOPBC");
}

CS2Backbone::CS2Backbone(const ActionOptions&ao):
PLUMED_COLVAR_INIT(ao)
{
  string stringadb;
  string stringamdb;
  string stringapdb;

  serial=false;
  parseFlag("SERIAL",serial);

  string stringa_data;
  parse("DATA",stringa_data);

  string stringa_forcefield;
  parse("FF",stringa_forcefield);

  bool disu=false;
  parseFlag("CYS-DISU",disu);

  double grains=1.0;
  parse("FLAT", grains);

  int neigh_f=10;
  parse("NEIGH_FREQ", neigh_f);

  int w_period=0;
  parse("WRITE_CS", w_period);
  pperiod=w_period;

  parse("NRES", numResidues);

  ensemble=false;
  parseFlag("ENSEMBLE",ensemble);
  if(ensemble&&comm.Get_rank()==0) {
    if(multi_sim_comm.Get_size()<2) plumed_merror("You CANNOT run Replica-Averaged simulations without running multiple replicas!\n");
    else ens_dim=multi_sim_comm.Get_size(); 
  } else ens_dim=0; 
  if(ensemble) comm.Sum(&ens_dim, 1);

  stringadb  = stringa_data + string("/camshift.db");
  stringamdb = stringa_data + string("/") + stringa_forcefield;
  stringapdb = stringa_data + string("/template.pdb");

  log.printf("  loading force-field %s\n", stringamdb.c_str()); log.flush();
  Almost::MDB mdb((char*)stringamdb.c_str());
  log.printf("  loading template %s\n", stringapdb.c_str()); log.flush();
  Almost::PDB pdb((char*)stringapdb.c_str());

  vector<string> termini;
  string stringa_mol;
  parse("TERMINI",stringa_mol);
  if(stringa_mol.length()>0) {
    int num_chains = pdb[0].size();
    vector<string> data=Tools::getWords(stringa_mol,",");
    if(data.size()!=2*num_chains) plumed_merror("You have to define both the NTerm and the CTerm for each chain of your system!\n");
    for(unsigned i=0;i<data.size();i++) termini.push_back(data[i]);
  } else {
    int num_chains = pdb[0].size();
    for(unsigned i=0;i<(2*num_chains);i++) termini.push_back("DEFAULT");
  }

  log.printf("  building molecule ..."); log.flush();
  for(unsigned i=0;i<pdb[0].size();i++){
    unsigned j=2*i;
    string str;
    str +='A'+i;
    Protein p(str);
    p.build_missing(pdb[0][i],mdb,termini[j],termini[j+1]);
    if(disu) p.auto_disu_bonds(2.9,mdb);
    molecules.add_protein(p);
  }
  log.printf(" done!\n"); log.flush(); 
  log.printf("  Writing converted template.pdb ...\n"); log.flush();
  mol2pdb(molecules,"converted-template.pdb");

  log.printf("  Initialization of the predictor ...\n"); log.flush();
  CamShift2 a = CamShift2(molecules, stringadb);

  log.printf("  Reading experimental data ...\n"); log.flush();
  stringadb = stringa_data + string("/CAshifts.dat");
  log.printf("  Initializing CA shifts %s\n", stringadb.c_str()); log.flush();
  a.read_cs(stringadb, "CA");
  stringadb = stringa_data + string("/CBshifts.dat");
  log.printf("  Initializing CB shifts %s\n", stringadb.c_str()); log.flush();
  a.read_cs(stringadb, "CB");
  stringadb = stringa_data + string("/Cshifts.dat");
  log.printf("  Initializing C' shifts %s\n", stringadb.c_str()); log.flush();
  a.read_cs(stringadb, "C");
  stringadb = stringa_data + string("/HAshifts.dat");
  log.printf("  Initializing HA shifts %s\n", stringadb.c_str()); log.flush();
  a.read_cs(stringadb, "HA");
  stringadb = stringa_data + string("/Hshifts.dat");
  log.printf("  Initializing H shifts %s\n", stringadb.c_str()); log.flush();
  a.read_cs(stringadb, "H");
  stringadb = stringa_data + string("/Nshifts.dat");
  log.printf("  Initializing N shifts %s\n", stringadb.c_str()); log.flush();
  a.read_cs(stringadb, "N");
  /* this is a workaround for those chemical shifts that can result in too large forces */
  a.remove_problematic("GLN","CB");
  a.remove_problematic("ILE","CB");
  /* this is a workaround for those chemical shifts that are not parameterized */
  a.remove_problematic("HIE", "HA"); a.remove_problematic("HIP", "HA"); a.remove_problematic("HSP", "HA");
  a.remove_problematic("HIE", "H");  a.remove_problematic("HIP", "H");  a.remove_problematic("HSP", "H"); 
  a.remove_problematic("HIE", "N");  a.remove_problematic("HIP", "N");  a.remove_problematic("HSP", "N"); 
  a.remove_problematic("HIE", "CA"); a.remove_problematic("HIP", "CA"); a.remove_problematic("HSP", "CA");
  a.remove_problematic("HIE", "CB"); a.remove_problematic("HIP", "CB"); a.remove_problematic("HSP", "CB");
  a.remove_problematic("HIE", "C");  a.remove_problematic("HIP", "C");  a.remove_problematic("HSP", "C"); 
  a.remove_problematic("GLH", "HA"); a.remove_problematic("ASH", "HA"); a.remove_problematic("HSE", "HA");
  a.remove_problematic("GLH", "H");  a.remove_problematic("ASH", "H");  a.remove_problematic("HSE", "H");
  a.remove_problematic("GLH", "N");  a.remove_problematic("ASH", "N");  a.remove_problematic("HSE", "N");
  a.remove_problematic("GLH", "CA"); a.remove_problematic("ASH", "CA"); a.remove_problematic("HSE", "CA");
  a.remove_problematic("GLH", "CB"); a.remove_problematic("ASH", "CB"); a.remove_problematic("HSE", "CB");
  a.remove_problematic("GLH", "C");  a.remove_problematic("ASH", "C");  a.remove_problematic("HSE", "C");
  if(disu) { 
    a.remove_problematic("CYS", "HA");
    a.remove_problematic("CYS", "H");
    a.remove_problematic("CYS", "N");
    a.remove_problematic("CYS", "CA");
    a.remove_problematic("CYS", "CB");
    a.remove_problematic("CYS", "C");
  }
  /* done */

  log.printf("  Setting parameters ...\n"); log.flush();
  unsigned stride=comm.Get_size();
  unsigned rank=comm.Get_rank();
  if(serial) {stride=1; rank=0;}
  if(stride>1) log.printf("  Parallelized over %u processors\n", stride);
  a.set_mpi(stride, rank);
  
  if(ensemble) { log.printf("  ENSEMBLE averaging over %i replicas\n", ens_dim); }

  a.set_flat_bottom_const(grains);
  a.set_box_nupdate(neigh_f);
  a.set_lambda(1);
  cam_list.push_back(a);

  sh = new double*[numResidues];
  sh[0] = new double[numResidues*6];
  for (int i=1; i<numResidues; i++)  sh[i]=sh[i-1]+6; 

  /* Energy and Lenght conversion */
  ene_pl2alm = 4.186/plumed.getAtoms().getUnits().getEnergy();
  len_pl2alm = 10.00*plumed.getAtoms().getUnits().getLength();
  for_pl2alm = ene_pl2alm*len_pl2alm;
  log.printf("  Conversion table from plumed to Almost:\n");
  log.printf("    Energy %lf\n", ene_pl2alm);
  log.printf("    Length %lf\n", len_pl2alm);

  vector<AtomNumber> atoms;
  parseAtomList("ATOMS",atoms);
  checkRead();

  log<<"  Bibliography "
     <<plumed.cite("Kohlhoff K, Robustelli P, Cavalli A, Salvatella A, Vendruscolo M, J. Am. Chem. Soc. 131, 13894 (2009)")
     <<plumed.cite("Camilloni C, Robustelli P, De Simone A, Cavalli A, Vendruscolo M, J. Am. Chem. Soc. 134, 3968 (2012)") <<"\n";

  addValueWithDerivatives();
  setNotPeriodic();
  requestAtoms(atoms);
  log.printf("  DONE!\n"); log.flush();
}

CS2Backbone::~CS2Backbone()
{
  delete[] sh[0];
  delete[] sh;
}


void CS2Backbone::calculate()
{
  double energy=0.;
  Tensor virial;
  virial.zero();
  vector<Vector> deriv(getNumberOfAtoms());
  int N = getNumberOfAtoms();
  Coor<double> coor(N); 
  Coor<double> forces(N);

  forces.clear();
  for(int i=0; i<numResidues; i++) for(unsigned j=0; j<6; j++) sh[i][j]=0.;

  for (int i = 0; i < N; i++) {
     int ipos = 4 * i;
     Vector Pos = getPosition(i);
     coor.coor[ipos]   = len_pl2alm*Pos[0];
     coor.coor[ipos+1] = len_pl2alm*Pos[1];
     coor.coor[ipos+2] = len_pl2alm*Pos[2];
  }
  cam_list[0].ens_return_shifts(coor, sh);
  if(!serial) comm.Sum(&sh[0][0], numResidues*6);

  bool printout=false;
  if(pperiod>0&&comm.Get_rank()==0) printout = (!(getStep()%pperiod));
  if(printout) {
    string csfile;
    char tmps1[21], tmps2[21];
    // add to the name the label of the cv in such a way to have different files
    // when there is more than one defined variable
    sprintf(tmps1, "%li", getStep());
    if(ensemble) {
      sprintf(tmps2, "%i", multi_sim_comm.Get_rank());
      csfile = string("cs")+tmps2+"-"+tmps1+string(".dat");
    } else csfile = string("cs")+tmps1+string(".dat");
    cam_list[0].printout_chemical_shifts(csfile.c_str());
  }

  double fact=1.0;
  if(ensemble) {
    fact = 1./((double) ens_dim);
    if(comm.Get_rank()==0) { // I am the master of my replica
      // among replicas
      multi_sim_comm.Sum(&sh[0][0], numResidues*6);
      multi_sim_comm.Barrier(); 
      for(unsigned i=0;i<6;i++) for(int j=0;j<numResidues;j++) sh[j][i] *= fact; 
    } else for(unsigned i=0;i<6;i++) for(int j=0;j<numResidues;j++) sh[j][i] = 0.;
    // inside each replica
    comm.Sum(&sh[0][0], numResidues*6);
  }

  energy = cam_list[0].ens_energy_force(coor, forces, sh);
  if(!serial) comm.Sum(&forces[0][0], N*4);

  for (int i = 0; i < N; i++)
  {
    Vector For;
    int ipos = 4 * i;
    For[0] = forces.coor[ipos];
    For[1] = forces.coor[ipos+1];
    For[2] = forces.coor[ipos+2];
    deriv[i] = fact*for_pl2alm*For;
    virial=virial+(-1.*Tensor(getPosition(i),deriv[i]));
  }

  for(unsigned i=0;i<getNumberOfAtoms();++i) setAtomsDerivatives(i,deriv[i]);
  setValue           (ene_pl2alm*energy);
  setBoxDerivatives  (virial);
}

}
#endif
