/* 

@2007, Jonathan Belof
Space Research Group
Department of Chemistry
University of South Florida

*/

#include <mc.h>

double round_toward_zero ( double a) {
	if (a>0.) return floor(a);
	if (a<0.) return ceil(a);
	if (a==0.) return 0.0;
}

/* flag all pairs to have their energy calculated */
/* needs to be called at simulation start, or can */
/* be called to periodically keep the total energy */
/* from drifting */
void flag_all_pairs(system_t *system) {

	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	pair_t *pair_ptr;

	for(molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next)
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next)
			for(pair_ptr = atom_ptr->pairs; pair_ptr; pair_ptr = pair_ptr->next)
				pair_ptr->recalculate_energy = 1;


}

/* set the exclusions and LJ mixing for relevant pairs */
void pair_exclusions(system_t *system, molecule_t *molecule_i, molecule_t *molecule_j, atom_t *atom_i, atom_t *atom_j, pair_t *pair_ptr) {

	/* recalculate exclusions */
	if((molecule_i == molecule_j) && !system->gwp) { /* if both on same molecule, exclude all interactions */

		pair_ptr->rd_excluded = 1;
		pair_ptr->es_excluded = 1;

	} else {

		/* exclude null repulsion/dispersion interations */
		if((atom_i->epsilon == 0.0) || (atom_i->sigma == 0.0) || (atom_j->epsilon == 0.0) || (atom_j->sigma == 0.0))
			pair_ptr->rd_excluded = 1;
		else
			pair_ptr->rd_excluded = 0;

		/* exclude null electrostatic interactions */
		if((atom_i->charge == 0.0) || (atom_j->charge == 0.0))
			pair_ptr->es_excluded = 1;
		else
			pair_ptr->es_excluded = 0;

	}

	/* get the frozen interactions */
	pair_ptr->frozen = atom_i->frozen && atom_j->frozen;

	/* get the mixed LJ parameters */
	if(!system->sg) {

		/* Lorentz-Berthelot mixing rules */
		if((atom_i->sigma < 0.0) || (atom_j->sigma < 0.0)) {
			pair_ptr->attractive_only = 1;
			pair_ptr->sigma = 0.5*(fabs(atom_i->sigma) + fabs(atom_j->sigma));
		} else if ((atom_i->sigma == 0 || atom_j->sigma == 0 )) {
			pair_ptr->sigma = 0;
			pair_ptr->epsilon = sqrt(atom_i->epsilon*atom_j->epsilon);
		} else {
			pair_ptr->sigma = 0.5*(atom_i->sigma + atom_j->sigma);
			pair_ptr->epsilon = sqrt(atom_i->epsilon*atom_j->epsilon);
		}

	}

	/* ensure that no ES calc for S-S pairs, and ONLY ES for S-everythingelse */
	if(system->spectre) {

		if(atom_i->spectre && atom_j->spectre) { /* case for both spectre */

			pair_ptr->rd_excluded = 0;
			pair_ptr->es_excluded = 1;

		} else if(atom_i->spectre || atom_j->spectre) { /* case for one spectre */

			pair_ptr->rd_excluded = 1;
			pair_ptr->es_excluded = 0;

		}

	}

}

/* perform the modulo minimum image for displacements */
void minimum_image(system_t *system, atom_t *atom_i, atom_t *atom_j, pair_t *pair_ptr) {

	int p, q;
	double img[3];
	double d[3], r, r2;
	double di[3], ri, ri2;
	double rnd;

	/* get the real displacement */
	pair_ptr->recalculate_energy = 0;	/* reset the recalculate flag */
	for(p = 0; p < 3; p++) {
		d[p] = atom_i->pos[p] - atom_j->pos[p];

		/* this pair changed and so it will have it's energy recalculated */
		if( d[p] != pair_ptr->d_prev[p]) {
			pair_ptr->recalculate_energy = 1;
			pair_ptr->d_prev[p] = d[p]; /* reset */
		}
	}

	/* matrix multiply with the inverse basis and round */
	for(p = 0; p < 3; p++) {
		for(q = 0, img[p] = 0; q < 3; q++) {
			img[p] += system->pbc->reciprocal_basis[p][q]*d[q];
		}
		//img[p] = rint(img[p]);
		//this is fine unless we're exactly at 0.5 +/- rounding error (i.e. solids)
		//in the case, we need to round in a consistent manner
		if ( rint(img[p]+SMALL_dR) != rint(img[p]-SMALL_dR) ) //if adding a small dr would reverse the direction of rounding
			rnd = round_toward_zero(img[p]); //we force rounding toward zero
		else rnd = rint(img[p]); //round like normal
		img[p] = rnd;
	}

	/* matrix multiply to project back into our basis */
	for(p = 0; p < 3; p++)
		for(q = 0, di[p] = 0; q < 3; q++)
			di[p] += system->pbc->basis[p][q]*img[q];

	/* now correct the displacement */
	for(p = 0; p < 3; p++)
		di[p] = d[p] - di[p];

	/* pythagorean terms */
	for(p = 0, r2 = 0, ri2 = 0; p < 3; p++) {
		r2 += d[p]*d[p];
		ri2 += di[p]*di[p];
	}
	r = sqrt(r2);
	ri = sqrt(ri2);

	/* store the results for this pair */
	pair_ptr->r = r;

	if ( isnan(ri) != 0 ) pair_ptr->rimg = r;
	else pair_ptr->rimg = ri;

	for(p = 0; p < 3; p++) {
		if ( isnan(di[p]) != 0 ) pair_ptr->dimg[p] = d[p];
		else pair_ptr->dimg[p] = di[p];
	}

}


