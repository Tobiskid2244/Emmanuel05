# This is only a partial example. This function does not compute the
# gradient, so it is useless for biasing. See the other regression
# tests for how to auto-grad.

# And, of course, one should not call slow functions (such as print)
# in the CV calculation.

import numpy as np
import plumedCommunications
from sys import stderr as log

# import plumedUtilities
print("Imported pyCoord.", file=log)

N:int=6
M:int=12
D0:float=0.0
R0:float=2.0
INVR0:float=1.0/R0
DMAX=D0+R0*(0.00001**(1./(N-M)))
STRETCH=1.0
SHIFT=0.0
def switch(d: np.ndarray) -> np.ndarray:
    ret=np.zeros_like(d)
    WhereToCalc=d<=DMAX
    rdist=d[WhereToCalc]*INVR0
    rNdist=(rdist)**(N-1)
    ret[WhereToCalc]=(1.0/(1+rdist*rNdist))*STRETCH
    ret[WhereToCalc]+=SHIFT
    
    return ret

def stretchSwitch(mydmax):
    s=switch(np.array([0.0,mydmax]))
    # DMAX=tmp
    print(s, file=log)
    stretch=1/(s[0]-s[1])
    return stretch,-s[1]*stretch

mydmax=DMAX
DMAX+=0.1
STRETCH,SHIFT=stretchSwitch(mydmax)
DMAX=mydmax

def pyCoord(action: plumedCommunications.PythonCVInterface):
    # NB: This is not a realistic case of using the neigbour list!!!
    # cvPY: PYCVINTERFACE GROUPA=1,4 IMPORT=pydistancegetAtPos CALCULATE=pydist

    atoms = action.getPositions()
    nat = atoms.shape[0]
    nl = action.getNeighbourList()

    assert nl.size() == ((nat - 1) * nat) // 2
    pbc = action.getPbc()
    couples = nl.getClosePairs()
    d = atoms[couples[:, 0]] - atoms[couples[:, 1]]
    d = pbc.apply(d)
    d = np.linalg.norm(d, axis=1)
    print(d, file=log)
    sw=switch(d)
    print(sw, file=log)
    return np.sum(sw)
