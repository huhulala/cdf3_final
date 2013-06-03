#include "helper.h"
#include "visual.h"
#include "init.h"
#include "uvp.h"
#include "boundary_val.h"
#include "sor.h"
#include <stdio.h>

/**
 * The main operation reads the configuration file, initializes the scenario and
 * contains the main loop. So here are the individual steps of the algorithm:
 *
 * - read the program configuration file using read_parameters()
 * - set up the matrices (arrays) needed using the matrix() command
 * - create the initial setup init_uvp(), init_flag(), output_uvp()
 * - perform the main loop
 * - trailer: destroy memory allocated and do some statistics
 *
 * The layout of the grid is decribed by the first figure below, the enumeration
 * of the whole grid is given by the second figure. All the unknowns corresond
 * to a two dimensional degree of freedom layout, so they are not stored in
 * arrays, but in a matrix.
 *
 * @image html grid.jpg
 *
 * @image html whole-grid.jpg
 *
 * Within the main loop the following big steps are done (for some of the 
 * operations a definition is defined already within uvp.h):
 *
 * - calculate_dt() Determine the maximal time step size.
 * - boundaryvalues() Set the boundary values for the next time step.
 * - calculate_fg() Determine the values of F and G (diffusion and confection).
 *   This is the right hand side of the pressure equation and used later on for
 *   the time step transition.
 * - calculate_rs()
 * - Iterate the pressure poisson equation until the residual becomes smaller
 *   than eps or the maximal number of iterations is performed. Within the
 *   iteration loop the operation sor() is used.
 * - calculate_uv() Calculate the velocity at the next time step.
 */
int main(int argn, char** args)
{
	/**************** Variable definition goes here  ****************/
	double Re, UI, VI, PI, GX, GY;
	double t_end, xlength, ylength;
	double dt, dx, dy;
	double alpha, omg, tau;
	double eps, dt_value, res, t, deltaP;
	int itermax, it, n;

	int imax = 0;
	int jmax = 0;
	int wl = 0;
	int wr = 0;
	int wt = 0;
	int wb = 0;

	/* Output filename */
	char* output_filename;
	char output_filename_array[64];
	char* problem;

	/* arrays */
	double **U = NULL;
	double **V = NULL;
	double **P = NULL;
	double **RS = NULL;
	double **F = NULL;
	double **G = NULL;
	int **Problem = NULL;
	int **Flag = NULL;

	char inputString[64];
	char problemImageName[64];
	char szFileName[64];
	/**************** Variable definition ends here  ****************/
	/* check arguments */
	if (argn <= 1)
	{
		printf("ERROR: you need to specify a problem \n");
		return 1;
	}
	if (!(strcmp(args[1], "karman") == 0
		|| strcmp(args[1], "plane") == 0
		|| strcmp(args[1], "step") == 0))
	{
		printf("ERROR: pass karman, plane or step\n");
		return 1;
	}

	/*************** parameter loading and problem input goes here *************************/

	t = 0.0;
	n = 0;
	problem = args[1];
	/* assemble parameter file string */
	output_filename= "./output_";
	strcpy(output_filename_array, output_filename);
	strcat(output_filename_array, args[1]);
	strcat(output_filename_array, "/");
	strcat(output_filename_array, args[1]);
	strcpy(inputString, args[1]);
	strcpy(szFileName, inputString);
	strcat(szFileName, ".dat");

	/* load parameters from "problem".dat file */
	/* grid size (dx,dy,imax,ymax) should now be read from the image */
    read_parameters(szFileName,&Re,&UI,&VI,&PI,&GX,&GY,&t_end,&xlength,&ylength,&dt,&alpha,
    		        &omg,&tau,&itermax,&eps,&wl,&wr,&wt, &wb, &dt_value, &deltaP);

    /* assemble prblem file string */
	strcpy(problemImageName, inputString);
	strcat(problemImageName, ".pgm");

	/*  load problem description from "problem".pgm file */
	/*  read imax and jmax (and calculate dx und dy) from problem description now */
	Problem = read_pgm(problemImageName, &imax, &jmax);
	/* calculute dx/dy */
	dx = xlength / (double) (imax);
	dy = ylength / (double) (jmax);

	/* allocate memory for the arrays */
	U = matrix(0, imax + 1, 0, jmax + 1);
	V = matrix(0, imax + 1, 0, jmax + 1);
	P = matrix(0, imax + 1, 0, jmax + 1);
	F = matrix(0, imax + 1, 0, jmax + 1);
	G = matrix(0, imax + 1, 0, jmax + 1);
	RS = matrix(0, imax + 1, 0, jmax + 1);

	/* allocate memory for the field */
	Flag = imatrix(0, imax + 1, 0, jmax + 1);

	/*************** the algorithm starts here *************************/

	/* init uvp - here the signature was extended to the problem
	 * string to init u&v in the step case to 0 */
	init_uvp(UI, VI, PI, imax, jmax, problem, U, V, P);
	/* init flag field */
	if (init_flag(Problem, imax, jmax, Flag) != 1)
	{
		printf("Invalid obstacle. Program quits ...\n");
		free_imatrix(Problem, 0, imax + 1, 0, jmax + 1);
		free_imatrix(Flag, 0, imax + 1, 0, jmax + 1);
		return -1;
	}

	while (t < t_end)
	{
		/*calculate the timestep */
        calculate_dt(Re, tau,&dt, dx, dy, imax,jmax, U,V);
		/*calculate the boundary values   */
		boundaryvalues( imax, jmax, wl, wr, wt, wb, U, V, F, G, P, Flag);
		/* set special boundary values*/
	    spec_boundary_val( problem, imax, jmax, dx, dy, Re, deltaP, U, V, P);
		/*calculate F&G* - here the signature was extended to the FLAG
	     * matrix to calculate values only for fluid cells */
	    calculate_fg( Re, GX, GY, alpha, dt, dx,dy, imax, jmax, U, V, F, G, Flag);
		/*calculate righthand site - here the signature was extended to the FLAG
	     * matrix to calculate values only for fluid cells */
        calculate_rs(dt,dx, dy, imax, jmax, F, G, RS, Flag);

		/* set initial residual*/
		it = 0;
		res = eps + 1;
		while (it < itermax && res > eps)
		{
			/* here the signature was extended to the FLAG
	         * matrix to calculate values only for fluid cells */
            sor(omg, dx, dy, imax, jmax, P, RS, &res, Flag, problem, deltaP);
			it++;
		}
		/* calculate uv - here the signature was extended to the FLAG
	     * matrix to calculate values only for fluid cells */
	    calculate_uv(dt,dx,dy,imax,jmax, U, V, F, G, P, Flag);

	 	t = t + dt;
	 	printf("t=%f dt=%f\n",t,dt);
	 	if (t > n * dt_value)
	 	{
	 		write_vtkFile(output_filename_array, n, imax, jmax, dx, dy, U, V, P);
	 		n++;
	 	}
	}
	write_vtkFile(output_filename_array, n, imax, jmax, dx, dy, U, V, P);

	/* free arrays */
	free_matrix(U, 0, imax + 1, 0, jmax + 1);
	free_matrix(V, 0, imax + 1, 0, jmax + 1);
	free_matrix(P, 0, imax + 1, 0, jmax + 1);
	free_matrix(F, 0, imax + 1, 0, jmax + 1);
	free_matrix(G, 0, imax + 1, 0, jmax + 1);
	free_matrix(RS, 0, imax + 1, 0, jmax + 1);
	free_imatrix(Flag, 0, imax + 1, 0, jmax + 1);
	return 1;
}