/* update everything necessary to describe the complete pairwise system */
void pairs(system_t *system) {

	int i, j, n;
	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	pair_t *pair_ptr;
	molecule_t **molecule_array;
	atom_t **atom_array;
	/* needed for GS ranking metric */
	int p;
	double r, rmin;

	/* generate an array of atom ptrs */
	for(molecule_ptr = system->molecules, n = 0, atom_array = NULL, molecule_array = NULL; molecule_ptr; molecule_ptr = molecule_ptr->next) {
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next, n++) {
			molecule_array = realloc(molecule_array, sizeof(molecule_t *)*(n + 1));
			memnullcheck(molecule_array,sizeof(molecule_t *)*(n + 1),0);
			molecule_array[n] = molecule_ptr;

			atom_array = realloc(atom_array, sizeof(atom_t *)*(n + 1));
			memnullcheck(atom_array, sizeof(atom_t *)*(n + 1), 1);
			atom_array[n] = atom_ptr;
		}
	}


	/* loop over all atoms and pair */
	for(i = 0; i < (n - 1); i++) {
		for(j = (i + 1), pair_ptr = atom_array[i]->pairs; j < n; j++, pair_ptr = pair_ptr->next) {

			/* set the link */
			pair_ptr->atom = atom_array[j];
			pair_ptr->molecule = molecule_array[j];

			/* set any necessary exclusions */
			/* frozen-frozen and self distances dont change */
			if(!(pair_ptr->frozen || (pair_ptr->rd_excluded && pair_ptr->es_excluded)))
				pair_exclusions(system, molecule_array[i], molecule_array[j], atom_array[i], atom_array[j], pair_ptr);

			/* recalc min image */
			if(!(pair_ptr->frozen))
				minimum_image(system, atom_array[i], atom_array[j], pair_ptr);

		} /* for j */
	} /* for i */


	/* update the com of each molecule */
	update_com(system->molecules);

	/* store wrapped coords */
	wrapall(system->molecules, system->pbc);

	/* rank metric */
	if(system->polar_iterative && system->polar_gs_ranked) {

		/* determine smallest polarizable separation */
		rmin = MAXVALUE;
		for(i = 0; i < n; i++) {

			for(j = 0; j < n; j++) {

				if((i != j) && (atom_array[i]->polarizability != 0.0) && (atom_array[j]->polarizability != 0.0) ) {
					for(p = 0, r = 0; p < 3; p++)
						r += pow( ( atom_array[i]->wrapped_pos[p] - atom_array[j]->wrapped_pos[p] ), 2.0);
					r = sqrt(r);

					if(r < rmin) rmin = r;
				}

			}

		}

		for(i = 0; i < n; i++) {

			atom_array[i]->rank_metric = 0;
			for(j = 0; j < n; j++) {

				if((i != j) && (atom_array[i]->polarizability != 0.0) && (atom_array[j]->polarizability != 0.0) ) {

					for(p = 0, r = 0; p < 3; p++)
						r += pow( ( atom_array[i]->wrapped_pos[p] - atom_array[j]->wrapped_pos[p] ), 2.0);
					r = sqrt(r);

					if(r <= 1.5*rmin)
						atom_array[i]->rank_metric += 1.0;

				}
			}
		}

	}


	/* free our temporary arrays */
	free(atom_array);
	free(molecule_array);


}

