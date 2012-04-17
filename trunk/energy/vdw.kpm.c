/*

Keith McLaughlin, 2011
University of South Florida

Calculation of VDW using coupled dipole. A_matrix is already obtained
via polar.c. Cholesky decomposition is performed on generalized
eigenvalue problem A.u=omega^2 K.u to convert to standard eigenvalue 
problem C.v=omega^2 v. See polar-vdw.pdf for more details.

Frequencies are input in atomic units, and converted to s^(-1) during
energy calculation.


*/

#include <mc.h>
#include <math.h>
#define TWOoverHBAR 2.6184101e11 //K^-1 s^-1
#define cHBAR 7.63822291e-12 //Ks //HBAR is already taken to be in Js
#define halfHBAR 3.81911146e-12 //Ks
#define au2invsec 4.13412763705e16 //s^-1 a.u.^-1

// STRUCT MATRIX, ALLOC, AND DELETE ************************************
struct mtx {
	int dim;
	double * val;
};

struct mtx * alloc_mtx ( int dim ) {
	//alloc matrix variable and set dim
	struct mtx * M = NULL;
	M = malloc(sizeof(struct mtx));
	checknull(M,"struct mtx * M", sizeof(struct mtx));
	M->dim=dim;
	//alloc matrix storage space
	M->val=calloc(dim*dim,sizeof(double));
	checknull(M->val,"struct mtx * M->val", dim*dim*sizeof(double));
	return M;
}

void free_mtx ( struct mtx * M ) {
	free(M->val);
	free(M);
	return;
}
// END STRUCT MATRIX STUFF *********************************************

struct vect {
	int dim;
	double * val;
};

// generateds two gaussian deviates (they generate in pairs)
double gaussd ( double mean, double std ) {
	static int notstored = 1; /* 0 if there's a value stored in y2*/
	double w1, w2, uni=0;
	static double y2;

	if ( (notstored = !notstored) )
		return std*y2+mean; //return previously stored gaussian deviate
	while ( uni==0 ) uni=rule30_rng(); //reject zeroes
	w1 = sqrt(-2.*log(uni));
	uni=0;
	while ( uni==0 ) uni=rule30_rng(); //reject zeroes
	w2 = 2.*M_PI*uni;

	//now we can generate our two deviates and return one
	y2 = w1*cos(w2);
	return std*w1*sin(w2)+mean;
}

//generate a "basis" of random vectors
struct vect * mkbasis ( int dim, int nvect ) {
	struct vect * rval = NULL;
	int i, j;
	double norm, rnd;

	rval=malloc(nvect*sizeof(struct vect));
	checknull(rval,"struct vect * rval", nvect*sizeof(struct vect));
	for ( i=0; i<nvect; i++ ) {
		//allocate each vector
		rval[i].dim=dim;
		rval[i].val=malloc(dim*sizeof(double));
		checknull(rval[i].val,"struct vect * rval->val", dim*sizeof(double));

		//fill
		norm=0;
		for ( j=0; j<dim; j++ ) {
			rnd=gaussd(0,1); //random number with 0 mean, 1 variance
			rval[i].val[j]=rnd;
			//running total of the norm
			norm+=rng*rng;
		}
		//normalize
		for ( j=0; j<dim; j++ )
			rval[i].val[j]/=sqrt(norm);

	}
	return rval;
}

//calculate SVM (scalar-vector multiplication)
void svm ( double a, struct vect * v ) {
	int i;
	for ( i=0; i<v->dim; i++ ) 
		v->val[i]=a*(v->val[i]);
	return;
}

//calculate VVI (vector-vector inner product)
double vvinner ( struct vect * v1, struct vect * v2 ) {
	double rval=0;
	int i;
	if ( v1->dim != v2->dim ) {
		fprinf("ERROR: Can't perform vector dot product. Dimension doesn't match.\n");
		exit(-1);
	}

	for ( i=0; i<v1->dim; i++ )
		rval+=(v1->val[i] * v2->val[i]);
	return rval;
}

//calculate VVA (vector-vector addition)
//adds the second vector to the first vector (overwrites the first)
void vva ( struct vect * v1, struct vect * v2 ) {
	int i;
	if ( v1->dim != v2->dim ) {
		fprinf("ERROR: Can't perform vector-vector addition. Dimension doesn't match.\n");
		exit(-1);
	}
	for ( i=0; i<v->dim; i++ )
		v1->val[i] += v2->val[i];
	return;
}

