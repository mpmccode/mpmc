/*

@2007, Jonathan Belof
Space Research Group
Department of Chemistry
University of South Florida

*/

/* herein lie sleeping demons */
/* i truly hate this fucking file, but dont see a better way JB */

#include <mc.h>

/* scale the volume : used in NPT ensemble */
void volume_change( system_t * system ) {
	molecule_t * m;
	atom_t * a;
	double new_volume, log_new_volume, basis_scale_factor;
	double new_com[3], old_com[3], delta_pos[3];
	int i,j;

	// figure out what the new volume will be
	log_new_volume=log(system->pbc->volume) + (get_rand()-0.5)*system->volume_change_factor;
	new_volume=exp(log_new_volume);

	//scale basis
	basis_scale_factor = pow(new_volume/system->pbc->volume,1.0/3.0);
	for ( i=0; i<3; i++ )
		for ( j=0; j<3; j++ )
			system->pbc->basis[i][j] *= basis_scale_factor;

	//recalculate PBC stuff (volume/reciprocal basis/cutoff)
	pbc(system);
	system->observables->volume = system->pbc->volume;

	//scale molecule positions
	for ( m = system->molecules; m; m = m->next ) {
		for ( i=0; i<3; i++ ) {
			old_com[i] = m->com[i]; //molecule ptr's com will be udated during pairs routine
			new_com[i] = m->com[i] * basis_scale_factor;
			delta_pos[i] = new_com[i] - old_com[i];
		}
		for ( a = m->atoms; a; a=a->next ) { //calculate new atomic positions based on new com
			for (i=0; i<3; i++ ) {
				a->pos[i] += delta_pos[i];
				a->wrapped_pos[i] += delta_pos[i];
			}
		}
	}		

	return;
}

/* revert changes to volume */
void revert_volume_change( system_t * system ) {
	double basis_scale_factor, new_volume;
	double old_com[3], new_com[3], delta_pos[3];
	molecule_t * m; atom_t * a;
	int i,j;
	new_volume = system->checkpoint->observables->volume; 

	//un-scale basis
	basis_scale_factor = pow(new_volume/system->pbc->volume,1.0/3.0); //system->pbc->volume still has rejected value until pbc() is called
	for ( i=0; i<3; i++ )
		for ( j=0; j<3; j++ )
			system->pbc->basis[i][j] *= basis_scale_factor;

	//recalc PBC stuff
	pbc(system);
	system->observables->volume = system->pbc->volume;

	//scale molecule positions
	for ( m = system->molecules; m; m = m->next ) {
		for ( i=0; i<3; i++ ) {
			old_com[i] = m->com[i]; //molecule ptr's com will ned to be updated since this is a revert (no energy call following)
			new_com[i] = m->com[i] * basis_scale_factor;
			m->com[i] = new_com[i];
			delta_pos[i] = new_com[i] - old_com[i];
		}
		for ( a = m->atoms; a; a=a->next ) { //calculate new atomic positions based on new com
			for (i=0; i<3; i++ ) {
				a->pos[i] += delta_pos[i];
				a->wrapped_pos[i] += delta_pos[i];
			}
		}
	}

	return;
}


