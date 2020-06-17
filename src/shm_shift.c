/*
 * CTU/DIT project - SHM signal analysis
 * Author: Jakub Parez <parez.jakub@gmail.com>
 */

#if defined(_WIN32) || defined(WIN32)
    #include <windows.h>
#else /* __unix__ */
    #include <unistd.h>
    #include <stdlib.h>
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <assert.h>

#include "../include/shm_shift.h"

//#define DEBUG

static void cb1(void *s, size_t len, void *dat, void *obj);
static void cb2(int c, void *data, void *obj);
static int is_space(unsigned char c);
static int is_term(unsigned char c);

static void local_max_min(int* array, int len, int* max_idx, int* min_idx, int* max_cnt, int* min_cnt);
static void pli(struct signal* sig, int interp_min);
static void csi(struct signal* sig, int ni, float* y_override, int interp_min);
static void eci(struct signal* sig, int ni);
static void cubic_spline(int n, int* x, float* a, float* b, float* c, float* d);
static void eci_get_points_cntr(int idx, int loc_cnt, int* loc, int* di, int* p1x, int* p2x, int* p1y, int* p2y);
static float line_eq(int x, int x1, int x2, int y1, int y2);

struct counts {
    long unsigned fields;
    long unsigned rows;
};


shm_shift_t* shm_shift__create()
{
    /** create */
    shm_shift_t* self = (shm_shift_t*) malloc(sizeof(shm_shift_t));

    /** init */
    if (csv_init(&self->parser, CSV_APPEND_NULL, self) != 0) {
        fprintf(stderr, "Failed to initialize csv parser\n");
        exit(EXIT_FAILURE);
    }
    self->interp_min = 0;

    return self;
}

int shm_shift__read_csv(shm_shift_t* self, char* csv_path, char delim)
{
    if (!self)
        return -3;

    self->second_pass = 0;
    self->rows = 0;
    self->cols = 0;
    self->rows_it = 0;
    self->cols_it = 0;

    FILE* f = fopen(csv_path, "r");

    if (!f)
    {
        printf("Can't open file\n");
        return -1;
    }

    char buf[1024];
    struct counts c = {0, 0};
    size_t bytes_read;

    csv_set_space_func(&self->parser, is_space);
    csv_set_term_func(&self->parser, is_term);
    csv_set_delim(&self->parser, delim);

    for (int i = 0; i < 2; i++)
    {
        while ((bytes_read=fread(buf, 1, 1024, f)) > 0)
        {
            if (csv_parse(&self->parser, buf, bytes_read, cb1, cb2, &c) != bytes_read)
            {
                fprintf(stderr, "Error while parsing file: %s\n", csv_strerror(csv_error(&self->parser)));
                return -2;
            }
        }
        if (i == 0)
        {
            self->rows = self->rows_it;
            self->rows_it = 0;
            self->cols_it = 0;

            self->signals = (struct signal*)malloc(self->cols * sizeof(struct signal));

            for (int j = 0; j < self->cols; j++)
                self->signals[j].data = (int*)malloc(self->rows * sizeof(int));

            rewind(f);
            memset(buf, '\0', 1024);
            self->second_pass = 1;
        }
    }

    csv_fini(&self->parser, cb1, cb2, &c);

    if (ferror(f))
    {
        fprintf(stderr, "Error while reading file %s\n", csv_path);
        fclose(f);
        return -4;
    }

    fclose(f);
    printf("%lu rows, %lu cols\n\n", self->rows, self->cols);

    return 0;
}

int shm_shift__analyse_csv(shm_shift_t* self, enum interp_alg alg, int ref_col, int interp_n)
{
    if (!self)
        return -3;

    assert(ref_col == 0); /* for now its enough to assume that ref col is the first one */

    self->alg = alg;
    self->interp_n = interp_n;

    /* int maxmin_sz = (self->rows / 2); */

    for (int i = 0; i < self->cols; i++)
    {
        shm_shift__analyse_signal(self->signals + i, alg, interp_n, self->interp_min);

        /** finally subtract global max from ref signal */
        if (i == ref_col)
            self->signals[i].shift_x_max = 0;
        else
            self->signals[i].shift_x_max = self->signals[i].glob_max.x - self->signals[ref_col].glob_max.x;
    }

    return 0;
}

