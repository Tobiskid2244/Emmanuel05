#include <iostream>

#include "plumed/wrapper/Plumed.h"
#include "plumed/tools/Communicator.h"

//When the user aks for a WALKERS_MPI in the METAD action, 
//if MPI is not installed then the user must be informed
//if MPI is installed then the comunications must be already set up

int main(int, char**){
    //GIVEN an initialized Plumed interface 
    PLMD::Plumed plumed;

    unsigned int natoms=10;

    std::vector<double> positions(3*natoms,0.0);
    for(unsigned i=0;i<3*natoms;i++) positions[i]=i;
    std::vector<double> masses(natoms,1.0);
    std::vector<double> forces(3*natoms,0.0);
    std::vector<double> box(9,0.0);
    std::vector<double> virial(9,0.0);
    plumed.cmd("setNatoms",&natoms);
    plumed.cmd("setLogFile","test.log");
    plumed.cmd("init");
    plumed.cmd("readInputLine","d: DISTANCE ATOMS=1,2");
    plumed.cmd("readInputLine","d1: DISTANCE ATOMS={1 2}");
   
    const std::string mokedLine=
    "METAD ARG=d,d1 SIGMA=0.1,0.2 HEIGHT=0.1 PACE=2 RESTART=YES WALKERS_MPI";
    
    //WHEN PLUMED is not compiled with MPI or the MPI routines are not initialized
    std::string expectedMessage="WALKERS_MPI flag requires MPI compilation";
    if (PLMD::Communicator::PlumedHasMPI()){
        expectedMessage="WALKERS_MPI needs the communicator correctly initialized";
    }
    try{
        //WHEN the user give ask for a METAD with WALKERS_MPI keyword in it
        plumed.cmd("readInputLine",mokedLine.c_str());
        //THEN plumed should gracefully exit with a clear error message 
    } catch(PLMD::Plumed::ExceptionError &e){//correct throw, we are happy
        std::string exceptionText{e.what()};
        
        if (exceptionText.find(expectedMessage) != std::string::npos) {
            //throws with the wanted message and we are happy
        return 0;
        }
        
        std::cout << "Exception thrown, wrong message: "
                  << e.what() << '\n';
        std::cout << "Expected message should contain: \""
                  << expectedMessage << "\"\n";
        return 1;
    }
    std::cout << "Exception not thrown\n";
    return 1;
}