/* make an exact copy of src */
molecule_t *copy_molecule(system_t *system, molecule_t *src) {

	int i, j;
	molecule_t *dst;
	atom_t *atom_dst_ptr, *prev_atom_dst_ptr, *atom_src_ptr;
	pair_t *pair_dst_ptr, *prev_pair_dst_ptr, *pair_src_ptr;

	/* allocate the start of the new lists */
	dst = calloc(1, sizeof(molecule_t));
	memnullcheck(dst,sizeof(molecule_t),__LINE__-1, __FILE__);
	/* copy molecule attributes */
	dst->id = src->id;
	strcpy(dst->moleculetype, src->moleculetype);
	dst->mass = src->mass;
	dst->frozen = src->frozen;
	dst->adiabatic = src->adiabatic;
	dst->spectre = src->spectre;
	dst->target = src->target;
	dst->nuclear_spin = src->nuclear_spin;
	dst->rot_partfunc_g = src->rot_partfunc_g;
	dst->rot_partfunc_u = src->rot_partfunc_u;
	dst->rot_partfunc = src->rot_partfunc;
	memcpy(dst->com, src->com, 3*sizeof(double));
	memcpy(dst->wrapped_com, src->wrapped_com, 3*sizeof(double));

#ifdef QM_ROTATION
	if(system->quantum_rotation) {
		dst->quantum_rotational_energies = calloc(system->quantum_rotation_level_max, sizeof(double));
		memnullcheck(dst->quantum_rotational_energies, system->quantum_rotation_level_max*sizeof(double),__LINE__-1, __FILE__);
		dst->quantum_rotational_eigenvectors = calloc(system->quantum_rotation_level_max, sizeof(complex_t *));
		memnullcheck(dst->quantum_rotational_eigenvectors,system->quantum_rotation_level_max*sizeof(complex_t *),__LINE__-1, __FILE__);
		for(i = 0; i < system->quantum_rotation_level_max; i++) {
			dst->quantum_rotational_eigenvectors[i] = calloc((system->quantum_rotation_l_max + 1)*(system->quantum_rotation_l_max + 1), sizeof(complex_t));
			memnullcheck(dst->quantum_rotational_eigenvectors[i],(system->quantum_rotation_l_max+1)*(system->quantum_rotation_l_max+1)*sizeof(complex_t),__LINE__-1, __FILE__);
		}
		dst->quantum_rotational_eigensymmetry = calloc(system->quantum_rotation_level_max, sizeof(int));
		memnullcheck(dst->quantum_rotational_eigensymmetry,system->quantum_rotation_level_max*sizeof(int),__LINE__-1, __FILE__);

		memcpy(dst->quantum_rotational_energies, src->quantum_rotational_energies, system->quantum_rotation_level_max*sizeof(double));
		for(i = 0; i < system->quantum_rotation_level_max; i++) {
			for(j = 0; j < (system->quantum_rotation_l_max + 1)*(system->quantum_rotation_l_max + 1); j++) {
				dst->quantum_rotational_eigenvectors[i][j].real = src->quantum_rotational_eigenvectors[i][j].real;
				dst->quantum_rotational_eigenvectors[i][j].imaginary = src->quantum_rotational_eigenvectors[i][j].imaginary;
			}
		}
		memcpy(dst->quantum_rotational_eigensymmetry, src->quantum_rotational_eigensymmetry, system->quantum_rotation_level_max*sizeof(int));
	}
#endif /* QM_ROTATION */

	dst->next = NULL;



	/* new atoms list */
	dst->atoms = calloc(1, sizeof(atom_t));
	memnullcheck(dst->atoms,sizeof(atom_t),__LINE__-1, __FILE__);
	prev_atom_dst_ptr = dst->atoms;

	for(atom_dst_ptr = dst->atoms, atom_src_ptr = src->atoms; atom_src_ptr; atom_dst_ptr = atom_dst_ptr->next, atom_src_ptr = atom_src_ptr->next) {

		atom_dst_ptr->id = atom_src_ptr->id;
		strcpy(atom_dst_ptr->atomtype, atom_src_ptr->atomtype);
		atom_dst_ptr->frozen = atom_src_ptr->frozen;
		atom_dst_ptr->adiabatic = atom_src_ptr->adiabatic;
		atom_dst_ptr->spectre = atom_src_ptr->spectre;
		atom_dst_ptr->target = atom_src_ptr->target;
		atom_dst_ptr->mass = atom_src_ptr->mass;
		atom_dst_ptr->charge = atom_src_ptr->charge;
		atom_dst_ptr->gwp_alpha = atom_src_ptr->gwp_alpha;
		atom_dst_ptr->gwp_spin = atom_src_ptr->gwp_spin;
		atom_dst_ptr->polarizability = atom_src_ptr->polarizability;
		atom_dst_ptr->omega = atom_src_ptr->omega;
		atom_dst_ptr->epsilon = atom_src_ptr->epsilon;
		atom_dst_ptr->sigma = atom_src_ptr->sigma;
		atom_dst_ptr->gwp_spin = atom_src_ptr->gwp_spin;

		memcpy(atom_dst_ptr->pos, atom_src_ptr->pos, 3*sizeof(double));
		memcpy(atom_dst_ptr->wrapped_pos, atom_src_ptr->wrapped_pos, 3*sizeof(double));
		memcpy(atom_dst_ptr->ef_static, atom_src_ptr->ef_static, 3*sizeof(double));
		memcpy(atom_dst_ptr->ef_induced, atom_src_ptr->ef_induced, 3*sizeof(double));
		memcpy(atom_dst_ptr->mu, atom_src_ptr->mu, 3*sizeof(double));
		memcpy(atom_dst_ptr->old_mu, atom_src_ptr->old_mu, 3*sizeof(double));
		memcpy(atom_dst_ptr->new_mu, atom_src_ptr->new_mu, 3*sizeof(double));

		atom_dst_ptr->pairs = calloc(1, sizeof(pair_t));
		memnullcheck(atom_dst_ptr->pairs,sizeof(pair_t),__LINE__-1, __FILE__);
		pair_dst_ptr = atom_dst_ptr->pairs;
		prev_pair_dst_ptr = pair_dst_ptr;
		for(pair_src_ptr = atom_src_ptr->pairs; pair_src_ptr; pair_src_ptr = pair_src_ptr->next) {

			pair_dst_ptr->rd_energy = pair_src_ptr->rd_energy;
			pair_dst_ptr->es_real_energy = pair_src_ptr->es_real_energy;
			pair_dst_ptr->es_self_intra_energy = pair_src_ptr->es_self_intra_energy;


			pair_dst_ptr->frozen = pair_src_ptr->frozen;
			pair_dst_ptr->rd_excluded = pair_src_ptr->rd_excluded;
			pair_dst_ptr->es_excluded = pair_src_ptr->es_excluded;
//			pair_dst_ptr->charge = pair_src_ptr->charge;
//			pair_dst_ptr->gwp_alpha = pair_src_ptr->gwp_alpha;
//			pair_dst_ptr->gwp_spin = pair_src_ptr->gwp_spin;
			pair_dst_ptr->epsilon = pair_src_ptr->epsilon;
			pair_dst_ptr->lrc = pair_src_ptr->lrc;
			pair_dst_ptr->sigma = pair_src_ptr->sigma;
			pair_dst_ptr->r = pair_src_ptr->r;
			pair_dst_ptr->rimg = pair_src_ptr->rimg;

			pair_dst_ptr->next = calloc(1, sizeof(pair_t));
			memnullcheck(pair_dst_ptr->next,sizeof(pair_t),__LINE__-1, __FILE__);
			prev_pair_dst_ptr = pair_dst_ptr;
			pair_dst_ptr = pair_dst_ptr->next;

		}
		prev_pair_dst_ptr->next = NULL;
		free(pair_dst_ptr);
		/* handle an empty list */
		if(!atom_src_ptr->pairs) atom_dst_ptr->pairs = NULL;

		prev_atom_dst_ptr = atom_dst_ptr;
		atom_dst_ptr->next = calloc(1, sizeof(atom_t));
		memnullcheck(atom_dst_ptr->next,sizeof(atom_t),__LINE__-1, __FILE__);
	}

	prev_atom_dst_ptr->next = NULL;
	free(atom_dst_ptr);

	return(dst);

}


