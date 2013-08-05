#include <cstdio>
#include <unordered_set>
#include <glint2/MatrixMaker.hpp>
#include <glint2/IceSheet.hpp>
#include <giss/memory.hpp>

namespace glint2 {

// -----------------------------------------------------
IceSheet::IceSheet() : name("icesheet") {}

IceSheet::~IceSheet() {}

/** Number of dimensions of atmosphere vector space */
size_t IceSheet::n1() const { return gcm->n1(); }

size_t IceSheet::n4() const
{
	fprintf(stderr, "IceSheet::n4() is not implement by default, it only owrks for L0 ice grids.\n");
	throw std::exception();
}

blitz::Array<double,1> IceSheet::ice_to_exch(blitz::Array<double,1> const &f2)
{
	fprintf(stderr, "IceSheet::ice_to_exch() is not implement by default, it only owrks for L0 ice grids.\n");
	throw std::exception();
}


// -----------------------------------------------------
void IceSheet::clear()
{
	grid2.reset();
	exgrid.reset();
	mask2.reset();
	// elev2.clear();	// Don't know how to do this!
}
// -----------------------------------------------------
/** Call this after you've set data members, to finish construction. */
void IceSheet::realize()
{
	if (name == "") name = grid2->name;

	// Check bounds, etc.
	if (exgrid->grid1_ncells_full != gcm->grid1->ncells_full()) {
		fprintf(stderr, "Exchange Grid for %s incompatible with GCM grid: ncells_full = %d (vs %d expected)\n",
			name.c_str(), exgrid->grid1_ncells_full, gcm->grid1->ncells_full());
		throw std::exception();
	}

	if (exgrid->grid2_ncells_full != grid2->ncells_full()) {
		fprintf(stderr, "Exchange Grid for %s incompatible with Ice grid: ncells_full = %d (vs %d expected)\n",
			name.c_str(), exgrid->grid2_ncells_full, grid2->ncells_full());
		throw std::exception();
	}

	long n2 = grid2->ndata();
	if (mask2.get() && mask2->extent(0) != n2) {
		fprintf(stderr, "Mask2 for %s has wrong size: %ld (vs %ld expected)\n",
			name.c_str(), mask2->extent(0), n2);
		throw std::exception();
	}

	if (elev2.extent(0) != n2) {
		fprintf(stderr, "Elev2 for %s has wrong size: %ld (vs %ld expected)\n",
			name.c_str(), elev2.extent(0), n2);
		throw std::exception();
	}
}
// -----------------------------------------------------
std::unique_ptr<giss::VectorSparseMatrix>
IceSheet::atm_proj_correct(ProjCorrect direction)
{
	int n1 = gcm->n1();
	std::unique_ptr<giss::VectorSparseMatrix> ret(
		new giss::VectorSparseMatrix(
		giss::SparseDescr(n1, n1)));

	giss::Proj2 proj;
	gcm->grid1->get_ll_to_xy(proj, grid2->sproj);

	for (auto cell = gcm->grid1->cells_begin(); cell != gcm->grid1->cells_end(); ++cell) {
		double native_area = cell->area;
		double proj_area = area_of_proj_polygon(*cell, proj);
		ret->add(cell->index, cell->index,
			direction == ProjCorrect::NATIVE_TO_PROJ ?
				native_area / proj_area : proj_area / native_area);
	}
	ret->sum_duplicates();

	return ret;
}
// -----------------------------------------------------
/** Corrects the area1_m result */
void IceSheet::atm_proj_correct(
	giss::SparseAccumulator<int,double> &area1_m,
	ProjCorrect direction)
{
	int n1 = gcm->n1();

	giss::Proj2 proj;
	gcm->grid1->get_ll_to_xy(proj, grid2->sproj);

	for (auto cell = gcm->grid1->cells_begin(); cell != gcm->grid1->cells_end(); ++cell) {
		double native_area = cell->area;
		double proj_area = area_of_proj_polygon(*cell, proj);
		
		// We'll be DIVIDING by area1_m, so correct the REVERSE of above.
		area1_m[cell->index] *=
			(direction == ProjCorrect::NATIVE_TO_PROJ ?
				proj_area / native_area : native_area / proj_area);
	}
}
// -----------------------------------------------------
/** Made for binding... */
static bool in_good(std::unordered_set<int> const *set, int index_c)
{
	return (set->find(index_c) != set->end());
}

void IceSheet::filter_cells1(boost::function<bool (int)> const &include_cell1)
{
	// Remove unneeded cells from exgrid
	// Figure out which cells in grid2 to keep
	std::unordered_set<int> good_index2;
	for (auto excell = exgrid->cells_begin(); excell != exgrid->cells_end(); ++excell) {
		int index1 = excell->i;
		if (include_cell1(index1)) {
			good_index2.insert(excell->j);
		} else {
			exgrid->cells_erase(excell);
		}
	}

	// Remove unneeded cells from grid2
	grid2->filter_cells(boost::bind(&in_good, &good_index2, _1));
}
// -----------------------------------------------------

// ==============================================================
// Write out the parts that this class computed --- so we can test/check them

boost::function<void ()> IceSheet::netcdf_define(NcFile &nc, std::string const &vname) const
{
	printf("Defining ice sheet %s\n", vname.c_str());

	auto one_dim = giss::get_or_add_dim(nc, "one", 1);
	NcVar *info_var = nc.add_var((vname + ".info").c_str(), ncInt, one_dim);
	info_var->add_att("name", name.c_str());

	std::vector<boost::function<void ()>> fns;

	fns.push_back(grid2->netcdf_define(nc, vname + ".grid2"));
	fns.push_back(exgrid->netcdf_define(nc, vname + ".exgrid"));

	NcDim *n2_dim = nc.add_dim((vname + ".n2").c_str(), elev2.extent(0));
	if (mask2.get()) {
		fns.push_back(giss::netcdf_define(nc, vname + ".mask2", *mask2, {n2_dim}));
	}
	fns.push_back(giss::netcdf_define(nc, vname + ".elev2", elev2, {n2_dim}));

	return boost::bind(&giss::netcdf_write_functions, fns);
}
// -------------------------------------------------------------
void IceSheet::read_from_netcdf(NcFile &nc, std::string const &vname)
{
	clear();

	printf("IceSheet::read_from_netcdf(%s) 1\n", vname.c_str());

	NcVar *info_var = nc.get_var((vname + ".info").c_str());
	name = giss::get_att(info_var, "name")->as_string(0);

	grid2.reset(read_grid(nc, vname + ".grid2").release());
	exgrid = giss::shared_cast<ExchangeGrid,Grid>(read_grid(nc, vname + ".exgrid"));
	if (giss::get_var_safe(nc, vname + ".mask2")) {
		mask2.reset(new blitz::Array<int,1>(
		giss::read_blitz<int,1>(nc, vname + ".mask2")));
	}

	elev2.reference(giss::read_blitz<double,1>(nc, vname + ".elev2"));
	printf("IceSheet::read_from_netcdf(%s) END\n", vname.c_str());
}


}	// namespace glint2