int shm_shift__analyse_signal(struct signal* sig, enum interp_alg alg, int interp_n, int interp_min)
{
    if (!sig)
        return -3;

    int maxmin_sz = sig->data_cnt / 2;
    assert(maxmin_sz > 0);

    sig->loc_max_idx = (int*)malloc(maxmin_sz * sizeof(int));
    sig->loc_min_idx = (int*)malloc(maxmin_sz * sizeof(int));

#ifdef DEBUG
    //printf("dataset %d: %s - first value %d\n", i, sig->name, *sig->data);
#endif

    /** get local max and min */
    local_max_min(sig->data, sig->data_cnt, sig->loc_max_idx, sig->loc_min_idx,
        &(sig->loc_max_idx_cnt), &(sig->loc_min_idx_cnt));

#ifdef DEBUG
    printf("dataset %u: %u local max cnt, %u local min cnt\n", i,
        sig->loc_max_idx_cnt, sig->loc_min_idx_cnt);
#endif

    /** do interpolation and get global max and min */
    switch (alg)
    {
        case PLI:
            pli(sig, interp_min);
            break;
        case CSI:
            csi(sig, interp_n, NULL, interp_min);
            break;
        case ECI:
            eci(sig, interp_n);
            break;
    }

#ifdef DEBUG
    printf("dataset %u: x:%.1f y:%.1f global max, x:%.1f y:%.1f global min\n", i,
        sig->glob_max.x, sig->glob_max.y, sig->glob_min.x, sig->glob_min.y);

    printf("dataset %u: %.1f shift x from global max of ref\n\n", i, sig->shift_x_max);
#endif
    return 0;
}