/* perform a general random translation */
void translate(molecule_t *molecule, pbc_t *pbc, double scale) {

	atom_t *atom_ptr;
	double trans_x, trans_y, trans_z;
	double boxlength;

	trans_x = scale*get_rand()*pbc->cutoff;
	trans_y = scale*get_rand()*pbc->cutoff;
	trans_z = scale*get_rand()*pbc->cutoff;
	if(get_rand() < 0.5) trans_x *= -1.0;
	if(get_rand() < 0.5) trans_y *= -1.0;
	if(get_rand() < 0.5) trans_z *= -1.0;

	molecule->com[0] += trans_x;
	molecule->com[1] += trans_y;
	molecule->com[2] += trans_z;

	for(atom_ptr = molecule->atoms; atom_ptr; atom_ptr = atom_ptr->next) {
		atom_ptr->pos[0] += trans_x;
		atom_ptr->pos[1] += trans_y;
		atom_ptr->pos[2] += trans_z;
	}

}

/* perform a general random rotation */
void rotate(molecule_t *molecule, pbc_t *pbc, double scale) {

	atom_t *atom_ptr;
	double alpha, beta, gamma;	/* Euler angles */
	double rotation_matrix[3][3];
	double com[3];
	int i, ii, n;
	double *new_coord_array;

	/* get the randomized Euler angles */
	alpha = scale*get_rand()*2.0*M_PI; if(get_rand() < 0.5) alpha *= -1.0;
        beta = scale*get_rand()*M_PI; if(get_rand() < 0.5) beta *= -1.0;
        gamma = scale*get_rand()*2.0*M_PI; if(get_rand() < 0.5) gamma *= -1.0;

	/* count the number of atoms in a molecule, and allocate new coords array */
	for(atom_ptr = molecule->atoms, n = 0; atom_ptr; atom_ptr = atom_ptr->next)
		++n;
	new_coord_array = calloc(n*3, sizeof(double));
	memnullcheck(new_coord_array,n*3*sizeof(double),__LINE__-1, __FILE__);

	/* save the com coordinate */
	com[0] = molecule->com[0];
	com[1] = molecule->com[1];
	com[2] = molecule->com[2];

	/* translate the molecule to the origin */
	for(atom_ptr = molecule->atoms; atom_ptr; atom_ptr = atom_ptr->next) {
		atom_ptr->pos[0] -= com[0];
		atom_ptr->pos[1] -= com[1];
		atom_ptr->pos[2] -= com[2];
	}

	/* construct the 3D rotation matrix */
	rotation_matrix[0][0] = cos(alpha)*cos(beta)*cos(gamma) - sin(alpha)*sin(gamma);
	rotation_matrix[0][1] = sin(alpha)*cos(beta)*cos(gamma) + cos(alpha)*sin(gamma);
	rotation_matrix[0][2] = -sin(beta)*cos(gamma);
	rotation_matrix[1][0] = -cos(alpha)*cos(beta)*sin(gamma) - sin(alpha)*cos(gamma);
	rotation_matrix[1][1] = -sin(alpha)*cos(beta)*sin(gamma) + cos(alpha)*cos(gamma);
	rotation_matrix[1][2] = sin(beta)*sin(gamma);
	rotation_matrix[2][0] = cos(alpha)*sin(beta);
	rotation_matrix[2][1] = sin(alpha)*sin(beta);
	rotation_matrix[2][2] = cos(beta);

	/* matrix multiply */
	for(atom_ptr = molecule->atoms, i = 0; atom_ptr; atom_ptr = atom_ptr->next, i++) {

		ii = i*3;
		new_coord_array[ii+0] = rotation_matrix[0][0]*atom_ptr->pos[0] + rotation_matrix[0][1]*atom_ptr->pos[1] + rotation_matrix[0][2]*atom_ptr->pos[2];
		new_coord_array[ii+1] = rotation_matrix[1][0]*atom_ptr->pos[0] + rotation_matrix[1][1]*atom_ptr->pos[1] + rotation_matrix[1][2]*atom_ptr->pos[2];
		new_coord_array[ii+2] = rotation_matrix[2][0]*atom_ptr->pos[0] + rotation_matrix[2][1]*atom_ptr->pos[1] + rotation_matrix[2][2]*atom_ptr->pos[2];

	}

	/* set the new coordinates and then translate back from the origin */
	for(atom_ptr = molecule->atoms, i = 0; atom_ptr; atom_ptr = atom_ptr->next, i++) {

		ii = i*3;
		atom_ptr->pos[0] = new_coord_array[ii+0];
		atom_ptr->pos[1] = new_coord_array[ii+1];
		atom_ptr->pos[2] = new_coord_array[ii+2];

		atom_ptr->pos[0] += com[0];
		atom_ptr->pos[1] += com[1];
		atom_ptr->pos[2] += com[2];

	}

	/* free our temporary array */
	free(new_coord_array);

}

