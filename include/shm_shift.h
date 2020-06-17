/*
 * CTU/DIT project - SHM signal analysis
 * Author: Jakub Parez <parez.jakub@gmail.com>
 */

#ifndef SHM_SHIFT_H
#define SHM_SHIFT_H

#define CELL_MAX_LEN 10

#include "../lib/libcsv/csv.h"


enum interp_alg
{
    PLI = 1,  /** Piecewise Linear Interpolation */
    CSI = 2,  /** Cubic Spline Interpolation */
    ECI = 3   /** Extremum Center Inerpolation */
};

struct pt
{
    float x;
    float y;
};

struct signal
{
    int* data;                  /** signal values from csv array */
    int data_cnt;               /** count of signal values from csv */

    struct pt* interp;          /** interpolated points array */
    int ip_cnt;                 /** count of interpolated points */

    int* loc_min_idx;           /** local min indexes from data array */
    int loc_min_idx_cnt;        /** count of local min */

    int* loc_max_idx;           /** local max indexes from data array */
    int loc_max_idx_cnt;        /** count of local max */

    struct pt* loc_ext_cntr;    /** local extremes centers */
    int loc_ext_cntr_cnt;       /** count of local extremes centers */

    struct pt glob_max;         /** global max point from interpolated array */
    struct pt glob_min;         /** global min point from interpolated array */

    float shift_x_max;          /** main result - difference in global max from ref */

    char name[CELL_MAX_LEN];    /** signal header from csv */
};

typedef struct shm_shift        /** library main object */
{
     /** public: */
    struct signal* signals;     /** signals array */
    struct csv_parser parser;   /** csv parser */

    int interp_min;             /** optional cfg for prefering loc min rather then max */

    /** private: */
    int second_pass;
    long unsigned rows;
    long unsigned cols;
    long unsigned rows_it;
    long unsigned cols_it;
    enum interp_alg alg;
    int interp_n;
} shm_shift_t;


shm_shift_t* shm_shift__create();

int shm_shift__read_csv(shm_shift_t* self, char* csv_path, char delim);
int shm_shift__analyse_csv(shm_shift_t* self, enum interp_alg alg, int ref_col, int interp_n);
int shm_shift__write_csv(shm_shift_t* self, char* csv_path, char delim);

int shm_shift__analyse_signal(struct signal* sig, enum interp_alg alg, int interp_n, int interp_min);

void shm_shift__dispose_calc(shm_shift_t* self);
void shm_shift__dispose_all(shm_shift_t* self);
void shm_shift__free(shm_shift_t* self);



#endif
