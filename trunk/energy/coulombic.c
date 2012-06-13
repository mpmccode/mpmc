/* 

@2007, Jonathan Belof
Space Research Group
Department of Chemistry
University of South Florida

Revised by Keith McLaughlin
10 June 2009

All calculations exactly reproduce old version of the code.
Wolf doesn't match Ewald and seems extremely dependent on alpha
for a test system of H2 in an (18\AA)^3 box

*/

#include <mc.h>

double coulombic_self(system_t *);

/* total ES energy term */
double coulombic(system_t *system) {

	double real, reciprocal, self;
	double potential;

	/* construct the relevant ewald terms */
	if ( system->wolf )
		potential = coulombic_wolf(system);
	else {
		real = coulombic_real(system);
		reciprocal = coulombic_reciprocal(system);
		self = coulombic_self(system);
		/* return the total electrostatic energy */
		potential = real + reciprocal + self;
	}

	return(potential);
}

/* fourier space sum */
double coulombic_reciprocal(system_t *system) {

	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	int q, p, kmax, l[3];
	double alpha;
	double k[3], k_squared, gaussian, position_product;
	double SF_re, SF_im; /* structure factor */
	double multiplier; //geometric multiplier
	double potential = 0;

	alpha = system->ewald_alpha;
	kmax = system->ewald_kmax;

	// perform the fourier sum over a hemisphere (skipping certain points to avoid overcounting the face) 
	for(l[0] = 0; l[0] <= kmax; l[0]++) {
		for(l[1] = (!l[0] ? 0 : -kmax); l[1] <= kmax; l[1]++) {
			for(l[2] = ((!l[0] && !l[1]) ? 1 : -kmax); l[2] <= kmax; l[2]++) {

				//if norm is out of the sphere, skip
				if (iidotprod(l,l) > kmax*kmax) continue;

				/* get the reciprocal lattice vectors */
				for(p = 0; p < 3; p++) 
					k[p] = 2.0*M_PI*didotprod(system->pbc->reciprocal_basis[p],l);
				k_squared = dddotprod(k,k);

				/* structure factor */
				SF_re = 0; SF_im = 0;
				for(molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next) {
					for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next) {

						if(atom_ptr->frozen) continue; //skip frozen
						if(atom_ptr->charge == 0.0) continue; //skip if no charge

						/* the inner product of the position vector and the k vector */
						position_product = dddotprod(k, atom_ptr->pos);

						SF_re += atom_ptr->charge*cos(position_product);
						SF_im += atom_ptr->charge*sin(position_product);

					} /* atom */
				} /* molecule */

				potential += exp(-k_squared/(4.0*alpha*alpha))/k_squared*(SF_re*SF_re + SF_im*SF_im);

			} /* end for n */
		} /* end for m */
	} /* end for l */

	potential *= 4.0*M_PI/system->pbc->volume;

	return(potential);
}

double coulombic_self(system_t *system) {

	molecule_t * molecule_ptr;
	atom_t * atom_ptr;
	double alpha = system->ewald_alpha;
	double self_potential = 0.0;
	
	for(molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next) {
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next) {
			if ( atom_ptr->frozen ) continue;
			atom_ptr->es_self_point_energy = alpha*atom_ptr->charge*atom_ptr->charge/sqrt(M_PI);
			self_potential -= atom_ptr->es_self_point_energy;
		}
	}

	return(self_potential);

}

/* feynman-hibbs for real space */
double coulombic_real_FH(molecule_t * molecule_ptr, pair_t * pair_ptr, double gaussian_term, double erfc_term, system_t * system) {

	double du, d2u, d3u, d4u; //derivatives of the pair term
	double fh_2nd_order, fh_4th_order;
	double r = pair_ptr->rimg;
	double rr = r*r;
	double rrr = rr*r;
	double order = system->feynman_hibbs_order;
	double alpha = system->ewald_alpha;
	double reduced_mass = AMU2KG*molecule_ptr->mass*pair_ptr->molecule->mass/(molecule_ptr->mass+pair_ptr->molecule->mass);

	du = -2.0*alpha*gaussian_term/(r*sqrt(M_PI)) - erfc_term/(rr);
	d2u = (4.0/sqrt(M_PI))*gaussian_term*(pow(alpha, 3.0) + pow(r, -2.0)) + 2.0*erfc_term/rrr;

	fh_2nd_order = pow(METER2ANGSTROM, 2.0)*(HBAR*HBAR/(24.0*KB*system->temperature*reduced_mass))*(d2u + 2.0*du/r);

	if(order >= 4) {

		d3u = (gaussian_term/sqrt(M_PI))*(-8.0*pow(alpha, 5.0)*r - 8.0*pow(alpha, 3.0)/r - 12.0*alpha/pow(r, 3.0)) 
			- 6.0*erfc(alpha*r)/pow(r, 4.0);
		d4u = (gaussian_term/sqrt(M_PI))*( 8.0*pow(alpha, 5.0) + 16.0*pow(alpha, 7.0)*r*r + 32.0*pow(alpha, 3.0)
			/pow(r, 2.0) + 48.0/pow(r, 4.0) ) + 24.0*erfc_term/pow(r, 5.0);

		fh_4th_order = pow(METER2ANGSTROM, 4.0)*(pow(HBAR, 4.0)/(1152.0*pow(KB*system->temperature*reduced_mass, 2.0)))
			*(15.0*du/pow(r, 3.0) + 4.0*d3u/r + d4u);
	}
	else fh_4th_order = 0.0;

	return fh_2nd_order + fh_4th_order;
}