/* perform a 1D translation without periodic boundaries */
void displace_1D(system_t *system, molecule_t *molecule, double scale) {

	atom_t *atom_ptr;
	double trans;

	trans = scale*get_rand();
	if(get_rand() < 0.5) trans *= -1.0;
	for(atom_ptr = molecule->atoms; atom_ptr; atom_ptr = atom_ptr->next)
		atom_ptr->pos[0] += trans;

	molecule->com[0] += trans;

}

/* perform a random translation/rotation of a molecule */
void displace(molecule_t *molecule, pbc_t *pbc, double trans_scale, double rot_scale) {

	translate(molecule, pbc, trans_scale);
	rotate(molecule, pbc, rot_scale);

}

/* perform a perturbation to a gaussian width */
void displace_gwp(molecule_t *molecule, double scale) {

        atom_t *atom_ptr;
	double perturb;

	for(atom_ptr = molecule->atoms; atom_ptr; atom_ptr = atom_ptr->next) {

		if(atom_ptr->gwp_spin) {

			perturb = scale*(get_rand() - 0.5);
			atom_ptr->gwp_alpha += perturb;

			/* make sure the width remains positive */
			atom_ptr->gwp_alpha = fabs(atom_ptr->gwp_alpha);

		}

	}


}

/* perform a random translation/rotation of a molecule for SPECTRE algorithm */
void spectre_displace(system_t *system, molecule_t *molecule, double trans_scale, double max_charge, double max_target) {

	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	int p;
	double target[3], trans[3];
	double delta_charge;

	/* get the coordinates of the target particle */
	for(molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next) {

		if(molecule_ptr->target) {
			for(p = 0; p < 3; p++)
				target[p] = molecule_ptr->atoms->pos[p];
		}

	}

	/* randomly translate */
	for(p = 0; p < 3; p++)
		trans[p] = trans_scale*get_rand()*max_target; if(get_rand() < 0.5) trans[p] *= -1.0;

	for(atom_ptr = molecule->atoms; atom_ptr; atom_ptr = atom_ptr->next) {

		/* position reassignment */
		for(p = 0; p < 3; p++)
			atom_ptr->pos[p] += trans[p];

		/* charge reassignment */
		do {
			delta_charge = get_rand(); if(get_rand() < 0.5) delta_charge *= -1.0;
		} while (fabs(atom_ptr->charge + delta_charge) > max_charge);
		atom_ptr->charge += delta_charge;


	}

	/* restrict the SPECTRE charges to the domain */
	spectre_wrapall(system);

	/* renormalize all SPECTRE charges */
	spectre_charge_renormalize(system);

}