int shm_shift__write_csv(shm_shift_t* self, char* csv_path, char delim)
{
    if (!self)
        return -3;

    FILE* f_lmax;
    FILE* f_lmin;
    FILE* f_lcntr;
    FILE* f_interp;
    FILE* f_results;

    char path_lmax[256];
    char path_lmin[256];
    char path_lcntr[256];
    char path_interp[256];
    char path_results[256];
    char d = delim;

    char* algstr;
    switch (self->alg)
    {
        case PLI: algstr = "pli"; break;
        case CSI: algstr = "csi"; break;
        case ECI: algstr = "eci"; break;
    }

    sprintf(path_lmax, "%s_local_max.csv", csv_path);
    sprintf(path_lmin, "%s_local_min.csv", csv_path);
    sprintf(path_lcntr, "%s_local_ext_centers.csv", csv_path);
    if (self->interp_min)
        sprintf(path_interp, "%s_interpolated_min_%s.csv", csv_path, algstr);
    else
        sprintf(path_interp, "%s_interpolated_%s.csv", csv_path, algstr);
    sprintf(path_results, "%s_results_%s.csv", csv_path, algstr);

    f_lmax = fopen(path_lmax,"w");
    f_lmin = fopen(path_lmin,"w");
    f_lcntr = fopen(path_lcntr,"w");
    f_interp = fopen(path_interp,"w");
    f_results = fopen(path_results,"w");

    for(int i = 0; i < self->cols; i++)
    {
        fprintf (f_lmax, "%s_x%c%s_y%c", self->signals[i].name, d, self->signals[i].name, d);
        fprintf (f_lmin, "%s_x%c%s_y%c", self->signals[i].name, d, self->signals[i].name, d);
        fprintf (f_lcntr, "%s_x%c%s_y%c", self->signals[i].name, d, self->signals[i].name, d);
        fprintf (f_interp, "%s_x%c%s_y%c", self->signals[i].name, d, self->signals[i].name, d);

        fprintf (f_results, "%s_global_max_x%c%s_global_max_y%c%s_global_min_x%c%s_global_min_y%c%s_shift_max_x%c",
            self->signals[i].name, d, self->signals[i].name, d, self->signals[i].name, d, self->signals[i].name, d,
            self->signals[i].name, d);
    }

    int loc_max_maxcnt = 0;
    int loc_min_maxcnt = 0;
    int loc_extr_maxcnt = 0;
    int interp_maxcnt = 0;

    for(int j = 0; j < self->cols; j++)
    {
        if (self->signals[j].loc_max_idx_cnt > loc_max_maxcnt)
            loc_max_maxcnt = self->signals[j].loc_max_idx_cnt;

        if (self->signals[j].loc_min_idx_cnt > loc_min_maxcnt)
            loc_min_maxcnt = self->signals[j].loc_min_idx_cnt;

        if (self->signals[j].loc_ext_cntr_cnt > loc_extr_maxcnt)
            loc_extr_maxcnt = self->signals[j].loc_ext_cntr_cnt;

        if (self->signals[j].ip_cnt > interp_maxcnt)
            interp_maxcnt = self->signals[j].ip_cnt;
    }

    for(int i = 0; i < loc_max_maxcnt; i++)
    {
        fprintf (f_lmax, "\n");

        for(int j = 0; j < self->cols; j++)
        {
            if (i < self->signals[j].loc_max_idx_cnt)
                fprintf (f_lmax, "%d%c%d%c", self->signals[j].loc_max_idx[i], d,
                    self->signals[j].data[self->signals[j].loc_max_idx[i]], d);
            else
                fprintf(f_lmax, "%c%c", d, d);
        }
    }

    for(int i = 0; i < loc_min_maxcnt; i++)
    {
        fprintf (f_lmin, "\n");

        for(int j = 0; j < self->cols; j++)
        {
            if (i < self->signals[j].loc_min_idx_cnt)
                fprintf (f_lmin, "%d%c%d%c", self->signals[j].loc_min_idx[i], d,
                    self->signals[j].data[self->signals[j].loc_min_idx[i]], d);
            else
                fprintf(f_lmin, "%c%c", d, d);
        }
    }

    for(int i = 0; i < loc_extr_maxcnt; i++)
    {
        fprintf (f_lcntr, "\n");

        for(int j = 0; j < self->cols; j++)
        {
            if (i < self->signals[j].loc_ext_cntr_cnt)
                fprintf (f_lcntr, "%.3f%c%.3f%c", self->signals[j].loc_ext_cntr[i].x, d,
                    self->signals[j].loc_ext_cntr[i].y, d);
            else
                fprintf(f_lcntr, "%c%c", d, d);
        }
    }

    for(int i = 0; i < interp_maxcnt; i++)
    {
        fprintf (f_interp, "\n");

        for(int j = 0; j < self->cols; j++)
        {
            if (i < self->signals[j].ip_cnt)
                fprintf (f_interp, "%.3f%c%.3f%c", self->signals[j].interp[i].x, d,
                    self->signals[j].interp[i].y, d);
            else
                fprintf(f_interp, "%c%c", d, d);
        }
    }

    fprintf (f_results, "\n");
    for(int j = 0; j < self->cols; j++)
    {
        fprintf (f_results, "%.3f%c%.3f%c%.3f%c%.3f%c%.3f%c", self->signals[j].glob_max.x, d,
            self->signals[j].glob_max.y, d, self->signals[j].glob_min.x, d,
            self->signals[j].glob_min.y, d, self->signals[j].shift_x_max, d);
    }

    printf("%s created\n%s created\n%s created\n%s created\n%s created\n\n",
        path_lmax, path_lmin, path_lcntr, path_interp, path_results);

    fclose(f_lmax);
    fclose(f_lmin);
    fclose(f_interp);
    fclose(f_results);

    return 0;
}

void shm_shift__dispose_calc(shm_shift_t* self)
{
    if (self)
    {
        for (int j = 0; j < self->cols; j++)
        {
            free(self->signals[j].interp);
            free(self->signals[j].loc_min_idx);
            free(self->signals[j].loc_max_idx);
            free(self->signals[j].loc_ext_cntr);

            self->signals[j].ip_cnt = 0;
            self->signals[j].loc_min_idx_cnt = 0;
            self->signals[j].loc_max_idx_cnt = 0;
            self->signals[j].loc_ext_cntr_cnt = 0;
        }
    }
}

