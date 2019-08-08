# Import the JAX library
import jax.numpy as np
from jax import grad, jit, vmap

# Implementation of the angle function
def angle(x):
    r1 = x[0,:]-x[1,:]
    r2 = x[2,:]-x[1,:]

    costheta = np.dot(r1,r2) / np.linalg.norm(r1) / np.linalg.norm(r2)
    theta = np.arccos(costheta) 
    return theta

# Use JAX to auto-gradient it
grad_angle = grad(angle)

# The CV function actually called
def cv1(x):
    return angle(x), grad_angle(x)

