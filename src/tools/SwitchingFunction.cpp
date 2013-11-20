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
#include "SwitchingFunction.h"
#include "Tools.h"
#include "Keywords.h"
#include <vector>
#include <limits>

using namespace std;
namespace PLMD{

//+PLUMEDOC INTERNAL switchingfunction 
/*
Functions that measure whether values are less than a certain quantity.

Switching functions \f$s(r)\f$ take a minimum of one input parameter \f$d_0\f$.
For \f$r \le d_0 \quad s(r)=1.0\f$ while for \f$r > d_0\f$ the function decays smoothly to 0.
The various switching functions available in plumed differ in terms of how this decay is performed.

Where there is an accepted convention in the literature (e.g. \ref COORDINATION) on the form of the 
switching function we use the convention as the default.  However, the flexibility to use different
switching functions is always present generally through a single keyword. This keyword generally 
takes an input with the following form:

\verbatim
KEYWORD={TYPE <list of parameters>}
\endverbatim  

The following table contains a list of the various switching functions that are available in plumed 2
together with an example input.

<table align=center frame=void width=95%% cellpadding=5%%>
<tr> 
<td> TYPE </td> <td> FUNCTION </td> <td> EXAMPLE INPUT </td> <td> DEFAULT PARAMETERS </td>
</tr> <tr> <td>RATIONAL </td> <td>
\f$
s(r)=\frac{ 1 - \left(\frac{ r - d_0 }{ r_0 }\right)^{n} }{ 1 - \left(\frac{ r - d_0 }{ r_0 }\right)^{m} } 
\f$
</td> <td>
{RATIONAL R_0=\f$r_0\f$ D_0=\f$d_0\f$ NN=\f$n\f$ MM=\f$m\f$}
</td> <td> \f$d_0=0.0\f$, \f$n=6\f$, \f$m=12\f$ </td>
</tr> <tr>
<td> EXP </td> <td>
\f$
s(r)=\exp\left(-\frac{ r - d_0 }{ r_0 }\right)
\f$
</td> <td> 
{EXP  R_0=\f$r_0\f$ D_0=\f$d_0\f$}
</td> <td> \f$d_0=0.0\f$ </td>
</tr> <tr>
<td> GAUSSIAN </td> <td>
\f$
s(r)=\exp\left(-\frac{ (r - d_0)^2 }{ 2r_0^2 }\right)
\f$
</td> <td>
{GAUSSIAN R_0=\f$r_0\f$ D_0=\f$d_0\f$} 
</td> <td> \f$d_0=0.0\f$ </td>
</tr> <tr> 
<td> SMAP </td> <td>
\f$
s(r) = \left[ 1 + ( 2^{a/b} -1 )\left( \frac{r-d_0}{r_0} \right)\right]^{-b/a}
\f$
</td> <td>
{SMAP R_0=\f$r_0\f$ D_0=\f$d_0\f$ A=\f$a\f$ B=\f$b\f$}
</td> <td> \f$d_0=0.0\f$ </td>
</tr> 
</table>

For all the switching functions in the above table one can also specify a further (optional) parameter using the parameter
keyword D_MAX to assert that for \f$r>d_{\textrm{max}}\f$ the switching function can be assumed equal to zero. 
*/
//+ENDPLUMEDOC

void SwitchingFunction::registerKeywords( Keywords& keys ){
  keys.add("compulsory","R_0","the value of R_0 in the switching function");
  keys.add("compulsory","D_0","0.0","the value of D_0 in the switching function");
  keys.add("optional","D_MAX","the value at which the switching function can be assumed equal to zero");
  keys.add("compulsory","NN","6","the value of n in the switching function (only needed for TYPE=RATIONAL)");
  keys.add("compulsory","MM","12","the value of m in the switching function (only needed for TYPE=RATIONAL)");
  keys.add("compulsory","A","the value of a in the switching funciton (only needed for TYPE=SMAP)");
  keys.add("compulsory","B","the value of b in the switching funciton (only needed for TYPE=SMAP)"); 
}

void SwitchingFunction::set(const std::string & definition,std::string& errormsg){
  vector<string> data=Tools::getWords(definition);
  if( data.size()<1 ) errormsg="missing all input for switching function"; 
  string name=data[0];
  data.erase(data.begin());
  invr0=0.0;
  d0=0.0;
  dmax=std::numeric_limits<double>::max();
  init=true;

  double r0;
  bool found_r0=Tools::parse(data,"R_0",r0);
  if(!found_r0) errormsg="R_0 is required";
  invr0=1.0/r0;
  Tools::parse(data,"D_0",d0);
  Tools::parse(data,"D_MAX",dmax);

  if(name=="RATIONAL"){
    type=spline;
    nn=6;
    mm=12;
    Tools::parse(data,"NN",nn);
    Tools::parse(data,"MM",mm);
  } else if(name=="SMAP"){
    type=smap;
    Tools::parse(data,"A",a);
    Tools::parse(data,"B",b);
    c=pow(2., static_cast<double>(a)/static_cast<double>(b) ) - 1; 
    d = -static_cast<double>(b) / static_cast<double>(a);
  } else if(name=="EXP") type=exponential;
  else if(name=="GAUSSIAN") type=gaussian;
  else errormsg="cannot understand switching function type '"+name+"'";
  if( !data.empty() ){
      errormsg="found the following rogue keywords in switching function input : ";
      for(unsigned i=0;i<data.size();++i) errormsg = errormsg + data[i] + " "; 
  }
}

std::string SwitchingFunction::description() const {
  std::ostringstream ostr;
  ostr<<1./invr0<<".  Using ";
  if(type==spline){
     ostr<<"rational";
  } else if(type==exponential){
     ostr<<"exponential";
  } else if(type==gaussian){
     ostr<<"gaussian";
  } else if(type==smap){
     ostr<<"smap";
  } else{
     plumed_merror("Unknown switching function type");
  }
  ostr<<" swiching function with parameters d0="<<d0;
  if(type==spline){
    ostr<<" nn="<<nn<<" mm="<<mm;
  } else if(type==smap){
    ostr<<" a="<<a<<" b="<<b;
  }
  return ostr.str(); 
}

double SwitchingFunction::calculate(double distance,double&dfunc)const{
  plumed_massert(init,"you are trying to use an unset SwitchingFunction");
  if(distance>dmax){
    dfunc=0.0;
    return 0.0;
  }
  const double rdist = (distance-d0)*invr0;
  double result;
  if(rdist<=0.){
     result=1.;
     dfunc=0.0;
  }else{
    if(type==smap){
      double sx=c*pow( rdist, a ); 
      result=pow( 1.0 + sx, d ); 
      dfunc=-b*sx/rdist*result/(1.0+sx); 
    } else if(type==spline){
      if(2*nn==mm){
// if 2*N==M, then (1.0-rdist^N)/(1.0-rdist^M) = 1.0/(1.0+rdist^N)
        double rNdist=Tools::fastpow(rdist,nn-1);
        double iden=1.0/(1+rNdist*rdist);
        dfunc = -nn*rNdist*iden*iden;
        result = iden;
      } else {
        if(rdist>(1.-100.0*epsilon) && rdist<(1+100.0*epsilon)){
           result=nn/mm;
           dfunc=0.5*nn*(nn-mm)/mm;
        }else{
           double rNdist=Tools::fastpow(rdist,nn-1);
           double rMdist=Tools::fastpow(rdist,mm-1);
           double num = 1.-rNdist*rdist;
           double iden = 1./(1.-rMdist*rdist);
           double func = num*iden;
           result = func;
           dfunc = ((-nn*rNdist*iden)+(func*(iden*mm)*rMdist));
        }
      }
    }else if(type==exponential){
      result=exp(-rdist);
      dfunc=-result;
    }else if(type==gaussian){
      result=exp(-0.5*rdist*rdist);
      dfunc=-rdist*result;
    }else plumed_merror("Unknown switching function type");
// this is for the chain rule:
    dfunc*=invr0;
// this is because calculate() sets dfunc to the derivative divided times the distance.
// (I think this is misleading and I would like to modify it - GB)
    dfunc/=distance;
  }
  return result;
}

SwitchingFunction::SwitchingFunction():
  init(false),
  type(spline),
  nn(6),
  mm(12),
  invr0(0.0),
  d0(0.0),
  dmax(0.0)
{
}

void SwitchingFunction::set(int nn,int mm,double r0,double d0){
  init=true;
  type=spline;
  this->nn=nn;
  this->mm=mm;
  this->invr0=1.0/r0;
  this->d0=d0;
  this->dmax=d0+r0*pow(0.00001,1./(nn-mm));
}

double SwitchingFunction::get_r0() const {
  return 1./invr0;
}

double SwitchingFunction::get_d0() const {
  return d0;
}

}



