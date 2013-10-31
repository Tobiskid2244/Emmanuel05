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
#include "Analysis.h"
#include "core/ActionRegister.h"
#include "tools/Grid.h"
#include "tools/KernelFunctions.h"
#include "tools/IFile.h"
#include "tools/OFile.h"

namespace PLMD{
namespace analysis{

//+PLUMEDOC ANALYSIS HISTOGRAM
/* 
Calculate the probability density as a function of a few CVs using kernel density estimation

\par Examples

The following input monitors two torsional angles during a simulation
and outputs a histogram as a function of them at the end of the simulation.
\verbatim
TORSION ATOMS=1,2,3,4 LABEL=r1
TORSION ATOMS=2,3,4,5 LABEL=r2
HISTOGRAM ...
  ARG=r1,r2 
  USE_ALL_DATA 
  GRID_MIN=-3.14,-3.14 
  GRID_MAX=3.14,3.14 
  GRID_BIN=200,200
  BANDWIDTH=0.05,0.05 
  GRID_WFILE=histo
... HISTOGRAM
\endverbatim

The following input monitors two torsional angles during a simulation
and outputs the histogram accumulated thus far every 100000 steps.
\verbatim
TORSION ATOMS=1,2,3,4 LABEL=r1
TORSION ATOMS=2,3,4,5 LABEL=r2
HISTOGRAM ...
  ARG=r1,r2 
  RUN=100000
  GRID_MIN=-3.14,-3.14  
  GRID_MAX=3.14,3.14 
  GRID_BIN=200,200
  BANDWIDTH=0.05,0.05 
  GRID_WFILE=histo
... HISTOGRAM
\endverbatim

The following input monitors two torsional angles during a simulation
and outputs a separate histogram for each 100000 steps worth of trajectory.
\verbatim
TORSION ATOMS=1,2,3,4 LABEL=r1
TORSION ATOMS=2,3,4,5 LABEL=r2
HISTOGRAM ...
  ARG=r1,r2 
  RUN=100000 NOMEMORY
  GRID_MIN=-3.14,-3.14  
  GRID_MAX=3.14,3.14 
  GRID_BIN=200,200
  BANDWIDTH=0.05,0.05 
  GRID_WFILE=histo
... HISTOGRAM
\endverbatim

*/
//+ENDPLUMEDOC

class Histogram : public Analysis {
private:
  std::vector<std::string> gmin, gmax; 
  std::vector<double> point, bw;
  std::vector<unsigned> gbin;
  std::string gridfname;
  std::string kerneltype; 
public:
  static void registerKeywords( Keywords& keys );
  Histogram(const ActionOptions&ao);
  void performAnalysis();
};

PLUMED_REGISTER_ACTION(Histogram,"HISTOGRAM")

void Histogram::registerKeywords( Keywords& keys ){
  Analysis::registerKeywords( keys );
  keys.add("compulsory","GRID_MIN","the lower bounds for the grid");
  keys.add("compulsory","GRID_MAX","the upper bounds for the grid");
  keys.add("compulsory","GRID_BIN","the number of bins for the grid");
  keys.add("compulsory","KERNEL","gaussian","the kernel function you are using. More details on the kernels available in plumed can be found in \\ref kernelfunctions.");
  keys.add("compulsory","BANDWIDTH","the bandwdith for kernel density estimation");
  keys.add("compulsory","GRID_WFILE","histogram","the file on which to write the grid");
  keys.use("NOMEMORY");
}

Histogram::Histogram(const ActionOptions&ao):
PLUMED_ANALYSIS_INIT(ao),
gmin(getNumberOfArguments()),
gmax(getNumberOfArguments()),
point(getNumberOfArguments()),
bw(getNumberOfArguments()),
gbin(getNumberOfArguments())
{
  // Read stuff for Grid
  parseVector("GRID_MIN",gmin);
  parseVector("GRID_MAX",gmax);
  parseVector("GRID_BIN",gbin);
  parseOutputFile("GRID_WFILE",gridfname); 

  // Read stuff for window functions
  parseVector("BANDWIDTH",bw);
  // Read the type of kernel we are using
  parse("KERNEL",kerneltype);
  checkRead();

  log.printf("  Using %s kernel functions\n",kerneltype.c_str() );
  log.printf("  Grid min");
  for(unsigned i=0;i<gmin.size();++i) log.printf(" %s",gmin[i].c_str() );
  log.printf("\n");
  log.printf("  Grid max");
  for(unsigned i=0;i<gmax.size();++i) log.printf(" %s",gmax[i].c_str() );
  log.printf("\n");
  log.printf("  Grid bin");
  for(unsigned i=0;i<gbin.size();++i) log.printf(" %d",gbin[i]);
  log.printf("\n");
}

void Histogram::performAnalysis(){
  // Back up old histogram files
//  std::string oldfname=saveResultsFromPreviousAnalyses( gridfname );

  // Get pbc stuff for grid
  std::vector<bool> pbc; std::string dmin,dmax;
  for(unsigned i=0;i<getNumberOfArguments();++i){
     pbc.push_back( getPeriodicityInformation(i,dmin,dmax) );
     if(pbc[i]){ Tools::convert(dmin,gmin[i]); Tools::convert(dmax,gmax[i]); }
  }

  Grid* gg; IFile oldf; oldf.link(*this); 
  if( usingMemory() && oldf.FileExist(gridfname) ){
      oldf.open(gridfname);
      gg = Grid::create( "probs", getArguments(), oldf, gmin, gmax, gbin, false, false, false );
      oldf.close();
  } else {
      gg = new Grid( "probs", getArguments(), gmin, gmax, gbin,false,false);
  }
  // Set output format for grid
  gg->setOutputFmt( getOutputFormat() );

  // Now build the histogram
  double weight; std::vector<double> point( getNumberOfArguments() );
  for(unsigned i=0;i<getNumberOfDataPoints();++i){
      getDataPoint( i, point, weight );
      KernelFunctions kernel( point, bw, kerneltype, false, weight, true);
      gg->addKernel( kernel ); 
       
  }
  // Normalize the histogram
  gg->scaleAllValuesAndDerivatives( 1.0 / getNormalization() );

  // Write the grid to a file
  OFile gridfile; gridfile.link(*this); gridfile.setBackupString("analysis");
  gridfile.open( gridfname ); gg->writeToFile( gridfile );
  // Close the file 
  gridfile.close(); delete gg;
}

}
}
