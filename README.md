# VolumetricParticles
Volumetric Rendering with raymarching on an adaptive world space grid. Different volumetric
data is inserted into the grid separated into various frequencies. The effects currently
supported are:  
* Low frequency, large scale homogeneous global data
* Medium frequency, medium scale heterogeneous ground fog
* High frequency, low scale particle spheres
The different frequencies are separated from each other and combined during the raymarching.  

Only opaque geometry is currently supported. As light source a directional sun light casting
shadow rays is supported. The various effects and the rendering can be changed through the user
interface.

# Build
Run Build.bat
