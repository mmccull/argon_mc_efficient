
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "stringlib.h"

// Declare Subroutines
void read_cfg_file(int *, double *, double *, char *, char *, int *, int *, double *);
void write_xyz_step(double **, int, int, double, FILE *);
void init_positions(double **, int, double *);
double total_pair_energy(double **, int, double, double**);
void atom_pair_energy(double **, int, double, int, double *);
double lennard_jones(double *, double *, double);
double sum(double*, int);

static double kB = 1.9872041E-3; // actually R in units of kcal/mol/K
static double eps = 0.210849;    // units of kcal/mol
static double sigma = 3.345;  // units of angstroms
static double sigma6 = 1400.80193382; // units of angstroms^6

//Main Program
int main() {

	int nAtoms;     // Number of atoms
	double temp;     // temperature
	double box;      // cubic box size
	int nIter;      // number of MC iterations
	int deltaWrite; // how often to write coordinates and log info in MC
	double deltaX;   // how big to make the translation attempt

	char trajFileName[1024];  // output trajectory file name
	char logFileName[1024];   // log file name

	double **coord;  // coordinates of particles
	double **atomEnergy; // energy per atom

	int i, j, k, atom1;    // genereic indeces
	int iter;       // MC iteration
	int atom;       // MC selected atom
	double energy;   // energy of the system
	double delta[3]; // added position
	double kBT;
	double *newEnergy;
	double deltaE;
	int acceptedMoves;
		
	FILE *xyzOut;
	FILE *logOut;      // log output file

	time_t startTime;   // initial clock time
	time_t stopTime;    // final clock time
	time_t routineStartTime; // start time for a routine
	time_t routineStopTime;  // stop time for a routine
	double timeSpent; // amount of time in seconds 
	double energyCalcTime; // 

	// initialize job timing
	startTime = clock();

	// read config data from standard in
	read_cfg_file(&nAtoms, &temp, &box, trajFileName, logFileName, &nIter, &deltaWrite, &deltaX);
	kBT = kB*temp;
	printf("kB*T=%f\n",kBT);

	// allocate coordinate array
	coord = (double**) malloc(nAtoms*sizeof(double*));
	atomEnergy = (double**) calloc(nAtoms,sizeof(double*));
	newEnergy = (double*) malloc(nAtoms*sizeof(double));
	for (i=0;i<nAtoms;i++) {
		coord[i] = (double*) malloc(3*sizeof(double));
		atomEnergy[i] = (double*) calloc(nAtoms,sizeof(double));
	}
	// allocate atom energy array (calloc initializes the memory to zero
	// initialize particle positions
	init_positions(coord,nAtoms,&box);

	// Compute energy of system
	energy = total_pair_energy(coord,nAtoms,box,atomEnergy);

	xyzOut = fopen(trajFileName,"w");
	logOut = fopen(logFileName,"w");

	energyCalcTime=0;
	// Perform MC loop
	for(iter=0;iter<nIter;iter++) {
		if (iter%deltaWrite==0) {
			fprintf(logOut,"Step: %10d Energy: %f\n", iter, energy);
//			printf("Step: %10d Energy: %f\n", iter, energy);
			// write positions
			write_xyz_step(coord,nAtoms,iter, box, xyzOut);
			// flush buffers
			fflush(xyzOut);
			fflush(logOut);
		}

		// randomly choose a particle to move
		atom = rand()%nAtoms;

		// compute random translation
		for (i=0;i<3;i++) {
			delta[i] = deltaX*(rand()/((double) RAND_MAX)-0.5);
			coord[atom][i] += delta[i];
		}
		// compute new energy
		routineStartTime=clock();
		atom_pair_energy(coord,nAtoms,box,atom,newEnergy);
		routineStopTime=clock();
		energyCalcTime += (double)(routineStopTime-routineStartTime)/CLOCKS_PER_SEC;

		deltaE = sum(newEnergy,nAtoms)-sum(atomEnergy[atom],nAtoms);
//		fprintf(logOut,"atom: %d deltaE: %f\n",atom,deltaE);
		if (exp(-deltaE/kBT)> (rand()/((double) RAND_MAX))) {
			for (atom1=0;atom1<nAtoms;atom1++) {
				atomEnergy[atom][atom1] = newEnergy[atom1];
				atomEnergy[atom1][atom] = newEnergy[atom1];
			}
			energy += deltaE;
			acceptedMoves++;
			// check to see if we need to wrap
			for(i=0;i<3;i++) {
				if (coord[atom][i] > box) {
					coord[atom][i] -= box;
				} else if (coord[atom][i]<0) {
					coord[atom][i] += box;
				}
			}
		} else {
			for(i=0;i<3;i++) {
				coord[atom][i]-=delta[i];
			}
		}


	}

	fclose(xyzOut);

	// average energy routine calc time
	printf("Total time to compute energies (seconds): %f\n",energyCalcTime);
	energyCalcTime /= (double)(nIter);
	printf("Average time to compute energies (seconds): %f\n",energyCalcTime);

	// time job
	stopTime = clock();
	timeSpent = (double)(stopTime-startTime)/CLOCKS_PER_SEC;
	printf("Total job time (seconds): %f\n",timeSpent);
}

// Subroutines
//

double sum(double* array, int dim) {

	double temp;
	int i;

	temp = 0;
	for (i=0;i<dim;i++) {
		temp += array[i];
	}
	return temp;
}

