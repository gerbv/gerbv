#ifndef GERB_TRANSF_H
#define GERB_TRANSF_H

/** this linear transformation applies first shift by offset [0]=x, [1]=y,
  * then multiplication by r_mat and scale */
typedef struct gerb_transf {
    double r_mat[2][2];
    double scale;
    double offset[2];
} gerb_transf_t;


gerb_transf_t* gerb_transf_new(void);

void gerb_transf_free(gerb_transf_t* transf);

void gerb_transf_reset(gerb_transf_t* transf);
   
void gerb_transf_apply(double x, double y, gerb_transf_t* transf, double *out_x, double *out_y);

#endif


