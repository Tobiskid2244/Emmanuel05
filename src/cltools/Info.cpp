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
#include "CLTool.h"
#include "CLToolRegister.h"
#include "tools/Tools.h"
#include "config/Config.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace std;

namespace PLMD {
namespace cltools{

//+PLUMEDOC TOOLS info
/*
This tool allows you to obtain information about your plumed version

You can specify the information you require using the following command line
arguments

\par Examples

The following command returns the root directory for your plumed distribution.
\verbatim
plumed info --root
\endverbatim

*/
//+ENDPLUMEDOC

class Info:
public CLTool
{
public:
  static void registerKeywords( Keywords& keys );
  Info(const CLToolOptions& co );
  int main(FILE* in, FILE*out,Communicator& pc);
  string description()const{
    return "provide informations about plumed";
  }
};

PLUMED_REGISTER_CLTOOL(Info,"info")

void Info::registerKeywords( Keywords& keys ){
  CLTool::registerKeywords( keys );
  keys.addFlag("--configuration",false,"prints the configuration file");
  keys.addFlag("--root",false,"print the location of the root directory for the plumed source");
  keys.addFlag("--user-doc",false,"print the location of user manual (html)");
  keys.addFlag("--developer-doc",false,"print the location of user manual (html)");
  keys.addFlag("--version",false,"print the version number");
}

Info::Info(const CLToolOptions& co ):
CLTool(co)
{
  inputdata=commandline;
}

int Info::main(FILE* in, FILE*out,Communicator& pc){

 bool printconfiguration; parseFlag("--configuration",printconfiguration);
 bool printroot; parseFlag("--root",printroot);
 bool printuserdoc; parseFlag("--user-doc",printuserdoc);
 bool printdeveloperdoc; parseFlag("--developer-doc",printdeveloperdoc);
 bool printversion; parseFlag("--version",printversion);
 if(printroot) fprintf(out,"%s\n",config::getPlumedRoot().c_str());
 if(printconfiguration) fprintf(out,"%s",config::getMakefile().c_str());
 std::string userdoc=config::getPlumedRoot()+"user-doc/html/index.html";
 std::string developerdoc=config::getPlumedRoot()+"developer-doc/html/index.html";
 if(printuserdoc) fprintf(out,"%s\n",userdoc.c_str());
 if(printdeveloperdoc) fprintf(out,"%s\n",developerdoc.c_str());
 std::string versionname="v2.0b0";
 if(printversion) fprintf(out,"%s\n",versionname.c_str());

 return 0;
}



}
}
