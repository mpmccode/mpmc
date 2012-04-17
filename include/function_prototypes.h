#ifndef FXN_PROTOTYPES_H
#define FXN_PROTOTYPES_H


/* energy */
double energy(system_t *);
double energy_no_observables(system_t *);
double cavity_absolute_check (system_t *);
double lj(system_t *);
double lj_nopbc(system_t *);
double dreiding(system_t *);
double dreiding_nopbc(molecule_t *);
void update_com(molecule_t *);
void flag_all_pairs(system_t *);
void pair_exclusions(system_t *, molecule_t *, molecule_t *, atom_t *, atom_t *, pair_t *);
void minimum_image(system_t *, atom_t *, atom_t *, pair_t *);
void pairs(system_t *);
void setup_pairs(molecule_t *);
void update_pairs_insert(system_t *);
void update_pairs_remove(system_t *);
void unupdate_pairs_insert(system_t *);
void unupdate_pairs_remove(system_t *);
double pbc_cutoff(pbc_t *);
double pbc_volume(pbc_t *);
void pbc(system_t *);
double sg(system_t *);
double sg_nopbc(molecule_t *);
double coulombic(system_t *);
double coulombic_wolf(system_t *);
double coulombic_real(system_t *);
double coulombic_reciprocal(system_t *);
double coulombic_background(system_t *);
double coulombic_nopbc(molecule_t *);
double coulombic_real_gwp(system_t *);
double coulombic_reciprocal_gwp(system_t *);
double coulombic_background_gwp(system_t *);
double coulombic_nopbc_gwp(system_t *);
double coulombic_kinetic_gwp(system_t *);
double morse_energy(double, double, double, double);
double anharmonic(system_t *);
double anharmonic_energy(double, double, double);
double anharmonic_fk(double, double, double, double, double);
double anharmonic_fh_second_order(double, double, double, double, double);
double anharmonic_fh_fourth_order(double, double, double, double, double);
double h2_bond_energy(double r);
int bessik(float, float, float *, float *, float *, float *);	/* NR function */
double besselK(double, double);

/* io */
system_t *read_config(char *);
int check_config(system_t *);
molecule_t *read_molecules(system_t *);
system_t *setup_system(char *);
void error(char *);
void output(char *);
void clear_nodestats(nodestats_t *);
void clear_node_averages(avg_nodestats_t *);
void clear_observables(observables_t *);
void clear_root_averages(avg_observables_t *);
void track_ar(nodestats_t *);
void update_nodestats(nodestats_t *, avg_nodestats_t *);
void update_root_averages(system_t *, observables_t *, avg_nodestats_t *, avg_observables_t *);
int write_performance(int, system_t *);
int write_averages(system_t *);
int write_molecules(system_t *, char *);
void write_observables(FILE *, system_t *, observables_t *);
void write_dipole(FILE *, molecule_t *);
void write_field(FILE *, molecule_t *);
#ifdef MPI
	void write_states_MPI(MPI_File *, system_t *);
#else
	void write_states(FILE *, system_t *);
#endif
int wrapall(molecule_t *, pbc_t *);
void spectre_wrapall(system_t *);
int open_traj_files(system_t *);
int open_files(system_t *);
void close_files(system_t *);
curveData_t *readFitInputFiles( system_t *, int );

/* main */
void usage(char *);
double get_rand(void);
double rule30_rng(long int);
void seed_rng(long int);
#ifdef QM_ROTATION
void free_rotational(system_t *);
#endif /* QM_ROTATION */
void free_pairs(molecule_t *);
void free_atoms(molecule_t *);
void free_molecule(system_t *, molecule_t *);
void free_molecules(molecule_t *);
void free_averages(system_t *system);
void free_matricies(system_t *system);
void free_cavity_grid(system_t *system);
void cleanup(system_t *);
void terminate_handler(int, system_t *);
int memnullcheck ( void *, int, int );