//calculate MVM (matrix-vector multiplication)
//makes a new vector
struct vect * mvm ( struct mtx * m, struct vect * v ) {
	int i, j, element;
	struct vect * rval;
	if ( v->dim != m->dim ) {
		fprinf("ERROR: Can't perform matrix-vector multiplication. Dimension doesn't match.\n");
		exit(-1);
	}
	//allocate new vector
	rval=malloc(sizeof(struct vect));
	checknull(rval,"struct vect *", sizeof(struct vect));
	rval->dim = v->dim;
	rval->val=malloc(v->dim *  
	checknull(rval[i].val,"struct vect * rval->val", dim*sizeof(double));

	




//calculate moments


// we can optionally calculate the vdw via 2-body expansion (neglect damping) for comparison
double twobody ( system_t * system ) {
	molecule_t * molecule_ptr;
	atom_t * atom_ptr;
	pair_t * pair_ptr;
	double rm; //reduced mass
	double w1, w2, w; //omegas
	double a1, a2; //alphas
	double e_tot = 0; //correction to the energy	
	double e_single;

	//for each pair
	for ( molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next ) {
		for ( atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next ) {
			for ( pair_ptr = atom_ptr->pairs; pair_ptr; pair_ptr = pair_ptr->next ) {
				//skip if frozen
				if ( pair_ptr->frozen ) continue;
				//skip if they belong to the same molecule
				if ( molecule_ptr == pair_ptr->molecule ) continue;
				//skip if distance is greater than cutoff
				if ( pair_ptr->rimg > system->pbc->cutoff ) continue;
				//fetch alphas and omegas
				a1=atom_ptr->polarizability;
				a2=pair_ptr->atom->polarizability;
				w1=atom_ptr->omega;
				w2=pair_ptr->atom->omega;
				if ( w1 == 0 || w2 == 0 || a1 == 0 || a2 == 0 ) continue; //no vdw energy
				w=2*w1*w2/(w1+w2); //combine omegas
				// 3/4 hbar/k_B(Ks) omega(s^-1)  Ang^6
				e_single = -0.75 * cHBAR * w * au2invsec * a1 * a2 * pow(pair_ptr->rimg,-6.0);
				printf("ESINGLE %lf\n", e_single);
				e_tot += e_single;
			}
		}
	}

	return e_tot;
}


// long-range correction
double lr_vdw_corr ( system_t * system ) {
	molecule_t * molecule_ptr;
	atom_t * atom_ptr;
	pair_t * pair_ptr;
	double w1, w2, w; //omegas
	double a1, a2; //alphas
	double cC; //leading coefficient to r^-6
	double corr = 0; //correction to the energy


	for ( molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next ) {
		for ( atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next ) {
			for ( pair_ptr = atom_ptr->pairs; pair_ptr; pair_ptr = pair_ptr->next ) {
				//skip if already known
				if ( pair_ptr->lrc == 0.0 ) {
					//skip if frozen
					if ( pair_ptr->frozen ) continue;
					//skip if same molecule
					if ( molecule_ptr == pair_ptr->molecule ) continue;
					//fetch alphas and omegas
					a1=atom_ptr->polarizability;
					a2=pair_ptr->atom->polarizability;
					w1=atom_ptr->omega;
					w2=pair_ptr->atom->omega;
					if ( w1 == 0 || w2 == 0 || a1 == 0 || a2 == 0 ) continue; //no vdw energy
					w=2*w1*w2/(w1+w2); //combine omegas
					// 3/4 hbar/k_B(Ks) omega(s^-1)  Ang^6
					cC=0.75 * cHBAR * w * au2invsec * a1 * a2;

					// long-range correction
					pair_ptr->lrc = -4.0/3.0 * M_PI * cC * pow(system->pbc->cutoff,-3.0) / system->pbc->volume;

				}
				//add contribution
				corr += pair_ptr->lrc;
			}
		}
	}

	return corr;

}

// feynman-hibbs correction - based on 2-body. it's not very accurate, since damping is neglected :-(
double fh_vdw_corr ( system_t * system ) {
	molecule_t * molecule_ptr;
	atom_t * atom_ptr;
	pair_t * pair_ptr;
	double rm; //reduced mass
	double w1, w2, w; //omegas
	double a1, a2; //alphas
	double cC; //leading coefficient to r^-6
	double dv, d2v, d3v, d4v; //derivatives
	double du, d2u, d3u, d4u; //derivatives of the first term
	double dy, d2y, d3y, d4y; //derivatives of the second term
	double corr = 0; //correction to the energy
	double corr_single; //single vdw interaction energy

	//for each pair
	for ( molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next ) {
		for ( atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next ) {
			for ( pair_ptr = atom_ptr->pairs; pair_ptr; pair_ptr = pair_ptr->next ) {
				//skip if frozen
				if ( pair_ptr->frozen ) continue;
				//skip if they belong to the same molecule
				if ( molecule_ptr == pair_ptr->molecule ) continue;
				//skip if distance is greater than cutoff
				if ( pair_ptr->rimg > system->pbc->cutoff ) continue;
				//fetch alphas and omegas
				a1=atom_ptr->polarizability;
				a2=pair_ptr->atom->polarizability;
				w1=atom_ptr->omega;
				w2=pair_ptr->atom->omega;
				if ( w1 == 0 || w2 == 0 || a1 == 0 || a2 == 0 ) continue; //no vdw energy
				w=2*w1*w2/(w1+w2); //combine omegas
				// 3/4 hbar/k_B(Ks) omega(s^-1)  Ang^6
				cC=0.75 * cHBAR * w * au2invsec * a1 * a2;
				// reduced mass
				rm=AMU2KG*(molecule_ptr->mass)*(pair_ptr->molecule->mass)/
					((molecule_ptr->mass)+(pair_ptr->molecule->mass));

			//the potential has the form cC*r^-6 * (1 - 2.45*r^-2)
				//derivatives of the first term
				du = 6.0*cC*pow(pair_ptr->rimg,-7.0);	
				d2u= du * (-7.0)/pair_ptr->rimg;
				if ( system->feynman_hibbs_order >= 4 ) {
					d3u= d2u* (-8.0)/pair_ptr->rimg;
					d4u= d3u* (-9.0)/pair_ptr->rimg;
				}
				//derivatives of the second term
				dy = -2.45*8.0*cC*pow(pair_ptr->rimg,-9.0);
				d2y = dy * (-9.0)/pair_ptr->rimg;
				if ( system->feynman_hibbs_order >= 4 ) {
					d3y = d2y*(-10.0)/pair_ptr->rimg;
					d4y = d3y*(-11.0)/pair_ptr->rimg;
				}
				//combine terms
				dv = dy+du;
				d2v = d2y+d2u;
				d3v = d3y+d3u;
				d4v = d4y+d4u;

				//2nd order correction
				corr_single = pow(METER2ANGSTROM, 2.0)*(HBAR*HBAR/(24.0*KB*system->temperature*rm))*(d2v + 2.0*dv/pair_ptr->rimg);
				//4th order correction
				if ( system->feynman_hibbs_order >= 4 )
					corr_single += pow(METER2ANGSTROM, 4.0)*(pow(HBAR, 4.0) /
						(1152.0*pow(KB*system->temperature*rm, 2.0))) *
						(15.0*dv/pow(pair_ptr->rimg, 3.0) + 4.0*d3v/pair_ptr->rimg + d4v);

				//cap the max correction (largest negative)
				//otherwise positions of vdw sites will converge and energy will blow up
				//since repulsion sites may differ
				if ( system->VDW_FH_cap > corr_single )
					corr += system->VDW_FH_cap;
				else
					corr += corr_single;
			}
		}
	}

	return corr;
}

//kill MPI before quitting, when neccessary
void die( int code ){
#ifdef MPI
	MPI_Finalize();
#endif
	exit(code);
}

//only run dsyev from LAPACK if compiled with -llapack, otherwise report an error
#ifdef VDW
//prototype for dsyev (LAPACK)
extern void dsyev_(char *, char *, int *, double *, int *, double *, double *, int *, int *);
#else
void dsyev_
(char * a, char *b, int * c, double * d, int * e, double * f, double * g, int * h, int * i) {
	error("ERROR: Not compiled with Linear Algebra VDW.\n");
	die(-1);
}
#endif

//can be used to test shit --- not actually used in any of the code
void print_mtx ( struct mtx * Cm ) {
	int iC, jC;

	printf("\n==============begin===================\n");
	for ( iC=0; iC<Cm->dim; iC++ ) {
		for ( jC=0; jC<Cm->dim; jC++ ) {
			if ( jC>iC ) continue;
			printf("%.1le ", (Cm->val)[iC+jC*(Cm->dim)]);
		}
		printf("\n");
	}
	printf("\n==============end=================\n");

	return;
}

//build C matrix for a given molecule/system, with atom indicies (offset)/3..(offset+dim)/3
struct mtx * build_M ( int dim, int offset, double ** Am, double * sqrtKinv ) {
	int i; //dummy
	int iA, jA; //Am indicies
	int iC, jC; //Cm indicies
	int nonzero; //non-zero col/rows in Am
	struct mtx * Cm; //return matrix Cm

	//count non-zero elements
	nonzero=0;
	for ( i=offset; i<dim+offset; i++ )
		if ( sqrtKinv[i] != 0 ) nonzero++;

	//allocate
	Cm = alloc_mtx(nonzero);

	//build lapack compatible matrix from Am[offset..dim, offset..dim]
	iC=jC=-1; //C index
	for ( iA=offset; iA<dim+offset; iA++ ) {
		if ( sqrtKinv[iA] == 0 ) continue; //suppress rows/cols full of zeros
		iC++; jC=-1;
		for ( jA=offset; jA<=iA; jA++ ) {
			if ( sqrtKinv[jA] == 0 ) continue; //suppress
			jC++;
			(Cm->val)[iC+jC*(Cm->dim)]=
				Am[iA][jA]*sqrtKinv[iA]*sqrtKinv[jA];
		}
	}

	return Cm;
}

void printevects ( struct mtx * M ) {
	int r,c;

	printf("%%vdw === Begin Eigenvectors ===\n");
	for ( r=0; r < (M->dim); r++ ) {
		for ( c=0; c < (M->dim); c++ ) {
			printf("%.2le ", (M->val)[r+c*M->dim]);
		}
		printf("\n");
	}
	printf("%%vdw=== End Eigenvectors ===\n");
}

/* LAPACK using 1D arrays for storing matricies.
	/ 0  3  6 \
	| 1  4  7 |		= 	[ 0 1 2 3 4 5 6 7 8 ]
	\ 2  5  8 /									*/
double * lapack_diag ( struct mtx * M, int jobtype ) {
	char job; //job type
	char uplo='L'; //operate on lower triagle
	double * work; //working space for dsyev
	int lwork; //size of work array
	int rval; //returned from dsyev_
	double * eigvals;

	//eigenvectors or no?
	if ( jobtype = 2 ) job='V';
	else job = 'N';

	if ( M->dim == 0 ) return NULL;

	//allocate eigenvalues array
	eigvals = malloc(M->dim*sizeof(double));
	checknull(eigvals,"double * eigvals",M->dim*sizeof(double));
	//optimize the size of work array
	lwork = -1;
	work = malloc(sizeof(double));
	checknull(work,"double * work",sizeof(double));
	dsyev_(&job, &uplo, &(M->dim), M->val, &(M->dim), eigvals, work, &lwork, &rval);
	//now optimize work array size is stored as work[0]
	lwork=(int)work[0];
	work = realloc(work,lwork*sizeof(double));
	checknull(work,"double * work",lwork*sizeof(double));
	//diagonalize
	dsyev_(&job, &uplo, &(M->dim), M->val, &(M->dim), eigvals, work, &lwork, &rval);

	if ( rval != 0 ) {
		fprintf(stderr,"error: LAPACK: dsyev returned error: %d\n", rval);
		die(-1);
	}

	free(work);

	return eigvals;
}

double wtanh ( double w, double T ) {
	if ( w < 1.0E-10 ) return TWOoverHBAR*T/au2invsec; //from Taylor expansion
	if ( T == 0 ) return w;
	return w/tanh(halfHBAR*w*au2invsec/T);
}

double eigen2energy ( double * eigvals, int dim, double temperature ) {
	int i;
	double rval=0;
	
	if ( eigvals == NULL ) return 0;

	for ( i=0; i<dim; i++ ) {
		if ( eigvals[i] < 0 ) eigvals[i]=0;
		rval += wtanh(sqrt(eigvals[i]), temperature);
	}
	return rval;
}

double calc_e_iso ( system_t * system, double * sqrtKinv ) {
	int n, nstart, curr_dimM;
	double e_iso; //total vdw energy of isolated molecules
	struct mtx * Cm_iso; //matrix Cm_isolated
	double * eigvals; //eigenvalues of Cm_cm
	molecule_t * molecule_ptr;
	atom_t * atom_ptr;

	e_iso=0; //init, will sum over molecules (eigenvalues of each molecule)

	n=nstart=0; //loop through each individual molecule
	for ( molecule_ptr = system->molecules; molecule_ptr; molecule_ptr=molecule_ptr->next ) {
		for ( atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next ) {
			n++;
		}
		//build matrix for calculation of vdw energy of isolated molecule
		Cm_iso = build_M(3*(n-nstart), 3*nstart, system->A_matrix, sqrtKinv);

		//diagonalize M and extract eigenvales -> calculate energy
		eigvals=lapack_diag(Cm_iso,1); //no eigenvectors
		e_iso+=eigen2energy(eigvals, Cm_iso->dim, system->temperature);

		//free memory
		free(eigvals);
		free_mtx(Cm_iso);
		//reset the offset
		nstart=n;
	}	

	//convert a.u. -> s^-1 -> K
    return e_iso * au2invsec * halfHBAR ;
}

//build the matrix K^(-1/2) -- see the PDF
double * getsqrtKinv ( system_t * system, int N ) {
	double * sqrtKinv;
	molecule_t * molecule_ptr;
	atom_t * atom_ptr;
	int i = 0;

	//malloc 3*N wastes an insignificant amount of memory, but saves us a lot of index management
	sqrtKinv = malloc(3*N*sizeof(double));
	checknull(sqrtKinv,"double * sqrtKinv",3*N*sizeof(double));

	for ( molecule_ptr = system->molecules; molecule_ptr; molecule_ptr=molecule_ptr->next ) {
		for ( atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next ) {
			//Seek through atoms, calculate sqrtKinv*
			sqrtKinv[i] = sqrt(atom_ptr->polarizability)*atom_ptr->omega;
			sqrtKinv[i+1] = sqrt(atom_ptr->polarizability)*atom_ptr->omega;
			sqrtKinv[i+2] = sqrt(atom_ptr->polarizability)*atom_ptr->omega;
			i+=3;
		}
	}
	
	return sqrtKinv;
}

int getnatoms ( system_t *system ) {
	molecule_t * molecule_ptr;
	atom_t * atom_ptr;
	int i=0;
	for ( molecule_ptr = system->molecules; molecule_ptr; molecule_ptr=molecule_ptr->next ) {
		for ( atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next ) {
			i++;
		}
	}
	return i;
}

//returns interaction VDW energy
double vdw(system_t *system) {

	int N, dimC; //number of atoms, number of non-zero rows in C-Matrix
	double e_total, e_iso; //total energy, isolation energy (atoms @ infinity)
	double * sqrtKinv; //matrix K^(-1/2); cholesky decomposition of K
	double ** Am = system->A_matrix; //A_matrix
	struct mtx * Cm; //C_matrix (we use single pointer due to LAPACK requirements)
	double * eigvals; //eigenvales
	double fh_corr, lr_corr;

	N=getnatoms(system);

	//allocate arrays. sqrtKinv is a diagonal matrix. d,e are used for matrix diag.
	sqrtKinv = getsqrtKinv(system,N);

	//calculate energy vdw of isolated molecules
	e_iso = calc_e_iso ( system, sqrtKinv );

	//Build the C_Matrix
	Cm = build_M (3*N, 0, Am, sqrtKinv);

	//setup and use lapack diagonalization routine dsyev_()
	eigvals = lapack_diag (Cm, system->polarvdw); //eigenvectors if system->polarvdw == 2
	if ( system->polarvdw == 2 )
		printevects(Cm);

	//return energy in inverse time (a.u.) units
	e_total = eigen2energy(eigvals, Cm->dim, system->temperature);
	e_total *= au2invsec * halfHBAR; //convert a.u. -> s^-1 -> K

	//vdw energy comparison
	if ( system->polarvdw == 3 )
		printf("VDW Two-Body | Many Body = %lf | %lf\n", twobody(system),e_total-e_iso);

	if ( system->feynman_hibbs ) fh_corr = fh_vdw_corr(system);
		else fh_corr=0;
	if ( system->rd_lrc ) lr_corr = lr_vdw_corr(system);
		else lr_corr=0;

//cleanup and return
	free(sqrtKinv);
	free(eigvals);
	free_mtx(Cm);

	return e_total - e_iso + fh_corr + lr_corr;

}