/* real space sum */
double coulombic_real(system_t *system) {

	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	pair_t *pair_ptr;
	double alpha, r, erfc_term, gaussian_term;
	double potential, potential_classical;

	alpha = system->ewald_alpha;

	potential = 0;
	for(molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next) {
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next) {
			for(pair_ptr = atom_ptr->pairs; pair_ptr; pair_ptr = pair_ptr->next) {

				if(pair_ptr->recalculate_energy) {
					pair_ptr->es_real_energy = 0;

					if(!pair_ptr->frozen) {

						r = pair_ptr->rimg;
						if(!((r > system->pbc->cutoff) || pair_ptr->es_excluded)) {	/* unit cell part */

							//calculate potential contribution
							erfc_term = erfc(alpha*r);
							gaussian_term = exp(-alpha*alpha*r*r);
							potential_classical = atom_ptr->charge*pair_ptr->atom->charge*erfc_term/r;
							//store for pair pointer, so we don't always have to recalculate
							pair_ptr->es_real_energy += potential_classical;

							if(system->feynman_hibbs)
								pair_ptr->es_real_energy += coulombic_real_FH(molecule_ptr,pair_ptr,gaussian_term,erfc_term,system);		

						} else if(pair_ptr->es_excluded) /* calculate the charge-to-screen interaction */
							pair_ptr->es_self_intra_energy = atom_ptr->charge*pair_ptr->atom->charge*erf(alpha*pair_ptr->r)/pair_ptr->r;

					} /* frozen */

				} /* recalculate */

				/* sum all of the pairwise terms */
				potential += pair_ptr->es_real_energy - pair_ptr->es_self_intra_energy;

			} /* pair */
		} /* atom */
	} /* molecule */


	return(potential);

}

/* no ewald summation - regular accumulation of Coulombic terms without out consideration of PBC */
/* only used by surface module */
double coulombic_nopbc(molecule_t * molecules) {

	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	pair_t *pair_ptr;
	double pe, total_pe;

	total_pe = 0;
	for(molecule_ptr = molecules; molecule_ptr; molecule_ptr = molecule_ptr->next) {
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next) {
			for(pair_ptr = atom_ptr->pairs; pair_ptr; pair_ptr = pair_ptr->next) {
				if(!pair_ptr->es_excluded) {
					pe = atom_ptr->charge*pair_ptr->atom->charge/pair_ptr->r;
					total_pe += pe;
				}
			}
		}
	}

	return(total_pe);
}

double coulombic_wolf(system_t * system ) {

	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	pair_t *pair_ptr;
	double pot = 0;
	double erfc_term, gaussian_term;
	double alpha = system->ewald_alpha;
	double cutoff = system->pbc->cutoff;
	double erfcaRoverR = erfc(alpha*cutoff)/cutoff;
	double secondterm = erfcaRoverR/cutoff + 2*alpha/sqrt(M_PI)*exp(-alpha*alpha*cutoff*cutoff)/cutoff;
	
	double r;

	for(molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next) {
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next) {
			for(pair_ptr = atom_ptr->pairs; pair_ptr; pair_ptr = pair_ptr->next) {

				if ( pair_ptr->recalculate_energy ) {
					pair_ptr->es_real_energy = 0;

					r = pair_ptr->rimg;
					if( (!pair_ptr->frozen) && (!pair_ptr->es_excluded) && (r < cutoff) ) {
						erfc_term = erfc(alpha*r)/r;
						pair_ptr->es_real_energy = atom_ptr->charge * 
							pair_ptr->atom->charge * ( erfc_term - erfcaRoverR );
						// get feynman-hibbs contribution
						if(system->feynman_hibbs) {
							gaussian_term = exp(-alpha*alpha*r*r);
							pair_ptr->es_real_energy += coulombic_real_FH(molecule_ptr,pair_ptr,gaussian_term,erfc_term,system);
						} // FH
					}  // r<cutoff
					else if ( pair_ptr->es_excluded )
						pair_ptr->es_self_intra_energy = atom_ptr->charge*pair_ptr->atom->charge*erf(alpha*r)/r;

				} //recalculate
				pot += -pair_ptr->es_self_intra_energy + pair_ptr->es_real_energy;
			} //pair

			//self term
			if (!atom_ptr->frozen)
				pot -= atom_ptr->charge*atom_ptr->charge*(0.5*erfcaRoverR + alpha/sqrt(M_PI));
		} //atom
	} //molecule

	return(pot);
}
