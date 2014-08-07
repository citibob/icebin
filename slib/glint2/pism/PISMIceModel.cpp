#include <glint2/pism/PISMIceModel.hpp>
#include <glint2/pism/GLINT2EnthalpyConverter.hpp>
#include "bedrockThermalUnit.hh"

using namespace pism;

namespace glint2 {
namespace gpism {

// /** Sum a 3-D vector in the Z direction to create a 2-D vector.
// 
// <p>Note that this sums up all the values in a column, including ones
// above the ice. This may or may not be what you need. Also, take a look
// at IceModel::compute_ice_enthalpy(PetscScalar &result) in iMreport.cc.</p>
// 
// <p>As for the difference between IceModelVec2 and IceModelVec2S, the
// former can store fields with more than 1 "degree of freedom" per grid
// point (such as 2D fields on the "staggered" grid, with the first
// degree of freedom corresponding to the i-offset and second to
// j-offset).</p>
// 
// <p>IceModelVec2S is just IceModelVec2 with "dof == 1", and
// IceModelVec2V is IceModelVec2 with "dof == 2". (Plus some extra
// methods, of course.)</p>
// 
// <p>Either one of IceModelVec2 and IceModelVec2S would work in this
// case.</p>
// 
// @see https://github.com/pism/pism/issues/229 */
// static PetscErrorCode sum_columns(IceModelVec3 &input, IceModelVec2S &output)
// {
//   PetscScalar *column = NULL;
//   IceGrid &grid = *input.get_grid();
//   int ierr = 0;
// 
//   ierr = input.begin_access(); CHKERRQ(ierr);
//   ierr = output.begin_access(); CHKERRQ(ierr);
//   for (PetscInt   i = grid.xs; i < grid.xs+grid.xm; ++i) {
//     for (PetscInt j = grid.ys; j < grid.ys+grid.ym; ++j) {
//       ierr = input.getInternalColumn(i, j, &column); CHKERRQ(ierr);
// 
//       output(i,j) = 0.0;
//       for (unsigned int k = 0; k < grid.Mz; ++k)
//         output(i,j) += column[k];
//     }
//   }
//   ierr = output.end_access(); CHKERRQ(ierr);
//   ierr = input.end_access(); CHKERRQ(ierr);
// 
//   return 0;
// }
// 

// ================================

PISMIceModel::PISMIceModel(pism::IceGrid &g, pism::Config &config, pism::Config &overrides, PISMIceModel::Params const &_params) :
	pism::IceModel(g, config, overrides),
	params(_params)
{}

PISMIceModel::~PISMIceModel() {} // must be virtual merely because some members are virtual


//! \brief Decide which enthalpy converter to use.
PetscErrorCode PISMIceModel::allocate_enthalpy_converter() {
	PetscErrorCode ierr;

	if (EC != NULL)
		return 0;

	EC = new GLINT2EnthalpyConverter(config);

	if (getVerbosityLevel() > 3) {
		PetscViewer viewer;
		ierr = PetscViewerASCIIGetStdout(PETSC_COMM_WORLD,&viewer); CHKERRQ(ierr);
		ierr = EC->viewConstants(viewer); CHKERRQ(ierr);
	}

	return 0;
}

PetscErrorCode PISMIceModel::allocate_subglacial_hydrology()
{
	printf("BEGIN PISMIceModel::allocate_subglacial_hydrology()\n");
//	printf("subglacial_hydrology = %p %p\n", subglacial_hydrology, pism::IceModel::subglacial_hydrology);
	if (pism::IceModel::subglacial_hydrology != NULL) return 0; // indicates it has already been allocated
	subglacial_hydrology = new NullTransportHydrology(grid, config);
	printf("END PISMIceModel::allocate_subglacial_hydrology()\n");
	return 0;
}



PetscErrorCode PISMIceModel::allocate_couplers()
{
	printf("BEGIN PISMIceModel::allocate_couplers()\n");
	PetscErrorCode ierr;
	// Initialize boundary models:
	PAFactory pa(grid, config);
	PSFactory ps(grid, config);
	POFactory po(grid, config);
	pism::AtmosphereModel *atmosphere;

	ierr = PetscOptionsBegin(grid.com, "", "Options choosing PISM boundary models", ""); CHKERRQ(ierr);

	// GLINT2-modified version
	if (surface == NULL) {
		surface = new PSConstantGLINT2(grid, config);
		external_surface_model = false;

		pa.create(atmosphere);
		surface->attach_atmosphere_model(atmosphere);
	}

	if (ocean == NULL) {
		po.create(ocean);
		external_ocean_model = false;
	}
	ierr = PetscOptionsEnd(); CHKERRQ(ierr);

	printf("END PISMIceModel::allocate_couplers()\n");
	return 0;
}


PetscErrorCode PISMIceModel::createVecs()
{
	super::createVecs();

	PetscErrorCode ierr;
	int WIDE_STENCIL = grid.max_stencil_width;

	// ---- Geothermal Flux: instantaneous
	ierr = upward_geothermal_flux.create(grid, "upward_geothermal_flux", WITHOUT_GHOSTS); CHKERRQ(ierr);
	ierr = upward_geothermal_flux.set_attrs("internal",
		"upward_geothermal_flux",
		"W m-2", ""); CHKERRQ(ierr);

	// ---- Geothermal Flux: cumulative
	ierr = geothermal_flux_sum.create(grid, "geothermal_flux", WITH_GHOSTS, WIDE_STENCIL); CHKERRQ(ierr);
	ierr = geothermal_flux_sum.set_attrs("climate_steady",
		"Geothermal energy from bedrock surface", "W m-2", ""); CHKERRQ(ierr);
	// ierr = geothermal_flux.set_glaciological_units("mJ m-2");

	// ---- Geothermal Flux: cumulative
	ierr = upward_geothermal_flux_sum.create(grid, "upward_geothermal_flux", WITH_GHOSTS, WIDE_STENCIL); CHKERRQ(ierr);
	ierr = upward_geothermal_flux_sum.set_attrs("climate_steady",
		"Geothermal energy from bedrock surface", "W m-2", ""); CHKERRQ(ierr);
	// ierr = upward_geothermal_flux.set_glaciological_units("mJ m-2");


#if 0
	// ---- Strain Heating: instantaneous
	ierr = strain_heating2.create(grid, "strain_heating2", WITHOUT_GHOSTS); CHKERRQ(ierr);
	ierr = strain_heating2.set_attrs("internal",
		"rate of strain heating in ice (dissipation heating), summed over column",
		"W m-2", ""); CHKERRQ(ierr);
#endif

	// ---- Basal Frictional Heating: cumulative
	ierr = basal_frictional_heating_sum.create(grid, "basal_frictional_heating", WITHOUT_GHOSTS); CHKERRQ(ierr);
	ierr = basal_frictional_heating_sum.set_attrs("internal",
		"Basal frictional heating",
		"W m-2", ""); CHKERRQ(ierr);

	// ---- Strain Heating: cumulative
	ierr = strain_heating_sum.create(grid, "strain_heating", WITHOUT_GHOSTS); CHKERRQ(ierr);
	ierr = strain_heating_sum.set_attrs("internal",
		"Strain heating",
		"W m-2", ""); CHKERRQ(ierr);

	// ---- Enthalpy: vertically integrated, and converted to J/m^2
	ierr = total_enthalpy.create(grid, "total_enthalpy", WITH_GHOSTS, WIDE_STENCIL); CHKERRQ(ierr);
	ierr = total_enthalpy.set_attrs("total_enthalpy",
		"Vertically integrated enthalpy of ice sheet", "J m-2", ""); CHKERRQ(ierr);

	// ----- Calving rate
	ierr = calving_mass.create(grid, "calving_mass", WITHOUT_GHOSTS); CHKERRQ(ierr);
	ierr = calving_mass.set_attrs("diagnostic",
		"discharge (calving) flux (positive means ice loss)",
		"kg m-2 s-1", ""); CHKERRQ(ierr);

	return 0;
}

PetscErrorCode PISMIceModel::massContPreHook()
{
#if 0	// Avoid warnings until we put something in this method
	PetscErrorCode ierr;

	// Enthalpy and mass continuity are stepped with different timesteps.
	// Fish out the timestep relevant to US.
	const double my_t0 = grid.time->current();
	const double my_dt = this->dt;
#endif

	return 0;
}


PetscErrorCode PISMIceModel::massContPostHook()
{
#if 0	// Avoid warnings until we put something in this method
	PetscErrorCode ierr;

	// Enthalpy and mass continuity are stepped with different timesteps.
	// Fish out the timestep relevant to US.
	const double my_t0 = grid.time->current();
	const double my_dt = this->dt;
#endif


	return 0;
}


PetscErrorCode PISMIceModel::energyStep()
{
	PetscErrorCode ierr;

	printf("BEGIN PISMIceModel::energyStep(t=%f, dt=%f)\n", t_TempAge, dt_TempAge);

	// Enthalpy and mass continuity are stepped with different timesteps.
	// Fish out the timestep relevant to US.
	const double my_t0 = t_TempAge;			// Time at beginning of timestep
	const double my_dt = dt_TempAge;

	// =========== BEFORE Energy Step

	// =========== The Energy Step Itself
	ierr = super::energyStep(); CHKERRQ(ierr);

	// =========== AFTER Energy Step

	// We need to integrate over strain_heating and geothermal_flux, which
	// are given in PISM as rates.


	// --------- Upward Geothermal Flux
	// Use actual geothermal flux, not the long-term average..
	// See: file:///Users/rpfische/git/pism/build/doc/browser/html/classPISMBedThermalUnit.html#details
	ierr = btu->get_upward_geothermal_flux(upward_geothermal_flux); CHKERRQ(ierr);
	ierr = upward_geothermal_flux_sum.add(dt, upward_geothermal_flux); CHKERRQ(ierr);

	// ----------- Geothermal Flux
	ierr = geothermal_flux_sum.add(dt, geothermal_flux); CHKERRQ(ierr);

	// ---------- Basal Frictional Heating (see iMenthalpy.cc l. 220)
	IceModelVec2S *Rb = NULL;
	ierr = stress_balance->get_basal_frictional_heating(Rb); CHKERRQ(ierr);
	basal_frictional_heating_sum.add(dt, *Rb);

	// ------------ Volumetric Strain Heating
	// strain_heating_sum += dt * sum_columns(strainheating3p)
	IceModelVec3 *strain_heating3p;
	stress_balance->get_volumetric_strain_heating(strain_heating3p);
#if 0
	ierr = strain_heating3p->sumColumns(strain_heating2); CHKERRQ(ierr);
	ierr = strain_heating_sum.add(dt, strain_heating2); CHKERRQ(ierr);
#else
	ierr = strain_heating3p->sumColumns(strain_heating_sum, 1e0, dt); CHKERRQ(ierr);
#endif

	printf("END PISMIceModel::energyStep(time=%f)\n", t_TempAge);
	return 0;
}

PetscErrorCode PISMIceModel::prepare_nc(std::string const &fname, std::unique_ptr<PIO> &nc)
{
	PetscErrorCode ierr;

	nc.reset(new PIO(grid, grid.config.get_string("output_format")));

	ierr = nc->open(fname, PISM_READWRITE_MOVE); CHKERRQ(ierr);
    ierr = nc->def_time(config.get_string("time_dimension_name"),
                       grid.time->calendar(),
                       grid.time->CF_units_string()); CHKERRQ(ierr);
// These are in iMtimseries, but not listed as required in iceModelVec.hh
//    ierr = nc->put_att_text(config.get_string("time_dimension_name"),
//                           "bounds", "time_bounds"); CHKERRQ(ierr);
//    ierr = write_metadata(nc, true, false); CHKERRQ(ierr);
//	ierr = nc->close(): CHKERRQ(ierr);

	return 0;
}


/** @param t0 Time of last time we coupled. */
PetscErrorCode PISMIceModel::write_post_energy(double t0)
{
	PetscErrorCode ierr;

	printf("BEGIN PISMIceModel::write_post_energy()\n");

	double t1 = enthalpy_t();	// Current time of the enthalpy portion of ice model.

	// ------ Write it out
	PIO nc(grid, grid.config.get_string("output_format"));

	nc.open((params.output_dir / "post_energy.nc").c_str(), PISM_READWRITE);	// append to file
	nc.append_time(config.get_string("time_dimension_name"), t1);

	// -------- Get variables that are not in this class
	pism::IceModelVec2 &basal_runoff_sum(null_hydrology()->basal_runoff_sum);
	bool update_2d_discharge = discharge_flux_2D_cumulative.was_created();
	if (!update_2d_discharge) {
		fprintf(stderr, "WARNING: You must us '-extra_vars discharge_flux_cumulative' on the PISM command line in order to get calving discharge from PISM.\n");
	}

	calving_mass.copy_from(discharge_flux_2D_cumulative);

	// ------ Divide by dt
	double by_dt = 1.0 / (t1 - t0);
	ierr = basal_frictional_heating_sum.scale(by_dt); CHKERRQ(ierr);
	ierr = strain_heating_sum.scale(by_dt); CHKERRQ(ierr);
	ierr = geothermal_flux_sum.scale(by_dt); CHKERRQ(ierr);
	ierr = upward_geothermal_flux_sum.scale(by_dt); CHKERRQ(ierr);
//	ierr = total_enthalpy.scale(by_dt); CHKERRQ(ierr);
	ierr = basal_runoff_sum.scale(by_dt); CHKERRQ(ierr);
	if (update_2d_discharge) {		// Calving, iceberg removal
		ierr = calving_mass.scale(by_dt); CHKERRQ(ierr);
		// ierr = discharge_flux_2D_cumulative.scale(by_dt); CHKERRQ(ierr);
	}

	// Write out the fields
	ierr = basal_frictional_heating_sum.write(nc, PISM_DOUBLE); CHKERRQ(ierr);
	ierr = strain_heating_sum.write(nc, PISM_DOUBLE); CHKERRQ(ierr);
	ierr = geothermal_flux_sum.write(nc, PISM_DOUBLE); CHKERRQ(ierr);
	ierr = upward_geothermal_flux_sum.write(nc, PISM_DOUBLE); CHKERRQ(ierr);
	ierr = total_enthalpy.write(nc, PISM_DOUBLE); CHKERRQ(ierr);
	ierr = basal_runoff_sum.write(nc, PISM_DOUBLE); CHKERRQ(ierr);
	if (update_2d_discharge) {
		ierr = calving_mass.write(nc, PISM_DOUBLE); CHKERRQ(ierr);
		// ierr = discharge_flux_2D_cumulative.write(nc, PISM_DOUBLE); CHKERRQ(ierr);
	}

	nc.close();

	// ----- Zero the sum variables
	ierr = basal_frictional_heating_sum.set(0); CHKERRQ(ierr);
	ierr = strain_heating_sum.set(0); CHKERRQ(ierr);
	ierr = geothermal_flux_sum.set(0); CHKERRQ(ierr);
	ierr = upward_geothermal_flux_sum.set(0); CHKERRQ(ierr);
//	ierr = total_enthalpy.set(0); CHKERRQ(ierr);
	ierr = basal_runoff_sum.set(0); CHKERRQ(ierr);
	if (update_2d_discharge) {
		// ierr = calving_mass.set(0); CHKERRQ(ierr);
		ierr = discharge_flux_2D_cumulative.set(0); CHKERRQ(ierr);
	}

	printf("END PISMIceModel::write_post_energy()\n");

	return 0;
}

PetscErrorCode PISMIceModel::grid_setup()
{
//	PetscErrorCode ierr;
	super::grid_setup();

	// super::grid_setup() trashes grid.time->start().  Now set it correctly.
	grid.time->set_start(params.time_start_s);
	grid.time->set(params.time_start_s);

	return 0;
}



PetscErrorCode PISMIceModel::misc_setup()
{
	super::misc_setup();

	PetscErrorCode ierr;
	std::unique_ptr<PIO> nc;

//	nc = prepare_nc("pre_mass.nc");
//	nc = prepare_nc("post_mass.nc");
//	nc = prepare_nc("pre_energy.nc");

	std::string ofname = (params.output_dir / "post_energy.nc").string();
	ierr = prepare_nc(ofname, nc); CHKERRQ(ierr);
	basal_frictional_heating_sum.define(*nc, PISM_DOUBLE);
	strain_heating_sum.define(*nc, PISM_DOUBLE);

	geothermal_flux_sum.define(*nc, PISM_DOUBLE);
	upward_geothermal_flux_sum.define(*nc, PISM_DOUBLE);
	total_enthalpy.define(*nc, PISM_DOUBLE);
	nc->close();

	return 0;
}



PetscErrorCode PISMIceModel::run_to(double time)
{
	PetscErrorCode ierr;

	ierr = pism::IceModel::run_to(time); CHKERRQ(ierr);

	// ============ Compute Total Enthalpy of Ice Sheet
	double by_rhoi = 1e0 / config.get("ice_density");	// m^3 kg-1
	Enth3.sumColumns(total_enthalpy, 0.0, by_rhoi);

	return 0;
}

}}	// namespace glint2::gpism

