#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "gerb_transf.h"

void gerb_transf_free(gerb_transf_t* transf)
{
    free(transf);
    
}/*gerb_transf_t*/


gerb_transf_t* gerb_transf_new(void)
{
    gerb_transf_t *transf;
    
    transf = malloc(sizeof(gerb_transf_t));
    gerb_transf_reset(transf);
    return transf;
    

} /*gerb_transf_new*/

void gerb_transf_reset(gerb_transf_t* transf)
{
    memset(transf,0,sizeof(gerb_transf_t));
    
    transf->r_mat[0][0] = transf->r_mat[1][1] = 1.0; /*off-diagonals 0 diagonals 1 */
    transf->r_mat[1][0] = transf->r_mat[0][1] = 0.0;
    transf->scale = 1.0;
    transf->offset[0] = transf->offset[1] = 0.0;
    
} /*gerb_transf_reset*/

void gerb_transf_apply(double x, double y, gerb_transf_t* transf, double *out_x, double *out_y)
{

//    x += transf->offset[0];
//    y += transf->offset[1];
    *out_x = (x * transf->r_mat[0][0] + y * transf->r_mat[0][1]) * transf->scale;
    *out_y = (x * transf->r_mat[1][0] + y * transf->r_mat[1][1]) * transf->scale;
    *out_x += transf->offset[0];
    *out_y += transf->offset[1];
    
    
}/*gerb_transf_apply*/
