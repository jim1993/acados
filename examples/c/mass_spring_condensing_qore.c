/*
 *    This file is part of acados.
 *
 *    acados is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    acados is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with acados; if not, write to the Free Software Foundation,
 *    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

// external
#include <stdio.h>
#include <stdlib.h>
// acados
#include "acados/dense_qp/dense_qp_common.h"
#include "acados/ocp_qp/ocp_qp_common.h"
#include "acados/ocp_qp/ocp_qp_common_frontend.h"
#include "acados/ocp_qp/ocp_qp_condensing_solver.h"
#include "acados/utils/create.h"
#include "acados/utils/print.h"
#include "acados/utils/timing.h"
#include "acados/utils/types.h"

#define ELIMINATE_X0
#define NREP 100

#include "./mass_spring.c"

int main() {
    printf("\n");
    printf("\n");
    printf("\n");
    printf(" acados + condensing_qore\n");
    printf("\n");
    printf("\n");
    printf("\n");

    /************************************************
    * ocp qp
    ************************************************/

    ocp_qp_in *qp_in = create_ocp_qp_in_mass_spring();

    // print_ocp_qp_dims(qp_in->dim);
    // exit(1);

    int N = qp_in->dim->N;
    int *nx = qp_in->dim->nx;
    int *nu = qp_in->dim->nu;
    int *nb = qp_in->dim->nb;
    int *ng = qp_in->dim->ng;
    int *ns = qp_in->dim->ns;

    /************************************************
    * ocp qp solution
    ************************************************/

    ocp_qp_out *qp_out = create_ocp_qp_out(qp_in->dim);

    /************************************************
    * qore
    ************************************************/

    ocp_qp_condensing_solver_args *arg = ocp_qp_condensing_solver_create_arguments(qp_in->dim, CONDENSING_QORE);

    ocp_qp_condensing_solver_memory *mem = ocp_qp_condensing_solver_create_memory(qp_in->dim, arg);

    void *work = malloc(ocp_qp_condensing_solver_calculate_workspace_size(qp_in->dim, arg));

	int acados_return; // 0 normal; 1 max iter

    acados_timer timer;
    acados_tic(&timer);

    ocp_qp_info *info = (ocp_qp_info *)qp_out->misc;
    ocp_qp_info min_info;
    min_info.total_time = min_info.condensing_time = min_info.solve_QP_time = min_info.interface_time = 1e10;

	for (int rep = 0; rep < NREP; rep++) {
        acados_return = ocp_qp_condensing_solver(qp_in, qp_out, arg, mem, work);

        if (info->total_time < min_info.total_time) min_info.total_time = info->total_time;
        if (info->condensing_time < min_info.condensing_time) min_info.condensing_time = info->condensing_time;
        if (info->solve_QP_time < min_info.solve_QP_time) min_info.solve_QP_time = info->solve_QP_time;
        if (info->interface_time < min_info.interface_time) min_info.interface_time = info->interface_time;
    }

    double time = acados_toc(&timer)/NREP;

    /************************************************
    * extract solution
    ************************************************/

    ocp_qp_dims *dims = qp_in->dim;

    colmaj_ocp_qp_out *sol;
    void *memsol = malloc(colmaj_ocp_qp_out_calculate_size(dims));
    assign_colmaj_ocp_qp_out(dims, &sol, memsol);
    convert_ocp_qp_out_to_colmaj(qp_out, sol);

    /************************************************
    * compute residuals
    ************************************************/

    ocp_qp_res *qp_res = create_ocp_qp_res(dims);
    ocp_qp_res_ws *res_ws = create_ocp_qp_res_ws(dims);
    compute_ocp_qp_res(qp_in, qp_out, qp_res, res_ws);

    /************************************************
    * compute infinity norm of residuals
    ************************************************/

    double res[4];
    compute_ocp_qp_res_nrm_inf(qp_res, res);

    /************************************************
    * print solution and stats
    ************************************************/

    printf("\nu = \n");
    for (int ii = 0; ii < N; ii++) d_print_mat(1, nu[ii], sol->u[ii], 1);

    printf("\nx = \n");
    for (int ii = 0; ii <= N; ii++) d_print_mat(1, nx[ii], sol->x[ii], 1);

    printf("\npi = \n");
    for (int ii = 0; ii < N; ii++) d_print_mat(1, nx[ii+1], sol->pi[ii], 1);

    printf("\nlam = \n");
    for (int ii = 0; ii <= N; ii++) d_print_mat(1, 2*nb[ii]+2*ng[ii], sol->lam[ii], 1);

    printf("\ninf norm res: %e, %e, %e, %e\n", res[0], res[1], res[2], res[3]);

    dense_qp_qore_memory *tmp_mem = (dense_qp_qore_memory *) mem->solver_memory;

    printf("\nNumber of iterations = %d\n\n\n", tmp_mem->num_iter);

    print_ocp_qp_info(&min_info);

    /************************************************
    * free memory
    ************************************************/

    free(qp_in);
    free(qp_out);
    free(sol);
    free(qp_res);
    free(res_ws);
    free(arg);
    free(mem);
    free(work);

	return 0;
}