void shm_shift__dispose_all(shm_shift_t* self)
{
    if (self)
    {
        shm_shift__dispose_calc(self);

        for (int j = 0; j < self->cols; j++)
        {
            free(self->signals[j].data);
        }
        free(self->signals);
    }
}

void shm_shift__free(shm_shift_t* self)
{
    if (self)
    {
        shm_shift__dispose_all(self);
        csv_free(&self->parser);
        free(self);
    }
}

static void cb1(void *s, size_t len, void *dat, void *obj)
{
    shm_shift_t* shm = (shm_shift_t*) obj;

    if (shm->second_pass)
    {
        char str[CELL_MAX_LEN] = {0};
        memcpy(str, s, len);

        int* data_base = (int*)(shm->signals[shm->cols_it].data);

        if (shm->rows_it == 0)
            strcpy(shm->signals[shm->cols_it].name, str);
        else if (shm->rows_it < shm->rows)
        {
            *(data_base + shm->rows_it - 1) = atoi(str);
            shm->signals[shm->cols_it].data_cnt++;
        }
    }
    shm->cols_it++;
}

static void cb2(int c, void *data, void *obj)
{
    shm_shift_t* shm = (shm_shift_t*) obj;

    shm->rows_it++;
    shm->cols = shm->cols_it;
    shm->cols_it = 0;
}

static int is_space(unsigned char c)
{
    if (c == CSV_SPACE || c == CSV_TAB)
        return 1;
    return 0;
}

static int is_term(unsigned char c)
{
    if (c == CSV_CR || c == CSV_LF)
        return 1;
    return 0;
}

/** Piecewise Linear Interpolation */
static void pli(struct signal* sig, int interp_min)
{
    float max_val = INT_MIN;
    float min_val = INT_MAX;

    int* loc_idx = sig->loc_max_idx;
    int loc_idx_cnt = sig->loc_max_idx_cnt;

    if (interp_min)
    {
        loc_idx = sig->loc_min_idx;
        loc_idx_cnt = sig->loc_min_idx_cnt;
    }

    sig->ip_cnt = loc_idx_cnt;
    sig->interp = (struct pt*)malloc(sig->ip_cnt * sizeof(struct pt));

    for (int i = 0; i < sig->ip_cnt; i++)
    {
        float x = (float)loc_idx[i];
        float y = (float)sig->data[loc_idx[i]];

        struct pt p = { x , y };
        sig->interp[i] = p;

        if (y > max_val)
        {
            max_val = y;
            sig->glob_max = p;
        }
        else if (y < min_val)
        {
            min_val = y;
            sig->glob_min = p;
        }
    }
}

/** Cubic Spline Interpolation */
static void csi(struct signal* sig, int ni, float* y_override, int interp_min)
{
    float max_val = INT_MIN;
    float min_val = INT_MAX;

    assert(sig->loc_max_idx_cnt > 1);

    int* loc_idx = sig->loc_max_idx;
    int loc_idx_cnt = sig->loc_max_idx_cnt;

    if (interp_min)
    {
        loc_idx = sig->loc_min_idx;
        loc_idx_cnt = sig->loc_min_idx_cnt;
    }

    float* y = malloc(loc_idx_cnt * sizeof(float));
    float* b = malloc(loc_idx_cnt * sizeof(float));
    float* c = malloc(loc_idx_cnt * sizeof(float));
    float* d = malloc(loc_idx_cnt * sizeof(float));

    y[0] = (float)(y_override != NULL ? y_override[0] : sig->data[loc_idx[0]]);

    if (interp_min)
        y[0] = (float)sig->data[loc_idx[0]];

    for (int i = 1; i < loc_idx_cnt; i++)
        y[i] = (float)(y_override != NULL ? y_override[i] : sig->data[loc_idx[i]]);

    sig->ip_cnt =  ((1 + ni) * loc_idx_cnt) - ni; // ni * diff_x_cnt
    sig->interp = (struct pt*)malloc(sig->ip_cnt * sizeof(struct pt));

    cubic_spline(loc_idx_cnt, loc_idx, y, b, c, d);

    for (int i = 1, k = 0, q = 0; i < loc_idx_cnt + 1; i++, k++)
    {
        int block = 0;
        if (i < loc_idx_cnt)
            block = (loc_idx[i] - loc_idx[i - 1]);
        else
            ni = 0;

        float px2 = 0;
        float dt = (float)block / (float)(ni + 1);
        for (int m = 0; m < ni + 1; q++, m++)
        {
            px2 += dt;
            float px = (float)loc_idx[i - 1] + px2;
            float py = (d[k] * px2*px2*px2) + (c[k] * px2*px2) + (b[k] * px2) + y[k];

            struct pt p = { px , py };
            sig->interp[q] = p;

            if (py > max_val)
            {
                max_val = py;
                sig->glob_max = p;
            }
            else if (py < min_val)
            {
                min_val = py;
                sig->glob_min = p;
            }
        }
    }
    free(y);
    free(b);
    free(c);
    free(d);
}

