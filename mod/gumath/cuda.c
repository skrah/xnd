#include <Python.h>
#include <ndtypes.h>
#include <gumath.h>

#include <pyndtypes.h>
#include <pygumath.h>


/****************************************************************************/
/*                              Module globals                              */
/****************************************************************************/

/* Function table */
static gm_tbl_t *table = NULL;


/****************************************************************************/
/*                                  Module                                  */
/****************************************************************************/

static struct PyModuleDef cuda_module = {
    PyModuleDef_HEAD_INIT,        /* m_base */
    "cuda",                       /* m_name */
    NULL,                         /* m_doc */
    -1,                           /* m_size */
    NULL,                         /* m_methods */
    NULL,                         /* m_slots */
    NULL,                         /* m_traverse */
    NULL,                         /* m_clear */
    NULL                          /* m_free */
};


PyMODINIT_FUNC
PyInit_cuda(void)
{
    NDT_STATIC_CONTEXT(ctx);
    PyObject *m = NULL;
    static int initialized = 0;

    if (!initialized) {
       if (import_ndtypes() < 0) {
            return NULL;
       }
       if (import_gumath() < 0) {
            return NULL;
       }

       table = gm_tbl_new(&ctx);
       if (table == NULL) {
           return Ndt_SetError(&ctx);
       }

       if (gm_init_cuda_unary_kernels(table, &ctx) < 0) {
           return Ndt_SetError(&ctx);
       }

       if (gm_init_cuda_binary_kernels(table, &ctx) < 0) {
           return Ndt_SetError(&ctx);
       }

       initialized = 1;
    }

    m = PyModule_Create(&cuda_module);
    if (m == NULL) {
        goto error;
    }

    if (Gumath_AddCudaFunctions(m, table) < 0) {
        goto error;
    }

    return m;

error:
    Py_CLEAR(m);
    return NULL;
}