/* ensure neutrality for the SPECTRE system */
void spectre_charge_renormalize(system_t *system) {

	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	int num_spectre;
	double residual_charge, frac_charge;

	/* calculate the net charge */
	num_spectre = 0; residual_charge = 0;
	for(molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next) {
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next) {

			if(atom_ptr->spectre) {
				++num_spectre;
				residual_charge += atom_ptr->charge;
			}

		}
	}

	/* spread that charge out among the other SPECTRE sites*/
	frac_charge = -1.0*residual_charge/((double)num_spectre);

	for(molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next)
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next)
			if(atom_ptr->spectre) atom_ptr->charge += frac_charge;

}


/* apply what was already determined in checkpointing */
void make_move(system_t *system) {

	int i, j, k, p, q;
	cavity_t *cavities_array;
	int cavities_array_counter, random_index;
	double com[3], rand[3];
	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	pair_t *pair_ptr;

	/* update the cavity grid prior to making a move */
	if(system->cavity_bias) {
		cavity_update_grid(system);
		system->checkpoint->biased_move = 0;
	}

	switch ( system->checkpoint->movetype ) {

		case MOVETYPE_INSERT :  /* insert a molecule at a random pos and orientation */
			/* umbrella sampling */
			if(system->cavity_bias && system->cavities_open) {
				/* doing a biased move - this flag lets mc.c know about it */
				system->checkpoint->biased_move = 1;
				/* make an array of possible insertion points */
				cavities_array = calloc(system->cavities_open, sizeof(cavity_t));
				memnullcheck(cavities_array,system->cavities_open*sizeof(cavity_t),__LINE__-1, __FILE__);
				for(i = 0, cavities_array_counter = 0; i < system->cavity_grid_size; i++) {
					for(j = 0; j < system->cavity_grid_size; j++) {
						for(k = 0; k < system->cavity_grid_size; k++) {
							if(!system->cavity_grid[i][j][k].occupancy) {
								for(p = 0; p < 3; p++)
									cavities_array[cavities_array_counter].pos[p] = system->cavity_grid[i][j][k].pos[p];
								++cavities_array_counter;
							}
						} /* end k */
					} /* end j */
				} /* end i */
				/* insert randomly at one of the free cavity points */
				random_index = (system->cavities_open - 1) - (int)rint(((double)(system->cavities_open - 1))*get_rand());
				for(p = 0; p < 3; p++)
					com[p] = cavities_array[random_index].pos[p];
				/* free the insertion array */
				free(cavities_array);
			} // end umbrella

			else {
				/* insert the molecule to a random location within the unit cell */
				for(p = 0; p < 3; p++)
					rand[p] = 0.5 - get_rand();
				for(p = 0; p < 3; p++)
					for(q = 0, com[p] = 0; q < 3; q++)
						com[p] += system->pbc->basis[p][q]*rand[q];
			}

			/* process the inserted molecule */
			for(atom_ptr = system->checkpoint->molecule_backup->atoms; atom_ptr; atom_ptr = atom_ptr->next) {
				/* move the molecule back to the origin and then assign it to com */
				for(p = 0; p < 3; p++)
					atom_ptr->pos[p] += com[p] - system->checkpoint->molecule_backup->com[p];
			}

			/* update the molecular com */
			for(p = 0; p < 3; p++)
				system->checkpoint->molecule_backup->com[p] = com[p];
			/* give it a random orientation */
			rotate(system->checkpoint->molecule_backup, system->pbc, 1.0);

			// insert into the list 
			if( system->num_insertion_molecules ) {
				// If inserting a molecule from an insertion list, we will always insert at the end
				system->checkpoint->head->next = system->checkpoint->molecule_backup;
				system->checkpoint->molecule_backup->next = NULL;
			} else {
				if(!system->checkpoint->head) {      // if we're at the start of the list:
					system->molecules = system->checkpoint->molecule_backup;		
				} else {
					system->checkpoint->head->next = system->checkpoint->molecule_backup;
				}
				system->checkpoint->molecule_backup->next = system->checkpoint->molecule_altered;
			}

			/* set new altered and tail to reflect the insertion */
			system->checkpoint->molecule_altered = system->checkpoint->molecule_backup;
			system->checkpoint->tail = system->checkpoint->molecule_altered->next;
			system->checkpoint->molecule_backup = NULL;

			if( system->num_insertion_molecules ) { //multi sorbate
				// Free all pair memory in the list
				for( molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next ) {
					for( atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next ) {
						pair_ptr = atom_ptr->pairs;
						while( pair_ptr ) {
							pair_t *temp = pair_ptr;
							pair_ptr = pair_ptr->next;
							free( temp );
						}
					}
				}
				// Generate new pairs lists for all atoms in system
				setup_pairs( system );
			} // only one sorbate
			else		
				update_pairs_insert(system);

		break;
		case MOVETYPE_REMOVE : /* remove a randomly chosen molecule */
	
			if(system->cavity_bias) {
				if(get_rand() < pow((1.0 - system->avg_observables->cavity_bias_probability), 
					((double)system->cavity_grid_size*system->cavity_grid_size*system->cavity_grid_size)))
					system->checkpoint->biased_move = 0;
				else
					system->checkpoint->biased_move = 1;
			}
	
			/* remove 'altered' from the list */
			if(!system->checkpoint->head) {	/* handle the case where we're removing from the start of the list */
				system->checkpoint->molecule_altered = system->molecules;
				system->molecules = system->molecules->next;
			} else {
				system->checkpoint->head->next = system->checkpoint->tail;
			}
			free_molecule(system, system->checkpoint->molecule_altered);
			update_pairs_remove(system);

		break;
		case MOVETYPE_DISPLACE :	
	
			/* change coords of 'altered' */
			if(system->rd_anharmonic)
				displace_1D(system, system->checkpoint->molecule_altered, system->move_probability);
			else if(system->spectre)
				spectre_displace(system, system->checkpoint->molecule_altered, system->move_probability, 
					system->spectre_max_charge, system->spectre_max_target);
			else if(system->gwp) {
				if(system->checkpoint->molecule_altered->atoms->gwp_spin) {
					displace(system->checkpoint->molecule_altered, system->pbc, system->gwp_probability, system->rot_probability);
					displace_gwp(system->checkpoint->molecule_altered, system->gwp_probability);
				} else
					displace(system->checkpoint->molecule_altered, system->pbc, system->move_probability, system->rot_probability);
			} else
				displace(system->checkpoint->molecule_altered, system->pbc, system->move_probability, system->rot_probability);

		break;
		case MOVETYPE_ADIABATIC :
			/* change coords of 'altered' */
			displace(system->checkpoint->molecule_altered, system->pbc, system->adiabatic_probability, 1.0);
	
		break;
		case MOVETYPE_SPINFLIP :
	
			/* XXX - should have separate function do spinfip move */
			if(system->checkpoint->molecule_altered->nuclear_spin == NUCLEAR_SPIN_PARA)
				system->checkpoint->molecule_altered->nuclear_spin = NUCLEAR_SPIN_ORTHO;
			else
				system->checkpoint->molecule_altered->nuclear_spin = NUCLEAR_SPIN_PARA;
	
		break;
		case MOVETYPE_VOLUME :
	
			volume_change(system); // I don't want to contribute to the god damned mess -- kmclaugh

		break;
		default:
			error("MC_MOVES: invaid mc move\n");
			exit(-1);
	}

	return;
}

