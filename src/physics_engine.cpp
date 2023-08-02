#include "physics_engine.h"

bool PhysicsEngineHelpers::areCollinearAndSameDirection(const Eigen::Vector3d& vec1, const Eigen::Vector3d& vec2) {
    double tolerance = 1E-6;
    // The vectors are collinear and pointing in the same direction
    // if their normalized forms are approximately equal
    double mag_diff = (vec1.normalized() - vec2.normalized()).norm();
    return mag_diff < tolerance;
}

PhysicsEngine::PhysicsEngine(VoxelGrid& voxel_grid, const InteractionData& interaction_data, Detector& detector) : voxel_grid_(voxel_grid), interaction_data_(interaction_data),
                                                                                                               uniform_dist_(0.0, 1.0), photoelectric_effect_(std::make_shared<PhotoelectricEffect>()),
                                                                                                               coherent_scattering_(std::make_shared<CoherentScattering>()),
                                                                                                               incoherent_scattering_(std::make_shared<IncoherentScattering>()),
                                                                                                               detector_(detector){};
void PhysicsEngine::transportPhoton(Photon& photon) {
    while (!photon.isTerminated()) {
        transportPhotonOneStep(photon);
    }
}

void PhysicsEngine::transportPhotonOneStep(Photon& photon) {
    // delta tracking algorithm
    double photon_energy = photon.getEnergy();
    double max_cross_section = interaction_data_.interpolateMaxTotalCrossSection(photon_energy);

    // move photon to distance of free path length
    double free_path_length = getFreePath(max_cross_section);
    photon.move(free_path_length);

    // Check if photon is still in voxel grid. If no, kill photon
    std::array<int, 3> current_voxel_index{};
    try {
        current_voxel_index = voxel_grid_.getVoxelIndex(photon.getPosition());
    } catch (const std::out_of_range &e) {
        processPhotonOutsideVoxelGrid(photon);
        return;
    }

    // get material of new voxel
    Voxel& current_voxel = voxel_grid_.getVoxel(current_voxel_index);
    int current_material_id = current_voxel.materialID;

    // get total cross section for current material
    Material current_material = interaction_data_.getMaterial(current_material_id);
    double total_cross_section = (current_material.getData()->interpolateTotalCrossSection(photon_energy));

    // sample delta scattering
    bool delta_scattering = isDeltaScatter(total_cross_section, max_cross_section);
    if (delta_scattering) {
        return;
    }
    else {
        setInteractionType(photon, current_material, total_cross_section);
        photon.setPrimary(false); // photon has interacted
        double energy_deposited = photon.interact(interaction_data_, current_material);
        current_voxel.dose += energy_deposited;
    }
}


void PhysicsEngine::setInteractionType(Photon& photon, Material& material, double total_cross_section) {

    double photon_energy = photon.getEnergy();

    // get cross sections for current element
    double coherent_scattering_cross_section = material.getData()->interpolateCoherentScatteringCrossSection(photon_energy);
    double incoherent_scattering_cross_section = material.getData()->interpolateIncoherentScatteringCrossSection(photon_energy);
// these values seem to small while debugging


    // sample interaction type
    double p_coherent = coherent_scattering_cross_section / total_cross_section;
    double p_incoherent = incoherent_scattering_cross_section / total_cross_section;

    // This is a special case of inversion sampling that is done in DiscreteDistribution.
    // In this case, the x values do not matter, only the y values
    double random_number = uniform_dist_.sample();


    if (random_number < p_coherent) {
        photon.setInteractionBehavior(coherent_scattering_);
    }
    else if (random_number < p_coherent + p_incoherent) {
        photon.setInteractionBehavior(incoherent_scattering_);
    }
    else {
        photon.setInteractionBehavior(photoelectric_effect_);
    }
}

double PhysicsEngine::getFreePath(double max_cross_section) {
    double free_path = -log(uniform_dist_.sample()) /
                       (max_cross_section);
    return free_path;
}

void PhysicsEngine::processPhotonOutsideVoxelGrid(Photon& photon) {
    photon.terminate();
    if (isDetectorHit(photon)) {
        detector_.updateTallies(photon);
    }
}

bool PhysicsEngine::isDeltaScatter(double cross_section, double max_cross_section) {
    double p_delta_scatter = 1 - cross_section / max_cross_section;
    return uniform_dist_.sample() < p_delta_scatter;
}

bool PhysicsEngine::isDetectorHit(Photon& photon) {
    // check if Omega is a scalar multiple of detector position - photon position
    return PhysicsEngineHelpers::areCollinearAndSameDirection(photon.getDirection(), detector_.getPosition() - photon.getPosition());
}