/* molecular center of mass */
void update_com(molecule_t *molecules) {

	int i;
	molecule_t *molecule_ptr;
	atom_t *atom_ptr;

	for(molecule_ptr = molecules; molecule_ptr; molecule_ptr = molecule_ptr->next) {

		for(i = 0; i < 3; i++)
			molecule_ptr->com[i] = 0;

		if(!(molecule_ptr->spectre || molecule_ptr->target)) {

			for(atom_ptr = molecule_ptr->atoms, molecule_ptr->mass = 0; atom_ptr; atom_ptr = atom_ptr->next) {

				molecule_ptr->mass += atom_ptr->mass;

				for(i = 0; i < 3; i++)
					molecule_ptr->com[i] += atom_ptr->mass*atom_ptr->pos[i];
			}

			for(i = 0; i < 3; i++)
				molecule_ptr->com[i] /= molecule_ptr->mass;

		}

	}


}

/* add new pairs for when a new molecule is created */
void update_pairs_insert(system_t *system) {

	int i, n;
	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	pair_t *pair_ptr;


	/* count the number of atoms per molecule */
	for(atom_ptr = system->checkpoint->molecule_altered->atoms, n = 0; atom_ptr; atom_ptr = atom_ptr->next, n++);

	/* add n number of pairs to altered and all molecules ahead of it in the list */
	for(molecule_ptr = system->molecules; molecule_ptr != system->checkpoint->tail; molecule_ptr = molecule_ptr->next) {
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next) {


			/* go to the end of the pair list */
			if(atom_ptr->pairs) {

				/* go to the end of the pair list */
				for(pair_ptr = atom_ptr->pairs; pair_ptr->next; pair_ptr = pair_ptr->next);

				/* tag on the extra pairs */
				for(i = 0; i < n; i++) {
					pair_ptr->next = calloc(1, sizeof(pair_t));
					memnullcheck(pair_ptr->next, sizeof(pair_t), 2);
					pair_ptr = pair_ptr->next;
				}

			} else {

				/* needs a new list */
				atom_ptr->pairs = calloc(1, sizeof(pair_t));
				memnullcheck(atom_ptr->pairs, sizeof(pair_t), 3);
				pair_ptr = atom_ptr->pairs;
				for(i = 0; i < (n - 1); i++) {
					pair_ptr->next = calloc(1, sizeof(pair_t));
					memnullcheck(pair_ptr->next, sizeof(pair_t), 4);
					pair_ptr = pair_ptr->next;
				}

			}

		} /* for atom */
	} /* for molecule */

}

/* remove pairs when a molecule is deleted */
void update_pairs_remove(system_t *system) {

	int i, n, m;
	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	pair_t *pair_ptr, **pair_array;


	/* count the number of atoms per molecule */
	for(atom_ptr = system->checkpoint->molecule_backup->atoms, n = 0; atom_ptr; atom_ptr = atom_ptr->next, n++);

	/* remove n number of pairs for all molecules ahead of the removal point */
	pair_array = calloc(1, sizeof(pair_t *));
	memnullcheck(pair_array,sizeof(pair_t *), 5);
	for(molecule_ptr = system->molecules; molecule_ptr != system->checkpoint->tail; molecule_ptr = molecule_ptr->next) {
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next) {

			/* build the pair pointer array */
			for(pair_ptr = atom_ptr->pairs, m = 0; pair_ptr; pair_ptr = pair_ptr->next, m++) {

				pair_array = realloc(pair_array, sizeof(pair_t *)*(m + 1));
				memnullcheck(pair_array,sizeof(pair_t *)*(m + 1),6);
				pair_array[m] = pair_ptr;

			}

			for(i = (m - n); i < m; i++)
				free(pair_array[i]);

			/* handle the end of the list */
			if((m - n) > 0)
				pair_array[(m - n - 1)]->next = NULL;
			else
				atom_ptr->pairs = NULL;

		}
	}

	/* free our temporary array */
	free(pair_array);

}

/* if an insert move is rejected, remove the pairs that were previously added */
void unupdate_pairs_insert(system_t *system) {

	int i, n, m;
	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	pair_t *pair_ptr, **pair_array;


	/* count the number of atoms per molecule */
	for(atom_ptr = system->checkpoint->molecule_altered->atoms, n = 0; atom_ptr; atom_ptr = atom_ptr->next)
		++n;

	/* remove n number of pairs for all molecules ahead of the removal point */
	pair_array = calloc(1, sizeof(pair_t *));
	memnullcheck(pair_array,sizeof(pair_t *),7);
	for(molecule_ptr = system->molecules; molecule_ptr != system->checkpoint->tail; molecule_ptr = molecule_ptr->next) {
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next) {

			/* build the pair pointer array */
			for(pair_ptr = atom_ptr->pairs, m = 0; pair_ptr; pair_ptr = pair_ptr->next, m++) {

				pair_array = realloc(pair_array, sizeof(pair_t *)*(m + 1));
				memnullcheck(pair_array,sizeof(pair_t *)*(m + 1),7);
				pair_array[m] = pair_ptr;

			}

			for(i = (m - n); i < m; i++)
				free(pair_array[i]);

			/* handle the end of the list */
			if((m - n) > 0)
				pair_array[(m - n - 1)]->next = NULL;
			else
				atom_ptr->pairs = NULL;

		}
	}

	/* free our temporary array */
	free(pair_array);

}