/* this function (a) undoes what make_move() did and (b) determines the next move sequence by calling checkpoint() */
void restore(system_t *system) {

	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	pair_t *prev_pair_ptr, *pair_ptr;

	/* restore the remaining observables */
	memcpy(system->observables, system->checkpoint->observables, sizeof(observables_t));

	/* restore state by undoing the steps of make_move() */
	switch ( system->checkpoint->movetype ) {

		case MOVETYPE_INSERT : 
	
			/* take altered out of the list */
			if(!system->checkpoint->head) {	/* handle the case where we inserted at the head of the list */
				system->molecules = system->molecules->next;
			} else {
				system->checkpoint->head->next = system->checkpoint->tail;
			}
			unupdate_pairs_insert(system);
			free_molecule(system, system->checkpoint->molecule_altered);

		break;
		case MOVETYPE_REMOVE :
	
			/* put backup back into the list */
			if(!system->checkpoint->head) {
				system->molecules = system->checkpoint->molecule_backup;
			} else {
				system->checkpoint->head->next = system->checkpoint->molecule_backup;
			}
			system->checkpoint->molecule_backup->next = system->checkpoint->tail;
			unupdate_pairs_remove(system);
			system->checkpoint->molecule_backup = NULL;

		break;
		case MOVETYPE_VOLUME :
	
			revert_volume_change(system);

		break;
		default :

			/* link the backup into the working list again */
			if(!system->checkpoint->head)
				system->molecules = system->checkpoint->molecule_backup;
			else
				system->checkpoint->head->next = system->checkpoint->molecule_backup;
			system->checkpoint->molecule_backup->next = system->checkpoint->tail;
			free_molecule(system, system->checkpoint->molecule_altered);
			system->checkpoint->molecule_backup = NULL;

	}	
	
	/* renormalize charges */
	if(system->spectre) spectre_charge_renormalize(system);

	/* establish the previous checkpoint again */
	checkpoint(system);

	return;
}