/** Extremum Center Inerpolation */
static void eci(struct signal* sig, int ni)
{
    int ext_cnt = sig->loc_min_idx_cnt + sig->loc_max_idx_cnt;
    assert(ext_cnt > 0);

    sig->loc_ext_cntr_cnt = ext_cnt;
    sig->loc_ext_cntr = (struct pt*)malloc(ext_cnt * sizeof(struct pt));
    int* mid_x = (int*)malloc(ext_cnt * sizeof(int));
    float* mid_y = (float*)malloc(ext_cnt * sizeof(float));

    for (int i = 0, xc = 0, nc = 0; i < ext_cnt; i++) /* xc - max counter, nc - min counter */
    {
        /** local max projecting down to line between two local mins */
        if (xc < sig->loc_max_idx_cnt && (nc >= sig->loc_min_idx_cnt || sig->loc_max_idx[xc] < sig->loc_min_idx[nc]))
        {
            int p1x, p2x, p1y, p2y = 0;
            eci_get_points_cntr(nc, sig->loc_min_idx_cnt, sig->loc_min_idx, sig->data, &p1x, &p2x, &p1y, &p2y);
            float lower_cntr = line_eq(sig->loc_max_idx[xc], p1x, p2x, p1y, p2y);

            sig->loc_ext_cntr[i].x = (float)sig->loc_max_idx[xc];
            mid_x[i] = sig->loc_max_idx[xc];
            mid_y[i] = sig->loc_ext_cntr[i].y = (((float)sig->data[sig->loc_max_idx[xc]] - lower_cntr) / 2.0) + lower_cntr;
            xc++;
        }
        else /** local min projecting up to line between two local maxs */
        {
            assert(nc < sig->loc_min_idx_cnt);
            int p1x, p2x, p1y, p2y = 0;
            eci_get_points_cntr(xc, sig->loc_max_idx_cnt, sig->loc_max_idx, sig->data, &p1x, &p2x, &p1y, &p2y);
            float upper_cntr = line_eq(sig->loc_min_idx[nc], p1x, p2x, p1y, p2y);

            sig->loc_ext_cntr[i].x = (float)sig->loc_min_idx[nc];
            mid_x[i] = sig->loc_min_idx[nc];
            mid_y[i] = sig->loc_ext_cntr[i].y = ((upper_cntr - (float)sig->data[sig->loc_min_idx[nc]]) / 2.0) +
                (float)sig->data[sig->loc_min_idx[nc]];
            nc++;
        }
    }

    int* tmp_lmax = sig->loc_max_idx;
    int tmp_lmax_cnt = sig->loc_max_idx_cnt;

    sig->loc_max_idx = mid_x;
    sig->loc_max_idx_cnt = ext_cnt;

    csi(sig, ni, mid_y, 0);

    sig->loc_max_idx = tmp_lmax;
    sig->loc_max_idx_cnt = tmp_lmax_cnt;

    free(mid_x);
    free(mid_y);
}