/* mc */
void boltzmann_factor(system_t *, double, double);
void register_accept(system_t *);
void register_reject(system_t *);
int mc(system_t *);
molecule_t *copy_molecule(system_t *, molecule_t *);
void translate(molecule_t *, pbc_t *, double);
void rotate(molecule_t *, pbc_t *, double);
void displace(molecule_t *, pbc_t *, double, double);
void displace_1D(system_t *, molecule_t *, double);
void displace_gwp(molecule_t *, double);
void spectre_displace(system_t *, molecule_t *, double, double, double);
void spectre_charge_renormalize(system_t *);
void make_move(system_t *);
void checkpoint(system_t *);
void restore(system_t *);
double surface_energy(system_t *, int);
void molecule_rotate(molecule_t *, double, double, double);
int surface_dimer_geometry(system_t *, double, double, double, double, double, double, double);
int surface_dimer_parameters(system_t *, param_t *);
void surface_curve(system_t *, double, double, double, double *);
int surface(system_t *);
int surface_fit(system_t *);
int surface_virial(system_t *);
void setup_cavity_grid(system_t *);
void cavity_volume(system_t *);
void cavity_probability(system_t *);
void cavity_update_grid(system_t *);
void qshift_do(system_t *, qshiftData_t *, double, double);
double calcquadrupole(system_t *);
void volume_change(system_t *);
void revert_volume_change(system_t *);

/*surface_fit*/
void surface_curve( system_t *, double, double, double, double * );
double error_calc ( system_t *, int, int, curveData_t *, double );
int alloc_curves ( int, int, curveData_t * );
void output_pdbs ( system_t *, int, curveData_t * );
void output_params ( double, double, param_t * );
param_t * record_params ( system_t * );
void surf_perturb ( system_t *, double, qshiftData_t *, param_t *);
void output_fit ( int, int, curveData_t *, double, double *);
void get_curves ( system_t *, int, curveData_t *, double, double, double );
void revert_parameters ( system_t *, param_t *);
void new_global_min ( system_t *, int, int, curveData_t * );

/* polarization */
double polar(system_t *);
void thole_amatrix(system_t *);
void thole_bmatrix(system_t *);
void thole_bmatrix_dipoles(system_t *);
void thole_polarizability_tensor(system_t *);
void thole_field(system_t *);
void thole_field_wolf(system_t *);
void thole_field_nopbc(system_t *);
void thole_field_real(system_t *);
void thole_field_recip(system_t *);
void thole_field_self(system_t *);
int thole_iterative(system_t *);
void invert_matrix(int, double **, double **);
int num_atoms(system_t *);
void thole_resize_matrices(system_t *);
void print_matrix(int N, double **matrix);

/* polarization - CUDA */
#ifdef CUDA
float polar_cuda(system_t *);
#endif /* CUDA */

/* linear algebra - VDW */
double vdw(system_t *);

/* pimc */
int pimc(system_t *);

/* histogram */
int histogram(system_t *);
int cart2frac(double *, double *, system_t *);
int frac2cart(double *, double *, system_t *);
void setup_histogram(system_t *);
void allocate_histogram_grid(system_t *);
void write_histogram(FILE *, int ***, system_t *);
void zero_grid(int ***, system_t *);
void population_histogram(system_t *);
void mpi_copy_histogram_to_sendbuffer(char *, int ***, system_t *);
void mpi_copy_rcv_histogram_to_data(char *, int ***, system_t *);
void update_root_histogram(system_t *);
void write_histogram(FILE *, int ***, system_t *);

/* dxwrite */
void write_frozen(FILE *fp_frozen, system_t *system);

#ifdef QM_ROTATION
/* quantum rotation */
void quantum_system_rotational_energies(system_t *);
void quantum_rotational_energies(system_t *, molecule_t *, int, int);
void quantum_rotational_grid(system_t *, molecule_t *);
complex_t **rotational_hamiltonian(system_t *, molecule_t *, int, int);
int determine_rotational_eigensymmetry(molecule_t *, int, int);
double rotational_basis(int, int, int, double, double);
double rotational_potential(system_t *, molecule_t *, double, double);
double hindered_potential(double);
double rotational_integrate(system_t *, molecule_t *, int, int, int, int, int);
#endif /* QM_ROTATION */

#ifdef DEBUG
void test_pairs(molecule_t *);
void test_molecule(molecule_t *);
void test_list(molecule_t *);
void test_cavity_grid(system_t *);
#endif /* DEBUG */
#endif // FXN_PROTOTYPES_H

//fugacity functions
double h2_fugacity(double, double);
double h2_fugacity_back(double, double);
double h2_comp_back(double, double);
double h2_fugacity_shaw(double, double);
double h2_fugacity_zhou(double, double);
double ch4_fugacity(double, double);
double ch4_fugacity_back(double, double);
double ch4_comp_back(double, double);
double ch4_fugacity_PR(double, double);
double n2_fugacity(double, double);
double n2_fugacity_back(double, double);
double n2_comp_back(double, double);
double n2_fugacity_PR(double, double);
double n2_fugacity_zhou(double, double);
double co2_fugacity(double, double);
