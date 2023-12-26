# MIDSX (Monte Carlo Interactions and Dosage Simulation of X-rays)
![Contributors](https://img.shields.io/github/contributors/jmeneghini/MIDSX)
![Forks](https://img.shields.io/github/forks/jmeneghini/MIDSX)
![Stars](https://img.shields.io/github/stars/jmeneghini/MIDSX)
![Issues](https://img.shields.io/github/issues/jmeneghini/MIDSX)

## Table of Contents
- [Description](#description)
- [Getting Started](#getting-started)
  * [Dependencies](#dependencies)
  * [Installation](#installation)
- [Documentation](#documentation)
- [Usage](#usage)



## Description
MIDSX is a code system for simulating the propagation of X-rays through a medium. Using [EPDL](https://www-nds.iaea.org/epics/) and [NIST](https://www.nist.gov/pml/x-ray-mass-attenuation-coefficients) datasets, it samples photon free paths and interactions to propagate photons through a computational domain of specified dimensions and geometries. Geometries/bodies are defined using the [NIfTI-1 Data Format](https://nifti.nimh.nih.gov/nifti-1), which are specified in JSON files. To extract results from a simulation, both Volume and Surface tallies with specifiable measurable quantities and geometries are available, along with derived quantities, such as air kerma for HVL measurements. The following project is a **WIP**, so documentation is currently not available, but it is in the works.

## Getting Started

### Dependencies

MIDSX requires the following dependencies to be manually installed:

* **CMake 3.10.0 or higher:** If you don't have CMake installed, or require a newer version, follow this [guide](https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line).

* **SQLite3 Library:** On Linux, the library can be installed using your distribution's package manager. Using apt: `sudo apt install sqlite3 libsqlite3-dev`. On Mac, the library can be installed with brew: `brew install sqlite3 libsqlite3-dev`.

* **Python 3.8.x or higher:** If not already installed, go [here](https://www.python.org/downloads/).

* **nibabel:** To load NifTI files, MIDSX uses the python package nibabel. It can be easily installed with pip: `pip install nibabel`.

MIDSX additionally uses the following libraries via Git submodules; these do not need to be installed manually:

* **Eigen:** For data storage and linear algebra.

* **pybind11:** For use of nibabel in C++ code. Will be used later for MIDSX python bindings.

### Installation

MIDSX has been tested on macOS and Ubuntu. To install with the command line:

1. Clone the repo and enter the directory:
   ```sh
   git clone https://github.com/jmeneghini/MIDSX.git
   cd MIDSX
   ```

2. Create and enter the build directory:
   ```sh
   mkdir build
   cd build
   ```

3. Generate cmake files and install:
   ```sh
   cmake ..
   sudo make install
   ```

## Documentation
The [documentation](https://jmeneghini.github.io/MIDSX/) for MIDSX is generated via Doxygen and is hosted with Github Pages. \
\
The preprint for the validation of MIDSX can be found on [arXiv](https://arxiv.org/abs/2311.16873).

## Usage
To use the library, configure a project with the following CMakeLists.txt structure:

```cmake
cmake_minimum_required(VERSION 3.10)
project(project_name)

# Links libraries and turns off parallelization if building in debug mode
function(create_executable EXE_NAME SRC_FILE)
    add_executable(${EXE_NAME} ${SRC_FILE})
    target_link_libraries(${EXE_NAME} PRIVATE ${COMMON_LIBS})
endfunction()

# Finds pybind11 (doesn't seem to link with library) and MIDSX
find_package(pybind11 REQUIRED)
find_package(MIDSX REQUIRED)
# Needed for parallelization
find_package(OpenMP REQUIRED)

# If unable to find pybind11, manually set the extern directory
# add_subdirectory(../../extern/pybind11 pybind11)

# Sets common libs that are linked to executable
set(COMMON_LIBS pybind11::embed MIDSX::MIDSX)

create_executable(project main.cpp)
```

A typical MIDSX simulation has the following structure:

* A .json file containing scene information:
```json
{
  "dim_space": [
    4,
    4,
    100
  ],
  "background_material_name": "Air, Dry (near sea level)",
  "voxel_grids": [
    {
      "file_path": "path/to/nifti/file.nii",
      "origin": [0, 0, 10]
    }
  ]
}
```

* Using the .json file, the `ComputationalDomain` object can be initialized:
```C++
ComputationalDomain comp_domain("domain.json");
```

* The `InteractionData` object can either be manually initialized via a list of material names or by calling `comp_domain.getInteractionData()`, which generates an `InteractionData` object using the materials contained in the `ComputationalDomain` object.

```C++
std::vector<std::string> material_names = {"Air, Dry (near sea level)", "Water, Liquid"};
InteractionData interaction_data(material_names);

// or

InteractionData interaction_data = comp_domain.getInteractionData();
```

* Using these two objects, the `PhysicsEngine` can now be initialized:
```C++
PhysicsEngine physics_engine(comp_domain, interaction_data);
```

* To make measurements, tallies must be specified. Various `SurfaceTally` and `VolumeTally` objects are available, and these objects must be supplied with a `SurfaceQuantityContainer` or `VolumeQuanitityContainer` which holds `SurfaceQuantity`'s and `VolumeQuantity`'s, respectively, to be measured by the tallies. These containers can be created manually, or predefined containers can be used via `SurfaceQuantityContainerFactory` or `VolumeQuantityContainerFactory`. Since these tallies must be constructed in an OpenMP parallel region, you must create seperate functions which return a vector of unique pointers to these tallies.

```C++
std::vector<std::unique_ptr<SurfaceTally>> initializeSurfaceTallies() {
    // Create an empty vector of unique pointers to surface tallies
    std::vector<std::unique_ptr<SurfaceTally>> tallies = {};
    
    // Use the predefined container factory to create a container with all quantities
    // to create a disc surface tally and add it to the vector
    surface_tallies.emplace_back(std::make_unique<DiscSurfaceTally>(
                Eigen::Vector3d(2, 2, 100),
                Eigen::Vector3d(0, 0, 1),
                1.0,
                SurfaceQuantityContainerFactory::AllQuantities()));
    return tallies;
}
std::vector<std::unique_ptr<VolumeTally>> initializeVolumeTallies() {
    // Create an empty vector of unique pointers to volume tallies
    std::vector<std::unique_ptr<VolumeTally>> tallies = {};
    
    // Manually create a container with only energy deposition
    auto volume_container = VolumeQuantityContainer();
    volume_container.addVectorQuantity(VectorVolumeQuantity(VectorVolumeQuantityType::EnergyDeposition));
    
    // Create a cuboid volume tally and add it to the vector
    volume_tallies.emplace_back(std::make_unique<AACuboidVolumeTally>(
                Eigen::Vector3d(0, 0, 155),
                Eigen::Vector3d(39.0, 39.0, 175),
                volume_container));
    return tallies;
}
```

* In order to run the simulation, one just needs a way to generate photons. MIDSX uses `EnergySpectrum`, `Directionality`, and `SourceGeometry` objects to build a `PhotonSource`.

```C++
MonoenergeticSpectrum mono_spectrum(56.4E3);
std::unique_ptr<EnergySpectrum> spectrum = std::make_unique<MonoenergeticSpectrum>(mono_spectrum);

std::unique_ptr<Directionality> directionality = std::make_unique<BeamDirectionality>(
        BeamDirectionality(Eigen::Vector3d(39.0/2, 39.0/2, 180)));


std::unique_ptr<SourceGeometry> geometry = std::make_unique<PointGeometry>(
        PointGeometry(Eigen::Vector3d(0.0, dim_space.y()/2.0, dim_space.z()/2.0)));

PhotonSource source(std::move(spectrum), std::move(directionality), std::move(geometry));
```

* Now, with `PhotonSource`, `PhysicsEngine`, and the tally initialization functions, you can use `runSimulation` to run the simulation:

```C++
const int NUM_OF_PHOTONS = 1000000;
runSimulation(source, physics_engine, initializeSurfaceTallies,
              initializeVolumeTallies, NUM_OF_PHOTONS);
```

* Data can be retrieved from the simulation via `physics_engine.getSurfaceQuantityContainers()` and `physics_engine.getVolumeQuantityContainers()`.

* For further info, look at the several examples in the `cpp_simulations` folder.
