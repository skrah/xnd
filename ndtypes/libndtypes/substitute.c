/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2017-2024, plures
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <ndtypes.h>


#include "symtable.h"
#include "substitute.h"


static const ndt_t *
substitute_named_ellipsis(const ndt_t *t, const symtable_t *tbl, ndt_context_t *ctx)
{
    symtable_entry_t v;
    const ndt_t *u;
    int i;

    assert(t->tag == EllipsisDim && t->EllipsisDim.name != NULL);

    u = ndt_substitute(t->EllipsisDim.type, tbl, true, ctx);
    if (u == NULL) {
        return NULL;
    }

    v = symtable_find(tbl, t->EllipsisDim.name);

    switch (v.tag) {
    case FixedSeq: {
        for (i = v.FixedSeq.size-1; i >= 0; i--) {
            const ndt_t *w = v.FixedSeq.dims[i];
            assert(ndt_is_concrete(w));
            assert(w->tag == FixedDim);

            const ndt_t *x = ndt_fixed_dim(u, w->FixedDim.shape, INT64_MAX, ctx);
            ndt_move(&u, x);
            if (u == NULL) {
                return NULL;
            }
        }

        return u;
    }
    case VarSeq: {
        if (v.VarSeq.size == 0) {
            return u;
        }
        else {
            const ndt_t *w = v.VarSeq.dims[0];
            const ndt_t *x = ndt_copy_contiguous_dtype(w, u, v.VarSeq.linear_index, ctx);
            ndt_decref(u);
            return x;
        }
    }
    case ArraySeq: {
        for (i = v.ArraySeq.size-1; i >= 0; i--) {
            #ifndef NDEBUG
            const ndt_t *w = v.ArraySeq.dims[i];
            assert(ndt_is_concrete(w));
            assert(w->tag == Array);
            #endif

            const ndt_t *x = ndt_array(u, false, ctx);
            ndt_move(&u, x);
            if (u == NULL) {
                return NULL;
            }
        }

        return u;
    }
    default:
        ndt_err_format(ctx, NDT_ValueError,
            "variable not found or has incorrect type");
        ndt_decref(u);
        return NULL;
    }
}

const ndt_t *
ndt_substitute(const ndt_t *t, const symtable_t *tbl, const bool req_concrete,
               ndt_context_t *ctx)
{
    bool opt = ndt_is_optional(t);
    const ndt_t *u, *w;

    if (ndt_is_concrete(t)) {
        ndt_incref(t);
        return t;
    }

    switch (t->tag) {
    case FixedDim: {
        u = ndt_substitute(t->FixedDim.type, tbl, req_concrete, ctx);
        if (u == NULL) {
            return NULL;
        }

        w = ndt_fixed_dim(u, t->FixedDim.shape, t->Concrete.FixedDim.step,
                          ctx);
        ndt_decref(u);
        return w;
    }

    case VarDim: {
        u = ndt_substitute(ndt_dtype(t), tbl, req_concrete, ctx);
        if (u == NULL) {
            return NULL;
        }

        w = ndt_copy_abstract_var_dtype(t, u, ctx);
        ndt_decref(u);
        return w;
    }

    case SymbolicDim: {
        u = ndt_substitute(t->SymbolicDim.type, tbl, req_concrete, ctx);
        if (u == NULL) {
            return NULL;
        }

        const int64_t shape = symtable_find_shape(tbl, t->SymbolicDim.name, ctx);
        if (shape < 0) {
            if (req_concrete) {
                ndt_decref(u);
                return NULL;
            }
            else {
                ndt_err_clear(ctx);
                char *name = ndt_strdup(t->SymbolicDim.name, ctx);
                if (name == NULL) {
                    ndt_decref(u);
                    return NULL;
                }
                w = ndt_symbolic_dim(name, u, ctx);
                ndt_decref(u);
                return w;
            }
        }

        w = ndt_fixed_dim(u, shape, INT64_MAX, ctx);
        ndt_decref(u);
        return w;
    }

    case EllipsisDim: {
        if (t->EllipsisDim.name == NULL) {
            return ndt_substitute(t->EllipsisDim.type, tbl, true, ctx);
        }
        else {
            return substitute_named_ellipsis(t, tbl, ctx);
        }
    }

    case Typevar: {
        const ndt_t *v = symtable_find_typevar(tbl, t->Typevar.name, ctx);
        if (v == NULL) {
            if (req_concrete) {
                return NULL;
            }
            else {
                ndt_err_clear(ctx);
                char *name = ndt_strdup(t->Typevar.name, ctx);
                if (name == NULL) {
                    return NULL;
                }
                return ndt_typevar(name, ctx);
            }
        }

        return ndt_substitute(v, tbl, req_concrete, ctx);
    }

    case Constr: {
        char *name = ndt_strdup(t->Constr.name, ctx);
        if (name == NULL) {
            return NULL;
        }

        u = ndt_substitute(t->Constr.type, tbl, req_concrete, ctx);
        if (u == NULL) {
            ndt_free(name);
            return NULL;
        }

        w = ndt_constr(name, u, opt, ctx);
        ndt_decref(u);
        return w;
    }

    case Nominal: {
        char *name = ndt_strdup(t->Nominal.name, ctx);
        if (name == NULL) {
            return NULL;
        }

        u = ndt_copy(t->Nominal.type, ctx);
        if (u == NULL) {
            ndt_free(name);
            return NULL;
        }

        w = ndt_nominal(name, u, opt, ctx);
        ndt_decref(u);
        return w;
    }

    case Ref: {
        u = ndt_substitute(t->Ref.type, tbl, req_concrete, ctx);
        if (u == NULL) {
            return NULL;
        }

        w = ndt_ref(u, opt, ctx);
        ndt_decref(u);
        return w;
    }

    case Bool:
    case Int8: case Int16: case Int32: case Int64:
    case Uint8: case Uint16: case Uint32: case Uint64:
    case BFloat16: case Float16: case Float32: case Float64:
    case BComplex32: case Complex32: case Complex64: case Complex128:
    case FixedString: case FixedBytes:
    case String: case Bytes:
    case Char: {
        ndt_incref(t);
        return t;
    }

    default:
        ndt_err_format(ctx, NDT_NotImplementedError,
            "substitution not implemented for this type");
        return NULL;
    }
}