static void cubic_spline(int n, int* x, float* a, float* b, float* c, float* d)
{
    /** Numerical Analysis 9th ed - Burden, Faires (Ch. 3 Natural Cubic Spline, Pg. 149)
        https://gist.github.com/svdamani/1015c5c4b673c3297309 */

    /** Step 0 */
    int i, j;
    n--;

    float* h = malloc(n * sizeof(float));
    float* A = malloc(n * sizeof(float));
    float* l = malloc((n + 1) * sizeof(float));
    float* u = malloc((n + 1) * sizeof(float));
    float* z = malloc((n + 1) * sizeof(float));

    /** Step 1 */
    for (i = 0; i <= n - 1; ++i)
    {
        h[i] = (float)x[i + 1] - (float)x[i];
    }


    /** Step 2 */
    for (i = 1; i <= n - 1; ++i)
        A[i] = 3 * (a[i + 1] - a[i]) / h[i] - 3 * (a[i] - a[i - 1]) / h[i - 1];

    /** Step 3 */
    l[0] = 1;
    u[0] = 0;
    z[0] = 0;

    /** Step 4 */
    for (i = 1; i <= n - 1; ++i) {
        l[i] = 2 * (float)(x[i + 1] - x[i - 1]) - h[i - 1] * u[i - 1];
        u[i] = h[i] / l[i];
        z[i] = (A[i] - h[i - 1] * z[i - 1]) / l[i];
    }

    /** Step 5 */
    l[n] = 1;
    z[n] = 0;
    c[n] = 0;

    /** Step 6 */
    for (j = n - 1; j >= 0; --j) {
        c[j] = z[j] - u[j] * c[j + 1];
        b[j] = (a[j + 1] - a[j]) / h[j] - h[j] * (c[j + 1] + 2 * c[j]) / 3;
        d[j] = (c[j + 1] - c[j]) / (3 * h[j]);
    }

    free(h);
    free(A);
    free(l);
    free(u);
    free(z);
}

static void eci_get_points_cntr(int idx, int loc_cnt, int* loc, int* di, int* p1x, int* p2x, int* p1y, int* p2y)
{
    if (idx == 0)            /* left local min/max point is missing */
    {
        *p1x = loc[idx] - 1;
        *p2x = loc[idx];
        *p1y = di[loc[idx]];
        *p2y = di[loc[idx]];
    }
    else if (idx == loc_cnt) /* right local min/max point is missing */
    {
        *p1x = loc[idx - 1];
        *p2x = loc[idx - 1] + 1;
        *p1y = di[loc[idx - 1]];
        *p2y = di[loc[idx - 1]];
    }
    else                    /* both local min/max points present */
    {
        *p1x = loc[idx - 1];
        *p2x = loc[idx];
        *p1y = di[loc[idx - 1]];
        *p2y = di[loc[idx]];
    }
}

static float line_eq(int x, int x1, int x2, int y1, int y2)
{
    // return : y = mx + b
    int dx = x2 - x1;
    int dy = y2 - y1;
    float m = (float)dy / (float)dx; // int ??
    float b = (float)y1 - (m * (float)x1);
    return (m * (float)x) + b;
}

enum dirs {
    Up,
    Down,
    Start
};

static void local_max_min(int* array, int len, int* max_idx, int* min_idx, int* max_cnt, int* min_cnt)
{
    float prev = 0;
    enum dirs dir_curr = Start;
    enum dirs dir_prev = Start;

    *min_cnt = 0;
    *max_cnt = 0;

    int last = len - 1;
    for (int i = 0; i < len; i++)
    {
        float curr = array[i];

        if (i > 0)
        {
            if (prev < curr)
            {
                if (dir_curr == Down && dir_prev != Start && i != last)
                {
                    min_idx[*min_cnt] = i;
                    *min_cnt = *min_cnt + 1;
                }
                dir_curr = Up;
            }
            else if (prev > curr)
            {
                if (dir_curr == Up && dir_prev != Start && i != last)
                {
                    max_idx[*max_cnt] = i;
                    *max_cnt = *max_cnt + 1;
                }
                dir_curr = Down;
            }
        }
        prev = curr;
        dir_prev = dir_curr;
    }
}