/* if a remove is rejected, then add back the pairs that were previously deleted */
void unupdate_pairs_remove(system_t *system) {

	int i, n;
	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	pair_t *pair_ptr;


	/* count the number of atoms per molecule */
	for(atom_ptr = system->checkpoint->molecule_backup->atoms, n = 0; atom_ptr; atom_ptr = atom_ptr->next)
		++n;

	/* add n number of pairs to altered and all molecules ahead of it in the list */
	for(molecule_ptr = system->molecules; molecule_ptr != system->checkpoint->molecule_backup; molecule_ptr = molecule_ptr->next) {
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next) {


			/* go to the end of the pair list */
			if(atom_ptr->pairs) {

				/* go to the end of the pair list */
				for(pair_ptr = atom_ptr->pairs; pair_ptr->next; pair_ptr = pair_ptr->next);

				/* tag on the extra pairs */
				for(i = 0; i < n; i++) {
					pair_ptr->next = calloc(1, sizeof(pair_t));
					memnullcheck(pair_ptr->next,sizeof(pair_t),8);
					pair_ptr = pair_ptr->next;
				}

			} else {

				/* needs a new list */
				atom_ptr->pairs = calloc(1, sizeof(pair_t));
				memnullcheck(atom_ptr->pairs,sizeof(pair_t),9);
				pair_ptr = atom_ptr->pairs;
				for(i = 0; i < (n - 1); i++) {
					pair_ptr->next = calloc(1, sizeof(pair_t));
					memnullcheck(pair_ptr->next,sizeof(pair_t),10);
					pair_ptr = pair_ptr->next;
				}

			}

		} /* for atom */
	} /* for molecule */


}

/* allocate the pair lists */
void setup_pairs(molecule_t *molecules) {

	int i, j, n;
	molecule_t *molecule_ptr, **molecule_array;
	atom_t *atom_ptr, **atom_array;
	pair_t *pair_ptr, *prev_pair_ptr;

	/* generate an array of atom ptrs */
	for(molecule_ptr = molecules, n = 0, atom_array = NULL, molecule_array = NULL; molecule_ptr; molecule_ptr = molecule_ptr->next) {
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next) {

			atom_array = realloc(atom_array, sizeof(atom_t *)*(n + 1));
			memnullcheck(atom_array,sizeof(atom_t *)*(n + 1), 11);
			atom_array[n] = atom_ptr;

			molecule_array = realloc(molecule_array, sizeof(molecule_t *)*(n + 1));
			memnullcheck(molecule_array,sizeof(molecule_t *)*(n + 1), 12);
			molecule_array[n] = molecule_ptr;

			++n;

		}
	}

	/* setup the pairs, lower triangular */
	for(i = 0; i < (n - 1); i++) {

		atom_array[i]->pairs = calloc(1, sizeof(pair_t));
		memnullcheck(atom_array[i]->pairs,sizeof(pair_t),13);	
		pair_ptr = atom_array[i]->pairs;
		prev_pair_ptr = pair_ptr;

		for(j = (i + 1); j < n; j++) {

			pair_ptr->next = calloc(1, sizeof(pair_t));
			memnullcheck(pair_ptr->next,sizeof(pair_t),14);
			prev_pair_ptr = pair_ptr;
			pair_ptr = pair_ptr->next;

		}

		prev_pair_ptr->next = NULL;
		free(pair_ptr);

	}

	free(atom_array);
	free(molecule_array);


}

#ifdef DEBUG
void test_pairs(molecule_t *molecules) {

	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	pair_t *pair_ptr;

	for(molecule_ptr = molecules; molecule_ptr; molecule_ptr = molecule_ptr->next) {
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next) {
			for(pair_ptr = atom_ptr->pairs; pair_ptr; pair_ptr = pair_ptr->next) {

				if(!(pair_ptr->frozen || pair_ptr->rd_excluded || pair_ptr->es_excluded)) printf("%d: charge = %f, epsilon = %f, sigma = %f, r = %f, rimg = %f\n", atom_ptr->id, pair_ptr->atom->charge, pair_ptr->epsilon, pair_ptr->sigma, pair_ptr->r, pair_ptr->rimg);fflush(stdout);

			}
		}
	}

}
#endif