void atom_pair_energy(double **coord, int nAtoms, double box, int atom, double *atomEnergy) {

	int atom2;

	atomEnergy[atom]=0;
	for (atom2=0;atom2<nAtoms;atom2++) {

		if (atom2 != atom) {
			atomEnergy[atom2] = lennard_jones(coord[atom],coord[atom2],box);		
		}

	}


}

double total_pair_energy(double **coord, int nAtoms, double box, double** atomEnergy) {

	int atom1;
	int atom2;
	double energy;
	double tempE;

	energy=0;
	for (atom1=0;atom1<nAtoms-1;atom1++) {

		for (atom2=atom1+1;atom2<nAtoms;atom2++) {

			tempE = lennard_jones(coord[atom1],coord[atom2],box);		
			energy += tempE;
			atomEnergy[atom1][atom2] = tempE;
			atomEnergy[atom2][atom1] = tempE;

		}

	}

	return energy;

}

double lennard_jones(double *pos1, double *pos2, double box) {

	double dist2;
	double temp;
	double energy;
	double dist6;
	int i;

	// compute the distance between the atoms
	dist2 = 0;
	for (i=0;i<3;i++) {
		temp = pos1[i]-pos2[i];
		// check periodic boundaries
		if (temp< -box/2.0) {
			temp += box;
		} else if (temp > box/2.0) {
			temp -= box;
		}
		dist2 += temp*temp;
	}
	dist6 = dist2*dist2*dist2;

	// compute the energy
	energy = (double)(4*eps*(sigma6*sigma6/(dist6*dist6)-sigma6/dist6));

	return energy;	

}

void init_positions(double **coord, int nAtoms, double *box) {

	double cbrt(double x); // cube root function
	int iBoxD;            // integer box dimension
	double fBoxD;          // double box dimension

	int x, y, z;
	double xPos,yPos,zPos;
	int atomCount;

	// determine how many bins to divide the box into
	iBoxD = (int) cbrt((double) nAtoms);
	if (iBoxD*iBoxD*iBoxD < nAtoms) {
		iBoxD++;
	}
	// determine the size of the bins
	fBoxD = 3.55;
	*box = iBoxD*fBoxD;
	printf("box dimension: %f\n", *box);

	// add a particle in each box
	atomCount=0;
	for(x=0;x<iBoxD;x++) {
		xPos = (x+0.5)*fBoxD;
		for (y=0;y<iBoxD;y++) {
			yPos = (y+0.5)*fBoxD;
			for (z=0;z<iBoxD;z++) {
				if (atomCount<nAtoms) {
					zPos = (z+0.5)*fBoxD;
					coord[atomCount][0]=xPos;
					coord[atomCount][1]=yPos;
					coord[atomCount][2]=zPos;
					atomCount++;
				} else {
					break;
				}
			}
			if (atomCount>=nAtoms) {
				break;
			}
		}
		if (atomCount>=nAtoms) {
			break;
		}
	}
}		

void read_cfg_file(int *nAtoms, double *temp, double *box, char *trajFileName, char *logFileName, int *nIter, int *deltaWrite, double *deltaX) {

	char buffer[1024];
	char tempBuffer[1024];
	char check[15];
	char *firstWord;
	double *rCut;

	while (fgets(buffer,1024,stdin) != NULL) {

		strncpy(tempBuffer,buffer,1024);
		firstWord=string_firstword(tempBuffer);
//		printf ("First word = %s\n",firstWord);
		if (strncmp(firstWord,"nAtoms",6)==0) {
			*nAtoms = atoi(string_secondword(buffer));
		} else if (strncmp(firstWord,"nIter",5)==0) {
			*nIter = atoi(string_secondword(buffer));
		} else if (strncmp(firstWord,"deltaWrite",10)==0) {
			*deltaWrite = atoi(string_secondword(buffer));
		} else if (strncmp(firstWord,"temperature",11)==0) {
			*temp = atof(string_secondword(buffer));
//		} else if (strncmp(firstWord,"rcut",4)==0) {
//			*rCut = atof(string_secondword(buffer));
//		} else if (strncmp(firstWord,"box",3)==0) {
//			*box = atof(string_secondword(buffer));
		} else if (strncmp(firstWord,"deltaX",6)==0) {
			*deltaX = atof(string_secondword(buffer));
		} else if (strncmp(firstWord,"trajFile",8)==0) {
			strcpy(trajFileName,string_secondword(buffer));
		} else if (strncmp(firstWord,"logFile",7)==0) {
			strcpy(logFileName,string_secondword(buffer));
		}
	

	}	
	
	// Print log file info
	printf("Trajectory file: %s\n",trajFileName);
	printf("log file: %s\n",logFileName);
	printf("nAtoms: %d\n",*nAtoms);
	printf("Temperature: %f\n",*temp);
	printf("nIter: %d\n",*nIter);
	printf("deltaWrite: %d\n",*deltaWrite);
//	printf("box dimension: %f\n", *box);
	printf("deltaX (MC translation): %f\n", *deltaX);
//	printf("cutoff: %f\n",*rCut);


}


void write_xyz_step(double **coord, int nAtoms, int iter, double box, FILE *xyzOut) {

	int i, j, k;
	int atom;

	fprintf(xyzOut,"%d\n",nAtoms);
	fprintf(xyzOut,"Step %d box %8.3f %8.3f %8.3f\n",iter, box, box, box);
	for (atom=0;atom<nAtoms;atom++) {

		fprintf(xyzOut,"Ar %12.6f%12.6f%12.6f\n",coord[atom][0],coord[atom][1],coord[atom][2]);

	}


}

